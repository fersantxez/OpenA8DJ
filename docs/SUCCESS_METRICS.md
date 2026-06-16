# Audio8DJ C++/DriverKit Success Metrics

This document defines the offline success gates for the C++/DriverKit redesign.
It is intentionally comparable with the current mainline C/OpenA8DJ reference
and the Rust oracle contract, while keeping physical hardware, Core Audio,
USB, install/reload, and listening gates out of scope unless a separate
authorized hardware window exists.

## Scope

Allowed by this contract:

- offline build, parse, static, fixture, simulation, and artifact checks;
- software-only packet packing, decoding, routing, and DVS/timecode analysis;
- comparison against recorded mainline C and Rust oracle metrics;
- evidence generation under an authorized run directory in the C++ worktree.

Not allowed by this contract:

- opening, claiming, probing, resetting, or streaming the Audio 8 DJ hardware;
- touching Core Audio, AudioServerPlugIn, `coreaudiod`, USB, DriverKit install,
  dext activation, system extension activation, or HAL install/reload;
- physical soundcheck, iRig capture, microphone capture, Traktor operator
  testing, hotplug, sleep/wake, or human listening;
- writing artifacts into `/Users/fer/dev/opena8dj` or
  `/Users/fer/dev/audio8djrust`.

Any hardware-sensitive gate must report a blocked status, not PASS.

## Baseline References

### Mainline C Reference

Current mainline C/OpenA8DJ internal reference: `0.3.135`.

Recorded internal characteristics:

| Metric | Mainline C reference |
|---|---:|
| device start latency | about `0.094s` |
| first callback latency | about `0.101s` |
| driver CPU p95 | about `6.5%` |
| `coreaudiod` p95 | about `1.7%` |
| stress driver CPU p95 | about `6.0%` |
| stress `coreaudiod` p95 | about `1.5%` |
| timeline resets | `0` |
| active underruns | `0` |
| elastic drops/replays | `0/0` |
| late write frames/batches | `0/0` |
| playback completion outliers | `0` |
| capture-to-playback queue outliers | `0` |

Mainline C is the minimum internal product bar. C++/DriverKit cannot claim
readiness from cleaner architecture alone if these product counters, routing
semantics, or output-quality proxies regress.

### Rust Oracle Reference

Current Rust oracle candidate: `3429796`.

Recorded offline/software oracle facts:

| Metric | Rust oracle |
|---|---:|
| packer throughput floor | `>= 100 MiB/s` |
| packer frame throughput floor | `>= 1,000,000 frames/s` |
| simulated output alignment | `1.000000` |
| simulated output SNR | `75.22 dB` |
| simulated 1-5 kHz residual ratio | `0.000669` |
| simulated 1-5 kHz residual level | `-108.83 dBFS` |
| simulated CPU/noise correlation | `0.000000` |
| pack-sim matrix size | `72 rows` |
| pack-sim check errors | `0` |
| pack-sim panic flags | `0` |
| pack-sim mismatches | `0` |

Recorded Rust runtime oracle facts from three locked, real-music stress runs:

| Metric | Rust oracle band |
|---|---:|
| device start latency | `0.025478s..0.026860s` |
| first callback latency | `0.032509s..0.033130s` |
| driver CPU p95 | `6.7%..6.9%` |
| stress driver CPU p95 | `5.8%..6.2%` |
| `coreaudiod` p95 | `1.5%` |
| stress `coreaudiod` p95 | `1.3%..1.4%` |
| timeline resets | `0` |
| active underruns | `0` |
| output panic flags | `0` |
| elastic drops/replays | `0/0` |

For offline C++/DriverKit gates, Rust is the strict oracle wherever the metric
is software-only and directly comparable. For runtime and physical metrics,
Rust is a future comparison target but cannot be evaluated without hardware
authorization.

## Status Vocabulary

Use only these status values in C++/DriverKit metrics artifacts:

