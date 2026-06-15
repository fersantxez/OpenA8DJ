# OpenA8DJ-rust Audiophile Driver Design

This document defines the target design for the best possible OpenA8DJ-rust
driver: audiophile playback/capture quality, complete Audio 8 DJ functionality,
flexible routing, low CPU, and stable Traktor/timecode vinyl behavior.

It is a Rust-worktree design artifact. The C/Objective-C worktree remains
read-only reference material.

Current mainline findings imported on 2026-06-14 are tracked in
`docs/MAINLINE_FINDINGS_2026_06_14.md`.

## Product Objective

OpenA8DJ-rust should make the Audio 8 DJ feel like high-end supported hardware
on a modern Mac:

- clean playback with no crackle, sidebands, metallic/radio texture, clicks,
  pitch instability, or CPU/window-coupled noise;
- complete 8 input / 8 output operation;
- predictable A/B/C/D stereo pair routing;
- Traktor DVS/timecode vinyl and CD-line workflows that are stable at 44.1 and
  48 kHz first;
- low driver and `coreaudiod` CPU;
- a codebase where protocol, timing, routing, and quality policy are explicit
  modules rather than hidden flag combinations.

The design target is not "a Rust port". The target is a better driver core that
can eventually replace the fragile parts of the current HAL architecture.

## Non-Negotiable Constraints

- Work happens in `/Users/fer/dev/audio8djrust` on `rust/*` branches.
- `/Users/fer/dev/opena8dj` and mainline branches are read-only references.
- Hardware/audio-sensitive tests require the global hardware lock documented in
  `docs/HARDWARE_GATE.md`.
- No Rust artifact uses ambiguous mainline names. Public Rust names include
  `-rust`.
- Internal counters do not prove audio quality. Physical capture and listening
  remain final quality evidence.

## Design Principles

### 1. Physical quality is the product

The driver is judged by what reaches the ears and the iRig capture, not by
clean-looking queues. Metrics must prove that transport health, analog quality,
and CPU behavior all agree.

### 2. Stable public time, private device correction

Core Audio clients get a monotonic, stable public timeline. USB frame cadence is
used internally for queue pacing and duplex synchronization, but it must not
force public `GetZeroTimeStamp` behavior into a Core Audio CPU loop.

### 3. One explicit duplex engine

The device has one hardware clock. Playback, capture, MIDI/control state, and
DVS profiles must be coordinated by one engine with a shared device-cadence
model. Input activation must be deliberate so an unrelated microphone client
cannot accidentally perturb output.

### 4. Default path is bit-faithful and boring

The normal audio path applies no EQ, stereo widening, limiting, automatic gain,
or timecode DSP. It converts, routes, schedules, and preserves samples. Any
monitoring or diagnostic DSP is opt-in and outside the default DVS path.

### 5. Realtime work is bounded

Audio callbacks and USB completions may copy, convert, update atomics, and move
preallocated descriptors. They may not allocate, log, block on mutexes, touch
the filesystem, call Objective-C dynamically from the core, or loop on external
state.

## Target Component Graph

```text
Core Audio HAL adapter now / DriverKit adapter later
      |
      v
open-a8dj-ffi
      |
      v
open-a8dj-core
      +-- engine
      +-- topology
      +-- routing
      +-- sample
      +-- mode2
      +-- duplex_clock
      +-- output_timeline
      +-- input_decoder
      +-- dvs
      +-- controls
      +-- scheduler
      +-- metrics
      +-- sim
      |
      v
USB platform adapter
      +-- control EP1
      +-- isochronous capture IN 0x82
      +-- isochronous playback OUT 0x06
      +-- MIDI bridge
```

The Rust core owns the deterministic driver behavior. Platform adapters own
Apple APIs, USB object lifetimes, signing, installation, and service management.

## Core Modules

### `engine`

Owns lifecycle and coordinates the other modules.

States:

```text
Detached -> Enumerated -> Configured -> Priming -> Running -> Recovering
         -> Draining -> Stopped -> Detached
```

Rules:

- every transition is explicit and counted;
- sample-rate changes reconfigure the full duplex engine atomically;
- hotplug and sleep/wake produce structured states, not partial silent failure;
- StopIO drains and stops ISO safely unless a policy explicitly proves a safe
  near-zero-CPU open state.

### `topology`

Represents the device as four stereo pairs in each direction:

```text
Input A:  ch 1/2
Input B:  ch 3/4
Input C:  ch 5/6
Input D:  ch 7/8

Output A: ch 1/2
Output B: ch 3/4
Output C: ch 5/6
Output D: ch 7/8
```

