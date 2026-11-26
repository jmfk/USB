---
id: PRD-0004
title: Graphical routing and MIDI sync controls
status: completed
locked: true
created: 2026-11-26
related_adrs: [ADR-0005]
---
# PRD-0004: Graphical routing and MIDI sync controls

## Requirements
- Semantic colors, graphical IN/OUT connections and clear active menu/page identity.
- Short footer labels that fit physical cells; unused SHIFT/ALT remain empty/inactive.
- Explicit sync page: internal MIDI clock, configurable tempo/output, PLAY Start/Stop.
- REC toggles a selectable CC/channel/on/off value, as confirmed by owner.
- Preserve standalone powered-hub use, stopped route/config edits, USB role exclusion,
  ACK/toggle/cleanup invariants, diagnostic identity and manual-only firmware install.
- Native UI and model regression evidence; physical acceptance remains unverified.

## Completion evidence
R14 implements the graphical route view, four menu families, blank unused
modifiers and explicit clock/CC controls. Native screenshots checked; sanitizer
UI/core and actual USB transport models pass, including output-only sync and
Start/clock/CC/Stop/CC-off delivery. Target image72392 B, RAM57156 B.
Package build118966248 is prepared, not installed; physical acceptance pending.
