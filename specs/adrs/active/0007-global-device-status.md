---
id: ADR-0007
title: Shared CPU status strip with explicit inactive core
status: accepted
date: 2026-11-26
related_prds: [PRD-0006]
supersedes: []
superseded_by: []
---
# Shared CPU and revision strip

Place a shared single-line status at y44, below page/state indicators and above
content starting at y60. Format C0:nn% C1:OFF R16 (revision is generated HP_LABEL).
Core0 is the cumulative mean of instrumented exclusive timer/audio/display work
since run start, frozen after stop; -- before any sample. It is not total CPU
utilization. Snapshot64-bit counters atomically, compute outside the IRQ mask,
and reduce both operands before32-bit division to avoid hot-path/runtime64-bit
division. Clamp the display to100; raw profiles remain unchanged. Core1 is not
started, so show OFF instead of claiming a measured zero. Empty optional menu
positions remain blank. Swift header labels its CPU value as a frozen profile.
