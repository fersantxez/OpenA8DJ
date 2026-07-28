# Windows Notes And Evidence

This page contains internal Windows notes, candidate history, architecture
details, and validation evidence. It is intentionally separate from the
user-facing [Windows installation](WINDOWS.md) page.

Current package files: [EXE](../windows/releases/OpenA8DJUsb-Release-x64-installer.exe),
[ZIP](../windows/releases/OpenA8DJUsb-Release-x64-installer.zip), and
[SHA-256 checksums](../windows/releases/SHA256SUMS.txt).

## Current Status - Experimental Unsigned Driver

The Windows workstream now includes source code, WDK projects, a test-signed
driver package, `opena8djctl.exe`, and development installer ZIP/EXE artifacts
generated locally and by CI. It is still **experimental** and must not be
treated as a production Windows driver.

The Windows driver package is not Microsoft-signed yet. Installing it requires
Administrator rights, Windows test-signing or disabled driver signature
enforcement, and normally Secure Boot disabled. Windows may display an
unsigned-driver prompt; accept it only on a test machine where a kernel driver
crash or reboot is acceptable.

When Secure Boot cannot be disabled, the development package can be tested for
one boot using Windows Startup Settings option 7 (Disable driver signature
enforcement). The installer must then be invoked with
`-AllowStartupSettingsSignatureOverride`; this is an explicit current-session
override and does not enable persistent `testsigning` or modify UEFI settings.

Earlier local testing produced hangs/reboots and BSODs while this driver was
being developed. Those incidents remain relevant safety context. See
[WINDOWS_DRIVER_INCIDENT_2026-06-25.md](WINDOWS_DRIVER_INCIDENT_2026-06-25.md).

The Windows installer and driver are separate from the macOS HAL, DMG, PKG, and
Control Center packaging. Merging Windows code into `dev` must not change the
macOS installer or macOS runtime payloads.

## Current Windows Source

The repository now includes the first Windows WDK workstream in `windows/`:

- `windows/OpenA8DJWindows.sln`
- `windows/OpenA8DJVirtual.sln`
- `windows/driver/OpenA8DJUsb.vcxproj`
- `windows/driver/OpenA8DJUsb.inf`
- `windows/driver/OpenA8DJVirtual.vcxproj`
- `windows/driver/OpenA8DJVirtual.inf`
- `windows/scripts/build-driver.ps1`
- `windows/scripts/build-virtual-acx.ps1`

`OpenA8DJUsb` is a KMDF USB transport/surface driver for
`USB\VID_17CC&PID_1978`. It claims the Audio 8 DJ hardware, maps the known
CAIAQ USB pipes, and exposes the OpenA8DJ experimental Windows surface contract:

- 8 inputs and 8 outputs
- Input A/B/C/D and Output A/B/C/D pair metadata
- 44.1 and 48 kHz as first stable Windows rates
- 88.2 and 96 kHz as planned rates only
- 15-4096 frame buffer range
- timecode-vinyl, timecode-cd-line, and phono profiles
- ground-lift flags and software lock
- explicit surface state for USB, controls, audio endpoints, isochronous
  streaming, MIDI, and ASIO
- topology and diagnostics IOCTLs

The package also builds `opena8djctl.exe` for diagnostics. The source workstream
includes ACX endpoint wiring and a capture-paced isochronous engine for the
physical Audio 8 DJ. The current local artifact has been installed and runtime
validated on the Windows test machine. An isolated `OpenA8DJVirtual` ACX proof target can be
built without USB hardware; it is not part of the USB package and has not been
installed. MIDI publication, ASIO, real hardware control writes, hotplug/sleep
validation, long-run DPC/CPU gates, timecode validation, and a clean Windows
install matrix remain incomplete.

Current candidate (2026-07-25): `OpenA8DJUsb` 0.0.183/API 44 is installed and
active on the Windows test machine. The physical Audio 8 DJ and iRig Stream
were present and healthy during validation; the driver completed Traktor and
physical loopback tests without BSOD, reboot, underrun, overrun, packet-error,
or late-completion deltas. `OpenA8DJVirtual` remains a separate proof target
and does not target the Audio 8 DJ USB VID/PID.

### Local API 24 evidence - 2026-06-25

The local Windows tablet currently has the test-signed `OpenA8DJUsb`
0.0.81/API 24 package installed for the Audio 8 DJ. The API 24 change makes
control writes truthful: profiles update `input-mode` and `software-lock`, and
diagnostics record raw control bytes plus write/readback status. Ground-lift
bits are reported from hardware readback, but local hardware did not confirm
independent ground-lift writes, so profiles preserve the readback ground state
instead of claiming mutually exclusive ground flags.

