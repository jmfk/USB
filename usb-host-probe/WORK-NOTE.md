# USB work and verification notes

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
- `sh -n 'Flash USB R3.command'` and launcher `--check`: PASS.
- Specs governance validator: PASS. No unrelated regression suite or external CI.

Handoff: manual-flash/usb-host-probe-r3 and root Flash USB R3.command.
State PREPARED_NOT_INSTALLED. Never executed ordinary installer mode, updater,
flash write or recovery. OrbStack was started for the offline build; no project
infrastructure definitions or shared services were changed.

Hardware acceptance remains pending. MUSB cancellation/toggle semantics are
modeled assumptions, not measured USB0 evidence. The32-port limit is capacity,
not a validated throughput claim. MIDI hardware verification remains false.
Same outstanding Digitakt II format inspection and exact Behringer model apply.

## Independent project extraction, 2026-11-26

Canonical location is now /Users/jmfk/code/USB. Sources, packages and
manual handoffs moved together; independent vendor/compiler/SDK inputs and a new
Python environment remove the M-vave build dependency. See ../MIGRATION.md for
exact verification. R3 remains unchanged, not confirmed installed. A verify-only
R4 staging build passed; no R4 release was sealed and no hardware was accessed.

## R4 manual installation handoff, 2026-11-26

Batch goal: build the extracted standalone project and prepare an owner-operated
installation package. No firmware behavior changed. R1/R2/R3 remain immutable.

- `python3 usb-host-probe/build.py --revision usb-host-probe-r4`: PASS.
  Sanitized audio/USB transport, MIDI core and physical UI models passed;
  six collector checks and two descriptor-parser checks passed. Target build,
  ROM/MMIO and RAM no-call gates passed. Two existing unused audio-variable
  warnings remain in the MIDI-only model build.
- R4 build tag736471641; image39200 B, RAM51712/98304 B,
  pool30720/344064 B. Same staged firmware source fingerprint as R3;
  version label is R4. Hardware and MIDI verification flags remain false.
- `python3 usb-host-probe/prepare_manual.py --revision usb-host-probe-r4`: PASS.
  Package integrity, installer, collector and descriptor-reader checks were
  file-only. The owner Python environment was checked through the R4 launcher.
- Added `Flash USB R4.command`; `sh -n` and `--check`: PASS.
  Build/prepare defaults advance to unused R5; Python syntax checks: PASS.

Handoff: `manual-flash/usb-host-probe-r4/` and root R4 launcher. State remains
PREPARED_NOT_INSTALLED. Codex did not launch an installer, open MIDI/device
access, enter updater mode, write flash or request recovery. Only the owner may
start the launcher manually and type FLASH in Terminal. Firmware replaces the
current app. USB audio device-format compatibility and actual bus behavior remain
unverified; use a powered hub for the host experiment. No unrelated suite or CI.


## USB project name and R5 handoff, 2026-11-26

Owner requested the project name USB everywhere. Canonical folder is now
/Users/jmfk/code/USB and the public repository is https://github.com/jmfk/USB.
The chat is named USB. Codex's project-label UI is blocked to Computer Use and
has no connector rename operation; that sidebar label requires a manual change.
See ADR-0002 for compatibility boundaries. Historical paths and package IDs are
retained, while all current display names, docs and root launchers use USB.

Before moving the folder, the owner-operated R4 installer changed from running
to success (exit code 0), and its process exited. This is installer evidence,
not measured USB audio/MIDI acceptance. Codex did not start that installation.
The MIDI Python environment was recreated at the canonical USB path with the
same pinned dependencies. The previous folder and launcher paths are aliases
for old links; current builds and launchers use the canonical project itself.

Local evidence:
- `python3 usb-host-probe/build.py --revision usb-host-probe-r5`: PASS.
  Focused sanitized USB/audio transport, MIDI core/physical UI, six collector
  checks, two descriptor-parser checks, FM-1 cross-build, RAM no-call and MMIO
  gates passed. Two existing unused audio-variable warnings remain in the
  MIDI-only model. No unrelated suite or external CI was run.
- R5 build1809377051; image39176 B, RAM51712/98304 B,
  pool30720/344064 B. APP_NAME and update-loader SCREEN_NAME are USB.
- `python3 usb-host-probe/prepare_manual.py --revision usb-host-probe-r5`:
  PASS; installer, collector and descriptor-reader checks were file-only.
- R2–R5 root launchers: shell syntax and `--check` passed in the relocated
  environment. Previous launcher names resolve through compatibility links.
