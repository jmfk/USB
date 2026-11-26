#!/usr/bin/env python3
"""Read frozen performance diagnostics over MIDI after physical STOP. Never starts a run.
Offline report/compare/validate and --check open no MIDI. A run directory holds one
physical run; power loss cannot resume the physical stream. Fetch is resumable.
"""
import argparse
import secrets
import fcntl
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib
import collect as c

FIELDS='magic build sequence elapsed_ms state reason topology_hash route_hash rx_packets tx_packets events fanout retries backpressure invalid fault cleanup_errors sampled_input_queue_max sampled_output_queue_max service_max_us service_calls service_total_us log_count log_dropped audio_enabled'.split()
EVENTS={1:'start',2:'sample',3:'fault',4:'stop',5:'cleanup'}


def decode(raw):
    if len(raw)!=360 or zlib.crc32(raw[:-4])!=struct.unpack('<I',raw[-4:])[0]:
        raise ValueError('Diagnostic size/CRC mismatch')
    words=struct.unpack('<89I',raw[:-4]);v=dict(zip(FIELDS,words[:25]))
    if any(n>=1<<28 for n in words[25:29]):raise ValueError('Invalid run nonce')
    if v['magic']!=0x32445048 or v['log_count']>15 or v['audio_enabled']>1 or v['state'] not in (4,5,6):
        raise ValueError('Unsupported or unfinished diagnostic report')
    events=[]
    for i in range(v['log_count']):
        ms,kind,a,b=words[29+4*i:33+4*i]
        if kind not in EVENTS or (events and ms<events[-1]['elapsed_ms']):
            raise ValueError('Invalid diagnostic log')
        events.append(dict(elapsed_ms=ms,event=EVENTS[kind],a=a,b=b))
    seconds=v['elapsed_ms']/1000
    return dict(schema_version=2,metrics=v,nonce=list(words[25:29]),log=events,
        rates=dict(events_per_second=v['events']/seconds if seconds else None,
                   fanout_per_second=v['fanout']/seconds if seconds else None,
                   rx_packets_per_second=v['rx_packets']/seconds if seconds else None,
                   tx_packets_per_second=v['tx_packets']/seconds if seconds else None),
        crc32=struct.unpack('<I',raw[-4:])[0],hardware_verified=False,service_total_saturated=v['service_total_us']==0xffffffff,
        limitations='Service duration is not end-to-end MIDI latency or total CPU usage. Queue peaks are sampled. Packet counts exclude stop cleanup; cleanup_errors is finalized afterwards. Logs are bounded diagnostics, not MIDI payload capture.')


def validate(root):
    m=c.load(root/'manifest.json');record=c.load(root/'result.json');report=decode(bytes.fromhex(record['raw_hex']))
    if record['report']!=report or report['metrics']['build']!=m['build'] or report['nonce']!=m['nonce'] or record['manifest_sha256']!=hashlib.sha256(c.canonical(m)).hexdigest():
        raise ValueError('Local diagnostic identity/integrity mismatch')
    if c.load(root/'COMPLETE')!={'sha256':hashlib.sha256(c.canonical(record)).hexdigest()}:
        raise ValueError('Invalid completion checksum')
    return report


