#!/usr/bin/env python3
"""Create immutable owner-operated handoff. Runs file-only --check, never installer mode."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

ROOT=Path(__file__).resolve().parent.parent


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--revision',default='usb-host-probe-r17')
    args=ap.parse_args();revision=args.revision
    if not re.fullmatch(r'usb-host-probe-r[1-9][0-9]*',revision):ap.error('Invalid revision')
    release=ROOT/'usb-host-probe/releases'/revision
    target=ROOT/'manual-flash'/revision
    if target.exists():raise SystemExit('Handoff exists; never overwrite it')
    for name,digest in json.loads((release/'sha256.json').read_text()).items():
        if Path(name).name!=name or hashlib.sha256((release/name).read_bytes()).hexdigest()!=digest:
            raise ValueError('Release integrity failure: '+name)
    target.mkdir()
    package=f'FM1-{revision}.fwsc'
    for name in (package,'identity.json','collect.py','performance.py','inspect_usb.py','README.md','LICENSE','build-log.txt','staged-source.zip'):
        shutil.copy2(release/name,target/name)
    with zipfile.ZipFile(release/'staged-source.zip') as z:
        (target/'fm1_install.py').write_bytes(z.read('tools/fm1_install.py'))
    manual=(ROOT/'tools/manual_install_template.py').read_text()
    old="        print('Denna version installerades senast som menu-r2; den nya layouten ingår INTE.')"
    assert manual.count(old)==1
    manual=manual.replace(old,
        "        print('Separat USB-hostprov: ingen färdig mixer och ingen bekräftad hårdvarukompatibilitet.')\n"
        "        print('Testappen startar som USB-device. Fysisk PLAY upptäcker USB; andra PLAY startar routing/prov.')\n"
        "        print('Läs README.md om UAC1/PCM16, powered hub och hur resultat hämtas utan avstängning.')")
    (target/'manual_install.py').write_text(manual)
    launcher=target/'Installera USB.command'
    launcher.write_text('''#!/bin/sh
set -eu
PROBE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROBE_PY="$PROBE_DIR/../../.build-env/midi-venv/bin/python"
if [ ! -x "$PROBE_PY" ]; then echo 'Projektets Pythonmiljö saknas. Ingen åtgärd startades.'; exit 1; fi
exec "$PROBE_PY" "$PROBE_DIR/manual_install.py" "$@"
''')
    launcher.chmod(0o755)
    manifest=dict(version=revision,package=package,
        description='USB: USB0 MIDI1 per-input fan-out router and two-source UAC1 stereo audio experiment',
        state='PREPARED_NOT_INSTALLED',hardware_verified=False,
        sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(target.iterdir()) if p.is_file()})
    (target/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    subprocess.run([sys.executable,str(target/'manual_install.py'),'--check'],check=True)
    subprocess.run([sys.executable,str(target/'collect.py'),'--check'],check=True)
    subprocess.run([sys.executable,str(target/'inspect_usb.py'),'--check'],check=True)
    subprocess.run([sys.executable,str(target/'performance.py'),'--check'],check=True)
    print('PREPARED_NOT_INSTALLED:',launcher)


if __name__=='__main__':main()
