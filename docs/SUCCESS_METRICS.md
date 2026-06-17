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

## 2026-06-17 Offline Cadence-Safety Gate Addition

- Offline transfer-rate safety: PASS.
  - Evidence: `local-analysis/cpp-offline/transfer-pool-model.json`.
  - Debug offline gates: `17/17` PASS.
  - Release offline gates: `18/18` PASS.
- Required semantics:
  - Capture-paced playback must keep playback queue ratio near `1.0` unless an
    explicitly modeled and physically validated scheduling mode proves
    otherwise.
  - Pool fallback safety alone is not sufficient for timing readiness.
- Current unsafe variants blocked before hardware:
  - coalesce2: playback queue ratio `0.5`;
  - implicit lead2: playback queue ratio `2`;
  - implicit lead4: playback queue ratio `4`;
  - implicit lead64: playback queue ratio `64`.
- Readiness implication:
  - Do not count `HAL_CAPTURE_PACED_OUT_LEAD>1` as a candidate optimization
    until it preserves 1:1 transfer cadence and then passes physical music and
    CPU gates.

## 2026-06-17 Updated Snapshot After Reused ISO Completion Handlers Probe

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-reuse-isoc-completions.json`.

- Branch promotion allowed: `false`.
- Physical music quality: FAIL.
  - Run:
    `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal`.
  - `quality_alignment_score=0.961164` versus required `>= 0.980000`.
  - SNR floor `9.98 dB` versus required `>= 35.00 dB`.
  - mid/high residual ratios `1.459843/1.377935` versus required maxima
    `1.36/1.35`.
  - quiet mid noise `-34.84 dBFS` versus required `<= -58.00 dBFS`.
  - `lag_jumps_gt_2_frames=25` versus required `0`.
- Runtime CPU beats mainline: FAIL.
  - Product probe driver p95 `22.1%`.
  - Product probe `coreaudiod` p95 `15.0%`.
  - Mainline target remains around driver p95 `<= 6.5%` and `coreaudiod`
    p95 `<= 1.7%` under comparable conditions.
- Current threshold interpretation:
  - `HAL_REUSE_ISOC_COMPLETIONS=1` is rejected as a product default.
  - This result confirms that removing per-transfer block setup alone is not
    enough; the remaining CPU problem is deeper in IOUSBHost enqueue/completion
    behavior or transport architecture.

## 2026-06-17 Updated Snapshot After Fast ISO Transfer Config Probe

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-fast-iso-transfer-config.json`.

- Branch promotion allowed: `false`.
- Physical music quality: FAIL.
  - Run:
    `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal`.
  - `quality_alignment_score=0.959397` versus required `>= 0.980000`.
  - SNR floor `10.19 dB` versus required `>= 35.00 dB`.
  - mid/high residual ratios `1.450623/1.368530` versus required maxima
    `1.36/1.35`.
  - quiet mid noise `-35.05 dBFS` versus required `<= -58.00 dBFS`.
  - `lag_jumps_gt_2_frames=35` versus required `0`.
- Runtime CPU beats mainline: FAIL.
  - Product probe driver p95 `23.1%`.
  - Product probe `coreaudiod` p95 `25.9%`.
  - Mainline target remains around driver p95 `<= 6.5%` and `coreaudiod`
    p95 `<= 1.7%` under comparable conditions.
- Current threshold interpretation:
  - `HAL_FAST_ISO_TRANSFER_CONFIG=1` is rejected as a product default.
  - Passing ISO invariants and preserving descriptor layout do not establish
    sound quality, timecode readiness, or low resource use.

## 2026-06-17 Route-Signature Readiness Constraint

- Capture/reference route validity: FAIL/BLOCKED.
  - Evidence:
    `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature`.
  - Shared degraded route-family:
    mainline wait45, C++ inputdecode-off, and C++ ISO64/q8 StopIO all measured
    quality about `0.68`, SNR about `-0.83 dB`, mid residual about `2.53`,
    high residual about `1.78`, and mid coherence about `0.02`.
  - Current C++ failing family:
    quality about `0.96-0.97`, SNR about `10 dB`, mid/high residual about
    `1.4/1.36`, and persistent lag jumps.
- PASS requirement before audiophile claim:
  - A known-good physical capture/reference route must produce a clean music
    comparison independent of the Audio 8 DJ candidate, or the candidate must
    pass on a route whose validity is documented with equivalent evidence.
- Readiness implication:
  - Degraded shared-route evidence cannot prove C++ better than mainline.
  - Current C++ evidence still fails product thresholds, so route uncertainty
    does not permit promotion.

## 2026-06-17 Updated Snapshot After Inline Inactive Decode Bypass Probe

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-inline-inactive-decode-bypass.json`.

- Branch promotion allowed: `false`.
- Physical music quality: FAIL.
  - Run:
    `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal`.
  - `quality_alignment_score=0.961965` versus required `>= 0.980000`.
  - SNR floor `10.16 dB` versus required `>= 35.00 dB`.
  - mid/high residual ratios `1.429792/1.358387` versus required maxima
    `1.36/1.35`.
  - quiet mid noise `-35.03 dBFS` versus required `<= -58.00 dBFS`.
  - `lag_jumps_gt_2_frames=31` versus required `0`.
- Runtime CPU beats mainline: FAIL.
  - Product probe driver p95 `22.1%`.
  - Product probe `coreaudiod` p95 `41.3%`.
  - Mainline target remains around driver p95 `<= 6.5%` and `coreaudiod`
    p95 `<= 1.7%` under comparable conditions.
- Current threshold interpretation:
  - The inline inactive decode bypass is rejected as a product change.
  - This result confirms that inactive input decode call overhead is not the
    dominant route to audiophile quality or mainline-beating resource use.

## 2026-06-17 Updated Snapshot After Output Sample Time Follower Probe

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-output-sample-time-follower.json`.

