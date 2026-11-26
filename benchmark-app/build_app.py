#!/usr/bin/env python3
"""Build local ad-hoc signed Swift app. No MIDI/device/installer access."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
ROOT=Path(__file__).resolve().parent

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--firmware',type=Path,default=ROOT.parent/'manual-flash/usb-host-probe-r16')
    ap.add_argument('--output',type=Path,default=ROOT/'dist/USB Bench.app');args=ap.parse_args()
    firmware=args.firmware.resolve();identity=json.loads((firmware/'identity.json').read_text())
    if identity.get('profile_schema')!=1:raise SystemExit('Choose profiling firmware schema1')
    hashes=json.loads((firmware/'manifest.json').read_text())['sha256']
    for name,digest in hashes.items():
        if Path(name).name!=name or hashlib.sha256((firmware/name).read_bytes()).hexdigest()!=digest:raise SystemExit('Firmware handoff integrity failure')
    subprocess.run(['swift','build','--package-path',str(ROOT),'-c','release'],check=True)
    bin_path=subprocess.check_output(['swift','build','--package-path',str(ROOT),'-c','release','--show-bin-path'],text=True).strip()
    app=args.output.resolve()
    if app.exists():shutil.rmtree(app)
    resources=app/'Contents/Resources';resources.mkdir(parents=True);(app/'Contents/MacOS').mkdir()
    shutil.copy2(Path(bin_path)/'USBBench',app/'Contents/MacOS/USBBench')
    shutil.copytree(firmware,resources/'firmware')
    shutil.copy2(ROOT/'benchmark_bridge.py',resources/'benchmark_bridge.py')
    python=ROOT.parent/'.build-env/midi-venv/bin/python'
    (resources/'python-path.txt').write_text(str(python)+'\n')
    h=hashlib.sha256()
    for file in sorted((ROOT/'Sources').rglob('*.swift'))+[ROOT/'Package.swift',ROOT/'benchmark_bridge.py',Path(__file__)]:
        h.update(str(file.relative_to(ROOT)).encode()+b'\0'+file.read_bytes())
    revision=h.hexdigest();(resources/'app-revision.txt').write_text(revision)
    with (app/'Contents/Info.plist').open('wb') as f:plistlib.dump(dict(CFBundleExecutable='USBBench',CFBundleIdentifier='local.fm1.usb-bench',CFBundleName='USB Bench',CFBundlePackageType='APPL',CFBundleShortVersionString='0.1',CFBundleVersion='1',LSMinimumSystemVersion='14.0',NSHighResolutionCapable=True),f)
    subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
    print(app)
if __name__=='__main__':main()
