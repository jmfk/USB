#!/usr/bin/env python3
"""Explicit diagnostic commands only. No updater, host transition or flash entrypoint."""
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import secrets
import struct
import sys

class LoggedLink:
    def __init__(self,link,event):self.link=link;self.event=event
    def request(self,cmd,data=()):
        names={1:'Armerar ljudprovet.',2:'Läser ljudets frysta mätvärden.',3:'Kvitterar det säkert sparade ljudresultatet.',4:'Kontrollerar ljudprovets status.',9:'Läser MIDI-prestanda och enhetens logg.',10:'Kvitterar den säkert sparade profilen.',11:'Kontrollerar firmware och kör-ID.',12:'Armerar profilens kör-ID.',13:'Läser CPU, ljudmarginal och klockräknare.'}
        self.event('info',names.get(cmd,'Kommunicerar med enheten.'))
        return self.link.request(cmd,data)

def execute(command, root, config, ident, link, c, p, event=lambda level,message:None):
    link=LoggedLink(link,event)
    device=root/'device';device.mkdir(parents=True,exist_ok=True)
    audio_path=root/'audio-arm.json'
    if command=='arm':
        report=p.arm(link,device,ident)
        if config['audio']:
            if audio_path.exists():
                audio=c.load(audio_path)
                if (audio['rate'],audio['duration'])!=(config['sampleRate'],config['duration']*1000):raise ValueError('Audio configuration changed')
            else:
                audio=dict(token=secrets.randbelow(0x0fffffff)+1,rate=config['sampleRate'],duration=config['duration']*1000)
                c.atomic(audio_path,audio)
            status=struct.unpack('<4I',link.request(4))
            if status[0]!=ident['build_tag'] or status[3]:raise ValueError('Wrong build or unread audio result')
            if status[1:3]!=(1,audio['token']):
                if status[1]==1:raise ValueError('Another audio attempt is armed')
                link.request(1,c.seven(audio['token'],4)+c.seven(audio['duration'],3)+c.seven(audio['rate'],3)+[25,0])
            status=struct.unpack('<4I',link.request(4))
            if status[1:3]!=(1,audio['token']):raise ValueError('Audio ARM readback mismatch')
        event('info','Armeringen är verifierad. Väntar på kabelbyte och fysisk PLAY.')
        return report
    report=p.fetch(link,device,ident)
    result=c.load(device/'result.json');report=dict(report,profile=result['profile'])
    if config['audio']:
        audio=c.load(audio_path);path=root/'audio-result.json';status=struct.unpack('<4I',link.request(4))
        if path.exists():
            record=c.load(path);v=c.decode(bytes.fromhex(record['raw_hex']))
            if record['snapshot']!=v:raise ValueError('Audio result integrity mismatch')
        else:
            raw=link.request(2);v=c.decode(raw)
            record=dict(raw_hex=raw.hex(),snapshot=v)
        if v['build']!=ident['build_tag'] or v['run']!=audio['token'] or v['rate']!=audio['rate'] or v['duration']!=audio['duration']:
            raise ValueError('Audio run identity mismatch')
        if status[3] and status[2]!=audio['token']:raise ValueError('Another audio result is waiting')
        c.atomic(path,record)
        if status[3]:link.request(3,c.seven(audio['token'],4)+c.seven(v['crc32']&0x0fffffff,4))
        report['audio_snapshot']=v
    c.atomic(root/'device-report.json',report)
    event('info','Profil och loggar är verifierade och sparade.')
    return report

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('command',choices=['arm','fetch']);ap.add_argument('--root',required=True,type=Path)
    ap.add_argument('--firmware',required=True,type=Path);ap.add_argument('--port',required=True);args=ap.parse_args()
    sys.path.insert(0,str(args.firmware.resolve()));import collect as c;import performance as p
    root=args.root.resolve();manifest=c.load(root/'manifest.json');config=manifest['config'];ident=c.identity()
    if hashlib.sha256(json.dumps(config,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()).hexdigest()!=manifest['fingerprint']:raise ValueError('Host configuration fingerprint mismatch')
    if ident.get('profile_schema')!=1 or str(ident['build_tag'])!=config['firmware']:raise ValueError('Profiling package identity mismatch')
    # Separate bridge lock: Swift owns the run lock; collector owns its device journal.
    device=root/'device';device.mkdir(parents=True,exist_ok=True)
    with (root/'.bridge.lock').open('a') as lock,(device/'.lock').open('a') as device_lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        fcntl.flock(device_lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        link=c.Link(args.port)
        try:report=execute(args.command,root,config,ident,link,c,p,lambda level,message:print('USB_BENCH_EVENT '+json.dumps(dict(level=level,message=message),ensure_ascii=False),file=sys.stderr,flush=True))
        finally:link.close()
    print(json.dumps(report))
if __name__=='__main__':
    try:main()
    except (Exception,KeyboardInterrupt) as e:print(str(e),file=sys.stderr);sys.exit(1)
