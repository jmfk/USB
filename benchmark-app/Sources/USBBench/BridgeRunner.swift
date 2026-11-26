import Foundation
import BenchCore

enum BridgeRunner {
    static func run(command:String,root:URL,resources:URL,port:String,event:@escaping @Sendable (String,String) async -> Void) async throws {
        try await Task.detached(priority:.userInitiated) {
            let python=try String(contentsOf:resources.appendingPathComponent("python-path.txt"),encoding:.utf8).trimmingCharacters(in:.whitespacesAndNewlines)
            guard FileManager.default.isExecutableFile(atPath:python) else{throw BenchError.invalid("Projektets Pythonmiljö saknas. Bygg appen enligt README.")}
            let process=Process();process.executableURL=URL(fileURLWithPath:python)
            process.arguments=["-u",resources.appendingPathComponent("benchmark_bridge.py").path,command,"--root",root.path,"--firmware",resources.appendingPathComponent("firmware").path,"--port",port]
            let output=Pipe();process.standardOutput=output;process.standardError=output
            try process.run();var pending=Data(),failure:[String]=[]
            while true {
                let chunk=output.fileHandleForReading.availableData
                if chunk.isEmpty{break};pending.append(chunk)
                while let end=pending.firstIndex(of:10) {
                    let line=String(decoding:pending[..<end],as:UTF8.self);pending.removeSubrange(...end)
                    if line.hasPrefix("USB_BENCH_EVENT "),let data=line.dropFirst(16).data(using:.utf8),let value=try? JSONSerialization.jsonObject(with:data) as? [String:String],let message=value["message"] {
                        await event(value["level"] ?? "info",message)
                    }else if !line.isEmpty && !line.hasPrefix("{") {failure.append(line)}
                }
                if pending.count>1_000_000 {process.terminate();throw BenchError.invalid("Svaret är för stort. Hämtningen avbröts utan att starta om försöket.")}
            }
            if !pending.isEmpty {failure.append(String(decoding:pending,as:UTF8.self))}
            process.waitUntilExit()
            guard process.terminationStatus==0 else{throw BenchError.invalid(failure.suffix(8).joined(separator:"\n").isEmpty ? "Enhetskommandot avbröts. Kontrollera anslutningen och försök samma steg igen.":failure.suffix(8).joined(separator:"\n"))}
        }.value
    }
}
