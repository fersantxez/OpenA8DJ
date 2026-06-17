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
| Mode 2 cross-oracle byte parity | `PASS` | `local-analysis/cpp-offline/mode2-cross-oracle-parity.json` |
| timecode matrix | `PASS` | `local-analysis/cpp-offline/timecode-matrix.json` |
| timecode signal analysis | `PASS` | `local-analysis/cpp-offline/timecode-signal-analysis.json` |
| DVS signal smoke | `PASS` | `local-analysis/cpp-offline/dvs-signal-smoke.json` |
| DVS packet input decode | `PASS` | `local-analysis/cpp-offline/dvs-packet-input-decode.json` |
| realtime allocation audit | `PASS` | `local-analysis/cpp-offline/realtime-audit.json` |
| DriverKit surface model | `PASS` | `local-analysis/cpp-offline/driverkit-surface-model.json` |
| DriverKit shell contract | `PASS` | `local-analysis/cpp-offline/driverkit-shell-contract.json` |
| jitter model | `PASS` | `local-analysis/cpp-offline/jitter-model.json` |
| transfer-pool model | `PASS` | `local-analysis/cpp-offline/transfer-pool-model.json` |
| static policy | `PASS` | `local-analysis/cpp-offline/static-policy.json` |
| hardware lock policy | `PASS` | `local-analysis/cpp-offline/hardware-lock-policy.json` |
| Release packet/routing benchmark | `PASS` | `local-analysis/cpp-offline/offline-bench-release.json` |
| evidence schema | `PASS` | `local-analysis/cpp-offline/evidence-schema.json` |
| runtime isolation quiescence | `PASS` | `local-analysis/runtime-isolation/current.json` |

## Current Physical Readiness Status

As of 2026-06-17, physical readiness remains `FAIL_NOT_READY`.

The latest transfer-ledger diagnostic run narrows the fault space but does not
improve product quality:

| Metric | Result |
|---|---:|
| default transfer-ledger music quality alignment | `0.964608` |
| default transfer-ledger analog SNR | `10.48 dB` |
| default transfer-ledger lag jumps > 2 frames | `36` |
| diagnostic music quality alignment | `0.963726` |
| diagnostic analog SNR | `10.51 dB` |
| diagnostic lag jumps > 2 frames | `40` |
| diagnostic packed output USB alignment | `1.000000` |
| diagnostic packed output USB check errors | `0` |
| diagnostic packed output USB panic flags | `0` |
| diagnostic packed output inactive deck leakage | `B/C/D zero` |
| native-output music quality alignment | `0.003598` |
| native-output analog SNR | `-63.94 dB` |
| native-output clipped capture frames | `520014` |
| bounded full-ledger music quality alignment | `0.960392` |
| bounded full-ledger analog SNR | `10.37 dB` |
| bounded full-ledger lag jumps > 2 frames | `33` |
| bounded full-ledger row coverage | `91,647 / 91,647`, `overwritten=0` |
| bounded full-ledger playback transport failures | `0` |

Interpretation:

- Offline C++ gates currently pass.
- The HAL written, consumed, and packed output bytes can be perfect while the
  analog/iRig music capture still fails.
- A full transaction ledger can pass continuity/playback-transport checks while
  the product-quality gate still fails. Ledger PASS is necessary observability,
  not readiness.
- `HAL_OUTPUT_NATIVE=1` is rejected and must not be used as a candidate.
- No claim that C++ is better than mainline is allowed until physical music
  quality, runtime CPU, physical timecode/Traktor, and latest investigation
  gates all pass with reproducible evidence.

Current Release benchmark values:

