# Windows Architect State

Date: 2026-06-22
Owner role: OpenA8DJ Windows principal architect
Branch audited: `windows/rebuild-surface`
Workspace: `/Users/fer/dev/opena8dj`
Hardware use in this pass: none

Follow-up: the design was finalized in
`docs-state/windows/windows-final-design-and-implementation-plan.md`. Treat this
file as the audit baseline and the final-design document as the implementation
contract.

## Status Label

Windows is an experimental implementation track only.

Current label:

```text
windows experimental source/tooling
offline/prototype only
diagnostic only, sound quality not validated
not a public Windows release
not ready for Traktor or user audio testing
```

Do not offer a normal Windows build to a tester unless the exact artifact has
passed real Windows install validation, audio validation, MIDI validation,
Traktor validation, CPU/DPC validation, and physical sound-quality validation.
Until then, any installable Windows artifact must be marked `diagnostic only,
sound quality not validated`.

## Audit Summary

The active branch is `windows/rebuild-surface` at commit `984078c` and matches
`origin/windows/rebuild-surface`. `origin/main` is the macOS product line at
`27a8410`, with the 0.5.0 public validation and notarization state recorded
after the `v0.5.0` tag. The `v0.5.0` tag itself is not the latest release-state
documentation source because later main commits document the final signed,
notarized, stapled DMG publication flow.

Observed local tree state before this document was added:

- Tracked Windows files exist under `windows/`.
- The working tree already had uncommitted Windows updates in
  `docs/WINDOWS.md`, `windows/README.md`,
  `windows/include/OpenA8DJShared.h`, `windows/driver/OpenA8DJUsb.c`,
  `windows/driver/OpenA8DJUsb.h`, and `windows/tools/opena8djctl.c`.
- Additional Windows installer scripts and Windows/Linux planning documents
  were present as untracked files.
- `.github/workflows/windows-driver.yml` exists and builds on `windows-2022`
  only if the runner has the Windows SDK/WDK tools, especially `Inf2Cat.exe`.
- `git diff --check` passed before this document was added.

Existing versioned Windows source:

- `windows/OpenA8DJWindows.sln`
- `windows/driver/OpenA8DJUsb.vcxproj`
- `windows/driver/OpenA8DJUsb.inf`
- `windows/driver/OpenA8DJUsb.c`
- `windows/driver/OpenA8DJUsb.h`
- `windows/include/OpenA8DJShared.h`
- `windows/tools/OpenA8DJControl.vcxproj`
- `windows/tools/opena8djctl.c`
- `windows/scripts/build-driver.ps1`
- `.github/workflows/windows-driver.yml`

Existing untracked Windows source/tooling observed in the workspace:

- `windows/installer/`
- `windows/scripts/OpenA8DJ.WindowsCommon.psm1`
- `windows/scripts/build-installable-package.ps1`
- `windows/scripts/export-attestation-cab.ps1`
- `windows/scripts/install-driver.ps1`
- `windows/scripts/package-installer.ps1`
- `windows/scripts/sign-driver.ps1`
- `windows/scripts/uninstall-driver.ps1`
- `windows/scripts/verify-driver.ps1`
- `docs/WINDOWS_IMPLEMENTATION_PLAN_2026-06-19.md`
- `docs/WINDOWS_IDEAL_REALTIME_AUDIO_DRIVER_STUDY_2026-06-19.md`
- `docs/WINDOWS_STANDALONE_INSTALLER_DESIGN_2026-06-19.md`
- `docs/WINDOWS_PERFORMANCE_ROUTING_AND_MACOS_LESSONS_2026-06-19.md`
- `docs/WINDOWS_TESTER_INSTALL_GUIDE_UNSIGNED_2026-06-19.md`
- `docs/REALTIME_AUDIO_DRIVER_RESEARCH_WINDOWS_LINUX_2026-06-19.md`
- `docs/EXPERIMENTAL_WINDOWS_LINUX_MERGE_POLICY_2026-06-19.md`
- `docs/SESSION_HANDOFF_WINDOWS_LINUX_2026-06-19.md`

## Current Windows Implementation

The current Windows code is a KMDF USB function-driver surface for
`USB\VID_17CC&PID_1978`. It creates a device interface for
`GUID_DEVINTERFACE_OPENA8DJ_USB`, selects the configured USB interface, and maps
the known CAIAQ-style endpoints:

```text
bulk out:        0x01
bulk in:         0x81
isochronous in:  0x82
isochronous out: 0x06
```

The Windows shared API has been expanded to version 2 with:

