# OpenA8DJ audio crackle handoff - 2026-06-10

## Current state - 2026-06-12 0.2.57 loopback gate added, no candidate active

No active listening candidate is installed.

`0.2.57` / build `59` was built as the first old-kext-compatible packing
candidate after the disastrous `0.2.56` result:

- Default physical output packing changed back to native/little 24-bit
  (`HAL_OUTPUT_NATIVE=1`).
- Diagnostic builds now capture decoded USB input loopback to:
  `/tmp/opena8dj-input-loopback-f32.raw`.
- Added a local gate:
  `scripts/verify-loopback-quality.sh`.
- Added deterministic test fixture generation:
  `scripts/generate-loopback-reference.py`.
- Added loopback/reference analyzer:
  `scripts/analyze-loopback-quality.py`.

Old-kext evidence that motivated `0.2.57`:

- `createNewAudioStream()` advertises 24-bit signed PCM with little/native byte
  order.
- `AppleClipOutputSamples()` routes that format through
  `_Float32ToNativeInt24_X86`.
- `_Float32ToNativeInt24_X86` writes native 24-bit bytes from the Q31 value.
- `fillErrorCorrectionPatternMode2()` puts check bytes at offset `streams * 2`
  (`8` for Audio 8 DJ), with the known alternating stream pattern.

Gate results:

```text
local-analysis/loopback-0257-script-install-20260612-092158

internal consumed vs reference:
  pass=1
  lag_seconds=0.00000000
  left_gain=1.00000000
  right_gain=1.00000000
  min_snr_db=999.00
  min_correlation=1.000000
  click_outliers=0
  max_spectral_ripple_db=0.00

decoded input loopback vs reference:
  pass=0
  min_snr_db=-68.04
  min_correlation=-0.000396
  max_spectral_ripple_db=69.21
  click_outliers=0
  left_capture_rms=0.00073286
  right_capture_rms=0.00058928
```

Interpretation: the HAL/USB engine consumed the reference perfectly, but the
decoded input loopback did not contain the played signal. This looks like "no
valid physical loopback signal present" or an input-capture routing/decode issue,
not proof that analog output is good.

Safety action:

```text
OpenA8DJ.driver moved out of active HAL:
  /Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.failed-loopback-20260612-092228
Core Audio was force-restarted after a blocked audio-list/coreaudiod-hot state.
Final driver check: no active OpenA8DJ HAL. `coreaudiod` may still spike after
these load/unload attempts because non-driver audio clients such as Sound
Settings, Control Center, Spotify, Native Instruments agents, or the Codex audio
helper can keep poking the macOS audio stack. Treat that as a separate cleanup
state; do not confuse it with an installed OpenA8DJ candidate.
```

Do not ask the user to listen to `0.2.57` until one of these is true:

- A real physical loopback cable is connected from Audio 8 output A L/R to input
  A L/R and `scripts/verify-loopback-quality.sh --install` passes.
- A separate capture interface proves the analog output is clean.
- A new autonomous proof replaces analog loopback with an equally decisive
  physical measurement.

## Previous state - 2026-06-12 0.2.56 invalidated

No active listening candidate is installed.

The `0.2.56` / build `58` post-reboot candidate is invalidated by human
listening. The user reported disastrous audio: lower/heavier tone, faint bass in
the background, and music sounding completely filtered/noisy through a middle
filter. This is a total failure, not a partial improvement.

Immediate safety action after the report:

```text
default output reset to BuiltInSpeakerDevice
OpenA8DJ.driver moved out of active HAL:
  /Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.disastrous-0256-20260612-090645
Core Audio devices after disable:
  MacBook Air Microphone
  MacBook Air Speakers
freeze dir:
  local-analysis/2026-06-12-spotify-0256-disastrous-20260612-090645
```

Conclusion: do not reuse the `0.2.55`/`0.2.56` stream-usage/flush/ISO-64
family as a candidate. The internal gates below still passed while the analog
result was unacceptable, so these gates are necessary but not sufficient.

Next candidate must be preceded by an autonomous physical audio-quality check:
either a hardware loopback capture from Audio 8 output to Audio 8 input, or a
direct old-driver-derived byte-lane/packing proof. Transport counters alone
cannot qualify another user listening build.

## Rejected build details - 2026-06-12 post-reboot pass

Rejected candidate: `0.2.56` / build `58`.

```text
CFBundleShortVersionString = 0.2.56
CFBundleVersion = 58
sha256(OpenA8DJHAL) = 47161e05476062b2cefb8071528b6c57c555c719362e9001a027a4e88f50cd8f
sample-rate/config prepared for user test = 48000 Hz / 512 frames
default output = Open Audio 8 DJ
```