| Metric | Current value | PASS floor |
|---|---:|---:|
| Mode 2 pack throughput | median `1626.84 MiB/s` over `5` repeats, min `1622.01`, max `1644.65` | `100 MiB/s` |
| Mode 2 decode preallocated throughput | median `575.412 MiB/s` over `5` repeats, min `568.916`, max `576.667` | `100 MiB/s` |
| Mode 2 decode allocating wrapper throughput | median `567.326 MiB/s` over `5` repeats, min `530.519`, max `572.134` | informational |
| Float32 to S24 conversion throughput | median `86,641,300 frames/s` over `5` repeats, min `85,809,300`, max `86,763,100` | `1,000,000 frames/s` |
| identity routing throughput | median `1,004,860,000 frames/s` over `5` repeats, min `847,564,000`, max `1,015,900,000` | `1,000,000 frames/s` |
| reversed routing throughput | median `500,315,000 frames/s` over `5` repeats, min `494,222,000`, max `523,459,000` | `1,000,000 frames/s` |
| advanced mute/invert/cross-deck routing throughput | median `486,541,000 frames/s` over `5` repeats, min `482,067,000`, max `492,097,000` | `1,000,000 frames/s` |
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
| burst cadence rows | `3`, failures `0` | `>= 3`, failures `0` |
| unsafe burst scenarios | `coalesce2`, `coalesce4` rejected by model | unsafe CPU-only burst wins rejected |
| transfer-pool model rows | `6`, failures `0` | `>= 6`, failures `0` |
| transfer-pool fallback rejected scenarios | `capture_pool_leak_rejected`, `playback_pool_leak_rejected` | both required |
| transfer-ledger compatibility | old stream-stats evidence still parses; new fields null/0 when absent | no parser breakage |

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
- Mode 2 cross-oracle byte parity against
  `scripts/validate-mode2-output-packing.py`: `72` rows, `0` failures, `0`
  byte mismatches, `0` length deltas, `0` check errors, `0` panic flags.
- Packet matrix: 44.1/48 kHz, transfer bytes `48`, `80`, `352`, gains `1.0` and `0.5`, start bytes `0..5`, `72` rows.
- Simulated output matrix: deterministic dense/transient/wideband program
  material on output pairs A/B/C/D at 44.1/48 kHz, gains `1.0` and `0.5`,
  `48` rows, `0` failures, minimum SNR `119.407 dB`, max residual ratio
  `1.07069e-06`, max leakage `-240 dBFS`.
- Python Mode 2 oracle: inherited validator passes all start bytes at 352-byte transfers.
- Timecode policy matrix: `timecode-vinyl`, `timecode-cd-line`, `phono`, `disabled`.
- Hardware lock policy: HAL candidate install/reload, direct Audio 8 DJ gate,
  and physical soundcheck scripts must acquire the global hardware lock and
  record owner/evidence metadata before hardware-sensitive work.
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

Current status as of 2026-06-17:

| Gate | Current result | Reason |
|---|---|---|
| offline all gates | `PASS` | no hardware/CoreAudio/USB touched |
| offline timecode signal analysis | `PASS` | `8` rows, `0` failures |
| offline protocol contract | `PASS` | VID/PID, endpoints, 8-in/8-out, Mode 2 cadence/full-frame constants |
| offline simulated output matrix | `PASS` | `48` rows, SNR min `119.407 dB`, residual max `1.07069e-06`, leakage max `-240 dBFS` |
| offline DriverKit shell contract | `PASS` | device model valid, no System Extension activated |
| offline transfer-pool model | `PASS` | `6` rows, `0` failures, capture/playback fallback scenarios rejected |
| offline throughput | `PASS` | pack/decode/routing exceed offline floors |
| simulated output oracle | `PASS` | alignment/SNR/residual match oracle expectations |
| physical tone beats mainline best | `PASS` | `sideband_ratio=0.000657`, clicks `0` |
| physical music quality | `FAIL` | latest locked Pair A/iRig run at commit `056d29b`: `quality_alignment_score=0.964558`, `snr_db_min=10.41`, quiet mid noise `-35.90 dBFS`, lag jumps `43`, mid/high residual `1.430949/1.358723` |
| runtime CPU beats mainline | `FAIL` | latest locked Pair A/iRig run at commit `056d29b`: `opena8dj_driver_p95=37.5%`, `coreaudiod_p95=60.3%`, versus mainline driver p95 about `6.5%` |
| physical Traktor/timecode vinyl | `FAIL` | no real Traktor/timecode lock evidence |