- Branch promotion allowed: `false`.
- Physical music quality: FAIL.
  - Run:
    `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal`.
  - `quality_alignment_score=0.962572` versus required `>= 0.980000`.
  - SNR floor `9.94 dB` versus required `>= 35.00 dB`.
  - mid/high residual ratios `1.458736/1.377276` versus required maxima
    `1.36/1.35`.
  - quiet mid noise `-34.98 dBFS` versus required `<= -58.00 dBFS`.
  - `lag_jumps_gt_2_frames=28` versus required `0`.
- Runtime CPU beats mainline: FAIL.
  - Product probe driver p95 `24.7%`.
  - Product probe `coreaudiod` p95 `53.0%`.
  - Mainline target remains around driver p95 `<= 6.5%` and `coreaudiod`
    p95 `<= 1.7%` under comparable conditions.
- Current threshold interpretation:
  - `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1` is rejected as a product default.
  - Passing transport counters with failing music again means build cleanliness,
    ISO invariants, and local timeline counters are necessary but not sufficient
    for audiophile readiness.

## 2026-06-17 Current-Family Window Alignment Constraint

- Evidence:
  `local-analysis/timebase-window-comparison/20260617-current-family/summary.json`.
- Diagnostic result:
  PASS as analysis, FAIL as readiness evidence.
- Local lag correction does not clear the music-quality gate:
  - best observed mid-band residual improvement is about `2.09%`;
  - corrected mid residual medians remain `>= 1.413509`;
  - lag jumps remain `22-35` in the compared runs.
- Readiness implication:
  - A future timing candidate must reduce corrected residual materially, not
    only improve drift ppm or raw correlation.
  - Minimum diagnostic expectation before physical promotion evidence:
    corrected mid residual should move toward the product threshold instead of
    staying around `1.4`.
  - If corrected residual remains around `1.4`, the fault should be treated as
    route/capture-chain, analog/device-state, or deeper USB transport behavior,
    not simple sample-time alignment.

## 2026-06-17 Practical Mainline Music Floor Tracking

- Evidence:
  `local-analysis/mainline-practical-floor/20260617-current-cpp-music-family/summary.json`.
- Mainline route context:
  - The read-only mainline docs mark the Audio 8 DJ -> mixer REC OUT -> iRig
    Stream route as valid for QA.
  - That valid route still produced music SNR around `10 dB` after time-warp in
    documented mainline runs, so SNR alone must not be the only diagnostic
    signal for route validity.
- Current C++ status against the practical floor:
  - Best current-family run:
    `20260617-streamusage-irig-pairA-12s-cpp-hal`.
  - PASS within practical floor for:
    quality `0.971648 >= 0.925`, mid residual `1.399655 <= 1.45`,
    quiet mid `-35.20 <= -32.5 dBFS`, lag jumps `28 <= 45`, clipping `0`.
  - FAIL within practical floor for:
    high residual `1.358543 > 1.355`.
- Readiness implication:
  - Practical floor near-pass is not promotion evidence.
  - C++ still needs high residual below floor, CPU below mainline, strict
    physical tone/music improvement, and physical Traktor/timecode validation
    before any branch move.

## 2026-06-17 Physical Comparator And USB Hotspot Gate

- Evidence:
  `local-analysis/physical-run-compare/20260617-profile-family.json`.
- Practical comparator thresholds:
  - `quality_alignment_score >= 0.925`.
  - `mid_band_residual_ratio <= 1.45`.
  - `high_band_residual_ratio <= 1.355`.
  - `quiet_mid_band_noise_dbfs <= -32.5`.
  - `lag_jumps_gt_2_frames <= 45`.
  - `capture_clipped_frames == 0`.
  - `opena8dj_driver` CPU p95 `<= 12%`.
  - `coreaudiod` CPU p95 `<= 8%`.
- Current result:
  - `0/4` compared profiling runs pass.
  - Steady-state v5 driver p95 is `24.0%`; best short v4 is `22.2%`.
  - Both are above the practical CPU limit and far above the mainline target.
- New metric integrity requirement:
  - Future physical runs must expose reliable `playbackTransfersSubmitted` and
    `playbackTransfersCompleted` counters from the same HAL payload.
  - A run with missing or stale submitted/completed transfer accounting is
    diagnostic-only and cannot support a performance/readiness claim.
- Readiness implication:
  - The active CPU hotspot is USB transfer enqueue/IOKit/MIG overhead.
  - A future performance candidate must show driver p95 moving materially
    toward `<=12%` while preserving or improving practical physical quality.
  - A packer-only optimization is insufficient unless profiling proves the
    packer became the dominant cost.

## 2026-06-17 Output-Only No-Capture ISO Candidate Requirements

- Candidate:
  - `HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1`.
  - Default remains rejected/not enabled until physical evidence proves benefit.
- Minimum physical acceptance requirements:
  - HAL candidate safety PASS before playback.
  - `playbackTransfersSubmitted` and `playbackTransfersCompleted` both visible
    and paired in stream stats.
  - Driver CPU p95 must move materially toward `<=12%`; a tiny reduction is not
    useful.
  - `coreaudiod` p95 must remain `<=8%` for the practical gate.
  - `outputUnderruns=0`, `outputActiveUnderruns=0`,
    `outputLateWriteFrames=0`, and transfer-pool fallback allocations `0`.
  - Physical music must not regress below the practical floor:
    quality `>=0.925`, mid residual `<=1.45`, high residual `<=1.355`,
    lag jumps `<=45`, no clipping.
  - It must not be used for Traktor/timecode until input/DVS activation is
    physically validated.
- Rejection trigger:
  - Any recurrence of the fixed OUT failure family, negative SNR, decorrelated
    music, missing HAL input surface, or unstable CoreAudio enumeration rejects
    the candidate regardless of CPU.
- Current status:
  - `FAIL_REJECTED_FOR_PRODUCT`.
  - Pre-fill-fix run proved that lower driver CPU can be meaningless:
    `playbackTransfersSubmitted=0`, `playbackTransfersCompleted=0`, and
    `outputFramesRead=0`.
  - Fill-fix run restored playback but failed physical music quality:
    `quality_alignment_score=0.183990`, SNR floor `-21.45 dB`, mid/high
    residual `17.171794/11.452494`, and `41` lag jumps.
  - Driver p95 `8.0%` does not pass because coreaudiod p95 was `28.3%` and
    the capture was not audiophile-valid.

