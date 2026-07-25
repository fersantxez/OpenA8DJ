# OpenA8DJ Quality Runs - 2026-06-13

Rule after the rejected ISO16 load: internal counters, CPU, and underrun checks
are necessary but not sufficient. A candidate is not eligible for human
listening unless the physical iRig tone/music gate passes and is numerically
better than the current listening baseline.

Current listening baseline:

| Metric | Baseline |
| --- | ---: |
| build | final-0324 / ISO5 |
| tone fundamental | -15.26 dBFS |
| tone sideband ratio | 0.008407 |
| strongest sideband | -43.70 dB |
| tone residual ratio | 0.431691 |
| music mid residual ratio | 1.434795 |
| music high residual ratio | 1.364932 |
| music quiet mid noise | -33.91 dBFS |
| music CPU/noise correlation | 0.213122 |

Hard blocks:

- Physical tone fundamental below -45 dBFS.
- Physical tone peak below 0.020000.
- Tone sideband ratio not at least 5% better than baseline.
- Strongest sideband not at least 1 dB better than baseline.
- Tone residual ratio not better than baseline.
- Optional music gate not better than baseline in mid/high residuals, quiet
  mid-band noise, CPU/noise correlation, lag jumps, and clipping.

## 2026-06-13T11:01:15-04:00 - 0.3.25-iso16-rejected-human-metallic-bass

status=FAIL
quality_score=0.00
tone_score=0.00
notes=Human listening rejected: Spotify sounded very metallic/noisy, bass saturated. This run must never be treated as pass.

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -89.54 dBFS | -15.26 dBFS |
| tone peak | 0.001541 | 0.273651 |
| tone sideband ratio | 0.047597 | 0.008407 |
| strongest sideband | -26.86 dB | -43.70 dB |
| tone residual ratio | 0.990972 | 0.431691 |
| tone clicks | 0 | 66 |

failures:
- tone fundamental too low for physical QA: -89.54 dBFS < -45.00 dBFS
- tone capture peak too low for physical QA: 0.001541 < 0.020000
- tone sideband ratio not better than baseline: 0.047597 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -26.86 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.990972 > 0.423057

tone_summary=local-analysis/final-iso16-irig-tone-20260613-103826/tone-summary.txt


## 2026-06-13T11:03:35-04:00 - rollback-iso5-baseline-route-check

status=FAIL
quality_score=0.00
tone_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/rollback-iso5-baseline-route-check-20260613-110254

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -87.87 dBFS | -15.26 dBFS |
| tone peak | 0.001633 | 0.273651 |
| tone sideband ratio | 0.056028 | 0.008407 |
| strongest sideband | -24.76 dB | -43.70 dB |
| tone residual ratio | 0.987180 | 0.431691 |
| tone clicks | 0 | 66 |

failures:
- tone fundamental too low for physical QA: -87.87 dBFS < -45.00 dBFS
- tone capture peak too low for physical QA: 0.001633 < 0.020000
- tone sideband ratio not better than baseline: 0.056028 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -24.76 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.987180 > 0.423057

tone_summary=local-analysis/listen-gate/rollback-iso5-baseline-route-check-20260613-110254/tone-summary.txt


## 2026-06-13T11:37:45-04:00 - baseline-iso5-after-volume-route-check

status=FAIL
quality_score=53.56
tone_score=53.56
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/baseline-iso5-after-volume-route-check-20260613-113706

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -15.51 dBFS | -15.26 dBFS |
| tone peak | 0.251221 | 0.273651 |
| tone sideband ratio | 0.010533 | 0.008407 |
| strongest sideband | -39.92 dB | -43.70 dB |
| tone residual ratio | 0.469935 | 0.431691 |
| tone clicks | 53 | 66 |

failures:
- tone sideband ratio not better than baseline: 0.010533 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -39.92 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.469935 > 0.423057
- tone click outliers too high: 53 > 8

tone_summary=local-analysis/listen-gate/baseline-iso5-after-volume-route-check-20260613-113706/tone-summary.txt


## 2026-06-13T11:43:46-04:00 - inputstats-batch

status=FAIL
quality_score=0.00
tone_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/inputstats-batch-20260613-114301

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -17.70 dBFS | -15.26 dBFS |
| tone peak | 0.277283 | 0.273651 |
| tone sideband ratio | 0.196384 | 0.008407 |
| strongest sideband | -14.09 dB | -43.70 dB |
| tone residual ratio | 0.721213 | 0.431691 |
| tone clicks | 582 | 66 |

failures:
- tone sideband ratio not better than baseline: 0.196384 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -14.09 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.721213 > 0.423057
- tone click outliers too high: 582 > 8

tone_summary=local-analysis/listen-gate/inputstats-batch-20260613-114301/tone-summary.txt


## 2026-06-13T11:47:04-04:00 - valid-capture-out-layout

status=FAIL
quality_score=0.00
tone_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/valid-layout-20260613-114619

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -18.06 dBFS | -15.26 dBFS |
| tone peak | 0.278854 | 0.273651 |
| tone sideband ratio | 0.236064 | 0.008407 |
| strongest sideband | -12.60 dB | -43.70 dB |
| tone residual ratio | 0.747304 | 0.431691 |
| tone clicks | 591 | 66 |

