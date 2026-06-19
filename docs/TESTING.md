# OpenA8DJ Testing

## Build

```sh
make
```

This builds:

- `build/OpenA8DJ.driver`
- `build/opena8dj-probe`
- `build/audio-list`
- `build/audio-inspect`
- `build/audio-io-test`
- `build/audio-config`
- `build/audio-record`
- `build/audio-default`
- `build/audio-pair-tone`
- `build/audio-route`
- `build/macbook-mic-record`
- `build/opena8dj-usb-play`
- `build/opena8dj-control`
- `build/opena8dj-midid`
- `build/midi-list`

## Package

```sh
make package
sudo installer -pkg build/OpenA8DJ-<version>.pkg -target /
```

The package installs the HAL bundle, the control/MIDI tools, a user LaunchAgent
in `/Library/LaunchAgents`, and an uninstall helper at:

```sh
/usr/local/bin/opena8dj-uninstall
```

The package is unsigned unless a valid Developer ID Installer identity is passed
with `PKG_SIGN_IDENTITY`. The local test package has been installed with
`installer` and verified on macOS 26.5.

## Installed HAL Driver

The current HAL bundle is installed at:

```sh
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
```

After install/restart, device enumeration should show:

```text
Open Audio 8 DJ  uid=org.opena8dj.Audio8DJ  in=8 out=8 rate=48000
```

Local validation on 2026-06-13 after restoring the Traktor/timecode channel
surface on top of the 0.3.24 capture-paced playback tuning:

```text
Dispositivos Core Audio: 4
  Open Audio 8 DJ  uid=org.opena8dj.Audio8DJ  in=8 out=8 rate=48000
```

Detailed channel inspection should show 8 input channels and 8 output channels.
The 0.3.25 macOS-safe topology exposes one 8-channel input stream with A/B/C/D
left/right channel names, plus four stereo output streams:

```sh
./build/audio-inspect
```

Expected output shape:

```text
input buffers: 1 [8 channels] total=8
output buffers: 4 [2 channels] total=8
input streams: 1
output streams: 4
input channel names: 1=Input A Left ... 8=Input D Right
output channel names: 1=Output A Left ... 8=Output D Right
```

## Core Audio I/O Matrix

The Core Audio I/O path can be tested at each advertised HAL sample rate:

```sh
./build/audio-io-test 2 44100
./build/audio-io-test 2 48000
./build/audio-io-test 2 88200
./build/audio-io-test 2 96000
```

Local HAL I/O validation on 2026-06-10 completed successfully at 44.1, 48,
88.2, and 96 kHz. Human listening validation is still the release gate for
declaring subjective audio quality final, but the automated I/O counters were
healthy at all advertised rates.

## Automated Real-Music Soundcheck

Audio-path changes should use the automated soundcheck before and after the
change. This does not replace final human listening forever, but it catches the
class of regressions where counters look healthy while music sounds metallic,
clicky, or unstable.

Prepare a repeatable real-music fixture before rebuilding/installing a new
driver idea:

```sh
make soundcheck-preflight
```

The preflight target searches local music, converts a safe excerpt to
`reference.wav`, and writes the run under:

```text
local-analysis/soundcheck/<run-id>/
```

It defaults to a short 5-second `start` excerpt via
`SOUNDCHECK_PREFLIGHT_SECONDS=5` and `SOUNDCHECK_PREFLIGHT_MODE=start` so it is
cheap enough to run before trying a driver idea. Use a longer or denser excerpt
when needed:

```sh
make soundcheck-preflight SOUNDCHECK_PREFLIGHT_SECONDS=20 SOUNDCHECK_PREFLIGHT_MODE=dense
```

Use an explicit track when checking the exact song that exposed a problem:

```sh
make soundcheck-preflight SOUNDCHECK_MUSIC="/path/to/track.mp3" SOUNDCHECK_MODE=dense
```

Run the full analog loopback when an input recorder is connected:

```sh
make soundcheck SOUNDCHECK_CAPTURE="External Recorder" SOUNDCHECK_CAPTURE_CHANNELS=1,2
```

