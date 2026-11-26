# USB

USB is an independent FM-1 firmware experiment, extracted from M-vave on2026-11-26.
Two stereo USB audio sources plus an output device, with per-input USB MIDI1
routing to any set of output ports. Firmware and hardware support remain experimental.

Latest prepared package: **R13, build1115640002**, not confirmed installed or
hardware-verified. The owner-operated R4 installer reported success before the
rename; R13 remains prepared only. Digitakt II format inspection and the exact Behringer mixer
model are still pending. MIDI software capacity is32 inputs and32 outputs;
this is not a measured throughput claim.

- [R13 controls, formats and test procedure](manual-flash/usb-host-probe-r13/README.md)
- [R13 owner-operated manual installer](<Flash USB R13.command>)
- [Work and verification notes](usb-host-probe/WORK-NOTE.md)
- [Requirements and decisions](specs/index.md)
- [Extraction details](MIGRATION.md)

Project folder: `/Users/jmfk/code/USB`. Public repository:
[jmfk/USB](https://github.com/jmfk/USB). Historical `usb-host-probe` technical
paths and package IDs remain stable; existing R1–R4 packages are immutable.

R6 avoids repeated screen blanking on an unchanged page and updates changed
text boxes individually. The fix is locally tested; panel confirmation after
manual installation remains pending.

R8 uses KEEL/DUB-style main menus, contextual submenus and independent remembered
pages. Top row: USB, MIDI/MON, BUS, empty, empty, SHIFT. Bottom row: up to three
submenus, ALT, PLAY/STOP, contextual APPLY. Repeat a selected menu to cycle its
own pages; M/S show the two page indices. USB/MODE K1 selects audio/MIDI mode.
MIDI/LINK K1 edits a stopped route; MIDI/ACT chooses ALL/NONE with K1 and applies
with REC. Menu navigation itself changes no route, mode or transport.

## MIDI performance and logs

R12 adds MIDI/MON → PERF and frozen SysEx diagnostic reports. Use the packaged
`performance.py arm --run-dir ...` before a physical run, STOP on FM-1, then
reconnect USB0 to the Mac and use `performance.py fetch --run-dir ...` with the
same directory. `report`, `validate` and `compare` work offline. See the
[R13 runbook](manual-flash/usb-host-probe-r13/README.md#midi-prestanda-och-diagnostikloggar-r12-och-senare).

Reports contain routing events/fan-out per second, ACKed packet counters before
cleanup, retries, backpressure, errors, sampled queue peaks, service durations
and the last 15 timestamped diagnostics with explicit dropped count. They do
not measure end-to-end latency, total CPU utilization or capture MIDI payloads.
A durable 112-bit client run ID prevents confusing separate boots/runs; raw
CRC-checked reports and completion checksums are fsynced before device ACK.
Interrupted downloads resume without restarting a physical run. Ordinary routing
without diagnostic arming remains independent of a computer. No real benchmark
or device access has been performed by Codex.

Use R13 for the reviewed fixes: stopped-run evidence is frozen before controls
can restart discovery or edit routes; failed-run timing stops at the failure
boundary; selecting an unchanged mode preserves an armed audio attempt; returning
from a refused/failed updater invalidates the display cache. Earlier packages
remain immutable historical artifacts.

## Layout

`usb-host-probe/firmware` contains the experiment overlay. `vendor/dinghy` is a
local GPL-3.0 base snapshot, with its license/provenance in vendor/README.md.
`usb-host-probe/tests` contains focused transport/router/UI and collector checks.
`usb-host-probe/releases` and `manual-flash` preserve immutable R1/R2/R3/R4/R5/R6 files;
R1 was internal and has no manual handoff. Existing package IDs do not change.

## Local build

The local JieLi toolchain and AC79 SDK were copied into `.build-env/jieli` and
`.build-env/ac79-sdk`. The Python environment was recreated at its new path.
These local inputs are ignored by Git; they are independent of the M-vave folder.
The offline builder uses OrbStack and the existing `m-vave-dinghy-build:local`
image. That historical image tag is retained; it has no workspace dependency.
To recreate it from this project:

```sh
docker --context orbstack build --platform linux/amd64 -f Dockerfile.build -t m-vave-dinghy-build:local .
```

To recreate the owner MIDI Python environment if needed:

```sh
python3 -m venv .build-env/midi-venv
.build-env/midi-venv/bin/python -m pip install -r requirements-macos.txt
```

Run from this project root:

```sh
python3 usb-host-probe/build.py --verify-only
```

This runs focused local checks and the firmware cross-build without making a new
release or accessing devices. To prepare a new version, omit --verify-only and
choose an unused revision (the default is nowR14), then run prepare_manual.py for
that same revision. Never overwrite or edit an existing released package.
No firmware is installed by build.py or prepare_manual.py. Only the owner may
launch the manual installer; Codex may run its file-only --check.

No GitHub repository, remote deployment, device access or flash operation was
created as part of the extraction.
