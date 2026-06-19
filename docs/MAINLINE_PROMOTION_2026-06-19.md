# Mainline Promotion - 2026-06-19

## Decision

The C++/DriverKit redesign line is promoted to the repository mainline as
OpenA8DJ 0.4.0. From this point forward, `main` is the modern macOS driver
line.

The previous C/Objective-C mainline is preserved on the `legacy` branch. That
branch remains the historical reference for the 0.3.x C HAL implementation,
physical-test learnings, recovery tooling, baseline behavior, and the older
Linux/CAIAQ-derived architecture.

## Promotion Rationale

- The project now has a clear user-facing macOS install path: GitHub Release
  DMG -> bundled PKG -> Core Audio HAL driver install.
- The promoted line follows a modern macOS user-space architecture: Core Audio
  HAL today, IOUSBHost transport, CoreMIDI bridge, and a documented
  DriverKit/AudioDriverKit path for future System Extension work.
- The C++ line exposes the target Traktor-facing surface: 8 inputs, 8 outputs,
  one 8-channel input stream, and four stereo output streams A/B/C/D.
- The hardware profile `timecode-vinyl` is active and operator feedback reports
  responsive timecode behavior.
- The current loaded candidate keeps Core Audio visible and idle-cold after
  validation.
- iRig capture confirms the idle noise floor is low enough for continued human
  validation.
- Internal diagnostic capture proves the software path is coherent through
  Core Audio write, driver consumption, and USB packet packing.

## Current Promoted Load

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch before promotion: `driverkit/cpp-redesign`
- Candidate shape: 8 in / 8 out, four output streams, one input stream.
- Profile: `timecode-vinyl`
- Key build profile:
  - Makefile defaults now match this promoted profile.
  - `HAL_OUTPUT_STREAMS=4`
  - `HAL_INPUT_STREAMS=1`
  - `HAL_STREAM_USAGE=1`
  - `HAL_OUTPUT_GAIN=1.50f`
  - `HAL_OUTPUT_START_BYTE=4`
  - `HAL_OUTPUT_NATIVE_I24=0`
  - `HAL_PLAYBACK_CAPTURE_PACED=1`
  - `HAL_CAPTURE_PACED_OUT_LEAD=2`
  - `HAL_ISO_FRAMES=64`
  - `HAL_CAPTURE_QUEUE=8`
  - `HAL_PLAYBACK_QUEUE=8`
  - hot stats/write/amplitude stats disabled for normal load.

## Evidence

- Final normal load:
  `local-analysis/hal-safety/20260619T040726Z-restore-normal-after-diagnostic-evidence-load`
- Long B-pair iRig capture:
  `local-analysis/soundcheck/20260619T035921Z-best-4out-start4-B-long`
- Driver-internal diagnostic attribution:
  `local-analysis/soundcheck/20260619T040609Z-diagnostic-unique-4out-start4-B`
- Idle noise capture:
  `local-analysis/silence/20260619T035550Z-best-4out-gain150-idle`

## Evidence Summary

- Core Audio enumeration: `Open Audio 8 DJ`, 8 inputs / 8 outputs.
- Final idle health: PASS, `coreaudiod=0.0%`, OpenA8DJ driver `0.0%`.
- Idle capture: RMS about `-68 dBFS`, peak about `-42.7 dBFS`.
- Internal diagnostic path:
  - written alignment score: `1.000000`
  - consumed alignment score: `1.000000`
  - USB packed alignment score: `1.000000`
  - USB check errors: `0`
  - USB start byte: `4`
  - USB byte order: `big`

## Known Follow-Up

The promoted mainline is accepted as the new product direction, not as a closed
claim of final audiophile superiority. Continued work should focus on physical
USB scheduling, analog-route validation, and long-run capture stability.

Do not remove the `legacy` branch. Use it as the C baseline for behavior
comparison and emergency reference.