The full soundcheck:

- configures `org.opena8dj.Audio8DJ` to the requested rate/buffer
- plays the prepared reference WAV through the selected output pair
- records the selected Core Audio input device/channel pair
- compares reference vs capture for alignment, SNR, click outliers, lag jumps,
  high-band residual, 1-5 kHz residual noise, CPU correlation, and clipping
- samples CPU for Core Audio, the OpenA8DJ driver, audio/UI services, Spotify,
  Traktor, the player, and the recorder
- writes `summary.txt`, `metrics.json`, `cpu-profile.tsv`, and
  `coupling-profile.json`

When checking the specific "radio/vinyl noise that changes with CPU/window
activity" failure, run the soundcheck with an opt-in CPU pressure phase:

```sh
make playback-cpu-gate

make output-pair-smoke-gate

make capture-device-diagnose

make physical-bench-sanity-gate

make candidate-preflight CANDIDATE_PREFLIGHT_LABEL=current-loaded

make soundcheck \
  SOUNDCHECK_CAPTURE="External Recorder" \
  SOUNDCHECK_CAPTURE_CHANNELS=1,2 \
  SOUNDCHECK_CPU_STRESS=1
```

`make playback-cpu-gate` is an internal pre-physical gate. It plays the real
music fixture through the installed OpenA8DJ device, samples driver/coreaudiod
CPU, applies CPU stress, and applies a conservative UI/WindowServer stress
phase using silent screen captures. It must pass before a build is promoted to
physical iRig gates, but it does not replace physical capture.

`make output-pair-smoke-gate` is an internal A/B/C/D surface gate. It plays a
short low-level fixture through each exposed stereo pair and rejects a build if
any pair starts slowly, fails to advance output frames, or reports internal
underruns/resets/panic flags. It does not prove analog routing or mixer output
quality; iRig capture remains the physical proof.

`make capture-device-diagnose` is a read-only physical-capture diagnosis. It
records the Core Audio input devices, USB device summary, exact iRig USB/Core
Audio readiness, and any non-built-in external input candidates. It does not
promote alternate inputs automatically; the physical quality gate still uses the
configured capture device. For iRig Stream candidate gates, the USB identity
must match `0x1963:0x0059`; the `IK Multimedia` / `iRig Stream` strings are
reported as diagnostic evidence but are not enough to mark the capture path
READY. Core Audio readiness still requires the iRig Stream input UID.

The parser can be regression-tested without hardware:

```sh
make capture-device-diagnose-selftest
```

This fixture test covers decimal IDs, `0x` IDs, bare hex-like IDs, USB-present
but Core-Audio-missing cases, false USB devices, and name-only iRig impostors.

`make irig-isolation-diagnose` is a state-changing diagnosis for the specific
case where iRig Stream is physically connected but absent from macOS. It
temporarily removes OpenA8DJ from the active HAL directory, restarts audio
services, checks whether iRig appears without OpenA8DJ, restores OpenA8DJ, and
checks again. Use it to separate an OpenA8DJ/Core Audio interaction from a USB
enumeration problem outside the driver. The script must leave OpenA8DJ restored
before its result can be trusted.

`make irig-usb-recovery-diagnose` is the next recovery step when the user says
iRig is physically plugged in but `capture-device-diagnose` still reports
`irig_missing_from_usb_tree`. It records USB/Core Audio snapshots, restarts
audio services, tries an exact `0x1963:0x0059` iRig reset only if macOS can
match that device, and then watches for recovery. It also records low-level USB
port counters from IOService. If the result contains `usb_enumeration_failures=YES`
with non-zero `enum_fail` / `addr_fail`, macOS is seeing a port-level handshake
failure before an iRig USB device exists. In that state, do not rebuild the
driver or ask for listening; power-cycle iRig, move it to a direct Mac port or
known-good data cable/hub, then rerun this target.

