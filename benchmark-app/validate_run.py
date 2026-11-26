#!/usr/bin/env python3
"""Offline validation; never opens MIDI. Requires a finished host capture."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

def load(path):return json.loads(path.read_text())
def sha(data):return hashlib.sha256(data).hexdigest()
def validate_logs(path):
    if not path.exists():return 0
    raw=path.read_bytes()
    if raw and not raw.endswith(b'\n'):raise ValueError('Incomplete log tail; open the run in the app to recover')
    ids=set()
    for line in raw.splitlines():
        event=json.loads(line)
        if not isinstance(event.get('id'),str) or not event['id'] or event['id'] in ids:raise ValueError('Duplicate/invalid log ID')
        if any(not isinstance(event.get(k),str) for k in ('source','level','message')) or not isinstance(event.get('date'),(float,int)):raise ValueError('Invalid log event')
        ids.add(event['id'])
    return len(ids)

def validate(root,require_device=False):
    manifest=load(root/'manifest.json');host=load(root/'host.json')
    # Swift's exact serialized config is represented by sorted-key UTF-8 JSON.
    encoded=json.dumps(manifest['config'],sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()
    if manifest['schemaVersion']!=1 or sha(encoded)!=manifest['fingerprint']:raise ValueError('Configuration fingerprint mismatch')
    if host['status']!='complete' or (root/'HOST_COMPLETE').read_text()!=sha((root/'host.json').read_bytes()):raise ValueError('Host capture incomplete or corrupt')
    profile_path=root/'device/result.json'
    if profile_path.exists():
        sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'usb-host-probe'))
        import performance as p
        report=p.validate(root/'device');record=load(profile_path)
        profile=p.decode_profile(bytes.fromhex(record['profile_raw_hex']),report)
        if str(profile['build'])!=manifest['config']['firmware']:raise ValueError('Host/device build mismatch')
        device=True
    else:device=False
    if require_device and not device:raise ValueError('Device profile missing')
    audio=bool(manifest['config']['audio'])
    if audio and require_device:
        import collect as c
        a=load(root/'audio-result.json');v=c.decode(bytes.fromhex(a['raw_hex']));arm=load(root/'audio-arm.json')
        if a['snapshot']!=v or v['run']!=arm['token'] or str(v['build'])!=manifest['config']['firmware'] or v['rate']!=manifest['config']['sampleRate'] or v['duration']!=manifest['config']['duration']*1000:raise ValueError('Audio identity/integrity mismatch')
    return dict(host_complete=True,device_profile=device,audio_requested=audio,hardware_qualified=False,log_events=validate_logs(root/'events.jsonl'))
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('root',type=Path);ap.add_argument('--require-device',action='store_true');args=ap.parse_args()
    try:print(json.dumps(validate(args.root.resolve(),args.require_device),indent=2))
    except Exception as e:print('Invalid:',e,file=sys.stderr);sys.exit(1)