## 2026-06-17 Ignore HAL Output Sample-Time Candidate Requirements

- Candidate:
  - `HAL_IGNORE_OUTPUT_SAMPLE_TIME=1`.
  - Default remains `0`.
- Purpose:
  - Test whether the direct USB abs-deadline/contiguous-write matrix success can
    be approximated in the HAL path by ignoring CoreAudio
    `mOutputTime.mSampleTime`.
- Current status:
  - `FAIL_REJECTED_FOR_PRODUCT`.
  - HAL candidate safety PASS.
  - Physical music FAIL:
    `quality_alignment_score=0.963508`, SNR floor `10.20 dB`, mid/high
    residual `1.440572/1.369361`, quiet mid `-35.12 dBFS`, `32` lag jumps,
    clipped frames `0`.
  - CPU FAIL:
    driver p95 `22.6%`, coreaudiod p95 `44.7%`.
  - Stream stats showed no output active underruns, timeline resets, late
    writes, elastic drops/replays, or pool fallback allocations.
  - Failure analysis remains `timebase_or_alignment_instability`; simple
    polarity, matrix, memoryless nonlinear, and LTI transfer corrections are
    insufficient explanations.
- Readiness implication:
  - Clean counters and contiguous HAL writes do not imply sound quality.
  - Promotion remains blocked by physical music quality, CPU, and missing
    physical Traktor/timecode validation.

## 2026-06-17 ISO8 Queue64 Prefetch256 Candidate Status

- Candidate:
  - `HAL_ISO_FRAMES=8 HAL_PLAYBACK_ISO_FRAMES=8 HAL_CAPTURE_QUEUE=64
    HAL_PLAYBACK_QUEUE=64 HAL_OUTPUT_PREFETCH_FRAMES=256`.
  - Default remains ISO8/q8/prefetch64.
- Purpose:
  - Test whether direct-USB-like queue and startup margin improves HAL physical
    continuity while retaining capture-paced playback.
- Current status:
  - `FAIL_REJECTED_FOR_PRODUCT`.
  - HAL candidate safety PASS.
  - Pair A channel matrix PASS:
    max wrong-source leakage `-53.079 dB`, L->R leakage `-58.221 dB`, R->L
    leakage `-51.442 dB`, no clipping.
  - Physical music FAIL:
    `quality_alignment_score=0.966043`, SNR floor `10.15 dB`, mid/high
    residual `1.442529/1.373910`, quiet mid `-34.87 dBFS`, `25` lag jumps,
    clipped frames `0`.
  - CPU FAIL:
    driver p95 `23.7%`, coreaudiod p95 `86.6%`.
  - Promotion readiness FAIL:
    `physical_music_quality`, `runtime_cpu_beats_mainline`,
    `latest_physical_investigation`, and `traktor_timecode_physical`.
- Readiness implication:
  - Static routing/channel matrix quality is no longer the primary blocker for
    Pair A. The blocker is real-music continuity/residual plus runtime CPU.
  - Increasing queue/prefetch margin is not a valid path by itself because it
    did not remove lag jumps and worsened total resource use.

## 2026-06-17 Direct USB Music Diagnostic Status

- Diagnostic:
  - `scripts/run-direct-usb-soundcheck`
  - `build/opena8dj-usb-play-plain-gain05`, Pair A, lead `8192`.
- Current status:
  - `FAIL_DIAGNOSTIC_ONLY`.
  - Physical music FAIL:
    `quality_alignment_score=0.103211`, worst-channel SNR `-24.31 dB`,
    mid/high residual `17.114359/16.212469`, clipped frames `0`.
  - Failure-mode analysis:
    `timebase_or_alignment_instability`,
    `window_alignment_is_unstable_for_music`, and
    `residual_tracks_program_level`.
- Readiness implication:
  - Direct USB tone matrix evidence cannot be used as an audiophile music
    oracle.
  - Any future direct-USB-vs-HAL comparison must use real music and the same
    objective music metrics, not only static tones.

## 2026-06-17 Physical Latency Promotion Gate

- Gate:
  - `scripts/analyze-physical-latency.py` must report `result=PASS`.
  - `first_energy_seconds <= 1.5`.
  - `abs(best_correlation) >= 0.98`.
  - `aligned_snr_db >= 35.0`.
  - `linear_fit_snr_db >= 35.0`.
  - `linear_residual_over_capture_rms <= 0.10`.
  - `scripts/analyze-latency-marker-peaks.py` must report `result=PASS`
    for marker evidence when used:
    `paired_peaks >= 4`, `offset_std_seconds <= 0.025`, and
    `abs(offset_mean_seconds) <= 1.5`.
