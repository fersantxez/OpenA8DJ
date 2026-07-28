# Windows Build And Tests

This page is for maintainers. It is not part of the user installation flow.

## Build Requirements

- Windows 10 or Windows 11 x64.
- Visual Studio 2022 with MSBuild.
- Windows SDK.
- Windows Driver Kit.

Build the driver and package on Windows:

```powershell
.\windows\scripts\build-driver.ps1 -Configuration Release -Platform x64
.\windows\scripts\build-installable-package.ps1 -Configuration Release -Platform x64
.\windows\scripts\build-exe-installer.ps1 -Configuration Release -Platform x64
```

The package output is written under `windows\dist\`.

## Offline Tests

The portable contract tests can run on macOS without Windows or hardware:

```sh
windows/tests/run_offline_tests.py
```

On Windows, use:

```powershell
.\windows\tests\run-offline-tests.ps1
```

These tests cover source contracts and the offline audio-engine model. They do
not replace Windows installation or physical Audio 8 DJ validation.

## Physical Validation

Physical Windows validation is documented in
[Windows notes and evidence](WINDOWS_NOTES.md) and the
[Windows handoff](WINDOWS_HANDOFF_2026-07-25.md). Use the shared hardware
coordination rules before connecting or testing the Audio 8 DJ.
