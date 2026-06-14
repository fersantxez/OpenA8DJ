# OpenA8DJ-rust Product Plan

This is the product-management plan for the parallel Rust implementation. It
turns the goal into outcomes, constraints, workstreams, agent responsibilities,
promotion gates, and kill criteria.

## Product Role

The product manager for OpenA8DJ-rust owns the system of human and agent work:

- selecting the right problem;
- defining what "good" means;
- giving agents enough product and organizational context;
- exploring viable implementations in parallel;
- evaluating against real DJ/audio behavior, not merely whether code builds;
- driving rollout only after evidence supports trust.

The PM does not let the Rust project interfere with active mainline C
investigation. The Rust effort learns from mainline evidence, but it has its own
plan, tests, agents, and promotion criteria.

## Mission

Build `OpenA8DJ-rust`: a modern Rust-based Audio 8 DJ driver stack that is
audibly excellent, complete for real Audio 8 DJ workflows, fast, stable, and
maintainable enough to become the long-term driver architecture.

The product target is not "Rust parity." The target is a better driver:

- audiophile playback and capture quality;
- full 8 input / 8 output Audio 8 DJ behavior;
- Traktor/timecode vinyl and CD-line workflows;
- MIDI and hardware controls;
- low CPU and resource use;
- robust start/stop, hotplug, sleep/wake, sample-rate changes, and multi-client
  use;
- codebase clarity suitable for long-term maintenance and DriverKit migration.

## Absolute Constraints

- Work only under `/Users/fer/dev/audio8djrust`.
- The mainline C worktree `/Users/fer/dev/opena8dj` is read-only reference
  material.
- Rust agents may read mainline source, docs, quality logs, and test artifacts,
  but must not mutate the mainline tree.
- No Rust command may install, unload, replace, reset, or disturb the active
  HAL driver or hardware state without explicit user approval.
- Mainline physical tests are evidence inputs, not blockers. Rust continues
  with simulation and parity work when physical capture is unavailable.
- User-visible Rust names must include `-rust`.

## Nomenclature

Use these names consistently:

- Project: `OpenA8DJ-rust`
- Branch family: `rust/*`
- Worktree: `/Users/fer/dev/audio8djrust`
- Driver bundle candidate: `OpenA8DJ-rust.driver`
- Device display name for experiment builds: `Open Audio 8 DJ-rust`
- Device UID for experiment builds: `org.opena8dj.rust.Audio8DJ`
- CLI tools: `opena8dj-rust-control`, `opena8dj-rust-pack-sim`,
  `opena8dj-rust-metrics`
- Local artifacts: `local-analysis-rust/...`
- Labels: `<version>-rust-<candidate>`

Never publish or install a Rust artifact with an ambiguous mainline name.

## Definition Of Success

OpenA8DJ-rust is successful only when it beats or matches the mainline on all
four product axes.

### 1. Audio quality

Required outcomes:

- music playback is clean, full-bandwidth, stable, and free of metallic/radio,
  crackle, pitch, or CPU/window-correlated artifacts;
- analog loopback has valid measurement status and passes candidate quality;
- no clicks, lag jumps, high-band residual spikes, or sidebands beyond accepted
  thresholds;
- output quality holds across Output A/B/C/D;
- capture quality holds across Input A/B/C/D.

Non-negotiable gate:

- no human listening request and no readiness claim unless physical route status
  is valid and physical tone/music gates pass.

### 2. Functional completeness

Required outcomes:

- 8 input / 8 output Core Audio surface;
- four stereo output streams A/B/C/D;
- one safe 8-channel input surface with Input A/B/C/D pair names unless a later
  DriverKit architecture proves a better shape;
- 44.1 and 48 kHz production quality;
- 96 kHz production quality before 1.0;
- 88.2 kHz hidden unless proven;
- stable Traktor timecode vinyl at 44.1 and 48 kHz;
- timecode CD-line and phono/control profiles;
- MIDI in/out endpoints;
- hardware controls for input mode, ground lifts, software lock, and status.

### 3. Performance and stability

Required outcomes:

- no allocation, logging, filesystem I/O, Objective-C messaging, or blocking
  mutexes in realtime Rust core paths;
- low steady-state driver CPU during playback and duplex operation;
- low idle CPU while open-but-not-streaming;
- fast warm start without keeping risky ISO streaming alive after StopIO;
- no timeline resets, active underruns, output panic flags, or playback queue
  failures in internal gates;
- stable under UI/WindowServer stress, Traktor, system sounds, and microphone
  activation scenarios.

### 4. Maintainability

