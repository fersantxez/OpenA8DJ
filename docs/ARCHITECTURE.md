# C++/DriverKit Architecture

## Principle

Greenfield shell, brownfield behavior.

The C++ line is independent. It learns proven behavior from the C/Objective-C mainline and gates from Rust, but it does not inherit either runtime architecture blindly.

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
- `driverkit/`: DriverKit/AudioDriverKit shell placeholder, not installed or activated.
- `tools/`: future offline analyzers and evidence generators.
- `scripts/`: future safe wrappers for offline gates.
- `docs/`: living architecture and readiness documents.

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