def commit(root,ident,raw):
    report=decode(raw)
    if report['metrics']['build']!=ident['build_tag']:raise ValueError('Unexpected diagnostic build')
    m=dict(schema_version=1,build=ident['build_tag'],source_sha256=ident['source_sha256'],
           run_id=root.name,nonce=report['nonce'],collector_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    if (root/'manifest.json').exists() and c.load(root/'manifest.json')!=m:
        raise ValueError('Run identity changed; choose a new directory')
    c.atomic(root/'manifest.json',m)
    record=dict(raw_hex=raw.hex(),report=report,manifest_sha256=hashlib.sha256(c.canonical(m)).hexdigest())
    if (root/'result.json').exists() and c.load(root/'result.json')!=record:
        raise ValueError('Physical run changed; choose a new directory')
    c.atomic(root/'result.json',record)
    c.atomic(root/'COMPLETE',dict(sha256=hashlib.sha256(c.canonical(record)).hexdigest()))
    return validate(root)


def fetch(link,root,ident):
    status=struct.unpack('<8I',link.request(11))
    if status[0]!=ident['build_tag']:raise ValueError('Unexpected firmware; no ACK sent')
    # Recover a crash after result fsync but before COMPLETE, without reading a new run.
    if (root/'result.json').exists() and not (root/'COMPLETE').exists():
        record=c.load(root/'result.json');commit(root,ident,bytes.fromhex(record['raw_hex']))
    if (root/'COMPLETE').exists():
        report=validate(root)
        m=c.load(root/'manifest.json')
        if m['build']!=ident['build_tag'] or m['source_sha256']!=ident['source_sha256']:
            raise ValueError('Package identity changed')
        if status[1] and (status[2],status[3],list(status[4:]))!=(report['metrics']['sequence'],report['crc32'],report['nonce']):
            raise ValueError('Another physical run is waiting; use a new directory')
        if status[1] and not any(report['nonce']):raise ValueError('Unarmed snapshot: use a new directory to preserve ambiguous data without acknowledging a later run')
    else:
        if not status[1]:raise ValueError('No stopped run waiting; run and STOP physically first')
        raw=link.request(9);report=decode(raw)
        if (report['metrics']['sequence'],report['crc32'],report['nonce'])!=(status[2],status[3],list(status[4:])):raise ValueError('Device report changed during download')
        report=commit(root,ident,raw)
    # COMPLETE is durable before ACK; lost ACKs only retry the same sequence/CRC.
    if status[1]:link.request(10,c.seven(report['metrics']['sequence'],4)+c.seven(report['crc32']&0x0fffffff,4))
    return report


def arm(link,root,ident):
    if (root/'result.json').exists() or (root/'COMPLETE').exists():raise ValueError('Run already collected; choose a new directory')
    if (root/'manifest.json').exists():
        m=c.load(root/'manifest.json')
        if m['build']!=ident['build_tag'] or m['source_sha256']!=ident['source_sha256'] or m['collector_sha256']!=hashlib.sha256(Path(__file__).read_bytes()).hexdigest():raise ValueError('Run identity changed')
    else:
        m=dict(schema_version=1,build=ident['build_tag'],source_sha256=ident['source_sha256'],run_id=root.name,
               nonce=[secrets.randbelow(0x0fffffff)+1 for _ in range(4)],collector_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
        c.atomic(root/'manifest.json',m)
    status=struct.unpack('<8I',link.request(11))
    if status[0]!=ident['build_tag'] or status[1]:raise ValueError('Wrong build or unread result; no arming sent')
    if any(status[4:]) and list(status[4:])!=m['nonce']:raise ValueError('Another run is already armed')
    link.request(12,sum((c.seven(n,4) for n in m['nonce']),[]))
    check=struct.unpack('<8I',link.request(11))
    if list(check[4:])!=m['nonce'] or check[1]:raise ValueError('Run ID readback failed')
    return dict(status='ARMED_ID_ONLY',nonce=m['nonce'],note='Use physical PLAY to start; keep power on until fetch')


def compare(a,b):
    keys=('topology_hash','route_hash','audio_enabled')
    if any(a['metrics'][k]!=b['metrics'][k] for k in keys):raise ValueError('Topology/routing/audio mode differ')
    return dict(builds=[a['metrics']['build'],b['metrics']['build']],
                elapsed_ms=[a['metrics']['elapsed_ms'],b['metrics']['elapsed_ms']],
                rates=[a['rates'],b['rates']],
                service_max_us=[a['metrics']['service_max_us'],b['metrics']['service_max_us']],
                hardware_verified=False,note='Observed runs only; matching hashes do not prove identical external input workload.')


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('command',nargs='?',choices=('arm','fetch','report','validate','compare'))
    ap.add_argument('--check',action='store_true');ap.add_argument('--run-dir',type=Path);ap.add_argument('--other',type=Path);ap.add_argument('--port',default='Felucca');args=ap.parse_args()
    if args.check:
        c.identity();assert 'mido' not in sys.modules;print('PASS: performance collector files; no MIDI/device access');return
    if not args.command or not args.run_dir:ap.error('Choose command and --run-dir')
    root=args.run_dir.resolve()
    if args.command in ('arm','fetch'):
        ident=c.identity();root.mkdir(parents=True,exist_ok=True)
        with (root/'.lock').open('a') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            link=c.Link(args.port)
            try:report=arm(link,root,ident) if args.command=='arm' else fetch(link,root,ident)
            finally:link.close()
    else:
        report=validate(root)
        if args.command=='compare':
            if not args.other:ap.error('compare requires --other')
            report=compare(report,validate(args.other.resolve()))
    if args.command!='arm':print("Durable results: 1/1 (100%); checkpoint: "+str(root),file=sys.stderr)
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    try:main()
    except (Exception,KeyboardInterrupt) as e:print('Stopped:',str(e),file=sys.stderr);sys.exit(1)