```text
PASS
FAIL
NOT_READY
BLOCKED_LOCK_BUSY
BLOCKED_HARDWARE_FORBIDDEN
BLOCKED_PHYSICAL_CAPTURE
BLOCKED_DIRTY_ROUTE
BLOCKED_USB_ENUMERATION
BLOCKED_IRIG_UNSTABLE
BLOCKED_UNVALIDATED_DVS
BLOCKED_STALE_HASH
BLOCKED_DRIVERKIT_ENTITLEMENT
BLOCKED_INSTALL_WINDOW
BLOCKED_NO_BASELINE
SKIPPED_BUSY
SKIPPED_NOT_APPLICABLE
```

Semantics:

- `PASS`: every required metric for the named offline gate is present and
  within threshold.
- `FAIL`: a required metric is present and violates threshold, or required
  evidence is missing from an otherwise runnable offline gate.
- `NOT_READY`: required lower-level gates have not run or have not passed.
- `BLOCKED_*`: the gate is valid but cannot run because a precondition outside
  the offline scope is absent.
- `SKIPPED_BUSY`: a hardware lock or authorized window is busy and the gate was
  intentionally not attempted.
- `SKIPPED_NOT_APPLICABLE`: the gate is outside the claim being made.

Never convert a blocked hardware-sensitive gate into `PASS`.

## Offline Gate Summary

| Gate | Required for offline readiness | Hardware sensitive | Status if unavailable |
|---|---:|---:|---|
| identity and provenance | yes | no | `FAIL` |
| build hygiene | yes | no | `FAIL` |
| static policy checks | yes | no | `FAIL` |
| protocol constants snapshot | yes | no | `FAIL` |
| packet pack/unpack parity | yes | no | `FAIL` |
| packer throughput | yes | no | `FAIL` |
| simulated output matrix | yes | no | `FAIL` |
| synthetic DVS/timecode matrix | yes | no | `FAIL` |
| Core Audio/DriverKit surface model | yes | no | `FAIL` |
| evidence schema validation | yes | no | `FAIL` |
| physical tone capture | no | yes | `BLOCKED_HARDWARE_FORBIDDEN` |
| physical real-music capture | no | yes | `BLOCKED_HARDWARE_FORBIDDEN` |
| Traktor scope/operator test | no | yes | `BLOCKED_HARDWARE_FORBIDDEN` |
| hotplug/sleep/wake | no | yes | `BLOCKED_HARDWARE_FORBIDDEN` |
| install/dext activation | no | yes | `BLOCKED_INSTALL_WINDOW` |

## Current Executable Offline Gates

As of 2026-06-16, `scripts/run-cpp-offline-gates` produces the first objective
C++ evidence set:

| Gate | Current result | Evidence |
|---|---|---|
| default functional CTest | `PASS` | `local-analysis/cpp-offline/ctest-default.txt` |
| Release functional + matrix + performance CTest | `PASS` | `local-analysis/cpp-offline/ctest-release.txt` |
| packet matrix | `PASS` | `local-analysis/cpp-offline/packet-matrix.json` |
| protocol constants snapshot | `PASS` | `local-analysis/cpp-offline/protocol-contract.json` |
| simulated output matrix | `PASS` | `local-analysis/cpp-offline/simulated-output-matrix.json` |
| Python Mode 2 oracle | `PASS` | `local-analysis/cpp-offline/mode2-python-oracle.txt` |
| timecode matrix | `PASS` | `local-analysis/cpp-offline/timecode-matrix.json` |
| timecode signal analysis | `PASS` | `local-analysis/cpp-offline/timecode-signal-analysis.json` |
| DVS signal smoke | `PASS` | `local-analysis/cpp-offline/dvs-signal-smoke.json` |
| DVS packet input decode | `PASS` | `local-analysis/cpp-offline/dvs-packet-input-decode.json` |
| realtime allocation audit | `PASS` | `local-analysis/cpp-offline/realtime-audit.json` |
| DriverKit surface model | `PASS` | `local-analysis/cpp-offline/driverkit-surface-model.json` |
| DriverKit shell contract | `PASS` | `local-analysis/cpp-offline/driverkit-shell-contract.json` |
| jitter model | `PASS` | `local-analysis/cpp-offline/jitter-model.json` |
| static policy | `PASS` | `local-analysis/cpp-offline/static-policy.json` |
| Release packet/routing benchmark | `PASS` | `local-analysis/cpp-offline/offline-bench-release.json` |
| evidence schema | `PASS` | `local-analysis/cpp-offline/evidence-schema.json` |
| runtime isolation quiescence | `PASS` | `local-analysis/runtime-isolation/current.json` |

