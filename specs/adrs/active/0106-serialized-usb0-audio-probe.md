---
id: ADR-0106
title: Serialized USB0 audio probe before mixer implementation
status: accepted
date: 2026-11-24
related_prds: [PRD-0092]
supersedes: []
superseded_by: []
---

# ADR-0106: Serialized USB0 audio probe before mixer implementation

## Decision
Stage a dedicated Dinghy-based experiment. USB0 has one FADDR for the bus, so permit exactly one pending transaction and change address only after completion. Poll the experiment from the existing 10 kHz timer; retain device-mode updater behavior outside the experiment. Use two IN pipes and one OUT pipe at three distinct addresses. Support UAC1 PCM16 stereo at a common selected 44.1/48 kHz rate; the initial sink must use synchronous or adaptive synchronization. Explicitly reject asynchronous sinks rather than guessing feedback or silently sending at the wrong rate.

The experiment mixes at conservative gain for optional listening and measures ring drift without sample-rate conversion. It cannot establish long-term clock compatibility. Packet completion alone does not prove correct source identity, no cross-routing or analog output; report those limitations separately.

An owner-armed run is one uninterrupted work unit. Keep its latest frozen snapshot in RAM until it is collected via device-mode MIDI. Never write existing flash data. On the computer, checkpoint collected results atomically under immutable build/config/run identities and refuse to overwrite them. A power loss before collection loses that attempt; retry it with a new attempt identifier. Completed collected units survive interruption. Preserve the FM-1 battery power while moving the cable from hub to computer.

## Consequences
- No speculative production mixer or compatibility promise.
- Powered hub required; at most two hub levels and seven ports per hub are explored.
- Clock drift, missed frames, unsupported devices and transfer failures are results, never hidden by a success label.
- Firmware installation remains manual, including any later revision.

## Links
- [PRD-0092](../../prds/0092-usb-host-three-device-probe.md)
