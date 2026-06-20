# Uninstall

Use this if you want to remove OpenA8DJ from the Mac.

## Remove The Driver

Open Terminal and run:

```sh
sudo /usr/local/bin/opena8dj-uninstall
```

macOS may ask for your Mac password. The command removes the OpenA8DJ driver,
MIDI bridge, command-line tools, and installed documentation.

## Remove Control Center Only

If you installed the optional tools package and only want to remove Control
Center:

```sh
sudo /Library/Documentation/OpenA8DJ/ControlSurfaces/uninstall-opena8dj-control-surfaces.sh
```

## After Uninstall

Open Audio MIDI Setup and confirm `Open Audio 8 DJ` is no longer listed. If an
audio app was open during uninstall, close and reopen it.
