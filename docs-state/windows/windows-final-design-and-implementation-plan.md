# Windows Final Design And Implementation Plan

Date: 2026-06-22
Branch: `windows/rebuild-surface`
Status: final design for implementation, Windows remains experimental
Hardware use in this pass: none

Implementation progress is tracked in
`docs-state/windows/windows-implementation-progress.md`.

## Final Decision

The Windows line is finalized as an ACX-first, KMDF/WDF USB, evidence-gated
driver stack. The design is no longer open between broad alternatives. The only
major technical branch left is a bounded fallback: if the ACX proof-of-fit fails
for this vendor-specific USB device, move the endpoint layer to AVStream/KS
while keeping the same OpenA8DJ audio engine, USB transport, validation gates,
installer split, and ASIO facade rules.

Final stack:

```text
Traktor / DAWs / Windows audio apps
    |
    +-- ASIO facade, only after shared engine validation
    |
    +-- WASAPI shared/exclusive through Windows audio endpoints
    |
ACX 1.1 endpoint driver, KMDF
    |
OpenA8DJ Windows audio engine
    - endpoint topology model
    - render/capture rings
    - USB packet scheduler
    - active format and rate policy
    - CAIAQ 24-bit sample packing/unpacking
    - clock/cadence accounting
    - MIDI/control serialization
    - diagnostics snapshots
    |
WDF USB CAIAQ transport
    - EP1 control/MIDI command channel as protocol requires
    - isochronous capture endpoint 0x82
    - isochronous playback endpoint 0x06
    |
Native Instruments Audio 8 DJ
```

This design is final enough to implement. It is not final enough to claim
Windows support, because support claims come only from the validation ladder.

## Design Locks

These decisions are closed unless measured Windows evidence proves them wrong:

1. Windows is a separate experimental product line.

   No Windows changes go into the macOS public release line. Windows code,
   installers, artifacts, and validation live under Windows-owned paths.

2. The production audio endpoint layer is ACX 1.1 first.

   ACX is KMDF-based, exposes audio concepts as WDF objects, supports
   WaveRT-style streaming, and is recommended by Microsoft for new driver
   development. The target OS floor for the audio endpoint driver is Windows 10
   version 2004 or newer, with Windows 10 22H2 x64 and current Windows 11 x64
   as the first validation targets.

3. AVStream/KS is the endpoint fallback.

   AVStream/KS is only activated if ACX cannot compose with the
   vendor-specific USB transport after the proof-of-fit tasks below. The
   fallback must reuse the same audio engine and USB transport rather than
   becoming a parallel product.

4. PortCls is not the primary path.

   PortCls/WaveRT concepts still matter for latency, position reporting, and
   glitch accounting, but direct PortCls is not the first implementation target
   for this external vendor-specific USB device.

5. The Windows in-box USB Audio 2.0 driver is not the solution.

   Audio 8 DJ is being treated as vendor-specific CAIAQ hardware. The Windows
   class driver path is not a substitute for the CAIAQ transport, packet
   scheduler, hardware control, MIDI, and validation work.

6. ASIO is required for the pro/DJ product, but it comes after the shared engine.

   ASIO must be a facade over the same engine used by ACX/WASAPI. It must not
   be a second streaming implementation. Steinberg SDK or licensing material
   must not be copied into this repo without explicit clearance.

7. The streaming engine is capture-paced first.

   The first hardware streaming model submits capture continuously and shapes
   playback from capture completions. Do not start with an arbitrary software
   timer for OUT. This follows the strongest timing clue from CAIAQ-style
   behavior and the macOS evidence that packet cadence affects real sound.

8. 44.1 kHz and 48 kHz are the only first-class rates until validated.

   88.2/96 kHz remain hidden, rejected, or explicitly marked planned/diagnostic
   until they pass the same physical matrix. The current source should be fixed
   because `set-format` accepts planned rates too normally.

9. Initial buffer target is 48 kHz / 512 frames.

   256 frames may be attempted after 512 passes. 128 frames is later. 64/32
   frames are not product goals until physical capture, DPC/ISR, CPU, and
   Traktor evidence justify them.

10. Endpoint shape is dual-prototype until app evidence decides.

   Implement the engine to support both one 8-channel render/capture surface
   and four stereo A/B/C/D endpoint semantics. The first user-facing default can
   be conservative Output A/B. The final endpoint presentation is selected only
   after Traktor, DAW, WASAPI shared, and WASAPI exclusive tests.

