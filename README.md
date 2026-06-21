# OpenA8DJ

OpenA8DJ is an independent, open-source preservation driver for the Native
Instruments Audio 8 DJ on modern macOS.

It keeps the classic Audio 8 DJ workflow usable: 8-in/8-out Core Audio,
A/B/C/D deck routing, MIDI, and Traktor/timecode use. The project is not
affiliated with, endorsed by, sponsored by, or certified by Native Instruments,
and it does not include Native Instruments binaries, firmware, installers,
logos, or proprietary payloads.

## Support The Project

OpenA8DJ is independent and non-profit. If this helps you keep an Audio 8 DJ in
use, you can support the maintainer here:

[Buy me a coffee](https://ko-fi.com/fersantxez)

## Status

OpenA8DJ 0.5.0 is the current macOS C++ baseline.

- Core Audio HAL device: `Open Audio 8 DJ`
- 8 output channels as stereo deck pairs A/B/C/D
- 8 input channels as stereo input pairs A/B/C/D
- 44.1 kHz and 48 kHz validated as the primary rates
- CoreMIDI endpoints for Audio 8 DJ MIDI I/O
- Traktor Timecode Vinyl input active by default
- Optional Control Center for normal hardware profiles

Windows, Linux, Rust, and DriverKit branches are experimental or research work,
not the public release line.

## Download

Use the [latest GitHub release](https://github.com/fersantxez/OpenA8DJ/releases/latest).

Release assets:

- `OpenA8DJ-0.5.0.dmg`: driver installer
- `OpenA8DJ-0.5.0.pkg`: direct driver package
- `opena8dj-tools-0.5.0.dmg`: optional Control Center and support tools
- `opena8dj-tools-0.5.0.pkg`: direct tools package
- `OpenA8DJ-0.5.0-checksums.txt`: SHA-256 checksums

GitHub Actions artifacts are not release downloads.

## Install

Download `OpenA8DJ-0.5.0.dmg`, open it, and run the included package installer.

Install `opena8dj-tools-0.5.0.dmg` only if you also want
`OpenA8DJ Control Center.app` and the support tools.

After installation, select `Open Audio 8 DJ` in Audio MIDI Setup, Traktor, or
your audio application. If macOS blocks the package, or if you want checksum
verification, see the [install guide](docs/user/install.md).

If the macOS Installer app fails after the package is opened, the install guide
also includes a `sudo installer` fallback for the same downloaded package.

## Use

Typical output routing:

```text
1-2: deck/output A
3-4: deck/output B
5-6: deck/output C
7-8: deck/output D
```

For Traktor Timecode Vinyl, connect the turntables to the Audio 8 DJ inputs
you use for DVS and calibrate inside Traktor. OpenA8DJ keeps the vinyl input
path active by default.

Use `OpenA8DJ Control Center.app` for normal profile changes such as DVS Vinyl,
DVS CD/Line, Output Only, ground-lift flags, and support exports. The command
line tool is included for diagnostics, automation, and maintainer workflows.

## Validation

The 0.5.0 baseline was validated with:

- offline C++ and HAL safety tests;
- macOS package and signature checks;
- Audio MIDI Setup visibility as an 8-in/8-out device;
- CoreMIDI endpoint checks;
- real-music playback through Audio 8 DJ with real-time external capture;
- human listening sign-off on the installed artifact.

See the [public validation summary](docs/project/public-validation-summary.md)
and [release notes](docs/reference/release-notes-0.5.0.md).

## Documentation

- [Documentation index](docs/README.md)
- [Quick start](docs/user/quick-start.md)
- [Install guide](docs/user/install.md)
- [Traktor and Timecode Vinyl](docs/user/traktor-timecode.md)
- [Control Center](docs/user/control-center.md)
- [Architecture](docs/project/architecture.md)
- [Contributing](docs/project/contributing.md)
- [Release process](docs/project/release.md)
- [Development state and maintainer notes](docs-state/README.md)

## Contributing

Useful contributions include sound-quality reports, Traktor/timecode validation,
routing and MIDI validation, documentation fixes, and reproducible bug reports.

- [Report a bug](https://github.com/fersantxez/OpenA8DJ/issues/new?template=bug_report.yml)
- [Request a feature](https://github.com/fersantxez/OpenA8DJ/issues/new?template=feature_request.yml)
- [View open issues](https://github.com/fersantxez/OpenA8DJ/issues)

Please read [CONTRIBUTING.md](CONTRIBUTING.md) and the
[legal and publication policy](docs/reference/legal.md) before contributing
code or release material.

## License

OpenA8DJ is released under the MIT License. See [LICENSE](LICENSE).
