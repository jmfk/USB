---
id: ADR-0108
title: USB MIDI port matrix and serialized USB0 bus ownership
status: accepted
date: 2026-11-24
related_prds: [PRD-0094]
supersedes: []
superseded_by: []
---

# ADR-0108: USB MIDI port matrix and serialized USB0 bus ownership

## Decision
Use a bounded MIDI1 router with 32 logical input and 32 logical output ports and per-input 32-bit destination masks. Decode embedded-jack cable numbers; preserve MIDI payload and rewrite only the destination cable nibble. Each logical input has its own bounded queue so one blocked SysEx cable cannot block another cable's SysEx completion. Fan-out admission is atomic across selected outputs. Keep separate output endpoint queues and DATA toggles, acknowledging queue removal only on observed USB ACK/toggle advancement.

Route editing is allowed only with forwarding stopped, after discovery. Empty is the default; ALL means every discovered output, including self-routing when explicitly selected. On stop, stop admitting new inputs, finish/cancel the current bounded transaction, discard unsent musical data and send SysEx end/sustain-off/all-notes-off/all-sound-off on the old outputs before giving up host ownership. Failed/disconnected destinations are reported; cleanup is never asserted successful without ACKs.

Audio packets keep their three-per-frame priority. MIDI uses remaining frame time with a bounded bulk window; cancel requests and wait for quiescence before changing FADDR. The MUSB register semantics and single-packet toggle accounting are model-tested assumptions awaiting FM-1 hardware acceptance. If safe quiescence cannot be observed, fail closed instead of switching the address. Standalone MIDI uses the same transport without audio deadlines.

Routing is session-only. Preserve it across an identical rediscovery by comparing ordered VID/PID/hub-path/interface/endpoint/cable keys; clear it on topology changes. Do not write existing instrument flash. Preserve the original audio snapshot prefix and extend new snapshots with MIDI counters. This is a new release, not a mutation of sealed R2.

## Evidence
- R3 local evidence: [work note](../../../usb-host-probe/WORK-NOTE.md). Core, modeled USB transport, physical controls and cross-build passed. Hardware acceptance is pending; no device was flashed.
- [USB MIDI 1.0 event and cable specification](https://www.usb.org/sites/default/files/midi10.pdf)
- [Linux MUSB host cancellation, FIFO and toggle handling](https://code.googlesource.com/linux/torvalds/linux/+/d97ae6474ca0411bb8c2696e5764ec946dba43d0/drivers/usb/musb/musb_host.c)
- [PRD-0094](../../prds/0094-usb-host-midi-router.md)