The current candidate is therefore not ready for branch promotion, regardless
of offline PASS status or tone performance.

The latest physical rejection evidence is:

- HAL safety:
  `local-analysis/physical-hotpath-lock-reduction/20260617-056d29b/hal-candidate-safety/summary.txt`.
- Soundcheck:
  `local-analysis/soundcheck/20260617-hotpath-lock-056d29b-irig-pairA-16s-cpp-hal/`.
- Final isolation:
  `local-analysis/runtime-isolation/after-hotpath-manual-unload.json`.

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

## Physical Channel Matrix Metric

Existing music captures are not enough to prove routing or crosstalk because
the current reference music has highly correlated stereo channels. A release or
promotion candidate must therefore include a decorrelated physical matrix gate
before any claim that A/B/C/D routing, deck isolation, or timecode capture
topology is better than mainline.

Minimum future PASS requirements:

- generated reference L/R correlation near zero, recorded in evidence;
- valid iRig or equivalent physical post-DAC capture, no clipping, no silence;
- fitted L/R matrix is well conditioned;
- expected channel terms dominate opposite-channel terms;
- opposite-channel leakage cannot explain audible deck leakage or the current
  music residual;
- all artifacts are under `/Users/fer/dev/audio8djcpp/local-analysis`;
- run is protected by `$HOME/.opena8dj/hardware-gate.lock`;
- this gate passes before Traktor/timecode physical claims or branch promotion.

Current status:

- Offline prep PASS:
  `local-analysis/channel-matrix/offline-prepare-smoke`.
- Existing correlated-music matrix classification:
  `local-analysis/soundcheck-linear-matrix/recent-failed-physical-music.json`.
- Pair A physical tone-domain matrix PASS, limited to the iRig Stream capture
  path:
  `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/tone-matrix.json`.
- Fresh default 1 kHz physical tone on `bff59cc` remains insufficient for
  promotion:
  `local-analysis/physical-tone/20260617-bff59cc-default/tone-1khz-irig-pairA/tone-analysis.txt`.
  Metrics: `sideband_ratio=0.006623`, strongest sideband `-42.74 dB`,
  residual ratio `0.456797`, click outliers `40`. This does not beat the
  mainline-best sideband target `<= 0.004942` and fails click/strongest
  sideband targets.
- Latest promotion readiness evidence:
  `local-analysis/promotion-readiness-after-bff59cc-default-tone.json`,
  `FAIL`. Blockers are physical tone vs mainline best, physical music quality,
  runtime CPU vs mainline, latest physical investigation `FAIL_NOT_READY`, and
  absent Traktor/timecode physical validation.
- Future physical soundchecks with stream-stat snapshots must now include
  `transfer-ledger-after.tsv`. The ledger is required evidence for diagnosing
  queue/complete cadence, first-frame numbers, in-flight state, status values,
  and output read ranges before any further scheduling or CPU-optimization
  candidate can be promoted. It must be summarized with
  `scripts/analyze-transfer-ledger.py`; a ledger with sequence gaps,
  completion status errors, playback failed/short transactions, coverage
  mismatch, overwrite during the run, or playback first-frame regressions
  blocks promotion. Active-underrun and capture zero-complete transaction
  observations are warnings unless stream-stat deltas prove they occurred
  during active playback/capture in a product-relevant way.
- Product CPU/performance claims must be measured with
  `HAL_TRANSFER_LEDGER=0`. Full ledger runs are diagnostic evidence only and
  cannot be used to claim low CPU.
- A/B/C/D full physical matrix and Traktor/timecode physical validation remain
  `BLOCKED_UNVALIDATED`.

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
- [ ] No writes occurred outside `/Users/fer/dev/audio8djcpp`: violated and
  mitigated. Two accidental mainline writes occurred during this workstream:
  an untracked helper file was created and removed, and later
  `scripts/audio-stack-guard` briefly received `force-unload` lines that were
  removed immediately. No C++ readiness claim may ignore this process breach.
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

