# OpenA8DJ Windows Driver

This directory contains the Windows 10/11 driver workstream for OpenA8DJ.

The macOS Core Audio HAL cannot be repackaged into Windows. Windows needs its
own WDK driver package: a `.sys` driver, INF, catalog, signing flow, and later
an installer/bootstrapper.

## Current State

The current Windows package is an experimental WDK build. It is intended for
testers and developers who understand that kernel drivers can fail, need test
signing, and may require recovery steps.

`OpenA8DJUsb` is a KMDF USB function driver for the Native Instruments Audio 8
DJ hardware ID:

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

The driver exposes the same OpenA8DJ feature contract used by the macOS build:

- 8 input channels
- 8 output channels
- 4 stereo input pairs: Input A/B/C/D
- 4 stereo output pairs: Output A/B/C/D
- 44.1, 48, 88.2, and 96 kHz capability metadata
- 15-4096 frame buffer range
- DVS input modes: Timecode Vinyl, Timecode CD/Line, Phono
- ground-lift flags for vinyl, CD/line, and phono
- software lock state

The package also builds `opena8djctl.exe`, a Windows control tool that can query
capabilities, read/write controls, apply profiles, and set the experimental
format state.

Important: the Windows audio endpoint layer is still experimental and has not
been exhaustively tested on Windows 10/11 with the physical Audio 8 DJ. Expect
bugs. Feedback and logs are welcome.

## Build Requirements

Build on Windows, not macOS:

- Windows 10 or Windows 11 build host
- Visual Studio 2022
- Windows SDK
- Windows Driver Kit

Microsoft documents WDK setup here:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
- https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/building-a-driver

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

## Test Install

Only use this on a Windows test machine configured for test-signed or unsigned
development drivers:

```powershell
pnputil /add-driver windows\dist\Release\x64\OpenA8DJUsb.inf /install
```

Then query the device:

```powershell
windows\dist\Release\x64\opena8djctl.exe status
windows\dist\Release\x64\opena8djctl.exe profile timecode-vinyl
windows\dist\Release\x64\opena8djctl.exe set-format 48000 512
```

Do not use this on a production DJ system yet.

## Next Milestones

1. Load `OpenA8DJUsb.sys` on Windows 10/11 with the physical interface.
2. Validate `opena8djctl.exe status` and profile writes.
3. Add isochronous transfer queues and deterministic stream tests.
4. Complete and validate the Windows audio endpoint layer, most likely using the WDK
   audio samples and adapting the OpenA8DJ transport.
5. Validate MIDI endpoints and hardware controls.
6. Add signed driver package and MSI/bootstrapper.
