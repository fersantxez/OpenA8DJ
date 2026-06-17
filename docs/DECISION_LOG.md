# Decision Log

## 2026-06-17: Reject Reset-Off Variant Before Soundcheck

Decision:
- Keep `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=1` as the default.
- Do not run music soundcheck with `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0`
  unless a new hypothesis explains its load-time CoreAudio CPU spike.

Reason:
- The reset-off variant failed the HAL candidate safety gate before playback or
  capture. CoreAudio enumeration passed, but `coreaudiod` reached `115.1%` and
  total watched CPU reached `130.0%`, exceeding the safety threshold.
- The OpenA8DJ driver process itself was nearly idle (`0.1%`), so this looks
  like a CoreAudio load/enumeration interaction, not a playback-quality result.

Alternatives discarded:
- Proceed to soundcheck despite safety failure: rejected because the candidate
  already violated the low-resource/safe-load gate.
- Treat reset-off as neutral because no audio was played: rejected because
  high `coreaudiod` CPU during load is itself a product blocker.

Evidence:
- Build evidence:
  `local-analysis/physical-reset-audio-params-off/20260616-203712/build.log`
  contains `-DOPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM=0`.
- Safety evidence:
  `local-analysis/physical-reset-audio-params-off/20260616-203712/hal-candidate-safety/summary.txt`.
- Recovery evidence:
  `local-analysis/runtime-isolation/post-reset-audio-params-off-safety-fail.json`
  returns `PASS`, with HAL inactive and lock absent.

## 2026-06-17: HAL Flag Changes Must Force Rebuild

Decision:
- Add `build/.hal-cflags.stamp` and make `hal` plus `usb-play` rebuild when
  `HAL_CFLAGS` changes.
- When the stamp detects a flag change, remove only the generated HAL and
  `usb-play` binaries inside the C++ worktree so the next target cannot reuse a
  stale binary.

Reason:
- During reset-off preparation, `make HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0
  usb-play hal` initially reported parts as already up to date. That could have
  led to a physical test using a previous binary.
- Build flags are part of the candidate identity. Physical evidence is invalid
  if the binary was not rebuilt with the intended flags.

Alternatives discarded:
- Rely on manual `rm` before every variant: rejected because it is easy to miss
  under time pressure.
- Use `make -B` for every physical variant: rejected as a workaround rather
  than a reproducible build contract.

Evidence:
- `local-analysis/build-flags/reset0-rebuild-v2.log`: changing default to
  `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0` rebuilds both affected targets.
- `local-analysis/build-flags/reset0-repeat-v2.log`: repeating the same flags
  does not rebuild.
- `local-analysis/build-flags/default-restore-v2.log`: restoring default
  `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=1` rebuilds both affected targets.

## 2026-06-17: Promotion Evaluator Must Use Latest Paired Soundcheck Evidence

Decision:
- Make `scripts/evaluate-promotion-readiness.py` select the latest existing
  `local-analysis/soundcheck/*/metrics.json` and `cpu-profile.tsv` by default.
- Add a `latest_music_cpu_pair` gate requiring the music metrics and CPU profile
  to come from the same soundcheck directory.

Reason:
- The evaluator previously used fixed default paths. That still produced a
  `FAIL`, but it could hide the fact that newer physical evidence was worse or
  better than the fixed run.
- Promotion depends on current evidence, not any older convenient passing or
  failing artifact.

Alternatives discarded:
- Keep manual `--music` and `--cpu` paths only: rejected because the default
  promotion command must be safe and hard to misuse.
- Permit unpaired latest music/CPU files: rejected because quality and resource
  claims must refer to the same physical run.

