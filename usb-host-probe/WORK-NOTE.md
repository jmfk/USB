# USB host feasibility batch

Goal: test three distinct USB0 audio addresses before investing in a mixer.
Scope: isolated probe firmware, bounded UAC1 discovery, serialized scheduler,
offline register model, manual-only package, read-only/arming collector.
Base: local Dinghy v0.5 platform; host attach/enumeration sequences derived from
the local Melodee GPL-3.0 host implementation and AC79 SDK register evidence.
No sealed package or upstream checkout is edited. No device access or flashing
is part of local build/verification. Root folder is not a Git repository.

State: R2 prepared, not installed. Internal R1 remains unchanged and was not
handed off. R2 build tag 1722852506, image31960 B, RAM23556/98304 B,
pool30720/344064 B. No updater was launched.

Local verification 2026-11-24:
- `python3 usb-host-probe/build.py --revision usb-host-probe-r2`: PASS.
  Actual host C in a modeled hub + two IN devices + a separate OUT device;
  ASan/UBSan, 44.1/48k cadence, source/mix signatures, serialized addresses,
  length/transfer/IO/no-SOF/deadline failures, actual C-to-Python snapshot fixture.
- Five collector durability/identity/integrity/CRC/restart checks: PASS.
- Two read-only descriptor parser checks (UAC1/other-speed and UAC2/24-bit): PASS.
- FM-1 cross-build, ROM/MMIO and RAM no-call gates: PASS.
- Native DEVICE/NOT ARMED screenshot at800ms visually inspected: clear/unclipped.
- `python3 usb-host-probe/prepare_manual.py --revision usb-host-probe-r2`: PASS;
  installer/collector/descriptor-reader `--check` performed file-only checks.
- Specs governance validator: PASS.

Handoff: `manual-flash/usb-host-probe-r2/`. R2 is frozen; next code/package
change needs a new revision. No full unrelated synth/sequencer suite was run.

Actual equipment supplied by owner while preparing: a second FM-1 and Digitakt
II as candidate sources; a Behringer mixer can provide USB output, exact model
pending. Before recommending installation for this combination, inspect the
Digitakt II's current and other-speed descriptors in USB AUDIO/MIDI mode and
confirm the Behringer model/formats. Hi-Speed USB and class compliance alone do
not prove a UAC1/PCM16 Full Speed fallback. A read-only Mac inventory and the
descriptor reader currently found no attached Elektron device. No audio/USB
streaming or hardware acceptance measurement has been performed.

Pending user input: exact Behringer model and Digitakt II connected to Mac for
read-only format inspection. Preserve FM-1 manual-flash-only rule.

## R3: MIDI port router, 2026-11-24

Batch goal: each discovered USB MIDI1 input can feed zero, one, multiple or all
output ports. Standalone MIDI and combined audio+MIDI experiment, physical
SELECT/ALGORITHM/K1, per-input ALL/NONE, stopped-only editing. Virtual cables
are separate ports, limited to32 each direction. Empty routes by default;
RAM-only preservation requires identical ordered topology. No hotplug/MIDI2.

Files: new firmware/src/midi_router.h and midi_usb.h; extended probe_core.h,
probe_usb.h, probe_wire.h, dinghy.c; collector64-field snapshot and route/topology
identity checks; build/prepare scripts, README, core/transport/physical UI tests;
PRD-0094 and ADR-0108. Existing Dinghy/Melodee and sealed R1/R2 are unchanged.

Local evidence:
- `python3 usb-host-probe/build.py --revision usb-host-probe-r3`: PASS.
  Actual driver fake-register model with ASan/UBSan: composite discovery,
  multiple virtual cables, simultaneous three-device audio+MIDI, idle IN polling,
  NAK/backpressure, late RX/ACK, independent DATA toggles, no mid-transfer FADDR
  changes, no duplicate note delivery, successful stop and blocked-output failure.
- MIDI core model: PASS. Atomic fan-out, per-cable queues, SysEx ownership with
  realtime, topology preservation/reset, malformed/capacity rejection, F7 and
  all48 cleanup CCs per used output; unused outputs receive no cleanup.
- Physical UI model and native screenshots: PASS. Discovery/READY, ALL/NONE,
  K1 on/off, running route-edit guard, stop, immutable armed-result protection.
  READY/OFF, READY/ON, RUNNING and STOPPED screens visually inspected.
- Six collector checks including actual332-byte C snapshot, CRC/durability and
  changed MIDI routing/topology rejection: PASS. Two descriptor-reader checks: PASS.
- Target FM-1 build, ROM/MMIO gates and RAM no-call gate: PASS.
  Build tag736471641; image39200 B, .data+.bss51712/98304 B,
  pool30720/344064 B. The MIDI-only core test has two unused audio-global warnings;
  the firmware target build succeeds.
- `python3 usb-host-probe/prepare_manual.py --revision usb-host-probe-r3`: PASS.
  Installer/collector/descriptor-reader --check only; no MIDI/device access.
- `sh -n 'Flash USB Host Probe R3.command'` and launcher `--check`: PASS.
- Specs governance validator: PASS. No unrelated regression suite or external CI.

Handoff: manual-flash/usb-host-probe-r3 and root Flash USB Host Probe R3.command.
State PREPARED_NOT_INSTALLED. Never executed ordinary installer mode, updater,
flash write or recovery. OrbStack was started for the offline build; no project
infrastructure definitions or shared services were changed.

Hardware acceptance remains pending. MUSB cancellation/toggle semantics are
modeled assumptions, not measured USB0 evidence. The32-port limit is capacity,
not a validated throughput claim. MIDI hardware verification remains false.
Same outstanding Digitakt II format inspection and exact Behringer model apply.

## Independent project extraction, 2026-11-26

Canonical location is now /Users/jmfk/code/fm1-usb-router. Sources, packages and
manual handoffs moved together; independent vendor/compiler/SDK inputs and a new
Python environment remove the M-vave build dependency. See ../MIGRATION.md for
exact verification. R3 remains unchanged, not confirmed installed. A verify-only
R4 staging build passed; no R4 release was sealed and no hardware was accessed.
