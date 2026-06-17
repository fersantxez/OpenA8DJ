# Audio8DJ C++/DriverKit Test Plan

This plan defines how to validate the C++/DriverKit redesign with offline gates
first, then compare it against the mainline C reference and Rust oracle. It does
not authorize physical hardware, Core Audio, USB, install/reload, DriverKit
activation, or human listening work.

## Operating Rules

- Write artifacts only inside `/Users/fer/dev/audio8djcpp`.
- Treat `/Users/fer/dev/opena8dj` and `/Users/fer/dev/audio8djrust` as
  read-only references.
- Do not run hardware, Core Audio, USB, HAL install, DriverKit install, dext
  activation, iRig, Traktor, microphone, speaker, or physical soundcheck tests
  without a separate authorized window and global hardware lock.
- Hardware-sensitive rows in this plan must return `BLOCKED_*` during offline
  work.
- All PASS claims require machine-readable evidence plus a human-readable
  summary.

## Test Lanes

| Lane | Name | Default status offline | Purpose |
|---|---|---|---|
| A | identity/provenance | runnable | freeze candidate and baselines |
| B | build hygiene | runnable | prove offline build integrity |
| C | static policy | runnable | reject unsafe/provenance regressions |
| D | protocol constants | runnable | prove product contract model |
| E | packet parity | runnable | compare C++ packing against C/Rust |
| F | packer throughput | runnable | reject slow hot-path code |
| G | simulated output | runnable | real-music software output quality |
| H | synthetic DVS/timecode | runnable | model Traktor-facing behavior |
| I | surface model | runnable | validate Core Audio/DriverKit schema |
| J | evidence schema | runnable | validate artifact completeness |
| K | physical tone | blocked | future post-DAC tone validation |
| L | physical real music | blocked | future post-DAC music validation |
| M | Traktor operator | blocked | future DVS scope/listening validation |
| N | lifecycle | blocked | hotplug, sleep/wake, install, dext |

## Current Runnable Command

Use this command for the current offline gate set:

```sh
scripts/run-cpp-offline-gates
```

It runs:

- default CMake build + CTest functional contract;
- Release CMake build + CTest functional/performance gates;
- Release benchmark JSON emission.

Current evidence files:

- `local-analysis/cpp-offline/ctest-default.txt`
- `local-analysis/cpp-offline/ctest-release.txt`
- `local-analysis/cpp-offline/packet-matrix.json`
- `local-analysis/cpp-offline/protocol-contract.json`
- `local-analysis/cpp-offline/simulated-output-matrix.json`
- `local-analysis/cpp-offline/mode2-python-oracle.txt`
- `local-analysis/cpp-offline/mode2-cross-oracle-parity.json`
- `local-analysis/cpp-offline/timecode-matrix.json`
- `local-analysis/cpp-offline/dvs-signal-smoke.json`
- `local-analysis/cpp-offline/dvs-packet-input-decode.json`
- `local-analysis/cpp-offline/realtime-audit.json`
- `local-analysis/cpp-offline/driverkit-surface-model.json`
- `local-analysis/cpp-offline/jitter-model.json`
- `local-analysis/cpp-offline/static-policy.json`
- `local-analysis/cpp-offline/hardware-lock-policy.json`
- `local-analysis/cpp-offline/offline-bench-release.json`
- `local-analysis/cpp-offline/evidence-schema.json`

Current PASS coverage:

- surface and channel ordering;
- protocol constants snapshot: VID/PID `0x17cc:0x1978`, endpoints
  `0x01/0x81/0x82/0x06`, 8 input/output channels, required `44100/48000`
  rates, deferred `88200/96000` CAIAQ codes, Mode 2 check cadence `16`
  bytes, and full frame `32` bytes;
- sample format and sample-rate policy;
- identity routing;
- S24 big-endian conversion;
- Mode 2 round-trip for start bytes `0..5`;
- packet matrix for 44.1/48 kHz, transfer bytes `48`, `80`, `352`, gains `1.0` and `0.5`, start bytes `0..5`;
- cross-oracle Mode 2 byte parity against the inherited Python oracle:
  `72` rows, `0` byte mismatches, `0` length deltas, `0` check errors,
  `0` panic flags;
- simulated output matrix for A/B/C/D at 44.1/48 kHz over dense, transient,
  and wideband deterministic material with gains `1.0` and `0.5`: `48` rows,
  `0` failures, minimum SNR `119.407 dB`, max residual ratio `1.07069e-06`,
  max leakage `-240 dBFS`;
- inherited Python Mode 2 oracle;
- timecode profile/deck assignment matrix;
- DVS signal smoke across vinyl/CD-line/phono at 44.1/48 kHz;
- DVS packet input decode across vinyl/CD-line/phono, 44.1/48 kHz, decks A/B/C/D, including playback decode-off behavior;
- realtime hot-path allocation audit;
- DriverKit/CoreAudio surface model;
- jitter/timestamp model plus playback burst cadence rejection for unsafe
  coalesced completion gaps;
- static policy check;
- hardware lock policy check for HAL candidate safety, direct hardware gates,
  and physical soundcheck;
- evidence schema check;
- synthetic pair-A no-leakage;
- Release pack/decode/routing throughput floors.

Current NOT_READY coverage:

- runtime byte parity against real mainline capture/export evidence;
- full Traktor-grade DVS/timecode signal-quality analyzer;
- full runtime jitter measurement under CoreAudio/DriverKit;
- real AudioDriverKit dext stream mapping under DriverKit SDK;
- physical A/B/C/D I/O, Traktor, iRig capture, and listening.

## Baseline Inputs

Use these references for comparisons:

| Baseline | Id | Role |
|---|---|---|
| mainline C/OpenA8DJ | `0.3.135` | minimum internal product bar |
| Rust oracle | `3429796` | strict software/offline oracle |
| C++/DriverKit candidate | current immutable id | candidate under test |