11. Control and MIDI are separated from the audio hot path.

   Control writes must be serialized through bounded transitions. MIDI must not
   take stream locks or execute variable-time work in the audio completion path.

12. Installers are split.

   The driver installer owns INF/CAT/SYS, Driver Store state, device binding,
   signing, rebind, rollback, and hardware-lock discipline. The tools installer
   owns user-mode tools, future Control Center, diagnostics, Start Menu entries,
   and logs. A bundle EXE is allowed only as orchestration.

13. Public Windows release requires Microsoft signing.

   Internal development can use test signing and must say so. Public Windows
   distribution requires Microsoft-signed driver catalog through attestation or
   HLK/WHQL, signed installer artifacts, checksums, release notes, and validation
   reports.

14. No build handoff without sound validation.

   A normal tester candidate is blocked until the exact artifact passes real
   Windows install, audio, MIDI, Traktor, CPU/DPC, physical capture, and human
   listening gates. Otherwise label it `diagnostic only, sound quality not
   validated`.

## Final Component Model

### `OpenA8DJUsb` KMDF Transport

Responsibilities:

- Own binding to `USB\VID_17CC&PID_1978`.
- Select USB configuration/interface.
- Map endpoints `0x01`, `0x81`, `0x82`, and `0x06`.
- Own WDF USB target lifetime and PnP/power transitions.
- Expose a stable internal transport API to the audio engine.
- Keep user IOCTL diagnostics truthful.

Non-responsibilities:

- It does not claim audio endpoint readiness before ACX/KS exists.
- It does not publish a Windows audio endpoint by itself.
- It does not perform UI, logging, or expensive diagnostic formatting in the
  packet completion path.

### OpenA8DJ Windows Audio Engine

This is the shared engine below ACX/KS and ASIO. It should be testable with
offline deterministic sources and sinks before physical hardware is used.

Responsibilities:

- Active sample rate, buffer frame, channel count, and endpoint layout.
- Render and capture SPSC-style rings.
- CAIAQ packet framing and 24-bit sample conversion.
- Render underrun policy: deterministic silence, counted.
- Capture overrun policy: counted drop/overwrite behavior, never hidden.
- USB packet scheduler and packet status accounting.
- Stable host-facing frame position and QPC/cadence accounting.
- Diagnostic snapshot struct with fixed-size counters.

Hot-path rules:

- No heap allocation after stream start.
- No pageable code or data in USB/audio completion paths.
- No string formatting, file I/O, registry I/O, UI callback, or dynamic logging.
- No broad locks shared by render, capture, MIDI, and control.
- No unbounded scans or variable-time per-packet behavior.

### ACX Endpoint Driver

Responsibilities:

- Publish Windows render/capture endpoints.
- Declare 44.1/48 kHz and channel topology honestly.
- Implement stream create/run/pause/stop transitions.
- Bridge ACX RT packets to the OpenA8DJ engine.
- Report stable packet/frame position and glitch counters.
- Integrate PnP/power state with the USB transport state.

Proof-of-fit requirements:

- A virtual deterministic endpoint can enumerate and play/capture without Audio
  8 DJ attached.
- A hardware-backed endpoint can stream without fake readiness once M2 exists.
- ACX can either share the same WDF stack or use an ACX multi-stack shape
  cleanly enough to keep ownership and synchronization understandable.

### ASIO Facade

Responsibilities:

- Expose ASIO channel names A/B/C/D.
- Use the same render/capture engine and active format model.
- Provide coherent buffer-size negotiation and latency reporting.
- Support Traktor and DAW workflows without becoming the only working path.

Rules:

- Do not import or commit Steinberg SDK material until licensing is explicit.
- Do not ship ASIO before WASAPI/ACX validation is real.
- Compare ASIO and WASAPI exclusive with the same physical audio gates.

### MIDI And Control

Responsibilities:

- Expose MIDI In/Out to Windows apps.
- Implement Audio 8 DJ profile state: DVS Vinyl, DVS CD/Line, phono/recording,
  playback/output-only, MIDI-only, diagnostics.
- Serialize control writes with stream state.
- Reject or defer unsafe control changes during active streaming.
- Provide support export data without polling at audio rate.

### Installer And Tools

Driver package:

- `OpenA8DJUsb.inf`
- `OpenA8DJUsb.sys`
- `OpenA8DJUsb.cat`
- PDBs for internal/Partner Center automation as required
- signing manifest
- hashes
- install/uninstall/verify reports

