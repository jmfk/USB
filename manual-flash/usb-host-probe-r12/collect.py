#!/usr/bin/env python3
"""Owner-operated arming/result collection. Never installs or starts host mode.

--check imports no MIDI/audio modules and opens no hardware.
Each physical run is uninterrupted; only already collected units are resumable.
"""
import argparse
import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import secrets
import struct
import sys
import time
import zlib

HERE = Path(__file__).resolve().parent
PREFIX = [0x7D, 0x55, 0x48, 1]
UNITS = {'canary': 10000, 'stress': 60000, 'long': 300000}
NAMES = '''magic build run rate duration state reason elapsed sof missed late addr_switches max_us io_errors enum_devices rejected a_packets a_frames a_errors a_min a_max a_under a_over a_peak_l a_peak_r a_hash a_first a_last b_packets b_frames b_errors b_min b_max b_under b_over b_peak_l b_peak_r b_hash b_first b_last out_packets out_frames out_errors out_peak out_hash a_id b_id out_id gain midi_inputs midi_outputs midi_rx midi_tx midi_events midi_fanout midi_idle midi_retries midi_invalid midi_pressure midi_fault midi_cleanup audio_enabled midi_route_hash midi_topology_hash'''.split()


def canonical(value):
    return (json.dumps(value, sort_keys=True, ensure_ascii=False, separators=(',', ':'))+'\n').encode()