The comparison harness must copy baseline numbers into the C++ evidence
artifact. It must not write into the mainline C or Rust worktrees.

## Lane A: Identity And Provenance

Goal: prove the candidate and comparison inputs are named and reproducible.

Evidence:

- candidate git hash or immutable build id;
- branch name;
- dirty status;
- build/toolchain versions;
- baseline ids and source documents;
- artifact root path;
- policy flags showing no hardware/Core Audio/USB access.

PASS:

- all identity fields are present;
- artifact root is inside `/Users/fer/dev/audio8djcpp`;
- hardware policy is `offline_only`;
- forbidden-worktree write count is `0`.

FAIL:

- candidate id is missing;
- evidence root is outside the C++ worktree;
- hardware, Core Audio, or USB access is attempted.

## Lane B: Build Hygiene

Goal: prove the C++/DriverKit code can build offline without system mutation.

Offline command set should be limited to build-only targets and must be
recorded verbatim in `manifest.json`. Do not include install, HAL reload,
DriverKit activation, USB probe, soundcheck, or Core Audio enumeration commands
in this lane.

PASS:

- compile errors: `0`;
- required test tools build: `0` failures;
- promoted warnings: `0 new warnings`;
- output binary or library hashes recorded;
- no installed system state is changed.

FAIL:

- any compile failure;
- any required target missing;
- any command mutates system driver/audio state.

## Lane C: Static Policy

Goal: reject unsafe design drift before runtime.

Checks:

- no proprietary firmware or binary blobs added;
- no incompatible implementation imports;
- DriverKit entitlement needs are documented as requirements, not assumed;
- offline scripts do not call hardware, Core Audio, USB, install, unload, reset,
  or privileged system mutation paths;
- generated files remain inside the C++ worktree.

PASS: every check is explicit and clean.

FAIL: any unsafe or untracked policy exception.

## Lane D: Protocol Constants Snapshot

Goal: prove the C++/DriverKit model preserves the Audio 8 DJ product contract
without opening the device.

Required constants:

| Field | Expected |
|---|---|
| VID/PID | `0x17cc:0x1978` |
| bulk control OUT/IN | `0x01/0x81` |
| isochronous capture/playback | `0x82/0x06` |
| analog inputs/outputs | `8/8` |
| MIDI input/output | `1/1` |
| input pairs | `Input A/B/C/D` |
| output pairs | `Output A/B/C/D` |
| required rates | `44100`, `48000` |
| extended rates | `88200`, `96000` |

PASS: all constants match or deviations are documented as DriverKit naming
translation only, with no product behavior regression.

FAIL: missing channels, wrong endpoints, wrong pair names, or ambiguous rate
policy.

## Lane E: Packet Parity Matrix

Goal: make C++ packet conversion comparable with mainline C and Rust oracle
before hardware is touched.

Matrix:

| Dimension | Values |
|---|---|
| output pair | `A`, `B`, `C`, `D` |
| rate | `44100`, `48000` |
| transfer bytes | `48`, `80`, `352` |
| start byte | `0`, `1`, `2`, `3`, `4`, `5` |
| byte order | big, native |
| gain | `1.0`, `0.5` |
| source | sine, impulse, ramp, silence, deterministic music fixture |

Minimum coverage must be at least the Rust oracle's 72-row pack-sim matrix.

PASS:

- check errors: `0`;
- panic/assert flags: `0`;
- C++ vs mainline C mismatches: `0`;
- C++ vs Rust oracle mismatches: `0`;
- max absolute sample error: `<= 1 LSB`.

FAIL:

- any missing mandatory row;
- any unexplained mismatch;
- any wrong-pair energy or channel-order regression.

Current implementation status:

- S24 big-endian conversion and Mode 2 round-trip pass in C++.
- All start bytes `0..5` pass.
- Synthetic pair-A no-leakage passes.
- Byte-for-byte comparison against frozen mainline/Rust fixture output is still pending.

## Lane F: Packer Throughput

Goal: reject packet code that cannot plausibly meet low-latency operation.

Metrics:

| Metric | PASS |
|---|---:|
| packed data throughput | `>= 100 MiB/s` |
| frame throughput | `>= 1,000,000 frames/s` |
| hot-loop allocations | `0` |
| hot-loop lock acquisitions | `0` |
| valid-fixture fallback/error count | `0` |

Comparison:

- must record delta versus Rust oracle;
- must record whether the candidate meets or beats the Rust floor;
- must not claim runtime CPU superiority from this lane alone.

Current implementation status:

- Release pack throughput: latest median `1454.94 MiB/s`.
- Release preallocated decode throughput: latest median `546.495 MiB/s`.
- Release routing throughput: latest identity median `854,123,000 frames/s`, reversed median `449,037,000 frames/s`, advanced mute/invert/cross-deck median `441,878,000 frames/s`.
- Allocation count inside the audited pack/decode/routing hot loop: `0`.

## Lane G: Simulated Output Matrix

Goal: prove software output conversion preserves real music before post-DAC
capture is attempted.

Matrix:

| Dimension | Fast gate | Full offline gate |
|---|---|---|
| output pairs | `A` | `A/B/C/D` |
| rates | `48000` | `44100`, `48000` |
| excerpts | dense | dense, transient, wideband |
| gains | `0.5` | `1.0`, `0.5` |
| transfer bytes | `352` | `48`, `80`, `352` where applicable |
| start bytes | `4` | `0..5` where applicable |

Metrics:

