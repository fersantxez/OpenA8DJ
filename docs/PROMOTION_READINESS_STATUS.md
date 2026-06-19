# Promotion Readiness Status

Status: promoted.

As of 2026-06-19, the C++/DriverKit redesign line is the public `main` branch.
The previous C/Objective-C line is preserved on the `legacy` branch.

## Current Branch Map

| Branch | Role |
|---|---|
| `main` | Current user-facing macOS driver line. |
| `driverkit/cpp-redesign` | Development branch tracking the same C++/DriverKit line. |
| `legacy` | Historical C/Objective-C implementation and baseline reference. |

GitHub default branch: `main`.

Latest public release: `v0.4.0`, published from the modern macOS C++ line with
DMG, PKG, and checksum assets.

## Current Meaning Of "Main"

`main` is not the old C mainline. It is the modern macOS architecture:

- Core Audio HAL plug-in for the current installable preview.
- IOUSBHost USB transport for Audio 8 DJ.
- CoreMIDI endpoints and control bridge.
- Pure C++ core contracts and offline gates.
- DriverKit/AudioDriverKit shell prepared as the forward System Extension path.

The current installable preview still includes C and Objective-C source files
where macOS APIs require or strongly favor them, especially the HAL plug-in and
small command-line tools. Those files belong to the modern macOS driver line.
They are not the previous 0.3.x C mainline branch.

## Current Meaning Of "Legacy"

`legacy` preserves the previous C/Objective-C implementation. It remains useful
for:

- historical Linux CAIAQ / `snd-usb-caiaq` reverse-engineering behavior;
- packet and USB behavior comparison;
- physical-test history from the 0.3.x line;
- emergency rollback/reference work.

New user-facing work should target `main`; use `legacy` only as a reference or
baseline unless an explicit rollback is requested.

## Release Gate Status

`v0.4.0` is the current public preview release. It is acceptable as the current
OpenA8DJ macOS C++ mainline preview, not as a final notarized/audiophile claim.

Still open for future releases:

- Developer ID signing and Apple notarization.
- Full physical DVS/timecode matrix across all input pairs.
- Physical validation of every output pair.
- Long-run performance and sound-quality refinement against the `legacy`
  baseline.
- 88.2/96 kHz production-quality validation.

