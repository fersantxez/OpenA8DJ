# Test Evidence

This file records reproducible release and validation evidence for the modern
macOS mainline.

This is a technical evidence log. It intentionally contains internal build
target names, local evidence paths, rejected experiments, and command-level
details that are too noisy for public release notes. For the external-facing
summary, see [public validation summary](../../project/public-validation-summary.md).

## 2026-06-19 17:01 EDT - Main package fix, DVS default, Control Center, and calibrated soundcheck

- Worktree: `/private/tmp/opena8dj-main-merge`
- Branch at time: `main`
- Hardware touched: yes, under the hardware lock
- Driver installed/reloaded: yes, via the rebuilt 0.5.0 packages
- Driver source delta from public `v0.5.0`: HAL startup now re-applies the
  default DVS Vinyl low-noise state; `traktor-dvs-vinyl` now maps to that state
  in the control tool; Control Center presents `DVS Vinyl` first.

What changed:

- Fixed the tools package component metadata so
  `OpenA8DJ Control Center.app` installs into `/Applications` and is not
  relocated by macOS Installer.
- Made DVS Vinyl the default visible user state: input mode `timecode-vinyl`,
  input decode on, software lock on, identity A/B/C/D routing, and vinyl ground
  lift off for the validated low-noise state.
- Removed Terminal commands from the normal user-facing vinyl flow. Normal
  options are handled through Control Center.
- Exposed all soundcheck analyzer thresholds through `make soundcheck`.
- Added `make soundcheck-irig-calibrated` for the same iRig Stream physical
  validation route used by the accepted 0.5.0 sound-quality reference.

Commands run:

```sh
make checksums
(cd build && shasum -a 256 -c OpenA8DJ-0.5.0-checksums.txt)
sudo installer -pkg build/OpenA8DJ-0.5.0.pkg -target /
sudo installer -pkg build/opena8dj-tools-0.5.0.pkg -target /
scripts/audio-stack-health
OPENA8DJ_CONTROL_NO_WAKE=1 /usr/local/bin/opena8dj-control export-config /tmp/opena8dj-installed-config-final.json
make soundcheck-irig-calibrated \
  SOUNDCHECK_MUSIC="/Users/fer/Music/DJ/000_santxez_spring_25_select/Cable Guy - Dj Deep (Original Mix).mp3" \
  SOUNDCHECK_CAPTURE="iRig Stream" \
  SOUNDCHECK_CAPTURE_CHANNELS=1,2
make smoke-hal parity-smoke-hal
```

Results:

- Rebuilt driver and tools packages installed successfully.
- Checksums verified for driver DMG, driver PKG, tools DMG, and tools PKG.
- `pkgutil --check-signature` reports `Status: no signature` for both PKGs.
- Installed HAL, Control Center app, and `opena8dj-control` pass
  `codesign --verify --strict` as valid on disk.
- Audio stack health: PASS; `coreaudiod` and `opena8dj_driver` CPU at 0.0%.
- Installed config reports `preset=traktor-dvs-vinyl`,
  `inputMode=timecode-vinyl`, `inputDecode=true`, `softwareLock=true`,
  `groundLiftVinyl=false`, and normal A/B/C/D routing without a user Terminal
  command.
- HAL smoke: PASS, 8 input channels and 8 output channels.
- HAL parity smoke: PASS.
- Hardware lock released after install; final lock state free.

Final post-install physical soundcheck:

```text
SOUNDCHECK B 48000/512: PASS
source=Cable Guy - Dj Deep (Original Mix).mp3 offset=0.000s mode=start
alignment_score=0.953251
quality_alignment_score=0.949793
time_warp=1 drift_frames=-12
analog_snr_db=8.79
mid_band_1000_5000_residual_ratio=1.522415
high_band_5000_12000_residual_ratio=1.407434
quiet_mid_band_noise_dbfs=-39.89
mid_band_cpu_corr=0.542229 source=codex_audio_service
click_outliers=191
lag_jumps_gt_2_frames=20
capture_clipped_frames=0
run_dir=/private/tmp/opena8dj-main-merge/local-analysis/soundcheck/2026-06-19T170322
```

Final package install evidence:

```text
evidence=/private/tmp/opena8dj-main-merge/local-analysis/install-dvs-default-final-20260619-170303
```

Current replacement asset hashes from this run:

```text
d5ede10360873e154e14a37628ed36ca302d68752d80feb72fff54f7bc46b92b  OpenA8DJ-0.5.0.dmg
f509bc07fb8172556ac53d7bfd1d66ed6023781fab27c7a55543c10fc15e631a  OpenA8DJ-0.5.0.pkg
6c75c15e6259b76ea2708cd56a5f54ad8d1806a37326e5a26a5065eb2fe26025  opena8dj-tools-0.5.0.dmg
74b365ee11a629facb14a00e5a81599f2a4e09ded1b2de37dbbe73c1c00f5fe2  opena8dj-tools-0.5.0.pkg
```

## 2026-06-19 15:30 EDT - Canonical 0.5.0 repo cleanup and release packaging

- Commit under test: `be6d2a6`
- Worktree: `/private/tmp/opena8dj-main-merge`
- Branch at time: `main`
- Hardware touched: no
- Audio/CoreAudio touched: no
- Driver installed/reloaded: no

Commands run:

```sh
make all
make control-center
make dist
hdiutil verify build/OpenA8DJ-0.5.0.dmg
hdiutil verify build/opena8dj-tools-0.5.0.dmg
pkgutil --payload-files build/OpenA8DJ-0.5.0.pkg
pkgutil --payload-files build/opena8dj-tools-0.5.0.pkg
pkgutil --check-signature build/OpenA8DJ-0.5.0.pkg
pkgutil --check-signature build/opena8dj-tools-0.5.0.pkg
./build/opena8dj-control list-profiles
./build/opena8dj-control export-config /tmp/opena8dj-export-test.json
make smoke-hal parity-smoke-hal
cmake -S . -B build/cmake-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake-release --parallel
ctest --test-dir build/cmake-release --output-on-failure
```

Results:

- `make all`: PASS.
- `make control-center`: PASS.
- `make dist`: PASS.
- Driver artifact generated: `build/OpenA8DJ-0.5.0.pkg`.
- Driver DMG generated: `build/OpenA8DJ-0.5.0.dmg`.
- Tools artifact generated: `build/opena8dj-tools-0.5.0.pkg`.
- Tools DMG generated: `build/opena8dj-tools-0.5.0.dmg`.
- Checksums generated: `build/OpenA8DJ-0.5.0-checksums.txt`.
- DMG verification: PASS for driver and tools DMGs.
- Package payload inspection: PASS; driver package contains HAL, MIDI bridge,
  control CLI, uninstall tool, LaunchAgent, and docs. Tools package contains
  Control Center, `opena8dj-control`, and control-surface docs.
- Package signature status: expected unsigned/ad-hoc preview state,
  `pkgutil --check-signature` reports `Status: no signature` for both packages.
- HAL bundle version: `CFBundleShortVersionString=0.5.0`,
  `CFBundleVersion=150`.
- HAL smoke: PASS, 8 input channels and 8 output channels.
- HAL parity smoke: PASS, 1 input stream, 4 output streams, 16 total stream
  channels.
- CMake build: PASS.
- CTest: PASS, 88/88 tests.

Checksums:

```text
afca883993a85bdf65468b23030897f5d05be4115ae687c2abbb7641dffa3c49  OpenA8DJ-0.5.0.dmg
7b0b77bf623a2204d471a86805c36bbc7335abfce197012024d36210bfad0770  OpenA8DJ-0.5.0.pkg
1c03ee1f6effeef913e172f8fcf07500a08d539784f8d7acaccba36081a03cb9  opena8dj-tools-0.5.0.dmg
fbb066b8a991c2f298d2a3c009e755cc75fc5b52e2a5adfee32b317bbd490564  opena8dj-tools-0.5.0.pkg
```

Scope note:

This evidence validates repository cleanup, offline build/test gates, packaging,
and static release artifacts. It does not install the driver and does not
replace hardware sound-quality validation for a future loaded candidate.

## 2026-06-19 09:54 EDT - Signing and notarization release gate

- Commit before changes: `5f6e457`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch at time: pre-main C++ integration branch
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

## 2026-06-20 14:01 EDT - Developer ID signing and notarization submission

- Worktree: `/private/tmp/opena8dj-main-merge`
- Hardware touched: no
- Driver installed/reloaded: no
- Package executed: no

Local credential state:

```text
Developer ID Application: Fernando Sanchez (D3KWK7MN3Y)
Developer ID Installer: Fernando Sanchez (D3KWK7MN3Y)
OpenA8DJNotary keychain profile: stored and validated
```

Build commands:

```sh
make clean
make release-signed \
  SIGN_IDENTITY="Developer ID Application: Fernando Sanchez (D3KWK7MN3Y)" \
  PKG_SIGN_IDENTITY="Developer ID Installer: Fernando Sanchez (D3KWK7MN3Y)" \
  DMG_SIGN_IDENTITY="Developer ID Application: Fernando Sanchez (D3KWK7MN3Y)"
bash -n scripts/notarize-release scripts/verify-signed-release
cmake -S . -B build/cmake-release
cmake --build build/cmake-release
ctest --test-dir build/cmake-release --output-on-failure
```

Signing verification before notarization:

```text
build/OpenA8DJ.driver: valid on disk
build/OpenA8DJ.driver: satisfies its Designated Requirement
build/OpenA8DJControlCenter.app: valid on disk
build/OpenA8DJControlCenter.app: satisfies its Designated Requirement
OpenA8DJ-0.5.0.pkg: signed by Developer ID Installer
opena8dj-tools-0.5.0.pkg: signed by Developer ID Installer
ctest: PASS, 88/88 tests
```

Current Developer ID signed artifact hashes:

```text
faa8f357c2feb67b456dcd2e784ed8d37385e92f91057d5528e29b6bd22302b5  OpenA8DJ-0.5.0.pkg
c3f8f4c661cf897b805d27b2da4bfae6104a9901cd5733de0f46de88e53e447c  OpenA8DJ-0.5.0.dmg
1dc6851fe6d5be9e7bb29fdfa83be706266a47f743e1492062e2c038a5d54ed8  opena8dj-tools-0.5.0.pkg
f880e884b158a94c04691e00e259f2610ebdfdf56a7bf433aee43fcd2acf2db6  opena8dj-tools-0.5.0.dmg
```

Apple notary submissions:

```text
900d52a7-e153-42ac-a827-a5763cd6fc85  OpenA8DJ-0.5.0.pkg  In Progress
1feefd07-6918-4d9d-a621-15511ce815fb  OpenA8DJ-0.5.0.dmg  In Progress
5e4bb7c8-3de1-4b58-98ab-27ebde2b188c  opena8dj-tools-0.5.0.pkg  Accepted and stapled
9ab1b493-1a32-4674-aede-863f25a9242c  opena8dj-tools-0.5.0.dmg  Accepted and stapled
6c89683d-1ba2-4f9a-bda6-2bd6716f8ff6  OpenA8DJ.driver diagnostic ZIP  Accepted
```

Current blocker:

- Apple notary service still reports the final driver PKG and DMG submissions
  as `In Progress`.
- The tools PKG/DMG are accepted and stapled.
- The HAL bundle itself validates as `Notarized Developer ID`.
- Do not upload replacement public assets until Apple returns `Accepted`,
  stapling succeeds, checksums are regenerated, and `make verify-signed-release`
  passes.

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
- Branch at time: pre-main C++ integration branch
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
- Branch at time: pre-main C++ integration branch
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
- Branch at time: pre-main C++ integration branch
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
- Branch at time: pre-main C++ integration branch
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
- Branch at time: pre-main C++ integration branch
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
- Branch at time: pre-main C++ integration branch
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
- Branch at time: pre-main C++ integration branch
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
- Branch at time: pre-main C++ integration branch
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
- Branch at time: pre-main C++ integration branch
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