Core Audio surface:

- four stereo output streams named Output A/B/C/D;
- one 8-channel input stream with Input A/B/C/D pair names for the HAL path;
- future DriverKit may expose four stereo input streams only if Traktor and
  Core Audio compatibility are proven better.

### `routing`

Provides a deterministic stereo-pair routing matrix.

Default:

```text
Client Output A -> Hardware Output A
Client Output B -> Hardware Output B
Client Output C -> Hardware Output C
Client Output D -> Hardware Output D

Hardware Input A -> Client Input A
Hardware Input B -> Client Input B
Hardware Input C -> Client Input C
Hardware Input D -> Client Input D
```

Supported policy:

- pair-to-pair remap for output and input;
- explicit input-source remap for calibration;
- explicit left/right swap for calibration;
- explicit polarity inversion for calibration;
- per-pair mute;
- per-pair unity/default gain with fixed-point coefficients;
- optional direct monitor/pass-through only as an explicit profile;
- no hidden channel folding;
- no routing mutation from a realtime callback.

Routing changes are staged on the control thread and committed at a cycle
boundary with a versioned snapshot so audio and metrics agree about which
matrix is active.

DVS product profiles reset input-source remap, left/right swap, and polarity
inversion to identity. Those transforms are useful diagnostic tools, but hidden
transforms would make Traktor scope failures impossible to trust.

### `sample`

Converts Core Audio `f32` to Audio 8 DJ signed 24-bit big-endian samples and
back.

Policy:

- unity gain by default;
- saturating round-to-nearest conversion;
- no hidden normalization;
- NaN and infinities become silence and increment anomaly counters;
- clipping increments counters but never panics;
- optional diagnostic TPDF dither stays off by default because the hardware
  analog path dominates the 24-bit quantization floor.

### `mode2`

Owns mode-2 packet layout, check-byte validation, channel order, and start-byte
policy.

Acceptance:

- silence packs to known golden bytes;
- full-scale positive/negative edge cases pack correctly;
- A/B/C/D pair order is stable;
- playback offset mistakes that previously produced white noise are impossible
  to hide because golden tests cover the packet cursor;
- input check errors are counted by transaction and by pair.

### `duplex_clock`

Models the device cadence without exposing it directly as the public Core Audio
timeline.

Inputs:

- completed capture transaction timing;
- expected frames per USB transaction;
- host monotonic time;
- current sample rate;
- StartIO sample-time anchor.

Outputs:

- internal `device_frame` estimate;
- jitter estimate;
- capture-to-playback phase estimate;
- target playback lead;
- drift counters and confidence.

Rules:

- the public audio timeline remains monotonic and sample-rate based;
- device cadence can tune packet lead and recovery policy;
- if clock confidence drops, the engine enters `Recovering` and exposes the
  reason in metrics;
- no timecode resampling is performed merely to hide host timing error.

### `output_timeline`

Stores client output by sample time in a preallocated circular timeline.

Behavior:

- accepts per-stream writes from Core Audio;
- fills inactive streams with silence;
- commits one coherent 8-channel frame set for each sample position;
- smooths small callback-order jitter;
- treats large discontinuities as counted resets;
- primes with silence until target lead is available;
- uses bounded fade/replay only as recovery, never steady-state drift
  correction.

The audio-quality target is that recovery counters stay at zero in normal
playback. Recovery exists to avoid violent noise, not to mask a broken clock.

### `input_decoder`

Consumes capture USB packets and produces client input frames.

Behavior:

- validates packet shape and check bytes;
- decodes signed 24-bit big-endian samples;
- applies the input routing matrix;
- writes to preallocated input rings;
- preserves raw DVS/timecode signal in default profiles;
- counts channel swaps, bad packet sizes, check-byte failures, discontinuities,
  and input-ring over/underruns.

### `dvs`

Owns DVS-specific profiles and validation state. It does not alter the
timecode audio unless an explicit diagnostic monitor is enabled.

Profiles:

```text
timecode-vinyl:
  input-mode = 0
  gnd-vinyl = on
  software-lock = on
  input transforms = identity
  preferred rate = 44100 or 48000
  default inputs = A/B
  default outputs = A/B

timecode-cd-line:
  input-mode = 1
  gnd-cd-line = on
  software-lock = on
  input transforms = identity
  preferred rate = 44100 or 48000

phono:
  input-mode = 2
  gnd-phono = profile-controlled
  software-lock = optional
  input transforms = identity
```

