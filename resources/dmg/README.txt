OpenA8DJ 0.5.0
==============

OpenA8DJ is an independent, open-source preservation driver for the Native
Instruments Audio 8 DJ.

It is not affiliated with, endorsed by, sponsored by, or certified by Native
Instruments. Product names are used only to identify compatibility.

Install:

1. Double-click OpenA8DJ-0.5.0.pkg.
2. Follow the macOS Installer prompts.
3. Reconnect the Audio 8 DJ if it does not appear immediately.

Use the OpenA8DJ files attached to the GitHub Release. Do not install copies
from mirrors or unknown download sites.

The installer adds:

- /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
- /Library/LaunchAgents/org.opena8dj.midid.plist
- /usr/local/bin/opena8dj-control
- /usr/local/bin/opena8dj-midid
- /usr/local/bin/opena8dj-uninstall
- /Library/Documentation/OpenA8DJ

Uninstall:

sudo /usr/local/bin/opena8dj-uninstall

Current status:

- Version: 0.5.0
- Architecture: modern macOS C++ user-space driver stack
- Core Audio device: Open Audio 8 DJ
- Audio channels: 8 inputs / 8 outputs, named as stereo pairs A/B/C/D
- Primary validated playback rates: 44.1 kHz and 48 kHz
- Traktor Timecode Vinyl: vinyl input path active by default
- Stable sound profile: CPU pool profile validated with real-time external
  recording and human listening on 2026-06-20
- Optional tools: opena8dj-control and OpenA8DJ Control Center
- Windows/Linux: experimental platform areas, not included in this macOS driver

After install, open Audio MIDI Setup and confirm Open Audio 8 DJ appears with 8
inputs and 8 outputs. If it does not appear, reconnect the Audio 8 DJ once, then
reopen the audio app.

For more help, open the GitHub README or docs/user/install.md in the repository.