The same target writes `port_delta`. `usb_port_counter_changes=NO` means the
watch window saw no new USB connection/enumeration attempts; stale accumulated
port failures still explain the current missing device, but the latest action
did not reach the USB bus. `usb_port_counter_changes=YES` means the physical
action reached the bus; if iRig is still missing, inspect which port gained
`delta_enum_fail` or `delta_addr_fail` before touching driver code.

`make physical-bench-sanity-gate` is the physical bench gate before candidate
judgment. It requires clean stack health after a short cooldown/enumeration
guard, verified capture device readiness, a short successful capture, no
clipping, and no implausibly hot idle level. It is not an audio-quality
approval; it only proves the bench is safe enough to run physical tone/music
candidate gates. When the capture device is `iRig Stream`, the recorder log must
also prove both `device="iRig Stream"` and an `IK Multimedia:iRig Stream` Core
Audio UID before this gate can pass.

Physical music runs also include `measurement_status` in
`physical-music-gate.json` and `summary.txt`. `VALID` means the run can be used
to judge candidate quality. `BLOCKED_DIRTY_ROUTE`, `BLOCKED_CAPTURE_LEVEL`,
`BLOCKED_CAPTURE_ROUTE_UNVERIFIED`, and `BLOCKED_MISSING_CPU_PROFILE` mean the
measurement itself is not valid evidence for or against a driver build.
Diagnostic bypasses such as `--allow-non-irig` or
`--allow-missing-cpu-profile` set `measurement_status=DIAGNOSTIC_ONLY`; they may
help debugging but must not produce `candidate_quality_status=PASS`.

`make candidate-preflight` is the single pre-human-test gate. It runs stack
health, `output-pair-smoke-gate`, `timecode-smoke-gate`,
`playback-cpu-gate`, `physical-bench-sanity-gate`, iRig USB/Core Audio
recovery, a short iRig capture, and then the full physical
`candidate-listen-gate` when iRig is ready. If iRig is absent, it fails with
`candidate_preflight=BLOCKED_PHYSICAL_CAPTURE`; that is a bench/capture blocker,
not an audio-quality approval or rejection. Every preflight result records the
installed driver version and SHA-256 hash it actually tested. Diagnostic runs
that skip the physical candidate gate must not emit `candidate_preflight=PASS`.

If iRig is currently absent and you want the machine to wait safely until the
capture path returns, use:

```sh
make candidate-watch CANDIDATE_WATCH_LABEL=<version>
```

`candidate-watch` only polls Core Audio and IOUSB. It does not reset USB devices
or restart Core Audio. Each poll records a `capture-device-diagnose` result and
uses that common diagnosis as the readiness source. When iRig appears in both
USB and Core Audio for `CANDIDATE_WATCH_STABLE_POLLS` consecutive polls
(default: 3), it runs the full `candidate-preflight` ladder automatically. A
single transient iRig appearance is not enough to start physical gates.
When iRig is absent from the USB tree, `capture-device-diagnose` also records
`usb_enumeration_failures` and `failed_usb_ports` from IOService USB port
counters when macOS exposes them. This distinguishes a cleanly missing capture
device from a lower-level USB handshake/enumeration failure. It also records a
plain `next_recovery_action` so watch/status outputs say whether the next step
is to power-cycle or move iRig, restart Core Audio, verify device identity, or
proceed to the physical gate.

`irig-recovery-gate` distinguishes short capture readiness from candidate
approval. Without `--run-candidate-gate`, a successful iRig return emits
`irig_recovery_gate=READY_FOR_PHYSICAL_GATE`, not `PASS`. A candidate PASS
requires the full physical `candidate-listen-gate`.

For a read-only summary of the current installed candidate and latest gate
state:

```sh
make candidate-status
```