What changed from the failed late `0.2.50`-`0.2.54` family:

- Kept physical output packing conservative: 24-bit big-endian,
  `OPENA8DJ_OUTPUT_NATIVE_I24=0`, playback byte start `4`, and output gain
  `0.50`.
- Kept output-only HAL shape: no input streams, four stereo output streams.
- Enabled Core Audio `IOProcStreamUsage` support so the HAL can flush an output
  cycle once the streams Core Audio says are active have arrived, instead of
  waiting blindly for all four output streams.
- Flushes any touched output cycle during `StopIO` before closing the USB
  engine.
- Uses 64 ISO frames per transfer with capture/playback queue targets `8/8`.
  The immediately previous `0.2.55` used ISO `8` and passed audio counters, but
  driver CPU reached roughly 20% during local playback, so it was not handed to
  the user.

Autonomous gates passed before this user test:

```text
make smoke-hal:
  streams=4 buffer=512 bufferBytes=16384 bufferRange=512-4096

validate-mode2-output-packing:
  big_i24_output conversion vectors pass
  start_byte=0..5 pass, check_errors=0, panic_flags=0

Core Audio enumeration:
  Open Audio 8 DJ, in=0, out=8, rate=48000
  output buffers = 4 x 2 channels
  output streams = 4, starts = 1,3,5,7

production 48000 / 512, 8 seconds:
  generator_peak=0.250000
  driver_peak=0.125000
  active-underruns=0
  playback.failed=0
  playback.qfail=0
  elastic-drops=0
  elastic-replays=0
  timeline-resets=0
  mode2.output-panic-flags=0
  driver CPU max=6.2%

production 44100 / 512, 8 seconds:
  generator_peak=0.250000
  driver_peak=0.125000
  active-underruns=0
  playback.failed=0
  playback.qfail=0
  elastic-drops=0
  elastic-replays=0
  timeline-resets=0
  mode2.output-panic-flags=0
  driver CPU max=7.6%

production 48000 / 512, 30 seconds:
  generator_peak=0.250000
  driver_peak=0.125000
  active-underruns=0
  playback.failed=0
  playback.qfail=0
  elastic-drops=0
  elastic-replays=0
  timeline-resets=0
  mode2.output-panic-flags=0
  coreaudiod CPU max=10.6%, avg=1.4%
  driver CPU max=7.1%, avg=5.4%
  post-test idle coreaudiod=0.0%, driver=0.0%
```

Known limitation: this still does not prove analog sound quality. Human
listening remains decisive, because earlier builds passed similar transport
counters while sounding metallic or noisy.

## Current installed build - 2026-06-11 late pass

Update after human listening:

- `0.2.50` is invalidated. The user reported Spotify still sounded very
  metallic, like a high-pass/noisy metallic filter.
- Follow-up internal candidates were attempted and rejected before user test:
  - `0.2.51`: 1x8 topology plus old `0.2.30/0.2.31` transport flags
    (`ISO=8`, queues `64/64`, input decode on, USB clock anchor on). Diagnostic
    A/B signal path passed, but production/configuration drove `coreaudiod`
    above 100% and hung `audio-config`.
  - `0.2.52`: 1x8 topology, `ISO=8`, queues `64/64`, input decode on, USB
    clock anchor off. It still drove `coreaudiod` hot at idle.
  - `0.2.53`: returned to 4x2 topology, `ISO=8`, queues `8/8`, input decode
    off, USB clock anchor off. It idled, but WAV playback did not complete and
    the HAL bridge never became available.
  - `0.2.54`: 4x2 topology, `ISO=8`, queues `64/64`, input decode off, USB
    clock anchor off. It drove `coreaudiod` hot at idle.
- The active HAL was moved out of `/Library/Audio/Plug-Ins/HAL` after these
  failed passes to avoid leaving a broken driver loaded.
- At the end of this pass, Core Audio enumerated only the built-in MacBook
  microphone and speakers. If `coreaudiod` remains hot even with OpenA8DJ
  disabled, that is a separate macOS audio-service state issue caused during
  repeated driver load/unload testing; rebooting macOS is the clean reset before
  any further Audio 8 driver work.

No current active listening candidate. The last installed listening candidate
from this section was `0.2.50` / build `52`, now invalidated.

```text
CFBundleShortVersionString = 0.2.50
CFBundleVersion = 52
sha256(OpenA8DJHAL) = 4b83922d2e718f3a4adbeca0d49859aabf10ec4ad4a29fbbfb9a3cc21aa3f63c
sample-rate/config prepared for user test = 48000 Hz / 512 frames
default output = Open Audio 8 DJ
```