- Current status:
  - `FAIL_BLOCKING_PROMOTION`.
  - Representative direct USB Pair A:
    `first_energy_seconds=5.25`, `best_correlation=-0.623648`,
    `aligned_snr_db=-7.78`, `linear_fit_snr_db=-1.74`,
    `linear_residual_over_capture_rms=0.773905`.
  - Explicit-scheduling fallback Pair A:
    `first_energy_seconds=4.95`, `best_correlation=0.029593`,
    `aligned_snr_db=0.001`, `linear_fit_snr_db=-30.81`.
  - Marker Pair A with lead `8192`:
    `offset_mean_seconds=4.646000`, `offset_std_seconds=0.001237`,
    `readiness_result=FAIL`.
  - After subtracting the wrapper record pre-roll and expected internal
    lead/startup silence, unexplained residual offset is still `3.780667s`.
  - Marker Pair A with lead `0`:
    `offset_mean_seconds=4.900115`, `offset_std_seconds=0.001250`,
    unexplained residual offset `4.129448s`.
  - Marker Pair A with USB diagnostics:
    external marker mean `4.930875s`, std `0.001348s`, while internal
    written/consumed/packed USB analyses all show alignment score `1.000000`,
    lag `0`, and SNR `999.00 dB`.
  - Marker Pair A with `HAL_VALID_CAPTURE_OUT_LAYOUT=1`:
    `offset_mean_seconds=4.638750`, `offset_std_seconds=0.001297`,
    unexplained residual offset `3.773417s`; physical latency still FAILS with
    `best_correlation=0.565271`, `aligned_snr_db=-2.99`, and
    `linear_fit_snr_db=-3.28`.
  - Marker Pair A with forced playback profile control state:
    control changed to `01:02:03:00:02:00`, but
    `offset_mean_seconds=4.667208`, `offset_std_seconds=0.001308`,
    unexplained residual offset `3.807208s`; physical latency still FAILS with
    `best_correlation=-0.318510`, `aligned_snr_db=-4.36`, and
    `linear_fit_snr_db=-9.47`.
  - Marker Pair A with `HAL_SELECT_ALT0_BEFORE_ALT1=1` plus playback profile:
    `select_alt0_before_alt1=1`,
    `offset_mean_seconds=0.405589`, `offset_std_seconds=0.001256`,
    `first_energy_seconds=0.65`; marker/first-energy gates PASS, but physical
    latency still FAILS with `best_correlation=0.414578`,
    `aligned_snr_db=-3.99`, `linear_fit_snr_db=-6.76`, and residual/capture
    `0.908807`.
- Readiness implication:
  - No branch promotion, no hardware-readiness claim, and no timecode readiness
    claim while this gate fails.
  - A stable marker offset alone is not sufficient. The current marker runs
    prove a repeatable multi-second delay, not audiophile-valid output.
  - Changing direct-player lead changes only the small expected margin; it does
    not explain the base multi-second delay.
  - Internal buffer PASS is not product PASS. It narrows the failure boundary
    downstream of C++ timeline/packing, but external physical latency, music
    quality, CPU, and Traktor/timecode gates still fail.
  - `HAL_VALID_CAPTURE_OUT_LAYOUT=1` is rejected as a product fix because it
    preserves the multi-second marker delay and fails the same physical quality
    gates.
  - Forced playback profile is now the correct control-plane default for
    playback, but it is rejected as a product audio-quality fix because the
    physical marker and latency gates still fail.
  - Alt0-before-alt1 is a real latency improvement, but not a product fix until
    full physical music quality, linearity, CPU, and Traktor/timecode gates
    also pass.
  - Alt0-before-alt1 with real music is rejected as a product fix:
    `quality_alignment_score=0.103674`, SNR floor `-24.25 dB`, mid/high
    residual `16.213903/15.560684`, despite clean transport counters and no
    clipping.

## 2026-06-17 Explicit Scheduling Status

- Candidate:
  - `HAL_EXPLICIT_SCHED=1`.
  - Optional diagnostic fallback:
    `HAL_EXPLICIT_SCHED_FAIL_FALLBACK=1`.
- Current status:
  - `FAIL_REJECTED_FOR_PRODUCT`.
  - Without fallback:
    `queue_failures=2805`, `qfail_last=0xe00002be`,
    `qfail_other=2805`, `qfail_explicit=2805`, `sched_fallbacks=0`,
    quality `0.041196`.
  - With queue-full fallback:
    `sched_fallbacks=1`, queue failures reduced to `135`, but quality still
    failed at `0.005597` and SNR floor `-52.51 dB`.
- Readiness implication:
  - Explicit scheduling is useful only as a transport diagnostic. It is not a
    performance or audio-quality improvement over the default path.

## 2026-06-17 Continuous Timeline Reset Fix Status

- New objective evidence:
  - The fixed direct-USB music run passes the internal data-plane integrity
    oracle for 12 seconds:
    written, consumed, and packed USB output all have alignment `1.000000`,
    lag `0`, and SNR `999.00 dB`.
  - Packed USB has `check_errors=0`, `panic_flags=0`, gain `0.5`, and Mode 2
    `check_offset=8`, `start_byte=4`, big-endian layout.
- Product gate status:
  - Still `FAIL_BLOCKING_PROMOTION`.
  - Physical iRig music quality remains below thresholds:
    `quality_alignment_score=0.957628 < 0.98`,
    SNR `9.38 dB < 35 dB`,
    mid residual `1.422297 > 1.36`,
    high residual `1.413835 > 1.35`,
    quiet mid-band noise `-35.22 dBFS > -58 dBFS`.
  - Time-warped reanalysis improves the view only slightly and still fails.
- Readiness implication:
  - Internal USB integrity is necessary but not sufficient.
  - The candidate cannot be declared better than mainline until same-day
    physical A/B, fixed-candidate CPU measurement, physical route/capture
    isolation, and Traktor/timecode gates pass.

## 2026-06-17 Decorrelated Direct USB Routing And Quality Status

- Evidence:
  - `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag`.
  - `local-analysis/promotion-readiness/20260617-after-decorrelated-direct-usb.json`.
- Gates that now pass for this diagnostic route:
  - Direct USB internal integrity:
    written/consumed/packed USB alignment `1.000000`, lag `0`, SNR
    `999.00 dB`, USB `check_errors=0`, USB `panic_flags=0`.
  - Physical decorrelated Pair A routing:
    `max_wrong_source_leakage_db=-57.447168 <= -45.0`,
    `expected_floor_amplitude=0.147371 >= 0.005`, clipping `0`.
- Gates that still fail:
  - Physical waveform/music quality:
    `quality_alignment_score=0.721193 < 0.98`,
    SNR `-2.96 dB < 35 dB`,
    mid residual `2.117458 > 1.36`,
    high residual `2.018361 > 1.35`,
    quiet mid-band noise `-21.77 dBFS > -58 dBFS`.
  - Same-run product music and CPU pairing:
    latest physical quality evidence is direct USB diagnostic evidence, while
    latest CPU evidence is from an older HAL soundcheck run.
  - Runtime CPU:
    latest selected HAL CPU evidence still fails mainline thresholds.
  - Physical latency alignment:
    the best alt0 marker latency run improves first energy and offset but still
    fails correlation/SNR/residual gates.
  - Traktor/timecode physical validation:
    no DVS/timecode vinyl lock evidence exists for the C++ candidate.
