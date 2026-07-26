OpenA8DJ 0.5.1
==============

OpenA8DJ lets a Native Instruments Audio 8 DJ work as an audio and MIDI
interface on current macOS.

Requirements
------------

- Native Instruments Audio 8 DJ
- Apple Silicon Mac
- macOS 26 or later
- Administrator password during installation

Install
-------

1. Double-click OpenA8DJ-0.5.1.pkg.
2. Follow the macOS Installer prompts.
3. Connect or reconnect the Audio 8 DJ.
4. Reopen Traktor or any audio application that was already running.

The installer adds the audio driver, MIDI support, OpenA8DJ Control Center, and
the support tools. No second download is required.

Check The Installation
----------------------

1. Open Audio MIDI Setup.
2. Select Open Audio 8 DJ.
3. Confirm that it has 8 inputs and 8 outputs.
4. Select Open Audio 8 DJ in Traktor or the audio application.

Output pairs:

1-2: A
3-4: B
5-6: C
7-8: D

For Traktor Timecode Vinyl, the DVS Vinyl profile is selected by default.
Open OpenA8DJ Control Center from Applications if you need to reapply it or
choose a different connection profile.

Uninstall
---------

Open Terminal and run:

sudo /usr/local/bin/opena8dj-uninstall

Help
----

https://github.com/fersantxez/OpenA8DJ

Use only the installer attached to the official GitHub release. OpenA8DJ is an
independent open-source project and is not affiliated with, endorsed by,
sponsored by, or certified by Native Instruments.