Current Release benchmark values:

| Metric | Current value | PASS floor |
|---|---:|---:|
| Mode 2 pack throughput | median `1454.94 MiB/s` over `5` repeats, min `1407.95`, max `1594.53` | `100 MiB/s` |
| Mode 2 decode preallocated throughput | median `546.495 MiB/s` over `5` repeats, min `530.489`, max `549.875` | `100 MiB/s` |
| Mode 2 decode allocating wrapper throughput | median `516.065 MiB/s` over `5` repeats, min `473.331`, max `539.850` | informational |
| Float32 to S24 conversion throughput | median `76,538,400 frames/s` over `5` repeats, min `76,501,200`, max `82,625,800` | `1,000,000 frames/s` |
| identity routing throughput | median `854,123,000 frames/s` over `5` repeats, min `809,504,000`, max `900,065,000` | `1,000,000 frames/s` |
| reversed routing throughput | median `449,037,000 frames/s` over `5` repeats, min `447,759,000`, max `455,704,000` | `1,000,000 frames/s` |
| advanced mute/invert/cross-deck routing throughput | median `441,878,000 frames/s` over `5` repeats, min `440,671,000`, max `444,815,000` | `1,000,000 frames/s` |
| Mode 2 check errors | `0` | `0` |
| Mode 2 panic flags | `0` | `0` |
| preallocated decode overflows | `0` | `0` |
| hot path allocations | `0` | `0` |
| SPSC ring pushed/popped frames in realtime audit | `2815/2815`, remaining `0` | pushed equals popped, remaining `0` |
| jitter model rows | `8` | `>= 8` |
| jitter model lag jumps | `4` across modeled recovery scenarios | scenario-bounded |
| jitter model timeline resets | `4` across modeled recovery scenarios | scenario-bounded |
| jitter model elastic drops/replays | `172/82 frames` | scenario-bounded |
| jitter max error | `46.639 frames` in gap recovery scenario | scenario-bounded |
| jitter regressions | `0` | `0` |

Functional coverage in the current C++ test binary:

- Audio 8 DJ surface: 8 inputs, 8 outputs, A/B/C/D left/right ordering.
- Protocol constants: Native Instruments VID/PID `0x17cc:0x1978`, USB
  interface/config/alternate setting `0/1/1`, endpoints `0x01/0x81/0x82/0x06`,
  CAIAQ command ids, required rates `44100/48000`, and deferred known rate
  codes for `88200/96000`.
- Sample format distinction: host Float32 interleaved vs signed 24-bit packed USB.
- Sample-rate policy: 44.1 kHz and 48 kHz accepted, unsupported rates rejected.
- Identity routing for A/B/C/D.
- Advanced routing contract matching the Rust oracle shape: pair A from D,
  pair B muted, pair C side-swapped, and output D right inverted, with
  prevalidated `RoutingPlan` and no dynamic routing storage.
- S24 big-endian conversion vectors.
- Mode 2 round-trip for start bytes `0..5`.
- Packet matrix: 44.1/48 kHz, transfer bytes `48`, `80`, `352`, gains `1.0` and `0.5`, start bytes `0..5`, `72` rows.
- Simulated output matrix: deterministic dense/transient/wideband program
  material on output pairs A/B/C/D at 44.1/48 kHz, gains `1.0` and `0.5`,
  `48` rows, `0` failures, minimum SNR `119.407 dB`, max residual ratio
  `1.07069e-06`, max leakage `-240 dBFS`.
- Python Mode 2 oracle: inherited validator passes all start bytes at 352-byte transfers.
- Timecode policy matrix: `timecode-vinyl`, `timecode-cd-line`, `phono`, `disabled`.
- Input profile matrix: playback decode off/software lock off; timecode-vinyl,
  CD-line, and phono decode on/software lock on with CAIAQ modes `0`, `1`, and
  `2`, identity A/B/C/D source map, and ground-lift intent.
