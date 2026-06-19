# OpenA8DJ 0.5.0 Release Notes

OpenA8DJ 0.5.0 is the current macOS C++ driver baseline for the Audio 8 DJ. It
is the reference build for the public 0.5.x line.

## Release Reference

- Public reference: OpenA8DJ 0.5.0 macOS C++ release baseline
- Default runtime state: DVS Vinyl input active with the validated low-noise
  ground setting
- Technical evidence: see `docs/TEST_EVIDENCE.md`

## What Changed Since 0.4.0

- Timecode Vinyl responsiveness was improved without touching the raw DVS input
  signal path.
- The stable output timeline target is now 3072 frames, down from the safer but
  less responsive 8192-frame baseline.
- The driver keeps the 8-input / 8-output Traktor-facing surface.
- The stable DVS input profile keeps digital input gain, input gate, destructive
  input trimming, and channel transforms off.

## Confirmed Stable Behavior

- Human Traktor testing reported the 3072-frame output baseline is much better
  and working well enough to freeze as the stable reference.
- Targeted offline gates passed:
  - `opena8djcpp_timecode_readiness_gate`
  - `opena8djcpp_dvs_packet_input_decode`
  - `opena8djcpp_soundcheck_wav_quality`
  - `opena8djcpp_audiophile_tone_gate`
  - HAL safety gate
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
  DVS Vinyl state.
- More aggressive output latency reduction below 3072 frames needs a separate
  physical validation window before it can replace this reference.

## Release Assets

Expected public assets:

```text
OpenA8DJ-0.5.0.dmg
OpenA8DJ-0.5.0.pkg
OpenA8DJ-0.5.0-checksums.txt
opena8dj-tools-0.5.0.dmg
opena8dj-tools-0.5.0.pkg
```

`opena8dj-tools-0.5.0.pkg` is the optional direct installer for the Control
Center and support tools. It does not replace the HAL driver package.

## Signing Status

OpenA8DJ 0.5.0 release assets are unsigned, not Developer ID signed, and not
Apple-notarized. macOS Gatekeeper will likely block them unless manually
approved after checksum verification.
