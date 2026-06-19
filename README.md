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

Release assets are the only supported public binary downloads. GitHub Actions
artifacts are temporary build files and are not used as end-user distribution.

## Signing Status

OpenA8DJ 0.5.0 may be ad-hoc signed but is not yet Apple Developer ID signed
and notarized unless the release notes for the downloaded asset explicitly say
otherwise. macOS may therefore block the installer with a warning such as:

```text
"OpenA8DJ-0.5.0.pkg" Not Opened
Apple could not verify "OpenA8DJ-0.5.0.pkg" is free of malware.
```

This means Apple has not notarized this independent driver package yet. It does
not by itself mean the file changed or came from somewhere else.

If you choose to install the unsigned preview, use only official GitHub release
assets from this repository and verify the checksum first:

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

Normal path:

1. Download `OpenA8DJ-0.5.0.dmg` from GitHub Releases.
2. Open the DMG.
3. Double-click `OpenA8DJ-0.5.0.pkg`.
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

Detailed install notes are in [docs/INSTALL.md](docs/INSTALL.md).

## What Works In 0.5.0

OpenA8DJ 0.5.0 is the current macOS driver baseline.

- Core Audio device: `Open Audio 8 DJ`
- 8 output channels represented as stereo deck pairs A/B/C/D
- 8 input channels represented as stereo input pairs A/B/C/D
- 44.1 kHz and 48 kHz are the primary validated rates
- CoreMIDI endpoints for Audio 8 DJ MIDI I/O
- Traktor output routing for decks A/B/C/D
- Traktor Timecode Vinyl profile, including a low-noise variant
- Control bridge for input mode, ground-lift, software lock, routing transforms,
  stream statistics, and diagnostic state
- Optional Control Center and command-line tools for support workflows

The current release is meant to be useful, testable, and recoverable. It is not
claimed to be perfect. Please report hardware results, regressions, routing
issues, and timecode findings through GitHub Issues.

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
outputs to A/B/C/D as needed.

For vinyl timecode, put the hardware into the DVS profile:

```sh
/usr/local/bin/opena8dj-control profile timecode-vinyl
```

If your rig has audible computer/CPU-like background noise while the Traktor
scope is otherwise stable, try the reversible low-noise variant:

```sh
/usr/local/bin/opena8dj-control profile timecode-vinyl-low-noise
```

Validate the Traktor scope after changing profiles. If the scope degrades,
return to `profile timecode-vinyl`.

See [docs/TRAKTOR_TIMECODE.md](docs/TRAKTOR_TIMECODE.md) for the DVS checklist.

## Tools And Control Center

The driver installer includes the command-line control tool:

```sh
/usr/local/bin/opena8dj-control
```

Useful examples:

```sh
opena8dj-control list-profiles
opena8dj-control apply-preset traktor-dvs-vinyl
opena8dj-control profile timecode-vinyl-low-noise
opena8dj-control stream-stats
opena8dj-control export-config ~/Desktop/opena8dj-config.json
```

The optional `opena8dj-tools-0.5.0.dmg` installs:

```text
/Applications/OpenA8DJ Control Center.app
/usr/local/bin/opena8dj-control
/Library/Documentation/OpenA8DJ/ControlSurfaces
```

Those tools are for support, configuration, demonstrations, and experimental
workflows. They do not replace the HAL driver by themselves.

## Experimental Platforms

macOS is the only current canonical driver line.

Windows and Linux support are experimental community areas. They are not part of
the validated macOS driver path, and they should not be assumed to work without
platform-specific testing.

- Windows: experimental work belongs under Windows-specific branches and paths.
- Linux: experimental work belongs under Linux-specific branches and paths.
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
