opena8dj-tools 0.5.0
====================

This disk image installs only the optional OpenA8DJ control tools:

- /Applications/OpenA8DJ Control Center.app
- /usr/local/bin/opena8dj-control
- /Library/Documentation/OpenA8DJ/ControlSurfaces

It does not install or replace the OpenA8DJ HAL driver. It does not restart
Core Audio. It does not install the MIDI LaunchAgent. Use the full OpenA8DJ
installer when the driver itself needs to be installed or updated.

Install:

1. Double-click opena8dj-tools-<version>.pkg.
2. Follow the macOS Installer prompts.
3. Open /Applications/OpenA8DJ Control Center.app.

Command-line use:

opena8dj-control list-profiles
opena8dj-control apply-preset traktor-dvs-vinyl
opena8dj-control export-config ~/Desktop/opena8dj-config.json

Documentation:

- CONTROL_SURFACES_USER_GUIDE.md in this disk image.
- CONTROL_SURFACES_DEMO_RUNBOOK_2026-06-19.md in this disk image.
- /Library/Documentation/OpenA8DJ/ControlSurfaces after install.

Uninstall:

sudo /Library/Documentation/OpenA8DJ/ControlSurfaces/uninstall-opena8dj-control-surfaces.sh

OpenA8DJ is an independent, open-source preservation project. It is not
affiliated with, endorsed by, sponsored by, or certified by Native Instruments.
Product names are used only to identify compatibility.

This build may be ad-hoc signed and not Apple-notarized unless produced with
Developer ID signing and notarization.
