import SwiftUI
import AppKit
import BenchCore

@MainActor final class BenchModel:ObservableObject {
    @Published var scenario=Scenario.all[0].id
    @Published var duration=10
    @Published var destination:Int32=0
    @Published var source:Int32=0
    @Published var audio=false
    @Published var rate=48000
    @Published var notes=""
    @Published var port="Felucca"
    @Published var destinations:[Endpoint]=[]
    @Published var sources:[Endpoint]=[]
    @Published var stage=EvaluationStage.setup
    @Published var destinationView="evaluation"
    @Published var events:[EvaluationEvent]=[]
    @Published var logSearch=""
    @Published var logLevel="all"
    @Published var error=""
    @Published var running=false
    @Published var result:HostResult?
    @Published var profile:[String:Any]?
    @Published var runPath=""
    @Published var palette=false
    @Published var query=""
    private var store:RunStore?
    private var journal:EventJournal?
    private let engine=MIDIEngine()
    var firmwareURL:URL {Bundle.main.resourceURL!.appendingPathComponent("firmware")}
    var build:String {(try? JSONSerialization.jsonObject(with:Data(contentsOf:firmwareURL.appendingPathComponent("identity.json"))) as? [String:Any]).flatMap {$0["build_tag"].map {String(describing:$0)}} ?? "saknas"}
    var firmwareRevision:String {(try? JSONSerialization.jsonObject(with:Data(contentsOf:firmwareURL.appendingPathComponent("identity.json"))) as? [String:Any]).flatMap {$0["revision"] as? String}?.components(separatedBy:"-").last?.uppercased() ?? "—"}
    var core0Status:String {
        guard let p=profile?["profile"] as? [String:Any],let value=p["instrumented_core0_percent"] as? Double else{return "—"}
        return String(format:"%.1f %%",value)
    }
    var revision:String {(try? String(contentsOf:Bundle.main.resourceURL!.appendingPathComponent("app-revision.txt"),encoding:.utf8)) ?? "development"}
    var canCreateNew:Bool {!stage.isBusy && !(store != nil && profile==nil && FileManager.default.fileExists(atPath:store!.root.appendingPathComponent("device/manifest.json").path))}
    var commandsAllowed:Bool {store.map{$0.manifest.config.firmware==build && $0.manifest.config.appRevision==revision} ?? true}
    var deviceOutcome:String {
        guard let m=profile?["metrics"] as? [String:Any],let state=m["state"] as? Int else{return ""}
        switch state {
        case 4:return "FM-1 slutförde den inställda körningen."
        case 6:return "FM-1 stoppades med det fysiska reglaget."
        case 5:
            let reasons=["okänd orsak","ljudenheter saknas","USB-registerfel","missad ljuddeadline","USB-överföringsfel","USB-frameklocka saknas","ljudbuffertfel","ljudprovet var inte armerat","fel vid enhetskonfigurering","ofullständig körning","MIDI-routingfel","MIDI-portar saknas"]
            let reason=m["reason"] as? Int ?? 0
            return "FM-1 avbröt körningen: \(reasons.indices.contains(reason) ? reasons[reason]:"okänd orsak")."
        default:return "Enhetsstatus finns i mätvärdenas detaljer."
        }
    }
    var summary:String {"\(Scenario.all.first(where:{$0.id==scenario})?.name ?? scenario) · \(duration) s"+(audio ? " · ljud \(rate) Hz":"")}
    var filteredEvents:[EvaluationEvent] {events.filter{(logLevel=="all" || $0.level==logLevel) && (logSearch.isEmpty || ($0.source+" "+$0.message).localizedCaseInsensitiveContains(logSearch))}}
    func refresh(){destinations=endpoints(output:true);sources=endpoints(output:false)}
    @discardableResult private func record(_ message:String,source:String="App",level:String="info",id:String?=nil)->Bool {
        let event=EvaluationEvent(id:id ?? UUID().uuidString,source:source,level:level,message:message)
        do {
            if let journal {try journal.append(event);events=journal.entries}
            else {events.append(event);if events.count>500{events.removeFirst()}}
            return true
        }catch{self.error="Loggen kunde inte sparas: \(error.localizedDescription)";if running{engine.stop()};return false}
    }
    private func transition(_ next:EvaluationStage) throws {
        guard stage.allows(next) else{throw BenchError.invalid("Ogiltigt steg i utvärderingen. Ingen ny mätning startades.")}
        if let store {try atomicWrite(try canonical(["stage":next.rawValue]),to:store.root.appendingPathComponent("workflow.json"))}
        guard record(next.title) else{throw BenchError.invalid(error)}
        stage=next
    }
    func newRun() {
        guard canCreateNew else{fail("Slutför den armerade körningen och hämta dess resultat innan du startar en ny.");return}
        store=nil;journal=nil;runPath="";result=nil;profile=nil;error="";events=[];stage = .setup;destinationView="evaluation"
    }
    private func createRun() throws {
        let selected=Scenario.all.first(where:{$0.id==scenario})!
        if selected.id != "idle" {
            refresh()
            guard sources.contains(where:{$0.id==source}),selected.messagesPerSecond==0 || destinations.contains(where:{$0.id==destination}) else {throw BenchError.invalid("Välj MIDI-ingång och eventuell utgång under Anslutning. Macens MIDI-interface ska vara anslutet även när FM-1 är i device-läge.")}
        }
        let config=Configuration(scenario:selected,duration:duration,destination:destination,source:source,audio:audio,sampleRate:rate,notes:notes,firmware:build,appRevision:revision)
        let base=try FileManager.default.url(for:.applicationSupportDirectory,in:.userDomainMask,appropriateFor:nil,create:true).appendingPathComponent("USB Bench/runs")
        let created=try RunStore(root:base.appendingPathComponent(UUID().uuidString),config:config)
        let log=try EventJournal(root:created.root)
        store=created;journal=log;runPath=created.root.path;events=[];result=nil;profile=nil
        try created.state("prepared");try transition(.connectDevice)
    }
    func primary() {
        guard !stage.isBusy,commandsAllowed else{return};error=""
        do {
            switch stage {
            case .setup:try createRun()
            case .connectDevice:bridge("arm")
            case .connectHost:start()
            case .connectForRead,.interrupted:bridge("fetch")
            case .finished:destinationView="results"
            default:break
            }
        }catch{fail(error.localizedDescription)}
    }
    private func fail(_ text:String){error=text;record(text,level:"error")}
    func openRun() {
        guard !stage.isBusy else{return}
        let panel=NSOpenPanel();panel.canChooseDirectories=true;panel.canChooseFiles=false
        guard panel.runModal() == .OK,let url=panel.url else{return}
        do {
            if store?.root==url {destinationView="evaluation";return}
            let s=try RunStore(root:url),log=try EventJournal(root:url),r=try s.recover()
            store=s;journal=log;events=log.entries;runPath=url.path;result=r;error=""
            let c=s.manifest.config;scenario=c.scenario.id;duration=c.duration;destination=c.destination;source=c.source;audio=c.audio;rate=c.sampleRate;notes=c.notes
            try loadProfile()
            let armed=(try? String(contentsOf:url.appendingPathComponent("ARMED"),encoding:.utf8))==s.manifest.fingerprint
            let complete=FileManager.default.fileExists(atPath:url.appendingPathComponent("HOST_COMPLETE").path)
            let started=FileManager.default.fileExists(atPath:url.appendingPathComponent("started.json").path)
            stage=EvaluationStage.recover(started:started,hostComplete:complete,armed:armed,deviceFetched:profile != nil)
            if !commandsAllowed {error="Öppnad för visning. Fortsatt enhetskommunikation kräver samma app- och firmwareversion som körningen skapades med."}
            destinationView=profile != nil ? "results":"evaluation"
            record("Sparad körning öppnad. Ingen testtrafik startas automatiskt.")
        }catch{fail(error.localizedDescription)}
    }
    private func start() {
        guard let store,stage == .connectHost,!running else{return}
        do {
            let armed=try String(contentsOf:store.root.appendingPathComponent("ARMED"),encoding:.utf8)
            guard armed==store.manifest.fingerprint else{throw BenchError.invalid("Armeringens kör-ID stämmer inte. Ingen testtrafik startades.")}
            try transition(.running);try store.start()
        }catch{
            stage=EvaluationStage.recover(started:FileManager.default.fileExists(atPath:store.root.appendingPathComponent("started.json").path),hostComplete:false,armed:true,deviceFetched:false)
            fail(error.localizedDescription);return
        }
        running=true
        engine.run(config:store.manifest.config,progress:{[weak self] r in
            Task {@MainActor in
                guard let self,self.running else{return};self.result=r
                do {
                    try store.checkpoint(r)
                    let level=r.sendErrors>0 || r.skippedSends>0 ? "warning":"info"
                    self.record(String(format:"%.0f s · skickat %d · mottaget %d · överhoppat %d · sändfel %d",r.elapsed,r.sent,r.received,r.skippedSends,r.sendErrors),source:"MIDI",level:level)
                }catch{self.fail(error.localizedDescription);self.engine.stop()}
            }
        },finished:{[weak self] r in
            Task {@MainActor in
                guard let self else{return};self.running=false;self.result=r
                do {
                    try store.checkpoint(r)
                    if r.sendErrors>0 {self.record("MIDI kunde inte slutföras. Kontrollera att det valda interfacet och returvägen är anslutna.",source:"MIDI",level:"error")}
                    try self.transition(r.status=="complete" ? .connectForRead:.interrupted)
                }catch{self.stage = .interrupted;self.fail(error.localizedDescription)}
            }
        })
    }
    func stop(){if running {record("Avbryter testtrafiken. Försöket kommer inte att spelas om.",level:"warning");engine.stop()}}
    func reveal(){if !runPath.isEmpty {NSWorkspace.shared.open(URL(fileURLWithPath:runPath))}}
    private func loadProfile() throws {
        guard let store else{return}
        let path=store.root.appendingPathComponent("device-report.json")
        profile=FileManager.default.fileExists(atPath:path.path) ? try JSONSerialization.jsonObject(with:Data(contentsOf:path)) as? [String:Any]:nil
    }
    private func importDeviceLog() {
        guard let profile,let entries=profile["log"] as? [[String:Any]] else{return}
        for (index,event) in entries.enumerated() {
            let name=event["event"] as? String ?? "händelse"
            let ms=(event["elapsed_ms"] as? NSNumber)?.intValue ?? 0
            let a=(event["a"] as? NSNumber)?.uint32Value ?? 0,b=(event["b"] as? NSNumber)?.uint32Value ?? 0
            let detail:String
            switch name {
            case "start":detail="Körstart · \(a) MIDI-ingångar, \(b) utgångar"
            case "sample":detail="MIDI-paket mottagna \(a), skickade \(b)"
            case "fault":detail="MIDI-fel \(a) · ogiltiga meddelanden \(b)"
            case "stop":detail="Stopp · status \(a), orsak \(b)"
            case "cleanup":detail="Avslut · status \(a), cleanup-fel \(b)"
            default:detail=name
            }
            record("+\(ms) ms · \(detail)",source:"FM-1 · fryst",level:name=="fault" ? "error":"info",id:"device-\(index)")
        }
        if let metrics=profile["metrics"] as? [String:Any],let dropped=metrics["log_dropped"] as? Int,dropped>0 {
            record("Enhetens logg är begränsad: \(dropped) äldre händelser finns inte med i den frysta loggen.",source:"FM-1 · fryst",level:"warning",id:"device-dropped")
        }
    }
    private func bridge(_ command:String) {
        guard let store,commandsAllowed else{return}
        do {try transition(command=="arm" ? .arming:.fetching)}catch{fail(error.localizedDescription);return}
        let resources=Bundle.main.resourceURL!,selectedPort=port
        Task {
            do {
                try await BridgeRunner.run(command:command,root:store.root,resources:resources,port:selectedPort) {[weak self] level,message in
                    await self?.bridgeEvent(level,message)
                }
                if command=="arm" {try atomicWrite(Data(store.manifest.fingerprint.utf8),to:store.root.appendingPathComponent("ARMED"));try transition(.connectHost)}
                else {
                    try loadProfile();importDeviceLog()
                    try transition(result?.status=="complete" ? .finished:.interrupted)
                    destinationView="results"
                }
            }catch{
                stage=command=="arm" ? .connectDevice:.connectForRead
                fail(error.localizedDescription+" Behåll strömmen, kontrollera USB-device-anslutningen och försök samma steg igen.")
            }
        }
    }
    private func bridgeEvent(_ level:String,_ message:String){record(message,source:"Hämtning",level:level)}
}
@main struct USBBenchApp:App {
    @StateObject private var model=BenchModel()
    @AppStorage("appearance") private var appearance="system"
    init() {
        #if DEBUG
        if let i=CommandLine.arguments.firstIndex(of:"--checkpoint-fixture"),CommandLine.arguments.count>i+1 {
            do {
                let config=Configuration(scenario:Scenario.all[1],duration:10,destination:1,source:2,audio:false,sampleRate:48000,notes:"fixture / Ω",firmware:"123",appRevision:"test")
                let store=try RunStore(root:URL(fileURLWithPath:CommandLine.arguments[i+1]),config:config)
                let journal=try EventJournal(root:store.root);try journal.append(EvaluationEvent(id:"fixture-event",message:"Sparad före avbrott"))
                try store.start();try store.checkpoint(HostResult(status:CommandLine.arguments.contains("--complete") ? "complete":"running",sent:17,received:16))
                if CommandLine.arguments.contains("--complete") {exit(0)}
                print("DURABLE");fflush(stdout)
                while true {Thread.sleep(forTimeInterval:1)}
            }catch{exit(2)}
        }
        #endif
    }
    var body:some Scene {
        WindowGroup {
            BenchView(model:model).frame(minWidth:860,minHeight:680)
                .preferredColorScheme(appearance=="light" ? .light:appearance=="dark" ? .dark:nil)
        }.commands {
            CommandGroup(after:.newItem) {
                Button("Ny utvärdering",action:model.newRun).keyboardShortcut("n").disabled(!model.canCreateNew)
                Button("Öppna körning…",action:model.openRun).keyboardShortcut("o").disabled(model.stage.isBusy)
                Button("Sök kommandon",action:{model.palette.toggle()}).keyboardShortcut("k")
            }
            CommandMenu("Utvärdering") {
                Button("Fortsätt utvärdering",action:model.primary).keyboardShortcut(.return).disabled(model.stage.isBusy || !model.commandsAllowed)
                Button("Avbryt mätning",action:model.stop).keyboardShortcut(".").disabled(!model.running)
                Divider()
                Button("Utvärdering",action:{model.destinationView="evaluation"}).keyboardShortcut("1")
                Button("Resultat",action:{model.destinationView="results"}).keyboardShortcut("2")
                Button("Loggar",action:{model.destinationView="logs"}).keyboardShortcut("3")
                Divider()
                Button("Uppdatera MIDI-portar",action:model.refresh).keyboardShortcut("r")
                Button("Visa resultatmapp",action:model.reveal).keyboardShortcut("e",modifiers:[.command,.shift]).disabled(model.runPath.isEmpty)
            }
            CommandMenu("Utseende") {Picker("Utseende",selection:$appearance){Text("System").tag("system");Text("Ljust").tag("light");Text("Mörkt").tag("dark")}}
        }
    }
}
struct BenchView:View {
    @ObservedObject var model:BenchModel
    @State private var connectionOpen=false
    @State private var detailsOpen=false
    @State private var followLogs=true
    var body:some View {
        VStack(alignment:.leading,spacing:20) {
            HStack(alignment:.top) {
                VStack(alignment:.leading,spacing:5) {
                    Text("USB Bench").font(.largeTitle.bold())
                    Text("Firmwarepaket \(model.firmwareRevision) · build \(model.build)").font(.caption).foregroundStyle(.secondary)
                    Text("Fryst profil · CPU 0: \(model.core0Status) · CPU 1: ej använd").font(.callout).foregroundStyle(.secondary)
                }
                Spacer()
                Picker("Vy",selection:$model.destinationView) {
                    Text("Utvärdering").tag("evaluation");Text("Resultat").tag("results");Text("Loggar").tag("logs")
                }.pickerStyle(.segmented).frame(width:320).help("⌘1 Utvärdering · ⌘2 Resultat · ⌘3 Loggar")
            }
            if !model.commandsAllowed {Text("Tidigare appversion · öppnad endast för visning").font(.caption).foregroundStyle(.secondary)}
            Divider()
            if model.destinationView=="logs" {logs}
            else {ScrollView {
                VStack(alignment:.leading,spacing:24) {
                    if model.destinationView=="results" {results}else{evaluation}
                }.frame(maxWidth:.infinity,alignment:.leading).padding(.bottom,12)
            }}
        }.padding(28)
            .sheet(isPresented:$model.palette){palette.padding(24).frame(width:480,height:380)}
    }
    var evaluation:some View {
        VStack(alignment:.leading,spacing:22) {
            if model.stage == .setup {
                VStack(alignment:.leading,spacing:14) {
                    Text(model.stage.title).font(.title2.bold())
                    Text(model.stage.instruction).foregroundStyle(.secondary)
                    HStack {
                        Picker("Scenario",selection:$model.scenario){ForEach(Scenario.all){Text($0.name).tag($0.id)}}
                        Picker("Tid",selection:$model.duration){Text("10 s").tag(10);Text("1 min").tag(60);Text("5 min").tag(300)}.frame(width:170)
                    }
                    DisclosureGroup("Anslutning och ljud",isExpanded:$connectionOpen) {connection.padding(.top,12)}
                }
            }else {
                VStack(alignment:.leading,spacing:12) {
                    HStack {
                        if model.stage == .connectDevice || model.stage == .connectHost || model.stage == .connectForRead || model.stage == .interrupted {
                            Label("Väntar på dig",systemImage:"pause.circle").foregroundStyle(.secondary)
                        }else if model.stage.isBusy {Label("Pågår",systemImage:"waveform.path").foregroundStyle(.secondary)}
                        else {Label("Sparat",systemImage:"checkmark.circle").foregroundStyle(.secondary)}
                        Spacer();Text(model.summary).font(.callout).foregroundStyle(.secondary)
                    }
                    Text(model.stage.title).font(.title.bold())
                    Text(model.stage.instruction).fixedSize(horizontal:false,vertical:true)
                    if model.stage == .connectHost {
                        if model.audio {Text("Starta båda ljudkällorna. Ljudprovet och Macens trafik startas separat; notera när du börjar.").foregroundStyle(.secondary)}
                        else {Text("Välj MIDI ONLY på FM-1. Macens separata MIDI-interface används för testtrafiken.").foregroundStyle(.secondary)}
                        if model.scenario=="clock" {Text("Aktivera intern klocka på SYNC/CLK och välj rätt utgång på SYNC/OUT.").foregroundStyle(.secondary)}
                    }
                    if model.stage == .running {
                        ProgressView(value:min(model.result?.elapsed ?? 0,Double(model.duration)),total:Double(model.duration))
                        if let r=model.result {HStack(spacing:28){metric("Tid",String(format:"%.0f / %d s",min(r.elapsed,Double(model.duration)),model.duration));metric("Skickat",String(r.sent));metric("Mottaget",String(r.received))}}
                    }
                }
            }
            if !model.error.isEmpty {
                Label(model.error,systemImage:"exclamationmark.circle").foregroundStyle(.red).textSelection(.enabled).fixedSize(horizontal:false,vertical:true)
                if model.stage == .connectDevice || model.stage == .connectForRead {TextField("FM-1-port vid anslutning till Mac",text:$model.port).help("Ange ett entydigt namn om flera MIDI-portar matchar Felucca.")}
            }
            HStack {
                Button(model.stage.action,action:model.primary).buttonStyle(.borderedProminent).controlSize(.large)
                    .disabled(model.stage.isBusy || !model.commandsAllowed).help("⌘Return · Fortsätt det visade steget")
                if model.running {Button("Avbryt mätning",action:model.stop).help("⌘.")}
                Spacer()
                if !model.stage.isBusy && model.stage != .setup {Button("Ny utvärdering",action:model.newRun).disabled(!model.canCreateNew).help("⌘N · Slutför eller hämta det armerade försöket först.")}
            }
            if let last=model.events.last,last.message != model.stage.title {
                VStack(alignment:.leading,spacing:7) {
                    Text("Senaste händelse").font(.caption).foregroundStyle(.secondary)
                    Text(last.message).font(.callout).fixedSize(horizontal:false,vertical:true)
                }.padding(.top,8)
            }
            if !model.events.isEmpty {Button("Visa loggar"){model.destinationView="logs"}.buttonStyle(.link).help("⌘3")}
            if model.stage == .setup {Text("Första gången? Börja med 10 sekunder. Firmware installeras manuellt. Appen ber om kabelbyte, PLAY och STOP när det behövs.").font(.callout).foregroundStyle(.secondary)}
        }
    }
    var connection:some View {
        VStack(alignment:.leading,spacing:14) {
            HStack {
                Picker("MIDI ut",selection:$model.destination){Text("Välj utgång").tag(Int32(0));ForEach(model.destinations){Text($0.name).tag($0.id)}}
                Picker("MIDI in",selection:$model.source){Text("Välj ingång").tag(Int32(0));ForEach(model.sources){Text($0.name).tag($0.id)}}
                Button("Uppdatera",action:model.refresh).help("⌘R")
            }
            Text("Mac → separat MIDI-interface → FM-1:s hostkedja → MIDI-interface → Mac. Proberna måste routas tillbaka för rundresemätning.").font(.callout).foregroundStyle(.secondary)
            Toggle("Två externa ljudkällor och en USB-ljudutgång",isOn:$model.audio)
            if model.audio {Picker("Samplingsfrekvens",selection:$model.rate){Text("44,1 kHz").tag(44100);Text("48 kHz").tag(48000)}.frame(maxWidth:300)}
            TextField("FM-1-port vid anslutning till Mac",text:$model.port)
            TextField("Noteringar om koppling och testvillkor",text:$model.notes)
        }
    }
    var logs:some View {
        VStack(alignment:.leading,spacing:12) {
            HStack {
                TextField("Sök i loggar",text:$model.logSearch)
                Toggle("Följ live",isOn:$followLogs).toggleStyle(.checkbox)
                Picker("Nivå",selection:$model.logLevel){Text("Alla").tag("all");Text("Varningar").tag("warning");Text("Fel").tag("error")}.frame(width:180)
                Button("Resultatmapp",action:model.reveal).disabled(model.runPath.isEmpty).help("⇧⌘E")
            }
            Text("Händelser från appen visas löpande. FM-1:s frysta logg visas efter hämtning. De senaste 500 händelserna visas här; hela loggen sparas på disk.").font(.callout).foregroundStyle(.secondary)
            if model.filteredEvents.isEmpty {
                Text(model.events.isEmpty ? "Inga händelser ännu. Starta en utvärdering för att följa den här.":"Inga händelser matchar filtret.").foregroundStyle(.secondary).frame(maxWidth:.infinity,maxHeight:.infinity,alignment:.topLeading).padding(.top,16)
            }else {
                ScrollViewReader {proxy in
                    ScrollView {
                        LazyVStack(alignment:.leading,spacing:12) {
                            ForEach(model.filteredEvents){entry in
                                HStack(alignment:.top,spacing:12) {
                                    Text(entry.date,format:.dateTime.hour().minute().second()).monospacedDigit().foregroundStyle(.secondary).frame(width:70,alignment:.leading)
                                    Text(entry.source).foregroundStyle(.secondary).frame(width:110,alignment:.leading)
                                    Text(entry.message).frame(maxWidth:.infinity,alignment:.leading).foregroundStyle(entry.level=="error" ? Color.red:entry.level=="warning" ? Color.orange:Color.primary)
                                }.font(.callout).textSelection(.enabled).id(entry.id)
                            }
                        }.padding(.vertical,4)
                    }.onChange(of:model.events.last?.id){_,id in if let id,followLogs,model.logSearch.isEmpty {proxy.scrollTo(id,anchor:.bottom)}}
                }
            }
        }
    }
    var results:some View {
        VStack(alignment:.leading,spacing:22) {
            Text("Resultat").font(.title2.bold())
            Text(model.summary).foregroundStyle(.secondary)
            if !model.notes.isEmpty {Text(model.notes).font(.callout).textSelection(.enabled)}
            if model.result==nil && model.profile==nil {
                Text("Resultaten visas när mätningen är sparad och enhetens profil har hämtats.").foregroundStyle(.secondary)
                Button("Till utvärderingen"){model.destinationView="evaluation"}
            }else {
                if model.result?.status != "complete" {Label("Testtrafiken är ofullständig. Resultaten beskriver ett partiellt försök.",systemImage:"exclamationmark.circle").foregroundStyle(.orange)}
                else if model.profile==nil {Text("Macens trafik är sparad. Enhetens profil återstår att hämta.").foregroundStyle(.secondary)}
                else {Text("Mätvärden och loggar är sparade. Granska fel och mätvillkor innan du jämför körningar.").foregroundStyle(.secondary)}
                if let r=model.result {
                    HStack(spacing:32){metric("Mottagna prober","\(r.received) / \(r.sent)");metric("Rundresa p99",value(r.latencyP99MS)+" ms");metric("Sändfel",String(r.sendErrors))}
                    if r.sent>r.received {Text("\(r.sent-r.received) prober saknades i retur vid mätfönstrets slut.").foregroundStyle(.orange)}
                    Text("Överhoppade sändningar: \(r.skippedSends) · dubbletter: \(r.duplicates) · omordnade: \(r.reordered)").font(.callout)
                    if model.scenario=="clock" {Text("Klocka: \(r.clockEvents) pulser · \(r.clockCoalesced) utan separat tidsstämpel. Intervall medel/min/max: \(value(r.clockIntervalMeanMS)) / \(value(r.clockIntervalMinMS)) / \(value(r.clockIntervalMaxMS)) ms.")}
                }
                if let p=model.profile?["profile"] as? [String:Any] {
                    let a=p["audio"] as? [String:Any] ?? [:]
                    HStack(spacing:32){metric("Ljudets observerade marginal",value(a["observed_frame_margin_min_us"] as? Double)+" µs");metric("Största timerintervall",value(p["timer_gap_max_us"] as? Double)+" µs")}
                    Text(model.deviceOutcome).font(.callout)
                    DisclosureGroup("Mätvärden i detalj",isExpanded:$detailsOpen) {
                        VStack(alignment:.leading,spacing:12) {
                            if let metrics=model.profile?["metrics"] as? [String:Any] {
                                Text(verbatim:"Enhetsstatus: \(metrics["state"] ?? "—") · felorsak: \(metrics["reason"] ?? "—") · MIDI-fel: \(metrics["fault"] ?? 0) · cleanup-fel: \(metrics["cleanup_errors"] ?? 0)")
                            }
                            if a["enabled"] as? Bool==true {
                                Text(verbatim:"Buffertminimum A/B: \(value(a["buffer_a_min_us"] as? Double)) / \(value(a["buffer_b_min_us"] as? Double)) µs · sena frames \(a["late_frames"] ?? 0) · missade \(a["missed_frames"] ?? 0)")
                                Text(verbatim:"Underflows: \(a["underflows"] ?? []) · overflows: \(a["overflows"] ?? [])")
                            }
                            if let m=p["midi"] as? [String:Any] {Text(verbatim:"MIDI kömax in/ut: \(m["sampled_input_queue_max"] ?? 0) / \(m["sampled_output_queue_max"] ?? 0) · backpressure \(m["backpressure"] ?? 0) · retries \(m["retries"] ?? 0)");Text(verbatim:"Clock köad/överhoppad: \(m["clock_queued"] ?? 0) / \(m["clock_skipped"] ?? 0)")}
                            if let lanes=p["lanes"] as? [String:[String:Any]] {ForEach(["timer","audio_irq","display","host","mix","midi"],id:\.self){name in
                                HStack {Text(name).frame(width:90,alignment:.leading);Text(value(lanes[name]?["wall_percent"] as? Double)+" %");Spacer();Text("max "+value(lanes[name]?["max_us"] as? Double)+" µs")}.monospacedDigit()
                            }}
                        }.padding(.top,12)
                    }
                }
                Text("CPU-värdet avser instrumenterat arbete. Host, mix och MIDI ingår i timern. Rundresan omfattar hela MIDI-kedjan; ljudmarginalen utgår från observerad USB-frame. Detta är inte ett automatiskt godkännande av hårdvara.").font(.callout).foregroundStyle(.secondary)
                HStack {Button("Visa loggar"){model.destinationView="logs"}.help("⌘3");Button("Resultatmapp",action:model.reveal).help("⇧⌘E");Spacer();Button("Ny utvärdering",action:model.newRun).disabled(!model.canCreateNew)}
            }
        }
    }
    func metric(_ title:String,_ value:String)->some View {VStack(alignment:.leading,spacing:4){Text(title).font(.caption).foregroundStyle(.secondary);Text(value).font(.title2).monospacedDigit()}}
    func value(_ x:Double?)->String{x.map{String(format:"%.2f",$0)} ?? "—"}
    var palette:some View {
        VStack(alignment:.leading,spacing:10) {
            TextField("Sök kommandon",text:$model.query)
            let actions:[(String,()->Void,Bool)]=[
                ("Fortsätt utvärdering · ⌘Return",model.primary,!model.stage.isBusy && model.commandsAllowed),
                ("Ny utvärdering · ⌘N",model.newRun,model.canCreateNew),
                ("Öppna körning · ⌘O",model.openRun,!model.stage.isBusy),
                ("Utvärdering · ⌘1",{model.destinationView="evaluation"},true),
                ("Resultat · ⌘2",{model.destinationView="results"},true),
                ("Loggar · ⌘3",{model.destinationView="logs"},true),
                ("Avbryt mätning · ⌘.",model.stop,model.running),
                ("Uppdatera portar · ⌘R",model.refresh,!model.stage.isBusy),
                ("Resultatmapp · ⇧⌘E",model.reveal,!model.runPath.isEmpty)]
            ForEach(actions.indices,id:\.self){i in if model.query.isEmpty || actions[i].0.localizedCaseInsensitiveContains(model.query){Button(actions[i].0){model.palette=false;actions[i].1()}.disabled(!actions[i].2)}}
            Spacer();Button("Stäng"){model.palette=false}.keyboardShortcut(.cancelAction)
        }
    }
}