Timecode design requirements:

- Deck A input maps to Input A L/R by default.
- Deck B input maps to Input B L/R by default.
- Output A/B remain isolated while Input A/B timecode is active.
- Input C/D remain available for extended routing or future 4-deck validation.
- Profile changes are transactional: control write, hardware readback, metrics
  snapshot, then public readiness.
- DVS readiness is false if input decode is disabled, input pair labels are
  wrong, software lock is off in a locked profile, or physical scope validation
  has not passed.

### `controls`

Centralizes CAIAQ controls, software lock, ground lifts, input modes, MIDI
bridging state, and profile application.

The control ABI is versioned and shared by driver and tools. No tool keeps
Core Audio streaming open just to read controls.

### `scheduler`

Plans USB playback requests from device cadence and output timeline state.

Policy values are runtime configuration, not compile-time experiments:

- capture transfer frames;
- capture queue depth;
- playback transfer frames;
- playback target depth;
- playback max depth;
- target playback lead;
- explicit scheduling on/off;
- open-but-not-streaming policy;
- stop-on-StopIO policy.

The scheduler should favor a small, stable in-flight window over aggressive
queue churn. The goal is low CPU and low jitter, not maximum buffering.

Policy baselines:

- `iso5-normal`: conservative baseline for parity and failure comparison;
- `iso64-q8`: internal-performance candidate because recent mainline evidence
  showed strong CPU/start behavior, but not an audiophile candidate until valid
  physical iRig capture and listening pass;
- `stop-iso-on-stopio`: default safety policy until a near-zero-CPU keep-open
  state is physically proven.

The likely CPU win is reducing and stabilizing IOUSBHost enqueue/requeue
pressure. Packing micro-optimizations help less than queue pressure, transfer
reuse, and avoiding unnecessary transaction churn.

Mainline `0.3.133` is the current internal-performance reference: locked
sequential gates passed output-pair smoke, timecode smoke, and playback CPU/UI
stress with driver p95 around `6.8%`, low `coreaudiod`, zero timeline resets,
and zero active underruns. It remains blocked as an audiophile/listening
candidate while iRig physical capture is missing.

### `metrics`

Exports versioned snapshots in structured form.

Metric groups:

- build and policy identity;
- public device state;
- stream lifecycle state;
- routing/profile state;
- duplex clock confidence;
- capture transaction health;
- playback transaction health;
- timeline health;
- input decode health;
- DVS readiness;
- CPU and quality-gate annotations.

Metrics separate transport health from audio quality. A candidate can have
`transport_status=PASS` while `candidate_quality_status=FAIL`.

## Audio Data Plane

### Output path

```text
Core Audio client buffers
  -> per-stream copy into output_timeline
  -> routing matrix snapshot
  -> coherent 8-channel frame read
  -> sample conversion
  -> mode-2 packet packing
  -> USB playback transfer
```

Guarantees:

- no sample is packed before its routing/profile snapshot is known;
- no inactive stream leaks stale memory;
- no pair can swap silently without a topology test failing;
- underrun behavior is audible-safe, counted, and treated as a failing gate;
- output A/B/C/D can be validated independently.

### Input path

```text
USB capture transfer
  -> mode-2 validation
  -> sample decode
  -> hardware input pair assignment
  -> input routing matrix snapshot
  -> Core Audio input ring
  -> Traktor or capture client
```

Guarantees:

- DVS/timecode signal is preserved raw by default;
- check-byte and packet-shape failures never become silent input corruption;
- input activation is controlled by engine policy, not accidental host
  aggregate behavior;
- input A/B/C/D can be validated independently.

## Timecode Vinyl Synchronization

Timecode success requires three clocks to behave as one system:

- physical record/CD timecode signal entering Audio 8 DJ inputs;
- Audio 8 DJ USB capture/playback cadence;
- Core Audio client timeline used by Traktor.

The Rust design treats the Audio 8 DJ hardware cadence as the internal duplex
clock reference while keeping Core Audio public timing stable.

### Duplex synchronization model

The `duplex_clock` maintains:

```text
host_time
public_sample_time
device_frame_estimate
capture_frame_cursor
playback_frame_cursor
target_playback_lead
clock_confidence
phase_error_frames
jitter_p95_frames
```

The output scheduler consumes this model to keep playback lead stable. The
input decoder uses the same model to stamp capture availability. Traktor then
sees stable input timecode and stable deck output without the driver injecting
rate changes into the audio.

### DVS failure modes to prevent

