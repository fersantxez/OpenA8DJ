# Windows Normal Driver Plan - 2026-06-23

Goal: OpenA8DJ must install and run on normal Windows systems without asking
users to disable Secure Boot, boot into firmware, or enable test-signing.

## Non-Negotiable Product Rule

The public Windows package must be a normal Microsoft-trusted driver package.
Test-signing is only a local engineering tool. It is not a user-facing install
path.

For current Windows kernel drivers, the practical release paths are:

1. Microsoft attestation signing for Windows 10/11 Desktop distribution.
2. WHCP/HLK certification if Windows Update retail distribution or broader
   certification is required.

Official references:

- <https://learn.microsoft.com/en-us/windows-hardware/drivers/install/kernel-mode-code-signing-policy--windows-vista-and-later->
- <https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/>
- <https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-attestation>
- <https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/driver-signing-offerings>

## Current Repo Reality

The current Windows driver is a KMDF USB transport/surface driver:

- INF class: `USBDevice`
- Service: `OpenA8DJUsb`
- It claims `USB\VID_17CC&PID_1978`.
- It exposes diagnostics/control IOCTLs.
- It intentionally reports that Windows audio endpoints and the real
  isochronous engine are not ready yet.

That means there are two separate workstreams:

1. Make the package signable and Microsoft-trusted.
2. Implement the real Windows audio/MIDI driver behavior.

Neither replaces the other.

## Workstream A: Normal Windows Signing

Deliverables:

- A deterministic release package folder:
  - `OpenA8DJUsb.inf`
  - `OpenA8DJUsb.sys`
  - `OpenA8DJUsb.cat`
  - symbols and diagnostics tooling kept separate from the driver submission
    when required.
- A submission CAB suitable for Microsoft Hardware Dev Center.
- Automated checks before submission:
  - build Release x64 and ARM64 where supported;
  - `InfVerif`;
  - `Inf2Cat`;
  - signature verification;
  - package manifest with hashes;
  - no test certificate inside release submission payload.
- GitHub Actions artifact:
  - unsigned/signable submission package;
  - local test-signed engineering package;
  - verification logs.

Owner action eventually required:

- Hardware Dev Center account with an EV certificate associated with it.
- Submit CAB for attestation signing or WHCP as appropriate.
- Download Microsoft-signed package and feed it back into the release pipeline.

## Workstream B: Real Audio Driver

Deliverables:

- Windows audio endpoints visible to normal audio apps.
- 8 output channels and 8 input channels mapped like the macOS surface where
  Windows APIs allow it.
- Real CAIAQ USB isochronous playback and capture:
  - playback endpoint `0x06`;
  - capture endpoint `0x82`;
  - no dynamic allocation after stream start;
  - no hot-path logging;
  - bounded locks and preallocated nonpaged buffers.
- MIDI in/out.
- Control and routing surface that cannot glitch active streaming.
- ASIO layer, if needed, over the same transport engine.

Likely Windows driver model:

- ACX/WaveRT-style audio driver surface for Windows 10/11.
- KMDF USB transport behind it.
- User-mode tools only for configuration and diagnostics, never for the
  realtime audio path.

## Validation Gates

Before any claim that the Windows driver works:

1. Package installs and loads on Secure Boot Windows with Microsoft signature.
2. Device Manager reports OK, no Code 52, no unsigned-driver status.
3. Audio endpoints appear.
4. Playback/capture smoke tests run.
5. iRig physical loopback passes:
   - no clipping;
   - no repeated clicks/dropouts;
   - acceptable SNR/correlation for the analog path;
   - CPU/DPC within limit on the target tablet.
6. Hotplug/remove/reinstall path restores cleanly.
7. Rollback to Native Instruments driver remains available during development.

## Immediate Next Steps

1. Add a release packaging script that creates a Microsoft-submission CAB and
   excludes test certificates.
2. Add a signing-readiness script that runs WDK validation and writes a clear
   pass/fail report.
3. Wire those scripts into the existing GitHub builder.
4. Keep the local install script blocked on Secure Boot for test-signed
   packages, because that protects the current machine from Code 52.
5. Start the audio endpoint work as the next actual driver milestone, not as an
   installer problem.
