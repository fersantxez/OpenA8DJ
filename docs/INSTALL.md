# Installing OpenA8DJ On macOS

OpenA8DJ is an independent open-source preservation driver for the Native
Instruments Audio 8 DJ. It is not affiliated with, endorsed by, sponsored by,
or certified by Native Instruments.

This guide installs the current macOS package: a user-space Core Audio HAL
driver, MIDI support, and the control tools needed to switch Audio 8 DJ
profiles such as `timecode-vinyl`.

## Download

Download the latest release from:

```text
https://github.com/fersantxez/OpenA8DJ/releases/latest
```

For OpenA8DJ 0.5.0, the expected public assets are:

```text
OpenA8DJ-0.5.0.dmg
OpenA8DJ-0.5.0.pkg
OpenA8DJ-0.5.0-checksums.txt
opena8dj-tools-0.5.0.dmg
opena8dj-tools-0.5.0.pkg
```

Use the `OpenA8DJ` package for the driver. Use the separate `opena8dj-tools`
package only when you want the optional Control Center/support tools without
reinstalling the driver.

## Verify The Download

Before overriding any macOS security prompt, verify the checksum:

```sh
shasum -a 256 OpenA8DJ-0.5.0.pkg
grep OpenA8DJ-0.5.0.pkg OpenA8DJ-0.5.0-checksums.txt
```

The SHA-256 value printed by `shasum` must match the value in the checksum
file. Do not install if it differs.

## Normal Install

1. Open `OpenA8DJ-0.5.0.dmg`.
2. Double-click `OpenA8DJ-0.5.0.pkg`.
3. Follow the macOS Installer prompts.
4. Reconnect the Audio 8 DJ if it does not appear immediately.

## If macOS Blocks The Package

OpenA8DJ 0.5.0 may be ad-hoc signed but not yet Apple Developer ID signed and
notarized unless the release notes explicitly say otherwise. Recent macOS
versions may show:

```text
"OpenA8DJ-0.5.0.pkg" Not Opened
Apple could not verify "OpenA8DJ-0.5.0.pkg" is free of malware.
```

If the dialog only offers `Move to Trash` and `Done`:

1. Click `Done`.
2. Do not click `Move to Trash`.
3. Open System Settings.
4. Go to Privacy & Security.
5. In the Security section, find the blocked `OpenA8DJ-0.5.0.pkg` message.
6. Click `Open Anyway`.
7. Confirm the second warning, then continue with the installer.

Terminal fallback after checksum verification:

```sh
sudo installer -pkg OpenA8DJ-0.5.0.pkg -target /
```

This is a preview workaround, not the final distribution experience. The final
fix is Developer ID signing and Apple notarization.

## Installed Files

```text
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
/Library/LaunchAgents/org.opena8dj.midid.plist
/usr/local/bin/opena8dj-control
/usr/local/bin/opena8dj-midid
/usr/local/bin/opena8dj-uninstall
/Library/Documentation/OpenA8DJ
```

Optional tools package:

```text
/Applications/OpenA8DJ Control Center.app
/usr/local/bin/opena8dj-control
/Library/Documentation/OpenA8DJ/ControlSurfaces
```

## Verify The Install

After installation, macOS should show `Open Audio 8 DJ` as an audio device with
8 inputs and 8 outputs.

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

Useful local checks from a source checkout:

```sh
./build/audio-inspect
./build/audio-io-test 2 44100
./build/audio-io-test 2 48000
./build/midi-list
```

Useful installed control checks:

```sh
/usr/local/bin/opena8dj-control
/usr/local/bin/opena8dj-control list-profiles
/usr/local/bin/opena8dj-control profile timecode-vinyl
```

## Traktor / Timecode Vinyl

For Traktor Scratch / DVS:

```sh
/usr/local/bin/opena8dj-control profile timecode-vinyl
```

If the rig has audible CPU-like background noise while the Traktor scope is
otherwise stable:

```sh
/usr/local/bin/opena8dj-control profile timecode-vinyl-low-noise
```

Validate the scope in Traktor after changing profiles. If signal quality gets
worse, switch back to `profile timecode-vinyl`.

## Uninstall

Run:

```sh
sudo /usr/local/bin/opena8dj-uninstall
```

This unloads the MIDI bridge, removes installed files, and restarts Core Audio.

Optional tools uninstall:

```sh
sudo /Library/Documentation/OpenA8DJ/ControlSurfaces/uninstall-opena8dj-control-surfaces.sh
```

## Signing Note For Maintainers

Local and CI builds may be ad-hoc signed. A polished end-user release should
use a Developer ID Installer certificate and Apple notarization so macOS
Gatekeeper can verify the package without extra user steps.

Release maintainers must build with Developer ID identities, notarize with
Apple, staple the tickets, and pass the signed-release verification gate before
claiming that the public assets are officially signed/notarized.

## Historical Branch Note

The older C/Objective-C driver line is preserved on the `legacy` branch for
reference. New users who want the current macOS driver should install the
latest 0.5.x release from GitHub Releases.