Current physical evidence:

- `local-analysis/windows/pair-matrix-48k-api24-full-20260625`: 48 kHz full
  output-pair matrix through mixer/iRig; no clipping or PortAudio status
  events; driver underruns, overruns, packet errors, and late completions stayed
  at zero.
- `local-analysis/windows/pair-matrix-44k-api24-full-20260625`: same matrix at
  44.1 kHz with the same zero driver error counters.
- `local-analysis/windows/traktor-30min-api24-full-20260625`: 30-minute Traktor
  factory-track run with iRig capture; `pass=true`, streaming stayed `yes`, 8
  channels were active, all four render pairs advanced, and underrun/overrun,
  packet-error, and late-completion deltas were zero. iRig capture had peak
  0.0539, zero clipped/near-clipped frames, zero raw click outliers, and no
  capture status events.
- `local-analysis/windows/input-endpoint-api24-smoke-threshold-20260625`:
  Audio 8 DJ input endpoint smoke for MME, DirectSound, and WASAPI. All 12
  tested endpoints opened, delivered frames, and reported zero status events;
  driver capture-frame counters advanced by 6,662,839 with zero underruns,
  overruns, packet errors, and late completions. No endpoint crossed the
  0.001 signal threshold, so this proves endpoint stability but not calibrated
  timecode/input signal quality.
- `local-analysis/windows/input-endpoint-api24-wdmks-diagnostic-20260625`:
  WDM-KS diagnostic. MME, DirectSound, and WASAPI still opened; the four
  WDM-KS input endpoints failed in PortAudio with `Failed to read capture
  position register (IOCTL)`.
- `local-analysis/windows-full-round-20260625`: follow-up full round on the
  local tablet. Profiles, input-mode aliases, software lock, 44.1/48 kHz, and
  128/256/512/1024/2048-frame format changes passed clean readback with zero
  driver stream errors. 48 kHz and 44.1 kHz output-pair matrices plus a
  48 kHz/256-frame all-pairs stress pass completed through the mixer/iRig route
  with no PortAudio status events, no clipping, and zero driver underrun,
  overrun, packet-error, or late-completion deltas. A 15-minute Traktor/iRig
  run also passed with streaming `yes`, 8 channels, all four render pairs
  advancing, zero driver error deltas, zero iRig clipped/near-clipped frames,
  zero raw click outliers, and no iRig status events. During the same round,
  standalone `gnd-vinyl off` failed readback with Windows error 1117, so
  standalone `gnd-*` writes remain a known non-passing control variable.
- `local-analysis/windows-ground-readback-matrix-20260625-064106`: control-only
  ground-lift readback matrix. The driver and tool restored a safe baseline
  after every case. The local hardware read byte 3 back as `03` across the
  exercised combinations, so 24/48 requested combinations matched and 24/48
  mismatched; no stream error counters moved.
- `local-analysis/windows-control-classified-20260625-073937`: repeat
  control-only matrix after adding mismatch classification. The 24 mismatches
  are all requests for the phono ground bit, which the local hardware did not
  confirm in readback; `unexpected_mismatches=0` and stream error cases stayed
  at zero.
- `local-analysis/windows-midi-endpoint-smoke-20260625-074506`: read-only
  Windows `winmm` MIDI endpoint enumeration. The host reported zero MIDI input
  devices, one output device (`Microsoft GS Wavetable Synth`), and zero
  Audio 8 DJ/OpenA8DJ matching MIDI inputs or outputs. This turns the MIDI gap
  into measured evidence rather than an untested assumption.
- `local-analysis/windows-full-validation-midi-integration-20260625-074558`:
  shortened validation runner pass proving the MIDI endpoint smoke is integrated
  into the candidate-quality summary. `hard_regression_pass=True`; the MIDI gap
  reports zero matching inputs and zero matching outputs.
- `local-analysis/windows-asio-endpoint-smoke-20260625-075116`: read-only ASIO
  registry enumeration. The host has two `Audio 8 DJ` ASIO registrations
  pointing at the Native Instruments commercial ASIO DLL
  `a8djasio64.dll`, but zero OpenA8DJ ASIO registrations. This does not validate
  an OpenA8DJ ASIO path.
- `local-analysis/windows-full-validation-asio-midi-integration-20260625-075132`:
  shortened validation runner pass proving ASIO and MIDI endpoint evidence are
  both included in the candidate-quality summary. `hard_regression_pass=True`;
  OpenA8DJ ASIO matches remain zero.