`candidate-status` does not start audio. It reports installed version/hash,
Core Audio/USB device visibility, audio stack health, and the latest
`output-pair-smoke-gate`, physical capture diagnosis,
`physical-bench-sanity-gate`, `irig-usb-recovery-diagnose`,
`candidate-preflight`, and `candidate-watch` result. It includes the current
physical capture diagnosis `usb_enumeration_failures` / `failed_usb_ports` plus
the latest iRig USB recovery verdict, `usb_port_counter_changes`, and the
port-summary / port-delta paths so one status file explains whether the capture
blocker is Core Audio, USB identity, or a lower-level USB handshake problem. It
also reports any active `safe-replug-watch` LaunchAgent status and run
directory. It may report `READY_FOR_HUMAN_TEST` only when the current iRig USB
ID and Core Audio input are present and the latest PASS preflight hash matches
the currently installed driver hash.

Use `candidate-status` before asking for human listening, sending readiness
email, or starting a long `candidate-watch` session.

The readiness email is guarded by:

```sh
make candidate-ready-email-gate
```

This target never sends mail. It runs `candidate-status` and prepares the email
body only when the installed candidate is `READY_FOR_HUMAN_TEST`. If any gate is
missing or blocked, it exits blocked and records the exact reason. The Gmail
send step is allowed only after this gate returns PASS. As a second guard, this
script rechecks the iRig USB ID, Core Audio iRig input, preflight hash, required
subgate states, and the linked preflight result file before preparing any email
payload.

To leave the full physical readiness flow waiting for iRig and guarded against
premature email payloads, use:

```sh
make candidate-watch-ready-email-gate
```

This target runs `candidate-watch`; only after the stable-watch/preflight ladder
passes does it run `candidate-ready-email-gate`. It still never sends mail
itself. A PASS means the email payload is prepared and the Gmail connector may
send it. If the watch is blocked, the top-level result includes
`usb_enumeration_failures`, `failed_usb_ports`, and `next_recovery_action`
copied from the watch result.

For a watcher that survives the current shell and is appropriate while waiting
for a physical hub/iRig replug, use:

```sh
make safe-replug-watch-start
make safe-replug-watch-status
make safe-replug-watch-stop
```

This installs/runs a user LaunchAgent named
`com.fer.opena8dj.safe-replug-watch`. While iRig is absent it only polls USB and
Core Audio state; it does not reset USB, restart Core Audio, reboot, or install
drivers. If iRig returns and remains visible for
`SAFE_REPLUG_WATCH_STABLE_POLLS` consecutive polls (default: 3), it runs the
same guarded `candidate-watch-ready-email-gate` flow. It still does not send
mail by itself; it only reaches the email-payload gate after all physical gates
pass.

`make safe-replug-watch-status` also reports the latest poll attempt,
`ready_streak`, USB/Core Audio iRig visibility, failed USB port details, and
the next recovery action. That makes it safe to check progress without opening
the watcher run directory by hand.

For unattended recovery and QA, use the autonomous supervisor instead:

```sh
make autonomous-audio-qa-start
make autonomous-audio-qa-status
make autonomous-audio-qa-stop
```

This installs/runs `com.fer.opena8dj.autonomous-audio-qa` as a user
LaunchAgent. It assumes Audio 8 DJ and iRig Stream are physically connected. If
iRig is not visible, that is treated as macOS USB/Core Audio state to recover in
software. The first missing-iRig cycle runs a bounded recovery immediately, then
the loop retries at `AUTONOMOUS_AUDIO_QA_RECOVERY_INTERVAL_CYCLES` intervals
(default: 10 cycles at a 30-second polling interval). Recovery refreshes Core
Audio, audio HAL, the USB audio daemon, and USB power daemon state; it only
resets the iRig USB device when the
exact iRig VID/PID object is visible. It never resets Audio 8 DJ and never
installs a candidate. The supervisor uses the lightweight recovery mode, which
keeps `ioreg`, port counters, and Core Audio diagnosis but skips repeated
`system_profiler` snapshots in the permanent loop.

When Audio 8 DJ and iRig Stream are visible for
`AUTONOMOUS_AUDIO_QA_STABLE_POLLS` consecutive polls, the supervisor runs the
same `candidate-watch-ready-email-gate` ladder. A PASS prepares the readiness
email payload only after physical iRig gates, CPU gates, output-pair smoke, and
timecode smoke have passed. It still does not send mail by itself.

