# OpenA8DJ Linux Experimental Package

Readiness:

```text
diagnostic only, sound quality not validated
```

This package is for Linux-side experimental inspection of Native Instruments
Audio 8 DJ / OpenA8DJ. It installs user-space diagnostics, profile metadata,
documentation, and a conservative udev tag. It does not install a replacement
kernel module.

Driver channel:

```text
in-kernel snd-usb-caiaq
```

## What This Package Does

- Installs `opena8dj-linuxctl`.
- Installs profile/control schema documentation.
- Installs a udev rule for USB id `17cc:1978`.
- Installs OpenA8DJ Linux architecture and validation documents.
- Provides read-only diagnostics and report export.

## What This Package Does Not Do

- It does not load a kernel module.
- It does not replace `snd-usb-caiaq`.
- It does not bind or unbind USB drivers.
- It does not reset USB.
- It does not restart ALSA, PipeWire, JACK, or system audio services.
- It does not play audio.
- It does not record audio.
- It does not validate sound quality.
- It does not prove Traktor/DVS readiness.

## Debian/Ubuntu Install

```sh
sudo apt install ./opena8dj-linux-experimental_0.1.2~experimental20260726_all.deb
```

## Fedora, RHEL, Rocky, AlmaLinux, And openSUSE Install

```sh
sudo dnf install ./opena8dj-linux-experimental-0.1.2-0.experimental20260726.noarch.rpm
```

On openSUSE, use:

```sh
sudo zypper install ./opena8dj-linux-experimental-0.1.2-0.experimental20260726.noarch.rpm
```

## Arch, Manjaro, And EndeavourOS Install

```sh
sudo pacman -U ./opena8dj-linux-experimental-0.1.2-1-any.pkg.tar.zst
```

## Other Distributions

The `.tar.gz` archive is intentionally not a system installer. It is a
portable, inspectable payload for systems such as Gentoo or NixOS, where the
maintainer should provide native integration rather than bypass the package
manager.

## First Commands On Linux

```sh
opena8dj-linuxctl status
opena8dj-linuxctl hardware-model --json
opena8dj-linuxctl diagnostics --json --controls
opena8dj-linuxctl verify --controls --report-dir ~/opena8dj-linux-report
```

## Optional Explicit Hardware Controls

These commands write ALSA controls and should only be run intentionally:

```sh
opena8dj-linuxctl list-profiles
opena8dj-linuxctl apply-profile traktor-dvs-vinyl --yes
opena8dj-linuxctl apply-profile traktor-dvs-cd-line --yes
opena8dj-linuxctl apply-profile timecode-vinyl --yes
opena8dj-linuxctl set-control input-mode phono --yes
```

## Validation Rule

Do not treat successful installation, enumeration, or clean diagnostics as a
sound-quality pass. A normal candidate requires physical music playback through
the exact loaded artifact, independent external capture, comparison against a
reference, CPU/xrun evidence, and human listening only after measurements are
clean.
