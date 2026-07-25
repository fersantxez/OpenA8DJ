# Quality runs - 2026-06-12

## 0.2.32 build 34 - confirmed post-card capture through mixer REC OUT

Context:

- The iRig Stream is connected to the mixer `REC OUT`.
- The physical measurement path is now:
  `Open Audio 8 DJ -> mixer channel(s) -> mixer REC OUT -> iRig Stream input -> macOS capture`.
- Core Audio devices during the test:
  - `Open Audio 8 DJ`: `in=0 out=8 rate=48000`.
  - `iRig Stream`: `in=2 out=2 rate=48000`.
- Current installed OpenA8DJ identity remains frozen baseline `0.2.32` build
  `34`.

Route proof:

- Run:
  `local-analysis/irig-stream-capture/audio8-to-mixer-rec-irig-20260612-160523`.
- Played alternating left/right pips through Audio 8 `OUT 1|2` and `OUT 3|4`.
- Captured from `iRig Stream` channels `1,2`.
- Capture result:
  - Duration: 28.0 s.
  - Overall RMS: `0.03442846`.
  - Peak: `0.19369507`.
  - Clipped frames: `0`.
  - Noise floor before playback: about `-71 dBFS`.
  - Active pips appeared at about `-27 dBFS` RMS on pair A and
    `-25` to `-28 dBFS` RMS on pair B.
- Audio stack after the route proof was healthy:
  - `coreaudiod=0.4%`.
  - `OpenA8DJ.driver=0.4%`.

Audible-pattern capture proof after the user reported not hearing the test:

- Run:
  `local-analysis/irig-stream-capture/audible-route-proof-20260612-162429`.
- Test signal:
  - 12 s stereo probe at `-12 dBFS`.
  - Even seconds: 440 Hz on left only.
  - Odd seconds: 880 Hz on right only.
  - Played first through Audio 8 pair A, then through pair B.
- Capture result:
  - Capture device: `iRig Stream`, channels `1,2`, `48000 Hz`.
  - Duration: 30.005 s.
  - Overall RMS: `-21.58 dBFS` left, `-21.82 dBFS` right.
  - Peak: `-7.54 dBFS` left, `-7.31 dBFS` right.
  - Clipped frames: `0`.
  - Idle noise floor: about `-71 dBFS`.
  - The expected 440 Hz / 880 Hz alternating pattern was detected by second
    and channel in the iRig capture.
- Audio stack after the run:
  - `coreaudiod=0.3%`.
  - `OpenA8DJ.driver=0.0%`.
- Interpretation:
  - The REC OUT to iRig capture signal is real and strong enough for QA.
  - If the user did not hear this in headphones, the mixer headphone section was
    not monitoring the same master/record path or the relevant channel cue, but
    the REC OUT capture itself is valid.

Music soundcheck through physical capture:

- Pair A run:
  `local-analysis/soundcheck/2026-06-12T160644`.
  - Verdict: `FAIL`.
  - Alignment score: `0.961347`.
  - Analog SNR: `9.17 dB`.
  - 1-5 kHz residual ratio: `1.667333`.
  - 1-5 kHz residual level: `-45.34 dBFS`.
  - CPU correlation: `0.152372`, source `opena8dj_driver`.
  - Click outliers: `2`.
  - Lag jumps > 2 frames: `64`.
  - Capture clipped frames: `0`.
- Pair B run:
  `local-analysis/soundcheck/2026-06-12T160737`.
  - Verdict: `FAIL`.
  - Alignment score: `0.912414`.
  - Analog SNR: `5.91 dB`.
  - 1-5 kHz residual ratio: `2.639969`.
  - 1-5 kHz residual level: `-42.67 dBFS`.
  - CPU correlation: `0.195961`, source `traktor`.
  - Click outliers: `9`.
  - Lag jumps > 2 frames: `66`.
  - Capture clipped frames: `0`.

Important interpretation:

- These music-comparison numbers prove the analog capture path is working, but
  they are not yet a clean release gate because the Audio 8 DJ and iRig Stream
  are separate USB devices with independent clocks. The current analyzer does
  not time-warp the capture before computing waveform residuals, so lag jumps
  can inflate SNR and residual failures.
- The failure is still meaningful: both pairs fail badly, without clipping,
  through a real post-card path.

Tone modulation check:

- Run:
  `local-analysis/irig-stream-capture/audio8-tone-thdn-20260612-160916`.
- Played a 1 kHz sine at `-18 dBFS` through pair A and pair B, captured through
  mixer REC OUT and iRig.
- No clipping was detected.
- Captured spectrum showed a 1 kHz fundamental plus strong sidebands around the
  tone:
  - Pair A fundamental around `-24.7 dBFS`, with `1060 Hz` around `-34.8 dBFS`
    and `940 Hz` around `-40.0 dBFS`.
  - Pair B fundamental around `-19.9 dBFS`, with `1060 Hz` around `-33.8 dBFS`
    and `940/970 Hz` around `-37.7 dBFS`.
- After this test the audio stack was healthy:
  - `coreaudiod=0.1%`.
  - `OpenA8DJ.driver=0.0%`.

Decision:

- The iRig Stream path is valid for future post-device QA.
- A human candidate must not be requested from this state.
- Next QA improvement: update the physical-capture analyzer to compensate
  independent USB clock drift and add a tone/spectrum gate that catches the
  observed 60 Hz sideband modulation around a 1 kHz tone.
- Next driver investigation: determine whether the sideband modulation comes
  from OpenA8DJ USB output cadence, the Audio 8 analog output, or the external
  mixer/REC OUT path.

Analyzer update:

- `scripts/analyze-soundcheck-capture.py` now has an optional `--time-warp`
  pass for physical captures where the Audio 8 DJ and iRig Stream are running
  from independent clocks. The raw lag profile is still reported, but
  residual/noise metrics can be computed from a piecewise lag-corrected capture.
- The soundcheck runner enables that pass by default and reports
  `quality_alignment_score`, `time_warp` drift, 1-5 kHz residuals, 5-12 kHz
  residuals, quiet-window mid-band noise, and CPU correlation.
- Recheck of pair A from
  `local-analysis/soundcheck/2026-06-12T160644` still fails after time-warp:
  `quality_alignment_score=0.962043`, `snr_db=9.97`,
  `mid_band_residual_ratio=1.637216`,
  `high_band_residual_ratio=1.412494`, and
  `quiet_mid_band_noise_dbfs=-46.16`. This means the previous failure was not
  only clock drift.
- `scripts/analyze-tone-capture.py` now reports 60 Hz-spaced sidebands around
  the target tone. Recheck of pair A tone capture found strongest sideband at
  `1060 Hz`, `-11.63 dB` relative to the 1 kHz fundamental.

Run command:

```text
scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 20 --mode dense --cpu-profile --drift-profile
```

## 0.2.59 build 61 - rejected internally

Change:

- Added `OPENA8DJ_PLAYBACK_CAPTURE_PACED`.
- Built with `HAL_PLAYBACK_CAPTURE_PACED=0`, `HAL_ISO_FRAMES=8`,
  `HAL_CAPTURE_QUEUE=64`, `HAL_PLAYBACK_QUEUE=64`,
  `HAL_OUTPUT_GAIN=0.50f`, diagnostic capture off, USB clock anchor off.
- First attempt filled the playback queue immediately when streaming started.

Result:

- Software simulated output passed:
  - SNR: 75.46 dB.
  - Mid-band residual 1-5 kHz: 0.000680, -108.82 dBFS.
  - Alignment: 1.000000.
- Active output-path verification failed:
  - `active-underruns=18624`.

Conclusion:

- Rejected before listening. The queue was filled before enough useful output
  frames existed, so the driver could generate active underruns internally.

## 0.2.60 build 62 - loaded candidate

Change:

- Playback is still decoupled from capture completions, but prequeue refill is
  gated by available output timeline frames.
- `workerLoop` no longer fills playback immediately at stream start.
- `writeOutput` schedules playback refill after Core Audio has written frames.
- `fillPlaybackQueue` stops before consuming beyond available output frames.

Build flags:

```text
HAL_PLAYBACK_CAPTURE_PACED=0
HAL_ISO_FRAMES=8
HAL_CAPTURE_QUEUE=64
HAL_PLAYBACK_QUEUE=64
HAL_DIAGNOSTIC=0
HAL_OUTPUT_GAIN=0.50f
HAL_INPUT_DECODE=0
HAL_USB_CLOCK_ANCHOR=0
HAL_OUTPUT_NATIVE=0
HAL_STREAM_USAGE=1
```

Installed identity:

```text
CFBundleShortVersionString = 0.2.60
CFBundleVersion = 62
sha256 = 0c6196764768f76071b891a694a6f829ae8515b8196f78645f923c90a1d265cc
```

Autonomous checks:

- HAL smoke: pass.
  - Device: Open Audio 8 DJ.
  - Output shape: 4 stereo streams, 8 channels total.
  - Rate/buffer: 48000 Hz / 512 frames.
- Software simulated output: pass.
  - Run: `local-analysis/simulated-output/2026-06-12T134908`.
  - SNR: 75.46 dB.
  - Mid-band residual 1-5 kHz: 0.000680, -108.82 dBFS.
  - Alignment: 1.000000.
  - CPU correlation: 0.000000 in simulation.
- Active output path, 3 s: pass.
  - Generator peak: 0.250000.
  - Driver peak after gain: 0.125000.
  - Active underruns: 0.
  - Playback failures: 0.
- Active output path, 10 s: pass.
  - Generator peak: 0.250000.
  - Driver peak after gain: 0.125000.
  - Active underruns: 0.
  - Playback failures: 0.
  - Output startup silence: 8192 frames.
  - Elastic drops/replays: 0 / 0.
  - Timeline resets: 0.
  - Mode 2 input check errors / output panic flags: 0 / 0.
- CPU stress stream check:
  - Run: `local-analysis/candidate-0260-prequeue-cpu-stress-20260612-135057`.
  - Reference: `local-analysis/soundcheck/2026-06-12T125501/fixture/reference.wav`.
  - Streaming snapshots: 17.
  - Playback queue in-flight range: 0 to 54, target 64.
  - Last active snapshot: `stream-40.txt`.
  - Last active playback failures: 0.
  - Last active underruns: 0.
  - Last active elastic drops/replays: 0 / 0.
  - Last active output peak: 0.125595.
  - Max sampled coreaudiod CPU under synthetic stress: 18.9%.
  - Max sampled driver CPU under synthetic stress: 14.6%.

Human listening result:

- Rejected immediately.
- Reported sound: horrible noise, completely distorted, like a badly tuned
  radio or a noise filter.
- CPU rose a lot during the listening pass.
- The user judged the system possibly unstable and the test unacceptable.
- Rejection artifacts: `local-analysis/rejected-0260-human-20260612-135712`.

Rollback:

- Restored exact frozen `0.2.32` bundle from
  `/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.pre-0259-prequeue-20260612-134611`.
- Active version after rollback: `0.2.32` build `34`.
- Active hash after rollback:
  `bc8ebd741ea33ac70e580bb6b30955ffe67ed2226853933d3bc951edb0a46434`.
- Active output: `Open Audio 8 DJ`.
- Idle after rollback: `coreaudiod=0.0`, `OpenA8DJ.driver=0.0`.

Decision:

- Do not use 0.2.60 as a human candidate again.
- The autonomous checks were insufficient because they did not catch the
  radio/noise-filter distortion heard through the real device path.
- The playback prequeue experiment is not a safe direction unless a future test
  can prove real USB timing quality, not just clean sample bytes and counters.

Why the QA gate falsely passed:

- The simulated-output gate only validated software packing and decode. It did
  not send isochronous USB traffic to the Audio 8 DJ DAC.
- The active-output gate verified that Core Audio wrote samples and that the
  driver saw nonzero output peak after gain. It did not measure the analog
  output or a hardware loopback.
- Stream counters such as `active-underruns=0`, `playback.failed=0`,
  `elastic-drops=0`, and `mode2.output-panic-flags=0` are necessary but not
  sufficient. They do not prove that the device received a timing pattern it
  can turn into clean audio.
- 0.2.60 changed USB output cadence. The bytes looked valid in memory, but the
  real device path produced radio/noise-filter distortion and CPU growth.
- Future release gates must require either a real hardware loopback capture or
  a new timing metric that can detect bad isochronous output cadence before a
  human listening test.

## 0.2.32 build 34 - active-stream CPU/window-noise capture

Context:

- After rollback, the user reported that the system was back to the previous
  baseline: CPU usage still felt excessive, CPU rises were audible, and window
  switching remained very audible.
- Capture run:
  `local-analysis/active-stream-monitor-20260612-141203`.

Installed identity:

```text
CFBundleShortVersionString = 0.2.32
CFBundleVersion = 34
sha256 = bc8ebd741ea33ac70e580bb6b30955ffe67ed2226853933d3bc951edb0a46434
```

Capture result:

- Active stream detected and captured for all 120 samples.
- Playback queue in-flight: min 0, max 10, average 0.992.
- Output ring: min 8511, max 9038, average 8786.533 frames.
- Active underruns: 0.
- Playback failures / queue failures: 0 / 0.
- Elastic drops / replays: 0 / 0.
- Timeline resets: 0.
- Output peak: min 0.164672, max 0.171607.
- Capture transaction failures increased by 148335 during the window.
- Core Audio Driver CPU: min 12.8%, max 33.7%, average 26.9%.
- WindowServer CPU: max 52.7%, average 40.4%.
- Spotify CPU: max 151.9%, average 28.9%.
- Codex CPU: max 102.8%, average 19.1%.

Interpretation:

- This captures the mismatch that matters: real playback was active and the
  user-facing symptom was CPU/window-load related, yet all traditional audio
  health counters stayed clean.
- The current counters still do not expose the audible failure mechanism.
- The driver CPU cost during playback is high enough to be a real suspect.
- The old capture-paced path keeps very shallow playback in-flight depth during
  real playback, but the failed 0.2.60 prequeue experiment proves that simply
  filling the queue differently is not safe.

## 0.2.32 build 34 - fixed local WAV via VLC

Context:

- Baseline fixture:
  `local-analysis/baseline-fixtures/nueva-mexico-baseline-268s-60s-48k-s16.wav`.
- Source:
  `/Users/fer/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3`.
- Source offset/duration: 268 s / 60 s.
- Fixture hash:
  `79b8f5efe0170e3496f0d4230f3b84c3b4322b0c46cba6c8bc83692e6f2f092b`.
- Playback app: VLC, chosen to reduce player CPU vs Spotify.
- Capture run:
  `local-analysis/active-stream-monitor-20260612-141644`.

Capture result:

- Active stream: 112 of 120 samples.
- Playback queue in-flight: min 0, max 2, average 0.652.
- Output ring: min 8377, max 8959, average 8640.375 frames.
- Active underruns: increased by 680.
- Elastic replays: increased by 96.
- Elastic drops: 0.
- Playback failures / queue failures: 0 / 0.
- Output peak: min 0.505076, max 0.506854.
- Core Audio Driver CPU: min 4.5%, max 32.7%, average 26.6%.
- VLC CPU: min 2.5%, max 11.9%, average 4.6%.
- WindowServer CPU: max 51.8%, average 41.8%.

Interpretation:

- This is the best baseline reproduction so far because it uses a fixed local
  WAV and a low-CPU player.
- The audible problem now has objective counters: active underruns and elastic
  replays occur on 0.2.32 during real playback.
- The player is not the primary CPU cause in this run. VLC averaged only 4.6%,
  while the driver averaged 26.6%.
- The next implementation should target driver CPU cost and shallow
  capture-paced playback depth without using the unsafe 0.2.60 prequeue
  approach.

## 0.2.32 build 34 patch p1 - no USB frame polling

Change:

- Started from the exact frozen 0.2.32 executable hash
  `bc8ebd741ea33ac70e580bb6b30955ffe67ed2226853933d3bc951edb0a46434`.
- Patched only `-[OpenA8DJUSBEngine sampleStableUSBFrame:hostTime:]` at
  file offset `0x5390`.
- Original bytes: `f8 5f bc a9 f6 57 01 a9`.
- Patched bytes: `00 00 80 52 c0 03 5f d6` (`mov w0, #0; ret`).
- Installed executable hash:
  `ac61bd5e8071a8835f6803e9e3f736999d9f4f03a37834cd31421c5241b2a46a`.