The post-reboot startup LaunchAgent starts this autonomous supervisor after
opening Codex. The expected check after any reboot is:

```sh
./scripts/autonomous-audio-qa-status
```

`make candidate-status` also reports the active autonomous supervisor fields:
`autonomous_audio_qa_status`, pid, run directory, stable poll threshold,
recovery interval, latest cycle, latest capture status, and latest reason.

The initial rejection gates for that symptom are:

```text
mid_band_1000_5000_residual_ratio > 0.04
mid_band_cpu_corr > 0.60 when the windowed mid-band residual ratio is above 0.02
```

For a driver experiment, use this order:

1. `make soundcheck-preflight`
2. `make output-pair-smoke-gate`
3. `make playback-cpu-gate`
4. `make capture-device-diagnose`
5. `make physical-bench-sanity-gate`
6. `make soundcheck ...` on the currently installed driver, if capture is
   physically connected
7. rebuild/install the candidate
8. `make smoke-hal` and the normal transport checks
9. `make output-pair-smoke-gate OUTPUT_PAIR_GATE_LABEL=<version>`
10. `make timecode-smoke-gate`
11. `make playback-cpu-gate` on the candidate
12. `make candidate-preflight CANDIDATE_PREFLIGHT_LABEL=<version>` before asking
   for human listening or sending readiness email

Pair routing can be tested with:

```sh
./build/audio-pair-tone A 5 440 0.06
./build/audio-pair-tone B 5 660 0.06
./build/audio-pair-tone C 5 880 0.06
./build/audio-pair-tone D 5 1100 0.06
```

Local Traktor validation on 2026-06-08 confirmed that Output A and Output B
route independently at 44.1 and 48 kHz. Output C/D are exposed and expected to
follow the same mapping, but still need the physical mixer pass before being
marked complete.

## Buffer Size

Core Audio exposes both the modern frame-based buffer properties and the older
byte-based properties used by some legacy audio applications:

```text
buffer-range=15-4096
buffer-byte-range=120-32768
```

Local validation confirmed successful I/O at 48 kHz for 64, 128, 256, 512,
1024, 2048, and 4096 frame buffers. The deprecated byte-size path was also
validated from 512 to 32768 bytes, which maps to the same stereo-frame sizes.
Traktor buffer-size selection was re-tested after the 0.2.4 buffer-property fix
and no longer depends on the invalid sentinel value seen in earlier builds.

## USB Transport Probe

Non-invasive:

```sh
./build/opena8dj-probe
```

Active USB/device-info:

```sh
./build/opena8dj-probe --claim
```

Isochronous transport:

```sh
./build/opena8dj-probe --iso-test
```

Validated protocol facts:

```text
USB ID: 17cc:1978
EP1 bulk OUT/IN: 0x01 / 0x81
Isochronous capture/playback: 0x82 / 0x06
Firmware: 14
Analog audio in/out: 8 / 8
MIDI in/out: 1 / 1
Data alignment: 2
AUDIO_PARAMS 48 kHz: reply 09 01, bpp=352
```

44.1, 48, 88.2, and 96 kHz have all accepted `AUDIO_PARAMS` in probe tests.

The HAL transport now uses an asynchronous isochronous queue. Output is paced
from successful capture transactions and explicit future-frame scheduling
remains off. The 0.2.13 build also ports stream-start details from the legacy
macOS kext: a reset-style `AUDIO_PARAMS` call before the real stream parameters,
64 capture transfers, and 128 maximum playback transfers.

The 0.2.17 output path is timeline-based rather than FIFO-based. The HAL passes
`mOutputTime.mSampleTime` into the USB engine, and the USB engine stores output
frames in a circular sample timeline before USB reads them. This is closer to
the old kext's `clipOutputSamples` model and is intended to remove callback
ordering jitter from the playback path.