failures:
- tone sideband ratio not better than baseline: 0.236064 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -12.60 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.747304 > 0.423057
- tone click outliers too high: 591 > 8

tone_summary=local-analysis/listen-gate/valid-layout-20260613-114619/tone-summary.txt


## 2026-06-13T11:48:55-04:00 - capture-queue-32

status=FAIL
quality_score=0.00
tone_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/captureq32-20260613-114809

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -17.75 dBFS | -15.26 dBFS |
| tone peak | 0.272263 | 0.273651 |
| tone sideband ratio | 0.203621 | 0.008407 |
| strongest sideband | -13.51 dB | -43.70 dB |
| tone residual ratio | 0.725474 | 0.431691 |
| tone clicks | 609 | 66 |

failures:
- tone sideband ratio not better than baseline: 0.203621 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -13.51 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.725474 > 0.423057
- tone click outliers too high: 609 > 8

tone_summary=local-analysis/listen-gate/captureq32-20260613-114809/tone-summary.txt


## 2026-06-13T11:50:29-04:00 - rollback-after-captureq32-baseline-check

status=FAIL
quality_score=0.00
tone_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/rollback-after-captureq32-baseline-check-20260613-114943

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -17.58 dBFS | -15.26 dBFS |
| tone peak | 0.284302 | 0.273651 |
| tone sideband ratio | 0.182539 | 0.008407 |
| strongest sideband | -14.71 dB | -43.70 dB |
| tone residual ratio | 0.713343 | 0.431691 |
| tone clicks | 551 | 66 |

failures:
- tone sideband ratio not better than baseline: 0.182539 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -14.71 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.713343 > 0.423057
- tone click outliers too high: 551 > 8

tone_summary=local-analysis/listen-gate/rollback-after-captureq32-baseline-check-20260613-114943/tone-summary.txt


## 2026-06-13T11:59:14-04:00 - baseline-wav-gate-calibration

status=FAIL
quality_score=0.00
tone_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/baseline-wav-gate-calibration-20260613-115823

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -17.38 dBFS | -15.26 dBFS |
| tone peak | 0.270920 | 0.273651 |
| tone sideband ratio | 0.105770 | 0.008407 |
| strongest sideband | -19.96 dB | -43.70 dB |
| tone residual ratio | 0.674681 | 0.431691 |
| tone clicks | 777 | 66 |

failures:
- tone sideband ratio not better than baseline: 0.105770 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -19.96 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.674681 > 0.423057
- tone click outliers too high: 777 > 8

tone_summary=local-analysis/listen-gate/baseline-wav-gate-calibration-20260613-115823/tone-summary.txt


## 2026-06-13T12:32:17-04:00 - hotpath-pack-after-irig-recovery

status=FAIL
quality_score=0.00
tone_score=0.00
music_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/hotpath-pack-after-irig-recovery-20260613-123028

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -17.56 dBFS | -15.26 dBFS |
| tone peak | 0.269501 | 0.273651 |
| tone sideband ratio | 0.169365 | 0.008407 |
| strongest sideband | -15.98 dB | -43.70 dB |
| tone residual ratio | 0.714214 | 0.431691 |
| tone clicks | 544 | 66 |
| music alignment | 0.970659 | 0.924395 |
| music mid residual | 1.436186 | 1.434795 |
| music high residual | 1.355266 | 1.364932 |
| quiet mid noise | -33.85 dBFS | -33.91 dBFS |
| CPU/noise corr | 999.000000 | 999.000000 |
| lag jumps | 45 | 42 |

failures:
- tone sideband ratio not better than baseline: 0.169365 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -15.98 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.714214 > 0.423057
- tone click outliers too high: 544 > 8
- mid-band residual not better than baseline: 1.436186 > 1.363055
- high-band residual not better than baseline: 1.355266 > 1.296685
- quiet mid-band noise not at least 1 dB better than baseline: -33.85 dBFS > -34.91 dBFS
- CPU/noise correlation too high: 999.000000 > 0.200000
- lag jumps not better than baseline: 45 > 31
- click outliers too high: 444 > 0

tone_summary=local-analysis/listen-gate/hotpath-pack-after-irig-recovery-20260613-123028/tone-summary.txt
music_summary=local-analysis/listen-gate/hotpath-pack-after-irig-recovery-20260613-123028/music/summary.txt


## 2026-06-13T12:36:09-04:00 - output-only-0324-shape-tone

status=FAIL
quality_score=0.00
tone_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/output-only-0324-shape-tone-20260613-123518

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -17.16 dBFS | -15.26 dBFS |
| tone peak | 0.277588 | 0.273651 |
| tone sideband ratio | 0.173434 | 0.008407 |
| strongest sideband | -15.51 dB | -43.70 dB |
| tone residual ratio | 0.680375 | 0.431691 |
| tone clicks | 546 | 66 |

failures:
- tone sideband ratio not better than baseline: 0.173434 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -15.51 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.680375 > 0.423057
- tone click outliers too high: 546 > 8

tone_summary=local-analysis/listen-gate/output-only-0324-shape-tone-20260613-123518/tone-summary.txt


## 2026-06-13T12:40:20-04:00 - output-only-old-packer-tone