- Timecode deck assignment matrix: decks A/B/C/D map to input pairs `0/1`, `2/3`, `4/5`, `6/7`.
- Timecode signal analysis: `8` rows at 44.1/48 kHz, balanced synthetic
  carrier passes; wrong frequency, channel imbalance, and clipping fail for the
  intended reasons under Rust-oracle thresholds.
- DVS signal smoke: quadrature timecode-like signal across vinyl/CD-line/phono profiles on each deck at 44.1/48 kHz with zero synthetic leakage, `24` rows.
- DVS packet input decode: quadrature signal is packed as Mode 2 USB bytes,
  decoded through the input profile into caller-owned scratch/output buffers,
  then analyzed for RMS, frequency, p95 period jitter, correlation, and leakage;
  `24` rows pass, and playback profile decode-off preserves packet stats while
  writing `0` input frames.
- DriverKit surface model: one 8-channel input stream, four 2-channel output streams, 44.1/48 kHz.
- DriverKit shell contract: offline lifecycle start/duplicate-start/stop/
  duplicate-stop is bounded and validates the device model without requiring
  DriverKit SDK or activating a System Extension.
- Jitter model: 44.1/48 kHz, 64-frame period, no timestamp regressions.
- Static policy: official offline gate path contains no audited system-mutation commands.
- SPSC frame ring: fixed-capacity push/pop/clear contract and realtime audit
  push/pop of all decoded frames with zero hot-path allocations.
- Evidence schema: required evidence files exist and declare offline/no-hardware state.
- Synthetic no-leakage check: pair A signal does not appear on B/C/D.
- Runtime isolation quiescence: lock absent, mainline OpenA8DJ LaunchAgents
  disabled, active HAL absent, no OpenA8DJ process detected. This is not an
  audio-quality gate; it is a safety precondition before restoring the HAL for
  physical tests.

This is objective evidence for packet/routing correctness and software
throughput only. It is not yet evidence for physical sound quality, Traktor DVS
lock, actual DriverKit CPU, or user-visible listening quality.

## Promotion Gate

The branch promotion gate is:

```bash
scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json
```

Promotion to `main` is forbidden unless this script returns `PASS` and
`branch_promotion_allowed=true`.

Current status as of 2026-06-16:

| Gate | Current result | Reason |
|---|---|---|
| offline all gates | `PASS` | no hardware/CoreAudio/USB touched |
| offline timecode signal analysis | `PASS` | `8` rows, `0` failures |
| offline protocol contract | `PASS` | VID/PID, endpoints, 8-in/8-out, Mode 2 cadence/full-frame constants |
| offline simulated output matrix | `PASS` | `48` rows, SNR min `119.407 dB`, residual max `1.07069e-06`, leakage max `-240 dBFS` |
| offline DriverKit shell contract | `PASS` | device model valid, no System Extension activated |
| offline throughput | `PASS` | pack/decode/routing exceed offline floors |
| simulated output oracle | `PASS` | alignment/SNR/residual match oracle expectations |
| physical tone beats mainline best | `PASS` | `sideband_ratio=0.000657`, clicks `0` |
| physical music quality | `FAIL` | `quality_alignment_score=0.938154`, `snr_db_min=8.93`, quiet mid noise `-31.17 dBFS`, lag jumps `24` |
| runtime CPU beats mainline | `FAIL` | observed `opena8dj_driver_p95=11.5`, `coreaudiod_p95=95.8` |
| physical Traktor/timecode vinyl | `FAIL` | no real Traktor/timecode lock evidence |

The current candidate is therefore not ready for branch promotion, regardless
of offline PASS status or tone performance.

## Offline Thresholds

### Identity And Provenance

