# OpenA8DJ 0.5.0 Stable Reference

Date: 2026-06-19

This is the local stable reference for the next OpenA8DJ release line. It is
not yet a public GitHub release.

## Installed Reference

```text
installed_sha256=70ae8ca3735235b3efbcf48decb1b45eb844b48824f593f1cc3f50b3e2a52790
installed_path=/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL
evidence=local-analysis/timecode-output3072-20260619-150122
target=hal-timecode-frozen-good-output3072-candidate
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
```

## Runtime Profile

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

## Validation

```text
human_validation=PASS
operator_report=Mucho mejor. Esto funciona bien. Freeze as stable.
active_stream_target=3072 frames
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

## Rollback References

```text
previous_stable_output4096_sha256=5be65453c1e501f4c2a28bff67e37de71665662311d62a44c19087fe11a4caa7
previous_frozen_good_sha256=bdd6f2f9ba2666f48dd27c639b279f13d89baa522f4bb7e60d42a0688777c5aa
```