- deck A/B input swap;
- left/right inversion;
- input bleed between A/B/C/D;
- software lock off during timecode test;
- wrong input-mode profile;
- capture underrun that looks like bad vinyl;
- output drift that makes deck audio feel late or unstable;
- CPU/UI activity modulating the timecode signal;
- sample-rate mismatch between Traktor and the device;
- hidden full-duplex activation when another input client opens.

### DVS acceptance gates

At 44.1 and 48 kHz:

- Traktor scope stable on Deck A through Input A only.
- Traktor scope stable on Deck B through Input B only.
- Deck A output isolated on Output A.
- Deck B output isolated on Output B.
- No input pair leakage above the configured gate.
- No channel swaps after StopIO/StartIO, sample-rate change, hotplug, or sleep.
- No timecode input dropouts during CPU/UI stress.
- No `input_check_errors`, `timeline_resets`, `active_underruns`, playback
  queue failures, or clock-confidence drops.
- Human operator confirms absolute/relative mode behaves normally.

## Routing Flexibility

The driver should expose the hardware plainly, then add explicit profiles:

### `Studio8x8`

Default one-to-one 8-in/8-out routing for DAWs and measurement.

### `TraktorDvs2Deck`

```text
Input A -> Deck A timecode
Input B -> Deck B timecode
Output A -> Deck A audio
Output B -> Deck B audio
Input C/D and Output C/D remain available but untouched.
```

### `TraktorDvs4DeckCandidate`

Experimental until physically validated:

```text
Input A/B/C/D -> Deck A/B/C/D timecode or line input
Output A/B/C/D -> Deck A/B/C/D audio
```

### `CaptureBench`

One output pair under test, all others silent, with routing metadata emitted for
quality captures.

### `DirectMonitorCandidate`

Optional future profile. It must be explicit, off by default, and measured for
latency and leakage before being exposed as product behavior.

## CPU And Resource Design

### Hot path budget

The hot path does only:

- bounded copy between caller buffer and preallocated ring;
- fixed-point/floating conversion;
- packet pack/unpack;
- atomic counter updates;
- state-machine transitions.

It does not:

- allocate;
- log;
- parse strings;
- use blocking mutexes;
- create Objective-C objects;
- call filesystem APIs;
- inspect process state;
- perform expensive time polling per frame.

### Queue policy

Use enough queue depth to absorb host jitter, but not enough to hide broken
timing or inflate latency:

- capture queue depth starts conservative and is measured;
- playback target depth tracks device cadence;
- short transfers and missed windows are counted;
- queue policy is serialized into every run artifact;
- CPU improvements are rejected if residuals, clicks, sidebands, or DVS
  stability regress.

### Diagnostics policy

Always on:

- atomics and cheap counters;
- current state;
- policy identity.

Opt-in:

- packet trace buffers;
- packed-byte capture;
- per-window timing histograms;
- CPU correlation collection;
- verbose logs.

Never in realtime:

- file writes;
- malloc/free;
- text formatting;
- lock waits.

### Supervisor policy

Autonomous recovery and watcher loops are low priority. They must:

- acquire the shared hardware gate before touching Core Audio/USB/audio state;
- skip as `SKIPPED_BUSY` when another owner holds the lock;
- poll slowly enough that they do not raise `coreaudiod` during playback CPU or
  physical quality gates;
- preserve `ready_streak`, `stable_polls`, USB-port failure details, and
  `next_recovery_action` in status output;
- avoid launching physical gates until iRig has remained ready for the required
  stable-poll count.

## Rejected Shortcuts

These paths are specifically rejected for the Rust design:

- native I24 or alternate byte-order experiments without golden packet proof;
- output coalescing that lowers CPU while output consumption drops, timeline
  resets appear, or physical quality is unmeasured;
- keeping ISO streaming alive after StopIO unless it proves zero/near-zero idle
  CPU and no underrun/reset side effects;
- exposing USB cadence through public Core Audio timestamps;
- using gain reduction to make residual or noise metrics look better;
- treating a dirty iRig route or missing iRig as a candidate pass;
- claiming DVS readiness while input decode, input labels, or profile readback
  are incomplete.

## Quality Definition

### Simulated quality

Must catch:

- wrong byte order;
- wrong mode-2 cursor;
- gain change;
- polarity inversion;
- pair swap;
- clipping;
- non-deterministic packing.

### Physical quality

Must catch:

- tone sidebands;
- real-music residuals;
- spectral coloration:
  - `low_band_capture_to_ref_gain_db`;
  - `mid_band_capture_to_ref_gain_db`;
  - `high_band_capture_to_ref_gain_db`;
  - `mid_vs_low_coloration_delta_db`;
  - `high_vs_low_coloration_delta_db`;
  - `high_vs_mid_coloration_delta_db`;
  - `metallic_coloration_score_db`;
- click outliers;
- lag jumps;
- clipping;
- high-band harshness;
- 1-5 kHz radio/vinyl texture;
- CPU/window-correlated noise;
- wrong routing;
- DVS input instability.

Mainline absolute coloration floor:

- `mid_vs_low_coloration_delta_db` must stay within `+/-5 dB`;
- `high_vs_low_coloration_delta_db` must stay within `+/-6 dB`;
- `metallic_coloration_score_db <= 6 dB`.

When a valid physical baseline contains the same coloration keys, a candidate
must also stay within baseline + `0.75 dB`.

### Readiness language

Allowed statuses:

```text
PASS
FAIL
NOT_READY
BLOCKED_PHYSICAL_CAPTURE
BLOCKED_DIRTY_ROUTE
BLOCKED_LOCK_BUSY
BLOCKED_USB_ENUMERATION
BLOCKED_IRIG_UNSTABLE
BLOCKED_UNVALIDATED_DVS
SKIPPED_BUSY
```

Not allowed:

- "sounds good" without capture/listening context;
- "DVS ready" without physical scope validation;
- "low CPU" if output quality regresses;
- "fixed" based only on internal counters.

## Implementation Plan

### Milestone 1: types and policies

- `Pair`, `Side`, `ChannelIndex`, `SampleRate`, `FrameCount`;
- `Topology`;
- `RoutingMatrix`;
- `DvsProfile`;
- `EnginePolicy`;
- `QualityStatus`;
- `CaptureReadiness`;
- `SupervisorStatus`;
- `MetricsSnapshot`.

No hardware. Unit tests only.

### Milestone 2: bit-faithful audio core

- `sample` conversion;
- `mode2` pack/unpack;
- golden vectors;
- fixture runner for real music;
- parity comparison with C simulated output.

No hardware. Simulated output only.

### Milestone 3: routing and DVS model

- pair routing matrix;
- profile transactions;
- input-mode/ground/software-lock model;
- DVS readiness state;
- topology tests for A/B/C/D.

No hardware. Surface and state validation only.

### Milestone 4: duplex clock and timeline simulator

- output timeline;
- input capture cursor;
- duplex clock estimator;
- scheduler policy simulator;
- jitter, discontinuity, underrun, and StartIO/StopIO tests.

No hardware. Deterministic simulation.

### Milestone 5: FFI and HAL dry integration

- opaque engine handle;
- versioned config structs;
- caller-owned buffers;
- no panics across FFI;
- compile-only HAL adapter experiment with Rust-specific names.

No install by default.

### Milestone 6: controlled hardware candidate

Requires explicit user approval and the shared hardware lock.

- Rust-specific install identity;
- rollback path;
- internal CPU/UI gate;
- output A/B/C/D smoke;
- input A/B/C/D smoke;
- timecode smoke;
- iRig physical tone/music gates;
- human listening only after automated evidence passes.

## Initial Code Shape

```text
crates/open-a8dj-core/src/
  lib.rs
  engine.rs
  topology.rs
  routing.rs
  sample.rs
  mode2.rs
  duplex_clock.rs
  output_timeline.rs
  input_decoder.rs
  dvs.rs
  controls.rs
  scheduler.rs
  metrics.rs
  sim.rs
```

The first code should be `topology`, `routing`, `sample`, and `mode2`, because
those modules define whether the driver preserves channels and samples before
any USB or HAL integration exists.

## Kill Criteria

Redesign or kill any path that:

- requires mutating mainline;
- requires installing Rust before pure tests exist;
- depends on hidden compile-time behavior flags;
- needs heap allocation or blocking locks in realtime core paths;
- hides input activation from metrics;
- improves CPU by increasing residuals, sidebands, clicks, or timecode
  instability;
- cannot report exact routing/profile/clock policy in artifacts;
- cannot support all A/B/C/D pairs without special cases.

## Summary

The best OpenA8DJ-rust driver is a duplex, cadence-aware, routing-explicit,
measurement-first driver. It should be conservative in public Core Audio timing,
precise in USB scheduling, raw and faithful for timecode, and strict about
quality evidence.

The correct first win is not touching the hardware. It is a Rust core that can
prove sample fidelity, packet correctness, routing correctness, and timeline
stability before it is allowed anywhere near the active Audio 8 DJ.