- `local-analysis/windows-version-preflight-20260625-075621`: read-only
  source/package/loaded-driver version preflight. Source and packaged INF are
  API 25 / `06/25/2026,0.0.83.0`, while the driver currently loaded on the
  hardware is API 24.
- `local-analysis/windows-full-validation-version-integration-20260625-075637`:
  shortened validation runner pass proving version preflight is integrated into
  candidate-quality summaries. It records `loaded_matches_source_api=False` and
  keeps API 25 behavior as an explicit unvalidated gap until that package is
  actually loaded.
- Traktor active smoke artifacts produced after this point include
  `top-process-samples.json` and `top_cpu_processes` in the Traktor
  `summary.json`. Use those fields to attribute high tablet CPU to Traktor,
  Codex/browser, the probes, or other host processes before changing driver
  timing code. They also include `traktor-playback-gesture.json`; a passing
  smoke must prove the automated load/play gesture produced render-pair
  activity before the timed measurement.

Known remaining caveat: Audio 8 DJ capture endpoints open through WASAPI/MME,
but unattended local capture mostly measured silence or very low-level data
because no known signal is currently cabled into the Audio 8 DJ inputs. WDM-KS
input endpoints failed PortAudio open with a capture-position IOCTL error.
Source package 0.0.83 removes `KSCATEGORY_REALTIME` publication for capture
endpoints instead of advertising a WDM-KS capture path that ACX cannot satisfy
with the legacy hardware position register. Treat physical output and Traktor
playback as strong for this candidate; do not claim complete DVS/timecode input
readiness until all Audio 8 DJ inputs are validated with a known signal.

API 25 / 0.0.83 build note: a follow-up instrumentation build adds stream
worker counters for iterations, capture/playback bytes, active render/capture
masks, no-render iterations, and max per-iteration byte counts. The package
keeps the audio data path unchanged and no longer publishes broken WDM-KS
capture realtime interfaces. Local installation was intentionally not forced
because Secure Boot is enabled and the install script refused to bind a
test-signed kernel driver that Windows would reject with Code 52 / 0xC0000428.
The currently loaded local driver therefore remains API 24 until the machine is
booted/configured for test-signed driver loading or a Microsoft-signed package
is available.

Bugs are expected and tester feedback is welcome. Do not describe this branch
as a fully functional public Windows driver until the validation matrix below is
closed with current evidence.

## Target

The first Windows target should be:

- Windows 10 22H2 x64
- Windows 11 23H2/24H2 x64
- Audio 8 DJ USB VID/PID `17cc:1978`
- 8 input channels and 8 output channels
- 44.1 and 48 kHz first
- Buffer sizes suitable for Traktor low-latency use
- MIDI in/out
- Timecode vinyl and CD/line input modes

Arm64 and 88.2/96 kHz should stay out of the first Windows milestone until the
x64 44.1/48 kHz driver is stable.

## Required Deliverables

A useful Windows release must provide:

- A Windows audio function driver that exposes normal Windows audio endpoints.
- USB transport for the Audio 8 DJ CAIAQ protocol.
- MIDI endpoints.
- Device controls equivalent to `opena8dj-control`.
- An INF file matching USB VID/PID `17cc:1978`.
- Driver binary files, PDB symbols for debugging, a catalog file, and a signing
  flow.
- A test-signed driver package for development machines.
- A production-signed driver package for public release.
- An MSI or bootstrap installer only after the driver package itself installs
  and exposes the device correctly.

The Windows implementation must follow the same provenance rules as the macOS
implementation: no Native Instruments binaries, firmware, installers, logos, or
proprietary payloads, and no copied third-party implementation code under
incompatible license terms. Experimental Windows builds must also include
`BRAND_POLICY.md` and clearly state that they are not official Native
Instruments software.

## Driver Architecture

The deeper Windows/Linux real-time audio research track lives in
`docs/REALTIME_AUDIO_DRIVER_RESEARCH_WINDOWS_LINUX_2026-06-19.md`. Use it as
the starting point for ACX/WaveRT/ASIO decisions, Linux `snd-usb-caiaq`
comparisons, and real-time validation rules.

The current Windows implementation branch plan lives in
`docs/WINDOWS_IMPLEMENTATION_PLAN_2026-06-19.md`.

The standalone Windows installer design lives in
`docs/WINDOWS_STANDALONE_INSTALLER_DESIGN_2026-06-19.md` and
`windows/installer/`.