- R1–R4 package integrity matches the pre-rename inventory (107 immutable
  files). Runtime install logs/status remain ignored and are not published.
- Specs governance, Python syntax, source/document whitespace checks and
  redacted secret scans passed before publication. Generated immutable .dis
  output is excluded from whitespace checks. Build/prepare defaults advance to
  unused R6.

Handoff: `Flash USB R5.command` and `manual-flash/usb-host-probe-r5/`.
R5 is PREPARED_NOT_INSTALLED; hardware verification flags remain false. The
owner must start installation manually to see the new firmware name on FM-1.


## R6: static startup display refresh, 2026-11-26

Batch goal: remove repeated text blanking observed on the R4 idle page. The app
had reached DEVICE / NOT ARMED; hp_draw cleared all 240x240 pixels on every
scheduled refresh even when no displayed value changed. Native final-frame
screenshots had not modeled the panel's visible clear/redraw interval.

The overlay now retains two small text-box lists, not pixel framebuffers.
Unchanged pages issue zero LCD writes. A changed value or route replaces only
its own background/text box. A layout change clears once and redraws every
position, including all twelve physical button slots. All controls, USB roles,
routing/cleanup and updater paths remain unchanged. Vendor sources and all
previous sealed releases/manual handoffs remain unchanged.

Local evidence:
- `python3 usb-host-probe/build.py --revision usb-host-probe-r6 --verify-only`:
  PASS. UI regression checks repeated idle refreshes with zero LCD writes,
  single-box rate/route/counter changes, stable repeat frames and layout changes
  matching a fresh full redraw. Existing physical-control checks also pass.
- `python3 usb-host-probe/build.py --revision usb-host-probe-r6`: PASS; the
  existing builder repeated its integrated checks while sealing the package.
  Audio/USB/MIDI sanitizer models, six collector checks, two descriptor checks,
  target FM-1 cross-build, RAM no-call and MMIO gates passed. Only the two
  existing MIDI-model unused audio-variable warnings remain.
- Build1448665588; image39580 B, RAM54032/98304 B,
  pool30720/344064 B. No full pixel framebuffer was added.
- `python3 usb-host-probe/prepare_manual.py --revision usb-host-probe-r6`:
  PASS, installer/collector/descriptor-reader file-only checks.
- Root R6 launcher shell syntax and `--check`: PASS. Previous 137 immutable
  package files match their pre-R6 fingerprints. Python syntax, specs governance
  and source whitespace checks pass. Next unused build/prepare default is R7.
- Native idle page snapshots remain stable; existing routed UI snapshots retain
  their layout/content apart from the revision label. This is visual/model
  evidence, not measurement of the physical LCD timing.

Handoff: `Flash USB R6.command` and `manual-flash/usb-host-probe-r6/`.
PREPARED_NOT_INSTALLED. No installer, recovery or flash operation was started by
Codex; no MIDI/device access was made. Panel behavior after installing R6 and
actual audio/MIDI hardware acceptance remain unverified. Owner video/frame
analysis stays in ignored .build-env; no hardware result media is committed.


## R8: KEEL/DUB menu behavior, 2026-11-26

Local adaptation of DUB R64 ui_navigation.h/ADR-0122 and KEEL ADR-0008/0013.
No source/runtime/build dependency on M-vave was introduced. USB, MIDI/MON and
BUS main families own independent selected submenus and submenu-page memories.
Repeated main/sub buttons cycle only that level; switching restores memory.
GLO/SEQ are held SHIFT/ALT, PLAY retains transport. MODE/LINK K1 edits are guarded;
ALL/NONE uses an explicit stopped REC/APPLY action rather than a menu press.
Unused physical slots stay empty. The R6 stable text-box refresh is retained.

R7 was an internal sealed iteration without a manual handoff; retain it locally.
R8 shortens footer labels and corrects READY to show PLAY, then updates the
simulator physical input script. All previous sealed files remain unchanged.

- `python3 usb-host-probe/build.py --revision usb-host-probe-r8`: PASS.
  Independent main/sub memories, navigation inertness, explicit actions, held
  modifier inertness, stopped edits, cleanup/transport, protected result and
  stable rendering tests pass. Existing audio/MIDI sanitizer transport models,
  six collector checks, two descriptor checks and target ROM/MMIO/RAM gates pass.
- Build1540874594: image40952 B, RAM54048/98304 B, pool30720/344064 B.
  Existing two unused-variable MIDI-model warnings remain.
