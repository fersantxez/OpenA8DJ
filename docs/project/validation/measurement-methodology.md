# Automated soundcheck design

This document describes a repeatable sound-quality test that does not depend on
Spotify or manual app playback. It uses local audio files as realistic program
material, controlled synthetic signals for diagnosis, and a capture-and-compare
pipeline that can tell us whether the OpenA8DJ output path preserved the source.

## Current implementation

The first implementation is in the repo:

- `scripts/prepare-music-fixture.py`
  - finds a local music file or uses `--music-file`
  - converts MP3/WAV/AIFF/M4A/AAC/FLAC to a safe stereo PCM16 reference WAV
  - selects a `dense`, `transient`, `wideband`, or `start` excerpt
  - writes `reference.wav` and `source.json`
- `src/tools/audio-record.c`
  - records any Core Audio input device by UID, exact name, or name substring
  - records a selected 1-based stereo channel pair to PCM16 WAV
- `scripts/analyze-soundcheck-capture.py`
  - compares `reference.wav` with `captured.wav`
  - reports alignment, SNR, residual, clicks, lag drift, and clipping
  - reports residual energy in the 1-5 kHz band that matches the reported
    "radio/vinyl" noise
  - can correlate windowed residual noise against a synchronized CPU profile
- `scripts/run-soundcheck`
  - prepares the fixture
  - configures OpenA8DJ rate/buffer
  - plays the reference through the selected output pair
  - records the configured capture input
  - samples Core Audio, driver, UI, player, recorder, Spotify, and Traktor CPU
  - can add an opt-in CPU stress phase while capture is running
  - writes `summary.txt`, `metrics.json`, logs, and captured WAVs under
    `<evidence-dir>/soundcheck/<run-id>/`

Make targets:

```sh
make soundcheck-preflight
make soundcheck SOUNDCHECK_CAPTURE="External Recorder"
make simulated-output-soundcheck
```

`soundcheck-preflight` intentionally does not require a loopback cable. It
proves that real local music can be found, decoded, normalized, and prepared
before a driver idea is compiled or installed. It defaults to a short excerpt
through `SOUNDCHECK_PREFLIGHT_SECONDS=5` and `SOUNDCHECK_PREFLIGHT_MODE=start`
so it can be used often. When `ffmpeg` is available, that path converts only the
needed excerpt. `soundcheck` is the full analog-loopback gate and defaults to
`SOUNDCHECK_SECONDS=20` with `SOUNDCHECK_MODE=dense`.

`simulated-output-soundcheck` is the no-sound gate. Use it when the physical
outputs are muted, there is no real acoustic path, or no separate recording
interface is available. It takes real music, simulates the Audio 8 DJ mode-2 USB
output packing in software, decodes that simulated USB output back to audio,
and runs the same 1-5 kHz residual analysis. It does not use speakers,
headphones, or a microphone.

For audio-path changes, use this workflow:

1. Run `make soundcheck-preflight` before implementing or rebuilding, so the
   test material is known-good and repeatable.
2. If a capture input is connected, run `make soundcheck ...` against the
   currently installed driver to capture the baseline.
3. Implement the driver/tool change.
4. Run the normal compile/smoke checks.
5. Run `make simulated-output-soundcheck` when no physical output/capture path
   is available.
6. Run `make soundcheck ...` only when a real loopback/capture path exists.

## Goals

- Use real local music, not only tones.
- Keep the test local: no upload, streaming service, or network dependency.
- Never commit or redistribute user music.
- Produce a machine-readable pass/fail report plus enough metrics to explain
  what failed.
- Separate driver-internal correctness from real analog output quality.

## Source material

The harness should accept either an explicit file or a folder to scan:

```sh
scripts/run-soundcheck --music-file "/path/to/song.mp3"
scripts/run-soundcheck --music-dir "$HOME/Music"
```

Supported inputs should include at least:

- WAV
- AIFF
- MP3
- AAC / M4A
- FLAC if `ffmpeg` is available

On macOS, the first conversion backend should be `afconvert`, because it is
available with the system audio stack and handles common user-library formats.
If `ffmpeg` is installed, the harness can use it as a fallback for formats that
AudioToolbox does not decode.

The original file is read-only. The harness writes temporary normalized WAV
fixtures under `<evidence-dir>/soundcheck/<run-id>/`, which is already ignored
by git.

## Music selection

The test should use two kinds of real-music excerpts:

1. User-selected track
   - Best for checking the exact music the user hears as bad.
   - The command accepts `--music-file`.

2. Auto-selected clips from a folder
   - Best for regression testing without manual choice.
   - The scanner looks for playable files under common locations such as
     `$HOME/Music`, `$HOME/Downloads`, and user-provided directories.
   - It skips files shorter than the requested excerpt length.

For each chosen track, the harness should prepare fixed-length excerpts:

- `dense`: loudest RMS window, useful for metallic distortion and crackle.
- `transient`: highest short-term peak-to-RMS window, useful for clicks and
  sample jumps.
- `wideband`: highest high-frequency energy window, useful for harshness and
  alias-like artifacts.

The selected offsets are written to metadata so repeated runs can use the same
source windows.

## Fixture preparation

Each selected excerpt becomes a deterministic reference WAV:

```text
reference.wav
  PCM16
  stereo
  sample rate = requested test rate, usually 48000 or 44100
  peak normalized to a safe target, for example -12 dBFS
  short fade in/out to avoid artificial edge clicks
```

Normalization is important because user music libraries have wildly different
levels. The harness should preserve relative musical dynamics inside the
excerpt, but avoid clipping the device or making quiet tracks look like failure.

## Capture layers

The test should capture at two layers.

### 1. Driver-internal capture

This proves what the HAL received and what it handed to the USB output path.
It is useful for isolating bugs before the DAC:

- app/Core Audio input to the HAL
- stream-cycle material consumed by the output engine
- decoded USB payload if diagnostic capture is enabled

The existing `scripts/analyze-driver-capture.py` already compares a reference
WAV against raw driver captures. The soundcheck harness should call the same
analysis path.

### 1b. Software simulated output

This is the correct gate when no real sound should be produced. The script
`scripts/run-simulated-output-soundcheck`:

- prepares real local music exactly like the normal soundcheck
- places the selected stereo pair into an 8-channel output frame
- applies the configured output gain
- simulates mode-2 USB packing with the selected byte order and start byte
- decodes the simulated USB bytes back into stereo audio
- compares the decoded output with the reference using alignment, SNR,
  residual, clicks, and the 1-5 kHz noise gate

It writes:

```text
<evidence-dir>/simulated-output/<run-id>/
  fixture/reference.wav
  simulated-output-packed-usb.raw
  simulated-output-decoded.wav
  cpu-profile.tsv
  coupling-profile.json
  usb-raw-analysis.txt
  metrics.json
  summary.txt
```

This gate catches output conversion, byte-order, gain, and mode-2 packing
regressions without depending on speakers or microphones. It cannot prove the
analog DAC or the user's physical listening chain.

### 2. Analog loopback capture

This is the replacement for "does Spotify sound bad?"

The output pair under test is physically routed to a recording input, then the
harness records `captured.wav` while playing `reference.wav`.

Recommended setup:

- Best isolation: Audio 8 DJ output to a separate recording interface input.
- Acceptable loopback: Audio 8 DJ output to Audio 8 DJ input, if input decode is
  stable enough for the test.
- Fallback smoke test: MacBook microphone recording, useful only as a coarse
  audible check because room/speaker/mic coloration dominates the measurement.

The analog capture compares `reference.wav` and `captured.wav` after alignment,
gain matching, and optional channel selection.

## Analysis metrics

Each comparison should report:

- alignment lag in frames
- alignment confidence
- compared duration
- gain per channel
- signal RMS
- residual RMS
- SNR
- peak error
- click/outlier count
- lag drift across windows
- lag jumps greater than two frames
- high-band residual ratio
- 1-5 kHz residual ratio and dBFS level
- 1-5 kHz residual by time window
- CPU correlation for 1-5 kHz residual, with up to one second of lag
- clipped-sample count
- channel balance and polarity

The important audio-quality failures we care about should map to metrics:

- Crackle/clicks: high residual spikes and click outliers.
- Metallic/high-pass character: poor low-frequency match plus elevated
  high-band residual.
- Radio/vinyl background noise: elevated residual in the 1-5 kHz band.
- CPU/window-coupled noise: 1-5 kHz residual rises in the same windows as
  `coreaudiod`, the OpenA8DJ driver process, WindowServer, Control Center, or
  other audio/UI services.
- Dropped/inserted samples: moving lag, lag jumps, or low local correlation.
- Distortion/clipping: nonlinear residual and clipped sample count.
- Wrong routing: energy appears on the wrong pair or polarity is inverted.