def atomic(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name('.'+path.name+'.'+secrets.token_hex(6))
    try:
        with temp.open('xb') as f:
            f.write(canonical(value))
            f.flush()
            os.fsync(f.fileno())
        os.replace(temp, path)
        fd = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    finally:
        temp.unlink(missing_ok=True)


def load(path):
    return json.loads(Path(path).read_text())


def unpack7(data):
    out = bytearray(); acc = bits = 0
    for b in data:
        if b > 127:
            raise ValueError('Non-MIDI byte in reply')
        acc |= b << bits; bits += 7
        if bits >= 8:
            out.append(acc & 255); acc >>= 8; bits -= 8
    if acc:
        raise ValueError('Nonzero padding')
    return bytes(out)


def seven(value, count):
    if value < 0 or value >= 1 << (7*count):
        raise ValueError('Value outside wire range')
    return [(value >> (7*i)) & 127 for i in range(count)]


def decode(data):
    if len(data) != 332 or zlib.crc32(data[:-4]) != struct.unpack('<I', data[-4:])[0]:
        raise ValueError('Snapshot size/CRC mismatch')
    values = dict(zip(NAMES, struct.unpack('<64I', data[:256])))
    if values['magic'] != 0x31504855 or values['state'] not in (4, 5, 6):
        raise ValueError('No frozen completed/failed/stopped experiment')
    meta = struct.unpack('<18I', data[256:328])
    values['devices'] = [dict(zip(('vid', 'pid', 'path', 'max_packet', 'sync', 'route'), meta[i:i+6])) for i in (0, 6, 12)]
    values['crc32'] = struct.unpack('<I', data[-4:])[0]
    return values


def assessment(v):
    zero = ('missed', 'late', 'io_errors', 'a_errors', 'b_errors', 'out_errors',
            'a_under', 'b_under', 'a_over', 'b_over', 'midi_fault', 'midi_cleanup', 'midi_invalid')
    same_packets = v['a_packets'] == v['b_packets'] == v['out_packets']
    route_ids = [v[k] & 255 for k in ('a_id', 'b_id', 'out_id')]
    expected = v['duration']//1000*1000
    bus = (v['audio_enabled']==1 and v['state']==4 and not v['reason'] and all(v[k]==0 for k in zero) and
           same_packets and len(set(route_ids))==3 and all(route_ids) and
           v['out_packets']>=expected-2 and v['addr_switches']==3*v['out_packets'] and
           v['elapsed']>=v['duration'])
    activity=all(v[k]>0 for k in ('a_peak_l', 'a_peak_r', 'b_peak_l', 'b_peak_r'))
    return dict(bus_pass=bus, input_activity=activity,
                different_source_content=v['a_hash']!=v['b_hash'],
                qualified_for_next_unit=bus and activity and v['a_hash']!=v['b_hash'],
                input_drift_frames=[v['a_last']-v['a_first'], v['b_last']-v['b_first']],
                audio_integrity_verified=False, analog_output_verified=False, midi_hardware_verified=False,
                midi_activity=v['midi_rx']>0 and v['midi_tx']>0 and v['midi_fanout']>0,
                note='Packet counters and hashes are not independent audio/cross-routing measurements.')


def identity():
    ident = load(HERE/'identity.json')
    manifest = HERE/'manifest.json'
    hashes = load(manifest)['sha256'] if manifest.exists() else load(HERE/'sha256.json')
    for name, digest in hashes.items():
        if Path(name).name != name or hashlib.sha256((HERE/name).read_bytes()).hexdigest()!=digest:
            raise ValueError('Package file changed: '+name)
    return ident


class Link:
    def __init__(self, port):
        import mido
        ins = [n for n in mido.get_input_names() if port in n]
        outs = [n for n in mido.get_output_names() if port in n]
        if len(ins)!=1 or len(outs)!=1:
            raise ValueError('Need exactly one matching FM-1 MIDI input/output; choose --port precisely')
        self.mido = mido
        self.input = mido.open_input(ins[0])
        try:
            self.output = mido.open_output(outs[0])
        except BaseException:
            self.input.close()
            raise

    def close(self):
        self.input.close(); self.output.close()

    def request(self, cmd, data=()):
        for _ in self.input.iter_pending():
            pass
        self.output.send(self.mido.Message('sysex', data=PREFIX+[cmd]+list(data)))
        deadline = time.monotonic()+3
        while time.monotonic() < deadline:
            for message in self.input.iter_pending():
                p = list(message.data) if message.type=='sysex' else []
                if p[:5] == PREFIX+[cmd|0x40]:
                    if len(p)<6 or p[5]:
                        raise ValueError('Probe refused command: '+str(p[5:6]))
                    return unpack7(p[6:])
            time.sleep(.01)
        raise TimeoutError('No USB host probe reply; verify firmware and device mode')


def manifest(root, ident, rate, gain):
    root.mkdir(parents=True, exist_ok=True)
    path = root/'manifest.json'
    requested = dict(schema_version=1, run_id=root.name, source_sha256=ident['source_sha256'],
                     build=ident['build_tag'], rate=rate, gain=gain, units=UNITS,
                     collector_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    requested['config_fingerprint'] = hashlib.sha256(canonical(requested)).hexdigest()
    if path.exists():
        if load(path)!=requested:
            raise ValueError('Run build/config changed; use a new run directory')
    else:
        atomic(path, requested)
    return requested


def verify_record(record,m):
    v=record['snapshot'];meta=[]
    for dev in v['devices']:
        meta.extend(dev[k] for k in ('vid','pid','path','max_packet','sync','route'))
    raw=struct.pack('<64I',*(v[k] for k in NAMES))+struct.pack('<18I',*meta)+struct.pack('<I',v['crc32'])
    decode(raw)
    if (record['config_fingerprint']!=m['config_fingerprint'] or record['assessment']!=assessment(v) or
        record['raw_sha256']!=hashlib.sha256(raw).hexdigest() or record['attempt_id']!=v['run'] or
        v['build']!=m['build'] or v['rate']!=m['rate'] or v['gain']!=m['gain'] or
        v['duration']!=UNITS[record['work_unit_id']]):
        raise ValueError('Durable result integrity/identity mismatch')


def committed(root):
    result = {}
    m=load(root/'manifest.json')
    for unit in UNITS:
        p = root/'results'/(unit+'.json')
        if p.exists():
            record = load(p)
            verify_record(record,m)
            if record['work_unit_id']!=unit or not record['assessment']['qualified_for_next_unit']:
                raise ValueError('Invalid committed result')
            result[unit] = record
    return result


def reconcile(root, m):
    done = committed(root)
    atomic(root/'state.json', dict(schema_version=1, run_id=m['run_id'],
           config_fingerprint=m['config_fingerprint'], completed=list(done),
           total_work_units=len(UNITS), status='complete' if len(done)==len(UNITS) else 'in-progress'))
    if len(done)==len(UNITS):
        final = dict(manifest=m, results=done)
        atomic(root/'final.json', final)
        atomic(root/'COMPLETE', dict(sha256=hashlib.sha256(canonical(final)).hexdigest()))
    return done


def commit_snapshot(root, m, active, raw):
    v = decode(raw)
    for key, expected in (('build', m['build']), ('rate', m['rate']), ('gain', m['gain']), ('run', active['token']), ('duration', UNITS[active['unit']])):
        if v[key] != expected:
            raise ValueError('Snapshot identity mismatch: '+key)
    record = dict(work_unit_id=active['unit'], attempt_id=active['token'],
                  config_fingerprint=m['config_fingerprint'], snapshot=v, assessment=assessment(v),
                  raw_sha256=hashlib.sha256(raw).hexdigest())
    # Capture exact VID/PID/path/route identities from the canary. Different devices
    # or hub ports are a new experiment; never silently compare them with old units.
    done = committed(root)
    if done and (v['devices'] != next(iter(done.values()))['snapshot']['devices'] or
                 v['midi_route_hash']!=next(iter(done.values()))['snapshot']['midi_route_hash'] or
                 v['midi_topology_hash']!=next(iter(done.values()))['snapshot']['midi_topology_hash']):
        raise ValueError('Device identities/routes differ from committed canary')
    attempt = root/'attempts'/(str(active['token'])+'.json')
    if attempt.exists() and load(attempt)!=record:
        raise ValueError('Attempt identity collision or changed snapshot')
    if not attempt.exists():
        atomic(attempt, record)
    if record['assessment']['qualified_for_next_unit']:
        result = root/'results'/(active['unit']+'.json')
        if result.exists() and load(result)!=record:
            raise ValueError('Work unit already committed; refusing replacement')
        if not result.exists():
            atomic(result, record)
    reconcile(root, m)
    return record


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--check', action='store_true')
    ap.add_argument('command', nargs='?', choices=('arm', 'fetch', 'status'))
    ap.add_argument('--run-dir', type=Path)
    ap.add_argument('--unit', choices=UNITS, default='canary')
    ap.add_argument('--rate', type=int, choices=(44100, 48000), default=48000)
    ap.add_argument('--gain', type=int, default=25)
    ap.add_argument('--port', default='Felucca')
    ap.add_argument('--retry', action='store_true')
    args = ap.parse_args()
    ident = identity()
    if args.check:
        assert 'mido' not in sys.modules
        print('PASS: package files/identity. No USB/MIDI/audio access or flashing.')
        return
    if not args.command or (args.command!='status' and not args.run_dir) or not 0<=args.gain<=100:
        ap.error('Choose command, --run-dir and gain 0..100')
    root = args.run_dir.resolve() if args.run_dir else None
    if root:
        root.mkdir(parents=True, exist_ok=True)
    with contextlib.ExitStack() as stack:
        if root:
            lock = stack.enter_context((root/'.lock').open('a'))
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            m = manifest(root, ident, args.rate, args.gain)
            done = reconcile(root, m)
            if args.command=='arm' and args.unit in done:
                print('Already committed:', args.unit)
                return
        link = Link(args.port)
        stack.callback(link.close)
        status = struct.unpack('<4I', link.request(4))
        if status[0] != ident['build_tag']:
            raise ValueError('Unexpected firmware build; no arming/acknowledgement sent')
        if args.command=='status':
            print(dict(build=status[0], state=status[1], run=status[2], frozen=status[3]))
            return
        active_path = root/'active.json'
        if args.command=='arm':
            previous = {'stress': 'canary', 'long': 'stress'}.get(args.unit)
            if previous and (previous not in done or not done[previous]['assessment']['qualified_for_next_unit']):
                raise ValueError('Collect a passing prior unit with active source audio first')
            if status[3]:
                raise ValueError('Uncollected FM-1 result: fetch it first')
            if active_path.exists() and not args.retry:
                raise ValueError('Existing attempt: fetch, or --retry after an interrupted physical run')
            active = dict(unit=args.unit, token=secrets.randbelow(0x0fffffff)+1,
                          config_fingerprint=m['config_fingerprint'])
            if active_path.exists():
                old=load(active_path);atomic(root/'abandoned'/(str(old['token'])+'.json'),old)
            atomic(active_path, active)  # identity durable before device receives it
            link.request(1, seven(active['token'],4)+seven(UNITS[args.unit],3)+seven(args.rate,3)+[args.gain,0])
            readback=struct.unpack('<4I',link.request(4))
            if readback[1:3] != (1,active['token']):
                raise ValueError('Arming readback mismatch')
            print('ARMED:', args.unit, UNITS[args.unit]//1000, 's; USB output gain',args.gain)
            print('Keep FM-1 powered. Move USB to powered hub, start both synths, press physical PLAY.')
            print('After result, move USB back to computer and run fetch with this same run directory.')
        else:
            active = load(active_path)
            if active['config_fingerprint']!=m['config_fingerprint']:
                raise ValueError('Active attempt configuration mismatch')
            # Crash after device ACK but before local cleanup: durable data already
            # exists, and the cleared device must never trigger a repeated run.
            attempt_path=root/'attempts'/(str(active['token'])+'.json')
            if not status[3] and status[2]==active['token'] and attempt_path.exists():
                record=load(attempt_path)
                verify_record(record,m)
                reconcile(root,m);active_path.unlink()
                print('Recovered already acknowledged durable attempt:',active['token'])
                return
            raw = link.request(2)
            record = commit_snapshot(root, m, active, raw)
            # Acknowledge only after result + state fsync; repeat fetch is idempotent
            # if interrupted before this command. No flash writes on either command.
            link.request(3, seven(active['token'],4)+seven(record['snapshot']['crc32']&0x0fffffff,4))
            active_path.unlink()
            fd=os.open(root,os.O_RDONLY)
            try:os.fsync(fd)
            finally:os.close(fd)
            print(json.dumps(record['assessment'], indent=2))
            print('Durable result:',root/'attempts'/(str(active['token'])+'.json'))


if __name__=='__main__':
    try:
        main()
    except (Exception, KeyboardInterrupt) as error:
        print('Stopped:',str(error),file=sys.stderr)
        sys.exit(1)
