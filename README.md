# OpenA8DJ

OpenA8DJ is an independent, open-source preservation project for the Native
Instruments Audio 8 DJ.

It exists for people who still love this interface: the sound, the 8-in/8-out
layout, the A/B/C/D deck routing, MIDI, and the Traktor/timecode workflows that
made the Audio 8 DJ a classic piece of DJ hardware. The goal is practical and
community-minded: keep useful hardware working on modern systems instead of
letting it become e-waste.

OpenA8DJ is not affiliated with, endorsed by, sponsored by, or certified by
Native Instruments. It does not include Native Instruments driver binaries,
firmware, installers, logos, or proprietary payloads. Product names are used
only to identify compatible hardware and software.

## Support The Project

OpenA8DJ is independent and non-profit. If this helps you keep an Audio 8 DJ in
use, you can support the maintainer here:

[Buy me a coffee](https://ko-fi.com/fersantxez)

## Download For macOS

Start here if you want to use an Audio 8 DJ on a Mac.

- [Latest release](https://github.com/fersantxez/OpenA8DJ/releases/latest)
- `OpenA8DJ-0.5.0.dmg`: main driver installer
- `OpenA8DJ-0.5.0.pkg`: direct installer package
- `OpenA8DJ-0.5.0-checksums.txt`: SHA-256 checksums
- `opena8dj-tools-0.5.0.dmg`: optional control tools and Control Center
- `opena8dj-tools-0.5.0.pkg`: optional tools direct installer package

Release assets are the only supported public binary downloads. GitHub Actions
artifacts are temporary build files and are not used as end-user distribution.

## Install

1. Open the [latest release](https://github.com/fersantxez/OpenA8DJ/releases/latest).
2. Download `OpenA8DJ-0.5.0.dmg`.
3. Open the downloaded DMG file.
4. Double-click `OpenA8DJ-0.5.0.pkg`.
5. Follow the macOS Installer prompts.
6. Restart the audio app if it was already open.
7. Reconnect the Audio 8 DJ if it does not appear immediately.

After install, open Audio MIDI Setup and confirm `Open Audio 8 DJ` appears with
8 inputs and 8 outputs. If it does not appear, reconnect the Audio 8 DJ once,
then reopen the audio app.

## Basic Use

1. Connect the Audio 8 DJ by USB.
2. Open Audio MIDI Setup.
3. Select `Open Audio 8 DJ`.
4. Confirm it shows 8 inputs and 8 outputs.
5. Open your DJ/audio app.
6. Choose `Open Audio 8 DJ` as the audio device.
7. Assign outputs as stereo pairs:
   - channels 1-2: deck/output A
   - channels 3-4: deck/output B
   - channels 5-6: deck/output C
   - channels 7-8: deck/output D
8. For Traktor timecode vinyl, calibrate the vinyl inside Traktor as usual. The
   driver keeps the Audio 8 DJ input path ready for vinyl by default.

If anything looks wrong, unplug and reconnect the Audio 8 DJ once, then reopen
the audio app.

Detailed install, uninstall, and troubleshooting notes are in the
[macOS install guide](docs/user/install.md).

## Verify A Download

The release includes a checksum file. This step is optional for normal users,
but useful if you want to confirm a download:

```sh
shasum -a 256 OpenA8DJ-0.5.0.pkg
grep OpenA8DJ-0.5.0.pkg OpenA8DJ-0.5.0-checksums.txt
```

The two SHA-256 values must match. Do not install if they differ.

## What Works In 0.5.0

OpenA8DJ 0.5.0 is the current macOS driver baseline.

- Core Audio device: `Open Audio 8 DJ`
- 8 output channels represented as stereo deck pairs A/B/C/D
- 8 input channels represented as stereo input pairs A/B/C/D
- 44.1 kHz and 48 kHz are the primary validated rates
- CoreMIDI endpoints for Audio 8 DJ MIDI I/O
- Traktor output routing for decks A/B/C/D
- Traktor Timecode Vinyl input enabled by default, using the low-noise validated
  state
- CPU pool transport profile frozen as the stable 0.5.0 sound profile after
  iRig Stream validation and human listening sign-off
- Control bridge for input mode, ground-lift, software lock, routing transforms,
  stream statistics, and diagnostic state
- Optional Control Center and command-line tools for support workflows

The current release is stable for the validated macOS Audio 8 DJ workflow. It
is not claimed to be perfect. Please report hardware results, regressions,
routing issues, and timecode findings through GitHub Issues.

For a readable summary of what was validated, see the
[public validation summary](docs/project/public-validation-summary.md).

## Traktor And Timecode Vinyl

For Traktor, select `Open Audio 8 DJ` as the audio device and assign the deck
outputs to A/B/C/D as needed. For vinyl timecode, connect the turntables to
inputs A/B and calibrate the control vinyl inside Traktor. OpenA8DJ keeps the
vinyl input path active by default; no extra setup step is needed for normal
vinyl use.

If you want to confirm or re-apply the normal vinyl state, open
`OpenA8DJ Control Center.app`, choose `DVS Vinyl`, and click `Apply`.

See [Traktor and Timecode Vinyl](docs/user/traktor-timecode.md) for the DVS
checklist.

## Tools And Control Center

The optional `opena8dj-tools-0.5.0.dmg` installs:

```text
/Applications/OpenA8DJ Control Center.app
/usr/local/bin/opena8dj-control
/Library/Documentation/OpenA8DJ/ControlSurfaces
```

Those tools are for support, configuration, demonstrations, and experimental
workflows. They do not replace the HAL driver by themselves.

Use Control Center for normal options. The command-line control tool is still
installed for maintainers and diagnostics, but non-technical users should not
need it.

## More Help

- [Quick start](docs/user/quick-start.md)
- [Install guide](docs/user/install.md)
- [Traktor and Timecode Vinyl](docs/user/traktor-timecode.md)
- [Control Center](docs/user/control-center.md)
- [Troubleshooting](docs/user/troubleshooting.md)
- [Uninstall](docs/user/uninstall.md)

## Contributing

This is a preservation project run in the open. Useful contributions include:

- sound-quality reports with exact hardware/cable/software details
- Traktor and timecode validation
- routing and MIDI validation
- Windows/Linux experimental testing
- documentation fixes
- reproducible bug reports

Please use GitHub Issues:

- [Report a bug](https://github.com/fersantxez/OpenA8DJ/issues/new?template=bug_report.yml)
- [Request a feature](https://github.com/fersantxez/OpenA8DJ/issues/new?template=feature_request.yml)
- [View open issues](https://github.com/fersantxez/OpenA8DJ/issues)

## Follow Development

If you want to understand the architecture, validation process, roadmap,
experimental branches, or maintainer state, start with
[docs/README.md](docs/README.md).
