# OpenA8DJ Windows Driver

## Stop: Not Available For Use

The Windows driver in this directory is **not available for normal use**.
It is work in progress and has caused local Windows tablet hangs/reboots during
Audio 8 DJ driver testing. Do not install it on a machine you need to keep
stable. Do not use it for DJ playback, Traktor, iRig quality testing,
unattended runs, or production work.

The latest local status supersedes earlier positive hardware notes in this file:
`OpenA8DJUsb` was installed/associated during the v134 session, but the device
later appeared as `CM_PROB_UNSIGNED_DRIVER` and the user observed another
machine hang/reboot around the attempted short-stream canary sequence. Treat
driver loading, streaming, ISO diagnostics, and audio quality as unproven and
unsafe until the crash/reboot path is understood offline.

Read the incident note first:

- [../docs/WINDOWS_DRIVER_INCIDENT_2026-06-25.md](../docs/WINDOWS_DRIVER_INCIDENT_2026-06-25.md)

This directory contains the Windows 10/11 driver workstream for OpenA8DJ.

The macOS Core Audio HAL cannot be repackaged into Windows. Windows needs its
own WDK driver package: a `.sys` driver, INF, catalog, signing flow, and later
an installer/bootstrapper.

## Current State

The current Windows package is an experimental WDK build. It is intended for
testers and developers who understand that kernel drivers can fail, need test
signing, and may require recovery steps.

`OpenA8DJUsb` is a KMDF USB function driver surface for the Native Instruments
Audio 8 DJ hardware ID:

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

The Windows surface is experimental and versioned by
`OPENA8DJ_DRIVER_API_VERSION`. It exposes an honest feature contract for the
current ACX/USB implementation:

- 8 input channels
- 8 output channels
- 4 stereo input pairs: Input A/B/C/D
- 4 stereo output pairs: Output A/B/C/D
- 44.1 and 48 kHz as the first stable Windows rates
- 88.2 and 96 kHz as planned rates only
- 15-4096 frame buffer range
- DVS input modes: Timecode Vinyl, Timecode CD/Line, Phono
- ground-lift flags for vinyl, CD/line, and phono
- software lock state
- surface state for USB transport, controls, audio endpoints, isochronous
  engine, MIDI, and ASIO
- topology descriptors for the planned A/B/C/D render/capture channel layout
- diagnostics counters for ACX stream callbacks, USB packet errors, underruns,
  overruns, late completions, RT packet state, and per-output-pair activity

The package also builds `opena8djctl.exe`, a Windows control tool that can query
the surface, topology, diagnostics, capabilities, controls, profiles, and
experimental format state.

Important: the Windows audio endpoint layer and capture-paced isochronous
engine are now present in this workstream, but this is still not a production
driver. MIDI publication, ASIO, hotplug/sleep validation, long-run DPC/CPU
gates, and full Traktor/timecode input validation remain incomplete.
Feedback and logs are welcome. Install and use experimental Windows builds at
your own risk.

Local 2026-06-25 API 24 note: the current test-signed package is
`OpenA8DJUsb` 0.0.81/API 24. Hardware control profiles now verify readback for
`input-mode` and `software-lock`; ground-lift flags are exposed as hardware
readback and individual `gnd-*` writes must still be treated as
hardware-dependent until independently confirmed. A 30-minute Traktor/iRig run
at `local-analysis/windows/traktor-30min-api24-full-20260625` passed with zero
driver underruns, overruns, packet errors, and late completions. The iRig
capture reported no clipping, no near-clipping, no raw click outliers, and no
PortAudio status events. Audio 8 DJ input endpoints still need known-signal
validation before this can be called DVS/timecode-ready.

API 25 / 0.0.83 source note: the next package adds diagnostics-only stream
worker counters for iteration rate, capture/playback byte totals, active
render/capture masks, no-render iterations, and per-iteration byte maxima. It
builds and verifies locally, but was not installed on the tablet because Secure
Boot is enabled and the package is test-signed; forcing that install would leave
the device rejected by Windows signature policy. Treat API 25 as built but not
locally loaded until test-signed loading is available again or the package is
Microsoft-signed.
Package 0.0.83 no longer publishes Audio 8 DJ capture endpoints under
`KSCATEGORY_REALTIME`: ACX exposes packet/presentation callbacks rather than
the legacy WaveRT hardware position register that PortAudio WDM-KS expects for
capture, so the supported capture host APIs are MME, DirectSound, and WASAPI.

