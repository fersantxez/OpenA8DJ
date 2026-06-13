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
make soundcheck \
  SOUNDCHECK_CAPTURE="External Recorder" \
  SOUNDCHECK_CAPTURE_CHANNELS=1,2 \
  SOUNDCHECK_CPU_STRESS=1
```

The initial rejection gates for that symptom are:

```text
mid_band_1000_5000_residual_ratio > 0.04
mid_band_cpu_corr > 0.60 when the windowed mid-band residual ratio is above 0.02
```

For a driver experiment, use this order:

1. `make soundcheck-preflight`
2. `make soundcheck ...` on the currently installed driver, if capture is
   physically connected
3. rebuild/install the candidate
4. `make smoke-hal` and the normal transport checks
5. `make soundcheck SOUNDCHECK_CPU_STRESS=1 ...` again before asking for human
   listening

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

## Distribution Limitation

Core Audio enumeration, 8-in/8-out topology at 44.1/48 kHz, MIDI endpoint
publication, control read/write, buffer-size negotiation, and package install
have been validated locally for 0.3.25. Public distribution still requires a
Developer ID Installer certificate and Apple notarization. Keep physical
Traktor timecode validation, 88.2/96 kHz validation, and human listening across
all output pairs as final release matrix gates.