## Proposed command

```sh
scripts/run-soundcheck \
  --music-file "$HOME/Music/Test Track.mp3" \
  --pair A \
  --rate 48000 \
  --buffer 512 \
  --seconds 20 \
  --capture-device "External Recorder" \
  --capture-channels 1,2
```

Outputs:

```text
<evidence-dir>/soundcheck/2026-06-12T101530/
  source.json
  reference.wav
  captured.wav
  stream-stats.txt
  cpu-profile.tsv
  coupling-profile.json
  internal-capture/
  metrics.json
  summary.txt
```

Example summary:

```text
SOUNDCHECK A 48000/512: FAIL
source=User Track.mp3 offset=74.500s mode=dense
analog_snr_db=24.8
click_outliers=318
lag_jumps_gt_2_frames=12
high_band_residual_ratio=0.37
mid_band_1000_5000_residual_ratio=0.091
mid_band_cpu_corr=0.74 source=total_audio_ui
driver_internal=PASS
analog_loopback=FAIL
```

That result would mean the data path up to the driver looked correct, but the
real audible output was corrupted after that point.

## Test matrix

For a full regression pass, run:

- pair A, B, C, D
- 48000 Hz / 512 frames
- 44100 Hz / 512 frames
- one synthetic tone set
- one sweep/impulse set
- at least three real-music excerpts: dense, transient, wideband

For a fast pass before a human listening session, run:

- pair A only
- 48000 Hz / 512 frames
- one dense real-music excerpt
- one transient real-music excerpt
- one dense real-music excerpt with `SOUNDCHECK_CPU_STRESS=1`
- one tone/sweep diagnostic

## Pass/fail gates

Initial thresholds should be conservative and adjusted from known-good analog
captures:

```text
alignment_score >= 0.98
snr_db >= 50 for driver-internal capture
snr_db >= 35 for analog loopback
click_outliers == 0 for internal capture
click_outliers <= small calibrated analog limit
lag_jumps_gt_2_frames == 0
clipped_samples == 0
mid_band_1000_5000_residual_ratio <= 0.04
mid_band_cpu_corr <= 0.60 when residual is above 0.02
wrong_pair_energy_db <= -40
```

Analog thresholds must be calibrated with the actual loopback hardware. The
first useful version should print warnings instead of pretending the threshold
is universal.

The CPU-coupled noise gate is intentionally stricter than the old high-band
proxy. It is designed to fail a build when the capture has a narrow
mid-frequency residual that grows during CPU or UI pressure, even when USB
underrun counters and global SNR look acceptable. Use:

```sh
make soundcheck \
  SOUNDCHECK_CAPTURE="External Recorder" \
  SOUNDCHECK_CAPTURE_CHANNELS=1,2 \
  SOUNDCHECK_CPU_STRESS=1
```

The key files for this failure mode are:

- `metrics.json`: final scalar verdict, residual ratio, and CPU correlation.
- `coupling-profile.json`: per-window residual and CPU samples.
- `cpu-profile.tsv`: raw synchronized CPU samples.
- `stream-stats.txt`: driver transport counters sampled during playback.

## Implementation plan

1. Add a local fixture generator.
   - Finds user-selected tracks.
   - Converts supported formats to safe PCM16 WAV clips.
   - Writes `source.json` with file path, offset, duration, rate, and peak.

2. Generalize recording.
   - Replace the hard-coded MacBook recorder with a recorder that can target any
     Core Audio input device and channel pair.
   - Keep the MacBook mic mode as an explicit low-confidence fallback.

3. Add a soundcheck runner.
   - Configures OpenA8DJ rate/buffer.
   - Plays the reference WAV on the selected pair.
   - Records the chosen input device at the same time.
   - Samples `opena8dj-control stream-stats`.
   - Calls the comparison analyzer.

4. Extend the analyzer for real music.
   - Reuse the existing alignment/SNR/residual machinery.
   - Add high-band residual, clipping, channel balance, and local-correlation
     drift metrics.

5. Produce a clear report.
   - `summary.txt` for humans.
   - `metrics.json` for regression comparisons.
   - Keep all generated audio under `<evidence-dir>/`.

## Limitations

This system can replace most manual Spotify checks, but it still depends on the
capture path. If the same Audio 8 DJ is used for both output and input, a failed
analog test means "the round trip is bad" until internal captures isolate the
direction. For the strongest verdict, use a separate recording interface.