| Metric | PASS |
|---|---:|
| `measurement_status` | `VALID` |
| alignment score | `>= 0.995` |
| simulated SNR | `>= 72 dB` |
| 1-5 kHz residual ratio | `<= 0.0010` |
| 1-5 kHz residual level | `<= -105 dBFS` |
| mid-band CPU correlation | `<= 0.02` |
| click outliers | `0` |
| clipped frames | `0` |
| lag jumps over 2 frames | `0` |
| wrong-pair energy | `<= -80 dBFS` |
| channel swap flags | `0` |
| unexpected polarity flags | `0` |

Stretch comparison target: match or beat the Rust oracle values
`alignment=1.000000`, `SNR=75.22 dB`, residual ratio `0.000669`, residual
level `-108.83 dBFS`, and CPU correlation `0.000000`.

FAIL:

- any click, clipping, lag jump, channel swap, wrong-pair leak, or residual
  threshold violation;
- any candidate that passes SNR but fails residual/coloration.

## Lane H: Synthetic DVS/Timecode Matrix

Goal: prove the C++/DriverKit model preserves Traktor-facing routing and
profile behavior in software.

Matrix:

| Dimension | Values |
|---|---|
| rates | `44100`, `48000` |
| profiles | `timecode-vinyl`, `timecode-cd-line`, `phono` |
| input pairs | `A`, `B`, `C`, `D` |
| deck assignments | A->Input A, B->Input B, negative leakage cases |
| control state | input mode, ground-lift, software lock |
| stress model | deterministic jitter/dropout injection |

PASS:

- profile to hardware mode mapping is correct: vinyl `0`, CD/line `1`,
  phono `2`;
- DVS profiles reset remap, left/right swap, and polarity inversion to
  identity;
- Deck A reads Input A only and Deck B reads Input B only;
- C/D do not leak into A/B above threshold;
- synthetic carrier frequency error is `<= 0.5%`;
- synthetic edge jitter p95 is `<= 4 frames`;
- dropout windows are `0`;
- no channel swap appears across simulated start/stop and rate changes.

FAIL:

- wrong control state;
- channel swap;
- leakage above threshold;
- carrier/jitter/dropout regression.

## Lane I: Surface Model

Goal: validate the public device model expected by DJ applications without
querying Core Audio or activating DriverKit.

Expected model:

- device name: `Open Audio 8 DJ`;
- UID: `org.opena8dj.Audio8DJ` or documented DriverKit successor;
- one 8-channel input stream or a justified DriverKit equivalent;
- four stereo output streams: Output A/B/C/D;
- Input A/B/C/D and Output A/B/C/D left/right names;
- 44.1 and 48 kHz required;
- 88.2 and 96 kHz marked extended until physically validated;
- frame and legacy byte buffer-size semantics mapped or explicitly translated;
- input mode, ground-lift, and software-lock controls;
- one MIDI input and one MIDI output endpoint model.

PASS: schema matches the product contract and every naming translation is
documented.

FAIL: missing pair, missing control, missing MIDI model, or ambiguous rate
surface.

## Lane J: Evidence Schema

Goal: make results comparable across C, Rust, and C++/DriverKit.

Required files:

```text
metrics.json
summary.md or summary.txt
manifest.json
comparison.json or comparison.csv
protocol-snapshot.json
packet-parity.json
simulated-output/
synthetic-dvs/
```

Required JSON fields:

```text
schema
candidate
status
policy
baselines
gates
comparison
blocked
artifacts
```

PASS:

- schema is `open-a8djcpp.driverkit-metrics.v1`;
- every required gate has a status;
- every comparable metric includes mainline C and Rust oracle deltas;
- blocked physical gates preserve their blocked reason;
- command list and artifact hashes are recorded.

FAIL:

- missing status;
- missing baseline;
- missing delta for a comparable metric;
- generic `FAIL` used for a blocked hardware precondition.

## Blocked Physical Lanes

These lanes are part of release readiness but must not run during offline QA.

| Lane | Future PASS requirement | Offline status |
|---|---|---|
| physical tone | valid post-DAC capture, sidebands/clicks within threshold | `BLOCKED_HARDWARE_FORBIDDEN` |
| physical real music | valid capture, residual/coloration/click gates pass | `BLOCKED_HARDWARE_FORBIDDEN` |
| physical decorrelated channel matrix | Pair A L/R decorrelated fixture captured through iRig; expected channel gains/polarity valid and opposite-channel leakage below threshold | `BLOCKED_HARDWARE_FORBIDDEN` |
| Traktor scope | stable scope, absolute/relative behavior, no dropouts | `BLOCKED_HARDWARE_FORBIDDEN` |
| MIDI loopback | no dropped bytes, stable endpoints | `BLOCKED_HARDWARE_FORBIDDEN` |
| hotplug | idle/playback recovery without machine restart | `BLOCKED_HARDWARE_FORBIDDEN` |
| sleep/wake | recovery without reinstall/reboot | `BLOCKED_HARDWARE_FORBIDDEN` |
| long run | 8-hour playback, zero underruns | `BLOCKED_HARDWARE_FORBIDDEN` |
| install/dext | signed/authorized install and rollback | `BLOCKED_INSTALL_WINDOW` |

Before any blocked lane can run, the test owner must record:

- authorized time window;
- global hardware lock owner;
- exact device/capture route;
- rollback path;
- stop conditions;
- expected artifacts;
- baseline candidate to compare against.

## Comparison Plan

Normalize every metric into a row:

```text
gate
metric
unit
candidate_value
mainline_c_value
rust_oracle_value
absolute_delta_vs_mainline_c
percent_delta_vs_mainline_c
absolute_delta_vs_rust_oracle
percent_delta_vs_rust_oracle
threshold
comparison_status
notes
```

Rules:

- correctness counts must remain exactly `0`;
- packet parity must match C and Rust with no unexplained mismatches;
- simulated output must meet absolute thresholds and be within the Rust oracle
  tolerance;
