# OpenA8DJ Windows

This directory contains the Windows driver package and companion tools for the
Audio 8 DJ.

Start with the user-facing [Windows installation guide](../docs/WINDOWS.md).
The implementation and validation notes are in
[Windows technical details](../docs/WINDOWS_DETAILS.md).

## Current Package

The current Windows build provides:

- Audio 8 DJ USB device support for `USB\VID_17CC&PID_1978`.
- 8 input channels and 8 output channels, grouped as A/B/C/D stereo pairs.
- The `opena8djctl.exe` status, topology, profile, and diagnostics tool.
- A ZIP package with install, verify, and uninstall entry points.

The package is an experimental, test-signed development build. Windows
test-signing mode is required because the driver is not Microsoft-signed for
production distribution yet.

## Build On Windows

Use a Windows 10/11 x64 machine with Visual Studio 2022, the Windows SDK, and
the Windows Driver Kit installed:

```powershell
.\windows\scripts\build-installable-package.ps1 -Configuration Release -Platform x64
```

The package is written to:

```text
windows\dist\installer\OpenA8DJUsb-Release-x64-installer.zip
```

## Offline Checks

The portable source contracts can be checked on macOS or Windows:

```text
windows/tests/run_offline_tests.py
```

On Windows, the equivalent PowerShell runner is:

```powershell
.\windows\tests\run-offline-tests.ps1
```

These checks do not require the Audio 8 DJ. Hardware installation and audio
validation are documented in [Windows technical details](../docs/WINDOWS_DETAILS.md).