Reason:

- The live profile showed repeated calls into
  `IOUSBHostObject frameNumberWithTime:` from this function.
- This patch removes that CPU/jitter source without touching stream layout,
  sample format, gain, playback packing, capture pacing, or scheduling.

VLC fixed-WAV gate:

- Run: `local-analysis/active-stream-monitor-20260612-143009`.
- Active stream: 120 of 120 samples.
- Active underruns: 0.
- Elastic replays: 0.
- Playback failures / queue failures: 0 / 0.
- Output peak: min 0.501276, max 0.506854.
- Core Audio Driver CPU: max 21.5%, average 19.8%.
- VLC CPU: max 5.4%, average 3.0%.
- `usb-frame-clock.samples`: 0 after the run, confirming the polling path is
  disabled.

Stress gate:

- Run: `local-analysis/active-stream-monitor-20260612-143127`.
- Stress: 8 s of synthetic CPU load during VLC playback.
- Active stream: 120 of 120 samples.
- Active underruns: 0.
- Elastic replays: 0.
- Playback failures / queue failures: 0 / 0.
- Core Audio Driver CPU: max 28.1%, average 21.9%.

Decision:

- Kept as an improvement over baseline, but not final enough. The no-stress CPU
  average remained too high, so a second minimal patch targeted input-side CPU.

## 0.2.32 build 34 patch p2 - no USB frame polling, no input decode

Change:

- Started from p1 and added one more binary patch.
- Patched `-[OpenA8DJUSBEngine decodeCaptureBytes:length:]` at file offset
  `0x6f20`.
- Original bytes: `f8 5f bc a9 f6 57 01 a9`.
- Patched bytes: `60 fc 45 d3 c0 03 5f d6` (`lsr x0, x3, #5; ret`).
- The function now returns an estimated frame count from packet length and skips
  byte-by-byte input decode/check work. Output packing, gain, stream layout,
  capture pacing, and scheduling are unchanged.
- Installed executable hash:
  `a415c98d3dab3a1456a47da8b60477bbf8d8eb32cb2147db777def6e692ed8b0`.

Reason:

- The p1 profile still showed CPU in input decode even though this pass is
  output-only. Input and microphone behavior remain a separate problem.
- Returning `length >> 5` preserves approximate internal clock progression for
  352-byte capture packets while removing the expensive per-byte input path.

VLC fixed-WAV gate:

- Run: `local-analysis/active-stream-monitor-20260612-143428`.
- Active stream: 120 of 120 samples.
- Active underruns: 0.
- Elastic replays: 0.
- Elastic drops: 0.
- Timeline resets: 0.
- Playback failures / queue failures: 0 / 0.
- Output peak: min 0.501276, max 0.506854.
- Core Audio Driver CPU: max 22.2%, average 17.9%.
- VLC CPU: max 15.6%, average 3.1%.

Stress gate:

- Run: `local-analysis/active-stream-monitor-20260612-143522`.
- Stress: 8 s of synthetic CPU load during VLC playback.
- Active stream: 120 of 120 samples.
- Active underruns: 0.
- Elastic replays: 0.
- Elastic drops: 0.
- Timeline resets: 0.
- Playback failures / queue failures: 0 / 0.
- Output peak: stable at 0.506854.
- Core Audio Driver CPU: max 18.9%, average 16.4%.
- VLC CPU: max 5.1%, average 2.8%.

Post-run idle:

- `coreaudiod`: 0.0%.
- `Core Audio Driver (OpenA8DJ.driver)`: 0.0%.
- Stream state: `streaming: no`.
- Output topology: one 8-channel output stream, 48 kHz, buffer 512,
  `buffer-bytes=16384`.

Profile:

- Run: `local-analysis/profile-0232-patched-no-input-20260612-143649`.
- `sampleStableUSBFrame` and the old byte-by-byte `decodeCaptureBytes` path are
  no longer material hot spots.
- Remaining cost is dominated by system USB enqueue calls:
  `IOUSBHostPipe enqueueIORequest...` and `IOConnectCallAsyncMethod`.
- Driver-owned hot spots are now much smaller: `fillPlaybackBytes`,
  `loadNextOutputFrame`, and `CreateIsoTransfer`.

Decision:

- Leave p2 installed as the next human-listening candidate.
- Do not patch further before listening. The next CPU reduction would need to
  change USB transfer batching or allocation lifetime, which is higher risk
  than these two surgical patches.

## 0.2.62 source candidate - USB cadence instrumentation

Context:

- The user reported that VLC playback still had obvious CPU-correlated noise
  even when `active-underruns`, `elastic-replays`, and playback failures were
  zero.
- The previous gate was therefore incomplete: it proved that Core Audio samples
  reached the driver, but not that USB OUT cadence was clean enough for the DAC.

Change:

- Added stream-stat fields to split capture transaction failures into:
  `captureStatusFailures`, `captureZeroCompleteTransactions`,
  `captureExpectedTransactions`, and `captureOtherByteCountTransactions`.
- Added timing counters for capture completion deltas, playback completion
  deltas, and capture-completion to playback-queue deltas.
- Reordered capture-paced handling to requeue IN before queuing OUT, matching
  the old kext's documented `readHandler` ordering more closely.
- Built as `0.2.62` build `64` with 8 ISO microframes, 64 capture queue depth,
  64 playback target, one 8-channel output stream, input decode/check disabled,
  USB clock anchor disabled, and explicit scheduling disabled.

Run:

- `local-analysis/active-stream-monitor-20260612-145222`

Result:

- Active stream: 467 of 480 samples.
- Active underruns / elastic replays: 0 / 0.
- Playback failures / queue failures: 0 / 0.
- Capture status failures: 0.
- Capture zero-complete transactions: increased to 218464.
- Capture expected transactions: increased to 262144.
- Capture other-size transactions: 0.
- Output peak: min 0.501095, max 0.506854.
- Core Audio Driver CPU: max 21.4%, average 16.4%.
- VLC CPU: max 17.4%, average 1.8%.

Interpretation:

- The large legacy `capture.failed` number was mostly zero-complete USB
  microframes, not actual transaction status failures.
- This candidate did not clearly improve the CPU/noise hypothesis enough to be
  a human candidate.
- After the following rejected explicit-scheduling test and rollback, this
  source candidate also left `coreaudiod` unstable under Core Audio device
  configuration queries, so it is rejected.

## 0.2.63 source candidate - explicit USB frame scheduling

Context:

- The old kext schedules OUT against USB frames. This test tried explicit OUT
  scheduling without forcing HAL `GetZeroTimeStamp` to the USB clock.

Change:

- Built as `0.2.63` build `65`.
- Same as 0.2.62, but with `OPENA8DJ_ENABLE_EXPLICIT_ISOC_SCHEDULING=1`.
- `sampleStableUSBFrame` returns false when USB clock anchor support is disabled
  so this path uses one frame-number query rather than the expensive stable
  multi-sampling loop.

Run:

- `local-analysis/active-stream-monitor-20260612-145724`

Result:

- Active stream: 480 of 480 samples.
- Playback in-flight saturated at 127-128.
- Playback queue failures increased to 45102.
- Timeline resets increased to 172.
- Output peak stayed at 0.0.
- Output read rate averaged only about 11000 frames/s instead of 48000.
- Core Audio Driver CPU average fell to 9.9%, but the output path was invalid.

Decision:

- Rejected immediately. This was an objective no-audio / bad-scheduling failure.
- Rolled back first to 0.2.62, then to the exact stable 0.2.32 baseline after
  coreaudiod stayed hot during configuration queries.

## Current loaded state after failed cadence experiments

Installed:

```text
CFBundleShortVersionString = 0.2.32
CFBundleVersion = 34
sha256 = bc8ebd741ea33ac70e580bb6b30955ffe67ed2226853933d3bc951edb0a46434
```

Reason:

- The p1/p2 binary-patched 0.2.32 variants and the source-built 0.2.62/0.2.63
  candidates all failed to become trustworthy listening candidates.
- The source candidates introduced Core Audio instability after reload and
  configuration queries.