status=FAIL
quality_score=0.00
tone_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/output-only-old-packer-tone-20260613-123929

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -17.26 dBFS | -15.26 dBFS |
| tone peak | 0.277267 | 0.273651 |
| tone sideband ratio | 0.159522 | 0.008407 |
| strongest sideband | -16.14 dB | -43.70 dB |
| tone residual ratio | 0.690444 | 0.431691 |
| tone clicks | 430 | 66 |

failures:
- tone sideband ratio not better than baseline: 0.159522 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -16.14 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.690444 > 0.423057
- tone click outliers too high: 430 > 8

tone_summary=local-analysis/listen-gate/output-only-old-packer-tone-20260613-123929/tone-summary.txt


## 2026-06-13T12:46:20-04:00 - v0324-tag-control-tone-rerun

status=FAIL
quality_score=0.00
tone_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/v0324-tag-control-tone-rerun-20260613-124525

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -17.44 dBFS | -15.26 dBFS |
| tone peak | 0.283020 | 0.273651 |
| tone sideband ratio | 0.184246 | 0.008407 |
| strongest sideband | -14.93 dB | -43.70 dB |
| tone residual ratio | 0.704511 | 0.431691 |
| tone clicks | 497 | 66 |

failures:
- tone sideband ratio not better than baseline: 0.184246 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -14.93 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.704511 > 0.423057
- tone click outliers too high: 497 > 8

tone_summary=local-analysis/listen-gate/v0324-tag-control-tone-rerun-20260613-124525/tone-summary.txt


## 2026-06-13T12:49:37-04:00 - v0324-warm-repeat-tone

status=FAIL
quality_score=0.00
tone_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/v0324-warm-repeat-tone-20260613-124846

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -17.48 dBFS | -15.26 dBFS |
| tone peak | 0.274567 | 0.273651 |
| tone sideband ratio | 0.162547 | 0.008407 |
| strongest sideband | -16.01 dB | -43.70 dB |
| tone residual ratio | 0.708705 | 0.431691 |
| tone clicks | 519 | 66 |

failures:
- tone sideband ratio not better than baseline: 0.162547 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -16.01 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.708705 > 0.423057
- tone click outliers too high: 519 > 8

tone_summary=local-analysis/listen-gate/v0324-warm-repeat-tone-20260613-124846/tone-summary.txt


## 2026-06-13T12:56:37-04:00 - capture32-output1-tone

status=FAIL
quality_score=0.00
tone_score=0.00
notes=candidate-listen-gate run_dir=local-analysis/listen-gate/capture32-output1-tone-20260613-125542

| Metric | Candidate | Baseline |
| --- | ---: | ---: |
| tone fundamental | -18.24 dBFS | -15.26 dBFS |
| tone peak | 0.283691 | 0.273651 |
| tone sideband ratio | 0.148349 | 0.008407 |
| strongest sideband | -16.33 dB | -43.70 dB |
| tone residual ratio | 0.759216 | 0.431691 |
| tone clicks | 803 | 66 |

failures:
- tone sideband ratio not better than baseline: 0.148349 > 0.007987
- strongest sideband not at least 1 dB better than baseline: -16.33 dB > -44.70 dB
- tone residual ratio not better than baseline: 0.759216 > 0.423057
- tone click outliers too high: 803 > 8

tone_summary=local-analysis/listen-gate/capture32-output1-tone-20260613-125542/tone-summary.txt


## 2026-06-13T13:54:21-04:00 - candidate-0.3.35-lazy-input-inline-decode

status=TECHNICAL_CANDIDATE_LOADED
installed_hash=de6089e9e7086ae0cd5dea844ceb71b828653c1dee66619a55a0ccdf0c3b82d2
installed_bundle_version=127
guard=PASS
guard_dir=local-analysis/audio-stack-guard/guard-0.3.35-final-20260613-135421

Changes:
- Kept macOS Core Audio timeline stable; no USB GetZeroTimeStamp anchoring.
- Kept public topology at one 8-channel input stream and one 8-channel output stream.
- Added runtime lazy input decode: inputs stay visible, but USB input byte decode is inactive until Core Audio asks for input.
- Inlined the active input byte append path to remove per-byte Objective-C dispatch while preserving the existing mode-2 byte order.
- Rejected ISO16 due repeated coreaudiod/mediaremoted guard failures.
- Rejected ISO12 despite lower CPU because 1-5 kHz capture-to-reference gain shifted +2.89 dB vs accepted physical baseline.
- Rejected ISO10 because CPU did not improve and old music gate got worse.
- Rejected no-amplitude-stats / coldstats variants because CPU did not improve.

Playback-only physical music gate:
- run_dir=local-analysis/soundcheck/candidate-0.3.35-playback-only-20260613-135222
- legacy soundcheck: FAIL due absolute analog thresholds, but clicks=0, capture_clipped_frames=0, analog_snr_db=1.28.
- relative music comparator: PASS
- snr_db=-5.6461
- mid_residual_ratio=2.5135
- high_residual_ratio=2.3078
- mid_capture_to_ref_gain_db=6.1731
- high_capture_to_ref_gain_db=5.2570
- capture_peak_db=-8.2104
- capture_clipped_samples=0
- CPU profile: opena8dj_driver avg=25.85 p95=27.3 max=28.1; coreaudiod avg=4.11 p95=4.6 max=87.3.