- microbenchmarks can reject a candidate but cannot prove runtime CPU;
- hardware/runtime metrics remain `BLOCKED_*` until measured physically;
- if a baseline metric is stale or absent, use `BLOCKED_NO_BASELINE` or
  `NOT_COMPARABLE`, not an invented value.

## Offline Readiness Checklist

Use this checklist before requesting a hardware window.

- [ ] Candidate id and branch are frozen.
- [ ] Dirty state is recorded.
- [ ] Artifact root is inside `/Users/fer/dev/audio8djcpp`.
- [x] No generated files or writes occurred in mainline C or Rust worktrees.
- [x] Mainline C baseline id and values are copied into evidence/docs.
- [x] Rust oracle id and values are copied into evidence/docs.
- [x] Identity/provenance gate passes for the current committed candidate `775de71`.
- [x] Build hygiene gate passes for current CMake/CTest scope.
- [x] Static policy gate passes for the official offline CMake/script/tools path.
- [x] Protocol constants snapshot passes.
- [x] Packet matrix passes for current C++ fixtures.
- [x] Packer throughput meets Rust oracle floors in Release microbench.
- [x] Simulated output fast gate passes.
- [x] Simulated output full offline gate passes.
- [x] Initial DVS signal smoke and DVS packet input-decode gates pass at 44.1 and 48 kHz.
- [x] Surface model gate passes.
- [x] Evidence schema gate passes.
- [ ] Comparison table has all C/Rust/C++ deltas.
- [ ] Physical lanes are explicitly blocked, not passed.
- [x] Hardware-window request includes lock, route, rollback, stop conditions,
  and exact planned tests.

## Stop Conditions

Stop offline QA and return `FAIL` when:

- packet parity diverges from both baselines;
- simulated output has clicks, clipping, wrong-pair energy, channel swap, or
  residual above threshold;
- DVS synthetic routing maps Deck A/B to the wrong input pair;
- evidence cannot identify the candidate or baselines;
- a supposedly offline command attempts hardware, Core Audio, USB, install,
  reload, dext activation, or system mutation.

Stop future physical QA immediately when:

- global hardware lock is unavailable;
- capture route is dirty or ambiguous;
- iRig/Core Audio capture is unstable;
- decorrelated matrix capture clips, records silence, or shows the capture
  route is crosswired before candidate evaluation;
- the installed candidate hash differs from the frozen offline candidate;
- output is audibly unsafe, distorted, or unexpectedly loud;
- rollback command is missing or fails.

## Decorrelated Channel Matrix Gate

Purpose:

- distinguish physical route crosstalk/mix from candidate output/routing bugs;
- avoid drawing crosstalk conclusions from highly correlated stereo music;
- produce a small locked evidence bundle before another full music gate.

Offline preparation:

```sh
make channel-matrix-prepare CHANNEL_MATRIX_PAIR=A CHANNEL_MATRIX_RATE=48000 CHANNEL_MATRIX_SECONDS=8 CHANNEL_MATRIX_PEAK=0.30
```

Future physical command shape, only inside an authorized window and with the
global hardware lock:

```sh
AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" \
  scripts/run-channel-matrix-gate --run-physical \
  --pair A --rate 48000 --seconds 8 --peak 0.30 \
  --capture-device "iRig Stream" --capture-channels 1,2
```

Mainline-parity diagnostic shape for the next C++ A/B, controlling the
`IOProcStreamUsage` harness variable:

```sh
AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" \
  scripts/run-channel-matrix-gate --run-physical \
  --pair A --rate 48000 --seconds 8 --peak 0.30 \
  --capture-device "iRig Stream" --capture-channels 1,2 \
  --no-output-stream-usage
```

Expected artifacts:

- `fixture/reference.wav`
- `fixture/source.json`
- `captured.wav`
- `record.log`
- `play.log`
- `metrics.json`
- `linear-matrix.json`
- `tone-matrix.json`

PASS semantics are intentionally strict and not yet promotion semantics:

- reference L/R correlation must be near zero;
- captured audio must not clip or be silent;
- fitted matrix must be well conditioned;
- expected L->L and R->R terms must dominate;
- opposite-channel leakage must be low enough to explain neither the current
  residual nor audible deck leakage;
- the frequency-domain `tone-matrix.json` is the authoritative crosstalk/routing
  verdict for this fixture;
- `linear-matrix.json` is diagnostic only because analog phase/filtering can
  leave large sample-domain residual even when tone routing is correct;
- if the run fails, do not run Traktor/timecode or branch promotion from that
  candidate.

Current Pair A evidence:

- Earlier `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/tone-matrix.json`
  passed through iRig Stream, but newer same-route evidence supersedes it.
- Current C++ product HAL:
  `local-analysis/channel-matrix/20260617-inputdecode-default-pairA-chmatrix/tone-matrix.json`
  is `FAIL`, with max wrong-source leakage `-35.36 dB`.
- Current mainline `0.3.135` artifact in the same route:
  `local-analysis/channel-matrix/20260617-mainline-pairA-chmatrix/tone-matrix.json`
  is `FAIL`, with max wrong-source leakage `-42.58 dB`.
- The next C++ matrix should run `--no-output-stream-usage` to control the
  harness/CoreAudio difference before attributing the delta to driver routing.
- Current Pair A evidence does not satisfy full A/B/C/D or timecode physical
  matrix requirements.

## Offline Fractional Time-Warp Diagnostic

Purpose:

- test whether failed physical captures are mostly an analyzer/reference
  alignment issue rather than a driver or fixture problem;
- prevent more hardware windows from being spent on alignment-only hypotheses
  after existing WAVs already reject them.

Command shape:

```sh
.venv/bin/python scripts/analyze-fractional-time-warp.py \
  --json-out local-analysis/offline-diagnostics/<run-id>-fractional-time-warp.json \
  <soundcheck-run-dir> [...]
```

Expected artifacts:

- JSON result with `schema=opena8djcpp.fractional-time-warp.v1`;
- per-run scalar and matrix SNR before/after smoothed fractional time-warp;
- per-run delay score, delay span, and mid/high residual ratios.

PASS/FAIL semantics:

- This is a diagnostic PASS if it completes safely over existing evidence.
- It never promotes a candidate by itself.
- A run with `< 3 dB` SNR improvement rejects fractional time-warp as the
  dominant residual explanation.
- A run with `>= 3 dB` improvement requires a fixture/capture validation plan
  before further product claims.

Current status:

- `local-analysis/offline-diagnostics/20260617-fractional-time-warp-multi.json`
  rejects fractional time-warp for the current best C++ captures.
- Next physical work should focus on HAL CPU/transport behavior or a healthier
  same-session fixture reference, not another alignment-only probe.

## Offline Transport Budget Frontier

Purpose:

- make the current quality/CPU/cadence tradeoff explicit and testable;
- reject candidate families that only reduce CPU by increasing USB period and
  audibly damaging quality;
- provide the offline contract for the next DriverKit transport backend before
  any hardware run.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_transport_budget_model
./build/cpp-offline/opena8djcpp_transport_budget_model
```

The full offline gate also runs it automatically:

```sh
scripts/run-cpp-offline-gates
```

Expected artifacts:

- `local-analysis/cpp-offline/transport-budget-model.json`;
- `transport_budget_model` section in
  `local-analysis/cpp-offline/current-offline-gates.json`.

PASS/FAIL semantics:

- PASS means the model classifies all observed physical transport families and
  confirms that no observed family is a product candidate.
- PASS does not mean audio readiness.
- FAIL means the model no longer matches the documented physical frontier and
  the metrics/thresholds must be reconciled before more physical testing.

Next implementation gate:

- add a DriverKit/prepared-transport contract proving the simulated HAL
  steady-state path performs no direct `IOUSBHostPipe` enqueue/requeue work;
- preserve frame order, timestamps, 8 input channels, 8 output channels,
  A/B/C/D routing, and timecode profile semantics.

## Offline Prepared DriverKit Transport Contract

Purpose:

- turn the next CPU architecture hypothesis into a compiled test;
- require `0` HAL steady-state direct USB requeue work in safe scenarios;
- reject designs that hide CPU reduction behind allocations, completion gaps,
  timestamp reorder, routing breakage, or timecode/input loss.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_driverkit_prepared_transport_contract
./build/cpp-offline/opena8djcpp_driverkit_prepared_transport_contract
```

The full offline gate also runs it automatically:

```sh
scripts/run-cpp-offline-gates
```

Expected artifacts:

- `local-analysis/cpp-offline/driverkit-prepared-transport-contract.json`;
- `driverkit_prepared_transport_contract` section in
  `local-analysis/cpp-offline/current-offline-gates.json`.

PASS/FAIL semantics:

- PASS means the offline transport model enforces the intended architecture and
  catches known unsafe variants.
- The contract must be backed by the core `PreparedTransportBackend`; a
  tool-local simulation is no longer sufficient.
- PASS does not mean a dext exists, installs, runs, or beats mainline.
- FAIL blocks any physical prepared-transport candidate.

## Offline Prepared Transport Packet/Ring Contract

Purpose:

- verify that real Mode2 packet packing/decoding can move through
  `PreparedTransportBackend` batch rings without check errors, fallback
  allocations, HAL direct requeues, or channel/timestamp regressions.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_prepared_transport_packet_contract
./build/cpp-offline/opena8djcpp_prepared_transport_packet_contract
```

The full offline gate also runs it automatically:

```sh
scripts/run-cpp-offline-gates
```

Expected artifacts:

- `local-analysis/cpp-offline/prepared-transport-packet-contract.json`;
- `prepared_transport_packet_contract` section in
  `local-analysis/cpp-offline/current-offline-gates.json`.

PASS/FAIL semantics:

- PASS means packet pack/decode and prepared transport rings agree offline.
- PASS does not mean hardware, DriverKit, or physical sound quality is ready.
- FAIL blocks the prepared transport path before physical testing.

## Offline Prepared Transport Routing/Timecode Contract

Purpose:

- verify that playback routing and timecode profile/deck capture behavior still
  hold when routed through `PreparedTransportBackend`.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_prepared_transport_routing_timecode_contract
./build/cpp-offline/opena8djcpp_prepared_transport_routing_timecode_contract
```

The full offline gate also runs it automatically:

```sh
scripts/run-cpp-offline-gates
```

Expected artifacts:

- `local-analysis/cpp-offline/prepared-transport-routing-timecode-contract.json`;
- `prepared_transport_routing_timecode_contract` section in
  `local-analysis/cpp-offline/current-offline-gates.json`.

PASS/FAIL semantics:

- PASS means routing and synthetic timecode profile/deck behavior survive the
  prepared backend path offline.
- PASS does not mean Traktor, hardware, or physical sound quality is ready.
- FAIL blocks the prepared transport path before physical testing.

## Offline Prepared Transport Recovery Contract

Purpose:

- verify lifecycle hygiene around invalid config, stop, restart, counters,
  timestamps, and stale-frame isolation for `PreparedTransportBackend`.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_prepared_transport_recovery_contract
./build/cpp-offline/opena8djcpp_prepared_transport_recovery_contract
```

The full offline gate also runs it automatically:

```sh
scripts/run-cpp-offline-gates
```

Expected artifacts:

- `local-analysis/cpp-offline/prepared-transport-recovery-contract.json`;
- `prepared_transport_recovery_contract` section in
  `local-analysis/cpp-offline/current-offline-gates.json`.

PASS/FAIL semantics:

- PASS means invalid starts fail closed, stopped operations are blocked,
  unstarted safety is not falsely product-safe, restart clears stale frames,
  and counters/timestamp history reset for the new session.
- PASS does not mean hardware recovery, DriverKit recovery, or physical sound
  quality is ready.
- FAIL blocks the prepared transport path before physical testing.

## Offline DriverKit Runtime Bridge Contract

Purpose:

- verify the executable boundary between the DriverKit shell and the prepared
  transport backend without installing or activating a dext.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_driverkit_runtime_contract
./build/cpp-offline/opena8djcpp_driverkit_runtime_contract
```

