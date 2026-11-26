import XCTest
@testable import BenchCore

final class BenchmarkTests:XCTestCase {
    func config()->Configuration {Configuration(scenario:Scenario.all[1],duration:10,destination:42,source:43,audio:false,sampleRate:48000,notes:"fixture",firmware:"123",appRevision:"test")}
    func testProbeIdentityPayloadAndBounds() {
        for size in [16,256,1024] {
            let bytes=Probe.bytes(tag:127,sequence:0x0ffffffe,size:size)
            XCTAssertEqual(bytes.count,size);XCTAssertEqual(Probe.sequence(bytes,tag:127,size:size),0x0ffffffe)
            XCTAssertNil(Probe.sequence(bytes,tag:128,size:size))
            var corrupt=bytes;corrupt[12]^=1;XCTAssertNil(Probe.sequence(corrupt,tag:127,size:size))
        }
    }
    func testFragmentedSysExRealtimeAndMalformedBounds() {
        var decoder=MIDIByteDecoder();let bytes=Probe.bytes(tag:11,sequence:7,size:256)
        XCTAssertEqual(decoder.consume(Array(bytes.prefix(100))+[0xf8],tag:11,size:256),[.clock])
        XCTAssertEqual(decoder.consume(Array(bytes.dropFirst(100)),tag:11,size:256),[.probe(7)])
        XCTAssertEqual(decoder.consume([0xf0]+Array(repeating:1,count:3000)+[0xf7],tag:11,size:256),[])
        XCTAssertEqual(decoder.consume(bytes,tag:11,size:256),[.probe(7)])
        XCTAssertEqual(decoder.consume([0xf0,1,0x90]+Array(bytes.dropFirst()),tag:11,size:256),[])
    }
    func testInterruptedAttemptCannotReplayAndCompleteIsImmutable() throws {
        let root=FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer{try? FileManager.default.removeItem(at:root)}
        var run:RunStore?=try RunStore(root:root,config:config());try run!.start()
        try run!.checkpoint(HostResult(status:"running",sent:17,received:16));run=nil
        run=try RunStore(root:root,config:config())
        XCTAssertEqual(try run!.recover()?.status,"interrupted")
        XCTAssertEqual(try run!.recover()?.sent,17)
        XCTAssertThrowsError(try run!.start());run=nil
        // A new independent attempt can complete; replay/overwrite remains forbidden.
        let other=root.appendingPathComponent("another")
        let complete=try RunStore(root:other,config:config());try complete.start()
        try complete.checkpoint(HostResult(status:"complete",sent:10,received:10))
        XCTAssertEqual(try complete.recover()?.status,"complete")
        XCTAssertThrowsError(try complete.checkpoint(HostResult()))
    }
    func testConfigurationIntegrityLockAndTornResultFailClosed() throws {
        let root=FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer{try? FileManager.default.removeItem(at:root)}
        var run:RunStore?=try RunStore(root:root,config:config())
        XCTAssertThrowsError(try RunStore(root:root,config:config()));run=nil
        var changed=config();changed.duration=60
        XCTAssertThrowsError(try RunStore(root:root,config:changed))
        run=try RunStore(root:root,config:config());try run!.start()
        try Data("{".utf8).write(to:root.appendingPathComponent("host.json"))
        XCTAssertThrowsError(try run!.recover())
    }
    func testIndependentValidatorReadsSwiftArtifacts() throws {
        let package=URL(fileURLWithPath:#filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let root=FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer{try? FileManager.default.removeItem(at:root)}
        let create=Process();create.executableURL=package.appendingPathComponent(".build/debug/USBBench");create.arguments=["--checkpoint-fixture",root.path,"--complete"]
        try create.run();create.waitUntilExit();XCTAssertEqual(create.terminationStatus,0)
        let validate=Process();validate.executableURL=URL(fileURLWithPath:"/usr/bin/env");validate.arguments=["python3",package.appendingPathComponent("validate_run.py").path,root.path]
        try validate.run();validate.waitUntilExit();XCTAssertEqual(validate.terminationStatus,0)
    }
    func testRealProcessTerminationAndRecovery() throws {
        let product=URL(fileURLWithPath:#filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().appendingPathComponent(".build/debug/USBBench")
        for signal in [SIGTERM,SIGKILL] {
            let root=FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
            defer{try? FileManager.default.removeItem(at:root)}
            let process=Process();process.executableURL=product;process.arguments=["--checkpoint-fixture",root.path]
            let pipe=Pipe();process.standardOutput=pipe;try process.run()
            let data=pipe.fileHandleForReading.availableData
            XCTAssertTrue(String(decoding:data,as:UTF8.self).contains("DURABLE"))
            kill(process.processIdentifier,signal);process.waitUntilExit()
            let recovered=try RunStore(root:root)
            XCTAssertEqual(try EventJournal(root:root).entries.first?.id,"fixture-event")
            XCTAssertEqual(try recovered.recover()?.sent,17);XCTAssertEqual(try recovered.recover()?.status,"interrupted")
            XCTAssertThrowsError(try recovered.start())
        }
    }
    func testCrashBeforeCheckpointNeverClaimsComplete() throws {
        let root=FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer{try? FileManager.default.removeItem(at:root)}
        var run:RunStore?=try RunStore(root:root,config:config());try run!.start();run=nil
        let recovered=try RunStore(root:root)
        XCTAssertNil(try recovered.recover());XCTAssertThrowsError(try recovered.start())
        XCTAssertFalse(FileManager.default.fileExists(atPath:root.appendingPathComponent("HOST_COMPLETE").path))
    }
}