- Readiness implication:
  - A routing PASS is not a sound-quality PASS.
  - A direct USB diagnostic PASS is not a product HAL PASS.
  - Branch promotion remains forbidden until C++ beats or equals mainline in the
    same physical route for quality, CPU, routing, latency, recovery, and
    timecode.

## 2026-06-17 Same-Day Mainline A/B Gate Status

- Evidence:
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/ab-comparison.json`.
- Gate result:
  - `FAIL_CPP_NOT_BETTER_THAN_MAINLINE`.
- Absolute quality gate:
  - C++ FAIL:
    quality `0.134709`, SNR `-12.66 dB`, mid/high residual
    `4.904891/4.494813`, quiet mid `-23.34 dBFS`, lag jumps `18`.
  - Mainline FAIL:
    quality `0.246599`, SNR `-13.28 dB`, mid/high residual
    `5.774651/5.636904`, quiet mid `-24.03 dBFS`, lag jumps `41`.
- Relative quality gate:
  - C++ does not beat mainline overall because quality alignment is worse by
    `-0.111889`.
  - C++ is better on residual ratios, lag jumps, and SNR floor, but those wins
    do not overcome failed absolute quality and worse global alignment.
- Relative CPU gate:
  - C++ FAIL:
    driver p95 `23.2%` versus mainline `5.6%`.
    coreaudiod p95 `20.5%` versus mainline `10.3%`.
- Promotion semantics:
  - `PASS` requires C++ to clear absolute quality thresholds and be no worse
    than mainline on same-day quality and CPU.
  - Current status forbids moving C++ to `main` or C mainline to `Legacy`.

## 2026-06-17 Offline Rate-Shape Gate Status

- Evidence:
  - `local-analysis/cpp-offline/jitter-model.json`.
  - `local-analysis/cpp-offline/current-offline-gates.json`.
- Gate result:
  - `PASS` as an offline rejection/guardrail gate.
  - `3` rate-shape rows, `0` rate-shape failures.
- Required semantics:
  - A transport candidate fails before hardware if average output consumption
    deviates from the requested sample rate by more than the modeled tolerance.
  - A transport candidate does not become ready merely because the rate-shape
    row passes; it still needs physical music quality, CPU, routing, recovery,
    and Traktor/timecode evidence.
- Current decisions from the gate:
  - Observed ISO8 partial layout is rate-safe enough to keep investigating:
    about `47967.9` frames/s at 48 kHz, `-668 ppm`.
  - Forced full-8 ISO8 layout is rejected:
    about `88000` frames/s, `833333 ppm` too high.
  - Mainline-like ISO64/q8 is rate-safe but blocked by prior physical
    rejection, so it is not a product-readiness path by itself.

## 2026-06-17 Rejected-Default Static Policy Status

- Evidence:
  - `local-analysis/cpp-offline/static-policy.json`.
  - `local-analysis/cpp-offline/current-offline-gates.json`.
- Gate result:
  - `PASS`.
  - `21` rejected-default checks.
  - `0` default-policy failures.
- Required semantics:
  - `PASS` means known rejected or diagnostic-only HAL knobs remain opt-in and
    are not the Makefile product defaults.
  - `FAIL` blocks hardware testing until either the default is restored or the
    rejection is deliberately overturned with new written evidence.
- Readiness implication:
  - This protects evidence quality and hardware windows. It does not prove
    audiophile sound quality, timecode readiness, or CPU superiority.

## 2026-06-17 Timebase Family Status

- Evidence:
  - `local-analysis/timebase-window-comparison/20260617-current-family/timebase-family.json`.
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/timebase-ab.json`.
- Current-family diagnostic:
  - `analysis_result=PASS`, `stability_result=FAIL`, `result=FAIL`.
  - `7/7` C++ traces have lag jumps.
  - `7/7` C++ traces have residual after local lag correction.
  - Maximum drift is about `40 ppm`, so linear drift alone is not the product
    blocker.
- Same-day A/B diagnostic:
  - `analysis_result=PASS`, `stability_result=FAIL`, `result=FAIL`.
  - C++ shows a large fixed/local lag that can be corrected offline, but still
    fails product CPU and current-family stability.
  - Mainline shows worse drift/jump/residual behavior in that A/B trace, but C++
    still cannot be promoted until absolute quality and CPU gates pass.
- Required next metric:
  - A new candidate must reduce lag jumps and residual after local correction,
    not merely improve raw correlation or hide latency with offline alignment.

## 2026-06-17 Output Flush Timing Candidate

- Candidate:
  - `HAL_FLUSH_OUTPUT_IN_WRITE_MIX=0` by default.
  - This aligns C++ output-cycle flushing with mainline's `EndIOOperation`
    behavior.
- Offline guard:
  - `static_policy_check` now requires the default to remain `0`.
- Pass criteria before any quality claim:
  - Locked physical music run must pass strict music thresholds.
  - Same-route C++ A/B must reduce lag jumps and residual after local lag
    correction versus the prior C++ default.
  - Same-day comparison must show C++ CPU p95 no worse than mainline C.
  - Traktor/timecode remains unproven until a separate physical DVS gate passes.

Latest locked result:

- Evidence:
  - `local-analysis/soundcheck/20260617-output-flush-mainline-irig-pairA-12s-cpp-hal`.
  - `local-analysis/promotion-readiness-after-output-flush-mainline.json`.
- Result:
  - FAIL, `branch_promotion_allowed=false`.
  - Quality `0.962241`, SNR `10.29 dB`, `23` lag jumps.
  - Corrected mid residual median `1.413201`.
  - Driver CPU p95 about `22.4%`.
- Meaning:
  - The flush-timing alignment does not meet audiophile quality or performance
    gates. It cannot justify hardware readiness, Traktor/timecode claims, or
    branch promotion.

## 2026-06-17 Transport Cadence Family Gate

Latest offline transport matrix: `local-analysis/transport-cadence/current.json`.