- 8 input channels, 8 output channels, four stereo A/B/C/D pairs;
- stable-rate flags for 44.1 and 48 kHz;
- planned-rate flags for 88.2 and 96 kHz;
- surface state for USB transport, controls, audio endpoints, isochronous
  engine, MIDI, and ASIO;
- topology descriptors for render/capture channels;
- diagnostics counters for start requests, rejected starts, stops, format
  changes, controls, profiles, and future stream counters.

The CLI `opena8djctl.exe` can query:

- `status`
- `surface`
- `topology`
- `diagnostics`
- `caps`
- `format`
- `stream`
- profile/control commands
- `set-format`
- `start` and `stop`

The current `start` path is intentionally honest: it increments start counters,
keeps `Streaming` false, keeps `StreamingEngineReady` false, returns the stream
state, increments rejected-start counters, and completes with
`STATUS_NOT_SUPPORTED`. That is the correct behavior until a real Windows
isochronous engine exists.

## Critical Gaps Found

1. No Windows audio endpoints exist.

   Windows apps cannot select OpenA8DJ as a WASAPI/WDM/KS device, and Traktor
   cannot use it as an audio interface.

2. No real isochronous streaming engine exists.

   Pipes are mapped, but there are no preallocated URB/request pools, no packet
   scheduler, no render/capture ring, no capture-paced output model, and no
   physical audio proof.

3. Control state is local, not hardware-proven.

   The IOCTL control/profile paths update `RawControlState` in the driver
   context. This is useful for the control contract, but it is not yet evidence
   that EP1 CAIAQ commands are written to the physical Audio 8 DJ or that the
   hardware state changed.

4. MIDI is metadata/planned only.

   Capability structs report 1 MIDI input and 1 MIDI output as target shape, but
   `MidiReady` is false and no Windows MIDI endpoint is implemented.

5. ASIO is planned only.

   There is no ASIO driver, no Steinberg SDK integration boundary, and no
   Traktor ASIO validation.

6. 88.2/96 kHz are marked planned but currently accepted by `set-format`.

   `OpenA8DJ_ValidateAudioFormat` accepts all four sample rates in
   `kOpenA8DJSampleRates`. Product docs correctly mark 88.2/96 kHz as planned.
   The next implementation pass should either reject planned rates until
   validated or return an explicit diagnostic/planned-state warning so the API
   cannot imply production support.

7. The build is not verified in this environment.

   This macOS workspace cannot prove a WDK build, driver load, `InfVerif`,
   `Inf2Cat`, `pnputil` install, Device Manager state, or Windows audio
   enumeration. Those remain Windows-host gates.

8. No Windows physical validation exists.

   There is no Windows external capture, no real music capture, no Traktor
   DVS scope, no MIDI loopback, no CPU/DPC trace, and no install/uninstall
   matrix evidence.

## macOS 0.5.0 Oracle

The macOS `main` line is the behavior oracle, not a branch to modify from the
Windows track.

Current `origin/main` 0.5.0 state records:

- signed, notarized, stapled public DMGs;
- DMG-only GitHub release distribution with checksums;
- install validation from downloaded public DMGs;
- `Open Audio 8 DJ` visible in Core Audio as 8 inputs and 8 outputs at 48 kHz;
- 44.1 and 48 kHz as validated rates;
- CoreMIDI input/output endpoints visible;
- Control Center and `opena8dj-control` present and functional;
- DVS Vinyl state as the stable default;
- A/B/C/D routing surface;
- physical real-music external-capture validation on the signed artifact family;
- human listening sign-off on the installed stable profile.

Windows must earn equivalent evidence in Windows terms. A Windows driver that
only compiles, installs a USB device, or answers IOCTLs is not equivalent to
macOS 0.5.0.

## Architecture Decision

Recommended Windows architecture:

```text
Traktor / DAWs / Windows apps
    |
    +-- ASIO facade for pro/DJ apps, after the shared engine is real
    |
    +-- WASAPI shared/exclusive and KS-facing normal Windows endpoints
    |
ACX 1.1 audio driver, KMDF, WaveRT-style streaming concepts
    |
OpenA8DJ Windows audio engine
    - A/B/C/D topology
    - render/capture rings
    - packet scheduler
    - 24-bit CAIAQ format conversion
    - clock/cadence accounting
    - diagnostics counters
    - MIDI/control serialization
    |
WDF USB CAIAQ transport
    - EP1 command channel
    - 0x82 isochronous capture
    - 0x06 isochronous playback
    |
Audio 8 DJ hardware
```

Decision details:

- Use KMDF/WDF USB for the vendor-specific Audio 8 DJ transport.
- Investigate ACX 1.1 first for the Windows audio endpoint layer. Microsoft
  describes ACX as KMDF-based, audio-domain WDF objects, WaveRT-based
  streaming support, and recommended for new audio driver development.
- Keep AVStream/KS as the fallback if ACX cannot compose cleanly with the
  vendor-specific USB transport.
- Do not lead with direct PortCls as the primary plan. Microsoft describes
  PortCls as typical for PCI/DMA audio devices, while Audio 8 DJ is a
  vendor-specific external USB device.
- Do not rely on the in-box USB Audio 2.0 driver. Audio 8 DJ is not currently
  represented in this branch as a class-compliant UAC2 device; Windows
  `usbaudio2.sys` is for USB Audio Class 2.0 devices and has feedback/topology
  constraints that do not replace CAIAQ transport work.
- Use WinUSB/libusb/user-mode tools only for protocol diagnostics or early M1
  probes, not as the production audio path. User-mode USB cannot be the final
  low-latency Traktor path.
- Treat ASIO as a product-critical facade for Traktor/DAW use, but build it over
  the same streaming engine. Do not create a second audio engine that can drift
  from WASAPI/KS behavior.
- Keep the user-mode Control Center/service out of the streaming hot path.
- Keep installer ownership split: driver package for INF/CAT/SYS and Driver
  Store state; tools installer for CLI, diagnostics, and future Control Center;
  optional bundle EXE only as orchestration.

## Capability Matrix

| macOS 0.5.0 capability | Windows target | Current Windows status | Implementation path | Validation needed |
| --- | --- | --- | --- | --- |
| Signed/notarized/stapled DMG installer with checksums | Microsoft-signed driver package plus signed installer/bundle and checksums | Not available. Development ZIP design only; no Microsoft-signed catalog | WDK build, InfVerif, Inf2Cat, attestation or HLK/WHQL via Partner Center, signed driver installer, signed tools MSI, checksums | Clean Windows 10/11 install, signature verification, Driver Store state, uninstall/reinstall/upgrade, reboot/hotplug |
| Core Audio HAL device visible as `Open Audio 8 DJ` | Windows audio device visible in Sound settings, WASAPI, KS, and pro apps | Missing | ACX 1.1 endpoint skeleton first; AVStream/KS fallback if ACX fails | Windows Sound enumeration, WASAPI shared/exclusive tone, KS visibility, no fake endpoint claims |
| 8 outputs as A/B/C/D stereo deck pairs | 8 render channels or four stereo render endpoints with A/B/C/D names | Topology metadata only | Prototype both single 8-channel and four-stereo-pair models; let Traktor/DAWs decide | Channel isolation tones on A/B/C/D, Traktor deck routing, DAW routing |
| 8 inputs as A/B/C/D stereo input pairs | 8 capture channels or four stereo capture endpoints with A/B/C/D names | Topology metadata only | ACX/KS capture endpoints backed by CAIAQ capture ring | Known-signal input capture on A/B/C/D, no cross-talk, DVS input scope |
| 44.1 and 48 kHz validated | 44.1 and 48 kHz stable first release rates | Surface marks stable; no Windows audio proof | Start at 48 kHz / 512 frames, then 44.1 kHz after physical proof | WASAPI/ASIO playback/capture at both rates, speed/pitch check, physical capture |
| 88.2/96 kHz not primary release claim | Hide or mark 88.2/96 kHz as extended/unvalidated | Marked planned, but `set-format` currently accepts them | Reject planned rates or expose explicit planned-state diagnostics until validated | Same full matrix as 44.1/48 before advertising |
| CoreMIDI In/Out visible | Windows MIDI In/Out visible to apps | Missing; metadata only | Add MIDI endpoint implementation, likely separate from audio hot path but sharing transport state safely | MIDI app visibility, loopback, hotplug/reboot/sleep, no stream disturbance |
| Control Center profiles: DVS Vinyl, DVS CD/Line, Output Only and support export | Windows Control Center equivalent and CLI profiles: DVS Vinyl, DVS CD/Line, recording, playback, MIDI-only, diagnostics | CLI profile cache exists; no Windows UI; no hardware EP1 proof | Stabilize IOCTL contract, implement EP1 commands, then WinUI/WPF/Win32 Control Center or lightweight service/app | Hardware state readback if possible, profile application during idle, support export with logs |
| DVS Vinyl default state validated | Windows DVS Vinyl profile for Traktor timecode | Local control struct only | EP1 hardware profile command plus input decode/capture routing | Traktor timecode scope, absolute/relative modes, lift needle behavior |
| DVS CD/Line support | Windows DVS CD/Line profile | Local control struct only | Same control and input path, CD/line input mode | Traktor CD/line timecode scope and audio quality |
| Real music physical capture and human listening sign-off | Windows physical audio gate with exact artifact hash | Missing | External interface capture, WAV comparison, listening gate after numeric pass | Real music through Audio 8 DJ, independent capture, no clicks/noise/drift/dropouts |
| Low/stable CPU profile | Low CPU plus bounded DPC/ISR under Windows load | Missing | Preallocated hot path, ETW/WPA instrumentation, no per-packet logging | CPU, DPC/ISR by module, packet cadence, sustained playback/capture/Traktor stress |
| macOS LaunchAgent and installed support tools | Windows service/app only if needed, not in audio hot path | Tooling scripts only | Tools MSI, optional diagnostics service, Start Menu entry | Install/uninstall cleanliness, no streaming jitter from diagnostics |
| `opena8dj-control` diagnostics | `opena8djctl` and future diagnostics export | CLI exists for IOCTL surface; not built/validated here | Expand `verify-driver.ps1`, add ETW/log bundle export, add control readback | JSON report, hashes, signature, Driver Store, surface/topology/diagnostics, event logs |
| User uninstall path | Clean Windows uninstall | Script design present, not Windows-validated | Driver Store uninstall via `pnputil`, tools uninstall via MSI, bundle repair/uninstall | Driver package removed, device unbound cleanly, files/logs policy respected |
| Legal/provenance policy | Same: no NI binaries, firmware, installers, logos, proprietary payloads | Policy docs present | Keep release payload audit in packaging gate | Artifact inspection, release notes, license/NOTICE/brand policy included |