- The exact 0.2.32 baseline restored from
  `/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.pre-0259-prequeue-20260612-134611`
  returns `coreaudiod` and `Core Audio Driver (OpenA8DJ.driver)` to 0% idle.

Conclusion:

- Do not ask the user to test 0.2.62 or 0.2.63.
- Do not treat zero underruns/replays as a sufficient quality gate.
- The next safe implementation direction is not another live HAL experiment;
  it should be an isolated source cleanup against a known-good code baseline,
  with transfer pooling and timing instrumentation landed separately before any
  playback behavior change.

## 0.2.65 / 0.2.66 source follow-up - rejected, CPU emergency recovered

Context:

- After iRig was unplugged/replugged, Core Audio still saw `iRig Stream` and
  `Open Audio 8 DJ`.
- The active binary-patched P2 bundle was verified before retesting:
  `0.2.32` build `34`, hash
  `a415c98d3dab3a1456a47da8b60477bbf8d8eb32cb2147db777def6e692ed8b0`.
- Real capture was through Audio 8 DJ output A/B -> mixer REC OUT -> iRig.

P2 post-replug physical gate:

- Run: `local-analysis/soundcheck/post-irig-replug-p2-20260612-171600`.
- Result: FAIL.
- `alignment_score=0.971057`, so the iRig route was valid.
- `analog_snr_db=8.64`.
- `mid_band_1000_5000_residual_ratio=1.472880`.
- `high_band_5000_12000_residual_ratio=1.380525`.
- `quiet_mid_band_noise_dbfs=-33.92`.
- `click_outliers=368`, `lag_jumps_gt_2_frames=132`.
- Silence through the same path was clean:
  `local-analysis/irig-stream-capture/post-irig-replug-silence-20260612-171905`,
  `rms_dbfs=-74.36`, `peak_dbfs=-60.48`.

0.2.65 build 67:

- Intended as P3 `pool-only` source build: ISO 8, capture/playback queues
  64/64, gain 0.50, input decode/checks off, USB clock anchor off, explicit
  scheduling off, transfer pooling on.
- Built and smoke-tested successfully, hash
  `6ba1ca77a1d12f786d38af00e27f5723eb91d3694f02210daffbd9a21d186df6`.
- Music run:
  `local-analysis/soundcheck/candidate-0265-p3-pool-iso8-irig-20260612-172326`.
- Result: FAIL.
- `analog_snr_db=8.48`, `mid_band_1000_5000_residual_ratio=1.466040`,
  `high_band_5000_12000_residual_ratio=1.387000`,
  `quiet_mid_band_noise_dbfs=-31.86`, `click_outliers=250`,
  `lag_jumps_gt_2_frames=130`.
- Tone run:
  `local-analysis/irig-stream-capture/candidate-0265-p3-pool-tones-20260612-172531`.
- The 1 kHz tone at amp 0.12 had `sideband_ratio=0.130937` and strongest
  1060 Hz sideband at `-18.57 dB` relative, clearly worse than the best earlier
  P2 tone traces.
- Decision: rejected and rolled back.

P2 variability after rollback:

- The rollback restored P2 hash
  `a415c98d3dab3a1456a47da8b60477bbf8d8eb32cb2147db777def6e692ed8b0`.
- Repeated P2 1 kHz tone runs remained bad:
  `local-analysis/irig-stream-capture/p2-tone-variability-20260612-172815`.
- Sideband ratios across three runs: `0.089872`, `0.076705`, `0.104112`.
- Strongest sideband stayed around `-21` to `-24 dB` relative.
- Level sweep:
  `local-analysis/irig-stream-capture/p2-level-sweep-20260612-172940`.
- Tone amplitudes from `0.015` through `0.12` all kept roughly the same bad
  relative sideband level, so this is not just output headroom or hard clipping.
- Pair sweep:
  `local-analysis/irig-stream-capture/p2-output-pair-sweep-20260612-173127`.
- Pairs A and B reached iRig and failed similarly; C/D were not a useful REC OUT
  route in the current mixer wiring.

0.2.66 build 68:

- Added `OPENA8DJ_ENABLE_TRANSFER_POOL` and built with transfer pooling disabled
  to test whether the current source tree can reproduce a stable P2-like
  baseline.
- Built and smoke-tested successfully, hash
  `b57fbbb097992f79dca78f17b3df004c25fedc4993e3465267105e3a03b2b159`
  before install.
- Installed hash after codesign was
  `d1c0743605c79c58e5fd3baa45783a21a8cadf7c787eb93a4bf008f845abc22e`.
- Result: rejected before audio testing. `audio-list` hung and `coreaudiod`
  went over 100% CPU with 0.2.66 loaded.
- This proves the current source tree is not a safe base for more live installs,
  even with pooling disabled.

Emergency recovery:

- P2 was restored, but the Core Audio stack remained hot after the 0.2.66 hang.
- OpenA8DJ was then moved out of `/Library/Audio/Plug-Ins/HAL` entirely.
- With OpenA8DJ unloaded, Core Audio enumeration recovered and showed only:
  `iRig Stream`, `MacBook Air Microphone`, and `MacBook Air Speakers`.
- Stuck services were force-restarted: `coreaudiod`, `mediaremoted`,
  `PerfPowerServices`, `universalaccessd`, `ControlCenter`, `corespeechd`,
  `audioaccessoryd`, `audiomxd`, `systemsoundserverd`,
  `ContinuityCaptureAgent`, `avconferenced`, and `callservicesd`.
- Final safe state after emergency reset: OpenA8DJ unloaded, Core Audio
  enumeration OK, and the sampled CPU was `88.7% idle`.

Decision:

- Do not ask for human listening on 0.2.65 or 0.2.66.
- Do not reload source-built candidates from the current tree.
- Keep the machine safe with OpenA8DJ unloaded until a known-good source
  baseline is isolated offline.
- Treat the physical iRig gate as valid, but add a repeated-tone requirement:
  a candidate must pass multiple 1 kHz tone captures with low sidebands before
  the longer music capture or any human test.

## 0.2.67 through 0.2.71 source recovery pass

Context:

- Goal: recover from the unstable source-built line and produce a candidate
  that is at least as safe as P2 while reducing the CPU/window-correlated
  sideband noise heard by the user.
- The physical route remained Audio 8 DJ output A/B -> mixer REC OUT -> iRig.
- Human listening remains decisive, but candidates are no longer offered unless
  they pass safety, tone, and repeated music gates.

Rejected safety and quality variants:

- `0.2.67` build `69`: enabled `IOProcStreamUsage` and a 3 ms property backoff.
  Offline HAL parity passed, but safety failed on cycle 2 with `coreaudiod`
  around `92.6%` and `mediaremoted` around `58.7%`.
- `0.2.68` build `70`: disabled `IOProcStreamUsage`, kept the 3 ms property
  backoff. Audio 8 DJ was visible, but safety failed with `coreaudiod=62.5%`
  and `mediaremoted=35.0%`.
- `0.2.69` build `71`: removed property backoff and kept output-only topology.
  Safety passed, but quality failed. Music run
  `local-analysis/soundcheck/candidate-0269-no-streamusage-no-backoff-irig-20260612-181303`
  had `quality_alignment_score=0.448600`, `click_outliers=25`, and
  `clock-anchor: fallback`. Tone sideband ratio was still poor compared with P2.
- `0.2.70` build `72`: restored USB clock anchor while leaving
  `IOProcStreamUsage` and property backoff off. Safety passed and tone improved
  strongly (`sideband_ratio=0.038920`, strongest sideband `-27.46 dB`), but the
  45 s music gate showed excessive normalized window noise compared with P2:
  mid-band window residual p95 was `+2.10 dB` vs P2-good.

Current candidate: `0.2.71` build `73`.

- Change: keep `HAL_USB_CLOCK_ANCHOR=1`, but add
  `HAL_USB_STABLE_FRAME=0` / `OPENA8DJ_ENABLE_USB_STABLE_FRAME_POLL=0`.
  This preserves transaction-timestamp clock anchoring while reproducing the
  successful P2 surgery that removed repeated `frameNumberWithTime` polling.
- Other active flags: `HAL_OUTPUT_GAIN=0.50f`, input decode/checks off,
  one 8-channel output stream, ISO frames `8`, capture/playback queues `64/64`,
  capture-paced playback on, explicit scheduling off, native I24 off, start byte
  `4`, stream usage off, property backoff `0`, transfer pooling off.
