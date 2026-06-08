# Audio 8 DJ macOS Driver Feasibility

## Verdict

Feasible as an independent implementation. The publishable path is original
open-source code based on live testing against lawfully owned hardware, public
macOS APIs, public USB descriptors, public hardware specifications, and original
project test results.

OpenA8DJ now publishes the Audio 8 DJ on macOS 26.5/Apple Silicon as a Core
Audio HAL device. The current implementation exposes 8 inputs, 8 outputs, MIDI
endpoints, hardware control profiles, and 44.1/48 kHz playback validated in
local listening tests. Experimental 88.2/96 kHz support is exposed for extended
testing.

Estimated difficulty remains high. A testable HAL audio driver is implemented
and locally validated, but a polished public product still needs Developer ID
signing, Apple notarization, a settings UI, longer validation, and eventually a
DriverKit/System Extension distribution path.

## Local Compatibility Findings

- System tested: macOS 26.5, Darwin 25.5.0, Apple M1 / arm64.
- The Audio 8 DJ is visible on USB:
  - Product: `Audio 8 DJ`
  - Vendor: `Native Instruments`
  - `idVendor`: `6092` / `0x17cc`
  - `idProduct`: `6520` / `0x1978`
  - USB speed: high-speed USB 2.0
  - Device class/subclass/protocol: vendor-specific (`255`)
- The current OpenA8DJ HAL exposes the device to Core Audio as
  `Open Audio 8 DJ`.
- The device is vendor-specific USB hardware, so a custom compatibility driver
  is required for modern macOS systems that do not expose it as a class-compliant
  USB Audio device.

## Positive Evidence

Local OpenA8DJ tests confirm that the control plane and audio transport are
reachable through modern `IOUSBHost`:

- Device information requests succeed on the control endpoint.
- Audio parameter commands succeed for the tested sample rates.
- The OpenA8DJ transport identifies the expected control, capture, and playback
  pipes through public USB descriptors and live device tests.
- The device exposes 8 analog audio outputs, 8 analog audio inputs, and one MIDI
  input/output pair.
- 44.1 and 48 kHz playback are validated through Core Audio and Traktor.
- 44.1, 48, 88.2, and 96 kHz are available for extended validation in the HAL.

Current compatibility facts used by OpenA8DJ:

- Vendor ID: `0x17cc`
- Audio 8 DJ product ID: `0x1978`
- 8 analog inputs
- 8 analog outputs
- MIDI input/output pair
- Four stereo input pairs and four stereo output pairs
- 44.1 and 48 kHz validated locally
- 88.2 and 96 kHz exposed for extended testing
- Hardware controls for input mode, ground-lift profiles, and software lock

## Recommended Architecture

### Long-term Apple Distribution Route

Build a host macOS app containing a DriverKit system extension and
AudioDriverKit driver:

- Host app installs/activates the system extension.
- DriverKit/USBDriverKit matches `idVendor 0x17cc`, `idProduct 0x1978`.
- USB transport code opens the vendor-specific device and runs the control,
  MIDI, capture, and playback paths.
- AudioDriverKit publishes Core Audio device objects, streams, controls, sample
  rates, and timestamps.
- A small settings app exposes Audio 8 DJ-specific controls.

This is the strongest public distribution path, but requires Apple Developer
entitlements for DriverKit and USB transport.

### Current Public Preview Route

Ship a Core Audio HAL plug-in plus a user-space USB transport using IOUSBHost.

Pros:

- Already validates the device on modern macOS.
- Lets the project test protocol behavior, timing, routing, and DVS workflows
  before migrating to DriverKit.
- Keeps the implementation in user space while the project matures.

Cons:

- Less canonical for long-term distribution.
- Needs careful signing/notarization work.
- Needs longer testing because failures can affect `coreaudiod`.

## Major Risks

- Real-time stability: low-latency isochronous USB plus Core Audio timestamps
  and drift handling is the hard part.
- Entitlements: a distributable DriverKit driver requires Apple-granted
  entitlements.
- Licensing/provenance: this repository is MIT-licensed as original
  implementation work. Do not copy proprietary vendor material, firmware, or
  third-party implementation code under incompatible license terms into this
  repository.
- Firmware: the connected device currently enumerates normally with firmware
  already active. Do not redistribute firmware unless a clearly redistributable
  source and license are documented first.
- App compatibility: DJ apps may expect all 8 channels, stable clocking, and
  small buffer sizes; a partial Core Audio device is not enough for real use.

## Proposed Milestones

1. Repository bootstrap and hardware probe tool. Done.
2. Control-plane prototype. Done locally.
3. Audio transport prototype. Done locally.
4. Core Audio proof of concept. Done locally.
5. Full Audio 8 DJ device. Implemented locally as a HAL plus user bridge.
   - 8 input channels and 8 output channels exposed.
   - 44.1/48 kHz locally validated.
   - 88.2/96 kHz exposed for extended testing.
   - MIDI endpoints and command-line settings controls.
   - A polished graphical settings UI is still future work.
6. DriverKit production path.
   - Host app.
   - System Extension activation.
   - Signing, notarization, entitlement request.

## Feasibility Summary

The project is viable because the hardware is visible, modern macOS still
provides user-space USB and Core Audio mechanisms, and local OpenA8DJ tests have
validated the required audio, MIDI, and control paths. The hard work is making
that implementation stable, signed, notarized, and production-quality.
