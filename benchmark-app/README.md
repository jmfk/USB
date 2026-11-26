# USB Bench

Native macOS14+ SwiftUI/CoreMIDI benchmark harness for profiling firmware R16.
The current build targets this Mac's architecture. No dependency on ../m-vave.

## Build and launch

```sh
swift test --package-path benchmark-app
python3 benchmark-app/build_app.py
open 'benchmark-app/dist/USB Bench.app'
```

The local ad-hoc signed app bundles the immutable R16 firmware handoff and
collector, and uses this project's `.build-env/midi-venv/bin/python`. This is a
local development app, not a notarized standalone distribution. Rebuild after
moving the project; if necessary recreate the ignored Python environment with
`requirements-macos.txt`. No installers/updater entrypoints are called by the app.
Firmware must be installed manually by the owner using Flash USB R16.command.
Prepared is not installed. Codex has not run a hardware benchmark.

## Minimum wiring

FM-1 USB0 can be either device or host, never both. Keep power on during cable
changes. In host mode it connects to a **powered hub** with compatible USB MIDI
interfaces/instruments. Mac sends to an independently connected MIDI interface;
its physical MIDI OUT connects to a host-side interface MIDI IN. Route that FM-1
input to a host-side output, then return its physical MIDI OUT to the Mac
interface MIDI IN. A single multiport interface may provide these physical links
if it independently supports the two USB hosts; ordinary one-host USB interfaces
cannot be connected to Mac and FM-1 simultaneously. MIDI DIN bandwidth limits
apply: the high-rate scenario intentionally can exceed them. A direct Mac virtual
loopback only calibrates the harness and does not exercise FM-1.

For audio: connect two supported UAC1 PCM16 stereo sources plus one supported
adaptive/synchronous output to the host hub. Sources must independently generate
audio. This app does not generate or record USB audio; analog dropouts/integrity
need an external recorder or audio analyzer. No compatibility or VBUS claim.

## Guided evaluation

The app opens at **Utvärdering** (⌘1). Choose a scenario and10/60/300 seconds.
Connection, MIDI endpoints, audio rate and test notes are under **Anslutning och
ljud**. Select a separate MIDI interface for traffic scenarios; no traffic is
sent while setting up. Start with a10-second canary.

1. **Kör utvärdering** creates an immutable attempt and pauses at **Anslut FM-1
   till Mac**. Keep power on and connect USB0 as device. The main button checks
   firmware and arms the profile; optional audio is armed at the chosen rate,
   duration and gain25. Live acquisition messages appear in Loggar.
2. **Anslut testkedjan** pauses for the powered-hub cable change. Discover and
   configure stopped routes physically, then press PLAY on FM-1. Choose MIDI ONLY
   for MIDI-only attempts. For clock monitoring enable SYNC/CLK and choose OUT.
   **FM-1 kör — starta mätning** starts only the Mac capture/traffic.
3. Watch time, sent/received counters and the latest event. **Loggar** (⌘3) shows
   ongoing traffic, connection commands, warnings and errors. **Avbryt mätning**
   (⌘.) saves an interrupted attempt; it cannot be resumed/replayed. The workflow
   pauses automatically between physical steps; it does not pause/resume the
   middle of an accepted physical capture.
4. After capture, **Hämta enhetens resultat** instructs physical STOP and USB0
   return to Mac, keeping power on. Continue to fetch/verify frozen measurements
   and device logs. Errors leave the same step available to retry with the same
   IDs. No installer, host transition or device transport command is invoked.
5. **Resultat** (⌘2) shows Mac round-trip observations and the device's measured
   workload/audio/MIDI data separately. Details expand in place. Partial attempts
   are identified. **Ny utvärdering** creates a fresh attempt rather than replaying
   an interrupted stream. **Resultatmapp** (⇧⌘E) reveals raw files.

The main action always follows the current stage (⌘Return). **Öppna körning…**
(⌘O) recovers from durable artifacts instead of replaying optimistic UI state.
Earlier app builds can be inspected read-only; continuing device commands requires
an exact app/firmware match. CPU/revision stays visible across all three views.

Scenarios send bounded non-commercial SysEx probes with a random28-bit tag,
monotonic sequence and checked payload:100/1000 short16-byte messages/s,
20×256-byte SysEx/s, idle/external audio, or receive-only clock. No musical
notes/transport are generated. Stalled sender slots are skipped, not burst-replayed.
One extra second drains return traffic; later returns are outside the host window.

Physical PLAY and app start are independent. Device/audio and host windows differ;
no exact overlap or exclusive CPU attribution is claimed. For combined loads use
60/300 seconds, start host traffic promptly and inspect device elapsed/state.
10-second audio canaries check setup, not steady-state throughput. External
sources generate audio; the app does not play/record it. Avoid conflicting clocks.