## 2026-06-17 Current Gate Snapshot

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-streamusage-sample.json`.

- Offline gates: PASS.
- Simulated output matrix: PASS.
- Physical tone evidence: PASS against the stored sideband floor, but this is
  not enough for product readiness.
- Physical music quality: FAIL. Latest Pair A/iRig run
  `local-analysis/soundcheck/20260617-streamusage-irig-pairA-12s-cpp-hal`
  reports `quality_alignment_score=0.971648`, SNR `10.52 dB`, `28` lag jumps,
  mid/high residual ratios `1.399655/1.358543`, quiet mid noise
  `-35.20 dBFS`, and `0` clipped frames.
- Runtime CPU beats mainline: FAIL. Latest paired profile reports
  `opena8dj_driver_p95=37.2%` and `coreaudiod_p95=35.0%`, versus mainline
  budget `driver <= 6.5%`, `coreaudiod <= 1.7%`.
- Traktor/timecode physical validation: FAIL/BLOCKED, no physical DVS evidence.
- Branch promotion: forbidden. Do not move C mainline to Legacy and do not move
  C++ to main until these gates pass with reproducible evidence.

CPU-root-cause evidence:
`local-analysis/profiling/20260617-sudo-sample-streamusage-playback-only/opena8dj-driver.sample.txt`
shows the active CPU hotspot is IOUSBHost async enqueue cadence in the USB
completion path, not transfer-ledger diagnostics or pure sample conversion.

## 2026-06-17 Updated Gate Snapshot After ISO64/q8 StopIO

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-iso64q8-stopisoc.json`.

- Offline gates: PASS.
- Simulated output matrix: PASS.
- C++ default transport changed to `ISO64/q8/prefetch64` with
  `HAL_STOP_ISOC_ON_STOP=1`.
- StopIO final-state gate: PASS for latest physical run. Evidence:
  `local-analysis/soundcheck/20260617-cpp-iso64q8-stopisoc-irig-pairA-12s/stream-stats-after.txt`
  reports `streaming=0`, `outputUnderruns=0`,
  `outputActiveUnderruns=0`.
- Runtime CPU beats prior C++ default: PASS as an internal improvement.
  Driver p95 improved from `37.2%` to `9.8%`.
- Runtime CPU beats mainline: FAIL. Same-window mainline `0.3.135` baseline
  reports driver p95 `6.0%`; latest C++ reports `9.8%`.
- Physical music quality: FAIL. Latest C++ run reports
  `quality_alignment_score=0.686712`, SNR `-0.84 dB`, `35` lag jumps, mid/high
  residual ratios `2.525233/1.788470`, quiet mid noise `-35.97 dBFS`.
- Physical route sanity: FAIL/BLOCKED. Same-window mainline also failed the
  same route (`quality_alignment_score=0.680798`, SNR `-0.83 dB`), so this
  capture path cannot support audiophile quality claims until independently
  validated.
- Traktor/timecode physical validation: FAIL/BLOCKED, no physical DVS evidence.
- Branch promotion: forbidden. Do not move C mainline to Legacy and do not move
  C++ to main.

## 2026-06-17 Updated Snapshot After Input Decode And Channel Matrix

- Offline gates: PASS at commit `13ac259`.
- Playback input decode policy: PASS structurally. Input decode is off by
  default for playback/output-only use and enabled by timecode vinyl, CD-line,
  and phono profiles.
- Driver CPU: improved but not accepted as better than mainline.
  - mainline `0.3.135`: driver p95 `6.0%`.
  - C++ ISO64/q8 StopIO before input-decode control: driver p95 `9.8%`.
  - C++ input-decode-off: driver p95 `6.3%`.
- CoreAudio CPU: FAIL. C++ input-decode-off reports `coreaudiod_p95=43.2%`
  versus same-window mainline `8.0%`. The spike appears startup-heavy, but the
  current resource gate counts it.
- Physical music quality: FAIL for both drivers in the current iRig route.
  - mainline: `quality_alignment_score=0.680798`, SNR `-0.83 dB`,
    `39` lag jumps.
  - C++ input-decode-off: `quality_alignment_score=0.680121`,
    SNR `-0.83 dB`, `42` lag jumps.
