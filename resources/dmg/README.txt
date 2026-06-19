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

If macOS blocks the installer with:

"OpenA8DJ-0.5.0.pkg" Not Opened
Apple could not verify "OpenA8DJ-0.5.0.pkg" is free of malware.

click Done, not Move to Trash. Then open System Settings > Privacy & Security,
find the blocked OpenA8DJ package in the Security section, and choose
Open Anyway.

OpenA8DJ 0.5.0 release assets are unsigned, not Developer ID signed, and not
Apple-notarized. macOS Gatekeeper will likely block them unless manually
approved after checksum verification. Only override Gatekeeper for official
release assets from this repository, and verify the SHA-256 checksum first.

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
- Optional tools: opena8dj-control and OpenA8DJ Control Center
- Legacy C line: preserved separately on the repository legacy branch
- Windows/Linux: experimental platform areas, not included in this macOS driver

After install, open Audio MIDI Setup and confirm Open Audio 8 DJ appears with 8
inputs and 8 outputs. If it does not appear, reconnect the Audio 8 DJ once, then
reopen the audio app.

The permanent distribution goal is Developer ID signing plus Apple notarization.
