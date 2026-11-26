---
id: PRD-0006
title: CPU and firmware identity on every screen
status: completed
locked: true
created: 2026-11-26
related_adrs: [ADR-0007]
---
# Global device status

## Requirements
- Every instrument menu/page shows both CPU cores and the running firmware revision.
- Core0 uses the existing measured-work profiler; core1 explicitly unused, not a fabricated utilization value.
- Header fits without covering content or active page indicators; preserve physical controls and cached redraw.
- Swift benchmark header identifies its bundled revision and both cores, distinguishing frozen profile from live instrument measurement.
- Prepare a new immutable firmware package; manual installation only.

## Completion evidence
Shared instrument header and Swift benchmark header implemented in R16. All
menu/main/sub/page combinations checked for presence, fit and stable redraw;
64-bit long-run arithmetic checked. Native firmware route screenshot inspected.
Target build, focused C models and19 Python tests pass; Swift release build passes.
Prepared, not installed; physical acceptance pending.