Duplex/input-active physical music gate:
- run_dir=local-analysis/soundcheck/candidate-0.3.35-duplex-input-active-20260613-135035
- previous 0.3.30 duplex attempt failed preflight guard with opena8dj_driver=30.4%.
- legacy soundcheck: FAIL due absolute analog thresholds, but clicks=0, capture_clipped_frames=0.
- relative music comparator: PASS
- snr_db=-3.1655
- mid_residual_ratio=2.1902
- high_residual_ratio=1.9758
- mid_capture_to_ref_gain_db=6.4470
- high_capture_to_ref_gain_db=5.2757
- capture_peak_db=-7.7792
- capture_clipped_samples=0
- CPU profile: opena8dj_driver avg=21.57 p95=25.5 max=25.7; coreaudiod avg=10.76 p95=38.5 max=84.7.

Decision:
- Leave 0.3.35 loaded for human playback and Traktor smoke testing.
- Do not email release-ready notification yet: playback-only CPU is still higher than desired and Traktor/timecode has not been manually validated.
- Next improvement target is reducing IOUSBHost/callback completion cost or finding a safer ISO grouping than 8 without raising the 1-5 kHz band.

## 2026-06-13T15:07:04-04:00 - candidate-0.3.44-background-warm-open

status=TECHNICAL_CANDIDATE_LOADED
installed_hash=50cbd81505935e5f860a7bffd26799e911d27b16fe74ccd6c9c39e9f40a11c30
installed_bundle_version=0.3.44
guard=PASS
guard_dir=local-analysis/candidate-0.3.44-versioned-final-guard

Problem addressed:
- User reported client play/pause/record/start taking 1-2 seconds or more.
- Timing isolated the cold delay inside `AudioDeviceStart` / `StartIO`, not in client setup:
  - cold `device-start=4.323853s`, `first-callback=4.323912s`;
  - after background warm-open `device-start=0.116291s`, `first-callback=0.122482s`.

Changes:
- Added `HAL_BACKGROUND_WARM_OPEN`.
- On final `StopIO`, close the USB stream immediately so the device is not left consuming an empty output timeline.
- Reopen the USB device in a detached background thread without starting isochronous streaming.
- Keep Core Audio `GetZeroTimeStamp` on the stable HAL timeline; no USB clock anchoring.
- Added optional timing output to `audio-wav-play` via `OPENA8DJ_PLAYER_TIMING=1`.
- Makefile now records HAL build flags in `build/hal-build-config.txt` and stamps the HAL `Info.plist` version from `VERSION`.

Rejected during this pass:
- `0.3.37` / partial stop while keeping USB open: rejected after `coreaudiod` rose above 100%.
- ISO12: lower CPU than ISO8 but previously rejected because physical 1-5 kHz gain shifted against the accepted iRig baseline.
- ISO16: lower CPU, but previously rejected by human feedback as metallic/distorted bass and bad physical tone sidebands.
- ISO5 current-source retest: historically good sidebands, but this pass hit driver CPU around 22-30% during music.
- `0.3.43` warm-open with streaming alive during idle: rejected because end-of-playback produced `active-underruns=2542` and `elastic-replays=192`.

Final 0.3.44 checks:
- final guard: PASS; `coreaudiod=0.0%`, `opena8dj_driver=0.0%`, Core Audio enumeration PASS.
- final timing run: `local-analysis/client-start-timing-0.3.44-final-20260613-150533`.
- final music transport run: `local-analysis/music-playback-0.3.44-warm-open-nostream-stats-20260613-150402`.
- music transport during playback: no clipping, no reschedules, no cadence outliers, no active underruns during sampled playback.
- immediate post-playback sample: `active-underruns=0`, `elastic-replays=0`; previous 0.3.43 end-of-stream failure not reproduced.
- after background open: `streaming=no`, all stream counters zero, health PASS, idle CPU 0.0%.

Limitations:
- iRig Stream is not currently visible in Core Audio, so no physical analog capture / residual comparison was possible in this pass.
- This is not a release/audiophile signoff and no email notification was sent.
- Driver CPU during real-music playback remains high for a HAL driver, with samples around 19-26%; the next quality target remains transport CPU/cadence without regressing physical sidebands.

## 2026-06-13T15:56:48-04:00 - candidate-0.3.49-iso10-nowake

status=TECHNICAL_CANDIDATE_LOADED
installed_hash=415441d249775da3a952f4c7d36406bb26f25e9bd4be5abf5d2ef4ed67395b65
installed_bundle_version=0.3.49
guard=PASS
guard_dir=local-analysis/candidate-0.3.49-iso10-nowake-guard

Problem addressed:
- User still reported audible clicks/noise that correlated with CPU/window activity.
- `0.3.44` had clean transport counters but playback CPU was still high, around 19-26%.
- The old monitor could create false post-run activity because `opena8dj-control stream-stats`
  woke the HAL bridge when the socket was unavailable.

Changes:
- Added output timeline `late-write-frames` and `late-write-batches` counters.
- Optimized mode-2 OUT packing to write full 16-byte groups directly while preserving
  byte order, check-byte pattern, `OPENA8DJ_OUTPUT_START_BYTE=4`, and stable HAL time.
- Added `OPENA8DJ_CONTROL_NO_WAKE=1` to `opena8dj-control`, and updated
  `monitor-active-stream` to read stream stats without starting Core Audio.
