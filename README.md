# OpenA8DJ

OpenA8DJ lets a Native Instruments Audio 8 DJ work as an audio and MIDI
interface on current macOS.

Install it when you want to keep using the Audio 8 DJ with Traktor, a DJ mixer,
turntables, CDJs, or another Core Audio application. The installer adds the
audio driver, MIDI support, and a Control Center app for choosing how the
interface is connected.

OpenA8DJ is an independent, open-source preservation project. It is not
affiliated with, endorsed by, sponsored by, or certified by Native Instruments.

**Support the project:** [Buy me a coffee](https://ko-fi.com/fersantxez)

## Is This The Right Driver?

OpenA8DJ 0.5.1 is for this setup:

| | Requirement |
| --- | --- |
| Audio interface | Native Instruments Audio 8 DJ |
| Mac | Apple Silicon |
| Operating system | macOS 26 or later |
| Account | Administrator access during installation |
| Audio rates | 44.1 kHz and 48 kHz |

This release does not run on Intel Macs and should not be installed for a
different Native Instruments interface. Windows and Linux work is experimental
and is not included in the macOS download.

## What You Get

After installation, macOS and audio applications see:

- one device named `Open Audio 8 DJ`;
- 8 inputs, arranged as stereo pairs A, B, C, and D;
- 8 outputs, arranged as stereo pairs A, B, C, and D;
- `Open Audio 8 DJ MIDI In` and `Open Audio 8 DJ MIDI Out`;
- OpenA8DJ Control Center in the Applications folder.

The default profile is ready for Traktor Timecode Vinyl. You can choose another
profile in Control Center without using Terminal.

## Download

Download the current stable release from:

[OpenA8DJ 0.5.1 for macOS](https://github.com/fersantxez/OpenA8DJ/releases/tag/v0.5.1)

The release contains two files:

| File | Purpose |
| --- | --- |
| `OpenA8DJ-0.5.1.dmg` | The installer |
| `OpenA8DJ-0.5.1-checksums.txt` | An optional download-integrity check |

You only need the DMG to install OpenA8DJ. Do not use packages copied from
mirrors, forums, or GitHub Actions.

## Install

1. Download `OpenA8DJ-0.5.1.dmg`.
2. Double-click the DMG to open it.
3. Double-click `OpenA8DJ-0.5.1.pkg` inside the window.
4. Follow the macOS Installer steps and enter your Mac password when asked.
5. Connect or reconnect the Audio 8 DJ.
6. Close and reopen Traktor or any audio application that was already running.

The public installer is Developer ID signed and Apple-notarized. A normal
download should open without using **Open Anyway** or disabling macOS security.

For checksum verification and the documented installer fallback, see the
[full installation guide](docs/user/install.md).

## Confirm That It Is Working

1. Open **Audio MIDI Setup** on the Mac.
2. Select **Open Audio 8 DJ**.
3. Confirm that it shows 8 inputs and 8 outputs.
4. Open your DJ or audio application.
5. Select **Open Audio 8 DJ** as its audio device.

The four output pairs correspond to the labels printed on the interface:

| Application channels | Audio 8 DJ output |
| --- | --- |
| 1-2 | A |
| 3-4 | B |
| 5-6 | C |
| 7-8 | D |

If the device is not listed, reconnect it once and reopen the application.
Follow [Troubleshooting](docs/user/troubleshooting.md) if it still does not
appear.

## Traktor And Timecode Vinyl

For a typical two-turntable DVS setup:

1. Connect the left turntable to Audio 8 DJ input A.
2. Connect the right turntable to Audio 8 DJ input B.
3. Connect outputs A and B to the corresponding mixer channels.
4. Select **Open Audio 8 DJ** as Traktor's audio device.
5. Assign Traktor's input and output pairs.
6. Calibrate each control vinyl inside Traktor.

The installer selects the **DVS Vinyl** profile by default. If Traktor does not
receive a usable timecode signal, open **OpenA8DJ Control Center**, select
**DVS Vinyl**, click **Apply**, and calibrate again.

See [Traktor and Timecode Vinyl](docs/user/traktor-timecode.md) for the complete
setup and calibration guide.

## Control Center

Open **OpenA8DJ Control Center** from the Applications folder when you change
how the interface is being used.

Common profiles include:

- **DVS Vinyl** for control vinyl connected to phono inputs;
- **DVS CD-Line** for CDJs or line-level timecode;
- **Playback / 4 Stereo Outputs** when using the interface only for playback;
- **Vinyl Recording** for recording records through the phono inputs;
- **DJ Set Recording** for recording a mixer output through line inputs.

Choose a profile and click **Apply**. The setting belongs to the interface
workflow, not to a particular Traktor deck.

See the [Control Center guide](docs/user/control-center.md) for every profile.

## Remove OpenA8DJ

Open Terminal and run:

```sh
sudo /usr/local/bin/opena8dj-uninstall
```

Enter your Mac password when asked. The uninstaller removes the driver, MIDI
support, Control Center, command-line tools, and installed documentation. See
the [uninstall guide](docs/user/uninstall.md) for the final check.

## Known Limits

- The stable public installer is currently Apple Silicon only.
- 44.1 kHz and 48 kHz are the primary supported rates.
- The release has been used successfully with Traktor Timecode Vinyl, but it is
  not an official Native Instruments-certified driver.
- Windows, Linux, Rust, and DriverKit development live in separate experimental
  branches and are not user-ready alternatives to this macOS release.

## Help And Documentation

- [Quick start](docs/user/quick-start.md)
- [Installation](docs/user/install.md)
- [Traktor and Timecode Vinyl](docs/user/traktor-timecode.md)
- [Control Center](docs/user/control-center.md)
- [Troubleshooting](docs/user/troubleshooting.md)
- [Uninstall](docs/user/uninstall.md)
- [Documentation index](docs/README.md)

For a reproducible problem, include the macOS version, application and version,
sample rate, buffer size, physical routing, and selected Control Center profile.

- [Report a bug](https://github.com/fersantxez/OpenA8DJ/issues/new?template=bug_report.yml)
- [Request a feature](https://github.com/fersantxez/OpenA8DJ/issues/new?template=feature_request.yml)
- [View open issues](https://github.com/fersantxez/OpenA8DJ/issues)

## Project

Developers and contributors can start with:

- [Architecture](docs/project/architecture.md)
- [Contributing](CONTRIBUTING.md)
- [Release process](docs/project/release.md)
- [Legal and publication policy](docs/reference/legal.md)

OpenA8DJ is released under the [MIT License](LICENSE). Product names are used
only to identify compatibility. The repository and release packages do not
contain Native Instruments driver binaries, firmware, installers, logos, or
other proprietary vendor payloads.