Why this candidate exists:

- `0.2.48` is invalidated by human listening. The user reported recognizable
  music, but with a strong metallic / high-pass / noisy-filter character.
- The likely app-facing mistake in `0.2.48` was exposing output as one 8-channel
  stream. `0.2.50` exposes four stereo output streams: A, B, C, and D.
- Each stereo stream is copied into the corresponding pair in the internal
  8-channel USB buffer.
- Physical output remains big-endian 24-bit with `OPENA8DJ_OUTPUT_GAIN=0.50f`,
  `OPENA8DJ_OUTPUT_NATIVE_I24=0`, input decode off, diagnostic capture off,
  USB clock anchor off, 64 ISO frames, capture queue 8, playback queue 8.

Important rejected sub-experiment:

- A proposed change to flush a cycle when any output stream had been touched was
  tested in diagnostic `0.2.50`.
- It failed: CoreAudio calls `EndIOOperation` per stream, so flushing on the
  first touched stream corrupted the cycle. Pair A written/consumed/USB
  correlation collapsed.
- The installed production build keeps the safe rule: flush only when all four
  output streams for the cycle have been seen.

Autonomous gates before handing this build to the user:

```text
topology:
  output buffers = 4 x 2 channels
  output streams = 4
  starts = 1, 3, 5, 7
  preferred stereo = 1/2

diagnostic pair A:
  written_alignment_score = 1.000000
  consumed_alignment_score = 1.000000
  written_consumed_alignment_score = 1.000000
  written_consumed_lag_jumps_gt_2_frames = 0
  usb_check_offset = 8
  usb_start_byte = 4
  usb_check_errors = 0
  usb_panic_flags = 0
  usb_alignment_score = 1.000000
  usb_left_gain = 0.500000
  usb_right_gain = 0.500000

diagnostic pair B:
  written_alignment_score = 1.000000
  consumed_alignment_score = 1.000000
  written_consumed_alignment_score = 1.000000
  written_consumed_lag_jumps_gt_2_frames = 0
  usb_check_offset = 8
  usb_start_byte = 4
  usb_check_errors = 0
  usb_panic_flags = 0
  usb_alignment_score = 1.000000
  usb_left_gain = 0.500000
  usb_right_gain = 0.500000

production 48000 / 512:
  generator_peak = 0.250000
  driver_peak = 0.125000
  active-underruns = 0
  playback.failed = 0
  elastic-drops = 0
  elastic-replays = 0
  timeline-resets = 0
  mode2.output-panic-flags = 0

production 44100 / 512:
  generator_peak = 0.250000
  driver_peak = 0.125000
  active-underruns = 0
  playback.failed = 0
  elastic-drops = 0
  elastic-replays = 0
  timeline-resets = 0
  mode2.output-panic-flags = 0

CPU:
  first second after stream start can spike briefly
  sustained coreaudiod during internal 48/512 playback settled around 1.4-1.8%
  post-test idle coreaudiod = 0.0%
```

Known limitation:

- This is still output-only. Core Audio currently reports `input buffers: 0`;
  the microphone/input path remains intentionally disabled for this listening
  pass and must not be judged as fixed.
- Human listening remains decisive. The autonomous gates catch silent output,
  wrong stream topology, bad written/consumed correlation, and bad USB packing,
  but they cannot prove analog sound quality without physical loopback.

## Superseded installed build - 2026-06-11

Superseded candidate: `0.2.48` / build `50`.

```text
CFBundleShortVersionString = 0.2.48
CFBundleVersion = 50
sha256(OpenA8DJHAL) = 1e0f00479564eaebf562fc45596bbac8626d255679daea635ede5ab3ea1b836b
sample-rate/config prepared for user test = 48000 Hz / 512 frames
```

Why this candidate is different from the failed `0.2.47` listening build:

- It keeps the lower-CPU USB batching/scheduling changes from `0.2.47`.
- It reverts physical playback packing to big-endian 24-bit, matching the old
  kext's advertised IOAudio stream format: signed 24-bit PCM, high-byte aligned,
  big-endian.
- It keeps mode-2 playback byte position at `4`, not the legacy input cursor
  value `2`.

Autonomous gates before handing this build to the user:

```text
diagnostic capture:
  written -> consumed correlation = 1.000000
  packed USB decode: check_offset=8, start_byte=4
  usb_check_errors=0
  usb_panic_flags=0
  usb_left_gain=0.500000
  usb_right_gain=0.500000

production 48000 / 512:
  driver_cpu_max=6.2%, avg=5.4%
  output-level peak=0.040000 for generator outputPeak=0.080000
  active-underruns=0, elastic-drops=0, elastic-replays=0
  timeline-resets=0, playback.failed=0, playback.qfail=0
  mode2.input-check-errors=0, mode2.output-panic-flags=0

production 44100 / 512:
  driver_cpu_max=6.3%, avg=5.5%
  output-level peak=0.040000 for generator outputPeak=0.080000
  active-underruns=0, elastic-drops=0, elastic-replays=0
  timeline-resets=0, playback.failed=0, playback.qfail=0
  mode2.input-check-errors=0, mode2.output-panic-flags=0
```

This still requires human listening. The autonomous gates now catch the previous
silent-output and total-white-noise failures, but they cannot prove analog
sound quality without a physical loopback or listening test.

Previous update after human listening: `0.2.47` is **not** a valid listening build.
Despite clean autonomous counters, the user reported total white noise: no
recognizable music, nothing intelligible. The installed bundle was immediately
moved out of the active HAL directory as:

```text
/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.white-noise-0247-20260611-202206
```

The likely cause is the native-24-bit output byte order introduced from the old
binary interpretation. Because `0.2.38` never had a good human listening pass
and later `WriteMix` finally made that path audible, the native order must be
treated as disproven for the physical output path until proven otherwise.

`0.2.48` was initially built to keep the 0.2.47 CPU/scheduling changes while
reverting physical output packing to big-endian. The first install guard
observed `coreaudiod` startup load and disabled it too early; a later retry used
a longer cold-load guard and the production build above passed.

Earlier safe active rollback:

```text
CFBundleShortVersionString = 0.2.32
CFBundleVersion = 34
sha256(OpenA8DJHAL) = bc8ebd741ea33ac70e580bb6b30955ffe67ed2226853933d3bc951edb0a46434
default output = MacBook Air Speakers
post-restore idle CPU: coreaudiod=0.0%, driver=0.0%
```

Do not send another listening candidate based only on internal counters. The
next useful candidate must preserve big-endian physical output packing or prove
the physical byte order another way before asking for human playback.

## Superseded installed build notes

`0.2.47` / build `49` was the first build in this run that passed autonomous
gates strongly enough to hand to the user for listening, but the human result
invalidated it.

Installed identity:

```text
CFBundleShortVersionString = 0.2.47
CFBundleVersion = 49
sha256(OpenA8DJHAL) = 95ddfdb659e48cb07e8fea9b7dd36c04c3ff219b8e9779c80b7b7512240de208
```

What changed from the failed/silent attempts:

- Uses `kAudioServerPlugInIOOperationWriteMix` for output instead of
  `ProcessOutput`; this fixed the silent-output regression.
- Keeps output as one 8-channel stream and keeps public buffer size at 512.
- Packs USB output as native 24-bit int bytes (`value >> 8`, `>> 16`,
  `>> 24`), matching the old x86 kext's output converter.
- Builds with output gain `0.50`, diagnostic capture off, Audio 8 input decode
  off, and USB clock-anchor sampling off.
- Uses 64 USB microframes per transfer with capture/playback queue targets of
  8, plus 64-frame output prefetching.

Why USB clock-anchor sampling is off: profiling `0.2.46` while audio was active
showed the remaining driver CPU was dominated by repeated
`IOUSBHostObject frameNumberWithTime:` calls inside
`updateClockAnchorWithUSBTime`. That anchor was not feeding the HAL
`GetZeroTimeStamp` path, so `0.2.47` disables it for playback. In this build
`stream-stats` reporting `clock-anchor: fallback` is expected, not a failure.

Autonomous validation that passed:

```text
make package
make smoke-hal
scripts/validate-mode2-output-packing.py --start-byte all --frames 128 --gain 0.5
scripts/verify-active-output-path.sh 48000 512 0.25 6
```

Final 60 second internal playback gate on the installed HAL:

```text
I/O OK: rate=48000 callbacks=5626 outputFrames=2880512 outputSamples=23044096 outputPeak=0.25000000
driver CPU: max=8.0%, avg=7.0%
coreaudiod CPU: max=1.3%, avg=0.9%
output-level peak=0.125000
output.active-underruns=0
playback.failed=0
playback.qfail=0
elastic-drops=0
elastic-replays=0
timeline-resets=0
mode2.input-check-errors=0
mode2.output-panic-flags=0
post-test idle CPU: coreaudiod=0.0%, driver=0.0%
```

