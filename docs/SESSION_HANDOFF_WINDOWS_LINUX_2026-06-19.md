# Session Handoff: Windows And Linux Experimental Tracks - 2026-06-19

## Purpose

This handoff preserves the Windows and Linux context before this session closes.
It is written for a third architect who may later merge or continue the work.

The macOS/C++ mainline remains the priority implementation. Windows and Linux
are experimental tracks and must stay isolated until their candidate payloads,
docs, build scripts, packages, and validation reports are organized enough that
macOS work can ignore them completely.

## Worktrees And Branches

Current worktrees observed during this handoff:

```text
/Users/fer/dev/opena8dj              windows/rebuild-surface
/Users/fer/dev/opena8dj-linux-agent  linux/full-driver-agent
/Users/fer/dev/audio8djcpp           driverkit/cpp-redesign
/Users/fer/dev/audio8djrust          rust/modular-core-spike
/private/tmp/opena8dj-main-merge     main
```

Windows work is in:

```text
branch:   windows/rebuild-surface
worktree: /Users/fer/dev/opena8dj
```

Linux work is in:

```text
branch:   linux/full-driver-agent
worktree: /Users/fer/dev/opena8dj-linux-agent
agent:    Kant
agent id: 019ee0fe-3cfe-7bc3-b18a-0533b91587d2
```

## Shared Hardware Rule

No hardware was used for this handoff work.