0.2.17 also exposes the Core Audio `cfsz` cycle-size selector observed in
coreaudiod property probes and keeps the preferred cycle size at the small value
Core Audio selected after reload. The public `fsiz`/`fsz#` values can still look
derived through Core Audio's wrapper, so listening tests and `stream-stats` are
more important than treating those public timing lines as proof of audio health.

Important regression note: 0.2.11 tried mode 2 output packing at byte offset 2
after reading the old kext's mode 2 input cursor. That was wrong for playback
and produced loud white noise. The corrected path restores playback
byte offset 4 and must be validated only with a controlled low-volume listening
test.

Stream health is also available without enabling trace logging:

```sh
/usr/local/bin/opena8dj-control stream-stats
```

During a sustained playback test, watch these fields:

- `clock-anchor`: for the current 0.2.47 playback build, `fallback` is expected
  because `HAL_USB_CLOCK_ANCHOR=0` avoids expensive `frameNumberWithTime`
  polling that does not feed the HAL timestamp path. If rebuilding with USB
  clock anchors enabled, then it should become `valid`.
- `output-ring`: should remain near the target rather than growing without
  bound or draining to zero.
- `output.active-underruns`, `playback.failed`, `playback.qfail`, and
  `capture.qfail` should stay at zero during steady playback.
- `capture.failed` can rise because the hardware reports inactive high-speed
  microframes separately from valid audio payloads; treat it as suspicious only
  if `capture.bytes`, `playback.bytes`, or output frame consumption stop moving
  together.
- `scheduling.too-old`, `scheduling.too-new`, and `out-of-window` should remain
  at zero during steady playback.
- `elastic-drops` and `elastic-replays` should remain zero in short 48 kHz
  tests. Repeated sample replays are audible, so they are not used as normal
  drift correction in 0.2.13.

Installed 0.2.18 build on 2026-06-10:

```text
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
```

Build hash:

```text
4eac6fada5e2aff6b1770d13e2d753be18c102bfeaefc96a5b658a81e2530a88
```

The 0.2.18 bundle passed `make all smoke-hal`. After installation, Core Audio
enumerated `Open Audio 8 DJ` with 8 inputs and 8 outputs, and the system default
output was explicitly left on MacBook Air Speakers. The next playback validation
must start with the analog/headphone volume down.

0.2.18 specifically tests active-gap concealment: when USB needs a frame and
the timeline has a missing active frame, the driver briefly replays the previous
valid frame with decay instead of injecting an immediate hard zero. Watch
`output.active-underruns` and `output.elastic-replays` together during the
Spotify and Codex-microphone tests.

Listening note from the first timeline-based human test: Spotify output was
still not good enough, but it sounded slightly improved compared with the
previous build. Pressing the Codex voice/microphone button made the output noise
increase sharply even though the laptop microphone was selected. Test the base
playback path first; then repeat with the voice/microphone button as a separate
second-phase reproduction.

0.2.19 changed the next listening test. The old kext analysis showed an
IOAudioEngine-style default of `0x200` frames, while the HAL had been exposing
22 frames as the public Core Audio cycle. Core Audio logs during the microphone
reproduction showed repeated overloads and `client timeout` entries with a ring
buffer size of 22. The 0.2.19 HAL therefore restores an app-facing default of
512 frames and a 512-4096 frame range; USB packet pacing remains internal.

0.2.20 hardens this further: valid buffer requests are normalized to 512, 1024,
2048, or 4096 frames, so a client cannot leave the device at 192 or another
small public cycle.

0.2.21 disabled Audio 8 input I/O by default while keeping output I/O active.
0.2.22 goes further and hides input stream objects unless
`OPENA8DJ_ENABLE_INPUT_IO=1` is used at build time. This is intentional for the
microphone reproduction: activating the laptop microphone must not make Core
Audio run the Audio 8 as a full-duplex device.

For 0.2.22 validation, first confirm:

```sh
./build/audio-inspect
./build/audio-config org.opena8dj.Audio8DJ 48000 512
```

