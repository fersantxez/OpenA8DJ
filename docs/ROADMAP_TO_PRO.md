# Roadmap to a Production-Quality Audio 8 DJ Driver

This document captures the current gap between the working OpenA8DJ HAL
prototype and a production-quality independent macOS driver.

## Current evidence

- macOS enumerates `Open Audio 8 DJ` as an output-focused device with 0 inputs
  and 8 outputs in the 0.3.24 preview.
- The HAL exposes one 8-channel output stream with named A/B/C/D stereo pairs.
- Local listening confirmed substantially cleaner playback at 44.1 and 48 kHz
  after the capture-paced USB transport work.
- Traktor buffer-size selection no longer reports the invalid sentinel value
  seen in earlier builds.
- Physical iRig loopback gates now include tone sideband checks, music residual
  checks, click outlier checks, and Core Audio CPU guards.
- Output C/D are exposed and still need the final physical mixer pass.
- Core Audio input streams and the full vinyl/CD-line input matrix remain open.

## Reference targets

- Public Native Instruments documentation lists the Audio 8 DJ as a USB 2.0,
  bus-powered,
  24-bit interface with 8 inputs, 8 outputs, MIDI 1 in / 1 out, and 44.1, 48,
  and 96 kHz operation.
- Apple's modern target for hardware audio drivers is AudioDriverKit in a
  DriverKit system extension. Since macOS Monterey, Apple describes this as a
  single dext path without a separate HAL plug-in.

## Priority 0: full timecode/input matrix

Playback quality is now good enough to move the next engineering focus to input
capture and the full DVS matrix. That test depends on clean stereo inputs,
correct hardware input modes, and stable low-latency capture.

Actions:

1. Physically test Input A/B/C/D with known tones.
2. Re-run `opena8dj-control profile timecode-vinyl` with Traktor's scope.
3. Verify ground-lift flags for vinyl, CD/line, and phono paths.
4. In Traktor, validate timecode scope for Deck A and Deck B at 44.1 kHz.
5. Repeat timecode validation at 48 kHz.

Acceptance:

- Traktor scope is stable and correctly assigned per deck.
- Deck A/B timecode input does not leak across pairs.
- Output A/B remains isolated while timecode inputs are active.
- No speed drift, white noise, or input-channel swapping.

## Priority 1: timing correctness

Core Audio timing still needs a longer independent validation pass. Earlier
versions showed re-anchoring messages; the current version should be measured
again under sustained playback and Traktor use.

Actions:

1. Replace the independent HAL clock thread with a monotonic timestamp model.
2. Base `GetZeroTimeStamp` on a single start anchor plus the actual Core Audio
   IO period, not a separate drifting thread.
3. Use `gClockPeriodFrames` consistently where the callback period is needed.
4. Add a log-based acceptance test: during 5 minutes of playback there must be
   zero `TimeStampOutOfLine` messages for the OpenA8DJ device.
5. After that, measure `coreaudiod` CPU again. Target: below 10% during normal
   48 kHz stereo playback.

## Priority 2: real-time USB/audio path

The current transport works and uses a pipelined asynchronous isochronous model.
It still allocates Objective-C transfer objects and performs mutex-protected
ring operations inside the streaming path. That is not yet a professional
real-time design.

Actions:

1. Preallocate isochronous buffers and transaction arrays at stream start.
2. Reuse transfer descriptors rather than allocating Objective-C objects in the
   stream path.
3. Replace mutex-protected per-frame rings with single-producer/single-consumer
   lock-free rings.
4. Bulk-convert frames instead of calling ring read/write once per frame.
5. Remove the output non-zero scan from the realtime callback. Silence should be
   queued as silence, not detected by scanning the whole buffer.

Acceptance:

- No underruns during 30 minutes at 44.1, 48, and 96 kHz.
- No white noise or accelerated playback after sample-rate changes.
- Stable audio under Spotify, Traktor, and a local deterministic test player.