Tools package:

- `opena8djctl.exe`
- future Control Center
- future diagnostics collector
- Start Menu shortcuts
- support export path

Bundle:

- Optional signed EXE that chains driver installer and tools MSI.
- It must never merge driver and tools ownership.

## Implementation Plan

Each milestone must leave a commit, evidence, and a decision note. Hardware
steps require a hardware-use log entry before use and the shared hardware lock
for the full duration.

### M0: Normalize Baseline And Build Skeleton

Goal: make the Windows branch clean, reproducible, and ready for Windows-host
implementation.

Tasks:

1. Inventory current dirty tree.
2. Decide which untracked Windows docs/scripts are part of the branch baseline.
3. Commit Windows-only docs, scripts, manifests, and installer scaffolding.
4. Move or leave unrelated Linux agent artifacts out of the Windows commit.
5. Add `docs-state/windows/README.md` that routes state docs.
6. Update `docs/WINDOWS.md` to point to the final design without implying
   release readiness.
7. Verify `.gitignore` keeps `windows/dist/`, `windows/obj/`, generated ZIPs,
   symbols unless intentionally staged, and local analysis out of git.
8. On Windows, install Visual Studio 2022, Windows SDK, WDK, and ADK if needed
   for attestation packaging.
9. Run the build script for x64 Release.
10. Run `InfVerif`, `Inf2Cat`, and hash collection explicitly.

Deliverables:

- Clean Windows baseline commit.
- Build host manifest with OS version, VS version, SDK/WDK version.
- `local-analysis/windows/m0-build-<timestamp>/build-report.json`.
- Unsigned/test-signed build artifact hashes.

Exit gate:

- Windows x64 Release driver and tool build.
- INF and catalog generation pass.
- No macOS files modified as part of Windows normalization.

### M1: Truthful Surface, Control, And Diagnostic USB

Goal: prove the driver can bind to the device and perform diagnostic control
traffic without claiming audio readiness.

Tasks:

1. Add a repository hardware-use log template under `docs-state/windows/`.
2. Before physical install/load, write the first hardware-use log entry.
3. Acquire the shared hardware lock for install/load/open actions.
4. Install the test-signed driver on a Windows test machine.
5. Validate Driver Store, Device Manager, and hardware ID binding.
6. Run `opena8djctl status`, `surface`, `topology`, and `diagnostics`.
7. Implement EP1 command read/write helpers with strict timeouts.
8. Convert current local-only control state to truthful states:
   - `controls-contract-ready`
   - `hardware-control-write-ready`
   - `hardware-control-readback-ready`
9. Change `set-format` so 88.2/96 kHz are rejected or flagged planned until
   validation.
10. Keep `start` rejected until M2.
11. Extend `verify-driver.ps1` report with surface/topology/diagnostics output
   and driver version/source commit.

Deliverables:

- M1 source commit.
- Install log and verify JSON.
- EP1 diagnostic transcript.
- Updated capability/surface truth table.

Exit gate:

- Device binds on Windows.
- `start` remains honestly unsupported.
- Control state no longer implies hardware readiness without hardware evidence.
- Lock release is recorded after the run.

### M2: Offline Engine And ACX Proof-Of-Fit

Goal: publish and exercise deterministic Windows audio endpoints before real
USB audio streaming is attached.

Tasks:

1. Add an `windows/audio/` or equivalent project for the OpenA8DJ engine.
2. Add offline engine tests for:
   - render ring write/read
   - capture ring write/read
   - underrun and overrun counters
   - 24-bit sample pack/unpack
   - A/B/C/D channel ordering
   - 44.1/48 kHz format acceptance
   - 88.2/96 kHz rejection/planned handling
3. Add minimal ACX endpoint project or subproject.
4. Publish deterministic render/capture endpoints without Audio 8 DJ attached.
5. Implement silence/tone source and memory sink modes.
6. Validate Windows Sound enumeration.
7. Validate WASAPI shared playback tone.
8. Validate WASAPI exclusive playback tone.
9. Collect ETW/WPA trace for endpoint skeleton.
10. Decide single-stack ACX or multi-stack ACX based on actual project shape.

Deliverables:

- Engine project commit.
- ACX endpoint skeleton commit.
- Offline test logs.
- Endpoint enumeration screenshots or text export.
- ETW trace summary.
- ACX proof-of-fit decision note.

