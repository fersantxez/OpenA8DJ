# Control Center

OpenA8DJ Control Center is the optional macOS app for changing normal Audio 8 DJ
profiles without using Terminal.

The Control Center tools are separate from the driver installer. They add:

```text
/Applications/OpenA8DJ Control Center.app
/usr/local/bin/opena8dj-control
/Library/Documentation/OpenA8DJ/ControlSurfaces
```

The tools package does not replace the driver. Install the main OpenA8DJ driver
package first.

## Install

1. Download `opena8dj-tools-0.5.0.dmg` from the GitHub release.
2. Open the DMG.
3. Double-click `opena8dj-tools-0.5.0.pkg`.
4. Follow the macOS Installer prompts.
5. Open `OpenA8DJ Control Center.app` from Applications.

## Normal Profiles

### DVS Vinyl

Use this for Traktor Scratch or DVS with timecode vinyl. This is the normal
default profile for OpenA8DJ 0.5.0.

```text
Turntable A -> Audio 8 DJ input A
Turntable B -> Audio 8 DJ input B
Audio 8 DJ output A -> mixer channel A
Audio 8 DJ output B -> mixer channel B
```

Click `DVS Vinyl`, then `Apply`.

### DVS CD-Line

Use this for CDJs, media players, or line-level timecode sources.

Click `DVS CD-Line`, then `Apply`.

### Output Only

Use this when the Audio 8 DJ is only being used as an output interface.

Click `Output Only`, then `Apply`.

## Export Support Information

If someone asks for your OpenA8DJ settings, use Control Center to export the
current configuration and attach that file to the GitHub issue.

Normal users should not need the command-line tool. It is installed for
maintainers, diagnostics, and scripted support.
