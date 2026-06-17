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

- `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/tone-matrix.json`
  is `PASS` through iRig Stream.
- This does not satisfy the full A/B/C/D or timecode physical matrix.