For 0.2.29 and the 0.2.30 diagnostic build, `cfsz` is not a public client
property. The expected device list is `in=0 out=8`. The expected timing line is
`buffer=512`, `cycle-error=<nonzero>`, and `buffer-bytes=16384`. If `buffer=22`
or `buffer=192` appears after reload, the wrong HAL is still loaded or Core
Audio has cached stale properties.

The microphone-interaction regression test uses output-only Audio 8 playback
while `BuiltInMicrophoneDevice` records. It should leave
`output.active-underruns`, `elastic-drops`, `elastic-replays`,
`timeline-resets`, `playback.failed`, `mode2.input-check-errors`, and
`mode2.output-panic-flags` at zero.

The diagnostic capture build adds evidence for the part that counters cannot
prove: what Core Audio wrote into the HAL versus what the USB output path
actually consumed. A successful diagnostic run must include human listening
notes and the capture files; clean counters alone are not enough to declare
Spotify playback fixed.

Before each human playback pass:

1. Confirm the installed diagnostic build identity and record its version,
   build number, loaded UUID, and binary hash.
2. Confirm the intended Core Audio shape with `audio-list`, `audio-inspect`, or
   the equivalent installed tools: `in=0 out=8`, `rate=48000`, `buffer=512`,
   and `buffer-bytes=16384`.
3. Use the same physical output pair, mixer/headphone path, macOS volume,
   Spotify volume, Spotify track, and Spotify timestamp across all comparison
   passes.
4. Move aside any existing `/tmp/opena8dj-*` artifacts before starting the
   pass, because the diagnostic capture files are overwritten by the next
   stream.

Run the Spotify comparison as two separate passes:

1. `spotify-only`: play the selected Spotify segment through `Open Audio 8 DJ`
   with no microphone capture active. Record whether playback is clean,
   crackly, distorted, pitch-shifted, intermittent, or stopped.
2. `spotify-mic`: restart the same segment from the same timestamp, then
   activate the selected microphone path while Spotify is already playing. Use
   the same external microphone that reproduced the issue when applicable; use
   a separate labeled pass for `BuiltInMicrophoneDevice` if that comparison is
   also needed.

For both passes, record `opena8dj-control stream-stats` before playback, during
steady playback, immediately after microphone activation when applicable, and
after stopping playback. After each pass, stop all clients using
`Open Audio 8 DJ` long enough for the diagnostic capture to flush, then collect:

```text
/tmp/opena8dj-output-capture.txt
/tmp/opena8dj-output-written-f32.raw
/tmp/opena8dj-output-consumed-f32.raw
/tmp/opena8dj-output-packed-usb.raw
/tmp/opena8dj-output-events.tsv
```

The raw files are little-endian interleaved float32, 8 channels. The metadata
file records `sample_rate`, `channels`, `written_frames`, `consumed_frames`,
packed byte counts, and event counts. The packed USB file is the mode-2 output
payload after float-to-USB conversion; the events file records write,
consume-anomaly, and packed-transfer points with an explicit `timeline` column
so sample-time, served-frame, and USB-frame values are not mixed accidentally.
The capture is bounded, so keep the microphone transition inside the first part
of the pass. Also collect `/tmp/opena8dj-usb.log`,
`/tmp/opena8dj-hal-trace.log`, and `/tmp/opena8dj-midid.log` when present, but
do not collect `/tmp/opena8dj-control.sock`.

For a deterministic source-vs-driver comparison, use a known PCM16 WAV instead
of Spotify:

```sh
./build/audio-wav-play path/to/reference.wav A
python3 scripts/analyze-driver-capture.py path/to/reference.wav --capture-dir /tmp --usb-raw /tmp/opena8dj-output-packed-usb.raw --events /tmp/opena8dj-output-events.tsv --pair A
```

Interpret the results in this order:

1. If the human listener hears crackle, dropouts, or a full stop, the pass fails
   even when `stream-stats` looks clean.
2. If `written` aligns with the reference but `consumed` does not, the problem
   is after Core Audio handed frames to the HAL.