This does not prove subjective audio quality. Human listening remains the
release gate, especially Spotify window switching and Traktor kick/bass
material. But this is no longer a silent build, no longer a 20-30% CPU build,
and it passed signal-path, stream-health, and idle-load checks before handoff.

## Current human result

The `0.2.30` diagnostic build produced the first substantially improved
open-source-driver playback result in the user's Spotify test. Test track:
`Hit or Miss` by Odeta. Pass: Spotify-only, no microphone activation yet.
Listening result: still a little remote crackling, described as similar to old
vinyl noise, with occasional "guac" at entry and one at the end, but
substantially better than prior open-source-driver builds.

Frozen diagnostic artifacts for this pass:

```text
local-analysis/2026-06-10-spotify-hit-or-miss-odeta-0.2.30-pass1/
```

Captured counters after the pass: `output.active-underruns=0`,
`elastic-drops=0`, `elastic-replays=0`, `timeline-resets=0`,
`playback.failed=0`, `playback.qfail=0`, `mode2.input-check-errors=0`, and
`mode2.output-panic-flags=0`. This is a useful candidate baseline, but not yet
a final quality pass because the user still heard minor crackling and Traktor
validation is pending.

Follow-up Traktor test on `0.2.30`: Traktor selected `Audio 8 DJ`, sample rate
`48000`, buffer latency `512`; Deck 1 and Deck 2 were tested. Human result:
crackling became clearly audible when the signal got loud, especially when bass
came in; overall sound was still dirty on strong electronic material. A "guac"
artifact happened when stopping Deck 1. The user judged it worse than the
Spotify pass, while noting the track was not the same.

Frozen Traktor artifacts:

```text
local-analysis/2026-06-10-traktor-deck1-deck2-0.2.30/
```

The active live Traktor stats before stop had clean transport counters:
`output.active-underruns=0`, `elastic-drops=0`, `elastic-replays=0`,
`timeline-resets=0`, `playback.failed=0`, `playback.qfail=0`,
`mode2.input-check-errors=0`, and `mode2.output-panic-flags=0`. However, the
post-close `.raw` capture is silent and appears to be a short final empty stream
from Traktor shutdown, so use the live stats plus human notes as the preserved
evidence for this pass.

`0.2.31` tested the amplitude hypothesis by adding `OPENA8DJ_OUTPUT_GAIN=0.50f`
and output-level counters. Human Spotify result improved again: same song,
same volume, less crackling, but still some background crackle. Live
`opena8dj-control stream-stats` while playing showed `output-level peak` around
`0.507`, `near-clip=0`, and `clipped=0`; transport counters remained clean.
This confirms headroom helped and hard clipping is not the remaining active
failure in that pass.

The user also reported crackle/noise when changing windows/apps, as if UI/CPU
or memory activity leaked into playback. During that measurement, the driver
process was roughly 27-34% CPU and WindowServer was around 39-41%, while
`coreaudiod` itself was low after settling. That makes the next suspect
real-time load/scheduling overhead rather than USB transport loss.

`0.2.32` is the current installed listening build after that observation. It
keeps the `0.50` output headroom but builds with
`OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE=0` to remove raw/TSV capture overhead from
the playback path. Installed identity:

```text
CFBundleShortVersionString = 0.2.32
CFBundleVersion = 34
sha256 = bc8ebd741ea33ac70e580bb6b30955ffe67ed2226853933d3bc951edb0a46434
```

After installation and settling, `coreaudiod` and `Core Audio Driver
(OpenA8DJ.driver)` returned to `0.0%` CPU with no active stream. The next test
should repeat Spotify window-switching and then Traktor with this listening
build.

Follow-up listening on `0.2.32` found only a minimal difference. At `44100` /
`512` with Spotify actively playing, stream stats still showed no clipping
(`peak=0.5`, `near-clip=0`, `clipped=0`), no active underruns, no elastic
replays/drops, no timeline resets, and no playback failures. The driver process
was still around 29% CPU with Spotify running. `0.2.33` therefore keeps the same
audio format and gain but removes per-output-frame stream-stat locking from the
USB fill path, accumulating output counters per transfer instead.

With Spotify running at `44100` / `512`, `0.2.33` reduced the driver process
from roughly 29% CPU to roughly 19% CPU while keeping clean transport counters.
An attempted follow-up disabled full input audio decode/level stats in the
output-only HAL build. It compiled and the internal driver bridge still
responded, but after installation Core Audio public enumeration/configuration
tools began hanging and `coreaudiod` stayed hot while no stream was active.
That experiment was reverted. `0.2.36` is the safer listening build: it keeps
the `0.2.33` output-stat batching optimization, keeps gain at `0.50`, keeps
diagnostic capture disabled, and leaves input decode enabled.

