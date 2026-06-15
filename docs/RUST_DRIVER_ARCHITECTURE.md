# Rust Driver Architecture

This document defines the target architecture for the parallel Rust
implementation of OpenA8DJ. It is intentionally developed in the
`/Users/fer/dev/audio8djrust` worktree and must not interfere with active
mainline C/Objective-C driver investigation.

For the product-level audiophile/DVS target design, see
`docs/AUDIOPHILE_DRIVER_DESIGN.md`.

## Boundary

The main worktree at `/Users/fer/dev/opena8dj` is read-only reference material.
Rust work may inspect the C implementation, QA scripts, quality logs, and
documentation, but it must not modify, format, clean, install from, or generate
files into that tree.

The Rust branch is an experiment until it proves parity and audio quality
through the same or stricter gates as the mainline driver.

## Goals

- Build a modern, modular, maintainable driver core without losing the mainline
  hardware knowledge.
- Optimize for audio quality, predictable realtime behavior, low CPU, and clean
  recovery after start/stop, hotplug, and sample-rate changes.
- Keep the public Core Audio timeline stable; do not chase USB cadence through
  `GetZeroTimeStamp`.
- Keep USB/device-cadence correction inside the transport layer.
- Make physical evidence, not clean counters alone, the quality authority.
- Support both a short-term HAL-hosted experiment and a long-term
  AudioDriverKit/USBDriverKit path.

## Non-goals

- Do not port the Linux ALSA driver model. Linux remains comparative evidence,
  not an implementation source.
- Do not merge Rust into mainline while active C investigation depends on fast
  iteration.
- Do not install Rust-built HAL or system components unless the user explicitly
  asks for a Rust experiment install.
- Do not treat internal playback, CPU, or stream counters as proof of analog
  quality without physical loopback evidence.
- Do not expose experimental USB clock behavior through Core Audio public
  timing.

## Mainline Learnings To Preserve

### Quality gates are part of the architecture

The current project learned that healthy Core Audio counters can coexist with
bad music playback. The Rust design must preserve a ladder equivalent to:

1. Prepare repeatable real music before changing driver behavior.
2. Run no-sound simulated output packing tests.
3. Run internal playback CPU/UI stress gates.
4. Run output-pair and timecode surface smoke gates.
5. Run physical bench sanity.
6. Run physical tone/music loopback with route verification.
7. Ask for human listening only after the automated evidence is valid.

Blocked capture is not candidate failure. It must remain represented as
`BLOCKED_PHYSICAL_CAPTURE`, `BLOCKED_DIRTY_ROUTE`, or a similarly explicit
measurement state so Rust cannot produce false readiness.

### Stable HAL timing beats USB-clock exposure

The current direction keeps Core Audio timing stable and treats USB cadence as
an internal transport problem. Rust should model this explicitly:

- HAL/AudioDriverKit timestamp anchor is monotonic and client-facing.
- USB frame timing is an internal signal for queue pacing, drift observation,
  and transport correction.
- USB correction never changes the public Core Audio timeline directly.

### The corruption point is probably after Core Audio input to the HAL

Prior diagnostics showed Core Audio material entering the HAL could match the
source while output still sounded wrong. Rust should therefore make the path
observable in three layers:

- client/HAL input frames,
- transport-consumed frames,
- packed USB bytes and completed isochronous transactions.

### Cold start and idle behavior are separate problems

The mainline improved internal CPU and warm start behavior with an ISO64/q8
family plus pre-open/keep-open ideas. Rust should preserve the distinction:

- open-but-not-streaming may be acceptable if idle CPU is truly zero or near
  zero;
- ISO streaming after StopIO can create underruns/resets and must be treated as
  dangerous until proven;
- StartIO latency, first callback latency, and steady-state CPU are separate
  metrics.

### Completion allocation and locks are suspect in realtime paths

