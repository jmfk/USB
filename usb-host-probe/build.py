#!/usr/bin/env python3
"""Offline staging, focused tests and firmware build. No USB/MIDI or updater access."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
BASE = ROOT/'vendor/dinghy'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--revision', default='usb-host-probe-r15')
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument('--stage-only', action='store_true')
    mode.add_argument('--verify-only', action='store_true', help='Run local checks/build without creating a release')
    args = ap.parse_args()
    if not re.fullmatch(r'usb-host-probe-r[1-9][0-9]*', args.revision):
        ap.error('Invalid revision')
    release = HERE/'releases'/args.revision
    if release.exists():
        raise SystemExit('Sealed release exists: choose a new revision.')
    stage = ROOT/'.build-env'/(args.revision+'-work')
    stage.mkdir(parents=True, exist_ok=True)
    for folder in ('firmware', 'tools', 'tests'):
        target = stage/folder
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(BASE/folder, target,
                        ignore=shutil.ignore_patterns('__pycache__', '.DS_Store'))
    shutil.copy2(BASE/'build.sh', stage/'build.sh')
    shutil.copytree(HERE/'firmware', stage/'firmware', dirs_exist_ok=True)
    shutil.copytree(HERE/'tests', stage/'tests', dirs_exist_ok=True)
    platform = stage/'firmware/src/platform.c'
    s = platform.read_text()
    anchor = '    if (usb_due)\n        owed |= 1u;'
    assert s.count(anchor) == 1
    s = s.replace(anchor, '    if (hp_owns_usb) hp_tick();\n'+anchor)
    s = s.replace('if (usb_due)\n        usb_poll();', 'if (usb_due && !hp_owns_usb)\n        usb_poll();')
    s = s.replace('if (owed & 1u)\n        usb_poll();', 'if ((owed & 1u) && !hp_owns_usb)\n        usb_poll();')
    lo = s.index('    usb_retry(fm1_ms);', s.index('static void platform_poll'))
    hi = s.index('    if (fm1_ms >= 30000u', lo)
    s = s[:lo]+'    if (!hp_owns_usb) {\n'+s[lo:hi]+'    }\n'+s[hi:]
    anchor='            ota_session();'
    assert s.count(anchor)==1
    s=s.replace(anchor,'            {hp_ui_invalidate();ota_session();hp_ui_invalidate();}')
    platform.write_text(s)
    ota = stage/'firmware/src/ota.c'
    s = ota.read_text();anchor = '    d = ota_unpack7(p, n, ota_dec, sizeof ota_dec);'
    assert s.count(anchor) == 1
    ota.write_text(s.replace(anchor, '#ifdef APP_SYSEX\n    if (APP_SYSEX(p,n)) {ota_frame_done();return 0;}\n#endif\n'+anchor))
    tool = stage/'tools/build.py'
    s = tool.read_text().replace('SCREEN_NAME = "DINGHY"', 'SCREEN_NAME = "USB"')
    tool.write_text(s)
    h = hashlib.sha256()
    for folder in ('firmware', 'tools'):
        for p in sorted((stage/folder).rglob('*')):
            if p.is_file() and p.name!='run_identity.h':
                h.update(str(p.relative_to(stage)).encode()+b'\0'+p.read_bytes())
    source = h.hexdigest();tag = int(source[:8], 16)&0x7fffffff
    label=args.revision.rsplit('-',1)[1].upper()
    (stage/'firmware/src/run_identity.h').write_text(f'#pragma once\n#define HP_BUILD_TAG {tag}u\n#define HP_LABEL "{label}"\n')
    identity = dict(revision=args.revision, build_tag=tag, source_sha256=source,
                    status='PREPARED_NOT_INSTALLED', hardware_verified=False,
                    device_access=False, midi=dict(protocol='USB MIDI 1.0 Full Speed bulk', inputs=32, outputs=32, persistence='session', hardware_verified=False), formats=['UAC1 PCM16 stereo 44100/48000'],
                    output_sync=['adaptive', 'synchronous'], run_ms=[10000, 60000, 300000])
    (stage/'identity.json').write_text(json.dumps(identity, indent=2)+'\n')
    if args.stage_only:
        print(stage)
        return
    shell = '''set -eu
mkdir -p build
cc -O1 -g -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation -fsanitize=address,undefined -fno-omit-frame-pointer -Itests -Ifirmware/src tests/probe_test.c -o build/probe-test
ASAN_OPTIONS=detect_leaks=0 build/probe-test build/probe-wire-fixture.bin
cc -O1 -g -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation -fsanitize=address,undefined -Ifirmware/src tests/midi_core_test.c -o build/midi-core-test
ASAN_OPTIONS=detect_leaks=0 build/midi-core-test
cc -O1 -g -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation -fsanitize=address,undefined -Ifirmware/src tests/midi_sync_test.c -o build/midi-sync-test
ASAN_OPTIONS=detect_leaks=0 build/midi-sync-test
cc -O1 -g -Wall -Wno-unused-function -Wno-misleading-indentation -fsanitize=address,undefined -DDINGHY_HOST -Ifirmware/hal -Ifirmware/src -Ifirmware/gen tests/midi_ui_test.c -o build/midi-ui-test
ASAN_OPTIONS=detect_leaks=0 build/midi-ui-test
sh build.sh --board fm1
sh tests/run_host.sh tests/probe-screen.txt
'''
    cmd = ['docker', '--context', 'orbstack', 'run', '--rm', '--platform', 'linux/amd64', '--network', 'none',
           '--mount', f'type=bind,source={stage},target=/work',
           '--mount', f'type=bind,source={ROOT/".build-env/jieli"},target=/opt/jieli,readonly',
           '--mount', f'type=bind,source={ROOT/".build-env/ac79-sdk"},target=/opt/ac79-sdk,readonly',
           '-e', 'JIELI_TOOLCHAIN=/opt/jieli/toolchain', '-e', 'AC79_SDK=/opt/ac79-sdk',
           '-w', '/work', 'm-vave-dinghy-build:local', 'sh', '-c', shell]
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (stage/'build-log.txt').write_text(result.stdout)
    print(result.stdout)
    if result.returncode:
        raise SystemExit(result.returncode)
    memory = re.findall(r'image (\d+) B of (\d+); RAM .data\+\.bss (\d+) B of (\d+); pool (\d+) B of (\d+)', result.stdout)
    assert len(memory)==1
    identity['memory'] = dict(zip(('image_bytes', 'image_capacity', 'ram_bytes', 'ram_capacity', 'pool_bytes', 'pool_capacity'), map(int, memory[0])))
    sys.path.insert(0, str(stage/'tools'))
    from fm1_install import load_package
    assert load_package(str(stage/'build/dinghy.fwsc'), False)[0]=='FM-1_990'
    assert 'mido' not in sys.modules
    subprocess.run([sys.executable, str(HERE/'tests/test_collect.py'), str(stage/'build/probe-wire-fixture.bin')], check=True)
    subprocess.run([sys.executable,str(HERE/'tests/test_inspect_usb.py')],check=True)
    subprocess.run([sys.executable,str(HERE/'tests/test_performance.py'),str(stage/'build/probe-wire-fixture.bin.performance')],check=True)
    (stage/'identity.json').write_text(json.dumps(identity, indent=2)+'\n')
    if args.verify_only:
        print('VERIFIED_NOT_PACKAGED:', stage)
        return
    release.mkdir(parents=True)
    for suffix in ('fwsc', 'bin', 'elf', 'dis'):
        shutil.copy2(stage/f'build/dinghy.{suffix}', release/f'FM1-{args.revision}.{suffix}')
    for name in ('README.md', 'collect.py', 'inspect_usb.py', 'performance.py'):
        if name=='README.md':
            (release/name).write_text((HERE/name).read_text().replace('usb-host-probe-r1',args.revision).replace('USB R1','USB '+label))
        else:
            shutil.copy2(HERE/name, release/name)
    shutil.copy2(BASE/'LICENSE', release/'LICENSE')
    shutil.copy2(stage/'build-log.txt', release/'build-log.txt')
    (release/'identity.json').write_text(json.dumps(identity, indent=2)+'\n')
    for p in (stage/'build/host').glob('*.png'):
        shutil.copy2(p, release/p.name)
    with zipfile.ZipFile(release/'staged-source.zip', 'w', zipfile.ZIP_DEFLATED) as z:
        for folder in ('firmware', 'tools', 'tests'):
            for p in sorted((stage/folder).rglob('*')):
                if p.is_file() and '__pycache__' not in p.parts:
                    z.write(p, str(p.relative_to(stage)))
        z.write(stage/'build.sh', 'build.sh')
        z.write(HERE/'build.py', 'probe-build.py')
    (release/'sha256.json').write_text(json.dumps({p.name: hashlib.sha256(p.read_bytes()).hexdigest()
         for p in sorted(release.iterdir()) if p.is_file()}, indent=2)+'\n')
    print('PREPARED_NOT_INSTALLED:', release)


if __name__=='__main__':
    main()
