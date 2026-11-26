---
id: PRD-0007
title: Guided evaluation and visible logs
status: completed
locked: true
created: 2026-11-26
related_adrs: [ADR-0008]
---
# Guided evaluation and logs

## Requirements
- Simple primary flow: run evaluation, see live progress, get results.
- Dedicated searchable log view shows host events, acquisition progress, errors and frozen device diagnostics when available.
- Pause the workflow with precise instructions for physical connection, PLAY and STOP; no remote flashing/host transition.
- Preserve raw results, immutable attempts and interruption/recovery guarantees; no automatic traffic replay.
- Keep CPU/revision identity visible in shared app header, retain native light/dark themes and keyboard commands.

## Completion evidence
- Swift app now has evaluation/results/logs destinations and a stage-specific
  primary action with physical connection/PLAY/STOP pauses.
- Live host counters and streamed acquisition events; searchable/filterable
  persistent log with frozen device diagnostics, dropped-count warnings and dedup.
- Recovery/transition/journal tests, real process termination tests and bridge
  durability/progress checks pass. Native setup/wait/log views checked light/dark;
  results/read-only reopening/search checked with explicitly synthetic UI data.
- App rebuilt locally with existing immutable R16 firmware. No firmware rebuild,
  device access, test MIDI traffic or flashing in this UI batch.
