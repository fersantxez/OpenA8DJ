# Windows Laptop Handoff

Date: 2026-06-22
Source branch: `windows/rebuild-surface`
Latest Windows commit at handoff: `589ac48 Add Windows offline audio engine prototype`
Prepared on: macOS
Hardware used in this preparation: none

## Purpose

This document is the handoff for moving OpenA8DJ Windows work onto a Windows
laptop/tablet with the Native Instruments Audio 8 DJ attached later. It is
written for a fresh Codex instance with no thread state.

The Windows line is still experimental. Do not present any package as a normal
tester candidate until the exact artifact has passed Windows install,
enumeration, audio, MIDI, Traktor, CPU/DPC, physical capture, and listening
validation. Until then, label artifacts:

```text
diagnostic only, sound quality not validated
```

## Current Repo State

Use:

```powershell
git checkout windows/rebuild-surface
git log --oneline -3
```

Expected top commits:

```text
589ac48 Add Windows offline audio engine prototype
49b1688 Finalize Windows design and plan
9827dd2 Document Windows architecture state
```

Important documents:

- `docs-state/windows/windows-architect-state.md`
- `docs-state/windows/windows-final-design-and-implementation-plan.md`
- `docs-state/windows/windows-implementation-progress.md`
- `docs-state/windows/windows-laptop-handoff-2026-06-22.md`
- `windows/README.md`
- `windows/installer/README.md`

Important source paths:

- `windows/OpenA8DJWindows.sln`
- `windows/driver/OpenA8DJUsb.c`
- `windows/driver/OpenA8DJUsb.h`
- `windows/driver/OpenA8DJUsb.inf`
- `windows/include/OpenA8DJShared.h`
- `windows/tools/opena8djctl.c`
- `windows/audio/OpenA8DJAudioEngine.c`
- `windows/audio/OpenA8DJAudioEngine.h`
- `windows/tests/`

## OpenAI/Codex Authentication

The macOS source machine was switched to API-key auth. The key secret must not
be committed, emailed, pasted into docs, or included in ZIP bundles.

Known identity to reproduce on Windows:

- OpenAI Platform organization: `Personal`
- Do not use organization: `groundcontrol`
- API key display name in Platform: `fer-OAI-personal-free`
- Visible secret suffix in Platform: `sk-...T6cA`
- Local Codex key suffix observed on macOS: `zST6cA`
- Platform key status: `Active`
- Project access: `Default project`
- Permissions shown in Platform: `All`
- Billing surface shown in Platform: `Free trial`

On the Windows laptop/tablet:

1. Create or copy the API key secret securely from OpenAI Platform.
2. Set it only in the local shell/user secret store, not in the repo:

   ```powershell
   setx OPENAI_API_KEY "PASTE_SECRET_KEY_HERE"
   ```

   Restart the terminal after `setx`, or set it for the current session:

   ```powershell
   $env:OPENAI_API_KEY = "PASTE_SECRET_KEY_HERE"
   ```

3. Force Codex to use API-key auth in the Windows Codex config:

   ```toml
   forced_login_method = "api"
   ```

4. Log Codex in using stdin so the key is not printed:

   ```powershell
   $env:OPENAI_API_KEY | codex login --with-api-key
   codex login status
   ```

5. Expected status should say it is logged in using an API key and should show
   the same final visible suffix as the Platform `fer-OAI-personal-free` key.

## What Is Already Implemented

Implemented and committed:

- Windows design finalized as ACX-first, KMDF/WDF USB transport, shared engine,
  ASIO facade later.
- KMDF USB surface for `USB\VID_17CC&PID_1978`.
- API v2 surface state for USB, audio endpoints, isochronous engine, MIDI,
  controls, ASIO, topology, diagnostics.
- Stable Windows rates limited to 44.1 kHz and 48 kHz.
- 88.2/96 kHz marked planned, not stable.
- `start` intentionally rejected until real streaming exists.
- Offline user-mode audio engine prototype:
  - 8 input channels and 8 output channels.
  - render/capture rings.
  - deterministic underrun/overrun counters.
  - 24-bit big-endian pack/unpack helpers.
- macOS offline test runner:

  ```sh
  windows/tests/run_offline_tests.py
  ```

- Windows offline test runner:

  ```powershell
  .\windows\tests\run-offline-tests.ps1
  ```

The PowerShell runner should build an offline executable named:

```text
windows\dist\offline-tests\Release\opena8dj-audio-engine-contract.exe
```

That executable is only an offline contract test. It is not the OpenA8DJ
driver, not an ASIO driver, not a Control Center, and not an audio endpoint.

## What Could Not Be Built On macOS

The macOS preparation host did not have:

- MSBuild
- PowerShell
- Windows SDK
- Windows Driver Kit
- Inf2Cat
- Visual Studio Developer PowerShell
- MinGW cross-compiler

Therefore no Windows `.exe`, `.sys`, `.cat`, `.msi`, or installable driver
package was produced on macOS. This is intentional; do not fake a Windows build.

## First Commands On Windows

From the repo root in a Developer PowerShell:

```powershell
git checkout windows/rebuild-surface
git pull --ff-only
codex login status
.\windows\tests\run-offline-tests.ps1
```

Then build the WDK solution:

```powershell
.\windows\scripts\build-driver.ps1 -Configuration Release -Platform x64
```

If that passes, build the development installable ZIP:

```powershell
.\windows\scripts\build-installable-package.ps1 -Configuration Release -Platform x64
```