Controlled tester install instructions for unsigned/test-signed packages live
in `docs/WINDOWS_TESTER_INSTALL_GUIDE_UNSIGNED_2026-06-19.md`.

The Windows performance/routing plan that carries macOS/C++ lessons into the
Windows branch lives in
`docs/WINDOWS_PERFORMANCE_ROUTING_AND_MACOS_LESSONS_2026-06-19.md`.

Windows and Linux candidate merge policy lives in
`docs/EXPERIMENTAL_WINDOWS_LINUX_MERGE_POLICY_2026-06-19.md`.

Session handoff for recovering this Windows/Linux work lives in
`docs/SESSION_HANDOFF_WINDOWS_LINUX_2026-06-19.md`.

The macOS HAL code is useful as a protocol reference for our own implementation,
but Windows needs a native WDK driver.

Recommended architecture for the experimental Windows build:

1. Build and load the `OpenA8DJUsb` WDK package that binds to
   `USB\VID_17CC&PID_1978`.
2. Implement the CAIAQ USB control and isochronous transport in a WDF-based USB
   component.
3. Continue the current ACX audio endpoint implementation for the vendor-specific
   USB device while keeping the surface contract explicit about unsupported
   features.
4. Keep PortCls/WaveRT and ASIO as future compatibility/performance tracks after
   the ACX/USB streaming path has stronger stability evidence.
5. Keep the user-facing control panel/helper out of the streaming path. It can
   be a normal Win32 service or app once the driver exposes a stable control
   interface.

The driver model decision is the first serious Windows technical risk. The
project should not promise an MSI until this decision is proven on a real
Windows 10/11 test machine.

## Implementation Phases

### Phase 0: hardware and protocol confirmation

- Capture USB descriptors and endpoint behavior on Windows.
- Confirm that Windows does not bind the device to a usable in-box USB Audio
  Class driver.
- Reproduce the macOS CAIAQ control commands with a private test tool.
- Confirm input modes, ground-lift flags, MIDI endpoint behavior, and sample
  rate commands.

### Phase 1: WDK skeleton

- Create a Visual Studio/WDK solution.
- Add an INF for `USB\VID_17CC&PID_1978`.
- Add driver package metadata and service installation sections.
- Validate with `InfVerif`.
- Generate a catalog with `Inf2Cat`.
- Install on a test machine with `pnputil`.

### Phase 2: USB transport

- Implement EP1 CAIAQ command read/write.
- Implement isochronous capture endpoint `0x82`.
- Implement isochronous playback endpoint `0x06`.
- Use preallocated buffers and explicit underrun/overrun counters.
- Add deterministic playback/capture tests before connecting to the audio
  engine.

### Phase 3: audio endpoints

- Expose the primary 8-channel playback endpoint and stereo A/B/C/D endpoints
  that Traktor and Windows audio clients can open.
- Expose 4 stereo capture endpoints for Input A/B/C/D.
- Support 44.1 and 48 kHz first.
- Implement buffer-size negotiation without invalid sentinel values.
- Validate Spotify, Windows system audio, Traktor, and a deterministic local
  test player.

### Phase 4: MIDI and controls

- Expose MIDI In and MIDI Out.
- Add control APIs for `input-mode`, `gnd-vinyl`, `gnd-cd-line`, `gnd-phono`,
  and `software-lock`.
- Add a small control app/service after the driver control path is stable.
- Validate Traktor timecode vinyl and CD/line modes.

### Phase 5: signing and installer

- Test-sign the driver package for internal testing only.
- Run Windows HLK or attestation workflow depending on the release strategy.
- Submit the package through Microsoft Partner Center for production signing.
- Build the guarded development EXE/ZIP installer; use a signed production
  bootstrapper only after the driver package is Microsoft-signed.
- Build a WiX MSI for user-mode tools only; do not use MSI custom actions to
  pretend that copying files installs the kernel driver.
- Publish Windows artifacts only after a clean install, reboot, hotplug, audio,
  MIDI, and Traktor matrix has passed.

## Validation Matrix

- Windows 10 22H2 x64 clean install.
- Windows 11 current x64 clean install.
- Install, uninstall, reinstall, and upgrade.
- Hotplug during idle and playback.
- Sleep/wake recovery.
- 44.1 and 48 kHz playback.
- All 8 outputs with channel isolation.
- All 8 inputs with known signal.
- Traktor deck routing A/B/C/D.
- Traktor timecode vinyl and CD/line.
- MIDI in/out loopback.
- Sustained 30-minute playback and capture.
- CPU and DPC latency under Traktor load.