3. If `written` and `consumed` both look clean while Spotify still sounds bad,
   keep the files and the exact listening notes; the failure is still real and
   needs a reproduction path beyond the current counters.

0.2.25 uses a bounded 3 ms property-probe backoff during startup enumeration.
This should not create `/tmp/opena8dj-hal-trace.log` and should not affect the
USB audio callback path.

## MIDI And Controls

The package installs `opena8dj-midid` as a LaunchAgent. It creates CoreMIDI
endpoints:

```text
Open Audio 8 DJ MIDI In
Open Audio 8 DJ MIDI Out
```

Validate with:

```sh
./build/midi-list
/usr/local/bin/opena8dj-control
launchctl print gui/$(id -u)/org.opena8dj.midid
```

The control tool exposes:

- `input-mode`
- `gnd-vinyl`
- `gnd-cd-line`
- `gnd-phono`
- `software-lock`

The input mode mapping is:

```text
0 -> timecode-vinyl
1 -> timecode-cd-line
2 -> phono
```

For Traktor timecode vinyl testing, apply:

```sh
/usr/local/bin/opena8dj-control profile timecode-vinyl
```

The profile sets `input-mode` to `0`, enables the timecode vinyl ground-lift
flag, and enables `software-lock` so the front-panel input switch cannot
accidentally move the hardware out of DVS mode during a test.

Initial Traktor operator validation for Timecode Vinyl passed in the older
0.2.x line. For 0.3.25, treat this as a preserved compatibility target: the
8-input/8-output channel surface and `timecode-vinyl` hardware profile are
present, but the full physical input-pair and CD/line matrix remains a release
regression gate.

For an automated non-physical timecode surface smoke test:

```sh
make timecode-smoke-gate
```

This applies `timecode-vinyl`, verifies input decode is enabled, runs a short
duplex Core Audio I/O test, checks input/output frames advanced, and restores
the playback profile. It does not replace physical Traktor vinyl/scope testing.

## Digital Pre-Physical Gate

Before any physical iRig capture or human listening request, run the digital
pre-physical gate:

```sh
./scripts/digital-audio-quality-gate \
  --candidate 0.3.135-atomic-written \
  --music-file local-analysis/baseline-fixtures/nueva-mexico-baseline-268s-60s-48k-s16.wav
```

This gate is software-only and runs before the physical bench:

- exact Core Audio UID checks for `org.opena8dj.Audio8DJ`; a generic
  `Open Audio 8 DJ` name match is not enough when the Rust worktree driver or
  another HAL implementation is active;
- simulated digital USB packing for output pairs A/B/C/D;
- source-vs-decoded residual, SNR, click, and mid-band checks;
- real playback through the installed driver under CPU/UI stress;
- strict failure on timeline reset, active underrun, elastic drop/replay, late
  write, playback completion outlier, capture-to-playback queue outlier,
  zero-complete playback transaction, high driver/coreaudiod CPU, or missing
  UI-stress evidence.

Passing this gate means only that the candidate is low-risk under internal
stress. It does not measure physical sidebands, metallic coloration, analog
noise, iRig residual/SNR, Traktor vinyl scope quality, or human listening
quality. The required promotion ladder remains:

```text
internal quality window
digital-audio-quality-gate
candidate physical preflight when iRig is present
physical music/tone gate through iRig
human listening only after physical gates pass
```

`candidate-preflight` runs `digital-audio-quality-gate` before
`physical-bench-sanity-gate`; if the digital gate fails, physical gates are not
attempted.

If it passes, the candidate is only cleared to proceed to physical testing.
Physical iRig capture remains mandatory and is not weakened by this digital
gate.

## Distribution Limitation

Core Audio enumeration, 8-in/8-out topology at 44.1/48 kHz, MIDI endpoint
publication, control read/write, buffer-size negotiation, and package install
have been validated locally for 0.3.25. Public distribution still requires a
Developer ID Installer certificate and Apple notarization. Keep physical
Traktor timecode validation, 88.2/96 kHz validation, and human listening across
all output pairs as final release matrix gates.