## Priority 3: sample-rate and channel truth

Public hardware documentation lists 44.1, 48, and 96 kHz. Linux also enables
88.2 kHz for Audio 8 DJ. For a production-quality macOS driver, advertised
rates must match what the hardware supports reliably in OpenA8DJ testing.

Actions:

1. Verify sample-rate exposure through public documentation and live hardware
   loopback testing.
2. Keep 88.2 kHz hidden unless it passes loopback tests at production quality.
3. Verify all 8 output channels by physical loopback:
   - Output A L/R -> corresponding input pair
   - Output B L/R -> corresponding input pair
   - Output C L/R -> corresponding input pair
   - Output D L/R -> corresponding input pair
4. Verify all 8 input channels with known input tones.

Acceptance:

- Correct channel names and mapping in Audio MIDI Setup and Traktor.
- Channel isolation: a tone on one output appears only on the expected physical
  output pair.
- Round-trip capture confirms correct speed and pitch at each advertised rate.

## Priority 4: MIDI and hardware controls

The repository has a MIDI/control bridge, but the HAL's direct CoreMIDI creation
path is intentionally disabled. A professional driver must make MIDI and device
controls first-class, reliable features.

Actions:

1. Verify the installed `opena8dj-midid` LaunchAgent creates stable MIDI In and
   MIDI Out endpoints.
2. Add automated MIDI loopback tests.
3. Expose input mode, ground-lift flags, software lock, and firmware/status
   through a small signed control app.
4. Persist and restore control state where the hardware supports it.

Acceptance:

- MIDI endpoints appear consistently after login, hotplug, and sleep/wake.
- MIDI round-trip has no dropped bytes in a long-running test.
- Control changes update the hardware LEDs/state and survive driver restart
  where expected.

## Priority 5: installation and signing

The current PKG/DMG is useful for local testing. The production target should be
a signed app bundle containing a DriverKit system extension, plus a DMG or PKG
wrapper for distribution.

Actions:

1. Short term: keep the HAL package for internal testing.
2. Medium term: ship a signed app that installs/updates the HAL and control
   helper cleanly.
3. Long term: migrate audio and USB transport into an AudioDriverKit +
   USBDriverKit dext.
4. Request or configure the required Apple Developer entitlements:
   - DriverKit
   - AudioDriverKit family
   - USB transport access for the Native Instruments vendor/product ID
   - System Extension distribution

Acceptance:

- Two-click install on a clean Apple Silicon Mac.
- No Terminal commands needed for normal users.
- Clean uninstall removes HAL/dext, helper tools, LaunchAgents, and cached
  state.

## Long-term architecture

Recommended final architecture:

```text
OpenA8DJ.app
  - installer/update UI
  - device control panel
  - diagnostics exporter
  - SystemExtensions activation

OpenA8DJDriver.dext
  - IOUserAudioDriver
  - IOUserAudioDevice
  - IOUserAudioStream input/output
  - IOUserAudio controls
  - USBDriverKit pipes for CAIAQ control, MIDI, capture, playback

Core Audio / Core MIDI
  - 8 in / 8 out audio
  - MIDI in / out
  - stable clock and channel layout
```

The HAL prototype remains valuable as an independent test bed, but
AudioDriverKit is the right endpoint for a durable macOS driver.

## Test matrix before calling it 1.0

- Enumeration: device appears with 8 in / 8 out, correct names, correct rates.
- Playback: Spotify, Music, Safari/YouTube, Traktor.
- Recording: all input pairs at all supported rates.
- Loopback: channel map, pitch/speed, round-trip latency.
- Long run: 8 hours playback with underrun counter at zero.
- Hotplug: unplug/replug during idle and playback.
- Sleep/wake: device recovers without restarting the machine.
- Multi-client: Traktor plus system sounds or another Core Audio client.
- CPU: `coreaudiod` remains low and no timestamp re-anchor storm appears.
- Installer: install, upgrade, uninstall, reinstall on a clean machine.