- Offline parity:
  `ownedObjects=1`, input streams/channels `0/0`, output streams/channels
  `1/8`, `IOProcStreamUsage` missing by design, default output yes.
- Safety:
  `local-analysis/hal-candidate-safety/0271-clean-rerun-20260612-184234`
  passed two load/unload cycles from a clean unloaded state.
- Tone:
  `local-analysis/irig-stream-capture/candidate-0271-anchor-no-stable-tone-20260612-184438`
  produced `sideband_ratio=0.019363`, strongest 1060 Hz sideband
  `-34.17 dB`, zero active underruns, zero elastic drops/replays, zero timeline
  resets, and post-run audio stack PASS.
- Music run 1:
  `local-analysis/soundcheck/candidate-0271-anchor-no-stable-p2matched-irig-20260612-184522`
  used the P2-matched 45 s fixture at 48 kHz / 512 / target peak -12 dB.
  It had `quality_alignment_score=0.973530`, `analog_snr_db=8.86`,
  `mid_band_residual_ratio=1.478070`, `click_outliers=19`, `lag_jumps=121`,
  driver CPU max/avg `22.1%/17.0%`, valid clock anchor, and no underruns,
  drops, replays, timeline resets, or clipping.
- Music run 2:
  `local-analysis/soundcheck/candidate-0271-anchor-no-stable-p2matched-repeat-irig-20260612-184724`
  had `quality_alignment_score=0.971250`, `analog_snr_db=8.80`,
  `mid_band_residual_ratio=1.462309`, `click_outliers=269`, `lag_jumps=130`,
  driver CPU max/avg `21.0%/16.4%`, valid clock anchor, and no underruns,
  drops, replays, timeline resets, or clipping.

Normalized noise gate:

- P2-good
  `local-analysis/soundcheck/candidate-0232-p2-irig-20260612-170819`:
  mid-band window residual ratio p95 `1.532`, RMS p95 `0.02699477`.
- P2 post-replug:
  ratio p95 `1.618`, RMS p95 `0.02740337`.
- `0.2.70`: ratio p95 `1.802`, RMS p95 `0.03436865`.
- `0.2.71` run 1: ratio p95 `1.625`, RMS p95 `0.03377744`.
- `0.2.71` run 2: ratio p95 `1.579`, RMS p95 `0.03281578`.

Interpretation:

- The absolute RMS p95 remains higher than P2, but the 0.2.71 captures were also
  about 1.7-1.8 dB louder at the iRig than the P2 captures despite identical
  fixtures and internal output peaks. The normalized ratio p95 is therefore the
  more useful gate for the current physical route.
- `0.2.71` passes the normalized P2 +1 dB gate and strongly beats both P2 and
  `0.2.70` on 1 kHz CPU-sideband tone. It does not prove the user will hear it
  as clean; it is the first source-built candidate in this pass that is safe
  enough and measured enough to justify a human listening test.

## 0.2.72 through 0.2.78 follow-up after user rejects 0.2.71 as not hi-fi

Human feedback on `0.2.71`:

- Sensible improvement, but still audible background noise.
- Window switching still changes the noise/click texture.
- Not acceptable for a studio/high-fidelity driver.

Deterministic negative capture:

- Run:
  `local-analysis/soundcheck/candidate-0271-user-window-stress-20260612-190258`.
- Result: FAIL.
- `quality_alignment_score=0.849187`.
- `analog_snr_db=6.22`.
- `mid_band_1000_5000_residual_ratio=1.528360`.
- `mid_band_window_residual_ratio_p95=1.681541`.
- `quiet_mid_band_noise_dbfs=-32.40`.
- `click_outliers=31`.
- `lag_jumps_gt_2_frames=128`.

Rejected follow-up variants:

- `0.2.72` build `74`: transfer pool only. Rejected at safety because the
  required Audio 8 DJ device did not reappear in cycle 1.
- `0.2.73` build `75`: sample-time continuity follower. Rejected by tone:
  `sideband_ratio=0.205719`, strongest 1060 Hz sideband `-14.28 dB`, clearly
  worse than `0.2.71`.
- `0.2.74` build `76`: ISO frames per transfer increased to 16. Safety passed,
  but tone was worse: `sideband_ratio=0.242133`, strongest 1060 Hz sideband
  `-12.92 dB`.
- `0.2.75` build `77`: output gain raised to `0.75`. Rejected at safety because
  post-unload Core Audio/mediaremoted CPU spiked (`coreaudiod=88.1%`,
  `mediaremoted=56.3%`).
- `0.2.76` build `78`: USB zero timestamp exposed to HAL with an 8-sample
  jitter filter using transaction timestamps, without `frameNumberWithTime`.
  Safety passed, but tone capture was effectively silent
  (`fundamental_dbfs=-85.57`) and the control socket was unavailable after the
  run. Rejected.
- `0.2.77` build `79`: four stereo output streams, otherwise back to the
  `0.2.71` transport. Safety passed, but tone was worse:
  `sideband_ratio=0.235512`, strongest 1060 Hz sideband `-12.72 dB`.
- `0.2.78` build `80`: restored the `0.2.71`-style safe flags with failed
  experiments disabled. Safety failed during the first reload due to
  mediaremoted/coreaudiod CPU (`mediaremoted=53.3%`, `coreaudiod=29.9%`), then
  recovery unloaded OpenA8DJ and returned the audio stack to PASS.

Current conclusion:

- Do not ask the user to test any of `0.2.72` through `0.2.78`.
- `0.2.71` remains the best source-built audio result so far, but it is not a
  high-fidelity candidate.
- The old kext's `jitterFilter()` has now been decoded as a weighted average:
  `(current + (weight - 1) * previous + weight / 2) / weight`.
- Simply wiring the USB transaction timestamp into HAL `GetZeroTimeStamp` is
  unsafe/incorrect in the current HAL model. The next serious fix needs a
  designed timestamp model, not a direct anchor swap.

## Research-backed live path: CAIAQ / USB OUT cadence

The broad research captured in
`docs/USB_AUDIO_CADENCE_RESEARCH_2026-06-12.md` should stay on the active list.
It is worth investigating, but only as an instrumentation-first cadence path,
not as a blind Linux-driver port or a behavior change to install immediately.

OpenA8DJ remains macOS-first: Core Audio HAL, AudioServerPlugIn, IOUSBHost,
coreaudiod stability, enumeration, buffer size, `GetZeroTimeStamp`, and
physical output quality take priority over Linux parity.

Why it is promising:

- It matches the real failures better than a pure sample-packing theory:
  internal counters can be clean while physical iRig captures still show noise,
  1 kHz sidebands, lag jumps, and CPU/window-switch coupling.
- Linux `snd-usb-caiaq` and ALSA history point at output URB lifecycle, ISO
  packet lengths/offsets, active-output tracking, bogus packet filtering, and
  period/cadence reporting as first-class audio-quality risks.
- Those Linux details are not direct implementation APIs here. They should be
  converted into macOS measurements: IOUSBHost completion deltas,
  IN-completion to OUT-queue deltas, OUT completion deltas, packet
  lengths/offsets, output in-flight depth, queue misses, zero-length
  microframes, output read rate, coreaudiod/driver CPU, and iRig sidebands.
- The old macOS driver shows the same family of idea: completed input USB
  transactions influence the next output transaction shape. The current source
  should prove whether it is matching that cadence before adding bigger queues
  or more prequeue.
- The failed `0.2.76` result confirms that the device cadence cannot simply be
  exposed through HAL `GetZeroTimeStamp`. HAL timing must remain stable while
  any USB-device cadence following happens inside the transport path.

What this path must not repeat:

- Do not force HAL `GetZeroTimeStamp` to USB frame or transaction timing.
- Do not assume ALSA implicit feedback, URBs, `period_elapsed`, or Linux kernel
  scheduling maps directly onto AudioServerPlugIn/IOUSBHost.
- Do not repeat the `0.2.60` prequeue/saturation direction without new cadence
  evidence.
- Do not install candidates from an unsafe source tree if Core Audio
  enumeration hangs, coreaudiod/mediaremoted go hot, or the physical output goes
  silent.