## 2026-06-19 13:38 EDT - Timecode responsiveness input-latest candidate

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch at time: pre-main C++ integration branch
- Hardware touched: yes, with hardware/audio lock during install/reload and
  post-install checks
- Driver installed/reloaded: yes, local HAL only
- Playback/capture sound-quality run: no, this is a DVS input-latency candidate
- Default devices changed: no
- USB reset: no
- System reboot: no

Reason:

- Human Traktor testing reported that Timecode Vinyl is usable and improving,
  but still not reactive enough.
- A previous attempt to lower scratch latency by shrinking the output timeline
  to 192 frames caused bad sound and thousands of active underruns, so output
  latency must not be reduced blindly.
- The safer hypothesis is input-specific: the USB capture path writes decoded
  input into a large FIFO ring, and CoreAudio reads from the oldest available
  frames. If the ring accumulates backlog, Traktor receives stale timecode even
  though transport counters remain clean.

Code change under test:

```text
OPENA8DJ_INPUT_MAX_LATENCY_FRAMES=512
```

Behavior:

- Before serving CoreAudio input, trim the input ring to the latest 512 frames.
- If CoreAudio requests a larger buffer, preserve at least the requested frame
  count.
- Leave output timeline, output gain, routing, timecode profile, capture queue,
  playback queue, and quality path unchanged.
- Keep the new flag default-off in the Makefile and build/install it explicitly
  as `hal-timecode-responsive-candidate`.

Installed evidence:

```text
local-analysis/timecode-responsive-20260619-133751-install-input-latest512
candidate_build_sha=3895a09bef120a174d0bafa3816c3549128263a1dff76b21a427f60ee9f53cd7
installed_sha=3895a09bef120a174d0bafa3816c3549128263a1dff76b21a427f60ee9f53cd7
```

Validation:

```text
HAL smoke: PASS
timecode_readiness_gate: PASS
dvs_packet_input_decode: PASS
hal_input_spsc_ring_contract: PASS
audio_stack_health=PASS
Open Audio 8 DJ visible: 8 in / 8 out @ 48000
iRig Stream visible: 2 in / 2 out @ 48000
input-mode: timecode-vinyl
input-decode: on
input-check-errors=0
output-panic-flags=0
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
playbackTransferErrors=0
captureStatusFailures=0
hardware lock: absent after install
```

Known gate status:

- `opena8djcpp_hal_transport_runtime_gate` still fails. That is expected for
  product/superiority claims because the transport path is not yet proven
  better than mainline in same-session physical A/B tests.
- This candidate is therefore a controlled human DVS responsiveness test, not
  a CPU/audiophile superiority claim.

Next operator check:

- In Traktor, test whether scratch/timecode reaction feels closer to the hand.
- Confirm that Mac microphone/dictation still works while the driver is loaded.
- Confirm that no new output crackle, saturation, or white noise appears.
- If responsiveness improves without regressions, keep the input-latest trim as
  the next DVS baseline and tune the limit lower only with physical evidence.

## 2026-06-19 13:50 EDT - Low-noise active-silence candidate installed, physical soundcheck failed

Scope:

- Goal: reduce reported CPU/hardware-like headphone background noise without
  changing the working Timecode Vinyl input-latency behavior.
- Driver installed/reloaded: yes, local HAL only.
- Playback/capture sound-quality run: executed after the Control Center demo
  released the global hardware lock.
- Default devices changed: no intentional default-device change.
- USB reset: no.
- System reboot: no.

Code change under test:

```text
OPENA8DJ_INPUT_MAX_LATENCY_FRAMES=512
OPENA8DJ_IDLE_PLAYBACK_GATE_THRESHOLD=0.00003f
OPENA8DJ_IDLE_PLAYBACK_GATE_HOLD_FRAMES=1536
OPENA8DJ_OUTPUT_ZERO_FLOOR=0.0f
```

Behavior:

- Keeps the current input-latest trim for Traktor timecode responsiveness.
- Adds an active-silence path in the output timeline: if a whole Core Audio
  output buffer is below the idle threshold, the HAL writes deterministic zero
  frames instead of preserving possible client dither/sub-noise.
- Parks playback after 1536 consecutive silent frames.
- Does not apply a global zero-floor to normal music playback, so ordinary
  zero crossings and low-level musical detail are left untouched.
- Leaves output timeline latency, output gain, routing, capture queue,
  playback queue, and timecode profile unchanged.

Installed evidence:

```text
installed_sha=dd16595f7280a669b4d0928719bb4d07681abef789fe17fc19dd6035f79c79dc
build_sha=dd16595f7280a669b4d0928719bb4d07681abef789fe17fc19dd6035f79c79dc
installed_path=/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
profile=timecode-vinyl-low-noise
input-mode=timecode-vinyl
input-decode=on
```

Offline validation:

```text
ctest targeted gates: PASS
- opena8djcpp_timecode_readiness_gate
- opena8djcpp_dvs_packet_input_decode
- opena8djcpp_soundcheck_wav_quality
- opena8djcpp_hal_candidate_safety_gate
- opena8djcpp_hal_input_spsc_ring_contract

simulated output soundcheck: PASS
run_dir=/Users/fer/dev/audio8djcpp/local-analysis/simulated-output/2026-06-19T135007
alignment_score=1.000000
simulated_snr_db=75.54
mid_band_1000_5000_residual_ratio=0.000630
mid_band_1000_5000_residual_dbfs=-108.70
mid_band_cpu_corr=0.000000

audio stack guard after install: PASS
run_dir=local-analysis/low-noise-20260619-134933-audio-stack
global_cpu_idle_pct=81.70
coreaudiod.cpu_pct=0.0
opena8dj_driver.cpu_pct=0.0
```

Physical validation status:

```text
requested_soundcheck=route B, Cable Guy, iRig Stream capture, 48 kHz, 512 frames
run_dir=local-analysis/low-noise-20260619-135132-routeB-cable-guy
result=FAIL
quality_alignment_score=0.135231
analog_snr_db=-42.17
quiet_mid_band_noise_dbfs=-34.52
mid_band_cpu_corr=0.497966
lag_jumps_gt_2_frames=41
capture_clipped_frames=0
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
playbackTransferErrors=0
```

