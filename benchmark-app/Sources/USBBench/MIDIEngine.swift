import Foundation
import CoreMIDI
import Darwin
import BenchCore

struct Endpoint: Identifiable {let id:Int32;let ref:MIDIEndpointRef;let name:String}
func endpointName(_ ref:MIDIEndpointRef)->String {
    var name:Unmanaged<CFString>?
    if MIDIObjectGetStringProperty(ref,kMIDIPropertyDisplayName,&name)==noErr,let name {return name.takeRetainedValue() as String}
    return "MIDI \(ref)"
}
func endpoints(output:Bool)->[Endpoint] {
    (0..<(output ? MIDIGetNumberOfDestinations():MIDIGetNumberOfSources())).compactMap { i in
        let ref=output ? MIDIGetDestination(i):MIDIGetSource(i);var uid:Int32=0
        guard MIDIObjectGetIntegerProperty(ref,kMIDIPropertyUniqueID,&uid)==noErr else{return nil}
        return Endpoint(id:uid,ref:ref,name:endpointName(ref))
    }
}
private func seconds(_ ticks:UInt64)->Double {
    var info=mach_timebase_info_data_t();mach_timebase_info(&info)
    return Double(ticks)*Double(info.numer)/Double(info.denom)/1e9
}
final class MIDIEngine {
    private let queue=DispatchQueue(label:"USB.Bench.MIDI",qos:.userInitiated)
    private var client=MIDIClientRef(),input=MIDIPortRef(),output=MIDIPortRef()
    private var source=MIDIEndpointRef(),destination=MIDIEndpointRef()
    private var timer:DispatchSourceTimer?
    private var config:Configuration?
    private var start:UInt64=0,tag:UInt32=0,next=0,sent:[UInt32:Double]=[:],received=Set<UInt32>(),maxSequence:UInt32=0
    private var decoder=MIDIByteDecoder()
    private var rtt:[Double]=[],clock:[Double]=[],lastClock:Double?,lastCheckpoint:Double=0
    private var result=HostResult()
    private var progress:((HostResult)->Void)?,finished:((HostResult)->Void)?
    func run(config:Configuration,progress:@escaping(HostResult)->Void,finished:@escaping(HostResult)->Void) {
        queue.async {
            guard self.timer==nil else{return}
            self.config=config;self.progress=progress;self.finished=finished;self.result=HostResult(status:"running")
            self.sent=[:];self.received=[];self.rtt=[];self.clock=[];self.decoder=MIDIByteDecoder();self.lastClock=nil;self.lastCheckpoint=0;self.next=0;self.maxSequence=0
            self.tag=UInt32.random(in:1..<0x0fffffff)
            let destinations=endpoints(output:true),sources=endpoints(output:false)
            self.destination=destinations.first(where:{$0.id==config.destination})?.ref ?? 0
            self.source=sources.first(where:{$0.id==config.source})?.ref ?? 0
            let needsSource=config.scenario.id != "idle"
            if (config.scenario.messagesPerSecond>0 && self.destination==0) || (needsSource && self.source==0) {self.fail();return}
            if MIDIClientCreateWithBlock("USB Bench" as CFString,&self.client,nil) != noErr {self.fail();return}
            if self.source != 0 {
                if MIDIInputPortCreateWithBlock(self.client,"Benchmark receive" as CFString,&self.input,{[weak self] packets,_ in
                    // CoreMIDI owns the list only during this callback: copy bounded packet data.
                    var values:[(UInt64,[UInt8])]=[]
                    do {
                        var p=UnsafeRawPointer(packets).advanced(by:MemoryLayout<MIDIPacketList>.offset(of: \.packet)!).assumingMemoryBound(to:MIDIPacket.self)
                        for _ in 0..<packets.pointee.numPackets {
                            let stamp=p.pointee.timeStamp==0 ? mach_absolute_time():p.pointee.timeStamp
                            let offset=MemoryLayout<MIDIPacket>.offset(of: \.data)!
                            let bytes=Array(UnsafeRawBufferPointer(start:UnsafeRawPointer(p).advanced(by:offset),count:Int(p.pointee.length)))
                            values.append((stamp,bytes));p=UnsafePointer(MIDIPacketNext(p))
                        }
                    }
                    self?.queue.async {self?.consume(values)}
                }) != noErr || MIDIPortConnectSource(self.input,self.source,nil) != noErr {self.fail();return}
            }
            if self.destination != 0 && MIDIOutputPortCreate(self.client,"Benchmark send" as CFString,&self.output) != noErr {self.fail();return}
            self.start=mach_absolute_time()
            let t=DispatchSource.makeTimerSource(queue:self.queue);self.timer=t
            t.schedule(deadline:.now(),repeating:.milliseconds(1),leeway:.microseconds(50))
            t.setEventHandler {[weak self] in self?.tick()};t.resume()
        }
    }
    private func consume(_ packets:[(UInt64,[UInt8])]) {
        guard timer != nil,let config else{return}
        for (stamp,bytes) in packets {
            let at=stamp>=start ? seconds(stamp-start):0
            for event in decoder.consume(bytes,tag:tag,size:config.scenario.payloadBytes) {
                switch event {
                case .clock:
                    result.clockEvents+=1
                    if let last=lastClock {
                        if at>last,clock.count<1_000_000 {clock.append((at-last)*1000)}
                        else {result.clockCoalesced+=1}
                    }
                    lastClock=at
                case let .probe(n):
                    guard let begin=sent[n],at>=begin else{continue}
                    if !received.insert(n).inserted {result.duplicates+=1}
                    else {result.received+=1;rtt.append((at-begin)*1000);if n<maxSequence {result.reordered+=1};maxSequence=max(maxSequence,n)}
                }
            }
        }
    }
    private func tick() {
        guard let config else{return}
        let elapsed=seconds(mach_absolute_time()-start),rate=config.scenario.messagesPerSecond
        result.elapsed=elapsed
        if elapsed<Double(config.duration),rate>0 {
            let due=Int(elapsed*Double(rate))
            if due>=next {
                if due>next {result.skippedSends+=due-next;result.lateSends+=1}
                next=due+1
                let n=UInt32(due),bytes=Probe.bytes(tag:tag,sequence:n,size:config.scenario.payloadBytes)
                let storage=UnsafeMutableRawPointer.allocate(byteCount:2048,alignment:MemoryLayout<MIDIPacketList>.alignment)
                defer{storage.deallocate()};let list=storage.bindMemory(to:MIDIPacketList.self,capacity:1)
                let packet=MIDIPacketListInit(list)
                _ = bytes.withUnsafeBufferPointer {MIDIPacketListAdd(list,2048,packet,0,bytes.count,$0.baseAddress!)}
                let when=seconds(mach_absolute_time()-start)
                if MIDISend(output,destination,list) != noErr {result.sendErrors+=1;finish(complete:false);return}
                sent[n]=when;result.sent+=1
            }
        }
        if elapsed-lastCheckpoint>=1 {
            lastCheckpoint=elapsed
            // Stop on endpoint removal instead of counting a disconnected idle run as valid.
            if (source != 0 && !endpoints(output:false).contains(where:{$0.ref==source})) || (destination != 0 && !endpoints(output:true).contains(where:{$0.ref==destination})) {result.sendErrors+=1;finish(complete:false);return}
            progress?(snapshot())
        }
        if elapsed>=Double(config.duration)+1 {finish(complete:true)}
    }
    private func snapshot()->HostResult {
        var r=result;let sorted=rtt.sorted()
        if !sorted.isEmpty {r.latencyP50MS=sorted[(sorted.count-1)/2];r.latencyP99MS=sorted[Int(Double(sorted.count-1)*0.99)];r.latencyMaxMS=sorted.last}
        r.clockIntervals=clock.count
        if !clock.isEmpty {r.clockIntervalMeanMS=clock.reduce(0,+)/Double(clock.count);r.clockIntervalMinMS=clock.min();r.clockIntervalMaxMS=clock.max()}
        return r
    }
    private func fail(){result.sendErrors+=1;finish(complete:false)}
    private func finish(complete:Bool) {
        timer?.cancel();timer=nil
        if input != 0 {MIDIPortDispose(input);input=0};if output != 0 {MIDIPortDispose(output);output=0};if client != 0 {MIDIClientDispose(client);client=0}
        var r=snapshot();r.status=complete ? "complete":"interrupted";finished?(r)
        progress=nil;finished=nil;config=nil
    }
    func stop(){queue.async {if self.timer != nil {self.finish(complete:false)}}}
}