Required outcomes:

- protocol and realtime logic live in deterministic Rust modules;
- Apple APIs are isolated in platform adapters;
- behavior is configured by versioned runtime policy structs, not hidden
  compile-time flag forests;
- metrics are structured JSON, not human text scraping;
- unsafe Rust is isolated and documented;
- every major subsystem has simulation, parity, and regression tests.

## Quantitative Acceptance Targets

These targets are intentionally strict. They define "good" for
OpenA8DJ-rust, not just "working".

### Internal playback CPU/UI gate

- `device-start <= 0.25s`.
- `first-callback <= 0.30s`.
- driver CPU average `<= 10%`.
- driver CPU p95 `<= 12%`.
- stress driver CPU average `<= 10%`.
- stress driver CPU p95 `<= 12%`.
- `coreaudiod` p95 `<= 8%`.
- WindowServer p95 during UI stress `<= 45%`.
- `outputFramesWritten > 0`.
- `outputFramesRead >= 0.90 * outputFramesWritten`.
- `outputTimelineResets=0`.
- `outputActiveUnderruns=0`.
- `outputPanicFlags=0`.

### Physical tone gate

- capture peak between `0.020` and `0.920`.
- `sideband_ratio <= 0.008`.
- segment sideband p95 `<= 0.006`.
- segment sideband max `<= 0.008`.
- `click_outliers=0`.
- segment click rate `0`.

### Physical real-music gate

- `measurement_status=VALID`.
- `candidate_quality_status=PASS`.
- `verdict=PASS`.
- alignment `>= 0.970`.
- capture RMS between `-28` and `-10 dBFS`.
- clipped frames `0`.
- 1-5 kHz residual `<= 1.38`.
- 1-5 kHz residual window p95 `<= 1.40`.
- 1-5 kHz residual window max `<= 1.46`.
- 1-5 kHz p95/median `<= 1.03`.
- 1-5 kHz max/median `<= 1.06`.
- 5-12 kHz residual `<= 1.32`.
- mid-vs-low coloration within `+/-5 dB`.
- high-vs-low coloration within `+/-6 dB`.
- metallic score `<= 6 dB`.
- quiet 1-5 kHz noise `<= -32.5 dBFS`.
- clicks `0`.
- window clicks `0`.
- lag jumps greater than 2 frames `<= 3`.
- CPU/noise correlation `<= 0.08`.
- driver CPU average `<= 8%`.
- driver CPU p95 `<= 12%`.
- `coreaudiod` p95 `<= 8%`.
- candidate does not regress against the current valid physical baseline.

### Hard blockers

Any of these blocks promotion:

- missing iRig;
- dirty or unverified capture route;
- diagnostic-only bypass;
- missing CPU profile;
- stale preflight hash;
- any tone click;
- any physical music click;
- sideband breach;
- residual breach;
- CPU/noise correlation breach;
- high driver or `coreaudiod` CPU;
- `outputFramesRead=0`;
- timeline reset;
- active underrun;
- `coreaudiod >100%`;
- `audio-list` hang;
- output-only or input-disabled build claiming DVS readiness.

## Product Pillars

### Pillar A: Audiophile output path

Focus:

- mode-2 packing correctness;
- timeline/drift policy;
- packet cadence;
- sample conversion;
- click-free underrun recovery;
- route-verifiable analog quality.

Key product question:

Can OpenA8DJ-rust preserve real music through Audio 8 DJ output with no audible
CPU/window-correlated artifact and no measurable residual regression?

### Pillar B: Complete DJ workflow

Focus:

- Traktor-facing channel topology;
- timecode vinyl and CD-line profile correctness;
- input decode and channel isolation;
- MIDI and control bridge;
- A/B/C/D physical routing.

Key product question:

Can a Traktor user select OpenA8DJ-rust and use Audio 8 DJ as the original
hardware workflow intended, without channel surprises?

### Pillar C: Realtime stability and low resource use

Focus:

- preallocated transfer pools;
- lock-free audio rings;
- bounded atomics;
- stable start/stop state machine;
- safe open-but-not-streaming idle policy.

Key product question:

Can the driver remain boring under long playback, hotplug, sleep/wake,
multi-client use, and UI stress?

### Pillar D: Modern maintainable architecture

Focus:

- Rust core separated from macOS adapter;
- HAL experiment now, DriverKit path later;
- versioned policies and metrics;
- reproducible simulations;
- clear agent ownership.

Key product question:

Can a new contributor reason about the driver without reading a 5000-line
monolith?

## Workstreams

### WS0: Product governance