Additional full-round local validation on 2026-06-25 is stored under
`local-analysis/windows-full-round-20260625`. The verified control matrix
passed all profiles, input-mode aliases, software lock, 44.1/48 kHz, and
128/256/512/1024/2048-frame format changes with clean readback and zero driver
stream errors. A standalone `gnd-vinyl off` write failed readback with Windows
error 1117 during the broader exploratory matrix, so standalone `gnd-*` toggles
remain non-passing controls even though profile operations restore a clean
readback state. The same round includes 48 kHz and 44.1 kHz physical output
pair matrices plus a 48 kHz/256-frame all-pairs stress pass through the
mixer/iRig route; all completed without PortAudio status events, clipping, or
driver underrun/overrun/packet/late errors. A 15-minute Traktor/iRig active run
also passed with zero driver errors and no iRig clipping/click/status events;
Traktor used about 16% of one core while total system CPU averaged about 71%
on this old tablet. Newer Traktor smoke artifacts also include
`top-process-samples.json` plus `top_cpu_processes` in `summary.json`, so high
system CPU can be attributed to Traktor, Codex/browser, probes, or other host
processes before changing driver code. The smoke now also writes
`traktor-playback-gesture.json` and refuses to pass unless the automated
Traktor load/play gesture proves render-pair activity before the timed run.

## Standalone Installer

Windows owns a separate installer line. It must not share macOS packaging code
or macOS payloads.

Installer design lives in:

- `windows/installer/README.md`
- `windows/installer/driver-package.manifest.json`
- `windows/installer/tools-package.manifest.json`
- `docs/WINDOWS_STANDALONE_INSTALLER_DESIGN_2026-06-19.md`
- `docs/WINDOWS_TESTER_INSTALL_GUIDE_UNSIGNED_2026-06-19.md`
- `docs/WINDOWS_PERFORMANCE_ROUTING_AND_MACOS_LESSONS_2026-06-19.md`

The installer product is split into:

- `OpenA8DJ Windows Driver`: INF/CAT/SYS, Driver Store install, device binding,
  signing state, and hardware-lock-protected install/uninstall.
- `OpenA8DJ Windows Tools`: `opena8djctl.exe`, future diagnostics, future
  Control Center, Start Menu entries, and user-mode files.

A future bundle EXE may install both, but the driver and tools installers must
remain independently installable and independently uninstallable.

## Shared Hardware Lock

This hardware is shared with the macOS driver and macOS Control Center work.
Before any test that touches the physical Audio 8 DJ, iRig capture route, Core
Audio, USB reset path, playback/capture, MIDI, Traktor, or driver
install/load/unload state, check and acquire the shared lock:

```sh
./scripts/shared-hardware-lock-status
./scripts/shared-hardware-lock-run --gate windows-test -- <command>
```

Release the lock immediately when the hardware action finishes. Builds and
static analysis that do not touch hardware do not need the lock.

## Build Requirements

Build on Windows, not macOS:

- Windows 10 or Windows 11 build host
- Visual Studio 2022
- Windows SDK
- Windows Driver Kit

Microsoft documents WDK setup here:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
- https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/building-a-driver

## Offline Tests On macOS

This does not build or validate the Windows driver. It only checks the portable
Windows surface contract and the offline user-mode audio-engine prototype:

```sh
windows/tests/run_offline_tests.py
```

On Windows, use the PowerShell runner:

```powershell
.\windows\tests\run-offline-tests.ps1
```

Passing this runner means the source contracts are internally consistent. It
does not prove Windows installability, endpoint enumeration, Traktor behavior,
MIDI, USB streaming, CPU/DPC behavior, or sound quality.

## Local Hardware Tests On Windows

After installing the experimental driver on a machine with the Audio 8 DJ and
iRig Stream attached, use the conservative hardware probes under
`windows\tests`. They do not reset USB devices, do not change default audio
devices, record PnP snapshots, and attempt `opena8djctl.exe iso-silence` after
audio playback.

Run the 8-channel physical output matrix through the mixer/iRig route:

```powershell
.\windows\tests\run-a8dj-pair-matrix.ps1 `
  -Seconds 6 `
  -Mode full `
  -InputHostApi MME `
  -OutputHostApi MME `
  -InputName "Line In (iRig Stream)" `
  -OutputName "Speakers (Audio 8 DJ)"
```

Passing this test means the primary 8-channel Windows endpoint can drive
Output A/B/C/D, the external iRig capture path sees signal, PortAudio reported
no stream status events, and the driver diagnostics advanced for the expected
render pairs. It is not a substitute for Traktor deck playback, timecode input,
MIDI, DPC tracing, or long-run listening gates.

Run a stereo quality probe when you need a reference/capture WAV pair:

```powershell
.\windows\tests\run-irig-quality-probe.ps1 `
  -Seconds 20 `
  -Rate 48000 `
  -HostApi MME `
  -InputName "Line In (iRig Stream)" `
  -OutputName "Speakers (Audio 8 DJ)"
