---
id: ADR-0005
title: Colored cached UI and explicit host MIDI sync
status: accepted
date: 2026-11-26
related_prds: [PRD-0004]
supersedes: []
superseded_by: []
---
# ADR-0005: Colored cached UI and explicit host MIDI sync

## Decision
Extend the small cached draw-command renderer with semantic colors and a bounded
connection diagram. Keep page identity separate from M/S indicators; active footer
cells use color instead of a width-consuming prefix. BUS footer labels are ST/VER.
All twelve positions remain represented; GLO/SEQ have no active function and are blank.
The device has a fixed dark instrument palette, without a desktop theme/settings layer.

SYNC is a fourth physical main menu (LFO/B4), containing CLK, CTRL and OUT.
CLK page1 uses K1 tempo30–300 BPM, K2 enable; page2 shows clock/skip/late/CC counters. Enabling in idle device mode selects MIDI-only unless armed/protected audio evidence prevents it. CTRL has two pages: K1 CC0–119/K2 channel1–16;
then K1 on-value/K2 off-value0–127. Defaults CC20/channel1/on127/off0.
ALGORITHM selects a host MIDI output while stopped. First PLAY discovers, second
PLAY starts routing plus enabled 24-PPQN clock/Start. Global STOP sends Stop before
existing cleanup, followed by configured CC-off if REC was on. REC on SYNC toggles the CC only while forwarding. Enable/config
changes are stopped-only. Menu changes never start, stop or configure transport.

Use the existing host output queues and ACK handling; never transmit directly to
USB registers from UI code. MIDI-only discovery accepts output-only devices when
sync is enabled. Generated non-realtime CC is refused during owned SysEx; realtime
clock may interleave. Skip/count late or queue-blocked clocks rather than catch-up
bursts. Outgoing Stop precedes stop-cleanup and its failure is not reported as success.
Internal clock is off by default. Owner must avoid routing an external clock to the
same destination. No TRS/device-to-Mac generator is introduced.

Include enabled sync configuration in route fingerprints so incompatible benchmark
configurations cannot compare equal. Existing diagnostics wire shape remains.
Hardware clock accuracy/jitter and device compatibility remain unverified.
