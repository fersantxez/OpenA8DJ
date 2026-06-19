# Test Evidence

This file records reproducible release and validation evidence for the modern
macOS mainline.

## 2026-06-19 09:54 EDT - Signing and notarization release gate

- Commit before changes: `5f6e457`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Hardware touched: no
- Audio/CoreAudio touched: no
- Driver installed/reloaded: no

Commands run:

```sh
bash -n scripts/notarize-release scripts/verify-signed-release
make -n release-signed \
  SIGN_IDENTITY="Developer ID Application: Example Team (TEAMID)" \
  PKG_SIGN_IDENTITY="Developer ID Installer: Example Team (TEAMID)" \
  DMG_SIGN_IDENTITY="Developer ID Application: Example Team (TEAMID)"
make dist
make verify-signed-release
```

Results:

- Script syntax passed.
- Dry-run release path signs the HAL bundle and packaged tools with Developer
  ID Application, signs the PKG with Developer ID Installer, signs the DMG,
  and regenerates checksums.
- `make dist` still succeeds with the local ad-hoc preview signature.
- `make verify-signed-release` intentionally fails on the ad-hoc preview
  artifact, proving the official release gate blocks unsigned/non-Developer ID
  artifacts.

Observed verifier failure:

```text
HAL bundle is not signed with a Developer ID Application certificate.
Signature=adhoc
TeamIdentifier=not set
```

Generated ad-hoc preview artifact hashes from this run:

```text
7580d6efea5693498d572ae448702e364a0e1de43701972c75dcc92d165909c6  build/OpenA8DJ-0.4.0.dmg
4e037f187177a73a2504782bbca4427e89754dfb1ad44a3aaf51edcba01ce78e  build/OpenA8DJ-0.4.0.pkg
db08d6137a49b577003b9d48fa57cf6cb9df109a8ef984e88ce997a2187a4ce7  build/OpenA8DJ-0.4.0-checksums.txt
```

Current external blocker:

```text
0 valid codesigning identities found
No Keychain password item found for profile: OpenA8DJNotary
```

The Apple Developer browser session is authenticated and the Developer Program
enrollment form has been submitted. Apple currently shows:

```text
Thank you for your submission.
We'll review the details you provided and contact you soon.
```

The account owner must wait for Apple acceptance and complete any remaining
payment or verification before Developer ID certificates and notarization
credentials can exist on this machine.

## 2026-06-19 10:28 EDT - Unity gain and Timecode Vinyl input restore

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Hardware touched: yes, with hardware/audio lock
- Driver installed/reloaded: yes, local HAL install only
- GitHub release assets replaced: no

Reason:

- Human Traktor testing reported bass saturation and general output saturation.
- Traktor Timecode Vinyl scope showed no signal.
- Local control state before the fix showed `input-mode: timecode-vinyl` but
  `input-decode: off`.

Changes under test:

```text
HAL_OUTPUT_GAIN default: 1.50f -> 1.00f
gInputDecodeEnabledPreference default: false -> true
```

Commands/evidence:

```sh
make -B hal install-hal
/usr/local/bin/opena8dj-control profile timecode-vinyl
./build/audio-input-meter 5
./scripts/run-soundcheck --skip-build --music-file ".../Cable Guy - Dj Deep (Original Mix).mp3" --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --capture-device "iRig Stream" --capture-channels 1,2
./scripts/run-channel-matrix-gate --run-physical --pair A --rate 48000 --seconds 4 --peak 0.25 --capture-device "iRig Stream" --capture-channels 1,2
```

Evidence directory:

```text
local-analysis/unity-gain-fix-20260619-102845
```

Results:

- Installed HAL hash after unity gain + DVS input fix:
  `3b55a7d11b9efa401208e9addaa7e137ba0ddb329d91d561b22b3e00d883605d`