- Native route/monitor/port pages visually inspected. Physical acceptance pending.
- `python3 usb-host-probe/prepare_manual.py --revision usb-host-probe-r8`:
  PASS, file-only installer/collector/descriptor checks. Root R8 shell syntax and
  --check passed. Specs governance/source whitespace checks pass. Default R9.
- Package README retains a legacy B1 AUD sentence after its new control section;
  the new menu instructions and root README define the R8 controls. Use USB/MODE
  K1 right to return to AUDIO+MIDI. This documentation erratum changes no package.

Handoff: Flash USB R8.command / manual-flash/usb-host-probe-r8.
PREPARED_NOT_INSTALLED, all hardware flags false. No updater or device access.


## R12: MIDI performance and log export, 2026-11-26

Batch goal: observe forwarding performance and collect diagnostics over MIDI,
without concurrent USB0 host/device roles or a computer requirement for normal
routing. Only performance.py arm enables a protected frozen diagnostic report.
The client persists a random 112-bit run nonce before sending it; firmware
consumes it at physical PLAY and rejects conflicting pending IDs. Unread reports
block new physical discovery until durable fetch/ACK. Normal unarmed routing
remains restartable standalone. Existing audio snapshot format/CRC/ACK is intact.

New v2 report: 89 little-endian words plus CRC (360 bytes), commands 9/10/11/12
in the existing namespace. Forwarding-stop counters exclude cleanup; final state
and cleanup outcome are included. Last 15 timestamped start/sample/fault/stop/
cleanup records expose explicit truncation. Queue high-water marks are sampled;
service duration excludes observer work and may include preemption. No total CPU
utilization, end-to-end latency, raw MIDI payload logging or delivery guarantee
is inferred. Reports/comparisons retain topology, route and audio-mode identity;
owner must control external input workloads independently. Physical stream power
loss is not resumable. One physical run is one durable directory/work unit.

Evidence:
- `python3 usb-host-probe/build.py --revision usb-host-probe-r12`: PASS.
  Actual C SysEx diagnostic fixture parsed/CRC-checked by Python. Existing
  audio/USB/MIDI sanitizer/control tests, six collector and two descriptor tests,
  target RAM no-call/MMIO gates pass. Eight performance tests pass: throughput,
  CRC, identity, wrong-run/nonce rejection, interrupted arming/readback, lost ACK,
  durable result before completion, actual subprocess SIGTERM and SIGKILL and
  idempotent recovery with no repeated device read/run.
- R12 build1604105024: image42852 B, RAM55076/98304 B,
  pool30720/344064 B. One existing MIDI-only unused audio-variable warning and
  one native-render misleading-indentation warning remain; target build passes.
- `python3 usb-host-probe/prepare_manual.py --revision usb-host-probe-r12`:
  PASS, file-only installer/collector/descriptor/performance checks. Root R12
  shell syntax and --check pass. Specs governance, source whitespace and old
  package hash checks pass. Defaults advance to unused R13.
- R9–R11 are internal sealed iterations retained locally without manual handoff;
  R12 is the owner handoff. No earlier sealed package was changed. Owner media,
  runtime logs/results and build tools remain ignored. No real evaluation,
  updater, recovery, flash write or MIDI/device access was started by Codex.

Runbook: manual-flash/usb-host-probe-r12/README.md (MIDI performance section).
Operator validation: performance.py validate --run-dir RUN_DIR, offline.
After interruption: repeat arm/fetch with the same package and run directory;
completed raw/result/checksum files are the source of truth, never a RAM counter.
R12 is PREPARED_NOT_INSTALLED; all hardware acceptance flags remain false.


## PR #1 review fixes / R13, 2026-11-26

Reviewed base9638640 to headad8a3a0 plus one consolidated fix batch. Standing
policy: fix verified scope-preserving P0–P3 issues; no ignored findings.

Resolved:
- P1 stop/freeze control window: PLAY and K1/REC could restart or edit a completed
  MIDI run while USB host ownership still existed but hp_running was zero.
  Finalize/freeze before controls and gate starts/edits while that transition is
  pending, including IRQ completion after the main-loop check.
- P2 failure boundary: hp_fail now captures diagnostic stop before later USB
  teardown. A regression proves 100 ms forwarding remains 100 ms despite a
  simulated additional 2000 ms before freeze.
- P2 same-mode rotation: an already selected AUDIO mode no longer clears HP_ARMED
  or its run token. Original native reproduction showed state0 instead of1.
- P2 external display cache: returned updater refusal/failure invalidates cached
  app text before/after the session through the existing local platform overlay.
  Regression simulates an external display repaint and verifies full restoration;
  no updater, installer or device access was started to test this path.

