import Foundation
public struct MIDIByteDecoder {
    public enum Event:Equatable {case clock;case probe(UInt32)}
    private var sysex:[UInt8]=[]
    public init(){}
    public mutating func consume(_ bytes:[UInt8],tag:UInt32,size:Int)->[Event] {
        var events:[Event]=[]
        for b in bytes {
            if b==0xf8 {events.append(.clock);continue}
            if b>=0xf8 {continue}
            if b==0xf0 {sysex=[b];continue}
            guard !sysex.isEmpty else{continue}
            if b==0xf7 {
                sysex.append(b)
                if let n=Probe.sequence(sysex,tag:tag,size:size){events.append(.probe(n))};sysex=[]
            }else if b>=0x80 || sysex.count>=1024 {sysex=[]}else{sysex.append(b)}
        }
        return events
    }
}