- `opena8dj-control profile timecode-vinyl` reports `input-decode: on`.
- `audio-input-meter` no longer reports all-zero inputs; A/B/C/D show input
  energy after decode is enabled.
- Unity-gain Cable Guy soundcheck no longer clips the iRig capture:
  `capture_clipped_frames=0`.
- The same music soundcheck still fails quality thresholds, so this is not a
  release-quality claim.
- Unity-gain pair-A tone matrix passes physical channel-separation gate:
  `capture_clipped_frames=0`, `left_to_right_leakage_db=-61.99`,
  `right_to_left_leakage_db=-61.14`.

Conclusion:

- Saturation risk from the previous 1.5x default output gain is reduced.
- Timecode Vinyl silence caused by `input-decode: off` is corrected in the
  loaded local HAL by explicitly enabling the profile and by making decode
  default-on for future loads.
- Do not replace GitHub release assets until the real-music iRig quality gate
  passes.

## 2026-06-19 10:33 EDT - Idle CPU-noise investigation

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Hardware touched: yes, with hardware/audio lock
- Driver installed/reloaded: no
- Playback: no
- Capture: yes, iRig Stream idle capture

Reason:

- Human headphone test reported faint computer/CPU-like background noise in
  silence.

Commands/evidence:

```sh
/usr/local/bin/opena8dj-control stream-stats
./build/audio-record 8 idle-low-load.wav "iRig Stream" 1,2
yes >/dev/null & ...  # temporary CPU load
./build/audio-record 8 idle-high-load.wav "iRig Stream" 1,2
/usr/local/bin/opena8dj-control stream-stats
```

Evidence directory:

```text
local-analysis/unity-gain-fix-20260619-102845/idle-cpu-noise-20260619-103313
```

Observed stream state during reported idle:

```text
outputFramesWritten=0
outputPeak=0.000000
output near-clip=0
output clipped=0
outputActiveUnderruns=0
```

iRig idle capture:

```text
low load:  rms=0.00160015 peak=0.00869751 clipped=0
high load: rms=0.00028737 peak=0.00402832 clipped=0
```

Conclusion:

- The current evidence does not show non-zero digital output samples during
  idle; the HAL reports digital silence.
- The iRig route did not reproduce a CPU-load-correlated noise increase in
  this short capture. The high-load capture was quieter than the low-load
  capture.
- The headphone noise may be analog coupling, USB bus/power/ground noise, or a
  route not captured by the iRig loopback path.
- Safe mitigations already loaded: unity output gain and strict idle digital
  silence. Further mitigation should be a separate low-noise transport
  experiment that reduces idle USB transfer activity only if it does not break
  Timecode Vinyl input capture or fast playback start.

## 2026-06-19 10:41 EDT - Timecode Vinyl low-noise ground-lift A/B

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Hardware touched: yes, with hardware/audio lock
- Driver installed/reloaded: no
- Playback: no
- Capture: no new external capture; Core Audio input meter used

Reason:

- Human headphone test still reported computer/CPU-like noise after idle
  playback traffic was gated off.
- Live stream stats showed playback transfers parked in silence while capture
  stayed active for Timecode Vinyl, so the remaining likely mitigations are
  analog/control-state related or capture-traffic related.

Command/evidence:

```sh
/usr/local/bin/opena8dj-control gnd-vinyl off
./build/audio-input-meter 4
/usr/local/bin/opena8dj-control stream-stats
```

Evidence directory:

```text
local-analysis/cpu-noise-groundlift-ab-20260619-104123
```

Observed state after change:

```text
input-mode: 0 (timecode-vinyl)
gnd-vinyl: off
software-lock: on
input-decode: on
playbackSubmitAttempts: unchanged at 249 during silence
captureSubmitAttempts: still increasing for DVS
```

Input smoke after `gnd-vinyl off`:

```text
Input A: rmsL=0.00057953 rmsR=0.00060759
Input B: rmsL=0.00057249 rmsR=0.00058633
```

