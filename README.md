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

## Download For macOS

The canonical OpenA8DJ line is the macOS C++ driver stack. Start here if you
want to use an Audio 8 DJ on a Mac.

- [Latest release](https://github.com/fersantxez/OpenA8DJ/releases/latest)
- `OpenA8DJ-0.5.0.dmg`: main driver installer
- `OpenA8DJ-0.5.0.pkg`: direct installer package
- `OpenA8DJ-0.5.0-checksums.txt`: SHA-256 checksums
- `opena8dj-tools-0.5.0.dmg`: optional control tools and Control Center
- `opena8dj-tools-0.5.0.pkg`: optional tools direct installer package

Release assets are the only supported public binary downloads. GitHub Actions
artifacts are temporary build files and are not used as end-user distribution.

## Signing Status

OpenA8DJ 0.5.0 release assets are unsigned, not Developer ID signed, and not
Apple-notarized. macOS Gatekeeper will likely block them unless manually
approved after checksum verification. macOS may show a warning such as:

```text
"OpenA8DJ-0.5.0.pkg" Not Opened
Apple could not verify "OpenA8DJ-0.5.0.pkg" is free of malware.
```

This means Apple has not notarized this independent driver package. It does not
by itself mean the file changed or came from somewhere else.

Use only official GitHub release assets from this repository and verify the
checksum before approving any blocked installer:

```sh
shasum -a 256 OpenA8DJ-0.5.0.pkg
grep OpenA8DJ-0.5.0.pkg OpenA8DJ-0.5.0-checksums.txt
```

The two SHA-256 values must match. Do not install if they differ.

If Finder blocks the package, the current manual install path is:

```sh
sudo installer -pkg OpenA8DJ-0.5.0.pkg -target /
```

Alternatively, click `Done`, open System Settings -> Privacy & Security, find
the blocked OpenA8DJ package, and choose `Open Anyway`.

The permanent distribution goal is Developer ID signing plus Apple notarization
so the normal double-click DMG/PKG install works without this manual approval.

## Install

### Simple Step-By-Step Install

1. Open the [latest release](https://github.com/fersantxez/OpenA8DJ/releases/latest).
2. Download `OpenA8DJ-0.5.0.dmg`.
3. Open the downloaded DMG file.
4. Double-click `OpenA8DJ-0.5.0.pkg`.
5. Follow the macOS Installer prompts.
6. If macOS blocks the installer, click `Done`, open System Settings, go to
   Privacy & Security, and choose `Open Anyway` for OpenA8DJ.
7. Reconnect the Audio 8 DJ if it does not appear immediately.

After install, open Audio MIDI Setup and confirm `Open Audio 8 DJ` appears with
8 inputs and 8 outputs. If it does not appear, reconnect the Audio 8 DJ once,
then reopen the audio app.

### Basic Use After Install

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

Detailed install notes are in [docs/INSTALL.md](docs/INSTALL.md).

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
- Control bridge for input mode, ground-lift, software lock, routing transforms,
  stream statistics, and diagnostic state
- Optional Control Center and command-line tools for support workflows

The current release is meant to be useful, testable, and recoverable. It is not
claimed to be perfect. Please report hardware results, regressions, routing
issues, and timecode findings through GitHub Issues.

For a public, non-internal summary of what was validated, see
[docs/PUBLIC_VALIDATION_SUMMARY.md](docs/PUBLIC_VALIDATION_SUMMARY.md).

## Modern macOS Architecture

The `main` branch is the macOS product line. It is built around a modern
user-space macOS stack:

- Core Audio HAL plug-in for the current installable driver
- IOUSBHost transport for the Audio 8 DJ USB interface
- CoreMIDI bridge for MIDI I/O
- C++ core contracts for packet layout, channel topology, routing, timecode
  policy, buffer policy, and metrics
- macOS tooling for profile control, diagnostics, installation, and validation
- DriverKit/AudioDriverKit scaffolding as the forward System Extension path

The older macOS kernel-extension audio model is not the project direction. The
real-time audio path is kept separate from installation, UI, logging, heavy
diagnostics, and other non-audio work.

## Traktor And Timecode Vinyl

For Traktor, select `Open Audio 8 DJ` as the audio device and assign the deck
outputs to A/B/C/D as needed. For vinyl timecode, connect the turntables to
inputs A/B and calibrate the control vinyl inside Traktor. OpenA8DJ keeps the
vinyl input path active by default; no extra setup step is needed for normal
vinyl use.

If you want to confirm or re-apply the normal vinyl state, open
`OpenA8DJ Control Center.app`, choose `DVS Vinyl`, and click `Apply`.

See [docs/TRAKTOR_TIMECODE.md](docs/TRAKTOR_TIMECODE.md) for the DVS checklist.

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

## Experimental Platforms

macOS `main` plus GitHub Releases are the only user-facing driver line.

Windows, Linux, and Rust branches are public for research and continuity only.
They are not validated release branches, and they should not be assumed to work
without platform-specific testing.

- Windows: `windows/rebuild-surface` is experimental and not validated.
- Linux: `linux/full-driver-agent` is experimental and not validated.
- Rust: `rust/modular-core-spike` is a lab/oracle branch, not the macOS runtime.
- Feedback is welcome, especially hardware reports and reproducible logs.

See [docs/PLATFORM_SUPPORT.md](docs/PLATFORM_SUPPORT.md) for the current matrix.

## Legacy And Research Branches

The old C/Objective-C implementation is preserved on the `legacy` branch as a
historical reference. It captured useful reverse-engineering and physical-test
knowledge, including behavior inspired by Linux CAIAQ / `snd-usb-caiaq` work,
but it is not the recommended user-facing driver.

The Rust work is kept as an experimental lab/oracle branch. It may help with
tests, metrics, analyzers, and future ideas, but it is not the runtime direction
of the macOS `main` branch.

## Build From Source

```sh
make clean
make all
make dist
```

Generated local artifacts:

```text
build/OpenA8DJ-0.5.0.pkg
build/OpenA8DJ-0.5.0.dmg
build/opena8dj-tools-0.5.0.pkg
build/opena8dj-tools-0.5.0.dmg
build/OpenA8DJ-0.5.0-checksums.txt
```

Release maintainers should use Developer ID identities, notarize the artifacts,
staple notarization tickets, and run the signed-release verification gate before
replacing public GitHub assets.

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

If you want to thank the maintainer, you can
[buy me a coffee](https://ko-fi.com/fersantxez).