Two independent read-only reviews covered P0–P3; the closing review found no
remaining verified findings. Highest found/fixed: P1; none unresolved/ignored.
Hardware behavior remains unverified; passing models do not prove no hardware
faults. No merge approval is implied.

Verification:
- `python3 usb-host-probe/build.py --revision usb-host-probe-r13 --verify-only`:
  PASS after the consolidated fixes.
- `python3 usb-host-probe/build.py --revision usb-host-probe-r13`: PASS, standard
  immutable package sealing. New completion/mode/cache/failure regressions,
  existing C sanitizer USB/audio/MIDI models, 8 performance persistence tests,
  6 collector and2 descriptor tests, target ROM/MMIO/RAM gates all pass.
- `python3 usb-host-probe/prepare_manual.py --revision usb-host-probe-r13`:
  PASS; installer/collector/descriptor/performance file-only --check.
- Root R13 shell syntax/--check, specs governance/source whitespace and redacted
  secret scans pass. 279 previous package files were hash checked unchanged.
- Build1115640002: image42952 B, RAM55076/98304 B, pool30720/344064 B.
  One existing unused audio-global warning remains in the MIDI-only model;
  the new misleading-indentation warning was removed in the touched block.
- Unrelated suites skipped. No CI workflows configured; current local tests use
  the existing pinned offline build image/toolchain. No hardware acceptance run.

Handoff: Flash USB R13.command / manual-flash/usb-host-probe-r13.
PREPARED_NOT_INSTALLED. Earlier releases remain byte-identical. Default R14.

## R14 — graphical connections and MIDI sync (2026-11-26)

Local batch `codex/graphic-ui-midi-sync`: colored cached graphics, explicit IN/OUT
and cables, page identity, compact ST/VER, blank unused SHIFT/ALT, fourth SYNC menu.
Owner approved PLAY Start/Stop and REC configurable CC. CLK30–300 BPM/enable,
CTRL CC/channel/on/off, OUT selected stopped-only;24 PPQN uses existing ACK queues.
STOP sends CC-off after SysEx/cleanup. Output-only discovery works when enabled.
No host/device role bypass, installer or device access.

Verification: `build.py --revision usb-host-probe-r14 --verify-only` and sealed
`build.py --revision usb-host-probe-r14` PASS. C sanitizers cover tick wrap, tempo
range, backpressure, SysEx, no catch-up burst, stopped edits and real USB ACK
Start/50 clocks/CC/Stop/CC-off. Existing6 collector,2 descriptor and8 diagnostic
tests pass. Native screenshots visually checked. Image72392 B; RAM57156/98304 B;
pool30720/344064 B. Existing model-only unused-global warning remains.
`prepare_manual.py --revision usb-host-probe-r14` and file-only checks PASS.
Unrelated broad suites skipped; no CI workflow configured. Owner requested push/PR.

Build118966248, PREPARED_NOT_INSTALLED, hardware flags false. Clock precision,
jitter, actual panel and compatibility require owner hardware acceptance.
Earlier packages untouched; next default R15.

Final pre-PR verification (2026-11-26):
- R14 is sealed; the builder correctly refused to reuse its revision.
- `python3 usb-host-probe/build.py --revision usb-host-probe-r15 --verify-only`:
  PASS on current source, without sealing another package or device access.
- R14 root launcher syntax/file-only --check, specs governance and diff whitespace:
  PASS. Previous219 release files hash checked unchanged.
- One coherent graphic UI/sync PR; physical acceptance remains pending.

## R15 — device profiling and Swift benchmark app (2026-11-26)

Local batch `codex/profiler-benchmark`, based on PR2 graphic/sync work.
- Profiling schema1: core0 timer/audio/display time with IRQ overlap subtraction,
  inclusive host/mix/MIDI subdivisions,64-bit tick totals, timer gaps, observed
  USB audio frame margin and buffer/error/clock counters. Core1 is not started.
- Command13 frozen profile shares diagnostic build/sequence/nonce; Python persists
  raw profile+diagnostics before ACK. Independent audio snapshot ACK remains durable.
- Native SwiftUI/CoreMIDI USB Bench: idle/audio,100/1000 short SysEx/s,20x256-byte
  SysEx/s and clock monitoring; manual topology/PLAY, explicit endpoints, per-attempt
  results, recovery without traffic replay and independent offline validator.
- Timing reports are instrumented workload, not total CPU. External interface and
  Mac scheduling affect observed round trips. Audio sources/recording are external.