Conclusion:

- `gnd-vinyl off` does not disable the Timecode Vinyl input surface.
- Audible improvement still requires human/headphone confirmation because the
  previous iRig idle route did not reproduce the headphone CPU-noise symptom.
- Added `profile timecode-vinyl-low-noise` as a reversible control profile for
  this exact hardware-state experiment.

## 2026-06-19 10:45 EDT - Low-latency scratch response candidate - REJECTED

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Hardware touched: yes, with hardware/audio lock
- Driver installed/reloaded: yes, local HAL only
- Playback: yes, one-second A-pair tone smoke
- Capture: Core Audio input meter only

Reason:

- Human Traktor test reported good sound and working Timecode Vinyl, but
  scratch/control response lagged by roughly 0.5-1.0 seconds.
- The previous low-noise idle gate kept the output endpoint parked in silence,
  but the output timeline still used `8192` startup/target latency frames.
  At 48 kHz that alone is roughly 170 ms before Core Audio and Traktor latency.

Change:

```text
HAL_OUTPUT_START_LATENCY_FRAMES=192
HAL_OUTPUT_RESTART_LATENCY_FRAMES=192
HAL_OUTPUT_TARGET_LATENCY_FRAMES=192
```

Evidence directory:

```text
local-analysis/install-low-latency-scratch-20260619-104535
```

Verification:

```sh
make -B hal
make install-hal
/usr/local/bin/opena8dj-control profile timecode-vinyl-low-noise
./build/audio-input-meter 4
./build/audio-pair-tone A 1 440 0.02
/usr/local/bin/opena8dj-control stream-stats
```

Observed after tone wake:

```text
output-ring: 0 / target 192 frames
outputStartupSilenceFrames=192
outputUnderruns=0
outputActiveUnderruns=0
outputTimelineResets=0
playbackTransfersSubmitted=126
playbackTransfersCompleted=126
playbackTransferErrors=0
```

Input smoke:

```text
Input A: rmsL=0.00032448 rmsR=0.00035513
Input B: rmsL=0.00032514 rmsR=0.00032882
```

Rejection:

- Human real-music playback test immediately reported degraded sound: bass
  saturation and bad overall quality on the same "Cable Guy" track that had
  sounded good on the previous low-noise/stable-latency build.
- Live stream stats after the rejected build showed the failure mode:

```text
output-ring: 0 / target 192 frames
outputUnderruns=21249
outputActiveUnderruns=21249
outputLateWriteFrames=21440
outputLateWriteBatches=169
```

Conclusion:

- `192` output timeline frames is too aggressive for the current HAL transport
  and is rejected as a product default.
- Do not ship or publish the low-latency build until a different latency
  approach preserves sound quality with zero active underruns and zero late
  write batches under real music.

## 2026-06-19 10:51 EDT - Roll back rejected low-latency build

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Hardware touched: yes, with hardware/audio lock
- Driver installed/reloaded: yes, local HAL only
- Playback: yes, one-second A-pair tone smoke

Reason:

- Restore the last known good sound-quality path after rejecting the 192-frame
  scratch latency experiment.

Change:

```text
HAL_OUTPUT_START_LATENCY_FRAMES=8192
HAL_OUTPUT_RESTART_LATENCY_FRAMES=4096
HAL_OUTPUT_TARGET_LATENCY_FRAMES=8192
```

Evidence directory:

```text
local-analysis/rollback-low-latency-bad-sound-20260619-105143
```

Observed after rollback tone smoke:

```text
output-ring: 8192 / target 8192 frames
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
outputLateWriteBatches=0
playbackTransfersSubmitted=125
playbackTransfersCompleted=125
playbackTransferErrors=0
```

Conclusion:

- The installed local HAL is back on the stable sound-quality latency profile
  while keeping `timecode-vinyl-low-noise`.
- Scratch responsiveness still needs a safer approach than shrinking the output
  timeline to 192 frames.

