---
id: ADR-0003
title: Independent USB menu page memories
status: accepted
date: 2026-11-26
related_prds: [PRD-0002]
supersedes: []
superseded_by: []
---
# ADR-0003: Independent USB menu page memories

## Decision
Adapt the navigation behavior from DUB R64 ui_navigation.h and KEEL ADR-0008/0013
with the newer DUB ADR-0122 independent submenu-page contract. Keep a small local
helper with no build-time reads or runtime dependency on M-vave.

Top row: USB, MIDI/MON, BUS, empty, empty, SHIFT. Bottom row: three contextual
submenus, ALT, PLAY/STOP, contextual APPLY. USB contains MODE, FORMAT, RESULT;
RESULT has counters/errors pages. MIDI main pages ROUTING and MONITOR remember
separate submenu selections. ROUTING contains LINK, PORTS (IN/OUT pages), ACTION;
MONITOR contains TRAFFIC and FAULTS. BUS contains STATE and BUILD.

Navigation only updates UI memory. USB/MODE K1 changes AUDIO/MIDI mode while
stopped in device mode and without protected evidence. LINK K1 toggles the
selected stopped route. ACTION K1 chooses ALL/NONE; REC explicitly applies it.
SELECT/ALGORITHM retain input/output port selection. GLO/SEQ are held modifiers,
never navigation or BACK. Binary controls do not acquire artificial 10-step
behavior. No new analogue parameter is introduced.

Reuse the R6 text-box refresh cache. Existing transport, USB ownership and
cleanup paths remain. R8 is a new immutable manual handoff; hardware acceptance
remains pending. No other project or vendor snapshot is changed.
