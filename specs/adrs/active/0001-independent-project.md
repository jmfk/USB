---
id: ADR-0001
title: Independent project ownership and local build inputs
status: accepted
date: 2026-11-26
related_prds: [PRD-0001, PRD-0092, PRD-0094]
supersedes: []
superseded_by: []
---

# ADR-0001: Independent project ownership and local build inputs

## Decision
The personal FM-1 USB audio/MIDI experiment owns its source, tests, specifications,
releases and handoffs under /Users/jmfk/code/fm1-usb-router. It is independent of
M-vave, DUB, KEEL and their firmware evolution, and belongs to neither the Bad
Herring nor Staravenir product runtime/control planes. No infrastructure, account,
secret, domain or customer data is introduced or transferred.

Vendor the small GPL Dinghy source base with provenance. Copy the existing compiler
and SDK into the ignored local .build-env and recreate the MIDI virtual environment
at the new path. Keep the existing offline Docker image tag as a legacy identifier;
it contains generic build tools and has no M-vave source mount/dependency. Include
Dockerfile.build for recreation. Future build and manual-template paths resolve
within this project, without symlinks into M-vave.

Move immutable packages unchanged. Leave compatibility symlinks at their old
locations so existing chat links and historical M-vave references still resolve;
those links point into this project, never the reverse. Keep old spec copies as
historical references and make the new project canonical. Local Git only; no
remote repository or publication is part of this request.

Verify the relocated build with --verify-only, without sealing a new firmware
release just for the folder move. R3 remains the owner-operated test package.
Hardware acceptance and manual-flash-only restrictions are unchanged.

## Links
- [Extraction requirements](../../prds/0001-independent-project.md)
- [Migration evidence](../../../MIGRATION.md)
