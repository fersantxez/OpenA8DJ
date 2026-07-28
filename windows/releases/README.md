# OpenA8DJ Windows experimental package

This directory contains the current x64 Windows installer artifacts for the
experimental OpenA8DJ Audio 8 DJ driver.

Files:

- [OpenA8DJUsb-Release-x64-installer.exe](OpenA8DJUsb-Release-x64-installer.exe)
  — double-clickable self-extracting installer wrapper.
- [OpenA8DJUsb-Release-x64-installer.zip](OpenA8DJUsb-Release-x64-installer.zip)
  — the same package without the EXE wrapper.
- [OpenA8DJUsb-Release-x64-installer.exe.json](OpenA8DJUsb-Release-x64-installer.exe.json)
  — EXE metadata and SHA-256.
- [SHA256SUMS.txt](SHA256SUMS.txt) — checksums for the two packages.

The driver is test-signed, not Microsoft-signed, and not WHQL/attestation
certified. Windows users must use Startup Settings option 7 for the install
boot when signature enforcement would otherwise reject it. Read the
[Windows installation guide](../../docs/WINDOWS.md) before installing.

This package is Windows-only. It does not contain or modify the macOS HAL,
DMG, PKG, Control Center, Core Audio, or CoreMIDI payloads. The macOS `main`
release remains the primary supported product.