## Milestones

### M0: Inventory and build skeleton

Current status: partially done, not closed.

Done:

- Branch and workspace identified.
- Windows solution and KMDF project exist.
- INF matches `USB\VID_17CC&PID_1978`.
- CLI project exists.
- GitHub Windows workflow exists.
- Development installer ZIP scripts/design exist in workspace.
- This architecture/state document now records the audit baseline.

Exit criteria still needed:

- Commit or deliberately stage the current Windows docs/scripts/code as a clean
  Windows-only set.
- Build on a real Windows host with Visual Studio 2022, matching SDK/WDK, and
  `Inf2Cat`.
- Save build log, tool versions, source commit, hashes, and produced files.
- Run `InfVerif` explicitly and store output.

### M1: USB/control/MIDI offline or diagnostic

Goal:

- Prove the Windows package can bind to Audio 8 DJ and exchange diagnostic
  control traffic without claiming audio readiness.

Implementation:

- Build signed/test-signed driver package.
- Install on Windows test machine with hardware lock and hardware-use log.
- Validate pipe mapping.
- Implement EP1 command read/write for control state.
- Keep MIDI as diagnostic until endpoint model is chosen.

Exit evidence:

- Driver Store report.
- Device Manager state.
- `opena8djctl surface/topology/diagnostics` output.
- EP1 control command logs.
- Hardware-use log.
- Exact package hashes.

### M2: Audio playback/capture proof with stable buffers

Goal:

- Produce deterministic physical audio through Audio 8 DJ on Windows, without
  Traktor or ASIO yet.

Implementation:

- Add real CAIAQ isochronous engine.
- Preallocate IN/OUT WDF requests and packet descriptors.
- Start with 48 kHz / 512 frames / Output A/B.
- Use capture-paced OUT scheduling as the first hardware hypothesis.
- Add counters for packet status, queue depth, late completions, underruns,
  overruns, packet errors, render frames, capture frames.

Exit evidence:

- 1 kHz tone on Output A/B captured externally.
- No fake streaming state.
- Capture proof and logs stored under `local-analysis/windows/`.
- CPU/DPC trace for the run.

### M3: ASIO/Traktor surface

Goal:

- Make the device usable for Traktor and DAWs with low latency while preserving
  normal Windows endpoint behavior.

Implementation:

- Complete ACX/KS render/capture endpoints first.
- Add ASIO facade over the same streaming engine.
- Keep Steinberg ASIO SDK/license handling outside the repo until explicitly
  cleared.
- Decide endpoint shape with real app behavior: one 8-channel endpoint, four
  stereo pairs, or dual strategy.

Exit evidence:

- Traktor sees/selects OpenA8DJ.
- Deck A/B/C/D routing works.
- DVS Vinyl and CD/Line scopes are stable.
- WASAPI exclusive still works.
- ASIO and WASAPI report coherent latency/buffer behavior.

### M4: App/Control Center and installer

Goal:

- Provide a non-technical Windows install and control surface that does not hide
  driver state.

Implementation:

- Driver installer: INF/CAT/SYS, Driver Store, Microsoft signing gate.
- Tools installer: `opena8djctl`, diagnostics collector, future Control Center.
- Optional bundle EXE only after the two installers are independently reliable.
- Control Center profiles: DVS Vinyl, DVS CD/Line, recording, playback,
  MIDI-only, diagnostics.

Exit evidence:

- Clean install/uninstall/reinstall on Windows 10 and 11.
- Start Menu/tool presence.
- Support export contains logs, hashes, driver state, endpoint state, CPU/DPC
  summary, and limitations.

### M5: Validation ladder comparable to macOS

Goal:

- Turn Windows from source prototype into evidence-backed experimental
  candidate.

Validation ladder:

1. Build/package reproducibility.
2. Driver install/uninstall/reboot/hotplug.
3. Device enumeration and endpoint visibility.
4. 44.1/48 kHz playback/capture.
5. A/B/C/D channel isolation.
6. MIDI loopback.
7. Traktor DVS Vinyl and CD/Line.
8. CPU/DPC/ETW under idle, playback, capture, Traktor, and stress.
9. Physical tone/click capture.
10. Physical real-music capture.
11. Human listening after numeric gates pass.

### M6: Signed/public experimental release candidate

Goal:

- Publish a Windows experimental candidate only after the exact artifact passes
  the Windows ladder.

Required before M6:

- Microsoft-signed catalog or a clearly internal/test-signed diagnostic label.
- Signed installer(s).
- Checksums.
- Release notes and limitation labels.
- Rollback and uninstall docs.
- Validation report with exact artifact hashes.
- Public wording that Windows is experimental unless and until all gates pass.

## Immediate Next Steps

1. Normalize the current Windows workspace.

   Decide which existing uncommitted/untracked Windows docs/scripts are part of
   the Windows branch baseline. Commit them in a Windows-only commit or move
   anything out of scope before deeper implementation.

2. Fix API truth around controls and rates.

   The driver should not report hardware controls as ready until EP1 hardware
   write/readback exists. Planned rates should not be accepted as normal format
   changes until validated or clearly marked diagnostic.

3. Add M0 build evidence on Windows.

   Use a real Windows 10/11 build host with Visual Studio 2022, SDK, WDK,
   `InfVerif`, and `Inf2Cat`. Store logs and hashes. macOS cannot close this
   gate.

4. Start ACX proof-of-fit before deep streaming work.

   Build the smallest ACX render/capture endpoint skeleton. It may be virtual
   or deterministic silence/tone first. This proves endpoint model and tooling
   before hardware timing is entangled.

5. Add hardware-use log discipline before M1.

   Before the first Windows install/load/bind/open operation against the real
   Audio 8 DJ, add a repo hardware-use log entry and acquire the shared
   hardware lock. Release it on every exit path.

## Blockers And Risks

- Need a Windows build host with Visual Studio 2022, Windows SDK, and WDK.
- Need Windows test hardware access for real Audio 8 DJ validation.
- Need Microsoft Partner Center / EV certificate / attestation or HLK path for
  public driver signing.
- Need to prove ACX can own or compose with the vendor-specific USB transport.
- Need to choose endpoint shape based on Traktor and DAW behavior, not taste.
- Need ASIO SDK/licensing boundary before any ASIO source lands.
- Need CPU/DPC/ETW evidence; driver counters alone are insufficient.
- Need physical external capture and human listening before any sound-quality
  claim.

## External References Checked

- Microsoft ACX overview:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-audio-class-extensions-overview>
- Microsoft ACX version information:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-version-overview>
- Microsoft PortCls introduction:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/introduction-to-port-class>
- Microsoft USB Audio 2.0 driver:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/usb-2-0-audio-drivers>
- Microsoft low-latency audio:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/low-latency-audio>
- Microsoft driver package components:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/install/components-of-a-driver-package>
- Microsoft WDK build guidance:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/building-a-driver>
- Microsoft driver signing:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/signing-a-driver>
- Microsoft driver code-signing requirements:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-reqs>
- Microsoft attestation signing:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-attestation>
- Steinberg ASIO driver background:
  <https://helpcenter.steinberg.de/hc/en-us/articles/17863730844946-Steinberg-built-in-ASIO-Driver-information-download>