| Metric | PASS threshold |
|---|---|
| candidate id | non-empty git hash or immutable build id |
| source worktree | `/Users/fer/dev/audio8djcpp` |
| mainline C baseline id | recorded, preferably `0.3.135` until superseded |
| Rust oracle id | recorded, preferably `3429796` until superseded |
| generated artifact root | inside `/Users/fer/dev/audio8djcpp` |
| forbidden write roots | no writes under `/Users/fer/dev/opena8dj` or `/Users/fer/dev/audio8djrust` |
| hardware policy | `offline_only` |
| hardware lock intent | `not_requested` for offline gates |

### Build Hygiene

| Metric | Minimum | Stretch target |
|---|---:|---:|
| clean compile errors | `0` | `0` |
| test executable build failures | `0` | `0` |
| warnings in promoted targets | `0 new warnings` | `0 warnings` |
| sanitizer or analyzer crashes | `0` | `0` |
| generated binary hash recorded | required | required |
| generated header/API snapshot recorded | required | required |

The offline readiness claim must name the exact build command set used. A build
that touches install, Core Audio reload, USB, or hardware is not part of this
offline gate.

### Static Policy Checks

| Metric | PASS threshold |
|---|---|
| proprietary blobs or firmware added | `0` |
| GPL/proprietary implementation imports | `0` |
| DriverKit entitlement assumptions | documented, not assumed present |
| hard-coded local hardware paths | `0` in production code |
| unsafe system mutation in offline scripts | `0` |
| generated files outside C++ worktree | `0` |

### Protocol Constants Snapshot

| Metric | PASS threshold |
|---|---|
| VID/PID | `0x17cc:0x1978` |
| bulk control endpoints | OUT `0x01`, IN `0x81` |
| isochronous endpoints | capture `0x82`, playback `0x06` |
| analog input count | `8` |
| analog output count | `8` |
| MIDI input/output count | `1/1` |
| stream pairs | `A/B/C/D` input and output |
| supported baseline rates | `44100` and `48000` |
| extended rates | modelled separately for `88200` and `96000` |

The gate verifies that C++/DriverKit declarations match the known product
contract. It must not open the device to rediscover these values.

### Packet Pack/Unpack Parity

| Metric | Minimum | Stretch target |
|---|---:|---:|
| matrix rows | `>= 72` | `>= Rust oracle coverage` |
| start bytes | `0..5` | `0..5` |
| transfer sizes | `48`, `80`, `352` | plus implementation-specific edges |
| byte orders | big and native test vectors | all supported modes |
| gains | `1.0`, `0.5` | plus `0.0`, `0.25`, `0.75` |
| channel pairs | `A/B/C/D` | `A/B/C/D` |
| check errors | `0` | `0` |
| panic/assert flags | `0` | `0` |
| C vs C++ mismatches | `0` | `0` |
| Rust oracle mismatches | `0` | `0` |
| max absolute sample error | `<= 1 LSB` | `0 LSB` where deterministic |

### Packer Throughput

| Metric | Minimum | Stretch target |
|---|---:|---:|
| packed data throughput | `>= 100 MiB/s` | `>= Rust oracle value` |
| frame throughput | `>= 1,000,000 frames/s` | `>= Rust oracle value` |
| allocation count in hot loop | `0` | `0` |
| lock acquisitions in hot loop | `0` | `0` |
| branch/error fallback count | `0` for valid fixtures | `0` |

Throughput is a software-only proxy. It does not prove low physical CPU, but it
can reject slow C++/DriverKit packet code before hardware is touched.

### Simulated Output Soundcheck

| Metric | Minimum | Stretch target |
|---|---:|---:|
| `measurement_status` | `VALID` | `VALID` |
| output pairs | `A/B/C/D` | `A/B/C/D` |
| rates | `44100`, `48000` | plus `88200`, `96000` model coverage |
| alignment score | `>= 0.995` | `>= 0.999` |
| simulated SNR | `>= 72 dB` | `>= 75.22 dB` |
| 1-5 kHz residual ratio | `<= 0.0010` | `<= 0.000669` |
| 1-5 kHz residual level | `<= -105 dBFS` | `<= -108.83 dBFS` |
| mid-band CPU correlation | `<= 0.02` | `0.000000` |
| click outliers | `0` | `0` |
| clipped frames | `0` | `0` |
| lag jumps over 2 frames | `0` | `0` |
| wrong-pair energy | `<= -80 dBFS` | `<= -96 dBFS` |
| polarity/channel swap flags | `0` | `0` |

