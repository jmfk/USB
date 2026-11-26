import XCTest
@testable import BenchCore

final class EvaluationTests:XCTestCase {
    func testHardwareWaitsAndNoSkippingToTraffic() {
        let path:[EvaluationStage]=[.setup,.connectDevice,.arming,.connectHost,.running,.connectForRead,.fetching,.finished]
        for i in 0..<path.count-1 {XCTAssertTrue(path[i].allows(path[i+1]))}
        for wait in [EvaluationStage.connectDevice,.connectHost,.connectForRead] {XCTAssertFalse(wait.isBusy);XCTAssertFalse(wait.instruction.isEmpty)}
        XCTAssertFalse(EvaluationStage.setup.allows(.running));XCTAssertFalse(EvaluationStage.connectDevice.allows(.running))
        XCTAssertFalse(EvaluationStage.interrupted.allows(.running));XCTAssertFalse(EvaluationStage.finished.allows(.running))
        XCTAssertTrue(EvaluationStage.interrupted.allows(.fetching))
    }
    func testRecoveryUsesArtifactsAndNeverResumesTraffic() {
        XCTAssertEqual(EvaluationStage.recover(started:false,hostComplete:false,armed:false,deviceFetched:false),.connectDevice)
        XCTAssertEqual(EvaluationStage.recover(started:false,hostComplete:false,armed:true,deviceFetched:false),.connectHost)
        XCTAssertEqual(EvaluationStage.recover(started:true,hostComplete:false,armed:true,deviceFetched:false),.interrupted)
        XCTAssertEqual(EvaluationStage.recover(started:true,hostComplete:true,armed:true,deviceFetched:false),.connectForRead)
        XCTAssertEqual(EvaluationStage.recover(started:true,hostComplete:true,armed:true,deviceFetched:true),.finished)
        XCTAssertEqual(EvaluationStage.recover(started:true,hostComplete:false,armed:true,deviceFetched:true),.interrupted)
    }
    func testDurableLogTailRepairDeduplicationAndBoundedView() throws {
        let root=FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at:root,withIntermediateDirectories:true)
        defer{try? FileManager.default.removeItem(at:root)}
        var journal=try EventJournal(root:root)
        let event=EvaluationEvent(id:"device-1",message:"Profil hämtad")
        XCTAssertTrue(try journal.append(event));XCTAssertFalse(try journal.append(event))
        let handle=try FileHandle(forWritingTo:journal.url);try handle.seekToEnd();try handle.write(contentsOf:Data("{torn".utf8));try handle.close()
        journal=try EventJournal(root:root);XCTAssertEqual(journal.entries,[event])
        for i in 0..<505 {try journal.append(EvaluationEvent(id:"event-\(i)",message:"\(i)"))}
        XCTAssertEqual(journal.entries.count,500)
        journal=try EventJournal(root:root);XCTAssertEqual(journal.entries.count,500);XCTAssertFalse(try journal.append(event))
        let lines=try String(contentsOf:journal.url,encoding:.utf8).split(separator:"\n")
        XCTAssertEqual(lines.count,506)
    }
    func testWriteFailureRequiresReopenBeforeAppendingAgain() throws {
        let root=FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at:root,withIntermediateDirectories:true)
        defer{try? FileManager.default.removeItem(at:root)}
        let journal=try EventJournal(root:root)
        try FileManager.default.removeItem(at:root)
        XCTAssertThrowsError(try journal.append(EvaluationEvent(message:"not written")))
        try FileManager.default.createDirectory(at:root,withIntermediateDirectories:true)
        XCTAssertThrowsError(try journal.append(EvaluationEvent(message:"still poisoned")))
        let reopened=try EventJournal(root:root)
        XCTAssertTrue(try reopened.append(EvaluationEvent(message:"recovered")))
    }
    func testInteriorCorruptionAndDuplicateIDsRefused() throws {
        let root=FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at:root,withIntermediateDirectories:true)
        defer{try? FileManager.default.removeItem(at:root)}
        let path=root.appendingPathComponent("events.jsonl"),event=EvaluationEvent(id:"same",message:"fixture")
        var line=try canonical(event);line.append(10)
        try (line+line).write(to:path);XCTAssertThrowsError(try EventJournal(root:root))
        try (Data("bad\n".utf8)+line).write(to:path);XCTAssertThrowsError(try EventJournal(root:root))
    }
}
