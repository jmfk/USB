---
id: PRD-0002
title: USB physical main and submenu pages
status: completed
locked: true
created: 2026-11-26
related_adrs: [ADR-0003]
---
# PRD-0002: USB physical main and submenu pages

## Requirements
- Reuse KEEL/DUB main/submenu and independent remembered-page behavior locally.
- Repeated selected buttons cycle only their own pages; switching restores memory.
- Show main/sub selection and separate M/S page indicators and all twelve slots.
- GLO is held SHIFT, SEQ held ALT, PLAY transport; unused buttons stay inactive.
- Navigation never changes operating mode, routes, arming or transport.
- Retain stopped-only route edits, cleanup, protected audio evidence and manual flashing.
- New firmware version; preserve all previous packages and standalone build inputs.

## Implementation notes
- Local adaptation of DUB R64 ui_navigation.h and KEEL menu contracts; see ADR-0003.

## Completion evidence
R8 focused navigation/refresh/control tests, target build, sanitizer transport models and file-only handoff checks passed. Native route, monitor and port-page screenshots inspected. Hardware acceptance remains pending.