## 2026-06-19 12:31 EDT - Shared hardware lock policy update

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Hardware touched: no
- Playback/capture: no
- Driver installed/reloaded: no

Reason:

- Control Center is now being developed in parallel and may run its own
  hardware checks.
- Hardware access must be generous: release the global lock whenever Codex is
  compiling, editing, analyzing saved evidence, reading logs, packaging, or
  writing documentation.

Policy recorded:

```text
Acquire the hardware lock as late as possible.
Release it as soon as playback, capture, install/reload, Core Audio recovery,
USB access, or Traktor interaction is complete.
Treat Control Center and other OpenA8DJ tools as legitimate lock owners.
Do not steal or clean a live lock; only clean it if the recorded owner PID is
provably dead.
```

Current lock check:

```text
$HOME/.opena8dj/hardware-gate.lock absent
```

## 2026-06-19 12:35 EDT - Preopen responsiveness candidate

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Hardware touched: yes, with hardware/audio lock only during install and
  physical playback/capture windows
- Playback/capture: yes, iRig Stream capture from Audio 8 DJ output B
- Driver installed/reloaded: yes, local HAL only

Reason:

- Previous physical route tests could fail cold because CoreAudio did not
  deliver the first playback callback until roughly 4.26 seconds after
  `AudioDeviceStart`.
- The warm second run passed because USB remained open briefly after stop.

Candidate flags now promoted to Makefile defaults:

```text
HAL_OUTPUT_GAIN=0.75f
HAL_BACKGROUND_PREOPEN_ON_INIT=1
HAL_STOP_GRACE_USEC=10000000
HAL_STOP_ISOC_ON_STOP=1
HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0
HAL_AUDIO_PARAMS_RESET_WAIT_FOR_REPLY=0
HAL_STREAM_USAGE=1
HAL_IGNORE_OUTPUT_SAMPLE_TIME=1
```

Installed signed HAL hash:

```text
e4b473357bd6f18fb30e427ce2e8fc78696652b7564683a2a453d54f79a95c88
```

Evidence:

```text
local-analysis/live-stabilize-20260619-123318-install-preopen-gain075
local-analysis/channel-matrix/live-stabilize-20260619-123342-routeB-preopen-coldstart
local-analysis/live-stabilize-20260619-123403-music-routeB-preopen-gain075-cable-guy
```

Results:

```text
route B first_callback_seconds: 0.023052
route B tone matrix: PASS
left_to_right_leakage_db: -59.44
right_to_left_leakage_db: -61.90
capture_clipped_frames: 0

Cable Guy first_callback_seconds: 0.021345
Cable Guy quality_alignment_score: 0.952548
Cable Guy capture_clipped_frames: 0
Cable Guy outputUnderruns: 0
Cable Guy outputActiveUnderruns: 0
Cable Guy outputLateWriteFrames: 0
post-run audio_stack_health: PASS
post-run hardware lock: absent
```

Comparison against same-session direct USB reference:

```text
direct USB quality_alignment_score: 0.952811
preopen HAL quality_alignment_score: 0.952548
```

Conclusion:

- Preopen fixes the measurable cold-start delay without leaving the audio stack
  hot and without retaining the hardware lock outside the physical action.
- Route B is physically validated with strong channel separation.
- The iRig real-music absolute audiophile gate still fails because the direct
  USB reference also fails the same strict threshold on this analog route.
- This is an improved functional/stability candidate, not an audiophile
  superiority claim over the hardware/direct USB path.

## 2026-06-19 12:44 EDT - Final integrated installed load for human test

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Hardware touched: yes, with hardware/audio lock during install and physical
  playback/capture windows
- Playback/capture: yes, Audio 8 DJ output B into iRig Stream capture
- Driver installed/reloaded: yes, local HAL only
- Default devices changed: no
- USB reset: no
- System reboot: no

Installed HAL:

```text
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
installed_sha=e4b473357bd6f18fb30e427ce2e8fc78696652b7564683a2a453d54f79a95c88
Signature=adhoc
TeamIdentifier=not set
```

Final runtime profile:

```text
input-mode:        0 (timecode-vinyl)
gnd-vinyl:         off
software-lock:     on
input-decode:      on
input-transform:   A=normal B=normal C=normal D=normal
input-source:      A=A B=B C=C D=D
```

Evidence:

```text
local-analysis/final-integrated-20260619-124320-install-current
local-analysis/final-integrated-20260619-124408-installed-routeB-cable-guy-start-calibrated
local-analysis/final-integrated-20260619-124211-idle-irig-cpu-noise
local-analysis/direct-usb-soundcheck/20260619-122914-direct-usb-pairB-cable-guy-post-recover-reference/reanalysis-analyze-soundcheck-capture-current-thresholds.json
```

Final installed soundcheck:

```text
SOUNDCHECK B 48000/512: PASS
source=Cable Guy - Dj Deep (Original Mix).mp3
mode=start
first_callback_seconds=0.021167
first_energy_record_seconds=0.040000
quality_alignment_score=0.946549
analog_snr_db=8.47
quiet_mid_band_noise_dbfs=-40.80
mid_band_cpu_corr=0.289631
capture_clipped_frames=0
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
playbackTransferErrors=0
captureStatusFailures=0
audio_stack_health=PASS
```

Idle CPU-noise check:

```text
low load:   rms=0.00052401 peak=0.00619507 clipped=0
CPU stress: rms=0.00046297 peak=0.00491333 clipped=0
```

Direct-reference calibration:

```text
Direct USB reanalysis also reports high click outliers on the same iRig route:
left_click_outliers=165
right_click_outliers=244
quality_alignment_score=0.949624
capture_clipped_frames=0

Final HAL run:
left_click_outliers=173
right_click_outliers=241
quality_alignment_score=0.946549
capture_clipped_frames=0
```

Timecode Vinyl status:

- Previous confirmed failure mode: `input-mode: timecode-vinyl` with
  `input-decode: off`; Traktor saw no scope signal.
- Corrected state: `profile timecode-vinyl-low-noise` leaves `input-decode: on`
  and input A/B/C/D are nonzero in `audio-input-meter`.
- Human feedback on this corrected family of builds confirmed Timecode Vinyl
  synchronized and scratch control was responsive.

Conclusion:

- This is the installed human-test load.
- It combines the stable 8192-frame output timeline, reduced 0.75 output gain,
  preopen responsiveness, strict idle digital silence, and the low-noise
  timecode profile with input decode on.
- It does not prove final audiophile superiority over every reference path.
  It is ready for the next human listening/Traktor pass because the exact
  installed artifact passed the calibrated iRig soundcheck and has no driver
  underruns, no clipping, no transfer errors, and no measurable idle CPU-noise
  increase in the iRig capture.

## 2026-06-19 13:33 EDT - Human test feedback during Traktor session

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Installed commit under test: `6c2670e`
- Hardware touched by Codex for this note: no
- Driver installed/reloaded by Codex for this note: no

User feedback:

- Mac microphone/dictation works again while the installed OpenA8DJ load is
  active.
- Timecode Vinyl is now more or less responsive and usable.
- Responsiveness could still improve a little.
- Overall direction is improving.

Concurrent observation:

```text
Traktor CPU: roughly 71-75%
OpenA8DJ driver CPU: roughly 8-9.5%
coreaudiod CPU: roughly 1.5-1.8%
audio_stack_health=PASS
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
playbackTransferErrors=0
captureStatusFailures=0
hardware lock: absent
```

Conclusion:

- Keep this load as the current human-test baseline.
- Next optimization should target driver CPU and Timecode Vinyl responsiveness
  without regressing the now-working microphone/dictation state or the stable
  zero-underrun transport.