- Do not ask for human listening until physical iRig tone and music gates pass.
- Do not treat Linux parity as success. The success gate is macOS stability plus
  physical iRig quality.

Safe investigation order:

1. Add instrumentation only, with no playback behavior change: per-IN
   completion timing, microframe status/counts, actual packet lengths/offsets,
   derived OUT request lengths/offsets, OUT completion timing, active OUT slot
   id/bitmap, output read position, queue misses, zero-length packets, and CPU
   samples during window-switch stress.
2. Correlate those traces with physical iRig captures: 1 kHz sideband ratio,
   strongest sidebands around the fundamental, lag jumps, click outliers, and
   mid-band residuals from the `0.2.71` user-window-stress baseline.
3. Only if the trace shows a cadence/lifecycle defect, implement a CAIAQ-style
   active OUT slot tracker using preallocated descriptors and output packet
   layout derived from completed IN transactions.
4. Gate any behavior-changing candidate with safety reloads, three physical
   tone captures, physical music capture, CPU stability, and loaded-build
   identity before asking the user to listen.

Expected proof:

- The 1 kHz physical sideband ratio must materially improve versus the `0.2.71`
  best source-built baseline without raising CPU or reload instability.
- Physical music captures must improve residual/noise metrics, especially the
  mid-band and window-stress measures the user can hear.
- Clean internal underrun/replay/failure counters remain necessary but are not
  sufficient proof.

## 0.2.79 build 81 - macOS cadence instrumentation only

Change:

- Added macOS-first cadence instrumentation behind
  `OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC=1`.
- No intended audio behavior change: `GetZeroTimeStamp` remains on the stable
  Core Audio timeline, USB zero timestamp remains disabled, output gain remains
  `0.50`, output topology remains one 8-channel output stream, ISO frames remain
  `8`, and capture-paced playback remains enabled.
- New stream stats include completion outliers, IN USB timestamp deltas, IN/OUT
  packet layout ranges, playback queue attempts, in-flight depth at queue and
  completion, zero-complete counters, and rolling layout signatures.
- `opena8dj-control stream-stats` now prints stable `key=value` lines so
  `scripts/run-soundcheck --stream-stats-snapshots` can correlate driver
  counters with CPU and iRig captures.

Build:

- Version/build: `0.2.79` / `81`.
- Built hash:
  `d13219fabe238b6fec0e26a4e7e2c342ce698645a56eb3e668f8820cb68ddbb7`.
- HAL smoke: PASS.
- HAL parity: PASS. Shape remained output-only: input streams `0`, output
  streams `1`, global channels `8`, buffer `512`, `IOProcStreamUsage` missing.

Safety:

- Preflight audio stack:
  `local-analysis/audio-stack-guard/pre-0279-instrumentation-20260612-195619`
  PASS, OpenA8DJ unloaded, `coreaudiod=0.0`, global idle `80.23%`.
- Two-cycle load/unload safety:
  `local-analysis/hal-candidate-safety/0279-cadence-instrumentation-20260612-195633`
  PASS, `leave_loaded=0`.
- Post-safety guard:
  `local-analysis/audio-stack-guard/post-0279-safety-20260612-195723`
  PASS, OpenA8DJ unloaded, `coreaudiod=0.0`.

Diagnostic physical music run:

- Run:
  `local-analysis/soundcheck/candidate-0279-cadence-diagnostic-20260612-195928`.
- Physical route: Audio 8 DJ output A/B -> mixer REC OUT -> iRig Stream input.
- Command used `--stream-stats-snapshots`, 8 s dense music, 48 kHz, 512 buffer,
  pair A, and a short CPU-stress phase.
- Result: FAIL, diagnostic-only. Do not ask for human listening.
- `quality_alignment_score=0.972576`.
- `analog_snr_db=10.92`.
- `mid_band_1000_5000_residual_ratio=1.384711`.
- `high_band_5000_12000_residual_ratio=1.389209`.
- `quiet_mid_band_noise_dbfs=-31.82`.
- `mid_band_cpu_corr=0.612586`, source `active_stress_workers`.
- `click_outliers=0`.
- `lag_jumps_gt_2_frames=9`.
- `capture_clipped_frames=0`.

Last stream-stats sample from the diagnostic run:

- `captureTransfersCompleted=7999`.
- `playbackTransfersSubmitted=7999`.
- `playbackTransfersCompleted=7998`.
- `captureTransactionErrors=29088`.
- `playbackTransferErrors=0`.
- `outputFramesWritten=383488`.
- `outputFramesRead=383945`.
- `outputActiveUnderruns=0`.
- `outputElasticDrops=0`.
- `outputElasticReplays=0`.
- `outputTimelineResets=0`.
- `cadenceExpectedTransferTicks=24000`.
- `captureCompletionDeltaOutliers=9`.
- `playbackCompletionDeltaOutliers=11`.
- `captureToPlaybackQueueDeltaOutliers=1`.
- `captureUSBTimestampOutOfOrder=44121`.
- `captureUSBTimestampZero=0`.
- `playbackZeroCompleteTransactions=0`.
- `playbackInFlightAtQueueMax=19`.
- `playbackInFlightAtCompletionMax=19`.

Post-run incident and recovery:

- After the physical soundcheck, OpenA8DJ was unloaded but Core Audio/media CPU
  was hot:
  `local-analysis/audio-stack-guard/post-0279-soundcheck-unload-20260612-200021`
  failed with `coreaudiod=97.3%` and `mediaremoted=53.1%`.
- Recovery:
  `local-analysis/audio-stack-guard/recover-after-0279-soundcheck-hot-20260612-200030`
  PASS. Final state: OpenA8DJ unloaded, no driver pids, `coreaudiod=0.0`,
  `mediaremoted=0.0`, Core Audio enumeration PASS.

Conclusion:

- `0.2.79` is useful as a diagnostic instrumentation build, not as a listening
  candidate.
- The new stats captured actionable cadence signals during physical playback:
  IN/OUT completion outliers, one IN-to-OUT queue outlier, high OUT in-flight
  depth, and a large count of out-of-order capture USB timestamps.
- The next behavior-changing pass should not touch HAL timing. Analyze whether
  the `captureUSBTimestampOutOfOrder` count is expected for IOUSBHost timestamp
  semantics or indicates a bad assumption in the clock/cadence model, then
  choose between realtime cleanup and a macOS transport-only cadence experiment.

## 0.2.80 through 0.2.83 follow-up - no listening candidate

Context:

- The `0.2.79` cadence diagnostic showed many
  `captureUSBTimestampOutOfOrder` counts. The macOS SDK documents that
  multiple isochronous transactions may legitimately complete with the same
  timestamp under interrupt latency/system load, so the metric was corrected to
  distinguish repeated timestamps from truly backwards timestamps.
- `scripts/analyze-tone-capture.py` now has `--auto-window` because repeated
  tone captures showed that fixed `--trim-start/--trim-end` windows can include
  silence or startup delay and create false sideband failures. Future tone gates
  should use `--auto-window` and still report the selected window.

Rejected variants:

- `0.2.80` build `82`: transfer pool plus corrected timestamp metric.
  Build hash:
  `28116c09105e79b18602299a1a52ee87c0d7de3c9d45929cf6c142c479e342f6`.
  Rejected at safety:
  `local-analysis/hal-candidate-safety/0280-transfer-pool-cadence-20260612-200613`.
  Cycle 1 failed with `coreaudiod=111.4%`,
  `total_watched_cpu_pct=122.0`. Recovery passed:
  `local-analysis/audio-stack-guard/recover-after-0280-transfer-pool-fail-20260612-200715`.
- `0.2.81` build `83`: transfer pool off, cadence diagnostic off, intended as a
  clean realtime-hygiene/control run. Build hash:
  `70087c8954576bd9e0ecc73593ef7c12fcdcb07234fc990e0577f37921d0fdc8`.
  Safety passed:
  `local-analysis/hal-candidate-safety/0281-no-cadence-realtime-clean-20260612-200819`.
  Tone captures were initially misleading because fixed analysis windows
  included silence, but `0.2.81` still did not produce a candidate-worthy tone
  once the analyzer was corrected. It was unloaded and rejected.