Evidence: `build.py --revision usb-host-probe-r15 --verify-only` and sealed build
PASS; C sanitizers/actual transport+wire profile fixtures,19 Python tests.
`python3 benchmark-app/test_bridge.py`:3 PASS, including lost ARM/audio ACK recovery.
`swift test --package-path benchmark-app`:7 PASS, including real SIGTERM/SIGKILL
and independent Python validation of Swift artifacts. Native configuration view
checked light/dark, keyboard command palette and system theme restored. The
NSHostingView bitmap capture was blank; actual app screenshots via Computer Use
were used instead. Report view awaits real hardware data.
`prepare_manual.py --revision usb-host-probe-r15`: file-only checks PASS.
App release build/ad-hoc signature prepared locally; ignored dist output.
Build718334333: image73488 B, RAM57572/98304 B, pool30720/344064 B. Existing
model-only unused-global warning remains; new warning fixed. Unrelated suites
skipped. No GitHub Actions workflow, device access, benchmark traffic or flashing.

Handoff: Flash USB R15.command and benchmark-app/dist/USB Bench.app. Local app
uses project Python environment; not notarized/standalone-distributed. Manual
installation only. Hardware/profile overhead/clock acceptance pending. Default R16.

Final handoff checks: R15 root launcher syntax/--check, specs governance,
source whitespace, local mido import without ports and app codesign verification
PASS.237 earlier release files hash checked unchanged. App clock capture counts
coalesced/unresolved timestamp pulses separately instead of treating them as
individually timed intervals. No push/PR was requested for this new batch.

## R16 — CPU/revision on every menu page (2026-11-26)

Shared y44 status strip: C0 measured-work mean since run start, -- without
samples, C1:OFF explicitly unused, generated HP_LABEL revision. Snapshot64-bit
counters with IRQs masked, percentage arithmetic outside mask with bounded
32-bit rounded division. No new wire ABI or core1 startup. Swift shared header
shows bundled revision/build and labels C0 as frozen, C1 unused.

`build.py --revision usb-host-probe-r16 --verify-only` and sealed build PASS.
New focused coverage checks every menu/main/sub/page for header presence, glyph
fit, content boundary and unchanged redraw. Long-run percentage rounding found
and fixed before sealing. Native route screenshot checked, no overlap. Target
image73852 B, RAM57572/98304 B;19 Python tests and existing C models pass.
Swift release build passes; unchanged Swift core recovery evidence reused.
R16 manual package file-only checks pass. App rebuilt with R16 resource handoff.
Build1675001374, PREPARED_NOT_INSTALLED. No device/MIDI access or flashing.
Previous packages immutable; next default R17.

## Guided Mac evaluation and logs (2026-11-26)

Same local profiler batch, app-only follow-up. Simplified native flow with one
stage-specific primary action and evaluation/results/log destinations. Explicit
waits for USB-device connection, powered-hub/physical PLAY, and STOP/return cable.
Traffic abort is incomplete, never a pause/replay of accepted physical samples.
Old app results can be inspected read-only; execution retains exact-code identity.

Added fsynced events.jsonl, bounded500-event UI, search/level/follow-live controls,
streamed bridge command messages and separately labeled frozen FM-1 events.
ACK progress is emitted only after raw data is durable. Torn final log line
repair, duplicate/interior corruption rejection and write-failure poisoning
prevent misleading recovery. workflow.json is a projection of durable artifacts.

Verification: Swift focused suite12 PASS, Python bridge4 PASS, release app build
and ad-hoc signing PASS. Real SIGTERM/SIGKILL fixtures preserve host counters and
logs. Native light/dark configuration/wait/log views, command navigation, saved
result reopening and log search checked. Result screenshots used clearly labeled
UI-FIXTURE synthetic data, not hardware measurements. One unarmed GUI test attempt
was labeled UI-verifiering; no device commands, MIDI traffic or flashing.
Firmware suites reused for unchanged sealed R16; no new firmware package.
Behavior sketch: benchmark-app/design/evaluation-flow.html; docs updated.
Local only, no new push/PR.

## Requested publication (2026-11-26)

Owner requested push and a PR for the complete profiler/benchmark batch.
Base is codex/graphic-ui-midi-sync (open PR2, head a03f880), so this PR excludes
its already reviewed graphic/sync diff. Reuse passing R15/R16 build/model/Python
evidence and final Swift12/bridge4 tests plus release signing. Specs governance,
source whitespace and R15/R16 immutable release/handoff integrity pass.
No Actions workflows configured; no unrelated suite rerun for publication.
Local app/build outputs and the unrelated editor swap file are excluded.
Manual firmware installation only; no device acceptance or merge implied.