The minimum threshold intentionally sits near the Rust oracle. If this gate
cannot match Rust in a software-only simulation, the C++/DriverKit candidate is
not ready for a hardware window.

### Synthetic DVS/Timecode Matrix

| Metric | Minimum | Stretch target |
|---|---:|---:|
| rates | `44100`, `48000` | plus extended-rate model checks |
| profiles | `timecode-vinyl`, `timecode-cd-line`, `phono` | all profiles |
| input mode mapping | `0/1/2` correct | correct |
| ground-lift policy | correct per profile | correct |
| software lock policy | correct per profile | correct |
| Deck A input mapping | Input A only | Input A only |
| Deck B input mapping | Input B only | Input B only |
| C/D leakage in A/B test | below threshold | none measurable |
| channel swap flags | `0` | `0` |
| polarity inversion flags | `0` unexpected | `0` |
| synthetic carrier frequency error | `<= 0.5%` | `<= 0.1%` |
| synthetic edge jitter p95 | `<= 4 frames` | `<= 1 frame` |
| dropout windows | `0` | `0` |

This gate proves the C++/DriverKit model preserves the DVS contract. It does
not prove Traktor scope quality until a physical/operator run is authorized.

### Core Audio / DriverKit Surface Model

| Metric | PASS threshold |
|---|---|
| public device name | `Open Audio 8 DJ` |
| stable UID model | `org.opena8dj.Audio8DJ` or documented DriverKit successor |
| input surface | one 8-channel input stream or justified DriverKit equivalent |
| output surface | four stereo output streams A/B/C/D |
| input channel names | Input A/B/C/D left/right |
| output channel names | Output A/B/C/D left/right |
| sample rates | 44.1 and 48 kHz required, 88.2/96 kHz marked extended |
| buffer size model | frame and legacy byte semantics mapped or explained |
| controls | input mode, ground-lift profiles, software lock |
| MIDI model | one input endpoint and one output endpoint |

This is an offline model/schema gate. Do not query Core Audio or activate
DriverKit as part of this gate.

## Comparison Semantics

For every comparable metric, record:

```text
metric_name
candidate_value
mainline_c_value
rust_oracle_value
absolute_delta_vs_mainline_c
percent_delta_vs_mainline_c
absolute_delta_vs_rust_oracle
percent_delta_vs_rust_oracle
comparison_status
```

Comparison status:

- `PASS`: candidate meets the absolute threshold and is no worse than the
  required baseline tolerance.
- `FAIL`: candidate violates the absolute threshold or regresses beyond
  tolerance.
- `NOT_COMPARABLE`: metric does not exist in one baseline; explain why.
- `BLOCKED_*`: metric requires hardware or authorized system mutation.

Tolerance rules:

- zero-count correctness metrics have no tolerance: candidate must be `0`;
- packet parity has no tolerance except documented `<= 1 LSB` quantization;
- simulated SNR may be up to `0.5 dB` below Rust oracle only if residual,
  clicks, routing, and clipping still pass;
- simulated residual ratios must not exceed the minimum threshold even if SNR
  passes;
- latency/CPU runtime comparisons are blocked offline and must not be inferred
  from microbenchmarks.

## Evidence Schema

Every offline run should emit one top-level JSON artifact using this shape:

