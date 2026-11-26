# USB

This is an independent personal instrument project. Sources and build inputs must
not depend on ../m-vave. The legacy usb-host-probe-rN identifiers and released
package contents are intentionally preserved.

The project display name is USB. Its canonical folder is /Users/jmfk/code/USB
and its public repository is https://github.com/jmfk/USB. Keep legacy technical
paths and package identifiers independent of the display name.

## Manual firmware installation only
- Codex may edit, test, build and prepare versioned firmware packages.
- Only the user starts flashing manually outside Codex.
- Never start installers, updater mode, recovery or flash writes from Codex.
- File-only --check without MIDI/device access is allowed.
- Keep handed-off packages immutable; use a new version directory for a new build.
- Record package version, limitations and local evidence. Prepared is not installed.
- Check process/status after interruption; never start a competing updater.

## Scope and controls
- USB0 host and device roles are exclusive. Retain device-mode updater access.
- Powered hub required. Do not claim VBUS power or measured USB compatibility.
- The screen is read-only. All instrument actions use physical buttons/encoders.
- Show all12 button positions and leave unused positions blank/inactive.
- MIDI route editing is stopped-only; preserve ACK/toggle and cleanup invariants.
- Models are not hardware acceptance. Keep unverified hardware flags false.

## Work locally
- Use batch-pr-discipline for coding and prd-adr-governance for specifications.
- Prefer focused verification; do not run unrelated suites.
- `python3 usb-host-probe/build.py --verify-only` checks and builds without sealing.
- Build inputs are vendor/dinghy, Dockerfile.build and ignored local .build-env.
- Never modify the vendor snapshot casually; overlay firmware is usb-host-probe/firmware.
- Do not commit SDK/toolchain binaries, virtual environments or hardware results.
