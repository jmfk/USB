---
id: ADR-0006
title: Bounded core0 profiling and separate Mac MIDI benchmark harness
status: accepted
date: 2026-11-26
related_prds: [PRD-0005]
supersedes: []
superseded_by: []
---
# Profiling and benchmark

## Decision
Keep one USB owner. Add a bounded profile record (SysEx read13), tied to the
existing diagnostic build/sequence/112-bit nonce and cleared with its ACK.
The collector durably commits both raw CRC-checked records before ACK. Older
packages keep their protocol. Firmware declares the optional profile in identity.
Measure timer work, audio ISR excluding nested timer, display excluding IRQ,
USB host service, mix and MIDI software work with24 MHz timer ticks. Inclusive
subsections must not be added to parent totals. Main idle polling,
entry/exit wrappers and uninstrumented interrupts are excluded: show measured
scheduled-work utilization, never claim OS-style total CPU utilization. Core1
is not started. Frame completion margin is relative to software observation of
USB frame start; buffers convert frames to microseconds at configured rate.

Mac app uses SwiftUI/CoreMIDI and a packaged Python collector for existing wire
identity/fsync/ACK invariants. Explicit endpoint selection; no test traffic before
Start. USB0 role switching/transport is physical. External MIDI interface bridges
Mac traffic to the powered-hub host topology; audio sources are external/manual.
Scenario attempts are unique, durably recorded and independently retryable.
Completed attempts are immutable; interrupted attempts cannot claim completion
or automatically replay side effects. Final host results and device profile remain
separate until the exact armed diagnostic run is fetched. Timing is observed
round-trip through selected endpoints, not isolated FM-1 latency.