## Logs

**Loggar** has search, level filtering and a **Följ live** toggle. It shows the
latest500 host/bridge/device events; the entire event journal remains on disk.
Acquisition events stream while they happen. Device diagnostics are explicitly
marked **FM-1 · fryst**, include their device-relative timestamp, and appear only
after fetch. Firmware keeps a bounded diagnostic log; overwritten event counts
are reported, not reconstructed. Duplicate device entries are skipped on retry.

`events.jsonl` is append-only/fsynced. On open, an unterminated final line can be
truncated; duplicate IDs or interior corruption stop recovery. A logging write
failure stops traffic instead of quietly discarding the event history. Saved host
checkpoints/profile records remain authoritative. `workflow.json` is a UI projection;
physical side-effect markers decide which steps are safe after a restart.

## Read the measurements

- **Instrumented core0 work %**: exclusive timer + audio ISR + display ticks /
  profile wall ticks. Idle polling, controls, other IRQs and wrapper overhead are
  not measured. Probes inside timed regions contribute overhead. This is neither
  total CPU utilization nor measurement of core1. No second core is started.
- HOST, MIX and MIDI are inclusive subdivisions of timer work. Do not add them
  to the total. MIDI covers routing pump and USB MIDI service; internal sync
  scheduling/profiling bookkeeping remains in the timer/host parent.
- Timer gap max and gaps over150µs expose delayed100µs service. Audio frame
  margin is1000µs minus time from **software-observed** frame start to output
  completion, not exact hardware SOF. Late/missed frames and buffer under/overrun
  counters are separate. Buffer minimum frames becomeµs at configured sample rate;
  source clock drift may empty/fill buffers without CPU overload.
- Mac probe p50/p99/max are observed round-trip through all selected endpoints.
  Input timestamps use CoreMIDI's host clock, fallback to callback time if zero.
  Timestamp granularity, packet grouping, interface buffering and macOS scheduling
  contribute. This does not isolate FM-1 latency. Clock interval mean/min/max are
  receive observations, not oscillator accuracy certification. Pulses sharing a
  timestamp are counted separately as lacking an independent timestamp; they are
  excluded from interval statistics, which cannot resolve their jitter.
- Check send skips/errors before claiming a requested rate was achieved. No return
  traffic may mean a bad route rather than loss. Duplicate/reordering counts are
  checked against sent probe IDs; unrelated traffic is ignored.
- Device build,112-bit nonce, topology/route hashes and optional sync settings
  identify a physical run. Notes, chosen endpoints and app source fingerprint
  identify the host scenario. Preserve both when comparing firmware.

CoreMIDI timestamp/packet semantics: [Apple MIDI Services](https://developer.apple.com/documentation/coremidi/midi-services),
[host timestamps](https://developer.apple.com/documentation/coremidi/miditimestamp).

## Durability and recovery

Persistent path: `~/Library/Application Support/USB Bench/runs/<UUID>/` on the
Mac's normal user volume, outside build outputs. Do not use a temporary/network
volume for accepted results. The app uses an exclusive run lock and atomically
writes/fsyncs each manifest/checkpoint plus its directory. During capture it saves
at least each second. `started.json` is durable **before** outgoing traffic;
`host.json` and checksum `HOST_COMPLETE` mark a finished capture. Diagnostic
`device/result.json` includes raw CRC-checked profile bytes and `device/COMPLETE`
checksums; `device-report.json` is the display projection. Audio has its own raw
snapshot/arming token. There is no combined hardware-PASS marker.

Use **Öppna körning…** (⌘O) after restart. Completed captures are never rerun;
interrupted physical captures are marked interrupted and cannot restart. Up to
one second of interim host counters may be lost, but no partial run becomes
complete. Fetch/ARM resumes with the same immutable IDs; a new attempt must not
silently replace a pending device result. Power loss destroys firmware RAM data
and cannot resume the physical stream. Keep the same app/firmware build to resume.

Offline validation:

```sh
python3 benchmark-app/validate_run.py '/path/to/run'
```

A finished host capture may be validated without device data; the output states
which phases exist. Use `--require-device` to require the exact device profile.
Tests include a real SIGTERM/SIGKILL recovery fixture without MIDI/device access.
No sustained hardware traffic or costly benchmark is launched during build.

Every instrument page has `C0:nn% C1:OFF R16`. C0 is cumulative instrumented
work since physical run start, retained after STOP; -- means no sample. Core1
is unused, not a measured zero. The Mac header always shows bundled revision,
both cores and explicitly identifies its CPU0 number as the last frozen profile.
