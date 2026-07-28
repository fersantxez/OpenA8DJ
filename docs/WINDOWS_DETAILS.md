# Windows Technical Details

This page is for maintainers and testers who need to understand the Windows
implementation. For the normal installation path, use
[OpenA8DJ On Windows](WINDOWS.md).

## Status

The Windows driver package is operational with the Audio 8 DJ in the current
test setup. It is distributed as an experimental development package because
the driver is not Microsoft-signed for production distribution yet. Windows
test-signing mode is therefore required on the test machine.

The Windows line is independent from the macOS HAL and its packaging. It uses a
native Windows driver package, its own tools, and its own install and verify
scripts.

## Device Surface

The driver targets:

```text
USB\VID_17CC&PID_1978
```

The exposed Audio 8 DJ surface is:

- 8 input channels.
- 8 output channels.
- Four stereo input pairs: Input A, B, C, and D.
- Four stereo output pairs: Output A, B, C, and D.
- 44.1 kHz and 48 kHz as the primary operating rates.
- Timecode Vinyl, Timecode CD/Line, and Phono input profiles.
- Ground-lift controls and software-lock state.
- Status, topology, capability, profile, and diagnostic information through
  `opena8djctl.exe`.

The A/B/C/D grouping is the Windows equivalent of the channel surface used by
the macOS release and is intended to give DJ applications a familiar routing
layout.

## Package Contents

The development ZIP contains:

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

The manifest records SHA-256 hashes for the packaged files. The catalog is
test-signed for development use; it is not a Microsoft production signature.

## Build And Package

On Windows with Visual Studio 2022, the Windows SDK, and the WDK:

```powershell
.\windows\scripts\build-driver.ps1 -Configuration Release -Platform x64
.\windows\scripts\build-installable-package.ps1 -Configuration Release -Platform x64
```

The resulting ZIP is written to:

```text
windows\dist\installer\OpenA8DJUsb-Release-x64-installer.zip
```

The package scripts are deliberately separate from the macOS build and do not
install macOS files.

## Verification

The package verification command is:

```powershell
.\verify.cmd
```

For direct inspection:

```powershell
.\driver\opena8djctl.exe status
.\driver\opena8djctl.exe surface
.\driver\opena8djctl.exe topology
.\driver\opena8djctl.exe diagnostics
```

The verification output includes package hashes, catalog signature state,
Driver Store presence, connected-device detection, and control-tool output.

The repository also contains portable checks that can run on macOS without
Windows or hardware:

```text
windows/tests/run_offline_tests.py
```

These checks cover source contracts and the offline engine model. They do not
replace a Windows installation or physical Audio 8 DJ test.

## Architecture Summary

The current Windows stack is intentionally small:

```text
Audio 8 DJ USB device
        |
        v
OpenA8DJUsb KMDF driver
        |
        +-- Audio 8 DJ channel surface and profiles
        +-- topology and diagnostics IOCTLs
        +-- opena8djctl.exe
        +-- install / verify / uninstall scripts
```

The driver binds to the Audio 8 DJ USB identity and handles the device-facing
transport and control surface. User-mode tools remain outside the streaming
path. The deeper design history and future installer work are kept in:

- [Windows final design and implementation plan](../docs-state/windows/windows-final-design-and-implementation-plan.md)
- [Windows architecture state](../docs-state/windows/windows-architect-state.md)
- [Windows standalone installer design](WINDOWS_STANDALONE_INSTALLER_DESIGN_2026-06-19.md)
- [Windows performance and routing notes](WINDOWS_PERFORMANCE_ROUTING_AND_MACOS_LESSONS_2026-06-19.md)

## Signing

Development packages use a local test certificate. That is why installation
requires Windows test-signing mode. A future public package needs a Microsoft-
signed driver catalog and a separately signed installer before it can be
distributed as a normal Windows product.

The signing state is visible in `verify.cmd`; it is not hidden by the installer.
