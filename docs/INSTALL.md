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

## If macOS blocks the package

The current preview is ad-hoc signed but not Developer ID signed or
Apple-notarized. On recent macOS versions, Gatekeeper may show:

```text
"OpenA8DJ-0.4.0.pkg" Not Opened
Apple could not verify "OpenA8DJ-0.4.0.pkg" is free of malware.
```

If the dialog only offers `Move to Trash` and `Done`:

1. Click `Done`.
2. Do not click `Move to Trash`.
3. Open System Settings.
4. Go to Privacy & Security.
5. In the Security section, find the blocked `OpenA8DJ-0.4.0.pkg` message.
6. Click `Open Anyway`.
7. Confirm the second warning, then continue with the installer.

Only do this for the official GitHub release assets from this repository. Check
the SHA-256 file before overriding Gatekeeper:

```sh
shasum -a 256 OpenA8DJ-0.4.0.dmg OpenA8DJ-0.4.0.pkg
cat OpenA8DJ-0.4.0-checksums.txt
```

Advanced fallback for testers, after verifying the checksum:

```sh
xattr -dr com.apple.quarantine /path/to/OpenA8DJ-0.4.0.pkg
open /path/to/OpenA8DJ-0.4.0.pkg
```

This is a preview workaround, not the final distribution experience. The final
fix is Developer ID signing and Apple notarization.

## Manual Terminal install while signing is pending

If Finder refuses to open the package, testers can install the official GitHub
PKG from Terminal after verifying the checksum. This is how the current GitHub
package was installed locally for validation while Apple Developer ID signing
is pending.

Download:

```sh
curl -L -O https://github.com/fersantxez/OpenA8DJ/releases/download/v0.4.0/OpenA8DJ-0.4.0.pkg
curl -L -O https://github.com/fersantxez/OpenA8DJ/releases/download/v0.4.0/OpenA8DJ-0.4.0-checksums.txt
```

Verify:

```sh
shasum -a 256 OpenA8DJ-0.4.0.pkg
grep OpenA8DJ-0.4.0.pkg OpenA8DJ-0.4.0-checksums.txt
```

The SHA-256 value printed by `shasum` must match the value in the checksum
file. Do not install if it differs.

Install:

```sh
sudo installer -pkg OpenA8DJ-0.4.0.pkg -target /
```

Reconnect the Audio 8 DJ if it does not appear immediately.

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

Local and CI builds may be ad-hoc signed. The 0.4.0 public preview is published
from GitHub Releases but is not Developer ID signed or Apple-notarized, so macOS
may block it until you approve it manually in Privacy & Security. A polished
end-user release should use a Developer ID Installer certificate and Apple
notarization so macOS Gatekeeper can verify the package without extra user
steps.

When an official signed release is available, the manual Privacy & Security
override should not be needed. The release maintainer must build with
`make release-signed`, notarize with Apple, staple the tickets, and pass
`make verify-signed-release` before replacing the GitHub assets.

## Historical branch note

The older C/Objective-C driver line is preserved on the `legacy` branch. That
branch is kept for comparison and recovery. New users who want the current
macOS driver should install the `main`/`0.4.0` release from GitHub Releases.
