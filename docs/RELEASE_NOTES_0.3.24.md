# OpenA8DJ 0.3.24 Release Notes

OpenA8DJ 0.3.24 is an output-focused macOS public preview for the Native
Instruments Audio 8 DJ USB interface.

## Highlights

- Reworks the macOS USB transport around capture-paced isochronous output.
- Keeps Core Audio clocking stable and does not force `GetZeroTimeStamp` to the
  USB clock.
- Ships an output-only Core Audio device shape: 0 inputs / 8 outputs.
- Validates 44.1 kHz and 48 kHz playback locally with physical iRig capture and
  human listening.
- Adds repeatable audio QA tooling for tone sidebands, real-music residuals,
  click outliers, CPU guards, and package safety checks.

## Validation Snapshot

The final loaded candidate was built with ISO5, capture queue 64, transfer pool
enabled, capture-paced output, capture-paced lead 1, gain 0.50, USB clock anchor
disabled, and valid capture OUT layout filtering disabled.

Final local checks:

- Two-cycle HAL package safety load: PASS.
- Final audio-stack guard: PASS.
- `coreaudiod`: 0.0% at idle after validation.
- Device enumeration: `Open Audio 8 DJ`, 0 inputs / 8 outputs, 48 kHz.
- Final physical 1 kHz iRig tone: `sideband_ratio=0.008407`, strongest sideband
  `-43.70 dB`.
- Best same-build physical 1 kHz tone: `sideband_ratio=0.004942`, strongest
  sideband `-48.74 dB`.
- Final real-music physical capture: no clipping; one pass with
  `click_outliers=4`, repeat with `click_outliers=0`.

## Known Limits

- This release is not Apple-notarized and is not Developer ID Installer signed.
  macOS may require approving the installer manually.
- Core Audio input streams are hidden in this preview.
- Traktor Scratch/timecode capture is not supported in 0.3.24.
- 88.2/96 kHz operation remains an extended validation target, not a
  production-quality claim.
- Full output-pair matrix validation beyond the currently tested path remains
  open.

## Install

Download the DMG or PKG from GitHub Releases:

```text
OpenA8DJ-0.3.24.dmg
OpenA8DJ-0.3.24.pkg
OpenA8DJ-0.3.24-checksums.txt
```

Open the DMG, run the package, and reconnect the Audio 8 DJ if it does not
appear immediately.

Uninstall:

```sh
sudo /usr/local/bin/opena8dj-uninstall
```

OpenA8DJ is independent and is not affiliated with, endorsed by, or sponsored by
Native Instruments.