- `0.2.82` build `84`: macOS transport experiment, derive OUT packet layout only
  from valid/expected IN transactions, with cadence diagnostic enabled. Build
  hash:
  `5adabda844b4fa7bda1dbc484cccce058ce27707977e728babfe7dbf35f075f8`.
  Rejected at safety:
  `local-analysis/hal-candidate-safety/0282-valid-capture-layout-20260612-202206`.
  It loaded cleanly, but post-unload guard failed with `coreaudiod=105.1%`.
  Recovery passed:
  `local-analysis/audio-stack-guard/recover-after-0282-safety-fail-20260612-202314`.
- `0.2.83` build `85`: same valid-IN-layout OUT filter as `0.2.82`, cadence
  diagnostic disabled to isolate behavior from instrumentation. Build hash:
  `0bae866bd4c2ca32b1e7b8fa4024a6f164d9691bee5777b10595b546c6a95ee1`.
  Safety passed:
  `local-analysis/hal-candidate-safety/0283-valid-layout-no-diagnostic-20260612-202350`.
  Tone run:
  `local-analysis/irig-stream-capture/candidate-0283-valid-layout-settled-tone-20260612-202517`
  measured `sideband_ratio=0.241966`, strongest 1060 Hz sideband `-14.01 dB`
  relative. This is not a candidate and did not materially improve the current
  physical-route baseline.

Important measurement finding:

- The historical `0.2.71` baseline tone file
  `local-analysis/irig-stream-capture/candidate-0271-anchor-no-stable-tone-20260612-184438/tone-A-1k-amp-0.12.wav`
  still analyzes cleanly with `--auto-window`: `sideband_ratio=0.018609`,
  strongest 1060 Hz sideband `-34.60 dB` relative.
- Re-running the same archived `0.2.71` driver later through the current
  physical route no longer reproduces that clean result:
  - `local-analysis/irig-stream-capture/baseline-0271-control-settled-tone-20260612-201741`:
    `sideband_ratio=0.239881`, strongest 1060 Hz sideband `-13.35 dB`.
  - `local-analysis/irig-stream-capture/baseline-0271-after-rate-reset-20260612-201844`:
    `sideband_ratio=0.210388`, strongest 1060 Hz sideband `-14.00 dB`.
  - `local-analysis/irig-stream-capture/baseline-0271-pair-scan-20260612-201935`:
    pair A `0.257789`, pair B `0.262090`, pairs C/D effectively too low-level
    for a valid output gate.
- This means the current physical measurement route or device state is not
  reproducing the earlier clean baseline. Do not use the current sideband gate
  to claim a driver improvement until the archived `0.2.71` baseline again
  measures close to its historical clean tone, or until a new baseline route is
  established.

Current final state after this pass:

- OpenA8DJ unloaded.
- Recovery guard:
  `local-analysis/audio-stack-guard/recover-after-0283-final-hot-20260612-202625`
  PASS, `coreaudiod=0.0`, no OpenA8DJ driver pids, Core Audio enumeration PASS.
- No email sent and no human listening requested.

## 0.2.84 through 0.2.95 follow-up - ISO cadence search

Context:

- Re-running archived `0.2.71` after recovery still did not reproduce its clean
  historical tone. Current `0.2.71` tone measured around `sideband_ratio=0.25`
  with strongest sidebands near `-13 dB`.
- P2 in the same physical route still measured clean tone:
  `local-analysis/irig-stream-capture/p2-current-state-tone-20260612-204900`
  had `sideband_ratio=0.018496`, strongest sideband `-34.42 dB`.
- This isolated the current bad `0.2.71` tone to the source USB-clock-anchor
  path rather than the iRig/mixer path.

Exploratory variants:

- `0.2.84` build `86`: P2-like source defaults, USB clock anchor off and valid
  capture OUT layout filter off. Safety passed; tone recovered
  (`sideband_ratio=0.019747`), but music did not improve enough:
  `mid_band_residual_ratio=1.468478`, `mid_band_cpu_corr=0.297553`.
- `0.2.85` build `87`: `0.2.84` plus transfer pool. Safety and tone passed
  (`sideband_ratio=0.019352`), but music remained weak:
  `mid_band_residual_ratio=1.473278`.
- `0.2.86` build `88`: ISO16 plus pool. Rejected at safety cycle 1.
- `0.2.87` build `89`: ISO16 without pool. Safety passed, but tone failed:
  `sideband_ratio=0.059990`, strongest sideband `-25.82 dB`.
- `0.2.88` build `90`: ISO4 without pool. Tone improved
  (`sideband_ratio=0.012952`, strongest `-38.87 dB`) and music improved
  slightly (`quality_alignment_score=0.980534`,
  `mid_band_residual_ratio=1.431043`), but driver CPU was too high
  (`avg=36.73%`, `max=42.4%`).
- `0.2.89` build `91`: ISO6 without pool. Rejected at safety cycle 2.
- `0.2.90` build `92`: ISO4 plus pool. Best tone at that point
  (`sideband_ratio=0.007605`, strongest `-43.32 dB`) and music improved
  (`mid_band_residual_ratio=1.424793`, `click_outliers=3`), but driver CPU
  remained high (`avg=33.91%`, `max=43.5%`).
- `0.2.91` build `93`: ISO5 plus pool. Safety passed and tone was excellent
  (`sideband_ratio=0.005476`, strongest `-46.60 dB`). Music was comparable to
  the best runs (`mid_band_residual_ratio=1.432311`,
  `mid_band_cpu_corr=0.111789`, `click_outliers=4`) with lower CPU than ISO4
  (`avg=29.20%`, `max=33.1%`).
- `0.2.92` build `94`: ISO7 plus pool. Rejected at safety cycle 2.
- `0.2.94` build `96`: native I24 byte order. Safety passed, but tone capture
  nearly clipped (`peak=0.99996948`) with very high residual. Rejected.

Current listening candidate:

- `0.2.95` build `97`: same design as `0.2.91`, with safe byte order restored.
- Installed hash:
  `eddba3682361f0d0e08062243c4e2364c2ac187a53ed5f01d0848489d7e44b55`.
- Final safety rerun:
  `local-analysis/hal-candidate-safety/0295-iso5-pool-clean-rerun-20260612-212651`
  PASS, two cycles, left loaded.
- Final tone:
  `local-analysis/irig-stream-capture/candidate-0295-final-tone-20260612-212739`
  measured `sideband_ratio=0.009213`, strongest sideband `-41.39 dB`,
  `peak=0.26087952`, no clipping.
- Stress music:
  `local-analysis/soundcheck/candidate-0295-final-stress-music-20260612-212814`
  had `mid_band_residual_ratio=1.414267`,
  `high_band_residual_ratio=1.361805`, `click_outliers=3`,
  `mid_band_cpu_corr=0.184713`, no clipping. The run had a low absolute
  `quality_alignment_score` because the selected fixture offset was `30s`; use
  residual/click/correlation fields as the useful signal for this run.
- Final guard:
  `local-analysis/final-candidate-0295-ready-20260612-212937/final-guard`
  PASS. `OpenA8DJ` is loaded, `coreaudiod=0.1%`, driver idle `0.0%`.

Conclusion:

- `0.2.95` is not perfect: playback CPU during active music is still higher
  than desired. However, it materially improves the physical 1 kHz sideband
  gate and keeps music residual/click metrics in the best observed range while
  staying stable at idle.
- It is the best available candidate from this pass and has been left loaded
  for human listening.

## 0.2.96 through 0.3.03 follow-up - CPU/jitter reduction pass

Context:

- The user could not listen during this pass. All decisions below use the
  physical iRig route and Core Audio health gates.
- The working hypothesis was CPU/jitter coupling in the macOS HAL/IOUSBHost
  hot path, not byte order or HAL timestamp anchoring.
- `0.2.95` remained the safety baseline: ISO5, transfer pool, USB clock anchor
  off, valid capture OUT layout off, safe I24 byte order, gain `0.50`.

Baseline repeat:

- `0.2.95` repeat:
  `local-analysis/iteration-0295-repeat-20260612-214756`
  tone `sideband_ratio=0.017963`, music `mid_band_residual_ratio=1.419785`,
  `high_band_residual_ratio=1.358521`, `click_outliers=0`,
  `mid_band_cpu_corr=0.149150`, `lag_jumps_gt_2_frames=41`.