```json
{
  "schema": "open-a8djcpp.driverkit-metrics.v1",
  "candidate": {
    "id": "git-or-build-id",
    "worktree": "/Users/fer/dev/audio8djcpp",
    "branch": "branch-name",
    "dirty": false
  },
  "status": "PASS|FAIL|NOT_READY|BLOCKED_*|SKIPPED_*",
  "policy": {
    "scope": "offline_only",
    "hardware_lock_requested": false,
    "hardware_access_attempted": false,
    "coreaudio_access_attempted": false,
    "usb_access_attempted": false,
    "forbidden_worktree_writes": 0
  },
  "baselines": {
    "mainline_c": {
      "id": "0.3.135",
      "source": "recorded-reference"
    },
    "rust_oracle": {
      "id": "3429796",
      "source": "/Users/fer/dev/audio8djrust/docs/RUST_PM_SUCCESS_METRICS.md"
    }
  },
  "gates": {
    "identity": {},
    "build_hygiene": {},
    "static_policy": {},
    "protocol_constants": {},
    "packet_parity": {},
    "packer_throughput": {},
    "simulated_output": {},
    "synthetic_dvs": {},
    "surface_model": {},
    "evidence_schema": {}
  },
  "comparison": {
    "mainline_c": {},
    "rust_oracle": {}
  },
  "blocked": {
    "physical_tone": "BLOCKED_HARDWARE_FORBIDDEN",
    "physical_music": "BLOCKED_HARDWARE_FORBIDDEN",
    "traktor_scope": "BLOCKED_HARDWARE_FORBIDDEN",
    "install_driverkit": "BLOCKED_INSTALL_WINDOW"
  },
  "artifacts": []
}
```

Required artifact set:

| Artifact | Purpose |
|---|---|
| `metrics.json` | machine-readable result |
| `summary.md` or `summary.txt` | human-readable verdict |
| `manifest.json` | commands, timestamps, hashes, fixtures |
| `comparison.csv` or `comparison.json` | normalized baseline deltas |
| `protocol-snapshot.json` | modeled constants and surface contract |
| `packet-parity.json` | packet matrix details |
| `simulated-output/*.json` | output soundcheck details |
| `synthetic-dvs/*.json` | DVS/timecode synthetic details |

Generated audio fixtures must remain local and must not contain redistributed
user music in committed files.

## Readiness Levels

### `OFFLINE_BASELINE_READY`

Allowed only when all required offline gates PASS and all hardware-sensitive
gates are explicitly blocked with the correct blocked status.

### `READY_FOR_LOCKED_PHYSICAL_WINDOW`

Allowed only after `OFFLINE_BASELINE_READY` plus:

- candidate id and artifacts are frozen;
- hardware test owner, time window, and rollback path are recorded;
- global hardware lock path is known: `$HOME/.opena8dj/hardware-gate.lock`;
- planned physical gates are listed without running them;
- no dirty or stale baseline state exists.

### `READY_FOR_HUMAN_TEST`

Not reachable from offline gates alone. Requires authorized physical capture,
valid tone/music gates, DVS scope validation if DVS is claimed, and human
listening evidence.

### `READY_FOR_RELEASE`

Not reachable from offline gates alone. Requires the full product matrix:
physical A/B/C/D input and output validation, vinyl and CD-line timecode,
MIDI loopback, hotplug, sleep/wake, long-run playback, installer/uninstaller,
signing/notarization, DriverKit entitlement plan, and legal/provenance review.

## Readiness Checklist

- [x] Candidate id is immutable for current offline evidence: code commit `775de71`.
- [x] No writes occurred outside `/Users/fer/dev/audio8djcpp`.
- [x] Offline-only policy is recorded.
- [x] Mainline C baseline id and values are recorded in docs.
- [x] Rust oracle id and values are recorded in docs.
- [x] Build hygiene gate passes for current CMake/CTest scope.
- [x] Realtime hot-path allocation audit passes for pack/decode/routing simulation.
- [x] Static policy gate passes for the official offline CMake/script/tools path.
- [x] Protocol constants snapshot passes.
- [ ] Packet parity matrix passes against C and Rust expectations.
- [x] Packer throughput meets Rust oracle floors in Release microbench.
- [x] Simulated output matrix passes all pairs and required rates.
- [x] Initial synthetic timecode profile/deck matrix passes.
- [x] DriverKit surface model gate passes.
- [ ] Surface model matches the 8-in/8-out Traktor-facing contract.
- [ ] Evidence schema validates.
- [x] Current evidence schema validates.
- [ ] Comparison deltas are recorded for every comparable metric.
- [ ] Hardware-sensitive gates are blocked, not passed.
- [x] Physical-window plan names lock owner, rollback, and stop conditions.
