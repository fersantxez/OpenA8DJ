# OpenA8DJ 0.5.0 Public Validation Summary

Date: 2026-06-22

OpenA8DJ 0.5.0 is the current macOS C++ baseline for the Native Instruments
Audio 8 DJ. It is an independent preservation project and is not affiliated
with, endorsed by, sponsored by, or certified by Native Instruments.

## Download Safety

Use only the files attached to this repository's GitHub Releases page. Match
downloads against the checksum file shipped with the same release.

Maintainers must not describe replacement binary assets as Developer ID signed
or Apple-notarized unless the exact published files have passed the signed
release verifier.

Current signing status for the replacement 0.5.0 artifacts rebuilt from commit
`86bd027`, checked on 2026-06-22:

- Apple accepted all four current publication files:
  `OpenA8DJ-0.5.0.pkg`, `OpenA8DJ-0.5.0.dmg`,
  `opena8dj-tools-0.5.0.pkg`, and `opena8dj-tools-0.5.0.dmg`.
- All four files were stapled, the checksum file was regenerated, and
  `make verify-signed-release` passed on the exact final stapled artifacts.
- The public GitHub release was corrected to DMG-only distribution:
  `OpenA8DJ-0.5.0.dmg`, `opena8dj-tools-0.5.0.dmg`, and
  `OpenA8DJ-0.5.0-checksums.txt`.
- The published checksum file covers the two public DMGs.
- The downloaded public DMGs passed the GitHub-downloaded install flow on
  2026-06-22.
- Earlier partial Apple acceptances belong to older containers and are no longer
  final publication candidates after the documentation reorganization.

Current public install validation, completed on 2026-06-22:

- Downloaded the published `v0.5.0` GitHub DMG assets into `~/Downloads`
  and matched them against the published checksum file.
- Verified stapled tickets and Gatekeeper acceptance for the public DMGs.
- Opened the packages inside both mounted public DMGs with the normal macOS
  Installer app; no Gatekeeper block occurred.
- Used the documented `sudo installer` fallback to complete unattended
  validation after confirming the normal Installer app path opened.
- The installed driver, tools, MIDI bridge, Control Center app, package
  receipts, LaunchAgent, and code signatures were verified on disk.
- `Open Audio 8 DJ` appeared in Core Audio as an 8-input / 8-output device at
  48 kHz, and the MIDI input/output endpoints appeared.
- Control Center and `opena8dj-control` were present and functional.
- Audio stack health passed after install.
- CoreAudio was restarted only during the pre-install uninstall path, via the
  project uninstaller. No USB/hardware was physically touched for this public
  download validation.

Local installer validation, completed on 2026-06-21:

- Rebuilt local signed DMGs from the current publication artifact set installed
  successfully with the documented `sudo installer` fallback.
- The installed driver, tools, MIDI bridge, Control Center app, package
  receipts, and code signatures were verified on disk.
- `Open Audio 8 DJ` appeared in Core Audio as an 8-input / 8-output device at
  48 kHz, and the MIDI input/output endpoints appeared.
- Physical A/B output routing passed external-capture checks.
- A calibrated real-music external-capture soundcheck passed on the installed
  artifact after isolating the capture path from other audio apps.
- The public GitHub-download validation above confirms the published files
  install and expose the same installed binary hashes as this local
  sound-validated build.

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
  real music through the Audio 8 DJ and real-time external recording.
- Human listening accepted the installed CPU pool profile as the stable 0.5.0
  sound profile.

## Hardware And Software Used

- Hardware under test: Native Instruments Audio 8 DJ.
- Capture path for physical sound validation: real-time external recording.
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

- Windows, Linux, and Rust branches are experimental and not validated release
  branches.
- DriverKit/AudioDriverKit is scaffolding for a future System Extension path,
  not the active 0.5.0 runtime.
- More aggressive latency reductions below the published 0.5.0 baseline need
  separate same-session physical validation before replacing this release
  reference.
- Further CPU transport experiments need separate same-artifact physical sound
  validation before replacing the frozen 0.5.0 profile.

## Stable 0.5.0 Sound Validation

The 0.5.0 sound profile was validated with real music, Audio 8 DJ analog output,
real-time external recording, automated WAV comparison, and human listening.
The accepted profile had no clipped capture frames in the recorded validation
run and was accepted as the stable 0.5.0 sound baseline.

## Detailed Evidence

Technical logs, detailed build names, rejected experiments, and command-level
evidence are kept in [maintainer state](../../docs-state/evidence/test-evidence.md).
