# OpenA8DJ Windows Driver

This directory contains the Windows 10/11 driver workstream for OpenA8DJ.

The macOS Core Audio HAL cannot be repackaged into Windows. Windows needs its
own WDK driver package: a `.sys` driver, INF, catalog, signing flow, and later
an installer/bootstrapper.

## Current State

The current Windows package is an experimental WDK build. It is intended for
testers and developers who understand that kernel drivers can fail, need test
signing, and may require recovery steps.

`OpenA8DJUsb` is a KMDF USB function driver surface for the Native Instruments
Audio 8 DJ hardware ID:

```text
USB\VID_17CC&PID_1978
```

The driver initializes KMDF, claims the USB device, selects the configured USB
interface, and maps the known CAIAQ pipes:

```text
bulk out:         0x01
bulk in:          0x81
isochronous in:   0x82
isochronous out:  0x06
```

The Windows surface is now API v2. It exposes an honest feature contract for
the future ACX/USB implementation:

- 8 input channels
- 8 output channels
- 4 stereo input pairs: Input A/B/C/D
- 4 stereo output pairs: Output A/B/C/D
- 44.1 and 48 kHz as the first stable Windows rates
- 88.2 and 96 kHz as planned rates only
- 15-4096 frame buffer range
- DVS input modes: Timecode Vinyl, Timecode CD/Line, Phono
- ground-lift flags for vinyl, CD/line, and phono
- software lock state
- surface state for USB transport, controls, audio endpoints, isochronous
  engine, MIDI, and ASIO
- topology descriptors for the planned A/B/C/D render/capture channel layout
- diagnostics counters for format changes, controls, rejected start requests,
  and future stream counters

The package also builds `opena8djctl.exe`, a Windows control tool that can query
the surface, topology, diagnostics, capabilities, controls, profiles, and
experimental format state.

Important: the Windows audio endpoint layer and real isochronous streaming
engine are not implemented yet. `opena8djctl start` is intentionally rejected
until that engine exists; the driver must not pretend that streaming is active
without real USB audio transport. Feedback and logs are welcome. Install and
use experimental Windows builds at your own risk.

## Standalone Installer

Windows owns a separate installer line. It must not share macOS packaging code
or macOS payloads.

Installer design lives in:

- `windows/installer/README.md`
- `windows/installer/driver-package.manifest.json`
- `windows/installer/tools-package.manifest.json`
- `docs/WINDOWS_STANDALONE_INSTALLER_DESIGN_2026-06-19.md`
- `docs/WINDOWS_TESTER_INSTALL_GUIDE_UNSIGNED_2026-06-19.md`
- `docs/WINDOWS_PERFORMANCE_ROUTING_AND_MACOS_LESSONS_2026-06-19.md`

The installer product is split into:

- `OpenA8DJ Windows Driver`: INF/CAT/SYS, Driver Store install, device binding,
  signing state, and hardware-lock-protected install/uninstall.
- `OpenA8DJ Windows Tools`: `opena8djctl.exe`, future diagnostics, future
  Control Center, Start Menu entries, and user-mode files.

A future bundle EXE may install both, but the driver and tools installers must
remain independently installable and independently uninstallable.

## Shared Hardware Lock

This hardware is shared with the macOS driver and macOS Control Center work.
Before any test that touches the physical Audio 8 DJ, iRig capture route, Core
Audio, USB reset path, playback/capture, MIDI, Traktor, or driver
install/load/unload state, check and acquire the shared lock:

```sh
./scripts/shared-hardware-lock-status
./scripts/shared-hardware-lock-run --gate windows-test -- <command>
```

Release the lock immediately when the hardware action finishes. Builds and
static analysis that do not touch hardware do not need the lock.

## Build Requirements

Build on Windows, not macOS:

- Windows 10 or Windows 11 build host
- Visual Studio 2022
- Windows SDK
- Windows Driver Kit

Microsoft documents WDK setup here:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
- https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/building-a-driver

## Offline Tests On macOS

This does not build or validate the Windows driver. It only checks the portable
Windows surface contract and the offline user-mode audio-engine prototype:

```sh
windows/tests/run_offline_tests.py
```

On Windows, use the PowerShell runner:

```powershell
.\windows\tests\run-offline-tests.ps1
```

Passing this runner means the source contracts are internally consistent. It
does not prove Windows installability, endpoint enumeration, Traktor behavior,
MIDI, USB streaming, CPU/DPC behavior, or sound quality.

## Build

From a Visual Studio Developer PowerShell with WDK installed:

```powershell
.\windows\scripts\build-driver.ps1 -Configuration Release -Platform x64
```

Expected unsigned development package output:

```text
windows\dist\Release\x64\OpenA8DJUsb.sys
windows\dist\Release\x64\OpenA8DJUsb.inf
windows\dist\Release\x64\opena8djusb.cat
windows\dist\Release\x64\OpenA8DJUsb.cer
windows\dist\Release\x64\OpenA8DJUsb.pdb
windows\dist\Release\x64\opena8djctl.exe
windows\dist\Release\x64\opena8djctl.pdb
```

To build the current standalone development installer ZIP:

```powershell
.\windows\scripts\build-installable-package.ps1 -Configuration Release -Platform x64
```

Expected installer ZIP:

```text
windows\dist\installer\OpenA8DJUsb-Release-x64-installer.zip
```

## Test Install

Only use this on a Windows test machine configured for test-signed or unsigned
development drivers:

```powershell
pnputil /add-driver windows\dist\Release\x64\OpenA8DJUsb.inf /install
```

Then query the device:

```powershell
windows\dist\Release\x64\opena8djctl.exe status
windows\dist\Release\x64\opena8djctl.exe surface
windows\dist\Release\x64\opena8djctl.exe topology
windows\dist\Release\x64\opena8djctl.exe diagnostics
windows\dist\Release\x64\opena8djctl.exe profile timecode-vinyl
windows\dist\Release\x64\opena8djctl.exe set-format 48000 512
```

Do not use this on a production DJ system yet.

## Next Milestones

1. Load `OpenA8DJUsb.sys` on Windows 10/11 with the physical interface.
2. Validate `opena8djctl.exe status` and profile writes.
3. Prototype the ACX endpoint skeleton without touching macOS code.
4. Add isochronous transfer queues and deterministic stream tests.
5. Complete and validate the Windows audio endpoint layer, most likely using the WDK
   ACX samples and adapting the OpenA8DJ transport.
6. Validate MIDI endpoints and hardware controls.
7. Add signed driver package and MSI/bootstrapper.
