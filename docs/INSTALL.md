# Installing OpenA8DJ

OpenA8DJ installs a Core Audio HAL driver, a MIDI/control LaunchAgent, and two
command-line tools.

## Two-click install

1. Download the latest public preview DMG from
   [GitHub Releases](https://github.com/fersantxez/OpenA8DJ/releases).
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
8 inputs and 8 outputs in the 0.3.25 preview. The locally validated
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

Local and CI builds may be ad-hoc signed. The 0.3.25 public preview is published
from GitHub Releases but is not Developer ID signed or Apple-notarized, so macOS
may ask you to approve an unidentified installer. A polished end-user release
should use a Developer ID Installer certificate and Apple notarization so macOS
Gatekeeper can verify the package without extra user steps.

## Windows note

Windows builds are distributed from
[GitHub Releases](https://github.com/fersantxez/OpenA8DJ/releases) as
experimental driver ZIP packages. They are not production MSIs yet.

Use the Windows ZIPs only on test systems that can install test-signed or
experimental drivers. See [Windows status](WINDOWS.md) before installing.