- Physical Pair A channel matrix: FAIL and worse than mainline.
  - mainline max wrong-source leakage `-42.58 dB`.
  - C++ max wrong-source leakage `-35.36 dB`.
  - threshold `-45 dB`.
- Traktor/timecode vinyl physical validation: FAIL/BLOCKED.
- Branch promotion: forbidden. C++ has not objectively beaten mainline on
  quality, functionality/routing, or resource consumption.

## 2026-06-17 Updated Snapshot After ISO8/ISO10 Physical Gates

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-iso10q8.json`.

- Branch promotion allowed: `false`.
- Offline gates: PASS.
- Physical Pair A channel matrix: PASS for the current short-cadence C++
  candidates.
  - ISO8/q8 HAL: max wrong-source leakage about `-52 dB`.
  - ISO10/q8 HAL: max wrong-source leakage `-52.30 dB`.
- Physical music quality: FAIL.
  - ISO8/q8: `quality_alignment_score=0.964724`, SNR `10.00 dB`,
    mid/high residual `1.432051/1.356290`, `29` lag jumps.
  - ISO10/q8: `quality_alignment_score=0.969379`, SNR `10.18 dB`,
    mid/high residual `1.514509/1.396638`, `35` lag jumps.
- Runtime CPU beats mainline: FAIL.
  - ISO8/q8 driver p95 `23.1%`.
  - ISO10/q8 driver p95 `19.6%`.
  - Mainline target remains driver p95 `<= 6.5%` under comparable conditions.
- Full physical routing: FAIL/BLOCKED.
  - Pair A matrix evidence exists.
  - A/B/C/D physical output matrix is still required.
  - 8 inputs and 8 outputs are represented in code and offline tests, but
    physical capture verification is incomplete.
- Traktor/timecode vinyl physical validation: FAIL/BLOCKED.
  - Timecode policy exists in core and must remain enabled for vinyl/CD-line
    profiles.
  - No physical Traktor/DVS evidence exists for the C++ candidate.
- Current threshold interpretation:
  - A passing Pair A matrix is necessary evidence, not product readiness.
  - The candidate must pass real music, CPU, full routing, and timecode gates
    before any claim that it is better than mainline.
  - "Compiles" and "HAL loads" remain non-product metrics.

## 2026-06-17 Updated Snapshot After Input-Decode-Gated Playback Probe

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-inputdecode-gated-usage.json`.

- Branch promotion allowed: `false`.
- Offline gates: PASS.
  - Debug `17/17`.
  - Release `18/18`.
  - Release bench:
    `pack_mib_s=1627.01`,
    `decode_into_mib_s=577.291`,
    `route_frames_s=9.48938e+08`,
    `route_advanced_frames_s=4.90103e+08`.
- Harness/control-plane semantics: PASS.
  - Playback-only probes no longer activate input decode at stream start.
  - `audio-wav-play` disables input stream usage for playback-only runs when
    stream usage is enabled.
- Physical music quality: FAIL.
  - Latest run:
    `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal`.
  - `quality_alignment_score=0.959187`.
  - SNR `10.14 dB`.
  - mid/high residual ratios `1.467121/1.368783`.
  - quiet mid noise `-35.11 dBFS`.
  - `lag_jumps_gt_2_frames=30`.
- Runtime CPU beats mainline: FAIL.
  - Latest C++ driver p95 `24.2%`.
  - Latest C++ `coreaudiod` p95 `21.9%`.
  - Mainline target remains around driver p95 `<= 6.5%` under comparable
    conditions.
- Timebase/cadence quality: FAIL.
  - Failure-mode analysis classifies the run as
    `timebase_or_alignment_instability`.
  - Estimated drift `-180.6 ppm`, lag span `1645` frames.
  - Runtime discontinuity analysis found no strong CPU or stream-stat
    correlation, so current counters are insufficient to explain or prove
    correction.