The full offline gate also runs it automatically:

```sh
scripts/run-cpp-offline-gates
```

Expected artifacts:

- `local-analysis/cpp-offline/driverkit-runtime-contract.json`;
- `driverkit_runtime_contract` section in
  `local-analysis/cpp-offline/current-offline-gates.json`.

PASS/FAIL semantics:

- PASS means the offline DriverKit shell can validate config, start/stop a
  stream, move playback/capture batches through the prepared backend, and fail
  closed after shutdown.
- PASS does not mean an AudioDriverKit dext exists, installs, runs, or beats
  mainline.
- FAIL blocks dext binding work.

## Offline DriverKit Extension Scaffold Contract

Purpose:

- verify that the future AudioDriverKit dext scaffold exists and remains
  non-installing/non-activating by default.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_driverkit_extension_scaffold_contract
./build/cpp-offline/opena8djcpp_driverkit_extension_scaffold_contract
```

The full offline gate also runs it automatically:

```sh
scripts/run-cpp-offline-gates
```

Expected artifacts:

- `local-analysis/cpp-offline/driverkit-extension-scaffold-contract.json`;
- `driverkit_extension_scaffold_contract` section in
  `local-analysis/cpp-offline/current-offline-gates.json`.

PASS/FAIL semantics:

- PASS means the scaffold has required Info.plist, entitlement, IIG, binding,
  and safety markers, and the default build excludes extension sources.
- PASS does not mean a signed/runnable dext exists or that the driver can be
  installed or tested physically.
- FAIL blocks real dext binding work.

## Offline C++ Loopback Quality Analyzer

Purpose:

- provide a compiled, dependency-free signal-quality analyzer that can later be
  used on locked physical loopback captures.
- verify offline that the analyzer accepts clean loopback and rejects degraded
  loopback for measurable reasons.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_loopback_quality_analysis
./build/cpp-offline/opena8djcpp_loopback_quality_analysis
```

For real capture analysis after a locked physical window:

```sh
./build/cpp-offline/opena8djcpp_loopback_quality_analysis \
  --reference-wav /path/to/reference.wav \
  --capture-wav /path/to/captured.wav \
  --min-snr-db 45 \
  --min-correlation 0.98 \
  --max-clicks 0
```

Or for raw f32 interleaved capture:

```sh
./build/cpp-offline/opena8djcpp_loopback_quality_analysis \
  --reference-wav /path/to/reference.wav \
  --captured-f32 /path/to/captured.f32 \
  --sample-rate 48000 \
  --channels 8 \
  --pair 0
```

Expected artifacts:

- `local-analysis/cpp-offline/loopback-quality-analysis.json`;
- `loopback_quality_analysis` section in
  `local-analysis/cpp-offline/current-offline-gates.json`.

PASS/FAIL semantics:

- PASS in selftest mode means the analyzer can distinguish clean synthetic
  loopback from degraded synthetic loopback using SNR, correlation, residual,
  and click metrics.
- PASS does not mean the hardware candidate is audiophile-ready.
- FAIL blocks any physical quality claim until the analyzer or candidate is
  fixed.

## Offline C++ Channel Leakage Tone Contract

Purpose:

- verify a compiled tone-domain no-leakage detector over the real Mode 2
  pack/decode path.
- exercise all A/B/C/D output pairs at 44.1 kHz and 48 kHz without touching
  hardware.
- reject an injected inactive-deck leak so the test proves detection power, not
  only a clean happy path.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_channel_leakage_tone_contract
./build/cpp-offline/opena8djcpp_channel_leakage_tone_contract
```

Expected artifacts:

- `local-analysis/cpp-offline/channel-leakage-tone-contract.json`;
- `channel_leakage_tone_contract` section in
  `local-analysis/cpp-offline/current-offline-gates.json`.

PASS/FAIL semantics:

- PASS means clean digital A/B/C/D rows pass and injected-leak rows fail at
  both validated sample rates.
- PASS does not mean physical deck isolation is proven.
- FAIL blocks physical routing/no-leakage claims until fixed.

## Offline C++ Capture Matrix Quality Analyzer

Purpose:

- analyze stored capture directories with `fixture/reference.wav` and
  `captured.wav` without touching hardware.
- report both loopback quality and decorrelated-tone leakage in one JSON.
- keep routing/leakage evidence separate from full quality readiness.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_capture_matrix_quality_analysis
./build/cpp-offline/opena8djcpp_capture_matrix_quality_analysis
```

For existing stored captures:

```sh
./build/cpp-offline/opena8djcpp_capture_matrix_quality_analysis \
  --analysis-seconds 8 \
  --min-snr-db 45 \
  --min-correlation 0.98 \
  --max-clicks 0 \
  --max-leakage-db -45 \
  --min-expected-amplitude 0.005 \
  /path/to/run-dir
```

Expected artifacts:

- `local-analysis/cpp-offline/capture-matrix-quality-analysis.json`;
- `capture_matrix_quality_analysis` section in
  `local-analysis/cpp-offline/current-offline-gates.json`.

PASS/FAIL semantics:

- PASS in selftest mode means the analyzer accepts a clean synthetic capture
  and rejects a degraded synthetic capture.
- PASS on a stored physical run means that run met the supplied thresholds. It
  does not by itself prove the candidate beats mainline.
