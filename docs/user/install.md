# Install OpenA8DJ On macOS

OpenA8DJ is an independent open-source preservation driver for the Native
Instruments Audio 8 DJ. It is not affiliated with, endorsed by, sponsored by,
or certified by Native Instruments.

This guide installs the current macOS package: the OpenA8DJ audio driver, MIDI
support, Control Center, and command-line tools.

## Requirements

```text
Interface: Native Instruments Audio 8 DJ
Mac: Apple Silicon
Operating system: macOS 26 or later
Access: administrator password during installation
```

The 0.5.1 public binary does not run on Intel Macs.

## Download

Download the current stable release from:

```text
https://github.com/fersantxez/OpenA8DJ/releases/tag/v0.5.1
```

For OpenA8DJ 0.5.1, the release assets are:

```text
OpenA8DJ-0.5.1.dmg
OpenA8DJ-0.5.1-checksums.txt
```

The 0.5.1 DMG is self-contained; no second tools download is required. The
driver DMG and its embedded installer are Developer ID signed and
Apple-notarized.

## Verify The Download

The public release includes a checksum file. Verification is optional for a
normal install, but recommended if you want to confirm the download:

```sh
shasum -a 256 OpenA8DJ-0.5.1.dmg
grep OpenA8DJ-0.5.1.dmg OpenA8DJ-0.5.1-checksums.txt
```

The SHA-256 value printed by `shasum` must match the value in the checksum
file. Do not install if it differs.

## Normal Install

This is the normal install path:

1. Open `OpenA8DJ-0.5.1.dmg`.
2. Double-click `OpenA8DJ-0.5.1.pkg`.
3. Follow the macOS Installer prompts.
4. Restart the audio app if it was already open.
5. Reconnect the Audio 8 DJ if it does not appear immediately.

## If macOS Shows A Security Warning

The GitHub release is signed and notarized, so Gatekeeper should allow the
normal installation. If macOS says it cannot verify the DMG or PKG, stop and
download both release assets again from the official GitHub release. Verify the
checksum before trying again. Do not use `Open Anyway` for a file whose checksum
does not match.

## If The Installer Fails

If the package opens but macOS Installer reports an install error, verify the
checksum first, then install the package from the mounted DMG path:

```sh
sudo installer -pkg "/Volumes/OpenA8DJ 0.5.1/OpenA8DJ-0.5.1.pkg" -target /
```

## Installed Files

```text
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
/Library/LaunchAgents/org.opena8dj.midid.plist
/usr/local/bin/opena8dj-control
/usr/local/bin/opena8dj-midid
/usr/local/bin/opena8dj-uninstall
/Library/Documentation/OpenA8DJ
/Applications/OpenA8DJ Control Center.app
/Library/Documentation/OpenA8DJ/ControlSurfaces
```

## Verify The Install

After installation, macOS should show `Open Audio 8 DJ` as an audio device with
8 inputs and 8 outputs.

Open Audio MIDI Setup and confirm `Open Audio 8 DJ` appears with 8 inputs and 8
outputs. If it does not appear, reconnect the Audio 8 DJ once, then reopen the
audio app.

Primary validated rates:

```text
44100
48000
```

MIDI endpoints should appear as:

```text
Open Audio 8 DJ MIDI In
Open Audio 8 DJ MIDI Out
```

## Advanced Support Checks

These checks are for support and development from a source checkout. Normal
users do not need them for installation.

```sh
./build/audio-inspect
./build/audio-io-test 2 44100
./build/audio-io-test 2 48000
./build/midi-list
```

Open `/Applications/OpenA8DJ Control Center.app` to confirm that the profile
controls are available. `DVS Vinyl` is the default profile.

## Traktor / Timecode Vinyl

For Traktor Scratch / DVS, select `Open Audio 8 DJ` in Traktor and calibrate the
control vinyl there. OpenA8DJ keeps the vinyl input path active by default. If
you want to confirm or re-apply that state, open `OpenA8DJ Control Center.app`,
choose `DVS Vinyl`, and click `Apply`.

## Uninstall

Run:

```sh
sudo /usr/local/bin/opena8dj-uninstall
```

This unloads the MIDI bridge, removes installed files, and restarts Core Audio.

## More Help

- [Quick start](quick-start.md)
- [Traktor and Timecode Vinyl](traktor-timecode.md)
- [Control Center](control-center.md)
- [Troubleshooting](troubleshooting.md)
- [Uninstall](uninstall.md)
