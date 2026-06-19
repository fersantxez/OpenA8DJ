# OpenA8DJ 0.5.0 Release Notes

OpenA8DJ 0.5.0 is the current macOS C++ driver baseline for the Audio 8 DJ. It
is the reference build for the public 0.5.x line.

## Stable Reference

- Stable evidence: `local-analysis/timecode-output3072-20260619-150122`
- Installed HAL SHA256:
  `70ae8ca3735235b3efbcf48decb1b45eb844b48824f593f1cc3f50b3e2a52790`
- Build target: `hal-timecode-frozen-good-output3072-candidate`
- Runtime profile: `timecode-vinyl-low-noise`

## What Changed Since 0.4.0

- Timecode Vinyl responsiveness was improved without touching the raw DVS input
  signal path.
- The stable output timeline target is now 3072 frames, down from the safer but
  less responsive 8192-frame baseline.
- The driver keeps the 8-input / 8-output Traktor-facing surface.
- The stable DVS input profile keeps digital input gain, input gate, destructive
  input trimming, and channel transforms off.

## Confirmed Stable Behavior

- Human Traktor testing reported the 3072-frame output candidate is much better
  and working well enough to freeze as the stable reference.
- Targeted offline gates passed:
  - `opena8djcpp_timecode_readiness_gate`
  - `opena8djcpp_dvs_packet_input_decode`
  - `opena8djcpp_soundcheck_wav_quality`
  - `opena8djcpp_audiophile_tone_gate`
  - `opena8djcpp_hal_candidate_safety_gate`
- Post-install counters were clean:
  - `outputUnderruns=0`
  - `outputActiveUnderruns=0`
  - `outputLateWriteFrames=0`
  - `playbackTransferErrors=0`
  - `captureStatusFailures=0`
  - `outputPanicFlags=0`

## Rejected Experiments

- `HAL_INPUT_MAX_LATENCY_FRAMES=512` regressed Traktor calibration and must not
  be used as the DVS latency fix.
- Digital timecode input gain and timecode input gate are not part of the stable
  profile.
- More aggressive output latency reduction below 3072 frames needs a separate
  physical validation window before it can replace this reference.

## Signing Status

This release may be ad-hoc signed unless the GitHub release explicitly states
that Developer ID signing and Apple notarization are complete. Unsigned or
ad-hoc signed builds can require manual Gatekeeper approval on macOS.