The current implementation has useful but costly patterns: Objective-C transfer
objects, block completions, mutable data buffers, mutex-protected rings, and
telemetry locks in hot paths. Rust must improve this with preallocated pools,
typed state machines, bounded atomics, and lock-free single-producer /
single-consumer audio queues.

### Diagnostics must be cheap by default

The mainline accumulated many diagnostic flags. Rust should split diagnostics
into tiers:

- always-on atomic counters with bounded overhead;
- opt-in aggregate cadence diagnostics;
- opt-in capture buffers outside normal realtime paths;
- no file I/O, allocation, logging, or blocking in callbacks.

## Target Architecture

```text
Core Audio HAL or future AudioDriverKit host
      |
      v
thin platform adapter in C/Objective-C or DriverKit C++
      |
      v
open-a8dj-ffi
      |
      v
open-a8dj-core
      +-- audio topology and channel maps
      +-- CAIAQ protocol and hardware controls
      +-- sample conversion and mode-2 packing
      +-- input unpacking and routing
      +-- output timeline and drift follower
      +-- realtime queue planner
      +-- metrics snapshots
      +-- deterministic simulator
      |
      v
platform USB adapter
      +-- IOUSBHost for HAL experiment
      +-- USBDriverKit for final dext target
```

The important split is not "Rust versus C"; it is "pure deterministic driver
core versus platform adapter". The platform adapter owns Apple APIs. Rust owns
protocol, scheduling policy, queue state, conversion, and verification logic.

## Proposed Workspace

```text
crates/
  open-a8dj-core/
    src/audio.rs
    src/caiaq.rs
    src/controls.rs
    src/input.rs
    src/metrics.rs
    src/output.rs
    src/protocol.rs
    src/realtime.rs
    src/sim.rs
    src/transport.rs
  open-a8dj-ffi/
    src/lib.rs
    include/open_a8dj_core.h
  open-a8dj-tools/
    src/bin/a8dj-pack-sim.rs
    src/bin/a8dj-metrics-diff.rs
```

### `open-a8dj-core`

Pure Rust and mostly `no_std`-friendly except where tests and simulators need
`std`. It should not call Core Audio, IOKit, IOUSBHost, DriverKit, filesystem,
or process APIs.

Responsibilities:

- validate sample rates and topology;
- convert `f32` frames to/from signed 24-bit big-endian samples;
- pack and unpack Audio 8 DJ mode-2 USB frames;
- maintain input and output channel maps;
- own output timeline/drift policy;
- decide playback request sizes and target in-flight depth;
- produce metrics snapshots;
- run deterministic simulations.

The `sample` module keeps finite-value parity with the current C/Python
conversion path, but deliberately defines one stricter safety invariant:
non-finite `f32` values become flagged silence. That is not an accidental
protocol difference; it prevents NaN/Inf from reaching realtime integer
conversion and gives the driver a counter to expose the anomaly.

### `open-a8dj-ffi`

Stable C ABI around the pure Rust core. The C ABI must be boring and explicit:

- opaque engine handle;
- explicit config structs with version and size fields;
- caller-owned buffers;
- no panics across FFI;
- no allocation in realtime calls after engine initialization;
- structured error codes, never strings in hot paths.

### Platform HAL adapter

Short-term experiment only. The existing HAL remains C/Objective-C, but calls
the Rust core for pure operations. IOUSBHost ownership stays outside Rust until
there is evidence that the core improves safety and parity.

Initial HAL-facing calls should be:

- create/destroy engine;
- start/stop stream state;
- write client output frames at sample time;
- read decoded input frames;
- fill playback packet bytes for a provided request layout;
- consume capture packet bytes and transaction metadata;
- snapshot counters.

### Future DriverKit adapter

The durable end state is an app plus DriverKit system extension:

```text
OpenA8DJ.app
  - install/update UI
  - control panel
  - diagnostics export
  - SystemExtensions activation

OpenA8DJDriver.dext
  - IOUserAudioDriver / IOUserAudioDevice / IOUserAudioStream
  - USBDriverKit pipes for EP1, capture, playback, MIDI
  - Rust core linked as a static library where allowed
```

