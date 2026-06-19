# OpenA8DJ 0.5.0 Public Validation Summary

Date: 2026-06-19

OpenA8DJ 0.5.0 is the current macOS C++ baseline for the Native Instruments
Audio 8 DJ. It is an independent preservation project and is not affiliated
with, endorsed by, sponsored by, or certified by Native Instruments.

## Signing Status

OpenA8DJ 0.5.0 release assets are unsigned, not Developer ID signed, and not
Apple-notarized. macOS Gatekeeper will likely block them unless manually
approved after checksum verification.

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

## Hardware And Software Used

- Hardware under test: Native Instruments Audio 8 DJ.
- Capture path for physical sound validation: iRig Stream.
- Host platform: macOS.
- Main software path: Core Audio HAL plug-in, IOUSBHost transport, CoreMIDI
  bridge, C++ core, and macOS control tools.
- User workflow validated: Traktor/timecode-oriented DVS Vinyl state and A/B/C/D
  deck routing surface.

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

## Replacement Asset Hashes From Current Main

These hashes describe the locally rebuilt 0.5.0 replacement asset set after the
tools package install-location fix. Published downloads should always be checked
against the `OpenA8DJ-0.5.0-checksums.txt` asset shipped with the same release.

```text
d5ede10360873e154e14a37628ed36ca302d68752d80feb72fff54f7bc46b92b  OpenA8DJ-0.5.0.dmg
f509bc07fb8172556ac53d7bfd1d66ed6023781fab27c7a55543c10fc15e631a  OpenA8DJ-0.5.0.pkg
6c75c15e6259b76ea2708cd56a5f54ad8d1806a37326e5a26a5065eb2fe26025  opena8dj-tools-0.5.0.dmg
74b365ee11a629facb14a00e5a81599f2a4e09ded1b2de37dbbe73c1c00f5fe2  opena8dj-tools-0.5.0.pkg
```

## Detailed Evidence

Technical logs, internal build names, rejected experiments, and command-level
evidence are kept in `docs/TEST_EVIDENCE.md`.