- Longer `0.2.95` baseline:
  `local-analysis/iteration-0295-long-baseline-20260612-215550`
  failed strict music gate with `quality_alignment_score=0.979944`,
  `analog_snr_db=8.38`, `mid_band_residual_ratio=1.449180`,
  `click_outliers=27`, `lag_jumps_gt_2_frames=126`.

Rejected variants:

- `0.2.96` build `98`: gain `0.35`. Safety passed and absolute dBFS noise
  dropped, but relative mid-band quality worsened:
  `mid_band_residual_ratio=1.437778`. Rejected as a gain-level false positive.
- `0.2.97` build `99`: gain `0.42`. Safety failed cycle 2 and recovery was
  required. Rejected.
- `0.2.98` build `100`: output sample-time follower enabled. Safety passed,
  but tone worsened to `sideband_ratio=0.027214`. Rejected.
- `0.2.99` build `101`: playback prefill/output-paced instead of
  capture-paced. Safety failed with hot `coreaudiod`. Rejected and recovered.
- `0.3.01` build `103`: high-QoS USB dispatch queue. Safety failed with hot
  `coreaudiod`. Rejected and recovered.
- `0.3.02` build `104`: HAL 8-channel fast path. Safety passed, but tone
  worsened to `sideband_ratio=0.014470`. Rejected.
- `0.3.03` build `105`: playback queue target `32`. Safety passed, but tone
  worsened to `sideband_ratio=0.015350`. Rejected.

Current loaded candidate:

- `0.3.00` build `102`.
- Change from `0.2.95`: keeps the same audio transport and HAL contract, but
  batches stream-stat updates inside the isochronous capture/playback hot path
  and batches output-fill stats every 16 playback transfers. This reduces mutex
  churn without changing sample packing, byte order, gain, ISO cadence,
  `GetZeroTimeStamp`, or public Core Audio layout.
- Installed hash:
  `5c9fc079f5b7ed0c53fd08ff795f180af0c83271317bc8b6f356141cb66ee8b6`.
- Final load:
  `local-analysis/hal-candidate-safety/0300-final2-single-cycle-20260612-222100`
  PASS, left loaded.
- Final guard:
  `local-analysis/audio-stack-guard/final-0300-loaded-20260612-222129`
  PASS, `coreaudiod=0.0%`, driver idle `0.0%`, Core Audio enumeration PASS.
- Best tone observed on this build:
  `local-analysis/iteration-0300-output-stats-batch-tone-20260612-220946`
  had `sideband_ratio=0.003578`, strongest sideband `-50.20 dB`.
- Final confirmation tone after reload:
  `local-analysis/final-0300-confirm-tone-20260612-221848`
  had `sideband_ratio=0.009465`, strongest sideband `-42.09 dB`.
- Music with monitor:
  `local-analysis/iteration-0300-output-stats-batch-music-20260612-221017`
  had `mid_band_residual_ratio=1.434729`,
  `high_band_residual_ratio=1.364830`, `click_outliers=0`,
  `mid_band_cpu_corr=0.184821`, no clipping.
- Music without monitor:
  `local-analysis/iteration-0300-output-stats-batch-no-monitor-20260612-221110`
  had `mid_band_residual_ratio=1.427153`,
  `high_band_residual_ratio=1.361109`, `click_outliers=106`,
  `mid_band_cpu_corr=0.150972`, no clipping.

Conclusion:

- `0.3.00` is the best safe result from this pass because it sometimes
  materially lowers the 1 kHz sideband gate and remains cold/stable after final
  load.
- It is not yet a high-fidelity/audiophile result: music residual metrics remain
  in the same broad range as `0.2.95`, and click metrics are still variable.
- No email was sent because this pass did not produce a clearly high-fi
  candidate. `0.3.00` is left loaded only as the best current candidate for
  further automated work or optional human spot-checking.

## 0.3.24 follow-up - capture-paced lead experiment and final ISO5 candidate

Context:

- The previous exact `0.3.00` control bundle failed safety in the current system
  state with hot `coreaudiod`, so it was not left installed.
- The active system was recovered to a cold Core Audio stack before rebuilding.
- The implementation remains macOS-first: no `GetZeroTimeStamp` changes, no USB
  clock anchor, no explicit isochronous scheduling, and no public HAL contract
  changes.

Implemented source change:

- Added `HAL_CAPTURE_PACED_OUT_LEAD` / `OPENA8DJ_CAPTURE_PACED_OUT_LEAD` as a
  transport-only experiment. The default is `1`, which preserves existing
  capture-paced behavior. Values above `1` attempt to keep additional OUT
  transfers in flight from observed capture transaction layout.
- Version bumped to `0.3.24`.

Rejected variants:

- `0.3.24 lead=2`: safety passed, but physical tone was catastrophically wrong:
  `local-analysis/iteration-0324-lead2-tone-20260613-004317` measured
  `sideband_ratio=2.322885`, `click_outliers=1557`, and the 1 kHz fundamental
  collapsed. Conclusion: do not prequeue extra OUT transfers with
  `firstFrameNumber=0`; it disturbs device cadence.
- `0.3.24 no amplitude stats`: safety passed but tone did not improve:
  `local-analysis/iteration-0324-noampstats-tone-20260613-004820` measured
  `sideband_ratio=0.005842`, `click_outliers=95`. Rejected.
- `0.3.24 capture queue 32`: safety passed and tone was acceptable, but music
  did not improve: `local-analysis/iteration-0324-capture32-music-20260613-004941`
  measured `mid_band_residual_ratio=1.432187`, `click_outliers=2`,
  `mid_band_cpu_corr=0.144970`. Rejected as not clearly better than queue 64.
- `0.3.24 ISO4`: safety passed but tone worsened relative to ISO5:
  `local-analysis/iteration-0324-iso4-tone-20260613-005103` measured
  `sideband_ratio=0.008418`, `click_outliers=64`. Rejected.

Current loaded candidate:

- Build: `0.3.24`, ISO5, capture queue 64, transfer pool on, capture-paced OUT,
  capture-paced lead `1`, amplitude stats on, USB clock anchor off, valid
  capture OUT layout off, gain `0.50`.
- Binary hash:
  `78def449f2ad963d0d481fdb2eedf386c4d4c8d1d08ebc61b6c3543b0c3454cd`.
- Final safety:
  `local-analysis/hal-candidate-safety/0324-final-iso5-clean-20260613-005346`
  PASS for two cycles and left loaded.
- Final tone:
  `local-analysis/final-0324-iso5-tone-20260613-005429` measured
  `sideband_ratio=0.008407`, strongest sideband `-43.70 dB`,
  `click_outliers=66`, no clipping, post-tone guard PASS.
- Best same-build tone after restoring lead 1:
  `local-analysis/iteration-0324-poststress-repeat-tone-20260613-004705`
  measured `sideband_ratio=0.004942`, strongest sideband `-48.74 dB`,
  `click_outliers=46`.
- Final music:
  `local-analysis/final-0324-iso5-music-20260613-005459` measured
  `mid_band_residual_ratio=1.419853`, `high_band_residual_ratio=1.359114`,
  `mid_band_cpu_corr=0.124710`, `click_outliers=4`, no clipping.
- Final music repeat:
  `local-analysis/final-0324-iso5-music-repeat-20260613-005553` measured
  `mid_band_residual_ratio=1.434795`, `high_band_residual_ratio=1.364932`,
  `mid_band_cpu_corr=0.213122`, `click_outliers=0`, no clipping.
- Final guard:
  `local-analysis/audio-stack-guard/final-0324-loaded-20260613-005646` PASS,
  OpenA8DJ loaded, `coreaudiod=0.0%`, OpenA8DJ driver `0.0%`, Core Audio
  enumeration PASS.

Conclusion:

- `0.3.24` ISO5/lead1 is the safest loaded candidate from this retry. It improves
  the physical 1 kHz sideband gate back into the good historical range and keeps
  CPU cold. Music residual remains in the same broad analog-loopback range as
  `0.2.95`/`0.3.00`, and clicks remain variable, so this is a listening
  candidate rather than a final audiophile signoff.
- An email notification was sent to `fernandosanchezmunoz@gmail.com` after the
  candidate was left loaded.