- Kept public topology at 8 inputs and 8 outputs; Traktor/timecode streams remain visible.
- Kept background warm-open from `0.3.44`.
- Built with ISO10, transfer pool, capture-paced OUT, no USB GetZeroTimeStamp anchoring,
  no hot stream stats, and no output amplitude stats.

Intermediate decisions:
- `0.3.45` (ISO8 + optimized packer + late-write counters) passed transport checks but
  CPU did not improve enough: driver CPU active avg about 25.06%, max 26.6%.
- `0.3.46` (ISO8 + hot stats off) also did not improve enough: driver CPU avg 23.02%,
  max 27.0%.
- `0.3.47` (ISO10) reduced CPU materially: music avg 19.78%, max 23.1%; stress avg
  19.89%, max 21.8%; no underruns/drops/replays/resets/late-writes.
- `0.3.48` (ISO9) was cleaner than ISO8 but weaker than ISO10: music avg 21.42%,
  max 26.5%; rejected for this pass.
- Note: ISO10 had been rejected earlier in a different branch because CPU did not improve
  and one old physical music gate got worse. In this pass ISO10 is only a technical
  CPU/click candidate; it still requires iRig physical confirmation before release.

Final 0.3.49 checks:
- final guard PASS; idle `coreaudiod=0.0%`, `opena8dj_driver=0.0%`.
- long real-music gate:
  `local-analysis/music-playback-0.3.49-iso10-nowake-monitor-20260613-155433`.
- music gate result: `monitor_rc=0`, `play_rc=0`, `ACTIVE STREAM MONITOR: PASS_CAPTURE`.
- output counters during music: `active-underruns=0`, `elastic-drops=0`,
  `elastic-replays=0`, `timeline-resets=0`, `late-write-frames=0`,
  `late-write-batches=0`.
- output read/write rate: avg about 48007 frames/s written and 48007 frames/s read.
- CPU during long music gate: driver avg 18.5%, max 23.4%; coreaudiod avg 1.3%.
- warm-start timing:
  `local-analysis/client-start-timing-0.3.49-warm-20260613-155636`.
  `device-start=0.083152s`, `first-callback=0.089192s`.
- duplex/timecode smoke:
  `audio-io-test 2 48000` passed with 96256 input frames and 96256 output frames.
  `audio-input-meter 2` saw all four input pairs advancing for 96256 frames.
  `opena8dj-control profile timecode-vinyl` applied successfully.
- window-switch stress:
  `local-analysis/music-playback-0.3.49-window-switch-stress-20260613-155806`.
  The osascript focus-switcher became a CPU contaminant and was killed, but the
  playback itself completed: driver avg 19.19%, max 22.5%, health PASS, and all
  sampled output anomaly counters stayed at zero.
- simulated output sanity:
  ISO8/ISO9/ISO10 all passed software decode with alignment 1.000000, SNR 74.10 dB,
  and mid-band residual ratio 0.000455 when using their real transfer sizes.

Limitations:
- iRig Stream is still not visible in Core Audio in this pass, so there is no decisive
  physical analog capture for sidebands/residual/noise.
- Do not email release-ready notification yet.
- Do not publish as audiophile/release build until iRig physical tone and music gates
  confirm that ISO10 did not reintroduce the earlier physical color/noise regression.

## 2026-06-13T22:08:13-04:00 - candidate-0.3.55-inputdecode-playback-iso12

status=TECHNICAL_CANDIDATE_LOADED
installed_hash=055017a20b2c865ec0deb1de20d1752d6611adf5d48be74fad05cf2f820afe2d
installed_bundle_version=0.3.55
final_loaded_profile=playback
final_control_state=input-decode-off
final_guard=PASS
final_guard_dir=local-analysis/audio-stack-guard/after-rollback-0.3.55-20260613-220532

Problem addressed:
- Profiling of `0.3.51` showed that Core Audio called the input path during
  output-only playback, so the driver was decoding input while VLC/Spotify-style
  clients only needed output.
- This matched user feedback: CPU-correlated noise/clicks, worse behavior when
  the laptop microphone/input was active, and clean output counters that still
  did not explain audible artifacts.

Changes:
- Added explicit input decode policy to the HAL/control payload:
  - `profile playback|output-only|spotify|vlc` sets `input-decode=off`.
  - `profile timecode-vinyl|timecode-cd-line|phono` sets `input-decode=on`.
- `ReadInput` and `PrepareInputCycle` no longer force decode unless the policy
  enables it.
- When decode is disabled, Audio 8 DJ input reads are zero-filled and the input
  ring is cleared; physical input streams remain visible to Core Audio.
- The decode policy is persisted inside the HAL process so it survives normal
  client open/close cycles.
- Kept the macOS public timeline stable: no USB GetZeroTimeStamp anchoring.
- Kept capture-paced OUT and transfer pool; this is a macOS transport change,
  not a Linux/ALSA translation.

Intermediate checks and decisions:
- `0.3.50` input-stats batching: music PASS, all sampled anomalies zero,
  driver CPU avg 17.2%, max 22.4%.
- `0.3.51` output dispatch fast-path: music PASS, all sampled anomalies zero,
  but driver CPU avg 17.8%, max 22.6%; not a playback win over `0.3.50`.
- `0.3.52` first input-decode-off experiment: playback CPU dropped to avg 12.0%,
  max 20.1%, with zero input frames in playback. It exposed that timecode decode
  policy needed to persist across HAL bridge unload/reload.