Human listening result for `0.2.36`: failed. Do not treat this as a candidate
build. The user reported the same bad result, worse than some earlier versions:
Spotify still has poor sound/crackle, Traktor still distorts heavily when kick
drums/bass raise the signal, window switching still causes crackle, and driver
CPU is still far too high for simple Spotify playback. The installed HAL was
moved out of the active HAL directory and preserved as:

```text
/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.bad-listening-20260611-094257
```

Conclusion: stop asking for incremental listening tests on this branch. The
remaining failure is not solved by output gain, disabling diagnostic capture, or
batching output stats. The next implementation needs a larger correction based
on the legacy driver architecture: likely USB output slot scheduling/copying,
mode-2 output byte phase across transfers, stream topology, or Core Audio
property/timing behavior.

`0.2.37` is an offline-only candidate until internally verified. It restores the
HAL output topology to four stereo output streams A/B/C/D instead of exposing a
single 8-channel output stream. The USB engine still receives one internal
8-channel buffer, but Core Audio and DJ applications should see the traditional
four stereo pairs again. Do not install for human listening without first
checking enumeration, CPU stability, and that output stream formats are
`4 x 2ch`, not `1 x 8ch`.

`0.2.38` is the active listening candidate installed on 2026-06-11. It keeps the
`0.2.37` four-stereo-output HAL topology and changes playback sample conversion
to match the old NI binary's output path: Float32 is quantized to native 24-bit
and emitted as `value >> 8`, `value >> 16`, `value >> 24` instead of the previous
big-endian `FloatToS24BE` order. This is specifically aimed at the user's report
that Traktor distorts heavily on kicks/bass, because a byte-order error would
turn low-order sample bytes into high-significance amplitude/sign bytes. Input
decode was deliberately left unchanged.

Internal checks before handing `0.2.38` to the user:

- Installed HAL binary hash matches `build/OpenA8DJ.driver`.
- `make smoke-hal`: `streams=4`, `buffer=512`, `bufferBytes=16384`.
- `validate-mode2-output-packing.py --start-byte all --frames 128 --gain 0.5`:
  conversion vectors pass and all start-byte cases pass.
- Core Audio enumeration: `Open Audio 8 DJ`, `in=0`, `out=8`, `rate=48000`,
  output buffers `4 x 2ch`.
- `audio-config org.opena8dj.Audio8DJ 48000 512`: applied successfully.
- `audio-io-test`: `I/O OK`, 282 callbacks, no input frames expected in this
  output-only HAL candidate.
- Idle `coreaudiod`: 0%.

This build should be judged first on whether strong-signal Traktor/Spotify
distortion changes. It is not expected to fully prove or disprove the separate
window-switch crackle / CPU scheduling problem; if tone improves but UI-pressure
crackle remains, the next front is USB output scheduling and preallocated
playback queue behavior.

Post-test correction on 2026-06-11: do not use `0.2.38` or `0.2.39` for human
listening. Both could enumerate in Core Audio, but an autonomous signal-path test
proved that they were silent at the HAL-to-USB boundary: `audio-io-test` wrote
real output samples (`outputPeak=0.25`) while `opena8dj-control stream-stats`
still reported `output-level peak=0.000000`.

Root cause: the HAL was advertising and handling
`kAudioServerPlugInIOOperationProcessOutput`, but Apple's `AudioServerPlugIn.h`
states that output devices must implement `kAudioServerPlugInIOOperationWriteMix`
to put the final mix into the device ring buffer. `ProcessOutput` is only an
arbitrary processing phase. The independent subagent audit agreed with this
contract.

`0.2.40` changed output handling to `WriteMix` and restored the objective signal
path:

```text
audio-io-test outputPeak=0.25000000
stream-stats output-level peak=0.125000
```

The `0.125` driver peak is expected because the build uses
`HAL_OUTPUT_GAIN=0.50f`. This verifies Core Audio client output -> HAL -> USB
engine signal flow; it does not prove physical speaker output or sound quality.

However, `0.2.40` and the follow-up `0.2.41` must not be left active yet. After
subsequent reloads, `coreaudiod` repeatedly crossed the CPU guard threshold and
the active HAL was moved back out of `/Library/Audio/Plug-Ins/HAL`. Current safe
state after this pass: no `OpenA8DJ.driver` is active in the HAL directory, and
`coreaudiod` has been restarted back to idle. Do not ask for another human
listening test until a build passes both:

- CPU guard during clean install/reload.
- Autonomous signal-path verification: generator output peak > 0 and driver
  `output-level peak` > 0 after that same test.

For physical autonomous verification, internal stream stats are not enough. The
next robust option is a hardware loopback test: patch Audio 8 DJ Output A L/R
into Audio 8 DJ Input A L/R, play a tone through output, capture from the USB
input path, and analyze the returned WAV for tone level, SNR, and click
outliers. The MacBook microphone can be used only as a weak fallback because it
depends on speaker placement and room acoustics.

The latest installed build, `0.2.29` build `31`, must not be treated as a
quality improvement.

Human listening result after installation:

- Base sound quality did not improve.
- It may be worse than the previous build, with more glitches.
- Activating/touching the external microphone caused the music to stop
  completely.

The internal counters from the automated tests were misleading in this case.
They showed clean transport stats, but the real listening test contradicted
them. Do not rely on `active-underruns=0` alone as proof that playback quality
is good.

## Human diagnostic test for the next build

The next diagnostic build must be judged by a repeatable human listening pass
plus the internal capture files. Treat stream counters as supporting evidence,
not the verdict.

Before the listening pass:

1. Record the installed build version, build number, loaded UUID, and binary
   hash supplied by the implementation owner.
2. Confirm Core Audio still sees the intended shape for this playback-focused
   test: `Open Audio 8 DJ`, `in=0 out=8`, `rate=48000`, `buffer=512`, and
   `buffer-bytes=16384`.
3. Use the same physical output pair, mixer channel, cable path, headphone or
   speaker path, Spotify volume, and macOS output volume for every pass. Start
   low.
4. Decide which microphone path is being compared. If the reported failure used
   an external microphone, record its exact name and use that same device. If a
   laptop-microphone comparison is needed, run it as a separate pass and label
   the artifacts separately.
5. Move aside old `/tmp/opena8dj-*` artifacts before each pass so the new files
   cannot be confused with a previous stream.

Run two Spotify passes with the same track and the same timestamp:

1. `spotify-only`: play the selected segment through `Open Audio 8 DJ` with no
   microphone or voice capture active. Listen for crackle, pitch/speed changes,
   dropouts, and whether either channel stops.
2. `spotify-mic`: restart the same Spotify segment from the same timestamp,
   then activate the selected microphone path while Spotify is already playing.
   Note the exact second when the microphone is activated and whether playback
   changes immediately, degrades slowly, or stops.

For both passes, capture `opena8dj-control stream-stats` before playback,
during steady playback, immediately after microphone activation when applicable,
and after stopping playback. Zero values for `output.active-underruns`,
`elastic-drops`, `elastic-replays`, `timeline-resets`, `playback.failed`,
`playback.qfail`, `mode2.input-check-errors`, or `mode2.output-panic-flags` are
good signs, but they do not override the listening result. If the music crackles
or stops with clean counters, the build still fails the human test.

After each pass, stop every app using `Open Audio 8 DJ` long enough for the
diagnostic capture to flush, then copy these files before starting the next
pass:

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
payload after float-to-USB conversion, and the events file records write,
consume-anomaly, and packed-transfer points with an explicit `timeline` column
so sample-time, served-frame, and USB-frame values are not mixed accidentally.
The capture is bounded, so a long Spotify run may only keep the first capture
window; make the microphone transition happen early in the pass.

Also collect these when present:

```text
/tmp/opena8dj-usb.log
/tmp/opena8dj-hal-trace.log
/tmp/opena8dj-midid.log
```

Do not collect `/tmp/opena8dj-control.sock`; it is the live control socket, not
a diagnostic artifact.

For a deterministic non-Spotify control, play a known PCM16 WAV through the
diagnostic build and compare the WAV against both internal captures:

```sh
./build/audio-wav-play path/to/reference.wav A
python3 scripts/analyze-driver-capture.py path/to/reference.wav --capture-dir /tmp --usb-raw /tmp/opena8dj-output-packed-usb.raw --events /tmp/opena8dj-output-events.tsv --pair A
```

If `written` aligns cleanly with the source but `consumed` shows poor SNR,
click outliers, frame-count surprises, or obvious audible damage, the failure is
after Core Audio handed audio to the HAL. If both captures look clean while
Spotify still sounds bad, preserve the files and the exact human notes; that is
still a real bug.

## Installed state at handoff

Installed HAL bundle:

```text
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
```

Installed version:

```text
CFBundleShortVersionString = 0.2.29
CFBundleVersion = 31
```

Installed binary hash that was verified immediately after install:

```text
13c46a4269877bc4f9509faa7f6a350281bf654b5f9a1d5cd436e2b8ba272b76
```

Loaded UUID that was verified immediately after install:

```text
EDD14EFD-FB45-376A-AA50-59999522A4DE
```

Core Audio enumeration after install showed:

```text
Open Audio 8 DJ uid=org.opena8dj.Audio8DJ in=0 out=8 rate=48000
timing: buffer=512 buffer-bytes=16384
```

This corrected the public `buffer=192` symptom, but did not improve real audio
quality. It may have made microphone interaction worse.

## Last code/documentation changes

Last driver-facing source change:

- `src/hal/OpenA8DJHAL.c`
  - `kAudioDevicePropertyZeroTimeStampPeriod` now reports `16384` instead of
    `gBufferFrames`.
  - `GetZeroTimeStamp()` advances on that same `16384` frame period.

Last tool change:

- `src/tools/audio-io-test.c`
  - Added an optional amplitude argument so tests can run silent or very quiet.

Version files:

- `Makefile`: `VERSION := 0.2.29`
- `resources/OpenA8DJ.driver/Contents/Info.plist`: `0.2.29` / build `31`

Documentation touched:

- `docs/OLD_DRIVER_COMPAT_PLAN.md`
- `docs/TESTING.md`

## Useful old macOS driver artifacts

The old working driver binary is here:

```text
old driver/useful-binaries/kext/NIUSBAudioDriver.kext/Contents/MacOS/NIUSBAudioDriver
```

Important symbols observed in the old kext:

```text
de_caiaq_driver_NIUSBAudioEngine::performAudioEngineStart()
de_caiaq_driver_NIUSBAudioEngine::queueInIrp()
de_caiaq_driver_NIUSBAudioEngine::queueOutIrp()
de_caiaq_driver_NIUSBAudioEngine::readHandler()
de_caiaq_driver_NIUSBAudioEngine::writeHandler()
de_caiaq_driver_NIUSBAudioEngine::clipOutputSamples()
de_caiaq_driver_NIUSBAudioEngine::convertInputSamples()
de_caiaq_driver_NIUSBAudioEngine::takeSimpleTimeStamp()
de_caiaq_driver_NIUSBAudioEngine::jitterFilter()
de_caiaq_driver_NIUSBAudioEngine::fillErrorCorrectionPatternMode2()
```

Useful facts already extracted from that binary:

- It is an x86_64 IOAudioFamily/IOUSBFamily kext.
- It uses a `0x200` / 512 frame visible buffer default.
- It queues 64 input isochronous IRPs and keeps up to 128 output slots.
- `readHandler()` picks a free output slot after a successful input completion,
  copies the input transaction byte counts into that output request, and then
  queues output.
- It has an explicit reset path with the log string:

```text
Unable to find USB output slot. Resetting.
```

- It initializes/pattern-fills the output USB byte ring before real audio.
- Its timing model is based on USB frame timestamps plus a jitter filter, not
  only on host-side Core Audio callbacks.

## Things not to repeat

- Do not treat `0.2.29` as successful because automated counters were clean.
- Do not assume the public 512 frame buffer fix solves audio quality.
- Do not continue experimenting from Linux as the primary reference; the user
  specifically wants the old macOS driver behavior analyzed and ported.
- Do not reinstall another build without making it clear what changed and why.
- Do not retry the earlier forced USB-clock `GetZeroTimeStamp` experiment; it
  previously destabilized Core Audio.

## Recommended next step

First, make the state safe and comparable:

1. Confirm what is currently installed and loaded.
2. Compare `0.2.28` versus `0.2.29` specifically, because `0.2.29` is the first
   build where activating/touching the external microphone reportedly stopped
   music completely.
3. If the user wants a quick safety rollback, rebuild/install the last less-bad
   build rather than continuing forward.

Then investigate from the old macOS driver:

1. Focus on `performAudioEngineStart`, `readHandler`, `queueOutIrp`,
   `clipOutputSamples`, `takeSimpleTimeStamp`, and `jitterFilter`.
2. Reconstruct how the old kext maps Core Audio sample positions into the USB
   output ring.
3. Compare that to the current HAL timeline write/read behavior.
4. Use the diagnostic capture files to compare Core Audio written frames against
   USB-consumed frames under real Spotify and microphone activation, because
   previous synthetic counters did not reproduce the audible failure.

## Last known user expectation

The user explicitly asked to stop changing the driver and hand off to another
agent. Any next implementation should start with a fresh plan and should not
make more driver changes without user confirmation.
