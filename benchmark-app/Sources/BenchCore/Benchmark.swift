import Foundation
import CryptoKit
import Darwin

public struct Scenario: Identifiable, Codable, Equatable {
    public let id: String
    public let name: String
    public let messagesPerSecond: Int
    public let payloadBytes: Int
    public static let all: [Scenario] = [
        .init(id: "idle", name: "Vila / endast externt ljud", messagesPerSecond: 0, payloadBytes: 0),
        .init(id: "midi100", name: "MIDI · 100 testmeddelanden/s", messagesPerSecond: 100, payloadBytes: 16),
        .init(id: "midi1000", name: "MIDI · 1 000 testmeddelanden/s", messagesPerSecond: 1000, payloadBytes: 16),
        .init(id: "sysex", name: "SysEx · 20 × 256 byte/s", messagesPerSecond: 20, payloadBytes: 256),
        .init(id: "clock", name: "Ta emot MIDI-klocka", messagesPerSecond: 0, payloadBytes: 0)
    ]
}
public struct Configuration: Codable, Equatable {
    public var scenario: Scenario
    public var duration: Int
    public var destination: Int32
    public var source: Int32
    public var audio: Bool
    public var sampleRate: Int
    public var notes: String
    public var firmware: String
    public var appRevision: String
    public init(scenario: Scenario, duration: Int, destination: Int32, source: Int32, audio: Bool, sampleRate: Int, notes: String, firmware: String, appRevision: String) {
        self.scenario=scenario; self.duration=duration; self.destination=destination; self.source=source
        self.audio=audio; self.sampleRate=sampleRate; self.notes=notes; self.firmware=firmware; self.appRevision=appRevision
    }
}
public func digest(_ data: Data) -> String { SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined() }
public func canonical<T: Encodable>(_ value: T) throws -> Data {
    let e=JSONEncoder(); e.outputFormatting=[.sortedKeys,.withoutEscapingSlashes]; return try e.encode(value)
}
public enum BenchError: LocalizedError {
    case invalid(String)
    public var errorDescription: String? { if case let .invalid(s)=self { return s }; return nil }
}
public func atomicWrite(_ data: Data, to url: URL) throws {
    let parent=url.deletingLastPathComponent()
    try FileManager.default.createDirectory(at: parent, withIntermediateDirectories: true)
    let temp=parent.appendingPathComponent(".tmp-"+UUID().uuidString)
    guard FileManager.default.createFile(atPath: temp.path, contents: nil) else { throw BenchError.invalid("Kan inte skapa resultatfil.") }
    do {
        let h=try FileHandle(forWritingTo: temp); try h.write(contentsOf: data); try h.synchronize(); try h.close()
        guard rename(temp.path,url.path)==0 else { throw BenchError.invalid("Kunde inte spara resultat atomiskt.") }
        let fd=open(parent.path,O_RDONLY); guard fd>=0 else { throw BenchError.invalid("Kan inte öppna resultatmappen.") }
        defer { close(fd) }; guard fsync(fd)==0 else { throw BenchError.invalid("Kunde inte synkronisera resultatmappen.") }
    } catch { try? FileManager.default.removeItem(at: temp); throw error }
}
public struct Manifest: Codable, Equatable {
    public let schemaVersion: Int
    public let id: UUID
    public let config: Configuration
    public let fingerprint: String
    public let createdAt: Date
}
public struct HostResult: Codable, Equatable {
    public var status: String
    public var sent: Int
    public var received: Int
    public var duplicates: Int
    public var reordered: Int
    public var lateSends: Int
    public var skippedSends: Int
    public var sendErrors: Int
    public var latencyP50MS: Double?
    public var latencyP99MS: Double?
    public var latencyMaxMS: Double?
    public var clockEvents: Int = 0
    public var clockCoalesced: Int = 0
    public var clockIntervals: Int
    public var clockIntervalMeanMS: Double?
    public var clockIntervalMinMS: Double?
    public var clockIntervalMaxMS: Double?
    public var elapsed: Double
    public init(status: String="interrupted",sent: Int=0,received: Int=0,duplicates: Int=0,reordered: Int=0,lateSends: Int=0,skippedSends: Int=0,sendErrors: Int=0,latencyP50MS: Double?=nil,latencyP99MS: Double?=nil,latencyMaxMS: Double?=nil,clockIntervals: Int=0,clockIntervalMeanMS: Double?=nil,clockIntervalMinMS: Double?=nil,clockIntervalMaxMS: Double?=nil,elapsed: Double=0) {
        self.status=status;self.sent=sent;self.received=received;self.duplicates=duplicates;self.reordered=reordered;self.lateSends=lateSends;self.skippedSends=skippedSends;self.sendErrors=sendErrors;self.latencyP50MS=latencyP50MS;self.latencyP99MS=latencyP99MS;self.latencyMaxMS=latencyMaxMS;self.clockIntervals=clockIntervals;self.clockIntervalMeanMS=clockIntervalMeanMS;self.clockIntervalMinMS=clockIntervalMinMS;self.clockIntervalMaxMS=clockIntervalMaxMS;self.elapsed=elapsed
    }
}
/// One physical attempt per directory. Recovery never replays outgoing MIDI.
public final class RunStore {
    public let root: URL
    public let manifest: Manifest
    private let lock: Int32
    public init(root: URL, config: Configuration? = nil) throws {
        self.root=root
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        lock=open(root.appendingPathComponent(".lock").path,O_CREAT|O_RDWR,0o600)
        guard lock>=0,flock(lock,LOCK_EX|LOCK_NB)==0 else { if lock>=0 { close(lock) };throw BenchError.invalid("Körningen används av en annan process.") }
        do {
            let path=root.appendingPathComponent("manifest.json")
            if FileManager.default.fileExists(atPath:path.path) {
                manifest=try JSONDecoder().decode(Manifest.self,from:Data(contentsOf:path))
                guard manifest.schemaVersion==1,manifest.fingerprint==digest(try canonical(manifest.config)),config==nil || config==manifest.config else { throw BenchError.invalid("Konfigurationen skiljer sig från den sparade körningen.") }
            } else {
                guard let config, [10,60,300].contains(config.duration),[44100,48000].contains(config.sampleRate),Scenario.all.contains(config.scenario) else { throw BenchError.invalid("Ogiltig körkonfiguration.") }
                manifest=Manifest(schemaVersion:1,id:UUID(),config:config,fingerprint:digest(try canonical(config)),createdAt:Date())
                try atomicWrite(try canonical(manifest),to:path)
            }
        } catch {close(lock);throw error}
    }
    deinit {close(lock)}
    public func state(_ status: String) throws {
        try atomicWrite(try canonical(["status":status,"updatedAt":ISO8601DateFormatter().string(from:Date())]),to:root.appendingPathComponent("state.json"))
    }
    public func start() throws {
        guard !FileManager.default.fileExists(atPath:root.appendingPathComponent("started.json").path) else { throw BenchError.invalid("Denna fysiska körning har redan startats. Skapa en ny för att försöka igen.") }
        // Durable boundary before the first side effect, including a hard kill.
        try atomicWrite(try canonical(["fingerprint":manifest.fingerprint]),to:root.appendingPathComponent("started.json"))
        try state("running")
    }
    public func checkpoint(_ result: HostResult) throws {
        guard !FileManager.default.fileExists(atPath:root.appendingPathComponent("HOST_COMPLETE").path) else {throw BenchError.invalid("Avslutat resultat är oföränderligt.")}
        let data=try canonical(result);try atomicWrite(data,to:root.appendingPathComponent("host.json"))
        try state(result.status)
        if result.status=="complete" { try atomicWrite(Data(digest(data).utf8),to:root.appendingPathComponent("HOST_COMPLETE")) }
    }
    public func recover() throws -> HostResult? {
        let path=root.appendingPathComponent("host.json")
        guard FileManager.default.fileExists(atPath:path.path) else {
            if FileManager.default.fileExists(atPath:root.appendingPathComponent("started.json").path) {try state("interrupted")};return nil
        }
        let data=try Data(contentsOf:path);var result=try JSONDecoder().decode(HostResult.self,from:data)
        let marker=root.appendingPathComponent("HOST_COMPLETE")
        if FileManager.default.fileExists(atPath:marker.path) {
            guard result.status=="complete",try String(contentsOf:marker,encoding:.utf8)==digest(data) else {throw BenchError.invalid("Resultatets kontrollsumma stämmer inte.")}
        } else {result.status="interrupted";try checkpoint(result)}
        return result
    }
}
/// Non-commercial SysEx probes. Every message has a run tag, sequence and deterministic payload.
public enum Probe {
    public static func bytes(tag: UInt32, sequence: UInt32, size: Int) -> [UInt8] {
        let size=max(16,min(1024,size));var b:[UInt8]=[0xf0,0x7d,0x55,0x42]
        for n in [tag,sequence] { for i in 0..<4 {b.append(UInt8((n >> (i*7))&127))} }
        while b.count<size-1 {b.append(UInt8((b.count+Int(sequence&127))&127))};b.append(0xf7);return b
    }
    public static func sequence(_ bytes: [UInt8],tag: UInt32,size: Int) -> UInt32? {
        guard bytes.count==size,bytes.count>=16,Array(bytes.prefix(4))==[0xf0,0x7d,0x55,0x42],bytes.last==0xf7 else{return nil}
        var t:UInt32=0,s:UInt32=0
        for i in 0..<4 {t|=UInt32(bytes[4+i]) << (i*7);s|=UInt32(bytes[8+i]) << (i*7)}
        guard t==tag,bytes==self.bytes(tag:tag,sequence:s,size:size) else{return nil};return s
    }
}
