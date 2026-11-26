import Foundation
import Darwin

public enum EvaluationStage:String,Codable,CaseIterable {
    case setup,connectDevice,arming,connectHost,running,connectForRead,fetching,finished,interrupted
    public var isBusy:Bool {self == .arming || self == .running || self == .fetching}
    public var title:String {
        switch self {
        case .setup:return "Kör en utvärdering"
        case .connectDevice:return "Anslut FM-1 till Mac"
        case .arming:return "Förbereder enheten"
        case .connectHost:return "Anslut testkedjan"
        case .running:return "Utvärdering pågår"
        case .connectForRead:return "Hämta enhetens resultat"
        case .fetching:return "Hämtar och verifierar"
        case .finished:return "Utvärderingen är sparad"
        case .interrupted:return "Utvärderingen avbröts"
        }
    }
    public var instruction:String {
        switch self {
        case .setup:return "Välj scenario och tid. Appen guidar dig genom anslutning, mätning och resultat."
        case .connectDevice:return "Behåll strömmen till FM-1. Anslut USB0 till Mac i device-läge. Fortsätt för att kontrollera firmware och armera profilen."
        case .arming:return "Kontrollerar firmware och sparar ett unikt kör-ID. Ingen mätning har startat."
        case .connectHost:return "Behåll strömmen. Flytta USB0 till din powered hub. Upptäck enheterna med PLAY, ställ in rutter medan stoppad och tryck PLAY igen. När FM-1 kör kan mätningen starta."
        case .running:return "Testtrafiken körs och sparas löpande. CPU och ljudmarginal hämtas från FM-1 efter stopp."
        case .connectForRead:return "Stoppa FM-1 med PLAY/STOP. Behåll strömmen och flytta USB0 tillbaka till Mac i device-läge. Fortsätt för att hämta profil och loggar."
        case .fetching:return "Hämtar frysta mätvärden och loggar. Resultaten sparas och verifieras innan enheten kvitteras."
        case .finished:return "Macens trafik och enhetens profil finns i Resultat. Alla händelser finns i Loggar. En sparad mätning innebär inte automatiskt godkänd hårdvara."
        case .interrupted:return "Testtrafiken är stoppad och försöket sparat som avbrutet. Stoppa FM-1 och anslut den till Mac om du vill hämta den partiella profilen. Ett nytt försök får ett nytt kör-ID."
        }
    }
    public var action:String {
        switch self {
        case .setup:return "Kör utvärdering"
        case .connectDevice:return "FM-1 är ansluten — fortsätt"
        case .connectHost:return "FM-1 kör — starta mätning"
        case .connectForRead:return "Ansluten till Mac — hämta resultat"
        case .finished:return "Visa resultat"
        case .interrupted:return "Hämta partiellt resultat"
        default:return "Arbetar…"
        }
    }
    public func allows(_ next:Self)->Bool {
        switch (self,next) {
        case (.setup,.connectDevice),(.connectDevice,.arming),(.arming,.connectHost),(.connectHost,.running),(.running,.connectForRead),(.running,.interrupted),(.connectForRead,.fetching),(.interrupted,.fetching),(.fetching,.finished),(.fetching,.interrupted):return true
        default:return false
        }
    }
    /// Recover from durable side-effect boundaries, never from an optimistic UI state.
    public static func recover(started:Bool,hostComplete:Bool,armed:Bool,deviceFetched:Bool)->Self {
        if hostComplete && deviceFetched{return .finished}
        if hostComplete{return .connectForRead}
        if started{return .interrupted}
        if deviceFetched{return .interrupted}
        return armed ? .connectHost:.connectDevice
    }
}
public struct EvaluationEvent:Codable,Identifiable,Equatable {
    public let id:String
    public let date:Date
    public let source:String
    public let level:String
    public let message:String
    public init(id:String=UUID().uuidString,date:Date=Date(),source:String="App",level:String="info",message:String) {
        self.id=id;self.date=date;self.source=source;self.level=level;self.message=message
    }
}
/// Single writer under RunStore's lock. The journal survives interrupted UI/bridge work.
public final class EventJournal {
    public let url:URL
    private var ids=Set<String>()
    private var writable=true
    public private(set) var entries:[EvaluationEvent]=[]
    public init(root:URL) throws {
        url=root.appendingPathComponent("events.jsonl")
        guard FileManager.default.fileExists(atPath:url.path) else{return}
        let data=try Data(contentsOf:url);var offset=0
        while offset<data.count {
            guard let newline=data[offset...].firstIndex(of:10) else {
                // Only an unterminated tail is repairable. Complete invalid lines fail closed.
                let h=try FileHandle(forWritingTo:url);try h.truncate(atOffset:UInt64(offset));try h.synchronize();try h.close();break
            }
            let entry=try JSONDecoder().decode(EvaluationEvent.self,from:data[offset..<newline])
            guard !entry.id.isEmpty,ids.insert(entry.id).inserted else{throw BenchError.invalid("Loggen har dubbla eller ogiltiga händelse-ID:n.")}
            entries.append(entry);if entries.count>500 {entries.removeFirst()};offset=newline+1
        }
    }
    @discardableResult public func append(_ entry:EvaluationEvent) throws -> Bool {
        guard writable else{throw BenchError.invalid("Loggen behöver öppnas på nytt efter ett skrivfel. Starta om appen och öppna den sparade körningen.")}
        if ids.contains(entry.id){return false}
        writable=false
        var data=try canonical(entry);data.append(10)
        let existed=FileManager.default.fileExists(atPath:url.path)
        let fd=open(url.path,O_CREAT|O_APPEND|O_WRONLY,0o600)
        guard fd>=0 else{throw BenchError.invalid("Kunde inte öppna loggen. Körningen stoppas.")};defer{close(fd)}
        try data.withUnsafeBytes { buffer in
            var at=0
            while at<buffer.count {
                let n=write(fd,buffer.baseAddress!.advanced(by:at),buffer.count-at)
                guard n>0 else{throw BenchError.invalid("Kunde inte skriva loggen. Körningen stoppas.")};at+=n
            }
        }
        guard fsync(fd)==0 else{throw BenchError.invalid("Kunde inte spara loggen säkert.")}
        if !existed {
            let directory=open(url.deletingLastPathComponent().path,O_RDONLY);guard directory>=0 else{throw BenchError.invalid("Loggmappen kan inte öppnas.")};defer{close(directory)}
            guard fsync(directory)==0 else{throw BenchError.invalid("Loggmappen kunde inte synkroniseras.")}
        }
        writable=true;ids.insert(entry.id);entries.append(entry);if entries.count>500{entries.removeFirst()};return true
    }
}