Exit gate:

- Windows sees real OpenA8DJ test endpoints.
- Deterministic audio streams through the endpoint skeleton.
- No physical Audio 8 DJ streaming claim yet.

Fallback trigger:

- If ACX cannot publish a stable endpoint or cannot compose with the transport
  model after bounded proof-of-fit, write an AVStream/KS fallback decision and
  start the equivalent endpoint skeleton over the same engine.

### M3: CAIAQ Isochronous Hardware Engine

Goal: stream deterministic audio through the physical Audio 8 DJ with stable
buffers and truthful diagnostics.

Tasks:

1. Add stream lifecycle states:
   - stopped
   - prepared
   - starting
   - running
   - draining
   - stopping
   - faulted
2. Preallocate IN request pool.
3. Preallocate OUT request pool.
4. Preallocate packet descriptor storage.
5. Preallocate nonpaged render/capture buffers.
6. Implement capture endpoint `0x82` continuous submission.
7. Implement playback endpoint `0x06` shaped from capture completions.
8. Implement packet status validation.
9. Implement output silence fill on underrun with counter.
10. Implement capture drop/overrun counter.
11. Implement queue depth min/max/avg counters.
12. Implement late completion counters.
13. Start with 48 kHz / 512 frames / Output A/B only.
14. Capture Output A/B 1 kHz through independent interface.
15. Repeat with 5, 10, and 30 minute runs before advancing.

Deliverables:

- Isochronous engine commit.
- `opena8djctl diagnostics` with real packet counters.
- Physical tone capture files.
- Analysis report for frequency, level, clipping, dropouts, sidebands, and
  click outliers.
- ETW/WPA CPU/DPC summary.

Exit gate:

- Physical Output A/B tone passes.
- No fake streaming state.
- No unbounded CPU/DPC behavior in the tested run.
- Hardware lock released and logged.

### M4: Full Duplex, Full Topology, MIDI, And Controls

Goal: move from Output A/B proof to complete Audio 8 DJ function.

Tasks:

1. Enable all render pairs A/B/C/D.
2. Enable capture pairs A/B/C/D.
3. Validate channel ordering with matrix tones.
4. Validate no cross-talk between pairs.
5. Add full-duplex Output A/B + Input A/B.
6. Expand to full-duplex A/B/C/D.
7. Add MIDI endpoint implementation.
8. Run MIDI loopback and long-run byte-loss test.
9. Implement hardware profile commands for:
   - DVS Vinyl
   - DVS CD/Line
   - phono/recording
   - output-only/playback
   - MIDI-only
   - diagnostics
10. Serialize control changes with stream lifecycle.
11. Reject unsafe live profile changes or defer them to stop/pause boundary.
12. Add support export command to `opena8djctl`.

Deliverables:

- Full topology commit.
- MIDI/control commit.
- Channel matrix evidence.
- MIDI loopback evidence.
- Profile application evidence.

Exit gate:

- 8 render and 8 capture channels validated.
- MIDI visible and stable.
- Control profiles affect hardware and are logged truthfully.
- Full-duplex does not degrade output quality.

### M5: Traktor, ASIO, And Pro Audio Surface

Goal: make the Windows line usable by real DJ/DAW software.

Tasks:

1. Run Traktor with WASAPI/Windows endpoint path if available.
2. Decide endpoint presentation default:
   - single 8-channel endpoint
   - four stereo endpoints
   - dual endpoint model
3. Record user-facing channel names and mapping.
4. Add ASIO facade over the shared engine.
5. Keep ASIO SDK/license material outside the repo until cleared.
6. Implement ASIO channel names A/B/C/D.
7. Implement ASIO buffer-size negotiation tied to engine policy.
8. Implement ASIO latency reporting from measured engine/USB latency.
9. Validate Traktor device selection.
10. Validate DVS Vinyl timecode scope.
11. Validate DVS CD/Line timecode scope.
12. Validate DAW playback/capture.
13. Compare ASIO and WASAPI exclusive on the same physical gates.

Deliverables:

- Endpoint model decision note.
- ASIO facade commit.
- Traktor routing evidence.
- DVS scope evidence.
- DAW evidence.
- Latency report.

Exit gate:

- Traktor can select and use OpenA8DJ.
- DVS Vinyl and CD/Line behave normally.
- ASIO improves pro workflows without becoming the only valid path.

### M6: Installer, Control Center, And Diagnostics UX

