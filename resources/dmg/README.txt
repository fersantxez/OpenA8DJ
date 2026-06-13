OpenA8DJ
========

OpenA8DJ is an open-source macOS driver stack for the Native Instruments Audio
8 DJ.

Download page:

https://github.com/fersantxez/OpenA8DJ/releases/latest

Install:

1. Double-click the OpenA8DJ package in this disk image.
2. Follow the macOS Installer prompts.
3. Reconnect the Audio 8 DJ if it does not appear immediately.

If macOS blocks the installer because it is from an unidentified developer,
open System Settings > Privacy & Security and approve it, or Control-click the
package and choose Open. This preview is ad-hoc signed but not Apple-notarized.

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

- Version: 0.3.25
- Core Audio device: Open Audio 8 DJ
- Audio channels: 8 inputs / 8 outputs, named as stereo pairs A/B/C/D
- Validated playback rates: 44.1 kHz and 48 kHz
- Timecode/DVS input capture: exposed for Traktor validation
- Windows: not included in this macOS release

This build is not notarized.