Local development evidence should be recorded under `local-analysis\windows*`
with:

- `windows\tests\run-a8dj-pair-matrix.ps1` for deterministic 8-channel physical
  output checks through the iRig return path.
- `windows\tests\run-irig-quality-probe.ps1` for a stereo reference/capture WAV
  quality probe.
- `windows\tests\run-traktor-active-smoke.ps1 -CaptureIrig` for Traktor-facing
  active playback, driver diagnostics, CPU/process attribution samples, and
  iRig capture metrics.

The consolidated full runner records short hardware cooldowns in
`cooldowns.log`. On the local tablet, isolated retests showed clean 48 kHz/512
and 48 kHz/256 matrix behavior after host CPU cooled down, so back-to-back
probe status events should be interpreted alongside driver counters and CPU
evidence. The pair-matrix probe now retries a case once after a PortAudio
status event and records `status_retry_attempts`; repeated status events remain
failures.

These probes deliberately avoid USB reset, Windows Audio restart, default-device
changes, and unattended recovery of a missing iRig. A pass is strong local
evidence for the tested route, not proof that MIDI, ASIO, timecode, hotplug,
sleep/wake, or a clean-install Windows release is complete.

## Standalone Installer Direction

The Windows release will use a separate installer line:

- driver installer for the Microsoft-signed INF/CAT/SYS package;
- tools installer for user-mode utilities and future control UI;
- optional bundle EXE that chains both without merging ownership.

The current development installer is a ZIP generated by
`windows\scripts\build-installable-package.ps1` plus a self-extracting EXE
generated by `windows\scripts\build-exe-installer.ps1`. Both use the same
guarded install flow and are experimental, unsigned artifacts. The public
installer must wait until the driver package is Microsoft-signed and clean
install/uninstall/upgrade passes on real Windows test machines.

## Why There Is No Public MSI Yet

An MSI without a real Windows driver package would not make the Audio 8 DJ
available to Windows audio applications. It would only install files. The
development EXE is therefore a bootstrapper for the real INF/CAT/SYS package,
not a claim that the driver is already an official Windows release.

For that reason, this repository intentionally does not build or publish a
placeholder Windows MSI. A future MSI is reserved for user-mode tools, while a
future production EXE will install the Microsoft-signed driver package.

## What Makes the Driver Official

The local test certificate and the development EXE are not enough for a normal
Secure Boot installation. The release path is:

1. Enroll the publisher in Microsoft's Windows Hardware Developer Program and
   Partner Center hardware dashboard.
2. Build a reproducible package containing the INF, CAT, SYS and exact hashes;
   run the matching Windows HLK for every supported Windows version and hardware
   configuration.
3. For controlled testing, submit an EV-signed CAB through Microsoft's
   attestation flow. Attestation is useful for testing but is not the same as a
   WHQL/Windows Update release.
4. For a public certified release, pass the applicable HLK/WHCP tests and submit
   the results through Partner Center so Microsoft returns the signed package.
5. Sign the EXE/MSI bootstrapper separately with a trusted Authenticode code-
   signing certificate, timestamp it, and distribute immutable versioned files.
6. Validate clean installation with Secure Boot and normal signature enforcement
   enabled; no test-signing mode, imported test root, or unsigned-driver prompt
   should be required.

## Microsoft References

Microsoft documents a Windows driver package as including an INF, catalog file,
driver files, and any other required files:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/install/components-of-a-driver-package

Windows audio drivers commonly use the kernel streaming/audio driver stack; the
PortCls documentation is the starting point for understanding the standard audio
miniport model and its limits:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/introduction-to-port-class

WDF is the normal framework family for reducing boilerplate in Windows drivers:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/

Development builds need test signing, and public Windows 10/11 kernel-mode
drivers need Microsoft signing/Partner Center flow:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/install/how-to-test-sign-a-driver-package
- https://learn.microsoft.com/windows-hardware/drivers/install/driver-signing
- https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-attestation
- https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/inf2cat

## Experimental Release Policy

Current OpenA8DJ artifacts:

- macOS DMG
- macOS PKG
- SHA-256 checksums
- experimental Windows WDK driver artifacts from CI

Windows artifacts may be shared only as experimental/test-signed builds until
physical Windows validation is complete.

Do not publish Windows binaries as production-ready until the legal/provenance
gate in `docs/LEGAL.md`, physical Windows validation, and the driver signing
requirements above are satisfied.