- FAIL blocks any claim tied to the failed threshold dimension.

## Locked Hot-Path CPU Attribution Window

Purpose:

- capture callback/hot-path timing for the sustained driver CPU blocker.
- separate process CPU from capture handler, capture requeue, playback queue,
  playback fill, playback enqueue, and playback completion costs.
- avoid changing audio math while gathering attribution.

Pre-window build:

```sh
make hal-hotpath-diagnostic
```

Safety requirements:

- acquire `$HOME/.opena8dj/hardware-gate.lock`;
- explicit user-approved physical window;
- no default-device changes unless separately approved;
- no CoreAudio/USB service restart unless separately approved;
- evidence directory named before starting.

After the diagnostic window:

```sh
make -B hal
```

PASS/FAIL semantics:

- PASS for the diagnostic means evidence contains nonzero hot-path timing
  samples and no cleanup failure.
- It is not a product PASS.
- Product CPU improvement still requires a separate same-session quality and
  CPU run against mainline thresholds.

## Offline Hot-Path Timing Attribution Analyzer

Purpose:

- summarize stored hot-path timing evidence without touching hardware.
- identify whether fixed queue/requeue/enqueue costs or playback fill/math
  dominate the diagnostic timing.
- preserve the rule that nested timings are not summed as total CPU.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_hot_path_timing_analysis
./build/cpp-offline/opena8djcpp_hot_path_timing_analysis
```

Expected artifact:

- `local-analysis/cpp-offline/hot-path-timing-analysis.json`.

PASS/FAIL semantics:

- PASS means the analyzer ran and either found usable stored timing evidence or
  explicitly reported that no stored hot-path timing evidence exists.
- PASS is diagnostic only. It cannot prove product CPU superiority.
- Future CPU improvements must still pass same-session product quality and CPU
  gates.

## Offline Quality Root-Cause Analysis

Purpose:

- combine existing stored evidence into one root-cause classification;
- distinguish clean packed USB payload from failed analog quality;
- block promotion when route health, timebase, physical quality, or CPU remain
  unresolved.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_quality_root_cause_analysis
./build/cpp-offline/opena8djcpp_quality_root_cause_analysis
```

Expected artifact:

- `local-analysis/cpp-offline/quality-root-cause-analysis.json`.
- `local-analysis/cpp-offline/capture-route-health-gate.json`.

Capture route health gate:
- Run as part of `scripts/run-cpp-offline-gates`.
- It consumes existing physical evidence only.
- It must not touch hardware, CoreAudio, USB, drivers, defaults, or services.
- It consumes `direct-usb-path-attribution.json` and exposes
  `direct_usb_capture_failed_after_clean_payload`. This must be `false` before
  any same-route physical result can support promotion.
- It also emits `required_physical_experiments` so the next hardware window is
  explicit and machine-readable.
- Product promotion requires `measurement_valid_for_promotion=true`; otherwise
  the next step is a lock-gated capture route revalidation rather than driver
  promotion.

Prepared DriverKit hot path gate:
- Run as part of `scripts/run-cpp-offline-gates`.
- It exercises the offline DriverKit shell with prepared iso8 batches at
  44.1/48 kHz.
- PASS requires zero HAL requeues/fallback allocations/ring faults and batch
  ring publication budget of max four publications per period.
- This gate supports a migration candidate only; physical CPU superiority still
  requires lock-gated A/B evidence.

Physical evidence frontier:
- Run as part of `scripts/run-cpp-offline-gates`.
- It scans existing soundcheck evidence only.
- It must not touch hardware, CoreAudio, USB, drivers, defaults, or services.
- It reports whether any stored physical run is already a product candidate
  under the strict quality and CPU gates.

Physical capture forensics:
- Run as part of `scripts/run-cpp-offline-gates`.
- It scans existing iRig WAV evidence only and recompares reference/capture
  audio in C++.
- It reports fixed-lag score, per-window lag spread/jumps, static stereo matrix
  explainability, residual level, high-change residual ratio, and a diagnostic
  classification for the best archived runs.
- PASS means analyzer health. Promotion still requires
  `strict_quality_candidates > 0` plus same-session C/mainline comparison.
- Current result: `61` WAV-backed runs, `12` deep analyses, `0` strict
  candidates; best run classified as
  `variable_timebase_or_route_capture_instability`.

Direct USB path attribution:
- Run as part of `scripts/run-cpp-offline-gates`.
- It consumes existing Direct USB diagnostics only.
- It verifies whether written audio, consumed audio, and packed USB payload are
  clean before comparing that against physical iRig capture quality.
- PASS means the diagnostic separation is valid. It does not mean Direct USB or
  the C++ driver is product-ready.
- Current result: latest Direct USB run has clean internal/USB payload
  (`alignment=1.000000`, USB SNR floor `999 dB`, USB errors `0`) while the
  physical iRig capture still fails quality/SNR. This blocks packet/packer
  churn as the next explanation.

iRig idle capture gate:
- Run as part of `scripts/run-cpp-offline-gates`.
- It consumes existing `local-analysis/irig-capture-isolation/**/
  idle-capture-analysis.json` files only.
- PASS means the latest saved iRig idle capture is below idle guardrails. It
  does not prove Audio 8 output quality, mixer route quality, or product
  readiness.
- Current result: latest idle capture passes with max RMS `-66.94 dBFS`, max
  peak `-41.65 dBFS`, and max first-difference RMS `-68.87 dBFS`.
- Next physical test after this gate is known-good non-Audio8 source through
  the same mixer/REC OUT -> iRig route. If that passes, test Audio 8 Pair A
  directly into iRig to isolate the DAC/analog output from the mixer route.

Known-good route soundcheck:
- Physical-only, lock-gated harness:
  `scripts/run-known-good-route-soundcheck`.
