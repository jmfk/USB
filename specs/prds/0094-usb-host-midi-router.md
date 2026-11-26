---
id: PRD-0094
title: USB host MIDI port router with per-input fan-out
status: in-progress
locked: true
created: 2026-11-24
related_adrs: [ADR-0108]
---

# PRD-0094: USB host MIDI port router with per-input fan-out

## Requirements
- Discover connected class-compliant USB-MIDI 1.0 devices and their virtual cable ports, including MIDI-only and audio+MIDI devices.
- Give each input port an independent set of zero, one, several or all output ports. Preserve channel data and support notes, controllers, program changes, system messages, clock and SysEx.
- Provide physical controls for selecting inputs/outputs, toggling routes, and applying ALL/NONE to one input. Start with no routes and never silently remap a route to a different device after discovery changes.
- Provide standalone MIDI routing and an audio+MIDI experiment on the existing powered-hub USB0 path. Serialize all transactions around the controller's shared address.
- Backpressure full queues; retain output packets through NAK/retry; do not acknowledge software delivery before the USB device ACKs. Keep per-endpoint toggles. Serialize merged SysEx per destination while allowing realtime messages.
- Prevent live route changes from orphaning held notes; clean up sustain/notes/sound and interrupted SysEx on stop, and report unsuccessful cleanup.
- Bound supported devices/ports/queues explicitly and reject excess capacity. Do not claim unrestricted USB or measured hardware support.
- Prepare a new versioned manual-only firmware package. Preserve R1/R2 and their launchers. Codex never starts flash/update mode.

## Implementation notes
- Hardware acceptance remains pending. Initial implementation is Full Speed, MIDI1 bulk endpoints; USB MIDI2 and hotplug are outside this revision.
- R3 implementation and manual package prepared: [work note](../../usb-host-probe/WORK-NOTE.md), [manual instructions](../../manual-flash/usb-host-probe-r3/README.md). Nothing installed.
- Focused ASan/UBSan core/transport/physical-UI tests and FM-1 cross-build passed; source snapshots, memory figures and build log are sealed with build tag736471641. Real FM-1 MIDI+audio acceptance remains outstanding.

- 2026-11-26: canonical project moved to /Users/jmfk/code/USB. See [ADR-0001](../adrs/active/0001-independent-project.md) and [migration evidence](../../MIGRATION.md). Released bytes and hardware status are unchanged.

- 2026-11-26: the project is named USB; canonical folder `/Users/jmfk/code/USB`, repository [jmfk/USB](https://github.com/jmfk/USB). See [ADR-0002](../adrs/active/0002-usb-project-name.md). R5 contains the new display name; previous package bytes remain unchanged.

- 2026-11-26: R6 prepares a display-refresh fix for idle-page flicker: unchanged frames do no LCD I/O; changed text boxes update individually. Focused UI regressions and target build pass. See [work note](../../usb-host-probe/WORK-NOTE.md). R6 is prepared, not installed; panel and USB hardware acceptance remain pending.