Evidence:
- `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  returns `FAIL` and selects
  `local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal`
  for both music and CPU evidence.
- Current blockers remain physical music quality, runtime CPU, physical
  investigation readiness, and unvalidated physical Traktor/timecode.

## 2026-06-17: Keep Audio-Params Reset Enabled But Testable

Decision:
- Add `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM`, defaulting to `1`, and make the
  HAL reset `AUDIO_PARAMS` with rate code `0xff` only when that flag is enabled.

Reason:
- Mainline exposed this as a physical-test knob. C++ was always performing the
  reset, which made one-factor parity tests harder.
- The default remains enabled to preserve current behavior until a locked
  physical test proves that skipping the reset is better.

Alternatives discarded:
- Disable the reset by default: rejected because prior physical history does
  not prove that this improves quality or startup reliability.
- Leave the behavior hard-coded: rejected because it blocks controlled
  mainline-parity experiments.

Evidence:
- `make usb-play hal`
- `scripts/run-cpp-offline-gates`: Debug `16/16` PASS, Release `17/17` PASS.
- Release bench after the change: `pack_mib_s=1657.06`,
  `decode_mib_s=585.852`, `route_frames_s=9.46081e+08`.

## 2026-06-16: Make The Hardware Lock Mandatory In Physical Scripts

Decision:
- Add a shared hardware-lock helper and require it in HAL candidate safety,
  direct Audio 8 DJ gates, and physical soundcheck.
- Add an offline CTest policy gate so removing that lock discipline fails the
  normal offline gate path.

Reason:
- The goal requires more physical testing, but physical tests are only useful
  if they cannot race mainline QA, other agents, CoreAudio reloads, or USB
  recovery work.
- `test-hal-candidate-safety` performs HAL install/reload and `coreaudiod`
  restart, and `run-soundcheck` plays/captures audio; both must be gated before
  any further physical evidence can be trusted.

Alternatives discarded:
- Rely on operator discipline or docs only: rejected because previous hardware
  work already exposed lock/process coordination risk.
- Put lock only in outer manual commands: rejected because direct script
  invocation would still be unsafe.

Evidence:
- `local-analysis/cpp-offline/hardware-lock-policy.json` records `PASS`,
  `4` audited scripts, and `0` missing requirements.
- `local-analysis/cpp-offline/current-offline-gates.json` records
  `hardware_lock_policy=PASS`.

## 2026-06-16: Require C++ Mode 2 Bytes To Match The Python Oracle

Decision:
- Add a cross-oracle gate that compares C++ Mode 2 packed output bytes against the inherited Python oracle for the full 72-row matrix.
- Match C++ Float32-to-S24 quantization to the oracle's Float32 rounding path.

Reason:
- The previous C++ packet matrix proved internal round-trip behavior but could still pass if pack and decode shared the same wrong quantization.
- The Python oracle is independent and was inherited from the proven mainline behavior checks. Byte-for-byte parity is stronger evidence than round-trip alone.

Alternatives discarded:
- Keep only the Python text oracle: rejected because it did not execute C++.
- Keep only C++ round-trip tests: rejected because they do not prove parity with the external oracle.

Evidence:
- `local-analysis/cpp-offline/mode2-cross-oracle-parity.json` records `72` rows, `0` failures, `max_byte_mismatches=0`, `max_length_delta=0`, `total_check_errors=0`, and `total_panic_flags=0`.
- `local-analysis/promotion-readiness-current.json` includes `offline_mode2_cross_oracle_parity=PASS`.

## 2026-06-16: Create Isolated C++/DriverKit Worktree

Decision:
- Use `/Users/fer/dev/audio8djcpp` on branch `driverkit/cpp-redesign`.

Reason:
- The C/Objective-C mainline and Rust experiment must remain read-only references.

Alternatives discarded:
- Editing `/Users/fer/dev/opena8dj`: rejected by isolation rule.
- Editing `/Users/fer/dev/audio8djrust`: rejected by isolation rule.

Evidence:
- `git worktree list --porcelain` shows a separate C++ worktree and branch.

## 2026-06-16: Start With Pure C++ Offline Core

Decision:
- Build a C++20 core first, with DriverKit only as a prepared shell.

Reason:
- Offline gates can validate packet, routing, metrics, and policy behavior without installing drivers or touching hardware.

Alternatives discarded:
- Direct DriverKit implementation first: rejected because signing, entitlement, install, and activation risk would obscure core behavior.
- Blind C port: rejected because the intended strategy is greenfield shell, brownfield behavior.

Evidence:
- `opena8djcpp_core_contract` validates initial channel/routing/surface contracts offline.

## 2026-06-16: No Readiness Claim From Compile Alone

Decision:
- A clean build is evidence only for build health, not product readiness.

Reason:
- Audio quality, jitter, routing, timecode, and physical behavior require measurable gates and later coordinated hardware tests.

Alternatives discarded:
- Declaring physical readiness after scaffold: rejected.

Evidence:
- `docs/SUCCESS_METRICS.md` and `docs/TEST_PLAN.md` define required gates.

## 2026-06-16: Split Functional And Performance Gates By Build Type

Decision:
- Keep the default build as a safe functional contract gate.
- Run performance/resource gates only in `Release` or `RelWithDebInfo`.

Reason:
- Debug/no-optimization throughput is not a valid comparison against mainline or Rust oracle performance.
- Release CTest now includes `opena8djcpp_offline_bench`, which fails if pack/decode are below `100 MiB/s`, routing is below `1,000,000 frames/s`, or Mode 2 check/panic counters are nonzero.

Alternatives discarded:
- Treat Debug benchmark failure as product failure: rejected because it measures compiler mode more than data-plane design.
- Hide performance from CTest entirely: rejected because objective performance evidence must block claims.

Evidence:
- `local-analysis/cpp-offline/offline-bench-release.json` records `pack_mib_s=924.305`, `decode_mib_s=499.804`, `route_frames_s=5.17603e+08`, `check_errors=0`, and `panic_flags=0`.

## 2026-06-16: Keep Start Byte 4 For Physical Output

Decision:
- Keep `OPENA8DJ_OUTPUT_START_BYTE=4` for the current HAL transport candidate.

Reason:
- Long physical tone capture with `ISO5/q64/start_byte=4/big/gain0.50` measured
  a clean 1 kHz tone through iRig: `sideband_ratio=0.000657`, strongest sideband
  `-64.78 dB`, `click_outliers=0`, no clipping.
- `start_byte=2` produced much higher output level but worse tone quality:
  `sideband_ratio=0.080717`, residual ratio `0.985763`, and peak at `1.0`.
- `ISO64/q8` was rejected immediately for physical output because the same tone
  worsened to `sideband_ratio=0.791833` and `click_outliers=372`.

Alternatives discarded:
- `start_byte=2`: rejected because it behaves like malformed audio despite
  loud signal.
- `ISO64/q8`: rejected because it catastrophically worsened sidebands/clicks.

Evidence:
- `local-analysis/hardware-quality/20260616-170024-start-byte-2v4-tone-long/start-byte-4/tone-analysis.txt`
- `local-analysis/hardware-quality/20260616-170024-start-byte-2v4-tone-long/start-byte-2/tone-analysis.txt`
- `local-analysis/hardware-quality/20260616-165746-iso64-q8-irig-tone-pairA/tone-analysis.txt`

## 2026-06-16: Do Not Claim Physical Readiness From Current iRig Results

Decision:
- The current C++/HAL candidate is not ready to claim better-than-mainline
  physical quality or low-resource operation.

Reason:
- The best real-music iRig gate still fails strict thresholds:
  `quality_alignment_score=0.938154`, `analog_snr_db=8.93`,
  `mid_band_residual_ratio=1.379896`, `high_band_residual_ratio=1.347577`,
  `lag_jumps_gt_2_frames=24`.
- CPU evidence is not yet clean: the same run recorded `opena8dj_driver_p95=11.5`
  and `coreaudiod_p95=95.8` in the profiling window, which is not acceptable as
  a low-resource claim without a controlled baseline comparison.
- Removing cadence diagnostics caused a CoreAudio enumeration hang during a tone
  attempt; the system had to be recovered by reinstalling the cadence-diagnostic
  variant.

Alternatives discarded:
- Declaring readiness from clean tone sidebands: rejected because tone quality
  does not cover real-music residuals, lag jumps, CPU, or Traktor/timecode.

Evidence:
- `local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4/summary.txt`
- `local-analysis/hardware-quality/20260616-170759-recover-install-iso5-q64-start4-cadence`

## 2026-06-16: Benchmark Preallocated Mode 2 Decode As The Real-Time Metric

Decision:
- Treat `decode_mode2_usb_bytes_into` as the primary offline decode performance
  metric because it writes into caller-owned storage and matches the intended
  real-time data-plane policy.
- Keep the allocating wrapper benchmark as informational only.

Reason:
- The previous benchmark measured `decode_mode2_usb_bytes`, which resizes a
  `std::vector` and can mix harness allocation cost with decode cost.
- The real-time path must use preallocated storage, zero hot-loop allocations,
  and bounded work.
- The decode hot loop also used per-byte modulo operations for transfer/group
  position. Replacing them with bounded counters preserves behavior while
  reducing arithmetic variance.

Alternatives discarded:
- Continue using the allocating wrapper as the product metric: rejected because
  it does not model the callback-safe path.
- Skip the allocating wrapper entirely: rejected because it remains useful for
  tests and developer tooling.

Evidence:
- `local-analysis/cpp-offline/offline-bench-release.json` records
  `decode_into_mib_s=570.085`, `decode_allocating_mib_s=552.130`,
  `decode_into_output_overflows=0`, `decode_into_check_errors=0`, and
  `decode_into_panic_flags=0`.
- `scripts/run-cpp-offline-gates` passed default CTest `8/8`, Release CTest
  `9/9`, packet matrix `72/72`, Python Mode 2 oracle, timecode matrix, DVS
  signal smoke, realtime audit, jitter model, static policy, and evidence
  schema after the change.

## 2026-06-16: Reject Packer Counter Rewrite After Measured Regression

Decision:
- Keep the original `Mode2OutputPacker::fill_into` modulo/check-byte structure
  for now.

Reason:
- A counter-based rewrite looked cleaner but degraded measured pack throughput
  to about `1149.74 MiB/s` in the same Release benchmark flow, versus the
  reverted final median of `1602.11 MiB/s`.
- Real-time code changes must be accepted by evidence, not by aesthetics.

Alternatives discarded:
- Shipping the counter rewrite: rejected because it made the hot output packer
  materially slower.

Evidence:
- Final post-revert `local-analysis/cpp-offline/offline-bench-release.json`
  records `pack_mib_s=1602.11`, `decode_into_mib_s=570.085`,
  `float_to_s24_frames_s=85096700`, `route_frames_s=950086000`, and
  `route_reversed_frames_s=581896000`.

## 2026-06-16: Add Offline DriverKit Shell Contract Without Dext Claim

Decision:
- Compile and test a DriverKit shell lifecycle contract as ordinary C++ while
  the machine lacks DriverKit SDK/AudioDriverKit framework support.

Reason:
- The previous `driverkit/` code was a placeholder with empty start/stop
  methods. That is not enough for architecture confidence, even though a real
  dext build is still blocked by local toolchain/entitlement constraints.
- The offline shell validates device model ownership and bounded lifecycle
  behavior without installing, activating, or reloading any System Extension.

Alternatives discarded:
- Declare DriverKit readiness from the surface model alone: rejected because it
  did not execute any shell code.
- Attempt real dext install/activation now: rejected because the current
  toolchain lacks DriverKit SDK and hardware/system-extension operations require
  a separate authorized window.

Evidence:
- `local-analysis/cpp-offline/driverkit-shell-contract.json` records PASS with
  `system_extension_activated=false`.
- `local-analysis/promotion-readiness-current.json` now includes
  `offline_driverkit_shell_contract=PASS` but still returns FAIL overall because
  physical music quality, runtime CPU, and physical Traktor/timecode vinyl are
  not proven.

## 2026-06-16: Require DVS Claims To Traverse Mode 2 Input Decode

Decision:
- Offline DVS/timecode evidence must include a path that packs synthetic input
  into Mode 2 USB bytes, decodes those bytes through the selected input profile,
  and then runs the timecode analyzer on the decoded deck pair.

Reason:
- Direct synthetic signal analysis proves analyzer thresholds, but it does not
  prove the packet/input decode path feeding Traktor-facing data.
- Playback mode must keep input decode disabled while still preserving packet
  stats for observability.
- Timecode-vinyl, CD-line, and phono modes must explicitly enable decode and
  preserve A/B/C/D deck isolation.

Alternatives discarded:
- Treat `dvs-signal-smoke` alone as DVS readiness: rejected because it bypasses
  Mode 2 input decode.
- Always decode input in playback profile: rejected because playback mode should
  not spend data-plane work on timecode capture unless the profile enables it.

Evidence:
- `local-analysis/cpp-offline/dvs-packet-input-decode.json` records `24` rows,
  `0` failures, zero leakage, no decode overflows, and playback decode-off PASS.
- `local-analysis/promotion-readiness-current.json` includes
  `offline_dvs_packet_input_decode=PASS` but still returns FAIL overall because
  physical real-music quality, runtime CPU, and physical Traktor/timecode vinyl
  are not proven.

## 2026-06-16: Represent Full Routing As Fixed Route Entries

Decision:
- Extend C++ `RoutingMatrix` from source-channel-only mapping to fixed
  `RouteEntry` rows with source channel and precomputed gain `1`, `0`, or `-1`.

Reason:
- The Rust oracle explicitly covers identity, muted channels, pair remap,
  side swap, and inversion. C++ had only identity/reversed source mapping.
- Full A/B/C/D routing and DVS workflows need mute/invert/cross-deck behavior
  without introducing dynamic storage in the hot path.

Alternatives discarded:
- Keep source-only routing: rejected because it cannot represent mute or
  inversion.
- Add per-frame dynamic routing decisions: rejected because routing policy must
  be prevalidated outside the real-time path.

Evidence:
- `opena8djcpp_core_contract` passes the Rust-oracle compound case: A from D,
  B muted, C side-swapped, and D right inverted.
- `local-analysis/cpp-offline/offline-bench-release.json` records
  `route_advanced_frames_s=2.71652e+08`.
- `scripts/evaluate-promotion-readiness.py` now requires advanced routing
  throughput above the offline floor before promotion can pass.

## 2026-06-16: Distinguish Mode 2 Check Cadence From Full Frame Bytes

Decision:
- Keep two separate constants in the C++ protocol model: Mode 2 check cadence
  is `16` bytes, while one full 8-channel audio frame is `32` bytes.

Reason:
- Mainline behavior and packet checks operate on a 16-byte cadence, but the
  product data model still represents a full 8-channel frame as 32 USB bytes.
- The new protocol contract initially caught this ambiguity. Keeping both
  names prevents later code from treating the check cadence as the whole audio
  frame or vice versa.

Alternatives discarded:
- Collapse both meanings into `kMode2GroupBytes`: rejected because it hides a
  real protocol distinction and would make future DriverKit/USB transport code
  easier to misread.
- Advertise 88.2/96 kHz as supported now: rejected until physical quality
  evidence exists for those rates; the CAIAQ rate codes are modeled but marked
  deferred.

Evidence:
- `local-analysis/cpp-offline/protocol-contract.json` records PASS with
  `check_cadence_bytes=16`, `full_frame_bytes=32`,
  `default_start_byte=4`, VID/PID `0x17cc:0x1978`, endpoints
  `0x01/0x81/0x82/0x06`, and 8 input/output channels.
- `scripts/evaluate-promotion-readiness.py` now requires this protocol
  contract before branch promotion can pass.

## 2026-06-16: Require Full Simulated Output Matrix Before Promotion

Decision:
- Add a deterministic C++ simulated-output matrix gate covering output pairs
  A/B/C/D at 44.1/48 kHz with dense, transient, and wideband material at gains
  `1.0` and `0.5`.

Reason:
- The previous simulated output oracle was one strong software-output fixture,
  but it did not prove all output pairs and required rates through the C++ Mode
  2 pack/decode path.
- A deterministic in-process C++ gate avoids dependence on local music files
  while still checking alignment, quantization residual, SNR, check bytes,
  panic flags, and wrong-pair leakage.

Alternatives discarded:
- Depend on `scripts/run-simulated-output-soundcheck` inside the core offline
  gate: rejected because it may depend on local music discovery/conversion and
  is slower/noisier than a deterministic core contract.
- Treat packet matrix as sufficient output-quality evidence: rejected because
  it checks byte parity but not program-material SNR/residual/leakage metrics.

Evidence:
- `local-analysis/cpp-offline/simulated-output-matrix.json` records PASS with
  `48` rows, `0` failures, minimum SNR `119.407 dB`, max residual ratio
  `1.07069e-06`, and max leakage `-240 dBFS`.
- `scripts/evaluate-promotion-readiness.py` now requires
  `offline_simulated_output_matrix=PASS` before branch promotion can pass.

## 2026-06-16: Keep Physical Readiness Blocked After USB Tone/Music Failure

Decision:
- Do not promote the C++ candidate and do not claim hardware/listening
  readiness. Keep Mode 2 output defaults at big-endian, start byte `4`, check
  offset `8`, but make the direct USB diagnostic tool build with the same
  `HAL_CFLAGS` as the HAL bundle. Promotion evaluation must also consume the
  latest physical investigation summary, so negative current evidence overrides
  older passing runs.

Reason:
- Locked physical evidence showed the iRig route is available and clean when
  idle, but Audio 8 playback through the C++ HAL/direct USB path produces
  severe analog mismatch/noise.
- Start-byte, endian, check-offset, valid-capture-layout, and ISO-frame sweeps
  did not produce a passing physical tone. The best meaningful direct tone
  evidence remains far below product threshold, around `-17 dB` to `-19.5 dB`
  tone SNR depending on variant.
- The prior direct USB tool was not compiled with HAL flags, so its evidence
  could mislead future debugging unless the build was fixed.
- The previous promotion evaluator defaulted to an older physical music run;
  that made the report less conservative than the actual latest hardware state.

Alternatives discarded:
- Promote because offline gates pass: rejected because physical music quality,
  runtime CPU, and Traktor/timecode physical evidence still fail.
- Change default check offset away from `8`: rejected because the sweep did not
  improve quality and mainline notes still identify offset `8` as the least bad
  playback-compatible value.
- Enable valid-capture-layout by default immediately: rejected because the
  same-amplitude comparison improved tone SNR by only about `0.25 dB`, not a
  defensible product improvement.

Evidence:
- `local-analysis/usb-physical-investigation-summary.json`
- `local-analysis/soundcheck/20260616-185543-irig-pairA-24s-cpp-hal`
- `local-analysis/irig-baseline/20260616-191203-no-playback-9s`
- `local-analysis/start-byte-sweep/20260616-190418-tone1k-minus36db`
- `local-analysis/start-byte-sweep/20260616-190657-tone1k-minus36db-native-i24`
- `local-analysis/check-offset-sweep/20260616-191616-validlayout-tone1k-minus18db`
- `local-analysis/direct-usb-soundcheck/20260616-191418-validlayout-tone1k-minus18db`
- `local-analysis/direct-usb-soundcheck/20260616-191931-halflags-tone1k-minus18db`
- `local-analysis/runtime-isolation/current-after-usb-investigation.json`
- `local-analysis/promotion-readiness-current.json`

## 2026-06-16: Align C++ HAL Transport Defaults With Mainline 0.3.135 Baseline

Decision:
- Change C++ HAL build defaults from ISO5 with 64/64 capture/playback queues to
  ISO64 with 8/8 queues, and expose `OPENA8DJ_OUTPUT_PREFETCH_FRAMES` with a
  default build value of 64.

Reason:
- The latest mainline physical/CPU baseline documented for `0.3.135` used
  `HAL_ISO_FRAMES=64`, `HAL_CAPTURE_QUEUE=8`, `HAL_PLAYBACK_QUEUE=8`, and
  `HAL_OUTPUT_PREFETCH_FRAMES=64`.
- The C++ candidate had diverged to ISO5 and 64/64 queues, which changes USB
  cadence, transfer size, in-flight behavior, and callback load. Given the
  latest physical C++ failure plus driver CPU around `33-35%`, the next
  candidate should first match the known mainline transport profile before more
  speculative packet experiments.

Alternatives discarded:
- Keep ISO5 because one direct tone comparison beat ISO8: rejected because that
  result was still a severe physical FAIL and did not compare against the
  actual mainline ISO64 baseline.
- Continue changing packet byte offsets first: rejected because offset/endian
  sweeps did not find a passing physical tone.

Evidence:
- `/Users/fer/dev/opena8dj/Makefile` read-only defaults:
  `HAL_ISO_FRAMES=64`, `HAL_CAPTURE_QUEUE=8`, `HAL_PLAYBACK_QUEUE=8`,
  `HAL_OUTPUT_PREFETCH_FRAMES=64`.
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md` documents the
  `0.3.135` CPU/digital stability baseline using ISO64/8/8.