Any future action that touches Audio 8 DJ, iRig, USB live state,
playback/capture, Traktor, driver live install/reload, or CPU/audio measurement
must use:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
./scripts/shared-hardware-lock-status
```

Build-only, documentation, static analysis, and package metadata work do not
need the lock.

## Windows Track Summary

Status:

```text
windows experimental candidate source/tooling
diagnostic only, sound quality not validated
not built on Windows WDK in this session
```

Implemented source-level changes:

- Windows shared API v2 in `windows/include/OpenA8DJShared.h`.
- New IOCTLs:
  - `IOCTL_OPENA8DJ_GET_SURFACE`
  - `IOCTL_OPENA8DJ_GET_TOPOLOGY`
  - `IOCTL_OPENA8DJ_GET_DIAGNOSTICS`
- Driver surface/diagnostic implementation in `windows/driver/OpenA8DJUsb.c`.
- Driver context counters in `windows/driver/OpenA8DJUsb.h`.
- `opena8djctl` commands:
  - `surface`
  - `topology`
  - `diagnostics`
  - expanded `status`
- `start` intentionally rejects streaming until a real isochronous engine
  exists. This prevents fake readiness.

Windows installer/tooling added:

- `windows/scripts/OpenA8DJ.WindowsCommon.psm1`
- `windows/scripts/sign-driver.ps1`
- `windows/scripts/install-driver.ps1`
- `windows/scripts/uninstall-driver.ps1`
- `windows/scripts/verify-driver.ps1`
- `windows/scripts/build-installable-package.ps1`
- `windows/scripts/package-installer.ps1`
- `windows/scripts/export-attestation-cab.ps1`

Windows installer docs/manifests:

- `windows/installer/README.md`
- `windows/installer/driver-package.manifest.json`
- `windows/installer/tools-package.manifest.json`
- `docs/WINDOWS_STANDALONE_INSTALLER_DESIGN_2026-06-19.md`
- `docs/WINDOWS_TESTER_INSTALL_GUIDE_UNSIGNED_2026-06-19.md`

Windows candidate payload rule:

- A candidate is not only `OpenA8DJUsb.sys`.
- It must include INF/SYS/CAT, tools, installer/uninstaller, verification,
  README-FIRST, hashes, signing state, metadata, limitations, and rollback.

Windows next steps:

1. Build on a Windows host with Visual Studio 2022, Windows SDK, and WDK.
2. Run:
   ```powershell
   .\windows\scripts\build-installable-package.ps1 -Configuration Release -Platform x64
   ```
3. Validate the generated driver package and installer ZIP.
4. Test install on a clean Windows test machine.
5. Verify `opena8djctl surface`, `topology`, and `diagnostics`.
6. Keep audio endpoint/streaming marked not ready until ACX/WaveRT/isochronous
   engine work exists and passes validation.

## Linux Track Summary

Status:

```text
linux experimental design/scaffold/package track
diagnostic only, sound quality not validated
no ALSA driver implemented yet
no .deb or .rpm produced yet
```

Linux agent work added in `/Users/fer/dev/opena8dj-linux-agent`:

- `linux/README.md`
- `linux/ARCHITECTURE.md`
- `linux/CONFIGURATION_MODEL.md`
- `linux/QUALITY_AND_PERFORMANCE_GATES.md`
- `linux/SND_USB_CAIAQ_AUDIT.md`
- `linux/PACKAGING.md`
- `linux/CANDIDATE_PAYLOAD.md`
- `linux/driver/README.md`
- `linux/driver/Makefile`
- `linux/packaging/debian/README.md`
- `linux/packaging/rpm/README.md`
- `agents/linux-driver-agent/STATUS.md`

Linux design target:

- ALSA-native driver path.
- 8 playback channels and 8 capture channels.
- A/B/C/D stereo pair naming and routing.
- 44.1/48 kHz first.
- 88.2/96 kHz hidden until physical validation.
- ALSA controls for input mode, ground lift flags, software lock, profiles, and
  diagnostics.
- ALSA rawmidi in/out.
- PREEMPT_RT-friendly hot paths.
- Debian/Ubuntu packaging first; RPM second.

Linux candidate payload rule:

- A candidate is not only `.ko`.
- It must include driver source/module channel, tools, config/profile schema,
  udev/UCM if needed, README-FIRST, uninstall/rollback, diagnostics/verify
  report, hashes, build metadata, debug/symbols policy, provenance/licensing,
  readiness labels, and limitations.

Linux next steps:

1. Choose exact kernel baseline for audit.
2. Complete source-level `snd-usb-caiaq` audit against that kernel.
3. Decide formally between extending `snd-usb-caiaq` and creating
   `snd-opena8dj`.
4. Add real Debian metadata first:
   - `opena8dj-dkms`
   - `opena8dj-tools`
   - optional `opena8dj-alsa-ucm`
   - optional `opena8dj-udev`
5. Add RPM metadata second.
6. Do not install/load/bind hardware as part of early package install.

## Cross-Platform Research And Policy Docs

Key docs created or updated:

- `docs/REALTIME_AUDIO_DRIVER_RESEARCH_WINDOWS_LINUX_2026-06-19.md`
- `docs/WINDOWS_IDEAL_REALTIME_AUDIO_DRIVER_STUDY_2026-06-19.md`
- `docs/LINUX_IDEAL_REALTIME_AUDIO_DRIVER_STUDY_2026-06-19.md`
- `docs/WINDOWS_IMPLEMENTATION_PLAN_2026-06-19.md`
- `docs/WINDOWS_PERFORMANCE_ROUTING_AND_MACOS_LESSONS_2026-06-19.md`
- `docs/EXPERIMENTAL_CANDIDATE_PAYLOAD_REQUIREMENTS_2026-06-19.md`
- `docs/EXPERIMENTAL_WINDOWS_LINUX_MERGE_POLICY_2026-06-19.md`
- `docs/SHARED_HARDWARE_COORDINATION.md`
- `agents/README.md`
- `agents/linux-driver-agent/PROMPT.md`
- `agents/linux-driver-agent/IMPLEMENTATION_CONTRACT.md`
- `scripts/bootstrap-linux-driver-agent`

## Merge Guidance

Do not merge both branches blindly.

Recommended merge order for a third architect:

1. Inspect Windows branch:
   ```sh
   cd /Users/fer/dev/opena8dj
   git status --short --branch -uall
   git diff --check
   ```
2. Inspect Linux branch:
   ```sh
   cd /Users/fer/dev/opena8dj-linux-agent
   git status --short --branch -uall
   git diff --check
   ```
3. Review:
   - `docs/EXPERIMENTAL_WINDOWS_LINUX_MERGE_POLICY_2026-06-19.md`
   - `docs/EXPERIMENTAL_CANDIDATE_PAYLOAD_REQUIREMENTS_2026-06-19.md`
   - `linux/CANDIDATE_PAYLOAD.md`
   - `linux/PACKAGING.md`
4. Merge docs and isolated platform directories before any shared code.
5. Preserve macOS priority and run macOS smoke/build checks after merge.
6. Keep generated artifacts out of git unless intentionally released with
   hashes and validation reports.

## Verification Performed In This Session

Windows side:

- `git diff --check`
- JSON validation for Windows installer manifests.
- ASCII checks on new Windows policy/docs.
- No WDK build was possible on macOS.

Linux side, reported by agent Kant:

- `git diff --check`
- `make -C linux/driver help`
- `make -C linux/driver status`
- `make -C linux/driver module` intentionally fails to prevent accidental fake
  module readiness.
- No hardware tests.

## Known Non-Ready Areas

Windows:

- No compiled Windows artifacts from this session.
- No Microsoft signing.
- No test install on Windows yet.
- No Windows audio endpoint.
- No real isochronous streaming engine.
- No MIDI implementation.
- No audio-quality validation.

Linux:

- No implemented ALSA driver yet.
- No selected kernel baseline.
- No final CAIAQ-vs-new-driver decision.
- No DKMS metadata yet.
- No `.deb` or `.rpm`.
- No hardware enumeration.
- No PCM smoke.
- No audio-quality validation.

## Final State Label

Use this label for both platform tracks until proven otherwise:

```text
experimental candidate groundwork
diagnostic only, sound quality not validated
```