Owner: PM / integration lead.

Deliverables:

- this product plan;
- agent operating prompts;
- promotion gate checklist;
- decision log;
- risk register;
- mainline evidence intake notes.

Exit:

- every agent has a role, write scope, and no-interference policy.

### WS1: Rust core foundation

Owner: Rust Core Agent.

Deliverables:

- Cargo workspace;
- `open-a8dj-core`;
- sample format types;
- channel topology types;
- versioned policy structs;
- metrics schema.

Exit:

- unit tests run without hardware;
- all public types are documented enough for FFI.

### WS2: CAIAQ protocol and packing parity

Owner: Protocol Agent.

Deliverables:

- Audio 8 DJ constants;
- CAIAQ commands;
- sample-rate and bytes-per-packet model;
- `f32` to signed 24-bit big-endian conversion;
- mode-2 output packing;
- input unpacking/check-byte validation;
- golden vectors against mainline behavior.

Exit:

- Rust simulated packing matches known-good C simulation for representative
  music fixtures and edge cases.

### WS3: Output timeline and transport scheduler

Owner: Realtime Agent.

Deliverables:

- output timeline state machine;
- jitter smoothing;
- bounded elastic correction;
- replay/fade recovery policy;
- playback request planner;
- target in-flight depth controller;
- deterministic drift simulator.

Exit:

- simulation proves bounded behavior under discontinuities, underruns, late
  writes, bursty capture completions, and sample-rate changes.

### WS4: Input, DVS, and controls

Owner: DJ Workflow Agent.

Deliverables:

- input decode state machine;
- A/B/C/D input routing;
- profile model for timecode vinyl, CD-line, and phono;
- ground-lift and software-lock model;
- MIDI/control protocol model.

Exit:

- non-physical timecode surface tests can validate profile application,
  channel naming, input decode, and restore behavior.

### WS5: Platform adapter

Owner: Platform Adapter Agent.

Deliverables:

- `open-a8dj-ffi` C ABI;
- optional HAL adapter experiment;
- future DriverKit adapter design;
- explicit install guardrails.

Exit:

- compile-only adapter exists without installing or touching active HAL state;
- no platform code leaks into `open-a8dj-core`.

### WS6: QA and evidence system

Owner: QA Evidence Agent.

Deliverables:

- `local-analysis-rust` layout;
- JSON metrics format;
- parity comparison tool;
- no-sound simulated-output gate;
- imported mainline evidence notes;
- promotion dashboard format.

Exit:

- Rust can say `PASS`, `FAIL`, or `BLOCKED` with the same rigor as mainline.

## Architecture Attack Plan

OpenA8DJ-rust should not start by porting the full HAL. It should replace the
highest-risk pure logic first, while Apple APIs remain platform adapter code.

### Replace first

1. `open-a8dj-core::sample`
   - signed 24-bit big-endian conversion;
   - clipping and saturation policy;
   - gain policy;
   - golden edge cases for silence, max/min, NaN, and over-range samples.

2. `open-a8dj-core::mode2`
   - mode-2 output packing;
   - check-byte validation;
   - input unpacking;
   - output start-byte policy;
   - pair/channel order.

3. `open-a8dj-core::timeline`
   - output timeline ring;
   - sample-time smoothing;
   - late-write handling;
   - startup silence;
   - elastic drops;
   - bounded replay/fade recovery.

4. `open-a8dj-core::scheduler`
   - capture-paced request planning;
   - playback coalescing;
   - in-flight depth bounds;
   - explicit schedule fallback;
   - transfer layout validation.

5. `open-a8dj-core::metrics` and `open-a8dj-ffi`
   - versioned metrics snapshots;
   - versioned control payloads;
   - struct size checks;
   - one ABI definition shared by driver and tools.

6. `open-a8dj-core::controls`
   - input modes;
   - ground lifts;
   - software lock;
   - input transforms;
   - source routing;
   - input decode activation policy.

### Keep as adapter code for now

- Core Audio HAL COM-style entrypoints;
- property handling;
- object IDs;
- `StartIO` / `StopIO`;
- `GetZeroTimeStamp`;
- factory export;
- IOUSBHost object ownership;
- pipe enqueue APIs;
- Objective-C blocks;
- dispatch queues;
- USB device seize/configuration;
- install/signing/notarization;
- `coreaudiod` restart and hardware QA commands.

### Invariants

- no heap allocation in realtime core calls;
- no blocking mutex in realtime core calls;
- no logging or file I/O in realtime core calls;
- no Objective-C message send in realtime core calls;
- no panic across FFI;
- no unbounded external-state loop;
- public Core Audio time remains stable and monotonic;
- USB frame timing may inform internal transport pacing but never rewrites the
  public Core Audio timeline;
