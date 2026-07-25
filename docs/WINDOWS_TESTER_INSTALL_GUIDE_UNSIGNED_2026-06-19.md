# Windows Experimental Driver Install Guide

## Status

This guide is for controlled testers only.

The current Windows package is publicly visible as an experimental download,
but it is not a production release. It is test-signed, not Microsoft-signed,
and it is not suitable for a computer that cannot tolerate a kernel-driver
failure.

Use it only on a Windows test machine where kernel driver recovery is possible.

## What The Tester Gets

Choose either published package:

```text
OpenA8DJUsb-Release-x64-installer.zip
OpenA8DJUsb-Release-x64-installer.exe
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

## Install: the easiest path

1. Close Traktor and every other audio application.
2. Unplug the Audio 8 DJ USB cable.
3. Open **Windows Terminal (Admin)** or **PowerShell (Admin)** and run:

   ```text
   shutdown /r /o /t 0
   ```

4. Select **Troubleshoot** → **Advanced options** → **Startup Settings** →
   **Restart**, then press **7** (**Disable driver signature enforcement**).
5. When Windows is back, double-click
   `OpenA8DJUsb-Release-x64-installer.exe` and approve the UAC prompt.
6. If Windows Security asks **Install this driver software anyway**, choose
   that option. This is expected for this test-signed package.
7. Reconnect the Audio 8 DJ and wait for Windows to detect it.

The ZIP is the same package without the EXE wrapper. To use it, extract it,
right-click `install.cmd`, choose **Run as administrator**, and follow the
same Startup Settings sequence first.

Option 7 is a one-boot exception; it does not permanently enable test-signing.
Do not use `-ForceInstallDespiteSecureBoot`, do not change BCD settings, and do
not combine option 7 with `-EnableTestSigning`.

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

The current validated candidate exposes the Audio 8 DJ endpoint and the
experimental isochronous engine, but MIDI, ASIO, complete DVS/timecode input
validation, sleep/wake, and Microsoft signing remain unfinished.

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

If Windows does not show the endpoint after reconnecting the device, save the
verification output and do not repeatedly reinstall the driver.

## macOS is unaffected

This package contains only Windows files: the Windows INF, SYS, CAT, tools, and
PowerShell scripts. It does not contain or modify the macOS HAL, DMG, PKG,
Control Center, Core Audio, or CoreMIDI payloads. The macOS `main` release line
remains the primary supported product.

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
