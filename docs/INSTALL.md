# Installing OpenA8DJ

OpenA8DJ installs a Core Audio HAL driver, a MIDI/control LaunchAgent, and
command-line control and diagnostic tools.

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
/usr/local/bin/opena8dj-hardware-profiler
/usr/local/bin/opena8dj-midid
/usr/local/bin/opena8dj-uninstall
/Library/Application Support/OpenA8DJ/hardware-profiler-known-issues-v1.json
/Library/Documentation/OpenA8DJ
```

The hardware profiler is read-only. Run `opena8dj-hardware-profiler` for a
concise report or `opena8dj-hardware-profiler --json` for one support-redacted
JSON document. Its installed `HARDWARE_PROFILER.md` guide documents privacy,
status/exit semantics, evidence limitations, and offline catalog overrides.

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

## opena8dj-tools Installer

OpenA8DJ has two control surfaces:

- `/usr/local/bin/opena8dj-control`: scriptable engineering and recovery CLI.
- `OpenA8DJ Control Center`: native macOS app for presets, status, import/export,
  and safe profile switching.

These tools have their own installer, separate from the full driver package.
Use it when the HAL driver is already installed and you only need to update the
CLI/panel/documentation:

```sh
make tools-package
sudo installer -pkg build/opena8dj-tools-<version>.pkg -target /
```

To build the separate DMG:

```sh
make tools-dmg
open build/opena8dj-tools-<version>.dmg
```

The opena8dj-tools installer adds:

```text
/Applications/OpenA8DJ Control Center.app
/usr/local/bin/opena8dj-control
/Library/Documentation/OpenA8DJ/ControlSurfaces
```

It does not install or replace:

```text
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
/Library/LaunchAgents/org.opena8dj.midid.plist
/usr/local/bin/opena8dj-midid
```

Build and open the local app without packaging:

```sh
make control-center
open build/OpenA8DJControlCenter.app
```

Manual local install without creating a package:

```sh
make install-control-surfaces
```

Uninstall only the control surfaces:

```sh
sudo /Library/Documentation/OpenA8DJ/ControlSurfaces/uninstall-opena8dj-control-surfaces.sh
```

The local app bundle embeds the matching `opena8dj-control` binary in
`Contents/Resources`, so the UI and CLI use the same backend. Local builds are
ad-hoc signed for development; public distribution still requires Developer ID
signing and Apple notarization.

Detailed usage, presets, cabling workflows, and diagrams are in
[Control surfaces user guide](AUDIO8DJ_CONTROL_SURFACES_USER_GUIDE.md).
The planned recorded demo is documented in
[Control surfaces demo runbook](AUDIO8DJ_CONTROL_SURFACES_DEMO_RUNBOOK_2026-06-19.md).

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