- Capture ISO packetization: PASS for the latest inputdecode-gated run after
  corrected inference.
  - Evidence:
    `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`.
  - `iso_frames_per_transfer=8` derived from classified capture slots.
  - No status failures, no short transfers, no other-size transactions, and
    `bytes_per_expected_transaction=352`.
  - Warning only:
    `classified_transactions_missing_at_most_one_stop_transfer`.
  - This PASS does not clear the physical music gate.
- Full physical routing: FAIL/BLOCKED.
  - Pair A matrix evidence exists for ISO8/q8 and ISO10/q8.
  - A/B/C/D physical output matrix is still required.
  - 8 inputs and 8 outputs are represented in code and offline tests, but
    physical input/routing coverage is incomplete.
- Traktor/timecode vinyl physical validation: FAIL/BLOCKED.
  - Timecode policy exists in core and must stay enabled for vinyl/CD-line
    profiles.
  - No physical Traktor/DVS evidence exists for the C++ candidate.
- Current threshold interpretation:
  - The input-decode/stream-usage change is kept as test correctness.
  - It is not a product improvement and cannot support readiness.
  - Next PASS evidence must improve real-music residual, lag stability, and
    runtime CPU against mainline at the same time.

## 2026-06-17 Updated Snapshot After Cadence Diagnostic Capture

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-cadence-diagnostic.json`.

- Branch promotion allowed: `false`.
- Physical music quality: FAIL.
  - Run:
    `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal`.
  - `quality_alignment_score=0.958757`.
  - SNR `10.09 dB`.
  - mid/high residual ratios `1.447622/1.366173`.
  - quiet mid noise `-35.03 dBFS`.
  - `lag_jumps_gt_2_frames=27`.
- Runtime CPU beats mainline: FAIL.
  - Diagnostic run driver p95 `24.1%`.
  - Diagnostic run `coreaudiod` p95 `12.3%`.
  - Diagnostic flags add overhead, but the run still cannot support any
    resource-superiority claim.
- Transport integrity diagnostics: PASS with warnings.
  - Transfer ledger continuous, no sequence gaps, no overwrites.
  - Playback queue/complete delta `0`, max in-flight `8`.
  - Payload guard mismatches `0`.
  - Capture ISO invariants PASS after accounting for stop-transfer gap.
  - Final stop abort status is a warning, not a product transport failure.
- Remaining target:
  completion jitter and capture-paced queue timing, because payload format,
  fixed EQ, simple nonlinearity, and gross underrun counters are not sufficient
  explanations.

## 2026-06-17 Updated Snapshot After Playback-Before-Capture-Requeue Probe

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-playback-before-capture-requeue.json`.

- Branch promotion allowed: `false`.
- Physical music quality: FAIL.
  - Run:
    `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal`.
  - `quality_alignment_score=0.961360` versus required `>= 0.980000`.
  - SNR floor `10.25 dB` versus required `>= 35.00 dB`.
  - mid/high residual ratios `1.425897/1.365001` versus required maxima
    `1.36/1.35` in the current promotion evaluator.
  - quiet mid noise `-35.03 dBFS` versus required `<= -58.00 dBFS`.
  - `lag_jumps_gt_2_frames=28` versus required `0`.
  - no clipping and click outliers `0`, but those passes are not sufficient.
- Runtime CPU beats mainline: FAIL.
  - Product probe driver p95 `21.8%`.
  - Product probe `coreaudiod` p95 `12.2%`.
  - Mainline target remains around driver p95 `<= 6.5%` and `coreaudiod`
    p95 `<= 1.7%` under comparable conditions.
- Transport/counter interpretation:
  - Capture ISO invariants PASS with stop-window warning.
  - Lightweight stream stats did not report output active underruns, timeline
    resets, late writes, or completion delta outliers.
  - Actual audio still shows `28` lag jumps, so completion counters alone are
    not an adequate success metric.
- Current threshold interpretation:
  - `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1` is rejected as a product
    default.
  - A future candidate must beat this run and mainline on the full gate set:
    real-music quality, CPU, lag stability, full A/B/C/D routing, and
    Traktor/timecode evidence.
