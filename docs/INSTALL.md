# Installing OpenA8DJ

OpenA8DJ is an independent open-source preservation driver for the Audio 8 DJ.
It is not affiliated with, endorsed by, sponsored by, or certified by Native
Instruments.

This guide installs the current macOS package: a user-space Core Audio HAL
driver, a MIDI/control LaunchAgent, and the command-line tools needed to switch
Audio 8 DJ profiles such as `timecode-vinyl`.

## Two-click install

1. Download the latest public preview DMG from
   [GitHub Releases](https://github.com/fersantxez/OpenA8DJ/releases/latest).
2. Open the DMG.
3. Double-click the `OpenA8DJ-<version>.pkg` package inside the DMG.
4. Follow the macOS Installer prompts.
5. Unplug and reconnect the Audio 8 DJ if the device does not appear
   immediately.

Installed files:

```text
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
/Library/LaunchAgents/org.opena8dj.midid.plist
/usr/local/bin/opena8dj-control
/usr/local/bin/opena8dj-midid
/usr/local/bin/opena8dj-uninstall
/Library/Documentation/OpenA8DJ
```

## Verify

After installation, macOS should show `Open Audio 8 DJ` as an audio device with
8 inputs and 8 outputs in the 0.4.0 preview. The locally validated
playback/topology rates are:

```text
44100
48000
```

Some builds may expose 88.2/96 kHz for extended testing. Do not treat those
rates as production-ready until the matching release notes say they have passed
the same validation as 44.1/48 kHz.

MIDI endpoints should appear as:

```text
Open Audio 8 DJ MIDI In
Open Audio 8 DJ MIDI Out
```

To inspect the device from Terminal:

```sh
/usr/local/bin/opena8dj-control
```

For private builds, release checksums are published next to the DMG and PKG:

```sh
shasum -a 256 OpenA8DJ-<version>.dmg OpenA8DJ-<version>.pkg
```

## Uninstall

Run:

```sh
sudo /usr/local/bin/opena8dj-uninstall
```

This unloads the MIDI bridge, removes installed files, and restarts Core Audio.

## Signing note

Local and CI builds may be ad-hoc signed. The 0.4.0 public preview is published
from GitHub Releases but is not Developer ID signed or Apple-notarized, so macOS
may ask you to approve an unidentified installer. A polished end-user release
should use a Developer ID Installer certificate and Apple notarization so macOS
Gatekeeper can verify the package without extra user steps.

## Historical branch note

The older C/Objective-C driver line is preserved on the `legacy` branch. That
branch is kept for comparison and recovery. New users who want the current
macOS driver should install the `main`/`0.4.0` release from GitHub Releases.