Readiness statement:

- This build is rejected for human-test handoff. It passes offline gates, but
  the exact installed artifact failed the required external iRig sound-quality
  capture.
- The aggressive threshold/hold combination must not be promoted. The next
  low-noise candidate should either use the known-good idle threshold or prove
  any higher threshold through same-session A/B before install.

Follow-up diagnostic:

```text
stable_rollback_installed_sha=aae519c6d3b0d068b5cbf1d121c2de95f1288ed5ebeb98d69b4531859a018122
stable_rollback_run=local-analysis/stable-rollback-20260619-135326-routeB-cable-guy
stable_rollback_result=FAIL
wide_lag_reanalysis_alignment=0.95026155
wide_lag_reanalysis_quiet_mid_band_noise_dbfs=-33.30
route_matrix_pairB=strongest iRig route
route_matrix_pairC=near silence
route_matrix_pairD=near silence
observed_traktor_process=running, about 56 percent CPU
```

Interpretation:

- The route map shows output pair B is physically reaching the iRig, but the
  music soundcheck has large pre-existing energy/noise before playback starts.
- Traktor was still running during the failed quality run and can contaminate
  the analog capture because the iRig records the physical output mix, not just
  the test process.
- A clean quality decision requires a quiesced audio environment: no Traktor,
  VLC, Spotify, Control Center hardware demo, or other Audio 8 client during
  the capture.
- An attempt to close Traktor for a clean rerun was correctly blocked because
  `control-surfaces-demo` had reacquired the hardware lock.

Current safe state:

- The installed rollback keeps the Timecode Vinyl input-latest behavior and
  restores the known-good idle threshold:

```text
OPENA8DJ_INPUT_MAX_LATENCY_FRAMES=512
OPENA8DJ_IDLE_PLAYBACK_GATE_THRESHOLD=0.000001f
OPENA8DJ_IDLE_PLAYBACK_GATE_HOLD_FRAMES=0
OPENA8DJ_OUTPUT_ZERO_FLOOR=0.0f
```

- The active-silence infrastructure remains in code, but the reproducible
  low-noise target no longer enables the rejected high-threshold gate.

## 2026-06-19 14:10 EDT - SPSC CPU-lite rollback after Timecode Vinyl regression

Operator report:

- The SPSC CPU-lite build installed with SHA
  `23aa5508aab6792e48d773e5be9f2781d7d4805d9302be39df1b5f4c65331ccc`
  regressed Traktor Timecode Vinyl: signal was visible but did not synchronize.
- This is treated as a blocking DVS regression. Do not promote
  `OPENA8DJ_INPUT_SPSC_RING=1` without new physical DVS evidence.

Action taken:

```text
rebuilt_profile=hal-timecode-low-noise-candidate
OPENA8DJ_INPUT_SPSC_RING=0
OPENA8DJ_INPUT_MAX_LATENCY_FRAMES=512
OPENA8DJ_OUTPUT_GAIN=0.75f
OPENA8DJ_IDLE_PLAYBACK_GATE_THRESHOLD=0.000001f
OPENA8DJ_IDLE_PLAYBACK_GATE_HOLD_FRAMES=0
OPENA8DJ_OUTPUT_ZERO_FLOOR=0.0f
installed_sha256=aae519c6d3b0d068b5cbf1d121c2de95f1288ed5ebeb98d69b4531859a018122
profile_after_install=timecode-vinyl-low-noise
input-mode=timecode-vinyl
input-decode=on
lock_after_install=absent
```

Offline gates run before install:

```text
opena8djcpp_timecode_readiness_gate=PASS
opena8djcpp_dvs_packet_input_decode=PASS
opena8djcpp_soundcheck_wav_quality=PASS
opena8djcpp_hal_candidate_safety_gate=PASS summary, active hash mismatch expected before install
```

Post-install CPU note:

```text
ps_after_coreaudiod_restart=misleading high decay value for coreaudiod/mediaremoted
top_instant_coreaudiod=0.0%
top_instant_opena8dj_driver=0.0%
top_instant_mediaremoted=0.0%
coreaudiod_sample=local-analysis/cpu-spike-20260619-140843/coreaudiod.sample.txt
```

Status:

- Driver is installed and loaded for human DVS/audio retest.
- This is a rollback from the SPSC optimization, not a new CPU improvement.
- Next CPU optimization must preserve the proven non-SPSC capture semantics or
  include a same-session Traktor Timecode Vinyl physical validation.

## 2026-06-19 14:12 EDT - Live Traktor monitor after audible clipping reports

Installed artifact at start:

```text
installed_sha256=aae519c6d3b0d068b5cbf1d121c2de95f1288ed5ebeb98d69b4531859a018122
profile=timecode-vinyl-low-noise
operator_report=initial impression good, then a couple of audible clippings
monitor_run=local-analysis/live-monitor-20260619-141208
focused_monitor_run=local-analysis/live-monitor-focused-20260619-141323
lock_status=absent during passive monitoring
```

Observed during active Traktor playback:

```text
streaming=yes
sample_rate=48000
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
playbackTransferErrors=0
captureTransactionErrors=0
captureStatusFailures=0
outputPanicFlags=0
Traktor_CPU_observed=65-114%
OpenA8DJ_driver_CPU_observed=about 5-9% while streaming
coreaudiod_CPU_observed=about 1-6% before later audio-stack spin
output_ring_growth_observed=10528 -> 18432 frames
stream_transition_observed=streaming yes -> no after ring reached 18432 frames
```

Interpretation:

- The audible clipping did not coincide with the current transport counters:
  no USB transfer errors, active underruns, late writes, or panic flags were
  reported.
- The output timeline accumulated well above the 8192-frame target before the
  stream stopped. That is not classified as an underrun by current counters,
  but it is relevant to audible discontinuity/latency analysis.
- The installed stable build has amplitude stats disabled, so
  `output-level peak/near-clip/clipped=0` is not useful for proving or
  disproving sample clipping.

Rejected diagnostic attempt:

```text
diagnostic_sha256=fedfe71da4e1284eabcf17f9ae3e7854670f30ecdce5d39d12028f5f8193cbb8
changes=output amplitude/write stats enabled, output elastic high-water lowered to 12288 frames
offline_timecode_readiness=PASS
offline_dvs_packet_input_decode=PASS
offline_soundcheck_wav_quality=PASS
status=REJECTED
reason=coreaudiod remained about 95-100% after install, even with OpenA8DJ driver process at 0.0%
coreaudiod_sample=local-analysis/coreaudiod-spike-fedfe71-20260619-141734/coreaudiod.sample.txt
```

Recovery:

```text
rollback_sha256=aae519c6d3b0d068b5cbf1d121c2de95f1288ed5ebeb98d69b4531859a018122
profile_after_rollback=timecode-vinyl-low-noise
Open_Audio_8_DJ_visible=yes, 8 in / 8 out, 48000 Hz
iRig_Stream_visible=yes, 2 in / 2 out, 48000 Hz
lock_after_recovery=absent
```

Remaining contaminant:

- After rollback and audio stack recovery, `coreaudiod` still spun high while
  the Codex audio service and `audioaccessoryd` were active. The OpenA8DJ
  driver process remained at 0.0% and stream was stopped. Do not use that
  system state as a clean driver CPU or sound-quality measurement.
- For clean Traktor/audio validation, disable Codex dictation/microphone and
  avoid Control Center builds or other high-CPU work during the listening
  window.

## 2026-06-19 14:29 EDT - Traktor Timecode Vinyl stuck on Calculating

Operator report:

```text
symptom=Traktor Timecode Setup showed Deck B signal but stayed Calculating
deck_a=mostly no signal/dot
deck_b=visible grey timecode scope, no calibrated color
installed_sha256=aae519c6d3b0d068b5cbf1d121c2de95f1288ed5ebeb98d69b4531859a018122
```

Initial state:

```text
streaming=no
coreaudiod_CPU_observed=about 100-120%
Codex_audio_service_CPU_observed=about 20-50%
audioaccessoryd_CPU_observed=about 20-30%
OpenA8DJ_driver_CPU_observed=0%
profile_before_recovery=timecode-vinyl-low-noise
gnd-vinyl=off
```

Actions:

```text
coreaudio_recovery=local-analysis/live-timecode-recovery-20260619-142457
ni_agent_recovery=local-analysis/live-timecode-ni-agent-recovery-20260619-142742
traktor_relaunch=local-analysis/live-timecode-traktor-relaunch-20260619-142822
profile_sweep=local-analysis/live-timecode-profile-sweep-20260619-142853
final_profile=timecode-vinyl
gnd-vinyl=on
input-decode=on
input-transform=A=normal B=normal C=normal D=normal
input-source=A=A B=B C=C D=D
```

Observed after recovery and Traktor relaunch:

```text
streaming=yes
OpenA8DJ_driver_CPU_observed=about 5-7%
coreaudiod_CPU_observed=about 1-3%
NIHardwareAgent_CPU_observed=0% after killing its 100% spin
Input_B_RMS_L=about 0.039
Input_B_RMS_R=about 0.039
Input_B_peak=about 0.10-0.11
Input_B_corr=about 0.39-0.41
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
captureTransactionErrors=0
captureStatusFailures=0
outputPanicFlags=0
```

Interpretation:

- `timecode-vinyl-low-noise` reduced the ground-lift setting but left Deck B
  with too little or poorly shaped DVS signal in this session. It should not be
  the default Traktor DVS validation profile until same-session DVS evidence
  proves it stable.
- `timecode-vinyl` with `gnd-vinyl=on` restored a plausible Deck B DVS signal
  after the CoreAudio/NIHardwareAgent recovery and Traktor relaunch.
- If Traktor still remains on `Calculating` with this final state, the next
  reversible test is a Deck B-only `input-transform` sweep, starting with
  polarity/channel transforms, not a rebuild.

## 2026-06-19 14:35 EDT - Timecode input gate candidate for needle-up runaway

Operator report:

```text
symptoms=Deck B low input, absolute-position recognition weak, needle-up runaway acceleration
deck_a_status=ignored for this test; no vinyl connected on A
deck_b_status=only active Timecode Vinyl input
```

Diagnosis:

```text
pre_gate_needle_up_input_b_rms=about 0.0015
pre_gate_needle_up_input_b_peak=about 0.014
pre_gate_needle_up_input_b_corr=about 0.98
interpretation=Traktor was receiving low-level correlated noise instead of true silence
```

Rejected intermediate state:

```text
installed_sha256=6a39dc306229398842a951ed3d6a461e9bb75068f3792c990a2d89ddc1c9d300
timecode_input_gain=2.0
reason=Deck B level improved but later showed peak clamp at 1.0, unsafe for absolute-position decoding
```

Installed candidate:

```text
installed_sha256=e70a3d4d12767be22b83b39f9cdbf77f45b9ddd672accdd09b26403ead0fac29
evidence=local-analysis/live-timecode-input-gate-install-20260619-143536
timecode_input_gain=1.5
input_max_latency_frames=512
timecode_input_gate_threshold=0.025
timecode_input_gate_hold_frames=4096
output_start_latency_frames=8192
output_target_latency_frames=8192
profile=timecode-vinyl
gnd-vinyl=on
input-decode=on
```

Needle-up validation after install:

```text
streaming=yes
Input B: frames=398970 rmsL=0.00000000 rmsR=0.00000000 peakL=0.00000000 peakR=0.00000000 corr=0.0000
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
playbackTransferErrors=0
captureStatusFailures=0
outputPanicFlags=0
```

Interpretation:

- The driver now gives Traktor true silence when the timecode signal is below
  threshold, instead of passing correlated phono/control noise.
- This directly targets the needle-up runaway symptom.
- Human validation still needs Deck B needle-down scratch and absolute-position
  behavior. If absolute position remains poor, next candidates should tune the
  gate threshold/hold or Deck B transform, not reduce output latency.

Follow-up result:

- Rejected for DVS calibration. With needle down, Traktor stayed in
  `Calculating` and did not settle into the previously working colored/stable
  timecode state.
- The digital input gate remains useful as a diagnosis of the needle-up
  runaway cause, but must not be enabled in the current working Timecode Vinyl
  candidate.

## 2026-06-19 14:45 EDT - Frozen good Timecode Vinyl baseline

Operator report:

```text
Deck B Timecode Vinyl is working very well.
Deck A ignored; no vinyl connected on A.
Remaining issue reported later: latency still slightly too high.
```

Frozen installed state:

```text
installed_sha256=bdd6f2f9ba2666f48dd27c639b279f13d89baa522f4bb7e60d42a0688777c5aa
evidence=local-analysis/frozen-good-timecode-20260619-144506
build_family=hal-timecode-frozen-good-candidate
HAL_TIMECODE_INPUT_GAIN=1.0f
HAL_TIMECODE_INPUT_GATE_THRESHOLD=0.0f
HAL_TIMECODE_INPUT_GATE_HOLD_FRAMES=0
HAL_INPUT_MAX_LATENCY_FRAMES=0
HAL_OUTPUT_GAIN=0.75f
HAL_IDLE_PLAYBACK_GATE_THRESHOLD=0.000001f
HAL_IDLE_PLAYBACK_GATE_HOLD_FRAMES=0
HAL_OUTPUT_ZERO_FLOOR=0.0f
```

Runtime profile:

```text
profile=timecode-vinyl-low-noise
input-mode=0 (timecode-vinyl)
gnd-vinyl=off
gnd-cd-line=off
gnd-phono=off
software-lock=on
input-decode=on
input-transform=A=normal B=normal C=normal D=normal
input-source=A=A B=B C=C D=D
```

Post-freeze observed counters:

```text
streaming=yes
sample_rate=48000
Input B frames=1611222 rmsL=0.03932570 rmsR=0.03915196 peakL=0.11685729 peakR=0.13358474 corr=0.3697
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
playbackTransferErrors=0
captureTransactionErrors=0
captureStatusFailures=0
outputPanicFlags=0
```

Lessons frozen:

- Keep the raw timecode waveform intact for Traktor. Do not enable digital
  timecode gain, digital input gating, channel swap, polarity inversion, or SPSC
  input ring in the current DVS baseline.
- `timecode-vinyl-low-noise` with `gnd-vinyl=off` is the current human-validated
  profile for the working Deck B test state.
- The screenshot in this evidence folder may still show Traktor's setup panel
  text as `Calculating`; the operator's live Deck B control report is the
  acceptance signal for this freeze.

Next latency-only candidate:

```text
target=hal-timecode-frozen-good-responsive-candidate
change=HAL_INPUT_MAX_LATENCY_FRAMES=512
unchanged=timecode gain/gate off, output gain, low-noise profile, channel transform identity
intent=drop stale input backlog before CoreAudio serves Traktor, without changing waveform shape
rollback=hal-timecode-frozen-good-candidate + profile timecode-vinyl-low-noise
```

## 2026-06-19 14:49 EDT - Frozen-good responsive candidate installed

Reason:

- Human validation said the 14:45 baseline worked very well, but Deck B still
  had slightly too much Timecode Vinyl latency.
- The new candidate changes only input freshness. It does not change the
  waveform shape, digital gain, digital gate, output gain, output latency
  target, channel transform, or low-noise hardware profile.

Installed state:

```text
target=hal-timecode-frozen-good-responsive-candidate
evidence=local-analysis/timecode-frozen-good-responsive-20260619-144915
installed_sha256=fd8bd3ac6f422e2c3da783172fca964f47f289c9ffd7bb96f1e5be810725bca5
previous_frozen_good_sha256=bdd6f2f9ba2666f48dd27c639b279f13d89baa522f4bb7e60d42a0688777c5aa
HAL_TIMECODE_INPUT_GAIN=1.0f
HAL_TIMECODE_INPUT_GATE_THRESHOLD=0.0f
HAL_TIMECODE_INPUT_GATE_HOLD_FRAMES=0
HAL_INPUT_MAX_LATENCY_FRAMES=512
HAL_OUTPUT_GAIN=0.75f
HAL_IDLE_PLAYBACK_GATE_THRESHOLD=0.000001f
HAL_IDLE_PLAYBACK_GATE_HOLD_FRAMES=0
HAL_OUTPUT_ZERO_FLOOR=0.0f
```

Profile confirmed after install:

```text
input-mode=0 (timecode-vinyl)
gnd-vinyl=off
gnd-cd-line=off
gnd-phono=off
software-lock=on
input-decode=on
input-transform=A=normal B=normal C=normal D=normal
input-source=A=A B=B C=C D=D
```

Validation before handoff:

```text
hal_smoke=PASS
offline_gates=86/87 PASS
offline_expected_block=opena8djcpp_hal_transport_runtime_gate
offline_expected_block_reason=still no same-session physical proof for product/superiority claims
audio_inspect=Open Audio 8 DJ visible, 8 inputs, 8 outputs, 48000 Hz
audio_stack_health=PASS
coreaudiod_idle_cpu=0.0
OpenA8DJ_driver_idle_cpu=0.0
lock_after_install=absent
default_devices_changed=no
usb_reset=no
audio_playback=no
recording=no
```

Handoff status:

- Installed and ready for human Deck B Traktor validation.
- If latency improves and calibration remains stable, promote this as the new
  DVS working baseline.
- If calibration, absolute position, or needle-up behavior regresses, roll back
  to the previous frozen-good SHA and rebuild with
  `hal-timecode-frozen-good-candidate`.

Follow-up result:

- Rejected for DVS. Human validation reported the timecode scope went blank /
  low-signal-like and Traktor returned to `Calibrating`.
- Do not use `HAL_INPUT_MAX_LATENCY_FRAMES=512` as the current latency fix.
  Trimming the input ring this way appears to damage the continuity Traktor
  needs for calibration/absolute tracking.

Rollback:

```text
rollback_evidence=local-analysis/timecode-rollback-frozen-good-20260619-145209
rollback_installed_sha256=bdd6f2f9ba2666f48dd27c639b279f13d89baa522f4bb7e60d42a0688777c5aa
rollback_target=hal-timecode-frozen-good-candidate
HAL_INPUT_MAX_LATENCY_FRAMES=0
profile=timecode-vinyl-low-noise
input-mode=0 (timecode-vinyl)
gnd-vinyl=off
input-decode=on
input-transform=A=normal B=normal C=normal D=normal
audio_inspect=Open Audio 8 DJ visible, 8 inputs, 8 outputs, 48000 Hz
hardware_lock_after_rollback=absent
```

Next latency direction:

- Do not drop old input frames at CoreAudio read time.
- A safer latency improvement needs timestamp-aware capture pacing or smaller
  stable CoreAudio input buffering that preserves continuous frame order, not
  destructive ring trimming.

## 2026-06-19 14:55 EDT - Timecode latency output4096 candidate installed

Reason:

- The previous frozen-good baseline calibrated and sounded good, but human
  testing reported Timecode Vinyl latency: usable, nearly there, but still not
  responsive enough.
- The rejected `HAL_INPUT_MAX_LATENCY_FRAMES=512` experiment proved that
  destructive input trimming breaks Traktor calibration. This candidate leaves
  input continuity untouched and reduces only output timeline latency.

Installed state:

```text
target=hal-timecode-frozen-good-output4096-candidate
evidence=local-analysis/timecode-output4096-20260619-145416
installed_sha256=5be65453c1e501f4c2a28bff67e37de71665662311d62a44c19087fe11a4caa7
HAL_TIMECODE_INPUT_GAIN=1.0f
HAL_TIMECODE_INPUT_GATE_THRESHOLD=0.0f
HAL_TIMECODE_INPUT_GATE_HOLD_FRAMES=0
HAL_INPUT_MAX_LATENCY_FRAMES=0
HAL_OUTPUT_GAIN=0.75f
HAL_OUTPUT_START_LATENCY_FRAMES=4096
HAL_OUTPUT_RESTART_LATENCY_FRAMES=2048
HAL_OUTPUT_TARGET_LATENCY_FRAMES=4096
HAL_OUTPUT_ELASTIC_HIGH_WATER_FRAMES=12288
HAL_IDLE_PLAYBACK_GATE_THRESHOLD=0.000001f
HAL_IDLE_PLAYBACK_GATE_HOLD_FRAMES=0
HAL_OUTPUT_ZERO_FLOOR=0.0f
```

Profile confirmed:

```text
profile=timecode-vinyl-low-noise
input-mode=0 (timecode-vinyl)
gnd-vinyl=off
gnd-cd-line=off
gnd-phono=off
software-lock=on
input-decode=on
input-transform=A=normal B=normal C=normal D=normal
input-source=A=A B=B C=C D=D
```

Validation:

```text
hal_smoke=PASS
targeted_offline_gates=PASS
  opena8djcpp_timecode_readiness_gate
  opena8djcpp_dvs_packet_input_decode
  opena8djcpp_soundcheck_wav_quality
  opena8djcpp_audiophile_tone_gate
  opena8djcpp_hal_candidate_safety_gate
active_stream_target=4096 frames
audio_stack_health_after_reload_settle=PASS
coreaudiod_cpu_after_settle=0.0
OpenA8DJ_driver_cpu_after_settle=0.0
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
playbackTransferErrors=0
captureStatusFailures=0
outputPanicFlags=0
hardware_lock_after_install=absent
```

Handoff status:

- Installed for human Traktor Deck B validation.
- Expected improvement is lower perceived scratch/playback latency, not stronger
  timecode input signal.
- If calibration remains stable and latency improves, promote this as the next
  working DVS baseline.
- If sound quality, clipping, or calibration regresses, roll back to
  `bdd6f2f9ba2666f48dd27c639b279f13d89baa522f4bb7e60d42a0688777c5aa`.

## 2026-06-19 15:02 EDT - Timecode latency output3072 candidate installed

Reason:

- Human validation of the output4096 candidate reported very good progress:
  Timecode Vinyl was almost responsive enough, but still needed a small
  latency reduction.
- This candidate takes a smaller next step than jumping to 2048 frames. It
  leaves the timecode input path untouched and reduces only the output timeline
  target from 4096 to 3072 frames.

Installed state:

```text
target=hal-timecode-frozen-good-output3072-candidate
evidence=local-analysis/timecode-output3072-20260619-150122
installed_sha256=70ae8ca3735235b3efbcf48decb1b45eb844b48824f593f1cc3f50b3e2a52790
HAL_TIMECODE_INPUT_GAIN=1.0f
HAL_TIMECODE_INPUT_GATE_THRESHOLD=0.0f
HAL_TIMECODE_INPUT_GATE_HOLD_FRAMES=0
HAL_INPUT_MAX_LATENCY_FRAMES=0
HAL_OUTPUT_GAIN=0.75f
HAL_OUTPUT_START_LATENCY_FRAMES=3072
HAL_OUTPUT_RESTART_LATENCY_FRAMES=1536
HAL_OUTPUT_TARGET_LATENCY_FRAMES=3072
HAL_OUTPUT_ELASTIC_HIGH_WATER_FRAMES=9216
HAL_IDLE_PLAYBACK_GATE_THRESHOLD=0.000001f
HAL_IDLE_PLAYBACK_GATE_HOLD_FRAMES=0
HAL_OUTPUT_ZERO_FLOOR=0.0f
```

Profile confirmed:

```text
profile=timecode-vinyl-low-noise
input-mode=0 (timecode-vinyl)
gnd-vinyl=off
gnd-cd-line=off
gnd-phono=off
software-lock=on
input-decode=on
input-transform=A=normal B=normal C=normal D=normal
input-source=A=A B=B C=C D=D
```

Validation:

```text
hal_smoke=PASS
targeted_offline_gates=PASS
  opena8djcpp_timecode_readiness_gate
  opena8djcpp_dvs_packet_input_decode
  opena8djcpp_soundcheck_wav_quality
  opena8djcpp_audiophile_tone_gate
  opena8djcpp_hal_candidate_safety_gate
active_stream_target=3072 frames
audio_stack_health_after_settle=PASS
coreaudiod_cpu_after_settle=1.8
OpenA8DJ_driver_cpu_after_settle=6.0
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
playbackTransferErrors=0
captureStatusFailures=0
outputPanicFlags=0
hardware_lock_after_install=absent
```

Handoff status:

- Installed for human Traktor Deck B validation.
- Expected improvement is one small step of lower perceived scratch/playback
  latency versus output4096.
- If calibration, sound quality, or stability regresses, roll back first to the
  output4096 SHA `5be65453c1e501f4c2a28bff67e37de71665662311d62a44c19087fe11a4caa7`.
- Do not try destructive input trimming again for this latency problem.

Follow-up result:

- Human validation: PASS.
- Operator report: much better, works well, freeze this as stable.
- This state is the local `0.5.0` stable reference until a later release-prep
  change explicitly supersedes it.
- Stable reference document:
  `docs/state/current-release-state.md`.

## 2026-06-20 12:04 EDT - 0.5.0 CPU pool stable freeze

- Worktree: `/private/tmp/opena8dj-main-merge`
- Branch: `codex/cpu-optimization-investigation`
- Hardware touched: yes, with hardware/audio lock
- Driver installed/reloaded: already installed CPU pool HAL, no package
  execution during the repeat validation
- USB reset/default-device change/system reboot/Traktor launch: no

Stable freeze decision:

```text
release=OpenA8DJ 0.5.0
stable_profile=cpu-pool
installed_hal_sha256=c6e4d491e35e73d90109cab33c71a616173d002fbc6fa2519c241512eb85c951
unsigned_build_hal_sha256=79390010acbd96b799d3f69d9f1ae92ccaec68e37439ae4a54a2ab91ea091098
```

Physical validation:

```text
run=local-analysis/physical-cpu-candidate-ab/20260620T120432-cpu-pool-repeat-irig/soundcheck-candidate-repeat
route=Open Audio 8 DJ pair B -> iRig Stream channels 1,2
source=Cable Guy - Dj Deep (Original Mix).mp3
result=PASS
quality_alignment_score=0.948151
analog_snr_db=8.72
mid_band_1000_5000_residual_ratio=1.512976
high_band_5000_12000_residual_ratio=1.405052
quiet_mid_band_noise_dbfs=-39.90
mid_band_cpu_corr=0.253938
click_outliers=178
lag_jumps_gt_2_frames=22
capture_clipped_frames=0
opena8dj_driver_avg_cpu=5.470%
opena8dj_driver_max_cpu=6.300%
coreaudiod_avg_cpu=2.674%
coreaudiod_max_cpu=8.700%
audio_stack_health_after=PASS
hardware_lock_after=absent
```

Human listening result:

```text
PASS
```

Follow-up:

- The default Makefile HAL profile now enables the CPU pool cursor and fast ISO
  transfer configuration path.
- Reusable/raw ISO completion-handler experiments remain default-off.
- Further CPU/audio transport changes must pass same-artifact physical sound
  validation before replacing this stable reference.

## 2026-06-20 12:17 EDT - Apple Developer Program active, signing still locally blocked

Gmail evidence:

- Apple Developer welcome email received on 2026-06-20 confirming Apple
  Developer Program membership.
- Auto-renewal email received on 2026-06-20.
- Apple Store order email received on 2026-06-20 for Apple Developer Program
  membership for one year.

Local signing check:

```text
security find-identity -v -p codesigning
0 valid identities found
```

Tooling state:

```text
codesign=available
notarytool=available
Developer ID Application identity=missing
Developer ID Installer identity=missing
OpenA8DJNotary keychain profile=not yet stored
```

Conclusion:

- Apple Developer Program membership is active.
- Signed/notarized 0.5.0 replacement assets are blocked until Developer ID
  certificates are created/downloaded and installed locally, and notarization
  credentials are stored with `notarytool`.
- Do not describe any current asset as Developer ID signed or Apple-notarized
  until `make verify-signed-release` passes.

## 2026-06-20 12:29 EDT - Frozen 0.5.0 unsigned package build

- Worktree: `/private/tmp/opena8dj-main-merge`
- Hardware touched: no
- Driver installed/reloaded: no
- Package executed: no

Commands:

```sh
make hal-cpu-pool-candidate
ctest --test-dir build/cmake-release --output-on-failure
make dist
hdiutil verify build/OpenA8DJ-0.5.0.dmg
hdiutil verify build/opena8dj-tools-0.5.0.dmg
make verify-signed-release
```

Results:

- `make hal-cpu-pool-candidate`: PASS.
- Stable default HAL SHA-256 after restore:
  `79390010acbd96b799d3f69d9f1ae92ccaec68e37439ae4a54a2ab91ea091098`.
- Candidate copy SHA-256:
  `79390010acbd96b799d3f69d9f1ae92ccaec68e37439ae4a54a2ab91ea091098`.
- `ctest`: PASS, 89/89 tests.
- `make dist`: PASS.
- `hdiutil verify`: PASS for both DMGs.
- `make verify-signed-release`: expected FAIL, because the local artifacts are
  ad-hoc signed and not Developer ID signed.

Frozen unsigned/ad-hoc asset hashes:

```text
37d9fbd34e0fa76743bad568b62e722775269956479bfbe96f8137b55941f0cd  OpenA8DJ-0.5.0.dmg
f7b629a04eec1e37a58de806587a6c730bc6e86d4a1e5065b182839a0a2e9265  OpenA8DJ-0.5.0.pkg
c6bb68a41661ae7c3c617069d66a5b8a1a8fbb622afd978a5d4724a677665172  opena8dj-tools-0.5.0.dmg
17fd67f67d1d70f26faea5d16f28af9a204b27adcdfe42bab674d2f8dd8a4221  opena8dj-tools-0.5.0.pkg
```

Expected signed-release verifier failure:

```text
HAL bundle is not signed with a Developer ID Application certificate.
Signature=adhoc
TeamIdentifier=not set
```
