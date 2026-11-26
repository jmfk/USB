---
id: PRD-0005
title: Device profiling and native benchmark application
status: completed
locked: true
created: 2026-11-26
related_adrs: [ADR-0006]
---
# Device profiling and benchmark

## Requirements
- Prepare profiling firmware measuring instrumented core0 work, audio frame/buffer margins and MIDI performance on hardware.
- Native Swift macOS app selects scenarios, generates/receives MIDI test traffic, arms/fetches frozen device metrics and saves inspectable results.
- Distinguish instrumented work from total CPU utilization; no fabricated hardware acceptance or device clock precision.
- Host/device exclusion, physical transport/route controls and manual-only installation remain.
- Interrupted physical runs are marked incomplete; finished results persist, retries use new attempts, no silent replay of MIDI side effects.
- Offline fixtures and build verification without device access.

## Completion evidence
- R15 build718334333: profiling schema1, CRC/nonce tied frozen data, observed audio margins and bounded core0 timing. Prepared, not installed.
- SwiftUI/CoreMIDI local app built with scenarios, physical-run workflow, durable host checkpoints, profile/audio collector and independent validator.
- C sanitizer arithmetic/transport/wire/UI models;19 Python collector/parser/profile tests;3 bridge recovery tests;7 Swift tests including real SIGTERM/SIGKILL and cross-language validation pass.
- Native empty/configuration view checked in light/dark mode and command palette. No device/traffic/flash acceptance performed. Report view awaits actual hardware data.