- `0.3.53` persisted the decode policy and validated both sides:
  - playback music PASS, driver CPU avg 11.7%, max 22.4%;
  - MacBook microphone + playback PASS, driver CPU avg 11.7%, max 20.2%;
  - timecode/input-active stats advanced while active.
- `0.3.54` ISO16 reduced playback CPU further to avg 8.7%, max 14.9%, but
  `output_read_rate` jitter rose to cv 0.043. It is not selected without physical
  iRig proof because the extra burstiness may be audible as clicks.
- `0.3.56` ISO14 is rejected: post-load health remained FAIL on the second read
  with `coreaudiod` around 68.9% and `mediaremoted` around 46.9%. Recovery
  required `audio-stack-guard --recover --unload-opena8dj`; do not retest ISO14
  without a new hypothesis.

Final selected build:
- `0.3.55` ISO12, input-decode policy, hot stream stats off, output amplitude
  stats off, background warm-open on.
- final post-recovery music gate:
  `local-analysis/music-playback-0.3.55-post-recovery-final-20260613-220610`.
- final music result: `ACTIVE STREAM MONITOR: PASS_CAPTURE`, `play_status=0`,
  `monitor_status=0`, post-run `audio_stack_health=PASS`.
- final output counters: `active-underruns=0`, `elastic-drops=0`,
  `elastic-replays=0`, `timeline-resets=0`, `late-write-frames=0`,
  `late-write-batches=0`.
- final playback CPU: driver avg 10.2%, max 18.0%; coreaudiod avg 1.2%,
  max 15.2%.
- final output read rate: avg 48011.9/s, p95 50134.2/s, stdev 1524.2,
  cv 0.032.
- final startup timing: `device-start=0.106773s`, `first-callback=0.114319s`.
- final playback input isolation: all four Audio 8 DJ input stats stayed at
  zero frames during playback.
- final timecode/input-active smoke:
  `local-analysis/input-decode-0.3.55-final-timecode-20260613-220800`.
  During active input, stats advanced for all four input pairs; after the smoke,
  profile was returned to playback and health stayed PASS.

Limitations:
- This is the best technical candidate from the input-decode pass, but still
  not a final audiophile/release signoff.
- iRig Stream was later recovered in Core Audio and physical analog captures
  became possible again; those captures show that this build is stable enough
  to keep as rollback/baseline, but it still fails the stricter physical music
  quality gate because 1-5 kHz residual varies too much by window.
- Do not send the "driver ready" email yet.
- Before human listening or release, restore physical capture and run iRig tone
  and music gates against this exact installed hash.

## 2026-06-13 late physical capture recovery and ISO13 rejection

Context:
- User confirmed iRig was physically plugged in after Core Audio stopped listing
  it.
- USB tree still showed `iRig Stream` from IK Multimedia, but Core Audio did not
  publish it until the USB-audio/Core Audio daemons were restarted.

Recovery:
- `iRig Stream` recovered as a Core Audio device with 2 input and 2 output
  channels at 48000 Hz.
- Audio 8 DJ remained visible as 8 input / 8 output at 48000 Hz.
- post-recovery capture-open proof:
  `local-analysis/irig-recovered-capture-20260613-223305`.
  Result: iRig record opened successfully, rate 48000, channels `1,2`, no
  clipping; idle RMS was low as expected before playback.

0.3.55 physical A/B gate:
- run: `local-analysis/physical-pair-matrix-0.3.55-irig-20260613-223656`.
- A and B are the physical output pairs that reach mixer REC OUT; C and D are
  effectively not on the current REC OUT route.
- Pair A:
  - `quality_alignment_score=0.938824`
  - `mid_band_window_residual_ratio_p95=2.383221`
  - `mid_band_window_residual_ratio_p95_over_median=1.348574`
  - `mid_band_cpu_corr_max=0.508789` (`coreaudiod`)
  - driver CPU avg `15.46%`, p95 `17.50%`
  - verdict: FAIL
- Pair B:
  - `quality_alignment_score=0.958334`
  - `mid_band_window_residual_ratio_p95=2.523897`
  - `mid_band_window_residual_ratio_p95_over_median=1.391384`
  - `mid_band_cpu_corr_max=0.337029`
  - driver CPU avg `15.34%`, p95 `16.80%`
  - verdict: FAIL

Gate update:
- `scripts/physical-music-quality-gate` now reports:
  - `mid_band_window_residual_ratio_p95_over_median`
  - `mid_band_window_residual_ratio_max_over_median`
  - driver/coreaudiod CPU avg, p95, and max from `cpu-profile.tsv`
- This is intended to separate fixed analog coloration from window-to-window
  noise/click instability and CPU-coupled residual.

Rejected experiment:
- `0.3.57` changed only ISO transfer size from 12 to 13, keeping the 0.3.55
  behavior otherwise.
- safety load: PASS after a forced Core Audio reload.
- run: `local-analysis/physical-pair-ab-0.3.57-iso13-irig-20260613-224348`.
- Pair A worsened vs 0.3.55:
  - `quality_alignment_score=0.929252`
  - `mid_band_window_residual_ratio_p95=2.630192`
  - `mid_band_window_residual_ratio_p95_over_median=1.434244`
  - `mid_band_cpu_corr_max=0.351839`
  - driver CPU avg `14.43%`, p95 `16.59%`
  - verdict: FAIL
