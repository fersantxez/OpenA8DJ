OpenA8DJ
========

OpenA8DJ is an open-source modern macOS driver stack for the Native Instruments
Audio 8 DJ.

Easy download page:

https://github.com/fersantxez/OpenA8DJ/releases/latest

Install:

1. Double-click the OpenA8DJ package in this disk image.
2. Follow the macOS Installer prompts.
3. Reconnect the Audio 8 DJ if it does not appear immediately.

If macOS blocks the installer with:

"OpenA8DJ-0.4.0.pkg" Not Opened
Apple could not verify "OpenA8DJ-0.4.0.pkg" is free of malware.

click Done, not Move to Trash. Then open System Settings > Privacy & Security,
find the blocked OpenA8DJ package in the Security section, and choose
Open Anyway.

This preview is ad-hoc signed but not Developer ID signed or Apple-notarized.
Only override Gatekeeper for official release assets from this repository, and
verify the SHA-256 checksum first.

The installer adds:

- /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
- /Library/LaunchAgents/org.opena8dj.midid.plist
- /usr/local/bin/opena8dj-control
- /usr/local/bin/opena8dj-midid
- /usr/local/bin/opena8dj-uninstall
- /Library/Documentation/OpenA8DJ

Uninstall:

sudo /usr/local/bin/opena8dj-uninstall

This project is independent and is not affiliated with, endorsed by, sponsored
by, or certified by Native Instruments. Product names are used only to identify
compatibility.
OpenA8DJ is released under the MIT License.
The MIT License covers the source code. It does not make modified builds,
forks, mirrors, support services, or repackaged installers official OpenA8DJ
releases.
Install and use OpenA8DJ at your own risk.

Current preview status:

- Version: 0.4.0
- Architecture: modern macOS C++ mainline, packaged as a Core Audio HAL preview
- Core Audio device: Open Audio 8 DJ
- Audio channels: 8 inputs / 8 outputs, named as stereo pairs A/B/C/D
- Validated playback rates: 44.1 kHz and 48 kHz
- Timecode/DVS input capture: exposed for Traktor validation
- Legacy C line: preserved separately on the repository legacy branch
- Windows: not included in this macOS release

This build is not notarized. The permanent fix is Developer ID signing plus
Apple notarization.

Maintainers must not publish a replacement end-user DMG/PKG unless the artifacts
were built with Developer ID identities, notarized by Apple, stapled, and passed
the repository signed-release verification gate.