Current observed families:

| family | best quality | CPU p95 status | readiness meaning |
| --- | ---: | --- | --- |
| ISO5/q64 | `0.978050` | median driver p95 about `36.9%` | quality-near, CPU fail |
| ISO8/q8 | `0.964724` | median driver p95 about `22.4%` | quality fail, CPU fail |
| ISO10/q8 | `0.969379` | driver p95 `19.6%` | quality fail, CPU fail |
| ISO64/q8 | median about `0.678356` | min driver p95 `6.0%` | CPU-near, physical quality fail |

Required gate update:
- Every physical candidate report must name its effective ISO/queue family when evidence can infer it.
- A family cannot be promoted unless it simultaneously beats the strict physical music gate and the mainline-relative CPU gate.
- Existing evidence does not contain a family that satisfies both; branch promotion and hardware-readiness claims remain blocked.

## 2026-06-17 ISO12/q8 Candidate Status

Latest ISO12/q8 evidence:
`local-analysis/soundcheck/20260617-iso12q8-irig-pairA-12s-cpp-hal`.

Result: rejected.

| metric | ISO12/q8 result | required |
| --- | ---: | ---: |
| quality alignment | `0.963395` | `>= 0.980000` |
| SNR floor | `9.68 dB` | `>= 35 dB` |
| lag jumps > 2 frames | `32` | `0` |
| mid residual ratio | `1.653871` | `<= 1.36` practical / stricter product gate |
| high residual ratio | `1.494546` | `<= 1.35` practical / stricter product gate |
| quiet mid noise | `-34.53 dBFS` | `<= -58 dBFS` |
| driver CPU p95 | `16.55%` | `<= 6.5%` |
| output underruns/resets/panic | `0/0/0` | `0/0/0` |

Meaning:
- ISO12/q8 improves CPU versus ISO8/q8 but makes physical residual/quality
  worse. It is not a valid product path.
- Transport cadence remains a necessary variable to record, but it is not
  sufficient to solve the audiophile quality gate.

## 2026-06-17 Physical Product Evidence Summary Gate

Latest evidence:
`local-analysis/physical-product/20260617-product-evidence-summary.json`.

Required semantics:
- `branch_promotion_allowed=true` is required before any C++ to `main` or C to
  `Legacy` action.
- Same-session C++ vs mainline comparisons must use runs from the same physical
  session directory.
- Best-global C++ runs may establish the current C++ ceiling, but they cannot
  be used as same-session mainline comparisons.
- If both C++ and mainline are marked `fixture_degraded_candidate`, the run can
  diagnose relative behavior but cannot prove audiophile quality.

Current result: `FAIL`.

Current blockers:
- `cpp_physical_quality_gate_failed`.
- `cpp_runtime_cpu_gate_failed`.
- `cpp_quality_does_not_beat_mainline_same_session`.
- `cpp_driver_cpu_does_not_beat_mainline_same_session`.
- `cpp_coreaudiod_cpu_does_not_beat_mainline_same_session`.
- `same_session_fixture_degraded_for_both_candidates`.

Meaning:
- Current C++ has not beaten mainline in sound quality, resource use, or
  full-product readiness.
- The next promotable candidate needs simultaneous absolute quality PASS,
  same-session mainline comparison PASS, CPU PASS, routing PASS, recovery PASS,
  and physical Traktor/timecode evidence.

## 2026-06-17 Raw Completion Handler Probe

Latest evidence:
`local-analysis/soundcheck/20260617-raw-reuse-completions-irig-pairA-20s`.

Result: rejected.

| metric | raw/reuse result | required |
| --- | ---: | ---: |
| quality alignment | `0.973571` | `>= 0.980000` |
| SNR floor | `10.53 dB` | `>= 35 dB` |
| lag jumps > 2 frames | `57` | `0` |
| mid residual ratio | `1.401298` | below strict product threshold |
| high residual ratio | `1.352559` | below strict product threshold |
| driver CPU | about `21-22%` steady | `<= 6.5%` current mainline gate |

Policy:
- `HAL_RAW_ISOC_COMPLETIONS=0` is a required product default unless a future
  same-session candidate proves both quality and CPU wins.
- Raw completion handlers are allowed only as an isolated diagnostic build
  because they trade Objective-C weak-reference safety for less callback
  overhead.
- A partial CPU reduction cannot override physical quality failure.

## 2026-06-17 Fractional Time-Warp Diagnostic Gate

Latest evidence:
`local-analysis/offline-diagnostics/20260617-fractional-time-warp-multi.json`.

Result: current best C++ residual is not explained by fractional time-warp.

| run family | scalar SNR improvement | matrix SNR improvement | verdict |
| --- | ---: | ---: | --- |
| ISO10/q8 | `0.725 dB` | `0.705 dB` | rejected |
| ISO8/q8 | `0.372 dB` | `0.349 dB` | rejected |
| ISO12/q8 | `0.929 dB` | `0.914 dB` | rejected |
| raw/reused completions | `0.727 dB` | `0.718 dB` | rejected |

Threshold semantics:
- `>= 6.0 dB` improvement: fractional time-warp can explain a large residual
  fraction and fixture/capture alignment must be revalidated before blaming the
  driver.
- `>= 3.0 dB` and `< 6.0 dB`: partial factor; do not promote, but use as a
  diagnostic.
- `< 3.0 dB`: reject fractional time-warp as the dominant explanation.

Product meaning:
- Passing this diagnostic is not a sound-quality pass.
- Current evidence means that alignment-only fixes cannot satisfy the
  audiophile gate.
- A future candidate still needs absolute physical quality PASS, same-session
  mainline-relative PASS, CPU PASS, routing PASS, recovery PASS, and physical
  Traktor/timecode evidence before readiness or branch promotion.

## 2026-06-17 Transport Budget Frontier Gate

Latest offline evidence:
`local-analysis/cpp-offline/transport-budget-model.json`.

Purpose:
- encode the observed physical transport families as a regression frontier;
- prevent promotion of a candidate that merely moves along the same bad
  quality/CPU tradeoff curve;