- Purpose:
  - validate the shared analog route before evaluating the Audio 8 DJ driver;
  - use a known-good non-Audio8 CoreAudio output as the playback source;
  - capture through the explicit iRig route and analyze the result with the
    same soundcheck quality thresholds.
- Required command shape, inside an authorized hardware window only:

```sh
scripts/run-known-good-route-soundcheck \
  --output-device "<known-good output device name>" \
  --capture-device "iRig Stream" \
  --capture-channels 1,2 \
  --reference-wav /absolute/path/to/reference.wav \
  --seconds 12 \
  --run-dir local-analysis/known-good-route/<timestamp>-known-good-irig
```

- The output source must not be OpenA8DJ, Open Audio 8 DJ, or Audio 8 DJ.
- The wrapper must not change default devices, sample rate, CoreAudio services,
  USB state, HAL install/load state, or DriverKit state.
- Evidence must include:
  - `metrics.json` from the historical Python analyzer;
  - `native-quality.json` from the compiled C++ WAV analyzer;
  - `summary.txt`, `play.log`, `record.log`, `analysis.txt`, and
    `analysis.err`.
- PASS means the selected known-good source plus shared capture route met the
  soundcheck thresholds for that run.
- FAIL blocks driver quality comparison until the physical route is fixed or a
  cleaner route is selected.
- Current route blocker:
  - A route-only attempt with `iRig Stream` as both playback source and capture
    device failed: captured RMS stayed near `-68.1 dBFS` while the reference was
    around `-21.2/-22.1 dBFS`, with alignment about `0.004`.
  - That source must not be used as promotion-quality known-good evidence.
  - The next known-good attempt needs a real non-Audio8 output physically wired
    into the same mixer/REC OUT -> iRig capture path.

Physical superiority window runner:
- Script:
  `scripts/run-physical-superiority-window`.
- Default mode is dry-run plan only. Physical work requires `--execute`.
- Full execution starts with a read-only preflight:
  `scripts/physical-window-preflight`.
- The preflight must PASS before the hardware lock is acquired. It checks:
  - iRig capture visibility in CoreAudio;
  - Audio 8 DJ visibility on USB;
  - explicit mainline and C++ HAL bundle paths for full A/B execution;
  - reference/music files;
  - explicit non-Audio8 known-good output visibility;
  - hardware lock availability.
- `Open Audio 8 DJ` may be absent from CoreAudio before HAL loading, but if
  present it must expose `8 in / 8 out`.
- Full execution order:
  - run read-only physical-window preflight;
  - acquire the global hardware lock;
  - run known-good non-Audio8 route soundcheck into iRig;
  - install/reload the explicit read-only mainline HAL candidate through
    `scripts/test-hal-candidate-safety`;
  - run mainline Audio 8 DJ soundcheck with CPU profile and stream snapshots;
  - unload the mainline HAL candidate;
  - install/reload the explicit C++ HAL candidate through
    `scripts/test-hal-candidate-safety`;
  - run C++ Audio 8 DJ soundcheck with CPU profile and stream snapshots;
  - run `opena8djcpp_soundcheck_wav_quality` on both captured runs;
  - compare same-session mainline vs C++ physical metrics with
    `opena8djcpp_physical_run_compare`;
  - run `scripts/evaluate-promotion-readiness.py` against the new metrics;
  - unload the active HAL candidate unless `--leave-loaded` is explicit.
- Required physical command shape:

```sh
scripts/run-physical-superiority-window \
  --execute \
  --mainline-candidate /absolute/path/to/mainline/OpenA8DJ.driver \
  --candidate build/OpenA8DJ.driver \
  --known-good-output-device "<non-Audio8 output>" \
  --capture-device "iRig Stream" \
  --capture-channels 1,2 \
  --reference-wav /absolute/path/to/reference.wav \
  --music-file /absolute/path/to/music.wav \
  --seconds 12 \
  --run-dir local-analysis/physical-superiority-window/<timestamp>
```

- PASS for this runner is still not product readiness by itself. Readiness
  requires the same-session C++ vs mainline comparison to pass, the promotion
  evaluator to allow promotion, and the evidence to beat mainline thresholds
  for quality, CPU, routing, recovery, and timecode.
- `--candidate-only` is diagnostic only. It must produce a blocked
  same-session comparison result and must not support readiness or promotion.
- `--skip-known-good` is diagnostic only. It must block a successful runner
  exit because the shared physical capture route was not revalidated.

PASS/FAIL semantics:

- `same-session-physical-compare.json result=PASS` means C++ beat the explicit
  same-session mainline run on the comparator gates for that window.
- `branch_promotion_supported=true` is allowed only when the comparator used an
  explicit baseline run, not fixed historical thresholds.
- `scripts/evaluate-promotion-readiness.py` must consume the same-session
  compare JSON from the same physical window as the C++ soundcheck.
- The runner exits success only when known-good route validation, same-session
  comparison, and promotion evaluation all pass.
- Preflight PASS is not route proof. The known-good route capture must still
  pass after lock acquisition.

## Offline Timecode Readiness Gate

Purpose:

- aggregate timecode matrix, synthetic signal, DVS packet decode, and prepared
  transport profile/deck evidence;
- preserve an explicit distinction between offline PASS and physical Traktor
  vinyl readiness.

Command shape:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline --target opena8djcpp_timecode_readiness_gate
./build/cpp-offline/opena8djcpp_timecode_readiness_gate
```

Expected artifact:

- `local-analysis/cpp-offline/timecode-readiness-gate.json`.

PASS/FAIL semantics:

- Tool `result=PASS` means the aggregate diagnostic ran.
- `offline_timecode_pass=true` means synthetic/offline DVS contracts pass.
- `product_timecode_ready=false` blocks any Traktor/timecode-vinyl readiness
  claim until physical lock evidence exists.