- Pair B worsened vs 0.3.55:
  - `quality_alignment_score=0.960848`
  - `mid_band_window_residual_ratio_p95=2.751299`
  - `mid_band_window_residual_ratio_p95_over_median=1.481934`
  - `mid_band_cpu_corr_max=0.385233`
  - driver CPU avg `14.88%`, p95 `16.23%`
  - verdict: FAIL
- Decision: reject ISO13. It lowers CPU slightly, but worsens physical
  mid-band instability and does not solve clicks/noise.

Rollback:
- restored installed driver to 0.3.55 hash
  `055017a20b2c865ec0deb1de20d1752d6611adf5d48be74fad05cf2f820afe2d`.
- rollback run: `local-analysis/rollback-to-0.3.55-after-iso13-20260613-224510`.
- post-rollback audio stack guard: PASS; loaded driver PID changed, confirming
  Core Audio actually reloaded the rollback bundle.

Important decision:
- Do not reopen `IOProcStreamUsage`/flush-on-any-stream as the next quick test.
  Earlier project notes explicitly rejected that path after it produced mixed
  partial frames when Core Audio called `EndIOOperation` per stream.

## 2026-06-13 late transport CPU/quality matrix

Context:
- After iRig recovery, physical A/B testing showed a hard tradeoff:
  lower ISO grouping keeps the 1-5 kHz residual/spread cleaner but costs too
  much CPU; higher ISO grouping lowers CPU but brings back physical noise/clicks.
- Capture level was corrected from `--target-peak-db -6` to `-12` or lower after
  several runs showed iRig clipping near full scale. Runs with clipping are not
  accepted as fidelity proof.
- The decisive route remains `Open Audio 8 DJ -> mixer REC OUT -> iRig Stream`.

Rejected 0.3.58 / 0.3.59 / 0.3.60 / 0.3.61 matrix:
- `0.3.58` ISO12, no background warm-open:
  `local-analysis/physical-pair-ab-0.3.58-no-bg-warm-irig-20260613-225120`.
  Client start latency was fixed, but physical A/B still failed with mid-band
  residual around `1.80-1.82`, p95 around `2.46`, and driver CPU around `15-16%`.
- `0.3.59` ISO5, no background warm-open:
  `local-analysis/physical-pair-ab-0.3.59-iso5-no-bg-warm-irig-20260613-225415`.
  Physical residual improved materially (`mid_band_residual_ratio=1.40-1.41`,
  p95/median around `1.084`) but driver CPU was about `30%` average / `33%` p95.
- `0.3.60` ISO6, first pass at `-6 dB`:
  `local-analysis/physical-pair-ab-0.3.60-iso6-pool-no-bg-warm-irig-20260613-225920`.
  Rejected because both pairs clipped iRig and produced heavy clicks. The run is
  not a final ISO6 fidelity verdict because capture level was too high.
- `0.3.61` ISO5 with capture queue 32:
  `local-analysis/physical-pair-ab-0.3.61-iso5-capture32-no-bg-warm-irig-minus12-20260613-230703`.
  At the corrected level it did not clip, but still failed with driver CPU
  around `25-26%` and clicks around `9-12/s`. Capture queue 32 is not the fix.

Rejected implementation experiments:
- `0.3.62` O(1) transfer freelist:
  `local-analysis/candidate-0.3.62-iso5-capture32-o1pool-no-bg-warm-20260613-230428`.
  Rejected at safety before audio: `coreaudiod` rose to about `100%`. The guard
  recovered by unloading the candidate. Do not use this freelist approach without
  a new lifetime/IOUSBHost hypothesis.
- `0.3.63` capture-paced OUT coalescing by 2:
  `local-analysis/physical-pair-ab-0.3.63-coalesce2-irig-minus12-20260613-231612`.
  This cut driver CPU to about `14-15%`, but physical output nearly disappeared
  (`capture_peak` around `0.001`, alignment around `0.097`). The Audio 8 DJ does
  not tolerate coalesced OUT transfers here; normal OUT cadence appears required.
- `0.3.67` reusable isochronous completion handlers:
  `local-analysis/physical-smoke-0.3.67-reuse-completions-20260613-232924` and
  `local-analysis/physical-smoke-0.3.67-reuse-completions-minus16-20260613-233017`.
  At lower level clicks disappeared, but driver p95 stayed around `32%` and
  `coreaudiod` p95 around `67%`; the optimization is not useful for a candidate.

Corrected-level ISO retests:
- `0.3.65` ISO6 at `-12 dB`:
  `local-analysis/physical-pair-ab-0.3.65-iso6-irig-minus12-20260613-232048`.
  Clipping was mostly removed and clicks were low (`0` on A, `6` on B), but
  driver CPU remained high (`23.5-23.6%` average, `28%` p95) and CPU correlation
  still failed.
- `0.3.66` ISO10 at `-12 dB`:
  `local-analysis/physical-pair-ab-0.3.66-iso10-irig-minus12-20260613-232312`.
  Driver CPU finally passed (`16.8-17.1%` average, `20-21%` p95), but physical
  quality regressed: pair A had `44` clicks in 10 s and both pairs failed
  mid-band residual/p95 gates. ISO10 is a CPU win but not a high-fidelity win.

