# Modern macOS C++/DriverKit Architecture

## Status

OpenA8DJ `main` is the modern macOS C++ driver line. The current public release
is packaged as a macOS-native user-space Core Audio HAL driver backed by
IOUSBHost USB transport, CoreMIDI endpoints, and an Audio 8 DJ control bridge.
The DriverKit/AudioDriverKit shell remains the forward architecture for a
future System Extension build when the full DriverKit SDK/signing path is
available.

The previous C/Objective-C implementation is preserved on the parallel
`legacy` branch. Treat it as the historical C implementation informed by Linux
CAIAQ / `snd-usb-caiaq` reverse-engineering behavior and as an emergency
baseline, not as the current user-facing mainline.

## Principle

Greenfield shell, brownfield behavior.

The C++ line is independent. It learns proven behavior from the C/Objective-C
legacy line and gates from Rust, but it does not inherit either runtime
architecture blindly. The product architecture for new releases is macOS-first:
Core Audio HAL now, DriverKit/AudioDriverKit as the forward system-extension
path.

## Planes

### Data Plane

- Audio frames.
- USB packet/transfer representation.
- Timestamps.
- Routing hot path.
- Preallocated buffers and SPSC queues where needed.
- Underrun, overrun, and jitter counters.

### Control Plane

- Sample rate policy.
- Buffer size policy.
- Routing configuration.
- Timecode profile mode.
- Device state and recovery state.

### Observability Plane

- Atomic counters.
- Snapshots outside callback.
- Evidence files written only by tools/gates outside the real-time path.
- Logs outside per-buffer processing.

## Initial Components

- `core/`: pure C++20, no macOS dependency.
- `src/hal/`: current installable macOS Core Audio HAL driver path.
- `driverkit/`: DriverKit/AudioDriverKit shell placeholder, not installed or activated.
- `tools/`: future offline analyzers and evidence generators.
- `scripts/`: future safe wrappers for offline gates.
- `docs/`: user, contributor, reference, and maintainer-state documents.

## Current Core Model

- 8 input channels.
- 8 output channels.
- Stereo pairs A/B/C/D.
- Host sample placeholder: float32 interleaved.
- USB sample placeholder: signed 24-bit packed USB.
- Initial routing: identity A/B/C/D.

## Prepared Transport Core

`PreparedTransportBackend` is the current pure-C++ bridge between the audio
data plane and the future DriverKit/USB adapter.

Responsibilities:

- expose HAL-facing `hal_write_playback` and `hal_read_capture` calls backed by
  fixed SPSC rings;
- keep prepared-slot/requeue counters owned by the backend side;
- reject steady-state HAL direct requeue attempts through metrics;
- reject fallback allocation attempts after streaming starts;
- track timestamp regressions and channel identity failures;
- produce a `PreparedTransportSafety` snapshot for offline gates.

Non-responsibilities:

- it does not talk to USB;
- it does not install or activate a dext;
- it does not prove physical sound quality.

The current DriverKit prepared transport gate uses this core type directly.

## Packet/Ring Integration

`opena8djcpp_prepared_transport_packet_contract` validates the first packet
integration layer on top of `PreparedTransportBackend`:

- Mode2 capture bytes are produced with `Mode2OutputPacker`;
- capture bytes are decoded with `decode_mode2_usb_bytes_into`;
- decoded capture frames are published into the prepared backend capture ring;
- playback frames are read from the prepared backend playback ring;
- playback frames are packed back to Mode2 bytes and decoded again for parity;
- default `start_byte=4` and `352`-byte transfers are part of the contract.

This remains an offline model. The future DriverKit/USB adapter must preserve
the same invariants when the bytes come from hardware.

## Routing And Timecode Over Prepared Transport

`opena8djcpp_prepared_transport_routing_timecode_contract` validates behavior
above the packet/ring layer:

- S24 playback batches can be routed before entering the prepared backend.
- Routed playback batches emerge from the backend with zero channel mismatches.
- Timecode-vinyl, timecode-cd-line, and phono profile captures are decoded,
  passed through the backend capture ring, converted according to profile
  source maps, and analyzed per deck.
- Decks A/B/C/D must remain isolated; leakage RMS must stay at zero in the
  synthetic fixture.

This is the offline contract for Traktor/timecode behavior before hardware.
