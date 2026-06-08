# Windows 10/11 Support Plan

OpenA8DJ 0.2.4 does not include a production Windows audio driver or Windows
installer. The Windows workstream is experimental.

The current implementation is macOS-specific:

- Core Audio HAL bundle
- IOUSBHost USB transport
- CoreMIDI bridge
- macOS LaunchAgent
- macOS PKG/DMG packaging

None of those components can be repackaged into a functional Windows MSI. A
Windows release needs a separate driver implementation and a real Plug and Play
driver package.

## Current Windows Source

The repository now includes the first Windows WDK workstream in `windows/`:

- `windows/OpenA8DJWindows.sln`
- `windows/driver/OpenA8DJUsb.vcxproj`
- `windows/driver/OpenA8DJUsb.inf`
- `windows/scripts/build-driver.ps1`

`OpenA8DJUsb` is a KMDF USB transport driver for `USB\VID_17CC&PID_1978`. It
claims the Audio 8 DJ hardware, maps the known CAIAQ USB pipes, and exposes the
OpenA8DJ experimental feature contract:

- 8 inputs and 8 outputs
- Input A/B/C/D and Output A/B/C/D pair metadata
- 44.1, 48, 88.2, and 96 kHz capability metadata
- 15-4096 frame buffer range
- timecode-vinyl, timecode-cd-line, and phono profiles
- ground-lift flags and software lock

The package also builds `opena8djctl.exe` for testers. Windows audio endpoint
publication, MIDI publication, and sustained isochronous streaming still need
real Windows 10/11 validation with the physical hardware.

This is intentionally labeled experimental: it has been built and packaged, but
not exhaustively tested in Windows. Bugs are expected and tester feedback is
welcome.

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

The macOS HAL code is useful as a protocol reference for our own implementation,
but Windows needs a native WDK driver.

Recommended architecture for the experimental Windows build:

1. Build and load the `OpenA8DJUsb` WDK package that binds to
   `USB\VID_17CC&PID_1978`.
2. Implement the CAIAQ USB control and isochronous transport in a WDF-based USB
   component.
3. Prototype Windows audio endpoint exposure using AVStream or another Windows
   audio driver model suitable for a vendor-specific USB device.
4. Evaluate PortCls/WaveRT after the experimental build confirms the bus/model fit;
   Microsoft documents PortCls as the normal audio miniport path, but also
   notes that PortCls port drivers are for system buses rather than external USB
   buses.
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

- Expose 4 stereo playback endpoints or one 8-channel endpoint, depending on
  which layout Traktor handles best.
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
- Build an MSI/bootstrapper that installs the signed driver package and the
  companion tools.
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

## Why There Is No MSI Yet

An MSI without a real Windows driver package would not make the Audio 8 DJ
available to Windows audio applications. It would only install files.

For that reason, this repository intentionally does not build or publish a
Windows MSI until the Windows driver exists and can be tested. Publishing a
placeholder MSI would be misleading and could make support/debugging harder.

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
