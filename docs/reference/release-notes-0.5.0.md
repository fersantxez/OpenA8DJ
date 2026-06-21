# OpenA8DJ 0.5.0 Release Notes

OpenA8DJ 0.5.0 is the current macOS C++ driver baseline for the Audio 8 DJ. It
is the reference build for the public 0.5.x line.

## Release Reference

- Public reference: OpenA8DJ 0.5.0 macOS C++ release baseline
- Default runtime state: DVS Vinyl input active with the validated low-noise
  ground setting
- Stable build profile: CPU pool transport profile
- User validation summary: see
  [public validation summary](../project/public-validation-summary.md)

## What Changed Since 0.4.0

- Timecode Vinyl responsiveness was improved without touching the raw DVS input
  signal path.
- The stable output timeline target is now 3072 frames, down from the safer but
  less responsive 8192-frame baseline.
- The driver keeps the 8-input / 8-output Traktor-facing surface.
- The stable DVS input profile keeps digital input gain, input gate, destructive
  input trimming, and channel transforms off.
- The stable build uses the CPU pool transport profile accepted during physical
  playback validation and human listening.

## Confirmed Stable Behavior

- Human Traktor testing reported the 3072-frame output baseline as much better
  and stable enough to freeze as the release reference.
- Human listening on 2026-06-20 accepted the CPU pool profile as the 0.5.0
  stable sound profile.
- Same-artifact real-time capture validation passed on output pair B with real
  music and no clipped capture frames in the recorded validation run.
- Offline audio, routing, timecode-readiness, and HAL safety checks passed.
- Post-install runtime counters were clean during the validation run.

## Limits

- Digital timecode input gain and timecode input gate are not part of the
  normal DVS Vinyl state.
- More aggressive latency reductions need separate physical validation before
  they can replace this reference.
- Windows, Linux, Rust, and DriverKit work remains experimental unless a future
  release note says otherwise.
- Detailed engineering evidence and rejected experiments are preserved in
  [maintainer memories](../memories/evidence/test-evidence.md).

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

## Download Safety

Download OpenA8DJ only from this repository's GitHub Releases page. The release
includes SHA-256 checksums so users can confirm that the downloaded files match
the published assets.

Do not install copies from mirrors, chat attachments, or unknown download sites.
