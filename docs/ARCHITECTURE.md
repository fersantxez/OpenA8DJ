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