- force the next implementation to reduce HAL/USB enqueue cost while preserving
  the best known low-ISO quality behavior.

Observed frontier:

| family | quality alignment | median driver CPU p95 | lag jumps > 2 frames | verdict |
| --- | ---: | ---: | ---: | --- |
| ISO5/q64 | `0.978050` | `36.9%` | `22` | quality close, CPU/jitter fail |
| ISO8/q8 | `0.964724` | `22.4%` | `23` | quality/CPU/jitter fail |
| ISO10/q8 | `0.969379` | `19.6%` | `35` | quality/CPU/jitter fail |
| ISO12/q8 | `0.963395` | `16.6%` | `32` | quality/CPU/jitter fail |
| ISO64/q8 | `0.686712` | `6.3%` | `35` | CPU pass, quality/jitter fail |

Required product thresholds:
- quality alignment score `>= 0.980000`;
- driver CPU p95 `<= 6.5%` under comparable conditions;
- lag jumps greater than two frames: `0`;
- no branch promotion unless all physical product gates also pass.

Current result:
- `PASS` only as a negative offline model: no observed family is a product
  candidate.
- `FAIL` for product readiness: none of the observed families simultaneously
  satisfies quality, CPU, and jitter.

Next required evidence:
- a prepared DriverKit/transport backend model where steady-state HAL callback
  work has `0` direct `IOUSBHostPipe` enqueue/requeue calls;
- unchanged USB cadence/bytes/routing/timecode semantics;
- offline gates PASS before any new physical window;
- same-session physical proof that the new backend beats mainline in quality,
  driver CPU, coreaudiod CPU, routing, and timecode behavior.

## 2026-06-17 Prepared Transport Contract Gate

Required for the next CPU candidate:
- `opena8djcpp_driverkit_prepared_transport_contract` must PASS.
- Safe scenarios must report `minimum_hal_steady_requeues_for_safe=0`.
- Safe scenarios must reject steady-state fallback allocations.
- Completion gap ratio must be `<=1.25`.
- Timestamp regressions must be `0`.
- Channel identity failures must be `0`.
- Timecode profile failures must be `0`.

Current meaning:
- PASS is only an offline architecture gate.
- It does not prove real DriverKit behavior, physical sound quality, or
  better-than-mainline performance.
- It is required before implementing or physically testing a prepared transport
  candidate because the current HAL enqueue path is CPU-blocked.

Implementation status:
- `PreparedTransportBackend` now exists in core C++ and is covered by
  `opena8djcpp_core_tests`.
- `opena8djcpp_driverkit_prepared_transport_contract` uses that core backend
  directly and reports schema
  `opena8djcpp.driverkit-prepared-transport-contract.v2`.
- Product readiness remains blocked until packet/routing batches are integrated
  into the backend and physical same-session quality/CPU evidence beats
  mainline.

## 2026-06-17 Prepared Transport Packet/Ring Gate

Required for packet integration:
- `opena8djcpp_prepared_transport_packet_contract` must PASS.
- Capture and playback Mode2 decode check errors must be `0`.
- Capture and playback panic flags must be `0`.
- Capture and playback prefix mismatches must be `0`.
- HAL steady-state requeues must be `0`.
- Fallback allocations must be `0`.
- Capture/playback ring overruns and underruns must be `0`.
- Timestamp regressions and channel identity failures must be `0`.

Current status:
- PASS offline with default `start_byte=4`, transfer bytes `352`, `12`
  transfers, `131` capture decoded frames, and `131` playback decoded frames.
- This is still not physical readiness and not a better-than-mainline claim.

## 2026-06-17 Prepared Transport Routing/Timecode Gate

Required for routing/timecode integration:
- `opena8djcpp_prepared_transport_routing_timecode_contract` must PASS.
- Playback routing mismatches must be `0`.
- Timecode-vinyl, timecode-cd-line, and phono must pass for decks A/B/C/D.
- HAL steady-state requeues must be `0`.
- Fallback allocations must be `0`.
- Synthetic deck leakage RMS must be `0`.

Current status:
- PASS offline with `12` profile/deck rows, playback routing PASS,
  `0` mismatches, `0` HAL steady requeues, and `0` fallback allocations.
- This is still not physical Traktor readiness; it is an offline prerequisite.

## 2026-06-17 Prepared Transport Recovery Gate

Required for restart/recovery hygiene:
- `opena8djcpp_prepared_transport_recovery_contract` must PASS.
- Invalid configs must be rejected and leave the backend not started.
- A backend that was never started must not report `product_safe`.
- Operations after `stop()` must return `false`/`0` and preserve caller output
  buffers.
- Restart must produce `0` stale capture/playback frame mismatches.
- Restart must reset counters and timestamp history; a lower timestamp in the
  new session must not count as a regression.
- Final clean-session counters must report `0` HAL steady requeues, `0`
  fallback allocations, `0` ring overruns/underruns, and `product_safe=true`.

Current status:
- PASS offline in the focused build run.
- This is not physical recovery readiness; it is a required offline prerequisite
  for a future DriverKit/USB adapter and locked hardware recovery test.

## 2026-06-17 DriverKit Runtime Bridge Gate

Required for the executable DriverKit shell:
- `opena8djcpp_driverkit_runtime_contract` must PASS.
- Invalid stream configuration must be rejected before streaming.
- Stream start before driver start must fail.
- Valid stream config at 44.1 kHz or 48 kHz must be accepted; unsupported rates
  must fail.
- Playback/capture batch movement through the prepared backend must produce
  `0` frame mismatches.
- Shutdown must stop the stream and make `product_safe=false`.
- Running counters must report `0` HAL steady requeues, `0` fallback
  allocations, `0` ring overruns/underruns, `0` timestamp regressions, and
  `0` channel identity failures.

Current status:
- PASS offline in the focused build run at 48 kHz, `64` buffer frames, `32`
  frame batch.
- This is not a signed/installed DriverKit driver and not physical readiness.

## 2026-06-17 DriverKit Extension Scaffold Gate

