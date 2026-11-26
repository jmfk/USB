---
id: PRD-0001
title: USB — independent FM-1 audio and MIDI project
status: completed
locked: true
created: 2026-11-26
related_adrs: [ADR-0001, ADR-0002]
---

# PRD-0001: USB — independent FM-1 audio and MIDI project

## Requirements
- Move the USB audio/MIDI experiment to its own project folder, with its sources, tests, specs, build tools, released packages and manual handoffs.
- Eliminate build/runtime dependencies on M-vave; preserve GPL attribution and existing firmware/package identities.
- Preserve sealed releases byte-for-byte and keep firmware installation exclusively manual.
- Verify the relocated build and file-only package checks. Keep hardware acceptance pending.

## Implementation notes
- Destination: /Users/jmfk/code/USB. Codex sidebar naming is a manual UI step because project renaming is unavailable through the connector and Codex UI automation is blocked.
- The project is named USB; see [ADR-0002](../adrs/active/0002-usb-project-name.md). Historical package identifiers and sealed bytes are unchanged.

## Completion evidence
- Independent cross-build and focused checks passed, and immutable-file integrity/file-only installer checks passed. See [MIGRATION.md](../../MIGRATION.md).
- Filesystem extraction is complete; Codex sidebar registration remains an explicitly documented manual step. No hardware/device output or flashing.