```

The soundcheck analyzer is diagnostic. A poor alignment/SNR score can reflect
the analog mixer/iRig route or the selected Windows endpoint, so interpret it
with clipping, dropout, PortAudio status, driver counters, and repeated tone
captures rather than as a single pass/fail verdict.

Run the conservative input endpoint smoke to verify that Audio 8 DJ capture
endpoints open and deliver buffers without claiming signal quality:

```powershell
.\windows\tests\run-a8dj-input-endpoint-smoke.ps1 `
  -Seconds 4 `
  -Rate 48000 `
  -BlockSize 512 `
  -Channels 8
```

Passing this test means the selected host APIs opened the Audio 8 DJ input
endpoints, delivered frames, reported no stream status events, and left driver
USB error counters flat. It does not prove DVS/timecode input quality unless a
known signal crosses the configured `-SignalThreshold`. WDM-KS can be included
with `-IncludeWdmKs` for installed-package diagnostics. API 24 local evidence
shows the four published WDM-KS PortAudio input opens fail with a
capture-position IOCTL error; source package 0.0.83 avoids advertising those
broken capture interfaces.

Run the control readback matrix when changing the hardware control path:

```powershell
.\windows\tests\run-a8dj-control-readback-matrix.ps1
```

This is a control-only probe: it does not start an audio stream, does not reset
USB, logs every requested/readback control byte, and restores
`timecode-vinyl`/48 kHz/512 frames with an `iso-silence` attempt at the end.
On the local API 24 tablet, all requests read byte 3 back as `03`; cases that
request the phono ground bit read back without that bit set. The matrix reports
those as `expected_unsupported_phono_ground_cases` and keeps
`unexpected_mismatches` separate so input-mode, software-lock, and other
control regressions still fail loudly.

Run the Traktor active-audio smoke after configuring Traktor for the Audio 8 DJ
external mixer routing:

```powershell
.\windows\tests\run-traktor-active-smoke.ps1 `
  -Seconds 30 `
  -CaptureIrig
```

By default the script now picks the first MP3 from
`%USERPROFILE%\Downloads\000_santxez_spring_25_select`; pass `-TrackPath` for a
specific MP3 or `-TrackDirectory` for a different music folder. If that folder
is missing, it falls back to the Native Instruments factory track. The script
launches Traktor with the selected track, sends the same conservative play
gestures used during local validation, samples `opena8djctl.exe
diagnostics`, optionally records the iRig return, closes Traktor if it launched
it, and attempts `iso-silence` at the end. If Traktor opens on an empty deck,
the script falls back to the local collection browser, drags an `All Tracks`
item onto Deck A, and presses the Deck A play control. Passing this test means Traktor
held an 8-channel stream open, the load/play gesture produced render-pair
activity, the render-pair nonzero counters advanced during measurement, and the
USB error counters stayed flat during the measurement window. The summary also
records Traktor process CPU, total system CPU, top host processes by CPU,
render/capture frame deltas, and optional iRig capture peak/RMS/clipping/click
metrics. The iRig capture path uses a conservative high-latency input stream,
larger capture blocks, lower-rate CPU polling, and high process priority when
available so capture overload does not masquerade as driver distortion. With
API 25 or newer loaded, it also records worker iteration rate,
capture/playback byte deltas, no-render iterations, and active render/capture
masks so CPU investigations can be tied to driver work rather than only
system-wide CPU. When the external mixer/iRig path clips, pass
`-TraktorTrimWheelNotches N` to trim Traktor channel A gain and master output
before measuring; the selected trim is recorded in `safety.txt` and
`summary.json`. Use a short run for smoke testing and a multi-minute run
before treating a candidate as the current best Windows audio build.

Run the consolidated local validation round when comparing driver candidates:

```powershell
.\windows\tests\run-a8dj-full-validation-round.ps1 `
  -PairSeconds 6 `
  -PairStressSeconds 10 `
  -InputSeconds 4 `
  -TraktorSeconds 900
