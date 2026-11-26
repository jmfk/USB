# FM-1 USB audio mixer and MIDI router

Independent FM-1 firmware experiment, extracted from M-vave on2026-11-26.
Two stereo USB audio sources plus an output device, with per-input USB MIDI1
routing to any set of output ports. Firmware and hardware support remain experimental.

Latest prepared package: **R3, build736471641**, not confirmed installed or
hardware-verified. Digitakt II format inspection and the exact Behringer mixer
model are still pending. MIDI software capacity is32 inputs and32 outputs;
this is not a measured throughput claim.

- [R3 controls, formats and test procedure](manual-flash/usb-host-probe-r3/README.md)
- [R3 owner-operated manual installer](<Flash USB Host Probe R3.command>)
- [Work and verification notes](usb-host-probe/WORK-NOTE.md)
- [Requirements and decisions](specs/index.md)
- [Extraction details](MIGRATION.md)

## Layout

`usb-host-probe/firmware` contains the experiment overlay. `vendor/dinghy` is a
local GPL-3.0 base snapshot, with its license/provenance in vendor/README.md.
`usb-host-probe/tests` contains focused transport/router/UI and collector checks.
`usb-host-probe/releases` and `manual-flash` preserve immutable R1/R2/R3 files;
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
choose an unused revision (the default is nowR4), then run prepare_manual.py for
that same revision. Never overwrite or edit an existing released package.
No firmware is installed by build.py or prepare_manual.py. Only the owner may
launch the manual installer; Codex may run its file-only --check.

No GitHub repository, remote deployment, device access or flash operation was
created as part of the extraction.
