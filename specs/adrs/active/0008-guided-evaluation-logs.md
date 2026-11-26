---
id: ADR-0008
title: Explicit evaluation stages and durable event log
status: accepted
date: 2026-11-26
related_prds: [PRD-0007]
supersedes: []
superseded_by: []
---
# Guided evaluation and logs

Use one stage-dependent primary action, with evaluation/results/logs destinations.
Setup creates an immutable attempt. Await device connection → ARM → await host
wiring/physical PLAY → capture → await physical STOP/device connection → fetch →
results. These hardware waits are explicit workflow pauses. Abort during traffic
marks the attempt interrupted and cannot resume/replay it. Workflow state is a
projection; started/host-complete/armed/device artifacts are authoritative on
recovery. A new app revision may read prior artifacts for inspection, but command
execution requires the exact attempt code/firmware identity.

Append structured host/bridge/device events to a fsynced JSONL journal, repair
only a torn last line on recovery, fail on interior corruption or duplicate IDs.
UI tails a bounded window while full logs remain on disk. Stream bridge progress
as explicit stderr JSON lines; retain stdout final JSON and raw device records.
Device diagnostics are frozen and become visible only after fetch, never presented
as live host-mode telemetry. No changes to immutable R15/R16 firmware packages.
