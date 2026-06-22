# Debian and Ubuntu Packaging Scaffold

This directory is a placeholder for Debian, Ubuntu, and derivative packaging.

Current state:

```text
diagnostic only, sound quality not validated
```

It does not produce a real `.deb` yet.

## Planned Packages

Possible binary packages:

- `opena8dj-dkms`
- `opena8dj-tools`
- `opena8dj-alsa-ucm`
- `opena8dj-udev`
- `opena8dj`

The driver and tools should remain separable so tools can be inspected without
automatically installing or loading a kernel module.

## Planned Files

Future Debian packaging may include:

```text
control
rules
changelog
copyright
opena8dj-dkms.dkms
opena8dj-tools.install
opena8dj-alsa-ucm.install
opena8dj-udev.install
postinst
prerm
postrm
```

Do not add scripts that load, bind, unbind, or test Audio 8 DJ hardware until
the driver path and validation policy are complete.

## Safety Rule

Package install must not imply sound quality readiness. Live hardware tests,
module reloads, USB resets, playback, capture, and latency measurements require
explicit operator action and the shared hardware lock during project validation.
