---
id: PRD-0003
title: MIDI performance reports and diagnostics export
status: completed
locked: true
created: 2026-11-26
related_adrs: [ADR-0004]
---
# PRD-0003: MIDI performance reports and diagnostics export

## Requirements
- Measure forwarding throughput, peak queue occupancy and maximum USB service time.
- Retain a bounded timestamped diagnostic event log with explicit dropped count.
- Export immutable CRC-protected results over device-mode MIDI SysEx after stop.
- Save locally before acknowledgement; resume interrupted downloads idempotently.
- Provide offline validation, reports and comparisons with configuration identity.
- No automatic host start, traffic generation, flashing or hardware acceptance claim.
- Preserve USB role exclusion, routing/cleanup invariants and previous packages.

## Completion evidence
R12 native/model build and C diagnostic wire fixture passed; eight Python performance tests cover rates, CRC, identity, actual SIGTERM/SIGKILL, interrupted arming/download, lost ACK and reboot nonce collision. Packaged --check is file-only. Physical performance/hardware acceptance remains pending.