Current loaded recovery:
- `0.3.64` ISO5, capture queue 64, normal OUT cadence, coalescing off, reusable
  completions off, background warm-open off.
- Installed hash:
  `f7c57a5041a5adb19bdc2691eb6e4feeab659a9642d9bf40e826e3d9e3fe7df1`.
- Recovery load:
  `local-analysis/reload-0.3.64-after-reuse-completion-reject-20260613-233103`.
- Physical A/B:
  `local-analysis/physical-pair-ab-0.3.64-iso5-baseline-minus12-20260613-232538`.
  Pair A had good physical quality (`mid_band_residual_ratio=1.389742`,
  p95 `1.542063`, `0` clicks), but driver CPU failed (`avg=26.20%`,
  `p95=32.41%`). Pair B was slightly weaker (`mid_band_residual_ratio=1.424041`,
  p95 `1.591886`, `10` clicks) and also failed CPU.
- Decision: `0.3.64` is the best loaded recovery for physical sound, but it is
  not a high-fidelity/release candidate because CPU remains too high and B still
  has borderline clicks/noise. Do not email release-ready notification.

Conclusion:
- The physical measurements now support the user's subjective report: the issue
  is not only sample packing. It is primarily cadence/cost coupling in the
  IOUSBHost isochronous path.
- Normal ISO5 OUT cadence seems required for acceptable physical output.
- Increasing ISO or coalescing OUT lowers CPU but audibly/physically degrades
  output.
- The next safe research direction is not more ISO guessing; it is either a
  deeper IOUSBHost scheduling/lifetime change that preserves one OUT transfer per
  observed ISO5 capture cadence, or old-driver reverse engineering for how the
  Native Instruments transport achieved lower cost without changing the audible
  cadence.

## 2026-06-13 late gate hardening and input diagnostics

Gate hardening:
- `scripts/physical-music-quality-gate` now treats the iRig physical route and
  CPU profile as mandatory by default, and its default limits are stricter:
  quiet 1-5 kHz residual must be below `-32.5 dBFS`, p95 residual below `1.52`,
  max residual below `1.62`, clicks below `2` total / `0.10/s`, CPU correlation
  below `0.30`, and driver/coreaudiod CPU limits remain enforced.
- `scripts/run-soundcheck` now runs the physical music gate automatically when
  the capture device is iRig and writes `physical-music-gate.txt` plus
  `physical-music-gate.json`. The run summary now reports physical click rate
  and CPU p95 fields.
- Rechecking the known 0.3.64 iRig capture
  `local-analysis/physical-smoke-0.3.64-irig-replug-20260613-233347/A`
  correctly fails the hardened gate:
  `quiet_mid_band_noise_dbfs=-30.929443`, `mid_band_cpu_corr_max=0.458365`,
  driver CPU `23.34/33.71% avg/p95`, and coreaudiod p95 `86.16%`.

0.3.68 experiment:
- Change: in the capture completion callback, queue capture-paced OUT before
  requeueing the next IN transfer. This preserved ISO5, lead=1, normal OUT
  cadence, and Core Audio timestamps.
- Run: `local-analysis/physical-smoke-0.3.68-out-before-in-requeue-20260613-233954/A`.
- Result: physical sound metrics partially improved, with
  `mid_band_residual_ratio=1.383970`, `high_band_residual_ratio=1.354824`,
  `click_outliers_total=0`, and driver CPU avg down to `21.20%`.
- Rejected: still failed `mid_band_window_residual_ratio_max=1.855778`,
  driver CPU p95 `33.26%`, and coreaudiod p95 `91.22%`. The change is useful
  evidence but not a candidate.

0.3.69 experiment:
- Change: attempted a HAL fast path for playback profile where input-decode is
  disabled, keeping streams visible but zero-filling without calling into USB
  input read.
- Safety result: rejected before audio. After loading, `coreaudiod` stayed near
  `100%` and `audio-list` hung. Recovery run:
  `local-analysis/recover-after-0.3.69-audio-list-hang-20260613-234222`.
- Decision: do not retry that exact fast path. It may be conceptually useful,
  but any future input-gating work needs a safer HAL state model and must pass
  enumeration before audio.

0.3.70 diagnostic:
- Change: diagnostic build with Core Audio input streams removed
  (`HAL_INPUT_STREAMS=0`). This cannot be a final candidate because Traktor
  timecode requires inputs.
- Safety: passed and was loaded only for diagnostic testing.
- Run:
  `local-analysis/physical-smoke-0.3.70-no-input-streams-20260613-234655/A`.
- Result: coreaudiod p95 improved materially to `12.40%`, confirming input
  publication/callbacks contribute to Core Audio heat and startup pain.
- Rejected as candidate and rolled back to exact 0.3.64 hash
  `f7c57a5041a5adb19bdc2691eb6e4feeab659a9642d9bf40e826e3d9e3fe7df1`.
- Important: driver CPU did not improve (`26.07% avg`, `31.88% p95`) and
  physical noise still failed (`quiet_mid_band_noise_dbfs=-30.79`,
  `mid_band_cpu_corr_max=0.430619`). Therefore the primary driver CPU problem is
  still the ISO5 IOUSBHost transport cadence/cost, not input decode alone.
