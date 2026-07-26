# Quick Start

Use this when you only want to get the Audio 8 DJ working on macOS.

## Before You Start

OpenA8DJ 0.5.1 requires an Apple Silicon Mac running macOS 26 or later. It is
for the Native Instruments Audio 8 DJ only.

## 1. Download

Open the current stable release:

```text
https://github.com/fersantxez/OpenA8DJ/releases/tag/v0.5.1
```

Download:

```text
OpenA8DJ-0.5.1.dmg
```

## 2. Install OpenA8DJ

1. Open `OpenA8DJ-0.5.1.dmg`.
2. Double-click `OpenA8DJ-0.5.1.pkg`.
3. Follow the macOS Installer prompts.
4. Reconnect the Audio 8 DJ if it does not appear immediately.
5. Reopen your DJ or audio app if it was already running.

This one installer includes the audio driver, MIDI support, Control Center, and
command-line support tools.

## 3. Check macOS

1. Open Audio MIDI Setup.
2. Select `Open Audio 8 DJ`.
3. Confirm it shows 8 inputs and 8 outputs.

## 4. Use It In Traktor Or Another Audio App

1. Open the app.
2. Choose `Open Audio 8 DJ` as the audio device.
3. Assign outputs as stereo pairs:

```text
1-2: deck/output A
3-4: deck/output B
5-6: deck/output C
7-8: deck/output D
```

For Traktor timecode vinyl, calibrate the vinyl inside Traktor. OpenA8DJ keeps
the vinyl input path active by default. If calibration fails, open OpenA8DJ
Control Center, select `DVS Vinyl`, click `Apply`, and calibrate again.

## More Help

- [Full install guide](install.md)
- [Traktor and Timecode Vinyl](traktor-timecode.md)
- [Control Center](control-center.md)
- [Troubleshooting](troubleshooting.md)