The Rust core should be written so the HAL adapter can be discarded later
without rewriting protocol and realtime logic.

## Execution Model

### Threads and ownership

- Audio callback thread: copies client frames into the output timeline and reads
  input frames. No allocation, no filesystem, no Objective-C, no blocking locks.
- USB completion path: consumes capture transactions, decodes input, schedules
  playback, fills provided transfer buffers, updates atomics.
- Control path: handles EP1 commands, hardware controls, profiles, and MIDI.
- Diagnostics path: snapshots atomics and bounded aggregate counters.

Data flow:

```text
client output frames
  -> output timeline writer
  -> transport follower / packet filler
  -> isochronous OUT packets

isochronous IN packets
  -> capture decoder
  -> input channel router
  -> input ring for clients
  -> cadence signal for playback scheduling
```

### Realtime rules

The Rust core must treat the following as bugs in realtime paths:

- heap allocation;
- mutex blocking;
- logging;
- file I/O;
- Objective-C message send;
- dynamic dispatch through boxed trait objects;
- unbounded loops tied to external state;
- panics;
- lossy state hidden behind compile-time flag combinations.

Allowed in realtime paths:

- fixed-size stack buffers within bounded limits;
- preallocated ring buffers;
- atomics;
- branchless or predictable sample conversion;
- bounded aggregate counter updates;
- explicit state-machine transitions.

### Output timeline policy

The current C implementation's timeline direction is worth preserving but
should be made more explicit:

- client sample time is an input hint, not an authority that can force jumps;
- small sample-time jitter is smoothed into continuous playback;
- large gaps reset the transport timeline with measured counters;
- pre-roll/start latency is a policy parameter;
- elastic corrections are explicit and counted;
- replay/fade is a bounded recovery policy, never an invisible steady state.

The Rust core should expose this as a tested state machine:

```text
Idle -> Priming -> Running -> Recovering -> Draining -> Idle
```

Each transition emits a counter event. The simulator must be able to replay
sample-time discontinuities and verify no unbounded resets occur.

### USB queue policy

The current mainline found that queue depth, ISO frame grouping, and keep-open
behavior affect CPU and start latency. Rust should model these as runtime
policy values, not a large forest of compile-time flags:

- capture transfer frames;
- capture queue depth;
- playback transfer frames;
- playback target depth;
- playback max depth;
- capture-paced output lead;
- explicit scheduling enabled/disabled;
- open-but-not-streaming idle policy;
- stop ISO on StopIO.

Each candidate configuration should be serializable into the run artifact so
metrics always identify the exact policy under test.

## Metrics Contract

Every core snapshot should separate:

- public device state;
- stream state;
- capture transaction counters;
- playback transaction counters;
- output timeline counters;
- input decode counters;
- scheduling counters;
- capture readiness state;
- supervisor/watch state;
- spectral-coloration metrics;
- quality-gate metadata;
- build/config identity.

Required counters:

- `output_frames_written`;
- `output_frames_read`;
- `output_timeline_resets`;
- `output_active_underruns`;
- `output_elastic_drops`;
- `output_elastic_replays`;
- `capture_completion_delta`;
- `playback_completion_delta`;
- `capture_to_playback_queue_delta`;
- `playback_transfers_in_flight`;
- `playback_queue_failures`;
- `capture_zero_complete_transactions`;
- `capture_other_byte_count_transactions`;
- `playback_short_transfers`;
- `input_check_errors`;
- `output_panic_flags`;
- `clock_anchor_valid`;
- `physical_capture_status`;
- `physical_capture_reason`;
- `found_irig_usb_by_id`;
- `found_irig_core_audio`;
- `usb_enumeration_failures`;
- `failed_usb_ports`;
- `next_recovery_action`;
- `ready_streak`;
- `stable_polls`;
- `supervisor_status`;
- `supervisor_latest_reason`;
- `low_band_capture_to_ref_gain_db`;
- `mid_band_capture_to_ref_gain_db`;
- `high_band_capture_to_ref_gain_db`;
- `mid_vs_low_coloration_delta_db`;
- `high_vs_low_coloration_delta_db`;
- `high_vs_mid_coloration_delta_db`;
- `metallic_coloration_score_db`;
- `measurement_status`;
- `candidate_quality_status`.

