# Arch Linux Packaging

The experimental package builder emits a pacman-installable `.pkg.tar.zst`
artifact for Arch Linux, Manjaro, and EndeavourOS. It installs only the
diagnostic tool, profile metadata, udev rule, and documentation.

Readiness:

```text
diagnostic only, sound quality not validated
```

Install the artifact from the release directory with:

```sh
sudo pacman -U ./opena8dj-linux-experimental-0.1.2-1-any.pkg.tar.zst
```

The package has no install hook and does not load a driver, bind USB, reset the
device, change an audio service, or run an audio test. Future module-bearing
Arch packages must split tools/data from any kernel integration and keep those
same safety properties.

Every generated release directory contains a concrete `PKGBUILD` for
repackaging that exact release tarball with Arch tooling. Place the tarball
next to the generated `PKGBUILD` and run `makepkg`; the recipe carries the
exact release checksum.