- `local-analysis/usb-physical-investigation-summary.json` records the C++
  ISO5-era physical failure.

Follow-up:
- The locked ISO64 C++ HAL run failed worse than the prior ISO5 run:
  `quality_alignment_score=0.051643`, `analog_snr_db=-31.90`,
  `lag_jumps_gt_2_frames=60`.
- ISO64/8/8 is rejected as a C++ default until the remaining lifecycle
  differences are ported and retested. The C++ default returns to ISO5/64/64,
  the less-bad measured profile.

Evidence:
- `local-analysis/soundcheck/20260616-iso64-irig-pairA-16s-cpp-hal`
- `local-analysis/runtime-isolation/post-iso64-failed-unload.json`
- `local-analysis/audio-stack-guard/20260616-iso64-force-unload/force-unload.log`

## 2026-06-16: Add HAL Transport Lifecycle Parity Knobs Before More Physical Sweeps

Decision:
- Add C++ HAL build-time support for playback request coalescing, configurable
  playback-before-capture-requeue ordering, and transfer-pool cursor checkout.
- Keep all new lifecycle knobs neutral by default:
  `HAL_PLAYBACK_COALESCE_TRANSFERS=1`,
  `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=0`, and
  `HAL_TRANSFER_POOL_CURSOR=0`.