```

The runner serializes source/package/loaded-driver version preflight, the
conservative hardware smoke, control readback matrix, read-only MIDI endpoint
enumeration, read-only ASIO registry enumeration, 48 kHz and 44.1 kHz output
matrices, 48 kHz/256-frame output stress, normal input endpoint smoke, WDM-KS
input diagnostic, and Traktor/iRig active smoke.
The WDM-KS diagnostic is recorded as an expected nonzero exit on package
versions that still publish capture realtime interfaces. The runner restores
`timecode-vinyl`/48 kHz/512 frames and attempts `iso-silence` at the end.
It also inserts short cooldowns between hardware-heavy phases and records them
in `cooldowns.log`; on this old tablet, back-to-back probes can otherwise push
host CPU high enough for PortAudio to report probe-side overflow/underflow even
when driver transport counters stay clean.
The pair-matrix probe retries any case that reports a PortAudio status event
once after a short silence/cooldown pulse and records `status_retry_attempts`
so transient host scheduling pressure is distinguishable from repeated audio
instability.
Traktor CPU sampling prefers a Windows performance counter instead of WMI and
records `system_cpu_source`; iRig and pair-matrix CPU sampling are throttled so
the validation harness contributes less load while measuring this tablet. The
continuous PnP monitors default to 10-second polling plus before/after
snapshots, which preserves shared-USB evidence without hammering WMI during
playback.

Summarize a full validation artifact directory into a candidate-quality report:

```powershell
.\windows\tests\summarize-a8dj-full-validation.ps1 `
  -RunDir .\local-analysis\windows-a8dj-full-validation-YYYYMMDD-HHMMSS
```

The summary separates hard regressions, CPU/quality warnings, and known
candidate-readiness gaps. It records whether the loaded driver API matches the
source/package API so API 24 hardware results cannot be mistaken for API 25
validation, and includes top-process CPU attribution for Traktor runs. Use
`-StrictCandidateReady` when you want the command to exit nonzero until
known-signal Audio 8 DJ input quality, matching Windows MIDI input/output
endpoints, OpenA8DJ ASIO registration, ground-lift semantics, and production
install/signing gaps are closed.

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

To build the current standalone development installer ZIP:

```powershell
.\windows\scripts\build-installable-package.ps1 -Configuration Release -Platform x64
```

Expected installer ZIP:

```text
windows\dist\installer\OpenA8DJUsb-Release-x64-installer.zip
```

To prepare the Microsoft attestation signing payload:

```powershell
.\windows\scripts\build-driver.ps1 -Configuration Release -Platform x64
.\windows\scripts\test-signing-readiness.ps1 -Configuration Release -Platform x64 -SkipBuild
```

Expected attestation payload:

```text
windows\dist\attestation\OpenA8DJUsb-Release-x64-attestation.cab
windows\dist\attestation\OpenA8DJUsb-Release-x64-attestation-manifest.json
```

The readiness report should say `submission_payload_ready=true`. It should say
`partner_center_ready=false` until the CAB is signed with the organization EV
certificate for Microsoft Partner Center submission.

To sign the CAB before Partner Center submission:

```powershell
.\windows\scripts\sign-attestation-cab.ps1 `
  -Configuration Release `
  -Platform x64 `
  -CertificateSubject "Your EV certificate subject"
```

The script refuses to use local WDK/OpenA8DJ test certificates. After the
EV-signed CAB is submitted through Microsoft Partner Center Hardware Dashboard,
download the Microsoft-signed driver package and repackage that result for
normal Secure Boot Windows systems.

## Test Install

Only use this on a Windows test machine configured for test-signed or unsigned
development drivers:

```powershell
.\windows\scripts\install-driver.ps1 -Configuration Release -Platform x64 -SkipBuild -TrustTestCertificate -EnableTestSigning
```

Secure Boot must be disabled for the current test-signed package. If Secure
Boot is enabled, Windows blocks `bcdedit /set testsigning on`; installing the
package anyway can bind it to the Audio 8 DJ but leave the device failed with
Code 52 / `0xC0000428`. Use a Microsoft-signed driver package for Secure Boot
systems.

Then query the device:

```powershell
windows\dist\Release\x64\opena8djctl.exe status
windows\dist\Release\x64\opena8djctl.exe surface
windows\dist\Release\x64\opena8djctl.exe topology
windows\dist\Release\x64\opena8djctl.exe diagnostics
windows\dist\Release\x64\opena8djctl.exe profile timecode-vinyl
windows\dist\Release\x64\opena8djctl.exe set-format 48000 512
```

Do not use this on a production DJ system yet.

## Next Milestones

1. Extend the Traktor validation matrix beyond active playback smoke into deck
   routing A/B/C/D, timecode vinyl/CD-line inputs, and longer unattended runs.
2. Replace software-only control stubs with proven Audio 8 DJ hardware control
   writes, then validate profile changes while audio is idle and active.
3. Publish and validate MIDI endpoints.
4. Add ASIO or another low-latency application path only after the ACX route is
   stable and measured.
5. Run clean install/uninstall/upgrade, hotplug, sleep/wake, CPU, and DPC gates
   on Windows 10 and Windows 11.
6. Complete Microsoft signing and package a normal public installer only after
   the driver package itself passes the matrix.
