# OpenA8DJ 0.5.0 Public Validation Summary

Date: 2026-06-20

OpenA8DJ 0.5.0 is the current macOS C++ baseline for the Native Instruments
Audio 8 DJ. It is an independent preservation project and is not affiliated
with, endorsed by, sponsored by, or certified by Native Instruments.

## Signing Status

OpenA8DJ 0.5.0 release assets are unsigned, not Developer ID signed, and not
Apple-notarized. macOS Gatekeeper will likely block them unless manually
approved after checksum verification.

Apple Developer Program membership is active as of 2026-06-20. The remaining
release blocker is local signing setup: this Mac has no Developer ID
Application identity, no Developer ID Installer identity, and no stored
`notarytool` profile yet.

## What Was Tested

- macOS Core Audio HAL driver loads as `Open Audio 8 DJ`.
- The device appears with 8 input channels and 8 output channels.
- Output channels are exposed as stereo deck pairs A/B/C/D.
- MIDI endpoints appear as `Open Audio 8 DJ MIDI In` and
  `Open Audio 8 DJ MIDI Out`.
- Driver package installs the HAL driver, MIDI bridge, control CLI, uninstall
  tool, LaunchAgent, and documentation.
- Tools package installs `OpenA8DJ Control Center.app`, `opena8dj-control`, and
  support documentation without replacing the HAL driver.
- Offline C++ tests pass.
- HAL smoke and parity checks pass.
- Physical playback/capture validation passed on the installed artifact using
  real music through the Audio 8 DJ and an iRig Stream capture path.
- Human listening accepted the installed CPU pool profile as the stable 0.5.0
  sound profile.

## Hardware And Software Used

- Hardware under test: Native Instruments Audio 8 DJ.
- Capture path for physical sound validation: iRig Stream.
- Host platform: macOS.
- Main software path: Core Audio HAL plug-in, IOUSBHost transport, CoreMIDI
  bridge, C++ core, and macOS control tools.
- User workflow validated: Traktor/timecode-oriented DVS Vinyl state and A/B/C/D
  deck routing surface.
- Stable loaded HAL executable SHA-256:
  `c6e4d491e35e73d90109cab33c71a616173d002fbc6fa2519c241512eb85c951`.
- Unsigned stable build executable SHA-256 before ad-hoc signing:
  `79390010acbd96b799d3f69d9f1ae92ccaec68e37439ae4a54a2ab91ea091098`.

## Validated Rates

- 44.1 kHz
- 48 kHz

## Validated User Checks

After install, Audio MIDI Setup should show `Open Audio 8 DJ` with 8 inputs and
8 outputs. If it does not appear, reconnect the Audio 8 DJ once, then reopen the
audio app.

## Not Yet Claimed

- The 0.5.0 assets are not Developer ID signed.
- The 0.5.0 assets are not Apple-notarized.
- Windows, Linux, and Rust branches are experimental and not validated release
  branches.
- DriverKit/AudioDriverKit is scaffolding for a future System Extension path,
  not the active 0.5.0 runtime.
- More aggressive latency reductions below the 0.5.0 baseline need separate
  same-session physical validation before replacing this release reference.
- Further CPU transport experiments need separate same-artifact physical sound
  validation before replacing the frozen 0.5.0 profile.

## Stable 0.5.0 Sound Validation

Latest same-artifact physical validation before human listening:

```text
run=local-analysis/physical-cpu-candidate-ab/20260620T120432-cpu-pool-repeat-irig/soundcheck-candidate-repeat
route=Open Audio 8 DJ pair B -> iRig Stream channels 1,2
source=Cable Guy - Dj Deep (Original Mix).mp3
result=PASS
quality_alignment_score=0.948151
analog_snr_db=8.72
capture_clipped_frames=0
opena8dj_driver_avg_cpu=5.470%
opena8dj_driver_max_cpu=6.300%
audio_stack_health_after=PASS
hardware_lock_after=absent
```

## Frozen 0.5.0 Asset Hashes From Current Main

These hashes describe the locally rebuilt 0.5.0 replacement asset set after the
tools package install-location fix. Published downloads should always be checked
against the `OpenA8DJ-0.5.0-checksums.txt` asset shipped with the same release.

```text
37d9fbd34e0fa76743bad568b62e722775269956479bfbe96f8137b55941f0cd  OpenA8DJ-0.5.0.dmg
f7b629a04eec1e37a58de806587a6c730bc6e86d4a1e5065b182839a0a2e9265  OpenA8DJ-0.5.0.pkg
c6bb68a41661ae7c3c617069d66a5b8a1a8fbb622afd978a5d4724a677665172  opena8dj-tools-0.5.0.dmg
17fd67f67d1d70f26faea5d16f28af9a204b27adcdfe42bab674d2f8dd8a4221  opena8dj-tools-0.5.0.pkg
```

## Detailed Evidence

Technical logs, internal build names, rejected experiments, and command-level
evidence are kept in `docs/TEST_EVIDENCE.md`.