- sample rates are represented explicitly as `44100`, `48000`, `88200`, and
  `96000`, with production exposure controlled by gates;
- channel topology is 8 channels and 4 stereo pairs;
- capture transaction anomalies are counted states, not silent recovery;
- output timeline recovery is bounded and measurable;
- every checked-out transfer is queued or released once;
- playback in-flight depth cannot underflow or exceed configured max depth;
- metrics separate transport health from audio quality.

## Agent Model

Every agent gets this mandatory instruction:

> You are working in `/Users/fer/dev/audio8djrust` on branch
> `rust/modular-core-spike`. `/Users/fer/dev/opena8dj` is strictly read-only
> reference material. It is forbidden to modify, format, install from, clean,
> reset, or generate files into the main worktree.

### Standing agent roles

- PM / Integration Lead: owns product plan, prioritization, gate decisions,
  cross-agent integration, final readiness claims.
- C Reference Agent: reads C/Objective-C mainline and extracts invariants,
  constants, failure modes, and behavior.
- Rust Core Agent: owns `crates/open-a8dj-core`.
- Protocol Agent: owns CAIAQ, conversion, packing, unpacking, and golden tests.
- Realtime Agent: owns timeline, rings, scheduling policy, and simulators.
- DJ Workflow Agent: owns I/O topology, timecode, profiles, MIDI/control model.
- QA Evidence Agent: owns metrics schema, artifacts, parity reports, and gates.
- Platform Adapter Agent: owns FFI/HAL/DriverKit adapter boundaries.
- Release Safety Agent: owns naming, install guards, rollback rules, and
  no-interference audits.

### Delegation rules

- Agents receive disjoint file ownership for coding tasks.
- Explorers may read both worktrees but write nowhere.
- Workers may write only in `/Users/fer/dev/audio8djrust`.
- No agent gets permission to install, reset USB, touch `/Library`, or modify
  mainline.
- If an agent discovers a needed mainline fact, it writes a Rust-side note with
  source path, observed fact, confidence, and whether it came from code, metric,
  or listening evidence.

## Roadmap

### Phase 0: Product and architecture lock

Status: in progress.

Deliverables:

- `AGENTS.md`;
- `docs/RUST_DRIVER_ARCHITECTURE.md`;
- `docs/RUST_PRODUCT_PLAN.md`;
- initial agent model;
- initial no-interference rules.

Promotion:

- Rust planning committed;
- mainline untouched.

### Phase 1: Pure Rust core and parity fixtures

No hardware, no install.

Deliverables:

- Cargo workspace;
- `open-a8dj-core`;
- conversion tests;
- mode-2 packing tests;
- input unpacking tests;
- topology and policy structs;
- metrics schema.

Promotion:

- `cargo test` passes;
- Rust pack simulator matches C behavior on fixtures;
- no platform or HAL dependency in core.

### Phase 2: Output timeline simulator

No hardware, no install.

Deliverables:

- timeline state machine;
- SPSC ring;
- drift/underrun simulator;
- metric snapshots;
- property tests for discontinuities.

Promotion:

- no unbounded reset loop;
- bounded recovery under late writes and jitter;
- explicit counters for every recovery behavior.

### Phase 3: Rust simulated-output gate

No hardware required.

Deliverables:

- `opena8dj-rust-pack-sim`;
- decoded-output comparison;
- integration with real music fixtures in Rust-side output dirs;
- parity report against mainline simulated output.

Promotion:

- no gain/polarity/channel/packing mismatch;
- residual metrics match or improve simulated baseline;
- artifacts stay under `local-analysis-rust`.

### Phase 4: HAL adapter dry integration

Compile and smoke only. No install by default.

Deliverables:

- `open-a8dj-ffi`;
- optional HAL adapter using Rust core;
- renamed Rust bundle target `OpenA8DJ-rust.driver`;
- smoke checks that do not replace active mainline HAL.

Promotion:

- compile and local bundle smoke pass;
- no `/Library` mutation;
- device names and UIDs are Rust-specific.

### Phase 5: Controlled hardware candidate

Requires explicit user approval.

Deliverables:

- signed/ad-hoc Rust experiment bundle;
- exact install command and rollback path;
- version/hash identity capture;
- internal playback CPU/UI gate;
- output-pair smoke;
- timecode smoke;
- physical bench sanity;
- physical tone/music gate if capture route is valid.

Promotion:

- `audio_stack_health=PASS`;
- no timeline resets, active underruns, panic flags, or queue failures;
- timecode smoke pass;
- output-pair A/B/C/D pass;
- physical `measurement_status=VALID`;
- `candidate_quality_status=PASS`;
- human listening confirms quality.

### Phase 6: DriverKit productization

Only after HAL-hosted Rust proves value.

Deliverables:

- DriverKit entitlement plan;
- app/system extension architecture;
- installer/update/uninstall UX;
- diagnostics export;
- signed release path.

Promotion:

- clean install/upgrade/uninstall;
- 8-hour playback;
- hotplug and sleep/wake;
- full Traktor/timecode physical matrix;
- production-quality rates and channel isolation.

## Promotion Gates

### Gate A: No-interference

Must pass before every Rust milestone:

- mainline worktree unchanged by Rust task;
- no write paths into `/Users/fer/dev/opena8dj`;
- no `/Library` or LaunchAgent mutation;
- no hardware reset command.

### Gate B: Build/test

- `cargo fmt --check`;
- `cargo clippy -- -D warnings` once code exists;
- `cargo test`;
- FFI header generation check once FFI exists;
- no panics across FFI.

### Gate C: Parity

- conversion parity;
- mode-2 packing parity;
- input unpacking parity;
- channel map parity;
- policy snapshot includes all runtime parameters.

### Gate D: Internal runtime

- start latency target recorded;
- first callback latency recorded;
- driver/coreaudiod CPU target recorded;
- no timeline resets;
- no active underruns;
- no output panic flags;
- no queue failures;
- timecode smoke pass.

### Gate E: Physical quality

- capture device visible in USB and Core Audio;
- capture route verified;
- no clipping or dirty route;
- `measurement_status=VALID`;
- tone sidebands pass;
- real-music residual/click/lag metrics pass;
- CPU/noise correlation acceptable;
- human listening pass.

### Gate F: Readiness status

`READY_FOR_HUMAN_TEST` is allowed only when:

- the latest PASS preflight hash matches the currently installed Rust hash;
- all internal gates passed on that hash;
- all physical gates passed with `measurement_status=VALID`;
- the Rust candidate has a Rust-specific bundle name, display name, UID, and
  tool names;
- rollback path is recorded.

`READY_FOR_RELEASE` additionally requires:

- full A/B/C/D output physical validation;
- full A/B/C/D input physical validation;
- Traktor timecode vinyl physical scope validation;
- CD-line/timecode and phono profile validation;
- MIDI loopback validation;
- hotplug and sleep/wake validation;
- installer/uninstaller validation;
- legal/provenance review.

## Kill Criteria

Kill or redesign an implementation path if:

- it needs to mutate mainline to make progress;
- it cannot express behavior without hidden compile-time flags;
- it requires locks or allocation in realtime core calls;
- it gets clean internal counters but fails physical music quality;
- it improves CPU while worsening residuals, sidebands, or clicks;
- it depends on a fragile HAL topology that breaks Traktor/timecode;
- it cannot provide rollback-safe install semantics.

## Decision Framework

When deciding between implementations, rank in this order:

1. Valid physical audio quality.
2. Traktor/timecode functional correctness.
3. Realtime safety and stability.
4. Low CPU/resource use.
5. Maintainability and simplicity.
6. Speed of implementation.

Fast implementation never outranks valid audio evidence.

## Mainline Evidence Intake

Rust consumes mainline learnings through Rust-side notes and tests:

- source path or artifact path;
- version/build/hash if applicable;
- physical route if applicable;
- metric values;
- listening result if applicable;
- confidence level;
- Rust implication.

Example:

```text
evidence_id: mainline-2026-06-14-0.3.133-internal
source: /Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md
fact: 0.3.133 passed internal CPU/output/timecode gates but remained blocked
      by missing iRig physical capture.
rust_implication: internal gates cannot promote OpenA8DJ-rust without valid
                  physical measurement.
```

## Immediate Next Steps

1. Add `docs/RUST_PRODUCT_PLAN.md`.
2. Add an agent prompt library for the standing roles.
3. Create Cargo workspace skeleton.
4. Implement `open-a8dj-core::sample`.
5. Implement `open-a8dj-core::mode2`.
6. Add `opena8dj-rust-pack-sim`.
7. Create `local-analysis-rust/README.md` and artifact schema.
8. Build the first parity report against read-only mainline behavior.

The first code milestone is not a driver install. It is a deterministic Rust
core that can prove it understands Audio 8 DJ data better than the current
monolithic implementation.