Expected development package:

```text
windows\dist\installer\OpenA8DJUsb-Release-x64-installer.zip
```

This package is still diagnostic/development only unless real validation passes.

## Hardware Use Rules

Before touching the physical Audio 8 DJ on Windows:

1. Add a hardware-use log entry in the repo, under `local-analysis/` or the
   existing project hardware log convention.
2. Acquire the shared hardware lock if available on the Windows checkout.
3. Keep the lock only for the hardware action.
4. Release it immediately after install/test/uninstall.
5. Record exact artifact hashes and test results.

Never treat "compiled" as "works". Minimum evidence before any normal tester
handoff:

- WDK build output.
- INF verification and catalog generation.
- Driver Store install with `pnputil`.
- Device visible as `USB\VID_17CC&PID_1978`.
- `opena8djctl.exe surface`.
- `opena8djctl.exe topology`.
- `opena8djctl.exe diagnostics`.
- Audio endpoint enumeration when ACX/KS exists.
- MIDI in/out when implemented.
- Traktor selection and DVS validation when ASIO/WASAPI is ready.
- CPU/DPC trace.
- External physical capture.
- Listening sign-off.
- SHA256 hashes for the exact artifact.

## Prompt For Fresh Windows Codex

Copy this full prompt into the new Windows Codex thread:

```text
You are the Windows implementation owner for OpenA8DJ, working on a Windows
laptop/tablet with the repo checked out locally.

Project:
- Repo: opena8dj.
- Branch: windows/rebuild-surface.
- Stay on the Windows branch. Do not modify the macOS main release line.
- Treat macOS 0.5.0/main only as a behavior oracle.

Authentication:
- Codex must use OpenAI API-key auth, not ChatGPT-session auth.
- Force local Codex config: forced_login_method = "api".
- Use the OpenAI Platform Personal organization, not groundcontrol.
- The intended API key is named fer-OAI-personal-free.
- The key is active, under Default project, permissions All, and the visible
  secret suffix in Platform is sk-...T6cA.
- Never print, commit, email, or paste the full secret key. Ask the user to set
  OPENAI_API_KEY locally and run:
  $env:OPENAI_API_KEY | codex login --with-api-key
  codex login status

Current Windows state:
- Latest handoff commit: 589ac48 Add Windows offline audio engine prototype.
- Design docs:
  docs-state/windows/windows-architect-state.md
  docs-state/windows/windows-final-design-and-implementation-plan.md
  docs-state/windows/windows-implementation-progress.md
  docs-state/windows/windows-laptop-handoff-2026-06-22.md
- Windows source:
  windows/OpenA8DJWindows.sln
  windows/driver/OpenA8DJUsb.*
  windows/include/OpenA8DJShared.h
  windows/tools/opena8djctl.c
  windows/audio/OpenA8DJAudioEngine.*
  windows/tests/

Hard rules:
- Windows is experimental only.
- Do not call any Windows artifact a normal tester candidate unless the exact
  artifact has passed real Windows install, endpoint, audio, MIDI, Traktor,
  CPU/DPC, physical capture, and listening validation.
- Otherwise label it: diagnostic only, sound quality not validated.
- Before physical Audio 8 DJ use, write a hardware-use log entry and acquire
  the shared hardware lock if available. Release it immediately after use.
- Do not confuse compile success with functional support.
- Do not use Native Instruments proprietary binaries, firmware, logos, or
  incompatible copied code.

First tasks:
1. Inspect git status and confirm branch windows/rebuild-surface.
2. Confirm Codex auth is API key and not ChatGPT:
   codex login status
3. Run offline Windows tests:
   .\windows\tests\run-offline-tests.ps1
4. Install/verify toolchain if needed:
   Visual Studio 2022 Build Tools, Windows SDK, Windows Driver Kit, Inf2Cat.
5. Build:
   .\windows\scripts\build-driver.ps1 -Configuration Release -Platform x64
6. If build passes, package:
   .\windows\scripts\build-installable-package.ps1 -Configuration Release -Platform x64
7. Do not install or touch hardware until hardware-use logging and lock are
   ready.
8. If install is authorized, install only as diagnostic:
   .\windows\scripts\install-driver.ps1 -Configuration Release -Platform x64 -TrustTestCertificate -EnableTestSigning -SkipBuild
9. Verify:
   .\windows\scripts\verify-driver.ps1 -Configuration Release -Platform x64
   .\windows\dist\Release\x64\opena8djctl.exe surface
   .\windows\dist\Release\x64\opena8djctl.exe topology
   .\windows\dist\Release\x64\opena8djctl.exe diagnostics
10. Record exact hashes, logs, screenshots, and failures.

Implementation path after build:
- Fix WDK/MSBuild/INF errors first.
- Keep the current KMDF USB surface truthful.
- Add ACX proof-of-fit skeleton next.
- Keep ASIO as a facade over the shared engine, not a second engine.
- Keep MIDI/control out of the audio hot path.
- Only replace start rejection after real preallocated isochronous streaming is
  implemented and validated.

Report back with:
- branch and commit hash;
- Codex auth mode and key suffix only;
- exact build commands and outputs;
- artifact paths and SHA256 hashes;
- whether hardware was touched;
- install/verify results;
- blockers and next safe step.
```

## Email Summary

Send or forward this document with the handoff ZIP generated on macOS. The ZIP
must contain only context/source/docs and no API secrets.