Goal: create an installable Windows experience that a non-technical tester can
use without guessing system state.

Tasks:

1. Keep development ZIP for internal/test-signed diagnostics.
2. Create signed/elevated driver installer EXE project.
3. Create WiX tools MSI.
4. Add optional bundle EXE after driver and tools installers work separately.
5. Add Start Menu entries.
6. Add future Control Center app skeleton.
7. Implement Control Center profiles:
   - DVS Vinyl
   - DVS CD/Line
   - recording/phono
   - playback/output-only
   - MIDI-only
   - diagnostics/support export
8. Add support export with:
   - driver package version
   - source commit
   - file hashes
   - signature state
   - Driver Store state
   - device binding state
   - endpoint state
   - stream counters
   - CPU/DPC summary when available
   - recent Windows event logs
   - limitation label
9. Validate install/uninstall/reinstall/upgrade.
10. Validate reboot, hotplug, sleep/wake.

Deliverables:

- Driver installer commit.
- Tools MSI commit.
- Bundle commit if justified.
- Control Center commit.
- Installer validation reports.

Exit gate:

- Clean Windows 10 22H2 x64 install/uninstall/reinstall passes.
- Clean current Windows 11 x64 install/uninstall/reinstall passes.
- Hardware lock is respected and released after success/failure.

### M7: Validation Ladder And Experimental Release Candidate

Goal: qualify an experimental Windows candidate with evidence equivalent in
discipline to macOS 0.5.0.

Tasks:

1. Build exact release candidate from clean source commit.
2. Produce hashes for every artifact.
3. Run build/package gate.
4. Run install/uninstall/reinstall gate.
5. Run reboot/hotplug/sleep-wake gate.
6. Run endpoint visibility gate.
7. Run 44.1/48 kHz playback/capture gates.
8. Run A/B/C/D channel isolation gates.
9. Run MIDI loopback gate.
10. Run Traktor DVS Vinyl gate.
11. Run Traktor DVS CD/Line gate.
12. Run CPU/DPC/ETW stress gate.
13. Run physical tone/click capture gate.
14. Run physical real-music capture gate.
15. Run human listening only after numeric gates pass.
16. Create release notes with exact limitations.
17. If public, submit for Microsoft signing and re-run install validation on the
    Microsoft-signed result.

Deliverables:

- `docs-state/windows/release-candidate-<version>.md`
- validation evidence directory
- checksums
- signed artifacts when eligible
- release notes

Exit gate:

- Exact artifact passes all required gates.
- Windows remains labeled experimental unless public support criteria are
  explicitly met.

## Work Breakdown By Owner Area

Driver core:

- Transport binding and PnP/power.
- Engine rings and packet scheduler.
- ACX/KS endpoint bridge.
- MIDI/control serialization.

Tools and diagnostics:

- `opena8djctl` truth commands.
- Support export.
- Verification JSON.
- ETW/WPA collection wrappers.

Installer:

- Development ZIP hardening.
- Driver installer EXE.
- Tools MSI.
- Bundle EXE only after separate installers pass.

Validation:

- Windows host setup.
- Audio capture route.
- Traktor/DAW matrix.
- CPU/DPC stress.
- Release evidence assembly.

## Immediate Engineering Tasks

These are the first concrete tasks to execute from the current repo state:

1. Commit the existing untracked Windows installer/scripts/docs baseline or
   explicitly remove non-Windows artifacts from this worktree.
2. Change planned sample-rate handling so 88.2/96 kHz cannot be mistaken for
   supported Windows runtime rates.
3. Split control state into local contract state and hardware-proven state.
4. Add M0 Windows build-report schema.
5. Run M0 on a Windows WDK machine.
6. Start the ACX endpoint skeleton only after M0 evidence exists.

## References Checked On 2026-06-22

- Microsoft ACX overview:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-audio-class-extensions-overview>
- Microsoft ACX version information:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-version-overview>
- Microsoft ACX streaming:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-streaming>
- Microsoft ACX multi-stack:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/acx-multi-stack>
- Microsoft PortCls introduction:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/introduction-to-port-class>
- Microsoft USB Audio 2.0 driver:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/usb-2-0-audio-drivers>
- Microsoft attestation signing:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-attestation>
- Steinberg ASIO background:
  <https://helpcenter.steinberg.de/hc/en-us/articles/17863730844946-Steinberg-built-in-ASIO-Driver-information-download>
