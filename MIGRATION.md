# Project extraction — 2026-11-26

Destination: `/Users/jmfk/code/USB`. Personal instrument experiment,
separate from the ongoing M-vave/DUB/KEEL work and from business portfolios.

Moved: USB overlay, focused tests, collector/descriptor reader, R1/R2/R3 releases,
R2/R3 manual handoffs and both owner launchers. Old locations are compatibility
symlinks to the new project so existing links continue working. The new project
contains no dependency or symlink back to M-vave.

Copied: independent Dinghy build base with GPL notices/provenance, Dockerfile,
local compiler and SDK, and the two PRDs/two ADRs. M-vave spec copies retain
historical requirements plus a canonical-location note. Installer template is
owned here under tools; it no longer reads menu-r2 from another project.

Recreated: .build-env/midi-venv with pinned mido1.3.3 and python-rtmidi1.5.8.
Ignored SDK/compiler/venv remain on disk but are not committed. Git is local only.
Build stage paths resolve under this project. The --verify-only mode runs the
existing checks/cross-build without creating a new release; future releases start
at R4. Existing R3 build736471641 remains unchanged and not confirmed installed.

Codex sidebar project creation is unavailable through the current connector, and
Computer Use rejects automation of Codex itself. The user must add this folder
through Add project and move the existing chat there in the sidebar. No new chat
was created, and project sidebar membership is not claimed complete.

## Verification

- `python3 usb-host-probe/build.py --verify-only`: PASS from the new root.
  ASan/UBSan transport, MIDI core and physical UI checks; six collector and two
  descriptor tests; target FM-1 build, RAM no-call and MMIO gates. Screen renders
  produced. Image39200 B, RAM51712/98304 B, pool30720/344064 B.
- Verification stage is `.build-env/usb-host-probe-r4-work`, not a sealed R4 release.
  Functional source fingerprint/build tag remains736471641. R1/R2/R3 releases
  and R2/R3 handoffs are byte-identical to the pre-move inventory:73 files checked.
- R2 and R3 owner launchers: shell syntax and file-only `--check` PASS using the
  new local Python environment. No MIDI/audio device was opened.
- Both projects' specs governance validation: PASS.
- No new-project symlink resolves into M-vave. Compatibility links point from
  old locations into the new project. SDK/toolchain copies are local, independent
  files; the virtual environment was recreated rather than moved.
- Local Git initialized. No remote/push, new release, updater or flash operation.
- Codex sidebar project/chat membership remains a manual user action as explained
  above; the on-disk extraction and independent build are complete.


## USB name — 2026-11-26

The project's current name is USB. Canonical folder: `/Users/jmfk/code/USB`.
Public repository: [jmfk/USB](https://github.com/jmfk/USB). The former
`/Users/jmfk/code/fm1-usb-router` path is a compatibility symlink, preserving the
historical M-vave links and existing manual-install links. Current build inputs
and the recreated MIDI environment are independent of that alias.

Documentation, current root launchers, firmware APP_NAME and the next loader
screen use USB. R1–R4 package bytes and legacy technical IDs remain unchanged;
the new display name is delivered in R5. See [ADR-0002](specs/adrs/active/0002-usb-project-name.md)
and the [work note](usb-host-probe/WORK-NOTE.md) for verification and installation
status. The Codex chat is renamed; the sidebar project label needs a manual
change because the connector has no rename operation and Codex UI control is
blocked.
