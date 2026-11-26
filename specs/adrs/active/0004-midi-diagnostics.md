---
id: ADR-0004
title: Bounded post-run diagnostics over MIDI
status: accepted
date: 2026-11-26
related_prds: [PRD-0003]
supersedes: []
superseded_by: []
---
# ADR-0004: Bounded post-run diagnostics over MIDI

## Decision
Use new commands in the existing 7D 55 48 01 SysEx namespace: 9 reads a frozen
performance report, 10 acknowledges sequence/CRC, 11 reads its identity/status, 12 sets a durable client-generated 112-bit run ID.
The nonce is consumed at physical run start and included in the v2 report/status; client validates it before ACK. Only armed runs freeze protected reports. Normal routing without a diagnostic nonce can stop/restart without requiring a computer.
Keep existing audio snapshot commands and their wire shape unchanged.

Record event/fan-out throughput and packet counts at the forwarding stop boundary;
cleanup errors are finalized after cleanup. Service maxima and queue high-water
marks are sampled in the existing host tick. Timing is elapsed duration servicing that tick (including preemption), not end-to-end MIDI latency or total CPU utilization. Counters are
32-bit; runs must stop before their counters or elapsed milliseconds wrap.

Keep the last 15 timestamped diagnostic events (start, periodic counters, fault,
stop/cleanup); expose dropped count. This is a diagnostic log, not a note/SysEx payload
capture or a claim of lossless event logging. Freeze only after stopping and
returning USB0 to device mode. Protect unread diagnostics from the next physical
PLAY; acknowledge only after durable local export. Results remain in RAM until
power-off, so a physical stream cannot resume after power loss.

The Python collector reuses existing atomic fsync storage. Each owner-selected
run directory represents one immutable physical run. Persist manifest, raw data,
computed report and completion checksum before ACK; repeated fetch validates
local evidence and retries only the matching ACK. Offline validate/report/compare
never open MIDI. Config/build identity is preserved and comparison reports build
changes while requiring matching topology, routing and audio mode.