Required before real dext binding:
- `opena8djcpp_driverkit_extension_scaffold_contract` must PASS.
- `driverkit/extension/Info.plist.template` must contain `IOKitPersonalities`,
  `IOUserAudioDriverUserClientProperties`, Audio 8 DJ match IDs, and DEXT
  package metadata.
- Entitlements template must include DriverKit, DriverKit Audio Family, and USB
  transport entries for `0x17cc:0x1978`.
- IIG files must declare the future `IOUserAudioDriver` and
  `IOUserAudioDevice` boundaries.
- Future binding sources must point at `AudioDriverSkeleton`.
- Default CMake build must exclude the extension sources.
- The gate must report no system extension activation and no driver install.

Current status:
- PASS in the focused build run.
- This is not a runnable/signed dext and not physical readiness.

## 2026-06-17 C++ Loopback Quality Analyzer Gate

Required for objective audio-quality analysis:
- `opena8djcpp_loopback_quality_analysis` must PASS in selftest mode.
- Clean synthetic loopback must pass:
  - minimum SNR `>= 70 dB` in selftest;
  - minimum correlation `>= 0.995`;
  - click outliers `0`.
- Degraded synthetic loopback must fail for objective signal reasons.
- Real physical captures must use stricter per-run thresholds documented in the
  evidence file and must include:
  - reference path;
  - capture path;
  - sample rate;
  - compared frames/seconds;
  - alignment score;
  - per-channel fitted gain;
  - per-channel correlation;
  - per-channel SNR;
  - residual RMS/peak;
  - click outliers.

Current status:
- PASS offline in selftest mode.
- This gate proves the analyzer can distinguish clean from degraded synthetic
  loopback. It does not prove physical sound quality.

Readiness rule:
- A future C++ candidate may only claim better sound than mainline if locked
  same-session external-loopback captures show C++ meeting or exceeding
  mainline on SNR, residuals, click count, channel leakage, and subjective
  listening, while also beating or matching CPU/jitter/resource thresholds.

## 2026-06-17 C++ Channel Leakage Tone Contract

Required for digital no-leakage coverage:
- `opena8djcpp_channel_leakage_tone_contract` must PASS.
- Sample rates: `44100` and `48000`.
- Active output pairs: A/B/C/D.
- Clean rows must report:
  - check errors `0`;
  - panic flags `0`;
  - expected tone floor `>= 0.10`;
  - wrong-source leakage `<= -90 dB`;
  - inactive-deck leakage `<= -90 dB`.
- Injected-leak rows must be rejected.

Current status:
- PASS offline with `16` rows.
- Max clean wrong-source leakage: about `-167.97 dB`.
- Max clean inactive-deck leakage: `-240 dB`.
- Injected inactive-deck leakage around `-37.26 dB` is rejected.

Readiness rule:
- This proves the digital Mode 2 pack/decode path and tone-domain detector can
  enforce no-leakage offline. Physical A/B/C/D routing still requires locked
  same-session capture evidence and comparison against mainline.

## 2026-06-17 C++ Capture Matrix Quality Analyzer

Required for stored physical-capture analysis:
- `opena8djcpp_capture_matrix_quality_analysis` must PASS in selftest mode.
- Selftest clean row must pass SNR, correlation, click, leakage, expected
  amplitude, and clipping thresholds.
- Selftest degraded row must be rejected.
- Capture mode must report, per run:
  - alignment score and lag;
  - per-channel fitted gain, correlation, SNR, residual RMS/peak;
  - click outliers;
  - clipped frame count;
  - expected tone floor;
  - left-to-right leakage dB;
  - right-to-left leakage dB;
  - max wrong-source leakage dB.

Current status:
- PASS offline selftest.
- Existing stored physical routing analysis can pass leakage while still
  failing global SNR/correlation, which proves these are separate readiness
  dimensions.

Readiness rule:
- Physical readiness requires both routing/leakage and quality metrics to beat
  mainline in a same-session run. A leakage-only pass is not enough.

## 2026-06-17 Native Residual Attribution Gate

Required for any future audiophile-readiness claim:
- `opena8djcpp_soundcheck_wav_quality` must emit `residual_attribution`.
- Native WAV reanalysis must match the selected product run and be consumed by
  `opena8djcpp_physical_run_compare`.
- A candidate cannot pass promotion if native attribution reports
  `uncorrelated_residual_or_capture_path_dominant` while SNR, quiet residual,
  or residual-ratio gates fail.
- Timing may only be called dominant when `timing_explain_db > 3`.
- Routing/matrix may only be called dominant when:
  - `routing_matrix_explain_db > 3`;
  - source L/R correlation is low enough for a stable matrix fixture;
  - matrix condition is stable;
  - decorrelated physical routing evidence agrees.

Current status:
- Latest selected product run classifies as
  `uncorrelated_residual_or_capture_path_dominant`.
- Timing explain is `0.728741 dB`.
- Routing matrix explain is `0.150521 dB`.
- Source L/R correlation is `0.986751`, which means the stored stereo music is
  not sufficient to prove routing matrix behavior.

Readiness rule:
- This attribution is a blocker, not a workaround. The next candidate must
  either remove the residual/capture-path failure or provide stronger
  decorrelated physical evidence showing that the failure was not produced by
  the C++ driver path.

## 2026-06-17 CPU Attribution Gate

Required for any low-resource superiority claim:
- Product evidence must include process-level CPU and callback/hot-path
  attribution, or explicitly report that callback attribution is absent.
- If callback attribution is absent, CPU may still fail gates, but root-cause
  claims must stay limited to process-level evidence.
- Candidate evidence must include capture and playback transfer rates.
- A CPU improvement is not promotable unless quality gates remain passing in
  the same locked physical family.

Current status:
- Latest selected product run reports about `666.809545` capture transfers/s
  and `666.809545` playback transfers/s.
- Callback attribution status:
  `external_process_cpu_only_hot_path_timing_absent`.
- Driver CPU remains `16.6%` after 5s, so the sustained CPU blocker is real,
  but the current run does not prove which internal callback segment owns it.
