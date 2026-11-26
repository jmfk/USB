---
id: PRD-0092
title: Three-device USB audio host feasibility probe
status: in-progress
locked: true
created: 2026-11-24
related_adrs: [ADR-0106]
---

# PRD-0092: Three-device USB audio host feasibility probe

## Requirements
- Prepare an isolated FM-1 firmware experiment for two distinct stereo USB audio sources and a third USB audio sink on a powered hub.
- Measure serialized USB0 address changes, per-stream packets/frames/errors, missed service deadlines and input buffer drift.
- Begin with explicitly supported UAC1 stereo PCM16 formats at 44.1/48 kHz. Reject unsupported formats and clock arrangements visibly.
- Boot in device mode. Only a physical action starts host mode. Stop and return to device mode for result collection.
- Keep completed experiment results durably on the computer with immutable run/config/build identities and idempotent collection. Interrupted physical runs must be retried, never represented as resumed uninterrupted measurements.
- Preserve boot/recovery/update compatibility and existing sealed releases. Do not write music/settings or diagnostic flash during the experiment.
- Build and test offline; prepare a versioned manual-flash package. Only the owner flashes it. Local models are not evidence of USB PHY/audio success.

## Non-goals
- A finished mixer, general UAC2 support, asynchronous output without implemented feedback, independent clock resampling, or an automatic updater.

## Implementation notes
- 2026-11-24: work started in `usb-host-probe/`, separate from Dinghy, Melodee, KEEL and DUB handoffs.
- R2 build1722852506 prepared at `manual-flash/usb-host-probe-r2/`. Focused sanitizer/USB-model/collector/descriptor checks, cross-build and file-only handoff checks passed; native screen inspected. No flashing or measured hardware result.
- Owner's candidate setup is a second FM-1 plus Digitakt II as sources and a Behringer mixer as output. Exact Behringer model and Digitakt II descriptor inspection remain pending before recommending this package for that setup. See `usb-host-probe/WORK-NOTE.md`.

- 2026-11-26: canonical project moved to /Users/jmfk/code/fm1-usb-router. See [ADR-0001](../adrs/active/0001-independent-project.md) and [migration evidence](../../MIGRATION.md). Released bytes and hardware status are unchanged.