Reason:
- Read-only mainline comparison found lifecycle-level transport behavior that
  C++ did not yet model, while ISO64/8/8/prefetch64 was already physically
  rejected in the C++ implementation.
- More byte/start/check sweeps are low-value until the C++ transport can test
  the same lifecycle degrees of freedom mainline uses.
- Neutral defaults preserve the current less-bad measured profile while making
  future physical variants explicit and reproducible.

Alternatives discarded:
- Promote because offline gates pass: rejected; physical music quality and CPU
  still fail.
- Keep changing USB packet offsets first: rejected; recent physical sweeps did
  not produce a passing analog result.
- Enable coalescing/cursor by default immediately: rejected; those variants need
  locked physical evidence before becoming candidate defaults.

Evidence:
- Build: `make usb-play hal` passed.
- Offline gates: `scripts/run-cpp-offline-gates` passed Debug `15/15` and
  Release `16/16`.
- Promotion gate: `scripts/evaluate-promotion-readiness.py --json-out
  local-analysis/promotion-readiness-current.json` returned `FAIL` as expected.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-current.json`

Follow-up:
- Locked physical evidence at commit `e0ad0a0` improved from the ISO64
  regression but still failed product thresholds:
  `quality_alignment_score=0.934891`, `analog_snr_db=8.73`,
  `lag_jumps_gt_2_frames=25`, driver CPU p95 `36.0%`.

Evidence:
- `local-analysis/soundcheck/20260616-lifecycle-irig-pairA-16s-cpp-hal`
- `local-analysis/runtime-isolation/post-lifecycle-failed-unload.json`
- `local-analysis/audio-stack-guard/20260616-lifecycle-force-unload/force-unload.log`
- `local-analysis/promotion-readiness-current.json`

## 2026-06-16: Gate USB Input Decode On Actual Input IO

Decision:
- Port mainline-style input decode activation to the C++ HAL/USB bridge:
  `OpenA8DJUSBSetInputDecodeActive`, activation from HAL `PrepareInputCycle`,
  deactivation on final `StopIO`, and capture decode bypass when no input
  client is active.

Reason:
- The failed lifecycle candidate still showed OpenA8DJ driver CPU p95 around
  `36%` during output-only soundcheck. C++ was decoding input traffic even when
  the active physical test only used Audio 8 DJ output and iRig capture.
- Mainline gates input decode from HAL input cycles, so this is behavior parity
  and a plausible resource reduction without changing packet layout or output
  format.

Alternatives discarded:
- Continue with packet byte sweeps: rejected because the latest failure is now
  dominated by CPU/resource waste plus residual analog quality, and recent
  offset/layout sweeps did not pass.
- Disable input decode globally: rejected because timecode vinyl and Traktor
  require input routing and DVS capture once those tests run.

Evidence:
- Build: `make usb-play hal` passed.
- Offline gates: `scripts/run-cpp-offline-gates` passed Debug `15/15` and
  Release `16/16`.
- Evidence path: `local-analysis/cpp-offline/current-offline-gates.json`

Follow-up:
- Locked physical retest of active input decode gating failed worse than the
  lifecycle candidate: `quality_alignment_score=0.028314`,
  `analog_snr_db=-28.18`, `lag_jumps_gt_2_frames=52`, OpenA8DJ driver CPU p95
  `41.8%`, CoreAudio p95 `21.8%`.
- Decision amended: keep the API/knob for explicit future experiments, but set
  `HAL_INPUT_DECODE_ACTIVE_GATING=0` by default. The current default candidate
  must not include this physically rejected behavior.

Evidence:
- `local-analysis/soundcheck/20260616-inputdecode-irig-pairA-16s-cpp-hal`
- `local-analysis/audio-stack-guard/20260616-inputdecode-force-unload/force-unload.log`
- `local-analysis/runtime-isolation/post-inputdecode-failed-unload.json`
- `local-analysis/promotion-readiness-current.json`

## 2026-06-16: Reject Playback-Before-Capture-Requeue Variant

Decision:
- Keep `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=0` as the default.

Reason:
- The variant compiled and passed offline gates, but locked physical evidence
  was worse than the lifecycle baseline and still far below product thresholds.
- It did not solve runtime CPU: OpenA8DJ driver p95 remained `36.0%`, while
  CoreAudio p95 spiked to `74.0%` in the measured run.

Evidence:
- `local-analysis/hal-candidate-safety/20260616-queue-before-cpp-lockpolicy-leave-loaded`
- `local-analysis/soundcheck/20260616-queue-before-irig-pairA-16s-cpp-hal`
- `local-analysis/audio-stack-guard/20260616-queue-before-force-unload/force-unload.log`
- `local-analysis/runtime-isolation/post-queue-before-failed-unload.json`
- `local-analysis/promotion-readiness-current.json`

## 2026-06-16: Reject Transfer-Pool Cursor Variant For Safety

Decision:
- Keep `HAL_TRANSFER_POOL_CURSOR=0` as the default and do not run physical audio
  with `HAL_TRANSFER_POOL_CURSOR=1` in the current code.

Reason:
- The variant passed offline gates, but failed the HAL candidate safety gate
  before any soundcheck.
- During the safety load/enumeration window, the OpenA8DJ driver process was
  nearly idle (`0.1%`), while `coreaudiod` spiked to `172.2%`. That is a system
  safety failure and not an acceptable basis for an audio-quality run.
- The safety script unloaded the HAL and recovery passed, so this was contained
  without leaving the system in a dirty state.

Alternatives discarded:
- Run soundcheck anyway: rejected because the safety gate exists specifically to
  block high-risk audio-stack states before physical playback/capture.
- Treat the spike as harmless transitory load: rejected until a bounded
  lifecycle explanation and a repeatable safe-load metric prove it.

Evidence:
- `local-analysis/hal-candidate-safety/20260616-pool-cursor-cpp-lockpolicy-leave-loaded`
- `local-analysis/runtime-isolation/pre-pool-cursor-physical.json`

## 2026-06-16: Disable Output Amplitude Stats By Default

Decision:
- Set C++ `HAL_OUTPUT_AMPLITUDE_STATS=0` by default while keeping the knob
  available for explicit diagnostic builds.

Reason:
- Mainline and Rust-derived notes both keep output amplitude stats disabled by
  default.
- C++ had the flag enabled by default, which adds per-output-frame peak and
  clipping work in the USB output fill path.
- The current blocker includes high driver CPU during output-only physical
  soundchecks, so default candidates should not carry nonessential hot-path
  diagnostics.

Alternatives discarded:
- Remove amplitude stats entirely: rejected because the field is still useful
  in targeted diagnostics.
- Keep stats enabled until quality is fixed: rejected because CPU/resource
  superiority is a product gate, and this default diverges from mainline.

Evidence:
- C++ default before change: `Makefile: HAL_OUTPUT_AMPLITUDE_STATS ?= 1`.
- Mainline default: `/Users/fer/dev/opena8dj/Makefile:
  HAL_OUTPUT_AMPLITUDE_STATS ?= 0`.

## 2026-06-16: Reject Mainline Preopen/Stop-ISOC Lifecycle Defaults In C++

Decision:
- Keep the newly ported lifecycle knobs available, but set
  `HAL_BACKGROUND_PREOPEN_ON_INIT=0` and `HAL_STOP_ISOC_ON_STOP=0` by default
  after physical rejection.

Reason:
- The mainline-style lifecycle candidate passed safety and offline gates, but
  locked physical music capture regressed severely:
  `quality_alignment_score=0.159859`, `analog_snr_db=-16.87`, and
  `lag_jumps_gt_2_frames=59`.
- This is worse than the prior less-bad lifecycle baseline and cannot be used
  as a default or promotion candidate.

Alternatives discarded:
- Keep the defaults because they match mainline: rejected because the C++ host
  lifecycle is not yet equivalent enough for those defaults to be safe.
- Run a longer soundcheck: rejected because the short controlled run already
  shows a severe failure and the gates are designed to stop there.

Evidence:
- `local-analysis/hal-candidate-safety/20260616-lifecycle-preopen-cpp-lockpolicy-leave-loaded`
- `local-analysis/soundcheck/20260616-lifecycle-preopen-irig-pairA-16s-cpp-hal`
- `local-analysis/audio-stack-guard/20260616-lifecycle-preopen-force-unload/force-unload.log`
- `local-analysis/runtime-isolation/post-lifecycle-preopen-failed-unload.json`

## 2026-06-16: Reject Fast Prefetch Clear And Atomic Write Stats Defaults

Decision:
- Keep the code paths available, but set `HAL_FAST_OUTPUT_PREFETCH_CLEAR=0`
  and `HAL_OUTPUT_WRITE_STATS=0` by default.

Reason:
- The isolated fast-clear/write-stats candidate passed offline and safety
  gates, but locked physical capture regressed severely:
  `quality_alignment_score=-0.048481`, `analog_snr_db=-32.06`,
  `lag_jumps_gt_2_frames=46`.
- The failure is too large to treat as normal run variance. It must not be a
  candidate until a targeted explanation exists.

Alternatives discarded:
- Keep these defaults because they are CPU optimizations: rejected because
  product quality gates outrank theoretical CPU savings.
- Keep only atomic write stats on: deferred until a separate controlled
  one-factor run is justified; the combined run was not safe enough to promote.

Evidence:
- `local-analysis/hal-candidate-safety/20260616-fastclear-writestats-cpp-lockpolicy-leave-loaded`
- `local-analysis/soundcheck/20260616-fastclear-writestats-irig-pairA-16s-cpp-hal`
- `local-analysis/audio-stack-guard/20260616-fastclear-writestats-force-unload/force-unload.log`
- `local-analysis/runtime-isolation/post-fastclear-writestats-failed-unload.json`

## 2026-06-16: Reject Calibrated Default And Direct USB Plain Variants

Decision:
- Keep C++ in `NOT_READY` status after the calibrated `-16 dB` physical run
  and two direct USB plain-CFLAGS diagnostics.
- Do not promote, do not claim better-than-mainline, and do not move C++ toward
  `main` until physical quality and CPU both beat the C baseline.

Reason:
- Calibrated HAL physical run still failed:
  `quality_alignment_score=0.960076`, `analog_snr_db=2.71`,
  `lag_jumps_gt_2_frames=35`, `mid_band_residual_ratio=1.565287`,
  `high_band_residual_ratio=1.461400`.
- Runtime CPU was worse than mainline `0.3.135`: C++ driver avg/p95
  `30.73%/36.70%`, while the C baseline documents driver p95 around `6.5%`.
- Direct USB plain-CFLAGS did not rescue quality:
  plain CFLAGS produced `quality_alignment_score=0.186400` with clipping, and
  plain CFLAGS plus only `OPENA8DJ_OUTPUT_GAIN=0.50f` produced
  `quality_alignment_score=0.023502`.
- Active-section stream stats in the HAL run did not show simple render
  starvation; the ring stayed near target and active underruns remained zero
  during the fed section.

Alternatives discarded:
- Treat earlier failures as overdrive artifacts: rejected because `-16 dB`
  still failed without clipping in the HAL path.
- Blame only `HAL_CFLAGS` contamination in `usb-play`: rejected because the
  plain-gain05 direct USB diagnostic still failed badly.
- Promote based on offline gates: rejected because offline gates currently do
  not predict physical DAC/capture quality for this transport.

Evidence:
- `local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal`
- `local-analysis/direct-usb-soundcheck/20260616-plaincflags-minus16-music`
- `local-analysis/direct-usb-soundcheck/20260616-plaincflags-gain05-minus16-music`
- `local-analysis/runtime-isolation/post-default-minus16-failed-unload.json`

Follow-up:
- Prioritize below-HAL USB transport/cadence and device-state differences.
- Add byte-for-byte packer dumps and/or device-observed transfer diagnostics
  before more physical sweeps.

## 2026-06-16: Treat Mode 2 Packer Layout As Offline-Parity Verified

Decision:
- Do not spend the next physical window on more start-byte/check-offset layout
  sweeps unless new device-observed evidence contradicts the offline parity
  model.

Reason:
- Added an independent `opena8djcpp_mode2_mainline_layout_parity` gate that
  implements the mainline unrolled Mode 2 fill structure separately from the
  C++ `Mode2OutputPacker`.
- The gate passed `132` rows across start bytes `0..5`, short partial transfer
  sizes, and normal transfer sizes including `352`.
- This removes one important false-positive risk in the old offline oracle:
  packer/decode self-consistency is no longer the only byte-layout evidence.

Alternatives discarded:
- Continue byte-offset physical sweeps now: rejected because direct physical
  sweeps already failed and the independent byte layout gate now passes.
- Declare physical readiness from layout parity: rejected because physical iRig
  quality and CPU still fail badly.

Evidence:
- `local-analysis/cpp-offline/mode2-mainline-layout-parity.json`
- `scripts/run-cpp-offline-gates` with Debug `16/16`, Release `17/17`.

Follow-up:
- Focus on transfer cadence, USB transaction state, `AUDIO_PARAMS` reset
  behavior, and device-observed control/stream state.

## 2026-06-17: Add Hot Stream Stats Gate And Restore Atomic Output Write Stats

Decision:
- Port mainline-style `OPENA8DJ_ENABLE_HOT_STREAM_STATS` and
  `OPENA8DJ_HOT_STREAM_STATS_INTERVAL` gates to the C++ HAL, with default
  behavior still equivalent to the current per-completion stats path
  (`enabled=1`, `interval=1`).
- Change C++ `HAL_OUTPUT_WRITE_STATS` default from `0` to `1`, matching
  mainline atomic-only accounting and removing a useless mutex path where
  `addStreamStatAtOffset(outputFramesWritten)` was later overwritten by the
  atomic snapshot value.
- Add `outputLateWriteFrames` and `outputLateWriteBatches` observability to
  the C++ stream stats payload and control tool, matching mainline semantics.

Reason:
- Read-only comparison found C++ was paying a stream-stats mutex on every
  output write when `HAL_OUTPUT_WRITE_STATS=0`, but `streamStatsSnapshot`
  always overwrote `outputFramesWritten` from `_outputFramesWrittenAtomic`.
  That made the fallback both hot-path cost and unobservable.
- Mainline already uses atomic output write stats by default and reports late
  writes. C++ was missing late-write counters, leaving a plausible path where
  physical quality could be bad while output counters looked clean.
- The hot stream-stats gate gives controlled CPU/observability experiments
  without changing the default physical candidate behavior.

Alternatives discarded:
- Disable hot stream stats by default now: rejected until a locked physical run
  proves lower CPU without hiding required failure evidence.
- Treat the old fast-clear/write-stats physical rejection as proof against
  atomic write stats alone: rejected because that run changed two variables.
  `HAL_FAST_OUTPUT_PREFETCH_CLEAR=0` remains the default.

Evidence:
- `make usb-play hal`
- `make HAL_HOT_STREAM_STATS=0 usb-play hal`
- `make HAL_OUTPUT_WRITE_STATS=0 usb-play hal`
- `scripts/run-cpp-offline-gates` with Debug `16/16`, Release `17/17`.
- Logs:
  - `local-analysis/build-flags/hot-stats-off-build.log`
  - `local-analysis/build-flags/output-write-stats-off-build.log`
  - `local-analysis/build-flags/default-after-hot-stats-output-write-build.log`
  - `local-analysis/offline-gates-after-hot-stats-output-write.log`

Follow-up:
- Next locked physical candidate should verify whether atomic write stats plus
  late-write observability improves CPU diagnostics and whether any late writes
  correlate with iRig quality failure.
- If quality still fails, prioritize the other read-only findings: queue depth
  `64/64` versus mainline `8/8`, output prefetch `256` versus mainline `64`,
  and input/control-plane parity.

## 2026-06-17: Reject Hot-Stats/Atomic-Write Candidate As Not Ready

Decision:
- Do not promote commit `5e6fab7` and do not claim physical readiness after the
  locked iRig music run.
- Keep the code/tooling improvements, but continue optimization because the
  candidate still fails music quality and CPU gates.

Reason:
- Safety install passed, but physical soundcheck failed:
  `quality_alignment_score=0.962133`, `snr_db=10.24`,
  `click_outliers=29`, `lag_jumps_gt_2_frames=45`,
  `mid_band_residual_ratio=1.443461`,
  `high_band_residual_ratio=1.362932`, quiet mid-band noise `-35.91 dBFS`.
- CPU remains above mainline: OpenA8DJ driver avg/p95 `31.58%/36.00%`,
  coreaudiod avg/p95 `4.70%/7.00%`.
- Stream stats showed output write accounting is now observable and no active
  underruns/timeline resets/panic flags, but late-write text snapshots were
  all zero. Late writes do not explain this failure.

Alternatives discarded:
- Promote based on slight SNR/CoreAudio CPU improvement over the previous
  calibrated run: rejected because all product gates still fail and driver CPU
  is far above the C baseline.
- Retest the same candidate immediately: rejected until a new variable changes.

Evidence:
- Safety: `local-analysis/physical-hotstats-write-late/20260616-205111/hal-candidate-safety`
- Soundcheck: `local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal`
- Stream stats summary: `local-analysis/stream-stats/hotstats-write-late-summary-v2.json`
- Recovery/audit:
  - `local-analysis/audio-stack-guard/20260616-force-unload-hotstats-write-late`
  - `local-analysis/runtime-isolation/post-hotstats-write-late-failed-unload.json`

Follow-up:
- Fix structured TSV capture for late-write counters before the next physical
  run.
- Next one-factor physical hypotheses: reduce queue depth to mainline `8/8`,
  or reduce output prefetch to mainline `64`.

## 2026-06-17: Reject Queue-Depth 8/8 As Standalone Candidate

Decision:
- Do not change the default queue depths from C++ `64/64` to mainline `8/8`
  based on the standalone physical run.
- Keep `queue8` as a diagnostic clue only: it reduced click outliers, but did
  not beat quality or runtime CPU gates.

Reason:
- Physical soundcheck with `HAL_CAPTURE_QUEUE=8 HAL_PLAYBACK_QUEUE=8` still
  failed:
  `quality_alignment_score=0.964133`, `snr_db=10.22`,
  `lag_jumps_gt_2_frames=39`, `mid_band_residual_ratio=1.422599`,
  `high_band_residual_ratio=1.365050`, quiet mid-band noise `-36.08 dBFS`.
- Click outliers improved from `29` to `0`, and coreaudiod p95 improved from
  `7.0%` to `3.1%` versus the immediately preceding hotstats run.
- OpenA8DJ driver CPU worsened: avg/p95/max `32.335%/37.2%/37.9%`, still far
  above the mainline C baseline.
- Stream stats showed no late writes, no active underruns, no timeline resets,
  no panic flags, and capture transaction error ratio stayed around `2.273`.

Alternatives discarded:
- Promote `queue8` because clicks improved: rejected because SNR/residual/lag
  and driver CPU still fail.
- Make `queue8` default before testing prefetch/cadence: rejected because the
  driver CPU regression is material.

Evidence:
- Build/safety: `local-analysis/physical-queue8/20260616-205510`
- Soundcheck: `local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal`
- Stream stats: `local-analysis/stream-stats/queue8-summary.json`
- Recovery/audit:
  - `local-analysis/audio-stack-guard/20260616-force-unload-queue8`
  - `local-analysis/runtime-isolation/post-queue8-failed-unload.json`
- Default rebuild restored `HAL_CAPTURE_QUEUE_DEPTH=64` and
  `HAL_PLAYBACK_QUEUE_TARGET=64`:
  `local-analysis/build-flags/default-restore-after-queue8.log`.

Follow-up:
- Next standalone physical hypothesis: reduce `HAL_OUTPUT_PREFETCH_FRAMES` from
  `256` to mainline `64`, with default queue depth restored to `64/64`.
- If prefetch alone improves quality without driver CPU regression, consider a
  later controlled combination with `queue8`; do not combine before a positive
  one-factor result.

## 2026-06-17: Reject Output Prefetch 64 As Standalone Candidate

Decision:
- Keep `HAL_OUTPUT_PREFETCH_FRAMES=256` as the C++ default.
- Do not combine `prefetch64` with `queue8`; the standalone prefetch result did
  not improve the product gates.

Reason:
- Physical soundcheck with `HAL_OUTPUT_PREFETCH_FRAMES=64` failed:
  `quality_alignment_score=0.956371`, `snr_db=10.40`,
  `click_outliers=4`, `lag_jumps_gt_2_frames=48`,
  `mid_band_residual_ratio=1.431220`,
  `high_band_residual_ratio=1.365281`, quiet mid-band noise `-35.98 dBFS`.
- Compared with the hotstats/atomic-write baseline, prefetch64 worsened
  quality alignment and lag jumps, and driver CPU p95 increased to `39.5%`.
- Stream stats still showed no active underruns, late writes, timeline resets,
  or panic flags. Capture transaction error ratio stayed around `2.273`.

Alternatives discarded:
- Adopt mainline prefetch `64` for parity: rejected because parity did not
  translate into better physical evidence in the C++ HAL.
- Test `queue8 + prefetch64`: rejected because both one-factor runs failed
  driver CPU/quality gates.

Evidence:
- Build/safety: `local-analysis/physical-prefetch64/20260616-205818`
- Soundcheck: `local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal`
- Stream stats: `local-analysis/stream-stats/prefetch64-summary.json`
- Recovery/audit:
  - `local-analysis/audio-stack-guard/20260616-force-unload-prefetch64`
  - `local-analysis/runtime-isolation/post-prefetch64-failed-unload.json`
- Default rebuild restored `OPENA8DJ_OUTPUT_PREFETCH_FRAMES=256`:
  `local-analysis/build-flags/default-restore-after-prefetch64.log`.

Follow-up:
- The remaining pattern is stable: no software underruns/late writes, but
  persistent analog residual/lag and high driver CPU. Next work should inspect
  capture transaction error semantics and USB transaction request sizes/status
  rather than more parity knobs.

## 2026-06-17: Treat Zero-Complete Capture ISO Slots As Packetization, Not Transport Failure

Decision:
- Do not use aggregate `captureTransactionErrors/transfer ~= 2.273` as a
  physical-readiness blocker by itself.
- Keep blocking on status failures, short/other-size capture transactions,
  classified ISO-slot mismatches, residual/lag, and CPU.

Reason:
- New capture-detail counters decompose the aggregate "error" bucket. Recent
  detailed runs show:
  - `captureStatusFailures=0`
  - `captureOtherByteCountTransactions=0`
  - `captureShortTransfers=0`
  - `captureTransactionFailures == captureZeroCompleteTransactions`
  - `captureExpectedTransactions + captureZeroCompleteTransactions == 5 *
    captureTransfers`
  - useful capture transactions are exactly `352` bytes.
- This means the stable ratio is mostly empty ISO microframes in a 5-frame
  transfer, not failed USB status.
- The physical product blocker remains real: analog residual/lag and driver
  CPU still fail badly versus mainline.

Alternatives discarded:
- Continue chasing the aggregate counter as a USB transport failure: rejected
  because the decomposed counters show no status, short, or other-size failures
  in recent default/variant runs.
- Ignore capture details entirely: rejected because `lifecycle-preopen` has a
  real measurable mismatch (`classified_transactions != total_iso_slots`) and
  must stay rejected.

Evidence:
- Recent invariant pass:
  `local-analysis/stream-stats/capture-iso-invariants-recent-v3.json`
- Historical iRig sweep:
  `local-analysis/stream-stats/capture-iso-invariants-all-irig-v3.json`
  (`lifecycle-preopen` remains FAIL; older runs without detail are UNKNOWN).
- Tool:
  `scripts/analyze-capture-iso-invariants.py`

## 2026-06-17: Port Mainline-Style Unrolled HAL Output Packing As CPU Candidate

Decision:
- Replace the C++ HAL's default per-byte/modulo `fillPlaybackBytes` loop with a
  mainline-style unrolled 16-byte fast path when `OPENA8DJ_OUTPUT_CHECK_OFFSET`
  is the default `8`.
- Keep the previous generic path for non-default diagnostic check offsets.

Reason:
- Mainline already uses an unrolled group writer for Mode 2 output packing.
- C++ still paid modulo and branch work per byte on every playback transfer.
- The change preserves byte layout for the default check offset and should
  reduce CPU/jitter in the playback hot path. It does not alter USB request
  counts, queue depth, sample time policy, gain, or control-plane state.

Alternatives discarded:
- More queue/prefetch/ISO parity knobs: rejected for now because recent locked
  one-factor runs already failed quality/CPU gates.
- Claim product improvement from offline build/gates: rejected; the candidate
  must still pass locked iRig quality and CPU comparison.

Evidence:
- `make usb-play hal` PASS.
- `make HAL_OUTPUT_CHECK_OFFSET=4 hal` PASS for the generic fallback.
- `make hal` rebuilt the default artifact after fallback verification.
- `scripts/run-cpp-offline-gates` PASS: Debug `16/16`, Release `17/17`.

## 2026-06-17: Reject Unrolled HAL Output Pack As Default

Decision:
- Disable the unrolled HAL output pack path by default.
- Keep it only as an opt-in diagnostic knob via `HAL_UNROLLED_OUTPUT_PACK=1`.

Reason:
- The locked physical run failed far worse than previous candidates:
  `quality_alignment_score=0.131043`, SNR `-18.43 dB`,
  `lag_jumps_gt_2_frames=47`, mid-band residual ratio `8.393129`, high-band
  residual ratio `7.405798`.
- Runtime CPU did not improve enough to justify keeping it: OpenA8DJ driver
  avg/p95/max `30.959%/35.600%/35.900%`.
- Stream stats remained clean for underruns/late writes/timeline resets and
  capture ISO invariants passed, so this result does not support a readiness
  claim or a simple capture-transport explanation.

Alternatives discarded:
- Keep unrolled output pack as default because offline gates passed: rejected
  because physical quality is the governing gate.
- Continue directly to more physical sweeps with this candidate: rejected
  because the soundcheck is a severe regression.

Evidence:
- Safety first run failed on transient high `coreaudiod` CPU, then retry PASS:
  `local-analysis/physical-unrolled-pack/20260616-211513/hal-candidate-safety`
  and
  `local-analysis/physical-unrolled-pack/20260616-211622-retry/hal-candidate-safety`.
- Soundcheck:
  `local-analysis/soundcheck/20260616-unrolled-pack-irig-pairA-16s-cpp-hal`.
- Stream/capture summaries:
  `local-analysis/stream-stats/unrolled-pack-summary.json` and
  `local-analysis/stream-stats/unrolled-pack-capture-iso-invariants.json`.
- Cleanup:
  `local-analysis/audio-stack-guard/20260616-force-unload-unrolled-pack-explicit`
  and
  `local-analysis/runtime-isolation/post-unrolled-pack-failed-unload.json`.

## 2026-06-17: Treat Lag Correction As Insufficient Explanation For Default Residual

Decision:
- Stop treating the default music failure as a simple lag/drift problem.
- Keep lag-jump metrics as blockers, but prioritize signal coloration,
  distortion, wrong mixed signal, and runtime CPU as the next investigation
  targets.

Reason:
- Offline window traces over existing iRig captures show that local lag
  correction barely changes residual for default-like runs:
  - default-after-unrolled: median mid residual `1.448463 -> 1.430920`
    (`1.2%` improvement);
  - hotstats-write-late: `1.452215 -> 1.443643` (`0.6%` improvement);
  - queue8: `1.491993 -> 1.431929` (`4.0%` improvement).
- Corrected median correlation is consistently about `0.969-0.971`, but the
  residual remains far above the product gate.
- Stream stats still show no active underruns, no late writes, no timeline
  resets, and capture ISO invariants pass for the detailed runs.

Alternatives discarded:
- Chase only `lag_jumps_gt_2_frames`: rejected because even after per-window
  lag correction the spectral residual remains too high.
- Treat clean stream counters as product quality: rejected because the analog
  evidence is still failing.

Evidence:
- `local-analysis/soundcheck-window-trace/default-after-unrolled-v2.json`
- `local-analysis/soundcheck-window-trace/hotstats-write-late-v2.json`
- `local-analysis/soundcheck-window-trace/queue8-v2.json`

## 2026-06-17: Require Decorrelated Physical Matrix Before Blaming Crosstalk

Decision:
- Treat music-based 2x2 L/R matrix fits as diagnostic only.
- Add a dedicated decorrelated channel-matrix gate for the next physical
  iRig route check, rather than claiming that the existing music captures prove
  crosstalk or routing leakage.

Reason:
- Existing physical music captures use a stereo source whose L/R channels are
  too correlated for a stable crosstalk estimate: `input_lr_correlation` is
  `0.985848` and the fit condition number is about `140.3`.
- Default-like runs fit an apparent mixed L/R matrix but still leave large
  full-band residual after the linear model:
  - default-after-unrolled residual/predicted `0.303950`;
  - hotstats-write-late residual/predicted `0.306780`;
  - queue8 residual/predicted `0.503410`.
- The rejected unrolled-pack run is essentially unrelated to the reference
  (`global_mono_correlation=-0.182678`, residual/capture `0.999913`), which
  confirms that this tool can classify severe payload regressions but does not
  rescue that candidate.

Alternatives discarded:
- Declare the analog path has crosstalk based on correlated stereo music:
  rejected because the fit is ill-conditioned.
- Continue more music-only soundchecks before a matrix/crosstalk check:
  rejected because the next blocker is whether the physical route or candidate
  emits a wrong mixed signal.

Evidence:
- `scripts/analyze-soundcheck-linear-matrix.py`
- `local-analysis/soundcheck-linear-matrix/recent-failed-physical-music.json`
- `scripts/run-channel-matrix-gate`
- `local-analysis/channel-matrix/offline-prepare-smoke`
- `local-analysis/channel-matrix/20260617T014008Z-pairA-decorrelated-matrix`
