# Windows Standalone Installer Design - 2026-06-19

## Decision

The Windows installer line must be separate from macOS packaging and from the
driver implementation itself.

The product is split into:

- `OpenA8DJ Windows Driver`: kernel driver package, Driver Store install,
  device binding, signing/certification state, and lock-protected hardware
  install/uninstall.
- `OpenA8DJ Windows Tools`: user-mode utilities, diagnostics, future Control
  Center, Start Menu entries, and logs.

A future `OpenA8DJ-Windows-Setup.exe` may chain both, but the driver and tools
packages remain independently installable and uninstallable.

## Why Split Driver And Tools

Driver installation has different rules from normal app installation:

- requires admin elevation;
- modifies the Windows Driver Store;
- depends on catalog signing;
- may require reboot;
- can rebind or interrupt the physical USB device;
- must respect the shared hardware lock.

Tools installation is normal user-mode software:

- installs under `%ProgramFiles%`;
- can be MSI-based;
- does not need to bind hardware;
- can be upgraded more often than the driver.

Keeping them separate reduces risk and avoids reinstalling the kernel driver
when only the control tool changes.

## Current Development Installer

The current development installer is the ZIP produced by:

```powershell
.\windows\scripts\build-installable-package.ps1 -Configuration Release -Platform x64
```

Expected output:

```text
windows\dist\Release\x64\
windows\dist\installer\OpenA8DJUsb-Release-x64-installer.zip
```

The ZIP contains:

- `driver\OpenA8DJUsb.inf`
- `driver\OpenA8DJUsb.sys`
- `driver\OpenA8DJUsb.cat`
- `driver\OpenA8DJUsb-TestCertificate.cer`
- `driver\opena8djctl.exe`
- `scripts\install-driver.ps1`
- `scripts\verify-driver.ps1`
- `scripts\uninstall-driver.ps1`
- `install.cmd`
- `verify.cmd`
- `uninstall.cmd`
- `README-FIRST.txt`
- `installer-manifest.json`

This is not the final public installer. It is the development installer until a
Windows build host can produce signed EXE/MSI artifacts.

## Production Installer Architecture

### Driver Installer

Form:

- elevated EXE bootstrapper;
- no macOS payloads;
- no Native Instruments payloads;
- driver package embedded or placed beside the EXE.

Responsibilities:

1. Validate admin elevation.
2. Validate INF/CAT/SYS presence and hashes.
3. Validate catalog signature.
4. Refuse public install if the catalog is not Microsoft-signed.
5. Acquire the shared hardware lock.
6. Install the driver package with Windows driver install APIs or `pnputil`.
7. Rebind matching `USB\VID_17CC&PID_1978` devices when appropriate.
8. Write an install manifest.
9. Release the hardware lock on every exit path.

Development-only behavior:

- import `OpenA8DJUsb-TestCertificate.cer`;
- enable `TESTSIGNING`;
- stop and request reboot when test-signing has just been enabled.

### Tools Installer

Form:

- MSI, preferably WiX Toolset.

Responsibilities:

1. Install tools under `%ProgramFiles%\OpenA8DJ\Tools`.
2. Add Start Menu shortcuts.
3. Add optional PATH integration for CLI users.
4. Install future Control Center and diagnostic collector.
5. Avoid any Driver Store action.
6. Avoid hardware lock unless a future step talks to the device.

### Bundle Installer

Form:

- optional bootstrapper EXE.

Responsibilities:

1. Offer default install of driver plus tools.
2. Offer advanced selection: driver only, tools only, repair.
3. Run driver installer first.
4. Run tools installer second.
5. Surface exact final state: installed, reboot required, failed, or blocked by
   hardware lock.

## Lock Rules

Build/sign/package actions do not touch hardware and do not need the lock.

These actions must acquire the shared hardware lock:

- driver install;
- driver uninstall;
- driver repair;
- driver upgrade;
- device rebind;
- any future smoke test that opens the device.

These actions do not need the lock:

- building installer artifacts;
- installing user-mode tools only;
- verifying file hashes;
- verifying signatures offline.

## Verification Output

Every installer run should create a machine-readable report with:

- installer version;
- driver version;
- architecture;
- file hashes;
- catalog signature status;
- Driver Store state;
- connected `VID_17CC&PID_1978` state;
- `opena8djctl surface` output when available;
- `opena8djctl diagnostics` output when available;
- lock acquisition and release result;
- reboot-required state.

The existing `windows\scripts\verify-driver.ps1` is the first implementation of
this report.

## Signing And Certification Path

Development:

- self-signed test certificate;
- Windows test-signing mode;
- never public.

Internal QA:

- prefer Microsoft-signed driver package;
- signed installer EXE/MSI;
- controlled machines only.

Public:

- Microsoft-signed driver package through attestation or HLK/WHQL;
- signed driver installer EXE;
- signed tools MSI;
- optional signed bundle EXE.

Attestation route:

1. Build release driver package.
2. Generate CAT with `Inf2Cat`.
3. Export CAB with `windows\scripts\export-attestation-cab.ps1`.
4. Sign CAB with the organization EV/code-signing certificate if required by
   the current Partner Center workflow.
5. Submit CAB in Microsoft Partner Center Hardware Dashboard.
6. Download Microsoft-signed result.
7. Repackage into the public driver installer.

HLK/WHQL route:

1. Run HLK on target Windows versions and hardware.
2. Package HLK results as `.hlkx`.
3. Submit `.hlkx` in Partner Center.
4. Download Microsoft-signed package.
5. Repackage into public installer.

## Repository Artifacts

This design is represented in:

- `windows/installer/README.md`
- `windows/installer/driver-package.manifest.json`
- `windows/installer/tools-package.manifest.json`
- `windows/scripts/package-installer.ps1`
- `windows/scripts/build-installable-package.ps1`

The manifests are intentionally simple JSON so future CI can validate that
driver and tools payloads remain separate.

## Readiness Gate

The independent installer is not ready for user testing until:

- the package builds on a Windows WDK host;
- `OpenA8DJUsb-Release-x64-installer.zip` is generated;
- a clean Windows test machine installs it;
- first install handles test-signing/reboot correctly;
- second install succeeds after reboot;
- `verify.cmd` writes a passing report;
- uninstall releases the driver package;
- shared hardware lock is released after success and after failure.

The public installer is not ready until the same flow passes with a
Microsoft-signed catalog and without test-signing mode.
