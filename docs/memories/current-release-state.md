# OpenA8DJ 0.5.0 Stable Reference

Date: 2026-06-20

This is the public GitHub release reference for OpenA8DJ 0.5.0, the current
macOS C++ 0.5.x baseline.

## Public Reference

```text
release=OpenA8DJ 0.5.0
branch=main
platform=macOS
driver=Core Audio HAL user-space driver
runtime_state=DVS Vinyl input active, low-noise ground setting, CPU pool stable profile
technical_evidence=docs/memories/evidence/test-evidence.md
```

## Build Profile

```text
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
HAL_TRANSFER_POOL_CURSOR=1
HAL_FAST_ISO_TRANSFER_CONFIG=1
HAL_REUSE_ISOC_COMPLETIONS=0
HAL_RAW_ISOC_COMPLETIONS=0
```

## Runtime Profile

```text
state=DVS Vinyl input active, low-noise ground setting
input-mode=0 (timecode-vinyl)
gnd-vinyl=off
gnd-cd-line=off
gnd-phono=off
software-lock=on
input-decode=on
input-transform=A=normal B=normal C=normal D=normal
input-source=A=A B=B C=C D=D
```

## Validation

```text
human_validation=PASS
operator_report=Sounds excellent. Freeze as stable.
active_stream_target=3072 frames
stable_profile=cpu-pool
installed_hal_sha256=c6e4d491e35e73d90109cab33c71a616173d002fbc6fa2519c241512eb85c951
unsigned_build_hal_sha256=79390010acbd96b799d3f69d9f1ae92ccaec68e37439ae4a54a2ab91ea091098
irig_validation_run=local-analysis/physical-cpu-candidate-ab/20260620T120432-cpu-pool-repeat-irig/soundcheck-candidate-repeat
quality_alignment_score=0.948151
analog_snr_db=8.72
capture_clipped_frames=0
targeted_offline_gates=PASS
audio_stack_health_after_settle=PASS
outputUnderruns=0
outputActiveUnderruns=0
outputLateWriteFrames=0
playbackTransferErrors=0
captureStatusFailures=0
outputPanicFlags=0
hardware_lock_after_install=absent
```

## Important Rejections

- Do not use `HAL_INPUT_MAX_LATENCY_FRAMES=512` for DVS latency. It caused
  Traktor to return to blank/low-signal-like `Calibrating`.
- Do not enable digital timecode input gain or input gate in the stable DVS
  build. Those were useful diagnostics, but they are not part of the stable
  0.5.0 reference.
- Do not jump to 2048 output frames without a separate same-session physical
  validation window. The 3072-frame output target is the current stable balance.
- Do not enable reusable/raw ISO completion-handler experiments in the stable
  0.5.0 build without a separate same-artifact physical sound validation window.

## Rollback References

```text
previous_stable_output4096_sha256=5be65453c1e501f4c2a28bff67e37de71665662311d62a44c19087fe11a4caa7
previous_frozen_good_sha256=bdd6f2f9ba2666f48dd27c639b279f13d89baa522f4bb7e60d42a0688777c5aa
```