The Rust metrics schema should be versioned and exportable as JSON so the
existing Python/Bash gates can consume it without parsing human text.

## Validation Strategy

### Phase 1: pure parity

No hardware and no install.

- Golden tests for `f32` <-> signed 24-bit big-endian conversion.
- Golden tests for mode-2 packing/check bytes.
- Golden tests against captured raw USB payloads where available.
- Property tests for saturation, clipping, silence, polarity, and channel maps.
- Simulator tests for timeline resets, late writes, startup silence, and drift.

### Phase 2: no-sound integration

No physical output required.

- Rust simulated output must match C simulated output on the same fixture.
- Use real music fixture preparation from the existing soundcheck flow.
- Compare residual metrics on decoded packed output.
- Fail if Rust parity changes gain, polarity, sample order, or check-byte
  layout unexpectedly.

### Phase 3: HAL-hosted dry integration

Build the HAL adapter but do not install by default.

- Compile-only and smoke-load checks against a local bundle.
- `hal-parity-smoke` equivalent.
- IPC metrics schema checks.
- No behavior-changing install without explicit user approval.

### Phase 4: controlled hardware experiment

Only after explicit approval.

- Verify current installed driver identity and backup path.
- Install Rust experiment with unique version string and hash.
- Run internal gates first.
- Run physical bench and loopback gates only if the capture route is valid.
- Restore known baseline on failure or blocked measurement.

### Phase 5: production path

Only after Rust core beats or matches mainline evidence.

- DriverKit prototype.
- Signed app/system-extension flow.
- Full 1.0 test matrix.

## Codebase Rules

- `open-a8dj-core` is the source of truth for protocol and realtime state.
- Platform adapters are thin and replaceable.
- Build flags select diagnostics and platform adapters, not hidden behavior
  variants.
- Runtime policy structs replace most compile-time experiment flags.
- Every policy value must appear in metrics artifacts.
- Tests should prefer deterministic fixtures over sleeps and ambient state.
- Unsafe Rust is allowed only at FFI and fixed-buffer boundaries, and each
  unsafe block must state its invariant.

## Initial Implementation Order

1. Create the Cargo workspace and `open-a8dj-core`.
2. Implement sample conversion and mode-2 packing.
3. Build a parity tool that reads a WAV or raw fixture and emits packed bytes.
4. Implement the output timeline state machine with simulator tests.
5. Implement metrics structs and JSON export.
6. Add C ABI wrappers behind `open-a8dj-ffi`.
7. Add optional HAL adapter calls without changing default builds.
8. Only then consider a hardware experiment.

## Open Questions

- Whether Rust can be linked cleanly into the final DriverKit system extension
  under the chosen Apple toolchain and signing setup.
- Whether IOUSBHost should remain Objective-C for the HAL experiment or move to
  a Rust-owned C shim later.
- Which exact ISO64/q8 lessons survive physical loopback once iRig capture is
  available again.
- Whether input decode should be always active in the Rust core or lazily
  activated only when Core Audio input clients are present.
- How much telemetry can remain always-on before it affects CPU or analog
  quality.

## Current Design Bias

The first Rust win should not be a new installed driver. It should be a
deterministic core that proves:

- the USB audio packing is easier to inspect and test;
- output timeline behavior is explicit;
- metrics are structured;
- realtime code has no accidental allocation or locks;
- the existing QA harness can compare Rust and C behavior side by side.

Only after that should Rust touch the active audio stack.
