---
id: ADR-0002
title: USB project name and compatibility
status: accepted
date: 2026-11-26
related_prds: [PRD-0001, PRD-0092, PRD-0094]
supersedes: []
superseded_by: []
---

# ADR-0002: USB project name and compatibility

## Decision
The project display name is **USB**, as requested by the owner. The canonical
folder is `/Users/jmfk/code/USB` and the public GitHub repository is
[jmfk/USB](https://github.com/jmfk/USB). Documentation, owner launchers, firmware
app name and update-loader display use USB. The Codex chat is named USB; the
sidebar project label requires a manual change because no project-renaming tool
is available and Computer Use blocks Codex itself.

Keep `usb-host-probe/`, `usb-host-probe-rN`, `FM-1_990`, the Felucca MIDI port,
protocol/store constants and the existing build-image tag as legacy technical
identifiers. They are compatibility contracts, not the product display name.
Do not rename upstream Dinghy/Melodee attribution or modify the vendor snapshot.

R1–R4 releases and manual handoff contents remain byte-for-byte unchanged. Apply
the firmware display-name change in a new R5 package. The previous project path
and root launcher names remain compatibility symlinks so existing manual-install
links and historical source references continue to work. New references use USB.
The project has no dependency on those aliases for its own build or installer.

## Consequences
- Local verification runs from the canonical USB folder; recreate the MIDI
  virtual environment there instead of relying on its previous absolute path.
- The owner-operated R4 installer reported success before the directory move.
  R5 preparation does not install R5 or prove USB audio/MIDI hardware acceptance.
- Manual flashing, portfolio isolation and independent local build inputs remain
  unchanged. The name change introduces no services, credentials or data moves.

## Links
- [Independent project requirements](../../prds/0001-independent-project.md)
- [Build and handoff evidence](../../../usb-host-probe/WORK-NOTE.md)
- [Migration record](../../../MIGRATION.md)
