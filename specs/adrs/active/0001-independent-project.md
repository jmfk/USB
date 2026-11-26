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
USB, the personal FM-1 audio/MIDI experiment, owns its source, tests, specifications,
releases and handoffs under /Users/jmfk/code/USB. It is independent of
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
historical references and make the new project canonical. The extraction was
initially local-only. The later authorized public repository and project rename
are recorded in [ADR-0002](0002-usb-project-name.md).

Verify the relocated build with --verify-only, without sealing a new firmware
release just for the original extraction. Later owner-operated package versions
and installation status are recorded in the work note.
Hardware acceptance and manual-flash-only restrictions are unchanged.

## Links
- [Extraction requirements](../../prds/0001-independent-project.md)
- [Migration evidence](../../../MIGRATION.md)
- [USB project name and compatibility](0002-usb-project-name.md)
