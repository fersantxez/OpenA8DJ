# Windows Tester Install Guide - Unsigned/Test-Signed Builds - 2026-06-19

## Status

This guide is for controlled testers only.

The current Windows package is not a public release package. It is not
Microsoft-signed yet, and it is not suitable for normal end users who expect a
double-click consumer installer.

Use it only on a Windows test machine where kernel driver recovery is possible.

## What The Tester Gets

Development package:

```text
OpenA8DJUsb-Release-x64-installer.zip
```

Expected contents:

```text
driver\OpenA8DJUsb.inf
driver\OpenA8DJUsb.sys
driver\OpenA8DJUsb.cat
driver\OpenA8DJUsb-TestCertificate.cer
driver\opena8djctl.exe
scripts\install-driver.ps1
scripts\verify-driver.ps1
scripts\uninstall-driver.ps1
install.cmd
verify.cmd
uninstall.cmd
README-FIRST.txt
installer-manifest.json
```

## Requirements

- Windows 10 22H2 x64 or Windows 11 x64 test machine.
- Administrator access.
- Native Instruments Audio 8 DJ connected by USB.
- Ability to reboot.
- No production DJ session running on the machine.

## Important Safety Notes

- This package can bind a kernel driver to the Audio 8 DJ.
- This can temporarily remove the device from other drivers/software.
- Do not install during a live performance or critical session.
- If the package was not produced by the OpenA8DJ Windows build pipeline, do
  not install it.
- If Windows SmartScreen or Defender blocks the package, stop and report it.

## Install

1. Extract `OpenA8DJUsb-Release-x64-installer.zip`.
2. Right-click `install.cmd`.
3. Choose **Run as administrator**.
4. If the installer says test-signing was enabled and a reboot is required,
   reboot Windows.
5. Run `install.cmd` again as administrator after reboot.

The first run may only prepare the machine for test-signed drivers. That is
expected.

## Verify

Run as administrator:

```cmd
verify.cmd
```

Expected useful signals:

- `driver_store_has_opena8dj` is `true`.
- `connected_audio8dj_usbdevice` is `true` when the hardware is plugged in.
- `catalog_signature.status` is not `NotSigned`.
- `opena8djctl surface` can open the driver interface.
- `opena8djctl diagnostics` reports counters.

The current driver surface should report that audio endpoints and the real
isochronous streaming engine are not ready yet. That is intentional. The
Windows package must not claim audio streaming until the engine exists.

## Basic Commands

From the extracted package:

```cmd
driver\opena8djctl.exe surface
driver\opena8djctl.exe topology
driver\opena8djctl.exe diagnostics
driver\opena8djctl.exe status
```

Do not treat `start` failure as a bug at this stage. `start` is intentionally
rejected until the real Windows audio/isochronous engine exists.

## Uninstall

Run as administrator:

```cmd
uninstall.cmd
```

Then run:

```cmd
verify.cmd
```

The driver package should no longer be installed in the Driver Store.

## Turn Off Test-Signing

Only do this after uninstalling test builds if the machine should return to
normal Windows driver policy.

Run as administrator:

```cmd
bcdedit /set testsigning off
```

Reboot Windows.

## What To Report Back

Attach or paste:

- `verify.cmd` JSON output;
- Windows version;
- whether install required reboot;
- whether Audio 8 DJ was plugged in before install;
- whether Device Manager showed OpenA8DJ after install;
- output of:

```cmd
driver\opena8djctl.exe surface
driver\opena8djctl.exe diagnostics
```

If audio apps do not see an audio endpoint, that is expected in the current
surface driver stage. Report it only as confirmation, not as a regression.

## Ready/Not Ready Language

Correct:

- "Ready for controlled Windows driver installation test."
- "Ready to verify binding, surface IOCTLs, topology, and diagnostics."

Incorrect:

- "Ready for public users."
- "Ready for Traktor audio."
- "Audiophile-quality Windows audio driver."
- "Production Windows installer."

Those claims require the Windows audio endpoint, real isochronous engine,
physical audio validation, MIDI validation, routing validation, and Microsoft
signing/certification path.
