# OpenA8DJ

OpenA8DJ is an open-source macOS driver stack for the Native Instruments
Audio 8 DJ USB audio interface.

It currently ships as a Core Audio HAL driver with an IOUSBHost-based CAIAQ USB
transport, a CoreMIDI bridge, an Audio 8 DJ control bridge, and a two-click macOS
installer.

## Why This Exists

OpenA8DJ is a non-profit preservation project. We are long-time fans of the
Audio 8 DJ: the way it sounds, the channel layout, the routing options, and the
Traktor/timecode workflows made it a classic piece of DJ hardware. It would be
a shame for a useful, good-sounding interface to disappear just because the
original driver stopped working on modern systems.

This project exists so people who still love this hardware can keep using it.
It is not a commercial product, and there is no profit motive behind it. Any
optional support goes toward practical project costs such as test hardware,
signing, packaging, and ongoing maintenance.

## Download

The recommended download location is
[GitHub Releases](https://github.com/fersantxez/OpenA8DJ/releases).
Release assets are the only supported public binary downloads; GitHub Actions
artifacts are temporary CI files and are not used for distribution.

Current public preview downloads:

- [OpenA8DJ 0.2.6 public preview release](https://github.com/fersantxez/OpenA8DJ/releases/tag/v0.2.6)
- [macOS DMG installer](https://github.com/fersantxez/OpenA8DJ/releases/download/v0.2.6/OpenA8DJ-0.2.6.dmg)
- [macOS PKG installer](https://github.com/fersantxez/OpenA8DJ/releases/download/v0.2.6/OpenA8DJ-0.2.6.pkg)
- [SHA-256 checksums](https://github.com/fersantxez/OpenA8DJ/releases/download/v0.2.6/OpenA8DJ-0.2.6-checksums.txt)

Important: these are public preview builds. The macOS packages are not yet
Developer ID signed or Apple-notarized. Any Windows packages are experimental
driver packages, not a production MSI. See the release notes before installing.
Install and use OpenA8DJ at your own risk.

## Support

If you wanna thank me, just [buy me a coffee](https://ko-fi.com/fersantxez).

## Bugs And Feature Requests

Please use GitHub Issues for bugs, regressions, hardware validation results,
and feature requests:

- [Report a bug](https://github.com/fersantxez/OpenA8DJ/issues/new?template=bug_report.yml)
- [Request a feature](https://github.com/fersantxez/OpenA8DJ/issues/new?template=feature_request.yml)
- [View open issues](https://github.com/fersantxez/OpenA8DJ/issues)

## Current Status

OpenA8DJ 0.2.6 is validated locally on Apple Silicon/macOS 26.5:

- macOS enumerates `Open Audio 8 DJ` as 8 inputs and 8 outputs.
- The HAL exposes 4 stereo input streams and 4 stereo output streams:
  Input A/B/C/D and Output A/B/C/D.
- Traktor sees the driver and accepts it as an audio device.
- Output A and Output B route independently in Traktor.
- 44.1 and 48 kHz playback are working in local listening tests.
- Core Audio buffer-size control is implemented through both frame-based and
  legacy byte-based properties.
- CoreMIDI endpoints appear as `Open Audio 8 DJ MIDI In` and
  `Open Audio 8 DJ MIDI Out`.
- The Timecode Vinyl hardware profile has passed initial Traktor operator
  validation, including a DVS capture phase fix for Input B/D.
- A macOS DMG/PKG installer is generated and verified locally.

Remaining release gates:

- Physical validation of every input pair.
- Full DVS matrix validation for vinyl and CD/line modes across every input
  pair.
- 88.2/96 kHz production-quality validation.
- Public distribution through Developer ID signing and Apple notarization.

## Install

1. Download the latest `OpenA8DJ-<version>.dmg` from the release page.
2. Open the DMG.
3. Double-click `OpenA8DJ-<version>.pkg`.
4. Follow the macOS Installer prompts.
5. Reconnect the Audio 8 DJ if it does not appear immediately.

Installed files:

```text
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
/Library/LaunchAgents/org.opena8dj.midid.plist
/usr/local/bin/opena8dj-control
/usr/local/bin/opena8dj-midid
/usr/local/bin/opena8dj-uninstall
/Library/Documentation/OpenA8DJ
```

Uninstall:

```sh
sudo /usr/local/bin/opena8dj-uninstall
```

## Build

```sh
make clean
make all
make dist
```

Generated artifacts:

```text
build/OpenA8DJ-<version>.pkg
build/OpenA8DJ-<version>.dmg
build/OpenA8DJ-<version>-checksums.txt
```

## Local Validation

After installing:

```sh
./build/audio-inspect
./build/audio-io-test 2 44100
./build/audio-io-test 2 48000
./build/midi-list
/usr/local/bin/opena8dj-control
```

Pair routing:

```sh
./build/audio-pair-tone A 3 440 0.06
./build/audio-pair-tone B 3 660 0.06
./build/audio-pair-tone C 3 880 0.06
./build/audio-pair-tone D 3 1100 0.06
```

## Traktor Timecode

The driver exposes the required stereo inputs and hardware control profile for
Traktor Scratch/timecode use. Initial Timecode Vinyl validation has passed, but
the full release gate still requires every physical input pair and CD/line mode
to be tested.

For vinyl timecode testing, put the interface in the hardware DVS profile first:

```sh
/usr/local/bin/opena8dj-control profile timecode-vinyl
```

Use [docs/TRAKTOR_TIMECODE.md](docs/TRAKTOR_TIMECODE.md) for the test plan.

## Windows

OpenA8DJ 0.2.6 is primarily a macOS release. The repository now also contains
an experimental Windows 10/11 WDK workstream under `windows/`.

The Windows package builds a test-signed KMDF driver, INF/catalog package, and
`opena8djctl.exe` control tool. It exposes the OpenA8DJ 8-in/8-out capability
contract and hardware-control API, but it has not been exhaustively validated on
Windows with the physical interface. Treat Windows builds as experimental and
send feedback/logs.

Windows support is tracked separately in [docs/WINDOWS.md](docs/WINDOWS.md).
Experimental Windows ZIPs from earlier previews remain available in
[GitHub Releases](https://github.com/fersantxez/OpenA8DJ/releases), but Windows
artifacts are rebuilt and validated separately from the macOS HAL releases.

## Architecture

```text
Core Audio clients
      |
      v
OpenA8DJ HAL plug-in
      |
      +-- IOUSBHost transport
      |     - EP1 CAIAQ command channel
      |     - isochronous capture endpoint 0x82
      |     - isochronous playback endpoint 0x06
      |
      +-- local IPC socket /tmp/opena8dj-control.sock
            |
            v
       opena8dj-midid LaunchAgent
            |
            v
       CoreMIDI endpoints and control tool
```

OpenA8DJ is a new user-space implementation based on live hardware testing,
public macOS APIs, public USB descriptors, public hardware specifications, and
original project test results.

## Documentation

- [Install guide](docs/INSTALL.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Testing](docs/TESTING.md)
- [Traktor timecode test plan](docs/TRAKTOR_TIMECODE.md)
- [Windows status](docs/WINDOWS.md)
- [Roadmap to production quality](docs/ROADMAP_TO_PRO.md)
- [Release process](docs/RELEASE.md)
- [Legal and publication policy](docs/LEGAL.md)
- [Brand and risk policy](BRAND_POLICY.md)
- [Feasibility and background](FEASIBILITY.md)
- [Notice](NOTICE.md)
- [Contributing](CONTRIBUTING.md)

## License

OpenA8DJ is released under the [MIT License](LICENSE).

## Legal

OpenA8DJ is an independent project and is not affiliated with, endorsed by, or
sponsored by Native Instruments. Native Instruments, Audio 8 DJ, Traktor, and
other product names are trademarks of their respective owners and are used only
to identify compatibility.

This repository does not include Native Instruments driver binaries, firmware
blobs, installers, logos, or other proprietary vendor payloads. The project is
intended as an original implementation using live hardware testing, public macOS
APIs, public USB descriptors, public hardware specifications, and original
project test results.

Public releases must pass the provenance and trademark checks in
[docs/LEGAL.md](docs/LEGAL.md). In particular, do not copy third-party
implementation code under incompatible license terms into this MIT-licensed
repository, and do not publish binary artifacts that contain proprietary vendor
payloads.

The MIT License covers the source code, but it does not make modified builds,
forks, mirrors, support services, or repackaged installers official OpenA8DJ
releases. See [Brand and Risk Policy](BRAND_POLICY.md).
