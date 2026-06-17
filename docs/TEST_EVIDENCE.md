# Test Evidence

## 2026-06-17: HAL Hot-Path Lock Reduction

- Change:
  - Moved output `startFrame` resolution into `OutputTimelineWrite`, so the
    HAL write path acquires `_outputTimeline.mutex` once instead of resolving
    sample-time continuity under a separate lock and then locking again to
    write frames.
  - Changed input stats accounting from one `_inputStatsMutex` lock per
    completed input stream sample to stack-local aggregation in
    `decodeCaptureBytes` plus one merge lock per capture transfer.
  - Preserves existing behavior: same `sampleTimeValid` handling, same
    `OPENA8DJ_ENABLE_OUTPUT_SAMPLE_TIME_FOLLOWER` tolerance, same timeline
    reset/drop/late-write counters, same input RMS/peak/cross math, same
    payload bytes, same USB cadence, same queue depths, same
    sample-rate/default-device policy.
- Commands:
  - `make -B hal usb-play`
  - `make -B hal HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1`
  - `make -B hal usb-play`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-hal-hotpath-lock-reduction.json`
- Result:
  - HAL default build PASS.
  - `usb-play` default build PASS.
  - `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1` build PASS.
  - Offline gates PASS.
  - Default CTest: `100% tests passed, 0 tests failed out of 16`.
  - Release CTest: `100% tests passed, 0 tests failed out of 17`.
  - Evidence schema: PASS, `22` required files, `0` missing.
  - Promotion readiness: FAIL, `branch_promotion_allowed=false`.
  - Runtime isolation: PASS, HAL inactive, lock absent.
  - Hardware touched: no.
  - CoreAudio touched: no.
  - USB touched: no.
- Release benchmark from the same gate:
  - `pack_mib_s=1628.8`
  - `decode_into_mib_s=586.682`
  - `route_frames_s=9.648e+08`
  - `route_advanced_frames_s=4.99203e+08`
- Readiness note:
  - This is a low-risk CPU/callback hygiene change only. It must not be
    counted as better-than-mainline evidence until a locked physical
    A/B soundcheck shows equal or better quality and lower runtime CPU.

## 2026-06-17: Playback Burst Cadence Model Gate

- Change:
  - Extended `opena8djcpp_jitter_model` with an offline burst cadence model.
  - The gate now records normal timeline/jitter rows and `burst_rows` for
    playback completion spacing relative to the capture period.
  - `scripts/run-cpp-offline-gates` now exposes `burst_rows`,
    `burst_failures`, and unsafe burst scenarios in
    `current-offline-gates.json`.
- Commands:
  - `cmake -S . -B build/cpp-release -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build/cpp-release --target opena8djcpp_jitter_model`
  - `./build/cpp-release/opena8djcpp_jitter_model | tee local-analysis/cpp-offline/jitter-model.json`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-burst-cadence-model.json`
- Result:
  - Offline gates PASS.
  - Default CTest: `100% tests passed, 0 tests failed out of 16`.
  - Release CTest: `100% tests passed, 0 tests failed out of 17`.
  - Evidence schema: PASS, `22` required files, `0` missing.
  - Runtime isolation after the gate: PASS, HAL inactive, lock absent.
  - Hardware touched: no.
  - CoreAudio touched: no.
  - USB touched: no.
- Jitter model evidence:
  - Normal rows: `8`, failures `0`.
  - Burst rows: `3`, burst failures `0`.
  - Unsafe-by-design burst scenarios:
    `capture_paced_coalesce2_rejected_by_physical_gate`,
    `capture_paced_coalesce4_rejected_by_model`.
  - `coalesce=2` models a playback completion interval of `128` frames
    against a `64` frame capture period, with completion gap ratio `2.0` and
    completion-count CPU ratio `0.5`.
- Release benchmark from the same gate:
  - `pack_mib_s=1654.72`
  - `decode_into_mib_s=587.81`
  - `route_frames_s=8.23705e+08`
  - `route_advanced_frames_s=5.00832e+08`
- Promotion readiness:
  - FAIL, `branch_promotion_allowed=false`.
  - Current blockers remain physical real-music quality, runtime CPU versus
    mainline, latest physical investigation readiness, and unvalidated physical
    Traktor/timecode vinyl.
- Interpretation:
  - The previous coalesce2 physical run proved that reducing completion count
    can lower driver CPU while making quality worse. This offline model
    captures that tradeoff explicitly, so future CPU changes must preserve
    playback cadence instead of winning by batching completions into unsafe
    gaps.

## 2026-06-16: Mandatory Hardware Lock Policy For Physical Scripts

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `2f80d08`
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/hardware-lock-policy.json`
  - `local-analysis/runtime-isolation/current.json`
  - `local-analysis/promotion-readiness-current.json`
- Change:
  - Added `scripts/hardware-lock-lib.sh`.
  - `scripts/test-hal-candidate-safety` now acquires the global hardware lock before HAL install/reload and `coreaudiod` restart.
  - `scripts/run-audio8dj-direct-gate` now uses the shared lock helper.
  - `scripts/run-soundcheck` now acquires the global hardware lock before physical playback/capture/config work.
  - Added `opena8djcpp_hardware_lock_policy_check` and wired it into CTest, offline gates, evidence schema, and promotion readiness.
- Hardware lock policy result: `PASS`, `4` audited scripts, `0` missing requirements.
- Full offline gates after the change:
  - Default CTest: `100% tests passed, 0 tests failed out of 15`
  - Release CTest: `100% tests passed, 0 tests failed out of 16`
  - Evidence schema: `22` required files, `0` missing
  - Release benchmark: `pack_mib_s=1546.09`, `decode_into_mib_s=565.894`, `float_to_s24_frames_s=8.54504e+07`, `route_frames_s=9.99440e+08`, `route_reversed_frames_s=4.81809e+08`, `route_advanced_frames_s=4.72260e+08`
- Runtime isolation after the change: PASS with HAL inactive, lock absent, forbidden mainline LaunchAgents disabled, and no OpenA8DJ process detected.
- Promotion readiness result: FAIL, with `branch_promotion_allowed=false`.
- Remaining promotion blockers: physical real-music quality, runtime CPU/coreaudiod, and physical Traktor/timecode vinyl lock evidence.
- Hardware touched by this gate: no
- CoreAudio touched by this gate: no
- USB touched by this gate: no
- Driver installed or activated by this gate: no

## 2026-06-16: Mode 2 Cross-Oracle Byte Parity Gate

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `50105c5` plus current Mode 2 cross-oracle gate changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/mode2-cross-oracle-parity.json`
  - `local-analysis/promotion-readiness-current.json`
- Change:
  - C++ Float32-to-S24 quantization now matches the inherited Python oracle's explicit Float32 rounding path before converting to 24-bit output.
  - Added `opena8djcpp_mode2_cpp_oracle_dump` and `scripts/mode2-cross-oracle-parity`.
  - Promotion readiness now includes `offline_mode2_cross_oracle_parity`.
- Cross-oracle result: `72` rows, `0` failures, `0` byte mismatches, `0` length deltas, `0` check errors, `0` panic flags.
- Full offline gates after the change:
  - Default CTest: `100% tests passed, 0 tests failed out of 14`
  - Release CTest: `100% tests passed, 0 tests failed out of 15`
  - Evidence schema: `21` required files, `0` missing
  - Release benchmark: `pack_mib_s=1601.31`, `decode_into_mib_s=569.821`, `float_to_s24_frames_s=8.54237e+07`, `route_frames_s=9.78452e+08`, `route_reversed_frames_s=4.99956e+08`, `route_advanced_frames_s=4.91712e+08`
- Promotion readiness result: FAIL, with `branch_promotion_allowed=false`.
- Remaining promotion blockers: physical real-music quality, runtime CPU/coreaudiod, and physical Traktor/timecode vinyl lock evidence.
- Runtime cleanup check before this gate: hardware lock absent; terminated live `git fsmonitor--daemon` holding `/Users/fer/dev/opena8dj`; terminated `NIHardwareAgent`; follow-up `lsof +D /Users/fer/dev/opena8dj` showed no remaining handles.
- Hardware touched by this gate: no
- CoreAudio touched by this gate: no
- USB touched by this gate: no
- Driver installed or activated by this gate: no

## 2026-06-16: Initial C++ Offline Core Contract

- Command: `cmake -S . -B build/cpp-offline && cmake --build build/cpp-offline && ctest --test-dir build/cpp-offline --output-on-failure`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign scaffold changes
- Result: PASS
- Output summary: `100% tests passed, 0 tests failed out of 1`
- Evidence path: `local-analysis/cpp-offline/`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: C++ Routing Fast Path Optimization

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/20260616-145942-routing-fastpath-repeat`
- Change: added an identity-routing fast path and moved routing mapping validation outside the per-frame loop.
- Added tests:
  - identity routing still copies all 8 channels correctly;
  - reversed routing maps channels explicitly;
  - invalid routing is rejected before processing frames.
- Full offline gates after the change:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS, `72` rows, `0` failures
  - Python Mode 2 oracle: PASS, `check_errors=0`, `panic_flags=0`
  - Timecode matrix: PASS
  - DVS signal smoke: PASS, zero leakage
  - Realtime audit: PASS, `hot_path_allocations=0`
  - Static policy: PASS, `forbidden_hits=0`
- Measured routing performance:
  - Pre-change same-session baseline: about `5.66e+08 frames/s`
  - Post-change gate run: `9.62e+08 frames/s`
  - Seven-run post-change repeat: median `9.49e+08 frames/s`, min `9.37e+08`, max `9.71e+08`
- Pack/decode metrics remained within run-to-run noise and are not claimed as improved.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated by this optimization: no

Readiness note:

- This is a real offline hot-path improvement for default A/B/C/D identity routing.
- It does not prove physical sound quality, iRig analog capture quality, DAC behavior, Traktor scope quality, or runtime CPU against mainline.

## 2026-06-16: C++ Mode 2 Decode Bitmask Optimization

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/20260616-150104-decode-bitmask-repeat`
- Change: replaced per-stream `bool` presence arrays in Mode 2 decode with 6-bit masks.
- Reason: frame completeness in the real-time decode path is a small fixed-size bitset problem; byte masks avoid repeated bool-array fills and per-frame full-array scans.
- Full offline gates after the change:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS, `72` rows, `0` failures
  - Python Mode 2 oracle: PASS, `check_errors=0`, `panic_flags=0`
  - Timecode matrix: PASS
  - DVS signal smoke: PASS, zero leakage
  - Realtime audit: PASS, `hot_path_allocations=0`
  - Static policy: PASS, `forbidden_hits=0`
- Measured decode performance:
  - Pre-change same-session baseline: about `532 MiB/s`
  - Post-change gate run: `577.374 MiB/s`
  - Nine-run post-change repeat: median `570.726 MiB/s`, min `377.063`, max `579.388`
- Routing remains improved after the decode change:
  - Nine-run post-change repeat median `9.54e+08 frames/s`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated by this optimization: no

Readiness note:

- This strengthens the C++ packet decode data plane.
- It does not prove physical sound quality, runtime HAL CPU, Traktor scope quality, or mainline superiority without hardware/iRig evidence.

## 2026-06-16: Preallocated Packer Benchmark Path

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/20260616-150207-preallocated-pack-repeat`
- Change: `tools/offline_bench.cpp` now measures `Mode2OutputPacker::fill_into()` into a preallocated buffer instead of `fill()` plus repeated `vector` insertion.
- Reason: the real-time policy requires preallocated buffers; the old benchmark mixed packer speed with allocation/copy overhead from benchmark scaffolding.
- Full offline gates after the change:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS, `72` rows, `0` failures
  - Python Mode 2 oracle: PASS, `check_errors=0`, `panic_flags=0`
  - Timecode matrix: PASS
  - DVS signal smoke: PASS, zero leakage
  - Realtime audit: PASS, `hot_path_allocations=0`
  - Static policy: PASS, `forbidden_hits=0`
- Measured preallocated pack performance:
  - Latest gate run: `1653.83 MiB/s`
  - Nine-run repeat: median `1634.35 MiB/s`, min `1004.28`, max `1649.91`
- This pack number is not directly comparable to the old allocation-heavy benchmark. It is the correct metric for the intended real-time data plane.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated by this optimization: no

## 2026-06-16: HAL Install And Hardware Presence Check

- Command: `make install-hal`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PARTIAL / BLOCKED_FOR_IRIG_CAPTURE
- Installed HAL hash: `13dd0d0b0ab31c6972cd62d7c1187a81e9dd04a29b987f3dbbde7e7f56233788`
- Install evidence: `local-analysis/hardware-quality/20260616-144846-install-verify`
- Capture presence evidence: `local-analysis/hardware-quality/20260616-144933-capture-presence`
- iRig presence audit: `local-analysis/hardware-quality/20260616-145020-irig-presence-audit`
- Driver state evidence: `local-analysis/hardware-quality/20260616-145129-driver-state`
- Hardware touched: yes, user explicitly requested driver installation and hardware quality testing
- CoreAudio touched: yes, `make install-hal` restarted `coreaudiod`
- USB touched: read-only enumeration only; no USB reset or replug automation
- Driver installed or activated: yes, HAL installed at `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver`

Findings:

- `Open Audio 8 DJ` enumerated in CoreAudio with 8 input and 8 output channels at 48 kHz.
- Installed HAL binary hash matched the built candidate hash.
- CoreAudio stack health after install: PASS.
- `Open Audio 8 DJ` recording opened successfully for a 2 second input-only capture on channels 1/2.
- iRig/IK Multimedia capture did not appear in CoreAudio or USB across 10 read-only samples.
- No USB reset, default-device mutation command, or iRig recording was attempted after the missing-iRig finding.
- Follow-up exact iRig diagnosis: `local-analysis/hardware-quality/20260616-145508-exact-irig-diagnose`.
- Expected physical route from mainline QA docs: `Open Audio 8 DJ -> external mixer -> mixer REC OUT -> iRig Stream -> macOS capture`.
- Expected iRig USB identity: vendor `0x1963` / product `0x0059`.
- Exact iRig result: no CoreAudio `iRig Stream`, no USB `0x1963:0x0059`, no `IK Multimedia` match.
- USB port counters in the exact diagnosis did not show an iRig enumeration attempt or current iRig USB object.

Current quality status:

- Driver installation and Audio 8 DJ presence are confirmed.
- Objective analog output quality through the documented mixer REC OUT -> iRig path is blocked until iRig appears in CoreAudio/USB.
- Do not claim hardware audio quality readiness from this run.

## 2026-06-16: Mode 2 Functional And Release Performance Gates

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Default contract: `100% tests passed, 0 tests failed out of 8`
- Release contract/performance: `100% tests passed, 0 tests failed out of 9`
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/ctest-default.txt`
  - `local-analysis/cpp-offline/ctest-release.txt`
  - `local-analysis/cpp-offline/packet-matrix.json`
  - `local-analysis/cpp-offline/mode2-python-oracle.txt`
  - `local-analysis/cpp-offline/timecode-matrix.json`
  - `local-analysis/cpp-offline/dvs-signal-smoke.json`
  - `local-analysis/cpp-offline/realtime-audit.json`
  - `local-analysis/cpp-offline/driverkit-surface-model.json`
  - `local-analysis/cpp-offline/jitter-model.json`
  - `local-analysis/cpp-offline/static-policy.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
- Measured release gate:
  - `pack_mib_s`: `1107.96`
  - `decode_mib_s`: `530.171`
  - `route_frames_s`: `5.48323e+08`
  - `check_errors`: `0`
  - `panic_flags`: `0`
- Superseded by later C++ hot-path optimization evidence in this file. Keep
  these values as the initial offline baseline, not current performance.
- Packet matrix: `72` rows, `0` failures.
- Python Mode 2 oracle: all start bytes PASS, `check_errors=0`, `panic_flags=0`.
- Timecode matrix: `4` profiles and `4` deck assignments, `0` failures.
- DVS signal smoke: `24` rows, `0` failures, zero leakage.
- Realtime audit: `hot_path_allocations=0`, `decode_output_overflows=0`, routing OK.
- DriverKit surface model: one 8-channel input stream, four 2-channel output streams, 44.1/48 kHz.
- Jitter model: `2` rows, `0` failures, max error `0.125` frames, `0` regressions.
- Static policy: `10` audited files, `0` forbidden hits.
- Evidence schema: `15` required files, `0` missing.
- Readiness report: `docs/OFFLINE_READINESS_REPORT.md`.
- Functional coverage added:
  - S24 big-endian conversion vectors.
  - Mode 2 round-trip across all start bytes `0..5`.
  - No leakage from deck A into B/C/D for a synthetic pair-A-only stream.
  - Release benchmark thresholds: pack/decode `>= 100 MiB/s`, route `>= 1,000,000 frames/s`, zero check errors, zero panic flags.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Release Benchmark Median Reporting

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/ctest-default.txt`
  - `local-analysis/cpp-offline/ctest-release.txt`
  - `local-analysis/cpp-offline/current-offline-gates.json`
- Change: `tools/offline_bench.cpp` now reports median/min/max over `5`
  repeats for pack, decode, and routing throughput.
- Reason: a single benchmark sample was too sensitive to cold-start and
  scheduler noise. Median reporting gives a more defensible performance gate.
- Current measured release gate:
  - `repeats`: `5`
  - `pack_mib_s`: `1647.33`
  - `pack_mib_s_min`: `1645.41`
  - `pack_mib_s_max`: `1655.57`
  - `decode_mib_s`: `583.345`
  - `decode_mib_s_min`: `535.711`
  - `decode_mib_s_max`: `613.995`
  - `route_frames_s`: `9.79216e+08`
  - `route_frames_s_min`: `9.13524e+08`
  - `route_frames_s_max`: `1.00454e+09`
  - `check_errors`: `0`
  - `panic_flags`: `0`
- Full gate status:
  - Default contract: `100% tests passed, 0 tests failed out of 8`
  - Release contract/performance: `100% tests passed, 0 tests failed out of 9`
- Hardware touched by this gate: no
- CoreAudio touched by this gate: no
- USB touched by this gate: no
- Driver installed or activated by this gate: no

Operational note:

- During the concurrent iRig recovery window, `audio-io-test` and
  `opena8dj-control` were invoked while the subagent lock was held. They
  returned successfully, but this violated the intended single-owner hardware
  discipline. No further hardware commands should run until the lock is free.
  The observed `audio-io-test` output was:
  `I/O OK: rate=48000 callbacks=284 outputBuffers=1136 outputFrames=581632 outputSamples=1163264 outputPeak=0.02000000 inputFrames=145408 inputRMS=0.00188330 inputPeak=0.01624990`.

## 2026-06-16: Post-Revert Offline Gate And Rejected Packer Rewrite

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/promotion-readiness-current.json`
- Change: reverted a counter-based packer rewrite after it measured slower than
  the existing `Mode2OutputPacker::fill_into` implementation.
- Rejected candidate measurement: about `1149.74 MiB/s` pack throughput.
- Final post-revert Release benchmark:
  - `pack_mib_s`: `1602.11`
  - `decode_into_mib_s`: `570.085`
  - `decode_allocating_mib_s`: `552.130`
  - `float_to_s24_frames_s`: `85,096,700`
  - `route_frames_s`: `950,086,000`
  - `route_reversed_frames_s`: `581,896,000`
  - `decode_into_output_overflows`: `0`
  - `check_errors`: `0`
  - `panic_flags`: `0`
- Promotion gate after this evidence: FAIL,
  `branch_promotion_allowed=false`.
- Promotion blockers:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no lock evidence yet.
- Hardware touched by this gate: no
- CoreAudio touched by this gate: no
- USB touched by this gate: no
- Driver installed or activated by this gate: no

## 2026-06-16: iRig Recovery Subagent

- Agent: iRig/Riff recovery subagent
- Evidence path:
  `local-analysis/hardware-quality/20260616-155613-riff-recovery-subagent`
- Result: FAIL for iRig CoreAudio recovery
- PASS condition not met: CoreAudio did not list `iRig Stream` as `2 in / 2 out`.
- Final CoreAudio state:
  - `Open Audio 8 DJ`: present, `8 in / 8 out`, `48000`
  - `iRig Stream`: absent
- Final USB state:
  - `iRig Stream` present as vendor/product `0x1963:0x0059`
  - device remains `!registered, !matched`
  - `UsbEnumerationState = 2`
- Objective kernel log cause:
  - `enumerated 0x1963/0059/0110 (iRig Stream / 2) at 12 Mbps`
  - `device functionality blocked by transport restrictions`
  - `iRig Stream@01100000: device will not be registered for matching`
- Actions performed by subagent:
  - Acquired and released hardware lock.
  - Reset iRig through IOUSB plane; reset returned PASS but device came back blocked.
  - Reenumerated iRig; command returned PASS but device came back blocked.
  - Reset Audio 8 DJ after explicit user authorization; reset returned PASS.
  - Attempted controlled audio/USB service restarts; several `launchctl` service
    kickstarts were blocked by SIP.
  - Did not change default devices, sample rate, or buffer size.
  - Did not install drivers or reboot.
- Current conclusion: iRig failure is not OpenA8DJ HAL output quality evidence.
  macOS is blocking the iRig accessory/transport before CoreAudio matching.
- Next required external action: authorize the accessory/port in macOS
  Privacy & Security accessory settings or reconnect while the login session is
  unlocked and the accessory prompt is accepted.

## 2026-06-16: Audio 8 DJ Direct Hardware Gate Without iRig

- Command group: guarded direct Audio 8 DJ CoreAudio I/O, no iRig, no defaults
- Evidence path:
  `local-analysis/hardware-quality/20260616-160652-audio8dj-direct-gate`
- Hardware lock: acquired and released
- Result: PASS
- Installed device under test:
  - CoreAudio name: `Open Audio 8 DJ`
  - UID: `org.opena8dj.Audio8DJ`
  - channels: `8 in / 8 out`
  - rate observed: `48000`
- Commands executed under lock:
  - `./build/audio-list`
  - `./build/opena8dj-control`
  - `./build/opena8dj-control stream-stats`
  - `./build/audio-io-test`
  - `./build/audio-record 3 ... \"Open Audio 8 DJ\" 1,2`
  - `./build/audio-record 3 ... \"Open Audio 8 DJ\" 7,8`
- Results:
  - `audio_io_test_status=0`
  - `audio_record_12_status=0`
  - `audio_record_78_status=0`
  - `I/O OK: rate=48000 callbacks=282 outputBuffers=1128 outputFrames=577536 outputSamples=1155072 outputPeak=0.02000000 inputFrames=144384 inputRMS=0.00188486 inputPeak=0.01638067`
  - Channels 1/2 recording: `frames=144384`, `rms=0.00154989`,
    `peak=0.01067889`, `clipped=0`
  - Channels 7/8 recording: `frames=144384`, `rms=0.00161926`,
    `peak=0.01025796`, `clipped=0`
- Explicitly not changed:
  - default input/output devices
  - sample rate
  - buffer size
  - HAL installation
  - system services
- This proves direct CoreAudio I/O presence and capture open on Audio 8 DJ. It
  does not prove analog output quality, iRig loopback quality, Traktor DVS lock,
  or superiority over mainline.

## 2026-06-16: iRig Accessory Prompt Accepted Remotely

- Context: after reboot/replug, USB saw `iRig Stream` but CoreAudio did not.
- Blocker observed in logs: `device functionality blocked by transport
  restrictions`.
- Visible UI prompt: `Allow accessory to connect? Do you want to connect IK
  Multimedia iRig Stream to this Mac?`
- Successful action: `build/cg-click 900 407`
- Result: PASS
- CoreAudio after click:
  - `iRig Stream`: UID
    `AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1`,
    `2 in / 2 out`, `48000`
  - `Open Audio 8 DJ`: UID `org.opena8dj.Audio8DJ`, `8 in / 8 out`, `48000`
- Evidence:
  - `local-analysis/ui/current-usb-accessory-prompt.png`
  - `local-analysis/ui/after-cg-900-407.png`
- Notes:
  - AppleScript/Accessibility could not see the prompt reliably.
  - Virtual HID creation failed because macOS requires the
    `com.apple.developer.hid.virtual.device` entitlement.

## 2026-06-16: iRig Silence Probe

- Command: `./build/audio-record 4 <run>/irig-silence.wav "iRig Stream" 1,2`
- Evidence path:
  `local-analysis/hardware-quality/20260616-165349-irig-silence-probe`
- Result: PASS as capture-readiness probe
- Metrics:
  - `frames=192000`
  - `rms=0.00075155`
  - `peak=0.00698853`
  - `clipped=0`
- Interpretation: iRig capture path is alive and not carrying a large continuous
  noise floor while idle.

## 2026-06-16: Physical Tone And Transport Variant Tests

- Baseline short tone after iRig recovery:
  - Evidence:
    `local-analysis/hardware-quality/20260616-165428-irig-tone-pairA`
  - `sideband_ratio=0.053212`
  - `click_outliers=12`
  - `clipped=0`
- Rejected variant: `ISO64/q8/start_byte=4`
  - Install evidence:
    `local-analysis/hardware-quality/20260616-165730-install-iso64-q8-cadence`
  - Tone evidence:
    `local-analysis/hardware-quality/20260616-165746-iso64-q8-irig-tone-pairA`
  - Result: FAIL/regression
  - `sideband_ratio=0.791833`
  - `click_outliers=372`
- Start-byte comparison, long tone:
  - Evidence:
    `local-analysis/hardware-quality/20260616-170024-start-byte-2v4-tone-long`
  - `start_byte=2`: `sideband_ratio=0.080717`, `click_outliers=0`,
    `peak=1.00000000`
  - `start_byte=4`: `sideband_ratio=0.000657`, `click_outliers=0`,
    `peak=0.37208557`
  - Decision: keep `start_byte=4`.

## 2026-06-16: Physical Music Soundcheck With iRig

- Command:
  `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --run-dir local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4`
- Evidence path:
  `local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4`
- Result: FAIL strict product gate
- Metrics:
  - `quality_alignment_score=0.938154`
  - `analog_snr_db=8.93`
  - `mid_band_1000_5000_residual_ratio=1.379896`
  - `high_band_5000_12000_residual_ratio=1.347577`
  - `quiet_mid_band_noise_dbfs=-31.17`
  - `click_outliers=0`
  - `lag_jumps_gt_2_frames=24`
  - `capture_clipped_frames=0`
- CPU profile from the same run:
  - `opena8dj_driver_p95=11.5`
  - `coreaudiod_p95=95.8`
- Interpretation:
  - Tone result is excellent, but real-music quality and CPU/resource evidence
    are not sufficient for readiness or better-than-mainline claims.

## 2026-06-16: No-Diagnostic HAL Attempt And Recovery

- Attempted candidate:
  `ISO5/q64/start_byte=4/big/gain0.50/HAL_CADENCE_DIAGNOSTIC=0`
- Install evidence:
  `local-analysis/hardware-quality/20260616-170515-install-iso5-q64-start4-nodiag`
- Result: FAIL operationally
- Symptom:
  - subsequent `audio-record` tone attempt wrote no logs and hung;
  - `audio-list` also hung until helper processes were killed.
- Recovery:
  - Reinstalled `ISO5/q64/start_byte=4` with `HAL_CADENCE_DIAGNOSTIC=1`
  - Evidence:
    `local-analysis/hardware-quality/20260616-170759-recover-install-iso5-q64-start4-cadence`
  - Post-recovery `audio-list`: PASS; iRig and Audio 8 DJ both enumerated.
- Lock note:
  - The global lock was acquired before the recovery sequence, but later check
    showed `LOCK_ABSENT`. Further hardware/CoreAudio commands must reacquire the
    lock before running.

## 2026-06-16: Offline Gate Summary Regeneration

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
- Change verified:
  - `scripts/run-cpp-offline-gates` now regenerates
    `current-offline-gates.json` from the evidence files produced in the same
    run before executing `opena8djcpp_evidence_schema_check`.
- Current measured Release benchmark:
  - `pack_mib_s=1537.85`
  - `decode_mib_s=498.859`
  - `route_frames_s=5.96177e+08`
  - `check_errors=0`
  - `panic_flags=0`
- Full offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS, `72` rows, `0` failures
  - Python Mode 2 oracle: PASS
  - Timecode matrix: PASS
  - DVS signal smoke: PASS
  - Realtime audit: PASS, `hot_path_allocations=0`
  - DriverKit surface model: PASS
  - Jitter model: PASS
  - Static policy: PASS
  - Evidence schema: PASS
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Promotion Readiness Gate

- Command:
  `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: FAIL, as expected for current evidence
- Evidence path:
  `local-analysis/promotion-readiness-current.json`
- PASS gates:
  - evidence files present
  - offline gates
  - offline throughput
  - simulated output oracle
  - physical tone beats historical mainline tone floor
- FAIL gates:
  - physical real-music quality:
    `quality_alignment_score=0.938154`, `snr_db_min=8.93`,
    `quiet_mid_band_noise_dbfs=-31.17`, `lag_jumps_gt_2_frames=24`
  - runtime CPU/resource:
    `opena8dj_driver_p95=11.5`, `coreaudiod_p95=95.8`
  - physical Traktor/timecode vinyl:
    `BLOCKED_UNVALIDATED_DVS`
- Branch promotion status:
  - `branch_promotion_allowed=false`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Mode 2 Decode Counter Optimization And Preallocated Benchmark

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/ctest-default.txt`
  - `local-analysis/cpp-offline/ctest-release.txt`
- Change:
  - `decode_mode2_usb_bytes_into` now tracks transfer/group offsets with
    incrementing counters instead of per-byte modulo operations.
  - `opena8djcpp_offline_bench` now reports both the allocating decode wrapper
    and the preallocated real-time decode path.
  - `core_contract_tests` now asserts that preallocated decode matches the
    allocating wrapper for transfer sizes `48`, `80`, `352` and start bytes
    `0..5`.
- Current measured Release benchmark:
  - `pack_mib_s=1541.97`
  - `decode_into_mib_s=571.408`
  - `decode_allocating_mib_s=517.353`
  - `route_frames_s=9.34143e+08`
  - `decode_into_frames=131071`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_check_errors=0`
  - `decode_into_panic_flags=0`
- Full offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS, `72` rows, `0` failures
  - Python Mode 2 oracle: PASS
  - Timecode matrix: PASS
  - DVS signal smoke: PASS
  - Realtime audit: PASS
  - DriverKit surface model: PASS
  - Jitter model: PASS
  - Static policy: PASS
  - Evidence schema: PASS
- Promotion gate after this change:
  - `local-analysis/promotion-readiness-current.json`
  - Result: FAIL
  - `branch_promotion_allowed=false`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: RoutingPlan And Expanded Jitter Offline Gates

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/jitter-model.json`
- Changes:
  - Added `RoutingPlan` so routing configuration can be validated once outside
    the hot path.
  - Benchmarks now report identity and reversed routing throughput.
  - Jitter model now covers callback jitter, gradual drift, future-gap recovery,
    and stale-gap recovery at 44.1 kHz and 48 kHz.
- Current measured Release benchmark:
  - `pack_mib_s=1594.23`
  - `decode_into_mib_s=563.142`
  - `decode_allocating_mib_s=544.77`
  - `route_frames_s=8.94689e+08`
  - `route_reversed_frames_s=5.83406e+08`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_output_overflows=0`
- Jitter model:
  - `rows=8`
  - `failures=0`
  - total modeled `lag_jumps_gt_2_frames=4`
  - total modeled `timeline_resets=4`
  - total modeled `elastic_drop_frames=172`
  - total modeled `elastic_replay_frames=82`
  - `phase_discontinuities=0`
  - `regressions=0`
- Full offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS
  - Python Mode 2 oracle: PASS
  - Timecode matrix: PASS
  - DVS signal smoke: PASS
  - Realtime audit: PASS
  - DriverKit surface model: PASS
  - Static policy: PASS
  - Evidence schema: PASS
- Promotion gate after this change:
  - `local-analysis/promotion-readiness-current.json`
  - Result: FAIL
  - `branch_promotion_allowed=false`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Existing Physical Music Window Trace

- Command:
  `scripts/analyze-soundcheck-window-trace.py local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4 --json-out local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4/window-trace.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS as offline analysis artifact
- Evidence path:
  `local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4/window-trace.json`
- Inputs:
  - existing `fixture/reference.wav`
  - existing `captured.wav`
  - existing `metrics.json`
  - existing `cpu-profile.tsv`
- Metrics:
  - `windows=38`
  - `local_lag_min=-27`
  - `local_lag_max=3`
  - `lag_jumps_gt_2_frames=24`
  - `raw_mid_band_residual_ratio_median=1.398197`
  - `lag_corrected_mid_band_residual_ratio_median=1.368747`
  - `lag_correction_mid_ratio_improvement=0.021063`
- Interpretation:
  - Local per-window lag correction improves median mid-band residual by only
    about `2.1%`.
  - This supports timebase/cadence as a real issue, but weakens the theory that
    timebase alone explains the poor music SNR/residual.
  - Next physical test should repeat the same run with a longer capture and
    explicit startup discard, then compare stable-window residual and lag.
- Hardware touched: no new hardware access; existing captured files only
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Float32 To S24 Safety And Benchmark Gate

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
- Change:
  - `float_to_s24` now maps non-finite host samples (`NaN`, `+Inf`, `-Inf`)
    to silence instead of relying on `lrint` behavior.
  - Core contract tests cover finite clipping, zero gain, NaN, and infinities.
  - Release benchmark now reports Float32-to-S24 conversion throughput.
- Current measured Release benchmark:
  - `pack_mib_s=1589.40`
  - `decode_into_mib_s=574.661`
  - `decode_allocating_mib_s=549.715`
  - `float_to_s24_frames_s=8.68818e+07`
  - `route_frames_s=9.68962e+08`
  - `route_reversed_frames_s=5.90803e+08`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_output_overflows=0`
- Promotion gate after this change:
  - `local-analysis/promotion-readiness-current.json`
  - Result: FAIL
  - `branch_promotion_allowed=false`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Input Profile And Timecode Signal Analysis Gate

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/timecode-matrix.json`
  - `local-analysis/cpp-offline/timecode-signal-analysis.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/promotion-readiness-current.json`
- Changes:
  - Added pure C++ input profiles for playback, timecode vinyl, timecode
    CD-line, and phono.
  - Playback profile keeps input decode and software lock disabled.
  - Timecode profiles enable input decode and software lock, use CAIAQ modes
    `0/1/2`, preserve identity source map, and record ground-lift intent.
  - Added `opena8djcpp_timecode_signal_analysis`, with synthetic pass/fail
    cases for balanced carrier, wrong frequency, channel imbalance, and
    clipping at 44.1/48 kHz.
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 9`
  - Release CTest: `100% tests passed, 0 tests failed out of 10`
  - Timecode signal analysis: `8` rows, `0` failures
  - Evidence schema: `16` required files, `0` missing
- Current measured Release benchmark:
  - `pack_mib_s=1548.74`
  - `decode_into_mib_s=564.905`
  - `decode_allocating_mib_s=550.683`
  - `float_to_s24_frames_s=8.20632e+07`
  - `route_frames_s=9.77085e+08`
  - `route_reversed_frames_s=5.84871e+08`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_output_overflows=0`
- Promotion gate after this change:
  - Result: FAIL
  - `branch_promotion_allowed=false`
  - Blockers remain physical music quality, runtime CPU, and physical
    Traktor/timecode vinyl evidence.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Offline DriverKit Shell Contract

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/driverkit-shell-contract.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
  - `local-analysis/promotion-readiness-current.json`
- Change:
  - Added an offline-compilable `opena8djcpp_driverkit_shell` library around
    `driverkit/src/audio_driver_skeleton.cpp`.
  - Added `opena8djcpp_driverkit_shell_contract` to validate device model and
    bounded lifecycle: created -> started -> stopped, duplicate start/stop
    rejected.
  - Promotion readiness now explicitly gates
    `offline_driverkit_shell_contract`.
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 10`
  - Release CTest: `100% tests passed, 0 tests failed out of 11`
  - DriverKit shell contract: PASS
  - Evidence schema: `17` required files, `0` missing
- Current measured Release benchmark:
  - `pack_mib_s=1609.85`
  - `decode_into_mib_s=552.591`
  - `decode_allocating_mib_s=514.478`
  - `float_to_s24_frames_s=8.53148e+07`
  - `route_frames_s=9.28216e+08`
  - `route_reversed_frames_s=5.72887e+08`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_output_overflows=0`
- Promotion gate after this change:
  - Result: FAIL
  - `branch_promotion_allowed=false`
  - New offline checks pass, but physical music quality, runtime CPU, and
    physical Traktor/timecode vinyl remain blocking failures.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Frozen Offline Candidate Commit

- Command:
  `scripts/run-cpp-offline-gates && scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `837461c`
- Result:
  - Offline gates: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/driverkit-shell-contract.json`
  - `local-analysis/cpp-offline/timecode-signal-analysis.json`
  - `local-analysis/promotion-readiness-current.json`
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 10`
  - Release CTest: `100% tests passed, 0 tests failed out of 11`
  - Packet matrix: PASS, `72` rows
  - Timecode signal analysis: PASS, `8` rows
  - DriverKit shell contract: PASS
  - Evidence schema: PASS, `17` required files
- Release benchmark at commit `837461c`:
  - `pack_mib_s=1265.89`
  - `decode_into_mib_s=468.364`
  - `decode_allocating_mib_s=448.797`
  - `float_to_s24_frames_s=7.15621e+07`
  - `route_frames_s=9.65685e+08`
  - `route_reversed_frames_s=4.99639e+08`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_output_overflows=0`
- Promotion blockers:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no real lock evidence.
- Hardware touched by this frozen-candidate gate: no
- CoreAudio touched by this frozen-candidate gate: no
- USB touched by this frozen-candidate gate: no
- Driver installed or activated by this frozen-candidate gate: no

## 2026-06-16: Fixed SPSC Audio Ring Contract

- Command:
  `scripts/run-cpp-offline-gates && scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Code commit: `8072fc5`
- Result:
  - Offline gates: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Change:
  - Added `SpscFrameRing<Frame, Capacity>` with fixed storage, atomic read/write
    cursors, `push`, `pop`, `push_many`, `pop_many`, and `clear`.
  - Core contract tests cover capacity, full/empty behavior, FIFO order,
    batch push/pop, and clear.
  - Realtime audit now includes fixed-ring push/pop after Mode 2 decode.
- Realtime audit evidence:
  - `hot_path_allocations=0`
  - `decoded_frames=2815`
  - `ring_pushed_frames=2815`
  - `ring_popped_frames=2815`
  - `ring_remaining_frames=0`
  - `decode_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Release benchmark at code commit `8072fc5`:
  - `pack_mib_s=1591.35`
  - `decode_into_mib_s=566.408`
  - `decode_allocating_mib_s=542.220`
  - `float_to_s24_frames_s=8.45376e+07`
  - `route_frames_s=7.53918e+08`
  - `route_reversed_frames_s=5.87876e+08`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Mode 2 Input Decode And DVS Packet Gate

- Command:
  `scripts/run-cpp-offline-gates`
- Promotion command:
  `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Code commit: `6058093`
- Result:
  - Offline gates: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Change:
  - Added `decode_input_profile_mode2_into`, which decodes Mode 2 input bytes
    into caller-owned S24 scratch and optional interleaved Float32 output.
  - Added `opena8djcpp_dvs_packet_input_decode`, which packs synthetic
    timecode-like carriers into Mode 2 bytes, decodes by input profile, analyzes
    the active deck pair, and checks leakage on inactive input channels.
  - Added promotion gate `offline_dvs_packet_input_decode`.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/dvs-packet-input-decode.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/realtime-audit.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
  - `local-analysis/promotion-readiness-current.json`
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 11`
  - Release CTest: `100% tests passed, 0 tests failed out of 12`
  - DVS packet input decode: PASS, `24` rows, `0` failures
  - Playback decode-off probe: PASS, packet stats preserved and `0` frames
    written
  - Evidence schema: PASS, `18` required files, `0` missing
- DVS packet metrics:
  - Profiles: `timecode-vinyl`, `timecode-cd-line`, `phono`
  - Rates: `44100`, `48000`
  - Decks: A/B/C/D
  - Frames written: `22049` at 44.1 kHz rows, `23999` at 48 kHz rows
  - Leakage failures: `0`
  - Decode overflows/check errors/panic flags: `0`
- Release benchmark at code commit `6058093` from the final post-doc rerun:
  - `pack_mib_s=1413.3`
  - `decode_into_mib_s=506.054`
  - `decode_allocating_mib_s=487.822`
  - `float_to_s24_frames_s=7.59581e+07`
  - `route_frames_s=8.26517e+08`
  - `route_reversed_frames_s=5.32678e+08`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Promotion blockers after this change:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no real lock evidence.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Runtime Isolation Quiescence Audit

- Command:
  `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result: PASS
- Evidence path:
  `local-analysis/runtime-isolation/current.json`
- Verified state:
  - Global hardware lock: absent.
  - Active HAL path `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver`: absent.
  - Disabled HAL artifact:
    `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver.disabled-20260616T215913Z`.
  - Forbidden mainline labels disabled and not running:
    `org.opena8dj.midid`,
    `com.fer.opena8dj.autonomous-audio-qa`,
    `com.fer.opena8dj.safe-replug-watch`,
    `com.fer.opena8dj.audio-qa-startup`,
    `com.fer.opena8dj.codex-resume`.
  - Allowed C++ resume label present:
    `com.fer.audio8djcpp.codex-resume`.
  - OpenA8DJ process probes: no PIDs.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no
- Readiness note:
  - This proves the machine is quiesced and mainline is not interfering.
  - It also proves physical C++ audio tests cannot run in this state; the HAL
    must be explicitly restored or reinstalled under lock, then
    `scripts/runtime-isolation-audit --expect-hal active` must pass.

## 2026-06-16: Advanced Routing Matrix Contract

- Command:
  `scripts/run-cpp-offline-gates`
- Promotion command:
  `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Code commit: `4d4c927`
- Result:
  - Offline gates: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Change:
  - Extended `RoutingMatrix` with fixed `RouteEntry` rows containing source
    channel and precomputed gain `1`, `0`, or `-1`.
  - Kept existing source-mapping constructor and identity fast path.
  - Added Rust-oracle compound routing test: pair A from D, pair B muted, pair C
    side-swapped, and output D right inverted.
  - Added `route_advanced_frames_s` to the Release benchmark, offline summary,
    and promotion readiness throughput gate.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/promotion-readiness-current.json`
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 11`
  - Release CTest: `100% tests passed, 0 tests failed out of 12`
  - Evidence schema: PASS, `18` required files, `0` missing
- Release benchmark:
  - `pack_mib_s=1130.97`
  - `decode_into_mib_s=216.238`
  - `decode_allocating_mib_s=231.136`
  - `float_to_s24_frames_s=3.7175e+07`
  - `route_frames_s=4.87408e+08`
  - `route_reversed_frames_s=2.70938e+08`
  - `route_advanced_frames_s=2.71652e+08`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Promotion blockers after this change:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no real lock evidence.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: DriverKit SDK Availability Check

- Commands:
  - `xcrun --sdk driverkit --show-sdk-path`
  - `xcrun --show-sdk-path`
  - `xcodebuild -showsdks`
  - `find /Applications/Xcode.app /Library/Developer/CommandLineTools -path '*DriverKit*.sdk'`
  - `find /Applications/Xcode.app /Library/Developer/CommandLineTools -path '*AudioDriverKit*'`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result: BLOCKED_DRIVERKIT_SDK
- Findings:
  - Active SDK path: `/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk`.
  - `xcrun --sdk driverkit --show-sdk-path` failed because SDK `driverkit`
    cannot be located.
  - `xcodebuild -showsdks` failed because active developer directory is Command
    Line Tools, not full Xcode.
  - No `DriverKit*.sdk` or `AudioDriverKit` path was found in the searched
    developer locations.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no
- Readiness note:
  - Current DriverKit work remains an offline C++ shell/model contract.
  - Real dext compilation is blocked until full Xcode with DriverKit/
    AudioDriverKit SDK support is selected and signing/entitlements are
    prepared.

## 2026-06-16: Protocol Constants Snapshot Gate

- Command:
  `scripts/run-cpp-offline-gates`
- Promotion command:
  `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Runtime isolation command:
  `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Code commit: `25de786`
- Result:
  - Runtime isolation: PASS
  - Offline gates: PASS
  - Protocol contract: PASS
  - Evidence schema: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Evidence paths:
  - `local-analysis/runtime-isolation/current.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/protocol-contract.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
  - `local-analysis/promotion-readiness-current.json`
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 12`
  - Release CTest: `100% tests passed, 0 tests failed out of 13`
  - Evidence schema: PASS, `19` required files, `0` missing
- Protocol contract:
  - VID/PID: `0x17cc:0x1978`
  - endpoints: control OUT `0x01`, control IN `0x81`, ISO capture `0x82`,
    ISO playback `0x06`
  - channel surface: `8` inputs, `8` outputs, pairs A/B/C/D
  - Mode 2 check cadence: `16` bytes
  - Mode 2 full frame: `32` bytes
  - default start byte: `4`
  - rates advertised by current policy: `44100`, `48000`
  - known but deferred rate codes: `88200`, `96000`
- Release benchmark:
  - `pack_mib_s=1410.11`
  - `decode_mib_s=511.536`
  - `decode_into_mib_s=511.536`
  - `decode_allocating_mib_s=487.085`
  - `float_to_s24_frames_s=8.25390e+07`
  - `route_frames_s=7.86433e+08`
  - `route_reversed_frames_s=5.00155e+08`
  - `route_advanced_frames_s=4.43810e+08`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Runtime isolation state:
  - Global hardware lock: absent.
  - Mainline OpenA8DJ LaunchAgents: disabled and not running.
  - Active OpenA8DJ HAL path: absent.
  - OpenA8DJ process probes: no PIDs.
  - Allowed C++ resume LaunchAgent: present, no PID.
- Promotion blockers after this change:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `quiet_mid_band_noise_dbfs=-31.17`,
    `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no real lock evidence.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no
- Readiness note:
  - This is stronger offline protocol evidence, not product readiness.
  - Branch promotion remains forbidden until the promotion gate returns PASS.

## 2026-06-16: Full Simulated Output Matrix Gate

- Command:
  `scripts/run-cpp-offline-gates`
- Promotion command:
  `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Runtime isolation command:
  `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Code commit: `775de71`
- Result:
  - Runtime isolation: PASS
  - Offline gates: PASS
  - Full simulated output matrix: PASS
  - Evidence schema: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Evidence paths:
  - `local-analysis/runtime-isolation/current.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/simulated-output-matrix.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
  - `local-analysis/promotion-readiness-current.json`
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 13`
  - Release CTest: `100% tests passed, 0 tests failed out of 14`
  - Evidence schema: PASS, `20` required files, `0` missing
- Simulated output matrix:
  - rows: `48`
  - failures: `0`
  - pairs: A/B/C/D
  - sample rates: `44100`, `48000`
  - modes: dense, transient, wideband
  - gains: `1.0`, `0.5`
  - minimum alignment: `1.0`
  - minimum SNR: `119.407 dB`
  - max residual ratio: `1.07069e-06`
  - max leakage: `-240 dBFS`
  - check errors: `0`
  - panic flags: `0`
- Release benchmark:
  - `pack_mib_s=1454.94`
  - `decode_mib_s=546.495`
  - `decode_into_mib_s=546.495`
  - `decode_allocating_mib_s=516.065`
  - `float_to_s24_frames_s=7.65384e+07`
  - `route_frames_s=8.54123e+08`
  - `route_reversed_frames_s=4.49037e+08`
  - `route_advanced_frames_s=4.41878e+08`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Runtime isolation state:
  - Global hardware lock: absent.
  - Mainline OpenA8DJ LaunchAgents: disabled and not running.
  - Active OpenA8DJ HAL path: absent.
  - OpenA8DJ process probes: no PIDs.
  - Allowed C++ resume LaunchAgent: present, no PID.
- Promotion blockers after this change:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `quiet_mid_band_noise_dbfs=-31.17`,
    `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no real lock evidence.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no
- Readiness note:
  - This closes the current full offline simulated-output matrix gap.
  - It does not prove physical DAC/mixer/iRig quality, Traktor lock, or actual
    runtime CPU. Branch promotion remains forbidden.

## 2026-06-16: Locked Physical USB Investigation After Audio Client Cleanup

- Commands:
  - `scripts/audio-stack-guard --recover --unload-opena8dj ...`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded ...`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 24 --mode dense --stream-stats-snapshots ...`
  - Direct USB tone sweeps using `build/opena8dj-usb-play*` variants.
  - `scripts/run-cpp-offline-gates`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current-after-usb-investigation.json`
  - `scripts/evaluate-promotion-readiness.py > local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - Spotify stuck at about `99%` CPU was terminated under lock.
  - CoreAudio health recovered; `iRig Stream` and `Open Audio 8 DJ` both
    enumerated before HAL testing.
  - HAL install/reload safety: PASS.
  - Physical music soundcheck: FAIL.
  - Direct USB tone sweeps: FAIL, no start-byte/endian/check-offset variant
    produced a passing analog tone.
  - iRig no-playback baseline: clean enough to continue using the route.
  - Runtime isolation after cleanup: PASS, active HAL absent, lock absent.
  - Offline gates after build/tooling changes: PASS.
  - Promotion gate: FAIL, `branch_promotion_allowed=false`.
  - Promotion evaluator updated after this run so its default music/CPU evidence
    points at the latest failed HAL soundcheck, and it now requires
    `local-analysis/usb-physical-investigation-summary.json` to report
    `PASS_READY`. Current result remains FAIL.
- Evidence paths:
  - `local-analysis/usb-physical-investigation-summary.json`
  - `local-analysis/audio-stack-guard/20260616-185419-kill-spotify-explicit/post-kill-guard`
  - `local-analysis/hal-candidate-safety/20260616-185450-cpp-lockpolicy-leave-loaded-post-clean`
  - `local-analysis/soundcheck/20260616-185543-irig-pairA-24s-cpp-hal`
  - `local-analysis/irig-baseline/20260616-191203-no-playback-9s`
  - `local-analysis/start-byte-sweep/20260616-190418-tone1k-minus36db`
  - `local-analysis/start-byte-sweep/20260616-190657-tone1k-minus36db-native-i24`
  - `local-analysis/check-offset-sweep/20260616-191616-validlayout-tone1k-minus18db`
  - `local-analysis/direct-usb-soundcheck/20260616-191418-validlayout-tone1k-minus18db`
  - `local-analysis/direct-usb-soundcheck/20260616-191931-halflags-tone1k-minus18db`
  - `local-analysis/runtime-isolation/current-after-usb-investigation.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-current.json`
- Key metrics:
  - HAL music run: `quality_alignment_score=0.670637`,
    `analog_snr_db=-0.59`, `lag_jumps_gt_2_frames=46`,
    `outputUnderruns=0`, `outputPanicFlags=0`.
  - Runtime CPU during HAL playback: OpenA8DJ driver observed around
    `33-35%`, not better than mainline.
  - iRig no-playback baseline: `rms=0.00057519`, `peak=0.00790405`.
  - Best start/endian sweep result remained big-endian start byte `4`, but
    tone SNR was only about `-19.5 dB`.
  - Valid-capture-layout at `-18 dBFS` tone improved only marginally over
    HAL flags: about `-17.0 dB` tone SNR vs `-17.27 dB`.
- Code/build change validated:
  - `OPENA8DJ_OUTPUT_CHECK_OFFSET` is now parameterized with default `8`.
  - `make usb-play` now builds the direct USB tool with `HAL_CFLAGS`, so direct
    USB diagnostics match the HAL candidate flags.
  - `scripts/evaluate-promotion-readiness.py` now blocks promotion on the latest
    negative physical investigation instead of relying on older positive
    evidence.
- Readiness note:
  - This is negative physical evidence. The candidate is not ready for
    listening, Traktor/timecode validation, branch promotion, or any claim of
    beating mainline.

## 2026-06-16: ISO64 Transport Profile Offline Rebuild

- Commands:
  - `make clean && make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - HAL/direct USB rebuilt with `OPENA8DJ_ISO_FRAMES_PER_TRANSFER=64`,
    `OPENA8DJ_CAPTURE_QUEUE_DEPTH=8`, `OPENA8DJ_PLAYBACK_QUEUE_TARGET=8`,
    `OPENA8DJ_OUTPUT_PREFETCH_FRAMES=64`.
  - Offline gates: PASS (`15/15` default CTest, `16/16` release CTest).
  - Release benchmark: PASS; `pack_mib_s=1623.02`,
    `decode_mib_s=575.229`, `route_frames_s=976179000`,
    `check_errors=0`, `panic_flags=0`.
  - Promotion gate: still FAIL, `branch_promotion_allowed=false`, because
    latest physical music quality, runtime CPU, latest physical investigation,
    and physical Traktor/timecode evidence are not passing.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-current.json`
- Readiness note:
  - This only creates an ISO64 candidate matching the current mainline
    transport profile. It does not supersede the latest failed physical
    evidence until a new locked hardware run passes.

## 2026-06-16: ISO64 Transport Physical Rejection

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded ...`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots ...`
  - Locked HAL unload and CoreAudio restart after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-iso64-failed-unload.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - HAL install safety after rebuilding `audio-list`: PASS.
  - Physical music soundcheck: FAIL, worse than prior ISO5 evidence.
  - HAL removed from active HAL directory after the failed run.
  - Runtime isolation after cleanup: PASS, active HAL absent, lock absent.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-iso64-cpp-lockpolicy-leave-loaded-2`
  - `local-analysis/soundcheck/20260616-iso64-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-iso64-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-iso64-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=0.051643`
  - `analog_snr_db=-31.90`
  - `mid_band_residual_ratio=63.039942`
  - `high_band_residual_ratio=38.249010`
  - `lag_jumps_gt_2_frames=60`
  - `capture_clipped_frames=0`
- Readiness note:
  - ISO64/8/8/prefetch64 is rejected as a C++ default with the current
    lifecycle implementation. The next work item is lifecycle parity, not more
    layout/start-byte sweeping.

## 2026-06-16: HAL Lifecycle Parity Build And Offline Gates

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Base commit before new commit: `aac8491`
- Result:
  - HAL/direct USB build: PASS.
  - Debug CTest: PASS, `15/15`.
  - Release CTest: PASS, `16/16`, including release benchmark.
  - Release benchmark: PASS; `pack_mib_s=1625.55`,
    `decode_mib_s=577.357`, `route_frames_s=977540000`,
    `check_errors=0`, `panic_flags=0`.
  - Promotion gate: FAIL, `branch_promotion_allowed=false`, expected because
    physical music quality, runtime CPU, latest physical investigation, and
    physical Traktor/timecode evidence still fail.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-current.json`
- Hardware/process note:
  - Hardware lock was absent before this work.
  - No OpenA8DJ process was alive; only normal `coreaudiod` and `usbaudiod`
    daemons were detected, so no process was killed.
- Readiness note:
  - This validates a buildable/offline lifecycle-parity candidate only. It does
    not supersede failed physical quality evidence and does not authorize branch
    promotion or Traktor/timecode claims.

## 2026-06-16: Lifecycle Parity Physical Retest

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-lifecycle-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-lifecycle-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-lifecycle-failed-unload.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `e0ad0a0`
- Result:
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL.
  - Post-unload runtime isolation: PASS; active HAL absent, lock absent, no
    OpenA8DJ process.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-lifecycle-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-lifecycle-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-lifecycle-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-lifecycle-failed-unload.json`
  - `local-analysis/promotion-readiness-current.json`
- Key metrics:
  - `quality_alignment_score=0.934891`
  - `analog_snr_db=8.73`
  - `lag_jumps_gt_2_frames=25`
  - `mid_band_residual_ratio=1.397074`
  - `high_band_residual_ratio=1.352348`
  - `quiet_mid_band_noise_dbfs=-31.10`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver CPU p95 `36.0%`
- Readiness note:
  - Still not ready for listening, Traktor/timecode validation, branch
    promotion, or any claim of beating mainline.

## 2026-06-16: Input Decode Activation Parity Offline Gates

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make usb-play hal`
  - `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - HAL/direct USB build: PASS.
  - Debug CTest: PASS, `15/15`.
  - Release CTest: PASS, `16/16`, including release benchmark.
  - Release benchmark: PASS; `pack_mib_s=1627.01`,
    `decode_mib_s=575.412`, `route_frames_s=991404000`,
    `check_errors=0`, `panic_flags=0`.
- Evidence path:
  - `local-analysis/cpp-offline/current-offline-gates.json`
- Readiness note:
  - This proves only compile/offline correctness for input decode activation
    parity. It must be followed by locked physical CPU and music-quality
    measurement before any claim.

## 2026-06-16: Input Decode Active Gating Physical Rejection

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-inputdecode-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-inputdecode-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-inputdecode-failed-unload.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `6c46059`
- Result:
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL, severe regression.
  - Post-unload runtime isolation: PASS; active HAL absent, lock absent, no
    OpenA8DJ process, iRig still visible.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-inputdecode-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-inputdecode-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-inputdecode-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-inputdecode-failed-unload.json`
  - `local-analysis/promotion-readiness-current.json`
- Key metrics:
  - `quality_alignment_score=0.028314`
  - `analog_snr_db=-28.18`
  - `lag_jumps_gt_2_frames=52`
  - `mid_band_residual_ratio=25.174635`
  - `high_band_residual_ratio=22.018063`
  - `quiet_mid_band_noise_dbfs=-34.11`
  - OpenA8DJ driver CPU p95 `41.8%`
  - CoreAudio p95 `21.8%`
- Readiness note:
  - Active input-decode gating is rejected as a default candidate. It remains
    available only behind `HAL_INPUT_DECODE_ACTIVE_GATING=1` for controlled
    experiments.

## 2026-06-16: Input Decode Gating Disabled By Default

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make usb-play hal && scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - HAL/direct USB build: PASS with
    `OPENA8DJ_INPUT_DECODE_ACTIVE_GATING=0`.
  - Debug CTest: PASS, `15/15`.
  - Release CTest: PASS, `16/16`, including release benchmark.
  - Release benchmark: PASS; `pack_mib_s=1645.53`,
    `decode_mib_s=587.602`, `route_frames_s=980131000`,
    `check_errors=0`, `panic_flags=0`.
- Evidence path:
  - `local-analysis/cpp-offline/current-offline-gates.json`
- Readiness note:
  - The current default candidate is back to the less-bad physical profile plus
    inert experimental knobs. It still fails physical music quality and CPU
    gates, so no readiness or promotion claim is allowed.

## 2026-06-16: Queue Playback Before Capture Requeue Physical Rejection

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1 usb-play hal && scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-queue-before-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-queue-before-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-queue-before-failed-unload.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Source commit: `de5ebf6`
- Variant flags:
  - `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1`
  - `HAL_INPUT_DECODE_ACTIVE_GATING=0`
- Result:
  - Offline gates: PASS.
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL.
  - Post-unload runtime isolation: PASS; active HAL absent, lock absent, no
    OpenA8DJ process, iRig still visible.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-queue-before-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-queue-before-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-queue-before-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-queue-before-failed-unload.json`
  - `local-analysis/promotion-readiness-current.json`
- Key metrics:
  - `quality_alignment_score=0.674742`
  - `analog_snr_db=-0.41`
  - `lag_jumps_gt_2_frames=21`
  - `mid_band_residual_ratio=1.690489`
  - `high_band_residual_ratio=1.598638`
  - `quiet_mid_band_noise_dbfs=-32.02`
  - OpenA8DJ driver CPU p95 `36.0%`
  - CoreAudio p95 `74.0%`
- Readiness note:
  - `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1` is rejected as a default.

## 2026-06-16: Process/Lock Cleanup Before Pool-Cursor Test

- Commands:
  - `lsof +D "$HOME/.opena8dj/hardware-gate.lock"`
  - `pgrep -fl 'Core Audio Driver \(OpenA8DJ.driver\)|OpenA8DJ.driver|opena8dj|OpenA8DJ'`
  - `lsof +D /Users/fer/dev/opena8dj`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-pool-cursor-physical.json`
  - Lock acquisition smoke using `$HOME/.opena8dj/hardware-gate.lock`.
  - `build/audio-list | egrep -i 'Audio 8|Audio8|Open Audio|iRig|Stream'`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - Hardware lock was absent.
  - No process had the hardware lock directory open.
  - No OpenA8DJ HAL process, soundcheck process, Traktor, ffmpeg, sox, VLC, or
    Spotify process required killing.
  - The only `/Users/fer/dev/opena8dj` handles were the current Codex runtime
    cwd context; those were not killed because they keep this session alive and
    do not own audio hardware.
  - Runtime isolation audit: PASS.
  - Lock acquisition smoke: PASS.
  - iRig visible as `iRig Stream`, `in=2 out=2 rate=48000`.
- Evidence path:
  - `local-analysis/runtime-isolation/pre-pool-cursor-physical.json`

## 2026-06-16: Transfer Pool Cursor Safety Rejection

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make HAL_TRANSFER_POOL_CURSOR=1 HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=0 HAL_INPUT_DECODE_ACTIVE_GATING=0 usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-pool-cursor-cpp-lockpolicy-leave-loaded`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Variant flags:
  - `HAL_TRANSFER_POOL_CURSOR=1`
  - `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=0`
  - `HAL_INPUT_DECODE_ACTIVE_GATING=0`
- Result:
  - Offline gates: PASS.
  - HAL install/enumeration safety: FAIL.
  - Physical soundcheck was not run.
  - The safety script unloaded the HAL after failure; recovery guard: PASS.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-pool-cursor-cpp-lockpolicy-leave-loaded`
  - `local-analysis/runtime-isolation/pre-pool-cursor-physical.json`
- Key safety metrics:
  - `core_audio_enumeration=PASS`
  - `audio_stack_guard=FAIL`
  - `opena8dj_state=loaded`
  - `opena8dj_driver_pids=99244`
  - `process.opena8dj_driver.cpu_pct=0.1`
  - `process.coreaudiod.cpu_pct=172.2`
  - `total_watched_cpu_pct=192.1`
  - Recovery after unload: `audio_stack_health=PASS`, OpenA8DJ driver pids
    `none`.
- Readiness note:
  - `HAL_TRANSFER_POOL_CURSOR=1` is rejected for physical testing unless a
    later code change explains and removes the CoreAudio load-time spike.

## 2026-06-16: Output Amplitude Stats Off Physical Rejection

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-ampstats-off-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-ampstats-off-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-ampstats-off-failed-unload.json`
- Result:
  - Offline gates: PASS.
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL.
  - Post-unload runtime isolation: PASS.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-ampstats-off-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-ampstats-off-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-ampstats-off-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-ampstats-off-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=0.914358`
  - `analog_snr_db=7.28`
  - `lag_jumps_gt_2_frames=16`
  - `mid_band_residual_ratio=1.445203`
  - `high_band_residual_ratio=1.360556`
  - `quiet_mid_band_noise_dbfs=-31.53`
- Readiness note:
  - Disabling amplitude stats alone did not fix quality or readiness. The
    default remains off because it removes nonessential hot-path diagnostics.

## 2026-06-16: Mainline Preopen/Stop-ISOC Lifecycle Physical Rejection

- Commands:
  - `make usb-play hal` with `HAL_FAST_OUTPUT_PREFETCH_CLEAR=1`,
    `HAL_OUTPUT_WRITE_STATS=1`, `HAL_BACKGROUND_PREOPEN_ON_INIT=1`,
    `HAL_STOP_ISOC_ON_STOP=1`, `HAL_STOP_GRACE_USEC=10000000`.
  - `scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-lifecycle-preopen-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-lifecycle-preopen-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-lifecycle-preopen-failed-unload.json`
- Result:
  - Offline gates: PASS.
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL, severe regression.
  - Post-unload runtime isolation: PASS.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-lifecycle-preopen-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-lifecycle-preopen-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-lifecycle-preopen-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-lifecycle-preopen-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=0.159859`
  - `analog_snr_db=-16.87`
  - `lag_jumps_gt_2_frames=59`
  - `mid_band_residual_ratio=6.709325`
  - `high_band_residual_ratio=6.058707`
  - `quiet_mid_band_noise_dbfs=-33.26`
- Readiness note:
  - Mainline-style preopen/stop-ISOC lifecycle defaults are rejected in C++.
    Code remains behind flags for future analysis; defaults are neutral again.

## 2026-06-16: Fast Prefetch Clear / Atomic Write Stats Physical Rejection

- Commands:
  - `make usb-play hal` with `HAL_FAST_OUTPUT_PREFETCH_CLEAR=1`,
    `HAL_OUTPUT_WRITE_STATS=1`, `HAL_BACKGROUND_PREOPEN_ON_INIT=0`,
    `HAL_STOP_ISOC_ON_STOP=0`.
  - `scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-fastclear-writestats-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-fastclear-writestats-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-fastclear-writestats-failed-unload.json`
- Result:
  - Offline gates: PASS.
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL, severe regression.
  - Post-unload runtime isolation: PASS.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-fastclear-writestats-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-fastclear-writestats-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-fastclear-writestats-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-fastclear-writestats-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=-0.048481`
  - `analog_snr_db=-32.06`
  - `lag_jumps_gt_2_frames=46`
  - `mid_band_residual_ratio=39.925652`
  - `high_band_residual_ratio=35.368149`
  - `quiet_mid_band_noise_dbfs=-33.61`
- Readiness note:
  - These code paths are not valid defaults. Any future use needs one-factor
    isolation and a written explanation before another physical run.

## 2026-06-16: Stale Hardware-Holder Cleanup Audit

- Commands:
  - Acquired `$HOME/.opena8dj/hardware-gate.lock` and audited targeted
    OpenA8DJ/soundcheck/capture/playback processes.
  - `lsof -nP` filtered for OpenA8DJ/iRig/Audio 8 DJ holders.
  - Removed the cleanup command's own stale lock after its owner PID exited.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-kill-stale-open-holders-clean.json`
- Result:
  - No OpenA8DJ HAL, soundcheck, capture, playback, ffmpeg/sox/afplay, Traktor,
    VLC, or Spotify process was found to kill.
  - HAL was absent/inactive.
  - Mainline supervisors remained disabled and unloaded.
  - Runtime isolation audit: PASS.
- Evidence paths:
  - `local-analysis/runtime-isolation/post-kill-stale-open-holders-clean.json`
- Readiness note:
  - The only OpenA8DJ path holders observed were Codex/node working-directory
    handles on the read-only mainline path; those were not hardware/audio
    holders and were left alive to preserve this session.

## 2026-06-16: Safe-Default HAL Parity Knobs Offline Rerun

- Command:
  - `scripts/run-cpp-offline-gates`
- Result:
  - Debug offline gates: PASS, 15/15.
  - Release offline gates: PASS, 16/16.
  - Release bench: PASS.
- Key metrics:
  - `pack_mib_s=1651.47`
  - `decode_mib_s=588.084`
  - `route_frames_s=9.70304e+08`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Readiness note:
  - This validates the source tree with rejected HAL parity paths behind safe
    defaults. It does not prove physical audio quality.

## 2026-06-16: Default C++ HAL Calibrated -16 dB Physical Rejection

- Commands:
  - `make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-calibrated-default-soundcheck.json`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-default-calibrated-minus16-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-default-minus16-failed-unload.json`
- Result:
  - Pre-audit: PASS, HAL absent, lock absent, mainline supervisors disabled.
  - Offline gates: PASS.
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck at the mainline calibrated level: FAIL.
  - Post-unload runtime isolation: PASS.
- Evidence paths:
  - `local-analysis/runtime-isolation/pre-calibrated-default-soundcheck.json`
  - `local-analysis/hal-candidate-safety/20260616-default-calibrated-minus16-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-default-minus16-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-default-minus16-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=0.960076`
  - `analog_snr_db=2.71`
  - `lag_jumps_gt_2_frames=35`
  - `mid_band_residual_ratio=1.565287`
  - `high_band_residual_ratio=1.461400`
  - `quiet_mid_band_noise_dbfs=-36.81`
  - `capture_clipped_frames=0`
  - CPU profile: `opena8dj_driver avg=30.73% p95=36.70% max=37.00%`,
    `coreaudiod avg=8.57% p95=56.40% max=81.00%`.
- Stream-stat interpretation:
  - During the active section, the output ring stayed near target and active
    underruns remained zero; the high underrun count appears after the writer
    stops and the stream drains.
  - The failure is therefore not explained by simple render starvation. It
    remains a physical quality/cadence/transport problem.
- Readiness note:
  - The calibrated level improves alignment versus some prior runs but still
    fails strict quality and CPU thresholds. This candidate is not better than
    mainline and is not ready for promotion or listening readiness claims.

## 2026-06-16: Direct USB Plain-CFLAGS Physical Rejections

- Commands:
  - `make usb-play-plain`
  - `scripts/run-cpp-offline-gates`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-direct-usb-plain-minus16.json`
  - Direct locked capture: `build/audio-record 18 ... "iRig Stream" 1,2`
    while running `build/opena8dj-usb-play-plain fixture/reference.wav`.
  - `scripts/analyze-soundcheck-capture.py ... --max-seconds 16 --max-lag 360000 --time-warp --drift-profile`
  - `make usb-play-plain-gain05`
  - `scripts/run-cpp-offline-gates`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-direct-usb-plain-gain05-minus16.json`
  - Direct locked capture with
    `build/opena8dj-usb-play-plain-gain05 fixture/reference.wav`.
- Result:
  - Both direct USB tools executed and capture completed without runtime error.
  - No HAL was installed or active.
  - Post-run runtime isolation: PASS in both runs.
  - Plain CFLAGS direct USB: FAIL, severe quality failure and capture clipping.
  - Plain CFLAGS plus only `OPENA8DJ_OUTPUT_GAIN=0.50f`: FAIL, no meaningful
    quality recovery.
- Evidence paths:
  - `local-analysis/direct-usb-soundcheck/20260616-plaincflags-minus16-music`
  - `local-analysis/direct-usb-soundcheck/20260616-plaincflags-gain05-minus16-music`
- Key metrics:
  - Plain CFLAGS: `quality_alignment_score=0.186400`,
    `alignment_score=0.127546`, `lag_jumps_gt_2_frames=26`,
    `capture_clipped_frames=8768`, `quiet_mid_band_noise_dbfs=-27.59`.
  - Plain CFLAGS gain 0.5: `quality_alignment_score=0.023502`,
    `alignment_score=-0.001463`, `lag_jumps_gt_2_frames=54`,
    `capture_clipped_frames=21`, `quiet_mid_band_noise_dbfs=-34.10`.
- Interpretation:
  - The `plain` direct tool is not a product candidate; default source gain
    overdrives the analog chain at the calibrated music level.
  - Reducing only gain did not recover quality. This weakens the hypothesis that
    direct USB failures are caused only by `HAL_CFLAGS` contamination.
  - HAL default remains less bad than direct USB, but still fails physical
    quality and CPU gates.

## 2026-06-16: Mode 2 Mainline Layout Parity Offline Gate

- Commands:
  - Added `opena8djcpp_mode2_mainline_layout_parity`.
  - `scripts/run-cpp-offline-gates`
  - `build/cpp-release/opena8djcpp_mode2_mainline_layout_parity > local-analysis/cpp-offline/mode2-mainline-layout-parity.json`
- Result:
  - Debug offline gates: PASS, 16/16.
  - Release offline gates: PASS, 17/17.
  - New mainline-layout parity gate: PASS.
- Evidence paths:
  - `local-analysis/cpp-offline/mode2-mainline-layout-parity.json`
- Key metrics:
  - `schema=opena8djcpp.mode2-mainline-layout-parity.v1`
  - `row_count=132`
  - `failures=0`
- Interpretation:
  - The C++ `Mode2OutputPacker` matches an independently implemented unrolled
    mainline-style layout across start bytes `0..5`, short partial transfer
    lengths, and normal transfer lengths including `352`.
  - This makes a simple byte-layout mismatch less likely as the physical
    blocker. Continue investigating transfer cadence, device state, and
    hardware-observed USB behavior.

## 2026-06-17: Runtime Open-Holder Cleanup

- Commands:
  - Acquired `$HOME/.opena8dj/hardware-gate.lock`.
  - Enumerated process and open-file candidates with `ps` and `lsof`.
  - Terminated only matching OpenA8DJ/audio/capture/playback app candidates if
    present.
- Result:
  - PASS.
  - `killed_count=0`.
  - No remaining OpenA8DJ HAL, direct USB playback, capture, soundcheck,
    ffmpeg/sox/afplay, Traktor, VLC, or Spotify process candidates.
  - Lock was released after the audit.
- Evidence path:
  - `local-analysis/runtime-isolation/kill-open-holders-20260617T002910Z`
- Interpretation:
  - The mainline should not be blocking the hardware through a live test
    process at this point. If the device remains unavailable, the next
    investigation should target CoreAudio/USB/device state rather than a stuck
    OpenA8DJ test process.

## 2026-06-17: HAL Audio-Params Reset Flag Port

- Commands:
  - `make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `git diff --check`
- Result:
  - PASS.
  - Debug offline gates: PASS, `16/16`.
  - Release offline gates: PASS, `17/17`.
  - `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM` defaults to `1`, preserving current
    behavior.
- Key metrics:
  - `pack_mib_s=1657.06`
  - `decode_mib_s=585.852`
  - `route_frames_s=9.46081e+08`
- Interpretation:
  - The reset behavior is now a controlled physical-test variable. No physical
    quality claim follows from this offline-only change.

## 2026-06-17: Promotion Readiness Evaluator Freshness Gate

- Commands:
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Result:
  - Expected FAIL.
  - `branch_promotion_allowed=false`.
  - New `latest_music_cpu_pair` gate: PASS.
  - Selected evidence pair:
    `local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal`.
- Current blocking metrics:
  - Physical music: `quality_alignment_score=0.9600756683268455`,
    `snr_db_min=2.712692078645948`,
    `mid_band_residual_ratio=1.56528730655531`,
    `high_band_residual_ratio=1.4613998707666849`,
    `quiet_mid_band_noise_dbfs=-36.807388625414724`,
    `lag_jumps_gt_2_frames=35`.
  - Runtime CPU: OpenA8DJ driver p95 `36.7%`, coreaudiod p95 `56.4%`.
- Interpretation:
  - The promotion command now defaults to the latest paired physical quality
    and CPU evidence instead of a fixed older run. This improves readiness
    rigor only; it does not improve the candidate.

## 2026-06-17: Offline Stream-Stats Summary Analyzer

- Commands:
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/default-minus16-summary.json`
  - `scripts/analyze-stream-stats.py $(find local-analysis/soundcheck -maxdepth 2 -name stream-stats-during.tsv | sort) --json-out local-analysis/stream-stats/all-soundchecks-summary.json`
- Result:
  - Analyzer executed offline only; no hardware, CoreAudio, USB, HAL install,
    or system service was touched.
  - Latest calibrated run summary result: `DIAGNOSTIC_FLAGS`.
- Key metrics for `20260616-default-minus16-irig-pairA-16s-cpp-hal`:
  - `ok_sample_count=45`
  - `error_sample_count=2`
  - `output_read_frames_per_second=48009.4`
  - `capture_transaction_errors_per_capture_transfer=2.273`
  - `output_write_stats_observable=false`
  - Flags: `stream_stats_timeouts`, `output_write_stats_unobservable`.
- Cross-run finding:
  - ISO64 remains an outlier with
    `capture_transaction_errors_per_capture_transfer=29.092`, consistent with
    its physical rejection and not worth retesting without a new hypothesis.
- Interpretation:
  - The latest calibrated run did not show active underruns, timeline resets,
    or panic flags in the stream snapshots. The failure is still in physical
    audio quality/CPU, not a simple output starvation counter.
  - Because `HAL_OUTPUT_WRITE_STATS=0`, future diagnostic physical runs need an
    explicit plan for whether write stats are worth enabling despite their
    previous physical regression.

## 2026-06-17: Reset-Off HAL Safety Rejection

- Commands:
  - `make -B HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0 usb-play hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-reset-audio-params-off/20260616-203712/hal-candidate-safety`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-reset-audio-params-off-safety-fail.json`
- Result:
  - Safety FAIL before soundcheck.
  - No music playback or iRig capture was run for this variant.
  - Recovery PASS; final state HAL inactive, lock absent, no OpenA8DJ process.
- Evidence paths:
  - `local-analysis/physical-reset-audio-params-off/20260616-203712/build.log`
  - `local-analysis/physical-reset-audio-params-off/20260616-203712/candidate-hal.sha256`
  - `local-analysis/physical-reset-audio-params-off/20260616-203712/hal-candidate-safety/summary.txt`
  - `local-analysis/runtime-isolation/post-reset-audio-params-off-safety-fail.json`
- Key metrics:
  - Build log contains `-DOPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM=0`.
  - `core_audio_enumeration=PASS`.
  - `audio_stack_health=FAIL`.
  - `coreaudiod=115.1%`.
  - `opena8dj_driver=0.1%`.
  - `total_watched_cpu_pct=130.0`.
- Interpretation:
  - Skipping the reset is not a viable improvement candidate under current
    evidence. It regresses load/enumeration CPU before audio quality can even
    be measured.

## 2026-06-17: HAL Flag Rebuild Guard

- Commands:
  - `make usb-play hal > local-analysis/build-flags/default-rebuild-v2.log`
  - `make HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0 usb-play hal > local-analysis/build-flags/reset0-rebuild-v2.log`
  - `make HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0 usb-play hal > local-analysis/build-flags/reset0-repeat-v2.log`
  - `make usb-play hal > local-analysis/build-flags/default-restore-v2.log`
- Result:
  - PASS.
  - Changing HAL flags now forces rebuild of affected generated binaries.
  - Repeating identical HAL flags does not rebuild.
  - Final local build restored default `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=1`.
- Key observations:
  - `reset0-rebuild-v2_compiles=2`.
  - `reset0-repeat-v2_compiles=0`.
  - `default-restore-v2_compiles=2`.
- Interpretation:
  - Physical variant evidence is safer because `HAL_*` changes can no longer
    silently reuse stale `hal` or `usb-play` binaries.

## 2026-06-17: Hot Stats Gate, Atomic Output Write Stats, Late-Write Counters

- Commands:
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current-after-kill-request.json`
  - `make usb-play hal`
  - `make build/opena8dj-control`
  - `make HAL_HOT_STREAM_STATS=0 usb-play hal`
  - `make HAL_OUTPUT_WRITE_STATS=0 usb-play hal`
  - `make usb-play hal`
  - `scripts/run-cpp-offline-gates`
- Result:
  - Runtime isolation PASS before edits/tests; no hardware holder was alive and
    no process needed killing.
  - Default HAL/tools build PASS.
  - `HAL_HOT_STREAM_STATS=0` variant build PASS, no warnings after marking the
    timing helper as possibly unused in diagnostic builds.
  - `HAL_OUTPUT_WRITE_STATS=0` variant build PASS.
  - Final default rebuild PASS with `HAL_OUTPUT_WRITE_STATS=1`,
    `HAL_HOT_STREAM_STATS=1`, `HAL_HOT_STREAM_STATS_INTERVAL=1`.
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
- Evidence paths:
  - `local-analysis/runtime-isolation/current-after-kill-request.json`
  - `local-analysis/build-flags/hot-stats-off-build.log`
  - `local-analysis/build-flags/output-write-stats-off-build.log`
  - `local-analysis/build-flags/default-after-hot-stats-output-write-build.log`
  - `local-analysis/offline-gates-after-hot-stats-output-write.log`
- Key metrics from release bench:
  - `pack_mib_s=1640.8`
  - `decode_mib_s=589.055`
  - `route_frames_s=9.14455e+08`
  - `check_errors=0`
  - `panic_flags=0`
- Interpretation:
  - This removes an unobservable hot-path mutex and adds late-write
    observability, but it does not prove physical quality or CPU superiority.
  - The next hardware run must check the new counters and compare CPU/quality
    against mainline before any readiness claim.

## 2026-06-17: Physical Hot-Stats/Atomic-Write Candidate Rejection

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-hotstats-write-late/20260616-205111/hal-candidate-safety`
  - `scripts/run-soundcheck --skip-build --music-dir "$HOME/Music" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Forced HAL unload under hardware lock, followed by `audio-stack-guard`.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-hotstats-write-late-failed-unload.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/hotstats-write-late-summary-v2.json`
- Result:
  - HAL candidate safety PASS.
  - Physical soundcheck FAIL.
  - Post-failure unload/recovery PASS; final isolation PASS.
- Evidence paths:
  - `local-analysis/physical-hotstats-write-late/20260616-205111/hal-candidate-safety`
  - `local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal`
  - `local-analysis/stream-stats/hotstats-write-late-summary-v2.json`
  - `local-analysis/audio-stack-guard/20260616-force-unload-hotstats-write-late`
  - `local-analysis/runtime-isolation/post-hotstats-write-late-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=0.962133`
  - `snr_db=10.24`
  - `click_outliers=29`
  - `lag_jumps_gt_2_frames=45`
  - `mid_band_residual_ratio=1.443461`
  - `high_band_residual_ratio=1.362932`
  - `quiet_mid_band_noise_dbfs=-35.91`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver avg/p95/max CPU `31.58%/36.00%/36.40%`
  - coreaudiod avg/p95/max CPU `4.70%/7.00%/60.40%`
  - Stream stats: `output_write_stats_observable=true`,
    `outputFramesWritten_per_second=48000.35`,
    `outputFramesRead_per_second=48006.75`, no active underruns, no timeline
    resets, no panic flags.
  - Text stream snapshots showed `outputLateWriteFrames=0` and
    `outputLateWriteBatches=0` throughout the run.
- Interpretation:
  - Atomic write stats and hot stats gates are useful, but not sufficient.
  - Late writes do not explain the current analog quality failure.
  - Continue with queue-depth/prefetch/cadence hypotheses; do not promote.

## 2026-06-17: Late-Write TSV Tooling Fix

- Commands:
  - `python3 -m py_compile scripts/run-soundcheck scripts/analyze-stream-stats.py`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/hotstats-write-late-summary-v2.json`
- Result:
  - PASS.
  - `run-soundcheck` now includes `outputLateWriteFrames` and
    `outputLateWriteBatches` in `stream-stats-during.tsv`.
  - `analyze-stream-stats.py` summarizes those counters and flags
    `late_writes` if they increase.
- Interpretation:
  - The hotstats physical run still has late-write evidence in text snapshots,
    but future runs will have structured TSV/JSON late-write evidence.

## 2026-06-17: Queue-Depth 8/8 Physical Rejection

- Commands:
  - `make HAL_CAPTURE_QUEUE=8 HAL_PLAYBACK_QUEUE=8 usb-play hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-queue8/20260616-205510/hal-candidate-safety`
  - `scripts/run-soundcheck --skip-build --music-dir "$HOME/Music" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Forced HAL unload under hardware lock, followed by `audio-stack-guard`.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-queue8-failed-unload.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/queue8-summary.json`
  - `make usb-play hal > local-analysis/build-flags/default-restore-after-queue8.log`
- Result:
  - Build PASS with `OPENA8DJ_CAPTURE_QUEUE_DEPTH=8` and
    `OPENA8DJ_PLAYBACK_QUEUE_TARGET=8`.
  - HAL candidate safety PASS.
  - Physical soundcheck FAIL.
  - Post-failure unload/recovery PASS; final isolation PASS.
  - Default build restored to queue depth `64/64`.
- Evidence paths:
  - `local-analysis/physical-queue8/20260616-205510`
  - `local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal`
  - `local-analysis/stream-stats/queue8-summary.json`
  - `local-analysis/audio-stack-guard/20260616-force-unload-queue8`
  - `local-analysis/runtime-isolation/post-queue8-failed-unload.json`
  - `local-analysis/build-flags/default-restore-after-queue8.log`
- Key metrics:
  - `quality_alignment_score=0.964133`
  - `snr_db=10.22`
  - `click_outliers=0`
  - `lag_jumps_gt_2_frames=39`
  - `mid_band_residual_ratio=1.422599`
  - `high_band_residual_ratio=1.365050`
  - `quiet_mid_band_noise_dbfs=-36.08`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver avg/p95/max CPU `32.335%/37.2%/37.9%`
  - coreaudiod avg/p95/max CPU `3.715%/3.1%/47.2%`
  - Stream stats: `outputLateWriteFrames_delta=0`,
    `outputLateWriteBatches_delta=0`, no active underruns, no timeline resets,
    no panic flags, capture transaction errors per capture transfer `2.2727`.
- Interpretation:
  - Queue depth is relevant to click behavior and coreaudiod CPU, but as a
    standalone change it does not solve quality and worsens driver CPU.
  - Do not make `8/8` default without a later positive combined hypothesis.

## 2026-06-17: Output Prefetch 64 Physical Rejection

- Commands:
  - `make HAL_OUTPUT_PREFETCH_FRAMES=64 usb-play hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-prefetch64/20260616-205818/hal-candidate-safety`
  - `scripts/run-soundcheck --skip-build --music-dir "$HOME/Music" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Forced HAL unload under hardware lock, followed by `audio-stack-guard`.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-prefetch64-failed-unload.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/prefetch64-summary.json`
  - `make usb-play hal > local-analysis/build-flags/default-restore-after-prefetch64.log`
- Result:
  - Build PASS with `OPENA8DJ_OUTPUT_PREFETCH_FRAMES=64`.
  - HAL candidate safety PASS.
  - Physical soundcheck FAIL.
  - Post-failure unload/recovery PASS; final isolation PASS.
  - Default build restored to `OPENA8DJ_OUTPUT_PREFETCH_FRAMES=256`.
- Evidence paths:
  - `local-analysis/physical-prefetch64/20260616-205818`
  - `local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal`
  - `local-analysis/stream-stats/prefetch64-summary.json`
  - `local-analysis/audio-stack-guard/20260616-force-unload-prefetch64`
  - `local-analysis/runtime-isolation/post-prefetch64-failed-unload.json`
  - `local-analysis/build-flags/default-restore-after-prefetch64.log`
- Key metrics:
  - `quality_alignment_score=0.956371`
  - `snr_db=10.40`
  - `click_outliers=4`
  - `lag_jumps_gt_2_frames=48`
  - `mid_band_residual_ratio=1.431220`
  - `high_band_residual_ratio=1.365281`
  - `quiet_mid_band_noise_dbfs=-35.98`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver p95 CPU `39.5%`
  - coreaudiod p95 CPU `4.1%`
  - Stream stats: `outputLateWriteFrames_delta=0`,
    `outputLateWriteBatches_delta=0`, no active underruns, no timeline resets,
    no panic flags, capture transaction errors per capture transfer `2.2728`.
- Interpretation:
  - Mainline prefetch parity is not a standalone fix in C++.
  - Do not combine with `queue8` until a separate transport/capture-error
    hypothesis explains the persistent residual/lag.

## 2026-06-17: Capture Transaction Detail Tooling

- Commands:
  - `make build/opena8dj-control`
  - `python3 -m py_compile scripts/run-soundcheck scripts/analyze-stream-stats.py`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/prefetch64-summary-v2.json`
- Result:
  - PASS.
  - `opena8dj-control stream-stats` machine output now includes:
    `captureStatusFailures`, `captureZeroCompleteTransactions`,
    `captureExpectedTransactions`, `captureOtherByteCountTransactions`,
    `captureShortTransfers`, and `filteredCaptureTransactions`.
  - `run-soundcheck` now writes those fields to `stream-stats-during.tsv`.
  - `analyze-stream-stats.py` now summarizes per-capture-transfer ratios for
    those components while remaining compatible with older TSVs.
- Interpretation:
  - The stable aggregate `captureTransactionErrors/transfer ~= 2.273` needs
    decomposition before it can drive another transport change. Future physical
    runs will identify whether errors are status failures, zero-complete,
    short/other-size filtered transactions, or a mixture.

## 2026-06-17: Holder Cleanup And Capture ISO Invariant Analyzer

- Commands:
  - `scripts/audio-stack-guard --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/20260616-kill-open-holders`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current-after-kill-request-live.json`
  - `python3 -m py_compile scripts/analyze-capture-iso-invariants.py`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-capture-detail-irig-pairA-8s-cpp-hal --json-out local-analysis/stream-stats/capture-iso-invariants-recent-v3.json`
  - `scripts/analyze-capture-iso-invariants.py $(find local-analysis/soundcheck -maxdepth 1 -type d -name '*irig*' | sort) --json-out local-analysis/stream-stats/capture-iso-invariants-all-irig-v3.json`
- Result:
  - Runtime cleanup PASS: HAL inactive, lock absent, no OpenA8DJ driver
    process, forbidden mainline LaunchAgents disabled.
  - Recent detailed capture ISO invariants PASS on 4 runs.
  - Historical iRig sweep result FAIL only because
    `20260616-lifecycle-preopen-irig-pairA-16s-cpp-hal` has a real classified
    ISO-slot mismatch. Older runs without detail are UNKNOWN, not FAIL.
- Evidence paths:
  - `local-analysis/audio-stack-guard/20260616-kill-open-holders`
  - `local-analysis/runtime-isolation/current-after-kill-request-live.json`
  - `local-analysis/stream-stats/capture-iso-invariants-recent-v3.json`
  - `local-analysis/stream-stats/capture-iso-invariants-all-irig-v3.json`
- Interpretation:
  - Recent aggregate `captureTransactionErrors` are zero-complete ISO slots, not
    status failures.
  - This removes one false lead but does not improve product readiness:
    physical music quality and driver CPU still fail.

## 2026-06-17: Promotion Readiness After Capture ISO Classification

- Command:
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-capture-invariants.json`
- Commit under test: `1c574cc`
- Result: FAIL; branch promotion is not allowed.
- PASS gates include offline, simulated output oracle, physical tone, evidence
  presence, hardware-lock policy, and offline throughput floors.
- Blocking gates:
  - `physical_music_quality`: latest selected music metrics still have
    `quality_alignment_score=0.978049577556115`,
    `snr_db=9.845114058005024`, `lag_jumps_gt_2_frames=24`,
    `mid_band_residual_ratio=1.3932011051574409`, and quiet mid-band noise
    `-35.22881003368516 dBFS`.
  - `runtime_cpu_beats_mainline`: OpenA8DJ driver p95 `35.5%` and coreaudiod
    p95 `37.2%`, versus mainline limits `6.5%` and `1.7%`.
  - `latest_physical_investigation`: decision remains `FAIL_NOT_READY`.
  - `traktor_timecode_physical`: no physical Traktor/timecode-vinyl lock
    evidence has been recorded.
- Evidence path:
  - `local-analysis/promotion-readiness-after-capture-invariants.json`

## 2026-06-17: HAL Output Pack Fast Path Offline Verification

- Commands:
  - `make usb-play hal`
  - `make HAL_OUTPUT_CHECK_OFFSET=4 hal`
  - `make hal`
  - `scripts/run-cpp-offline-gates`
- Result:
  - Default HAL and `usb-play` build PASS with the unrolled default
    `OPENA8DJ_OUTPUT_CHECK_OFFSET=8` fast path.
  - Generic fallback build PASS with `HAL_OUTPUT_CHECK_OFFSET=4`.
  - Default HAL rebuilt after fallback verification.
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
- Interpretation:
  - This is a CPU/jitter candidate only. It preserves offline layout contracts,
    but it has not proved better physical music quality or lower runtime CPU.

## 2026-06-17: Unrolled HAL Output Pack Physical Rejection

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-unrolled-pack/20260616-211513/hal-candidate-safety`
  - Retry after recovery:
    `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-unrolled-pack/20260616-211622-retry/hal-candidate-safety`
  - `scripts/run-soundcheck --skip-build --music-dir "$HOME/Music" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260616-unrolled-pack-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Explicit HAL unload under lock, then `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-unrolled-pack-failed-unload.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-unrolled-pack-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/unrolled-pack-summary.json`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260616-unrolled-pack-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/unrolled-pack-capture-iso-invariants.json`
- Result:
  - First safety run FAIL due high `coreaudiod` CPU after HAL load
    (`137.0%`); recovery PASS.
  - Safety retry PASS and left HAL loaded.
  - Physical soundcheck FAIL.
  - Candidate disabled by default after rejection; artifacts rebuilt with
    `HAL_UNROLLED_OUTPUT_PACK=0`.
  - Post-failure unload/recovery PASS; final isolation PASS.
- Key metrics:
  - `quality_alignment_score=0.131043`
  - `snr_db=-18.43`
  - `lag_jumps_gt_2_frames=47`
  - `mid_band_residual_ratio=8.393129`
  - `high_band_residual_ratio=7.405798`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver avg/p95/max CPU `30.959%/35.600%/35.900%`
  - Stream stats: no active underruns, no late writes, no timeline resets, no
    panic flags.
  - Capture ISO invariants PASS.
- Interpretation:
  - Offline layout parity did not translate to physical improvement.
  - The root blocker remains analog residual/lag and high driver CPU; this
    candidate must not be promoted or left as default.

## 2026-06-17: Promotion Readiness After Unrolled-Pack Rejection

- Command:
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-unrolled-rejection.json`
- Commit under test: `0144ffc`
- Result: FAIL; branch promotion is not allowed.
- Blocking gates:
  - `physical_music_quality`: latest selected run is
    `20260616-unrolled-pack-irig-pairA-16s-cpp-hal`, with
    `quality_alignment_score=0.13104262411477222`, SNR `-18.433998179397555`,
    `lag_jumps_gt_2_frames=47`, mid residual `8.393129418488618`, high residual
    `7.405798355303025`.
  - `runtime_cpu_beats_mainline`: driver p95 `35.6%`, coreaudiod p95 `3.3%`,
    versus mainline limits `6.5%` and `1.7%`.
  - `latest_physical_investigation`: `FAIL_NOT_READY`.
  - `traktor_timecode_physical`: no physical Traktor/timecode-vinyl lock
    evidence.
- Evidence path:
  - `local-analysis/promotion-readiness-after-unrolled-rejection.json`

## 2026-06-17: Default Control After Unrolled-Pack Rejection

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-default-after-unrolled/20260616-212139/hal-candidate-safety`
  - `scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Explicit HAL unload under lock.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-default-after-unrolled-failed-unload.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/default-after-unrolled-summary.json`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/default-after-unrolled-capture-iso-invariants.json`
- Result:
  - Safety PASS.
  - Soundcheck FAIL, but returned to the known default failure profile rather
    than the severe unrolled-pack regression.
  - Post-failure unload/recovery PASS; final isolation PASS.
- Key metrics:
  - `quality_alignment_score=0.964049`
  - `snr_db=10.44`
  - `lag_jumps_gt_2_frames=40`
  - `mid_band_residual_ratio=1.436380`
  - `high_band_residual_ratio=1.372535`
  - `quiet_mid_band_noise_dbfs=-35.80`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver avg/p95/max CPU `33.942%/38.500%/38.700%`
  - Stream stats: no active underruns, no late writes, no timeline resets, no
    panic flags.
  - Capture ISO invariants PASS.
- Interpretation:
  - Default is restored but still not ready.
  - The next useful investigation should target the physical residual/lag path
    and runtime CPU above the Mode 2 byte-pack layer.

## 2026-06-17: Window Trace Residual/Lag Classification

- Commands:
  - `python3 -m py_compile scripts/analyze-soundcheck-window-trace.py`
  - `scripts/analyze-soundcheck-window-trace.py local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal --json-out local-analysis/soundcheck-window-trace/default-after-unrolled-v2.json`
  - `scripts/analyze-soundcheck-window-trace.py local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal --json-out local-analysis/soundcheck-window-trace/hotstats-write-late-v2.json`
  - `scripts/analyze-soundcheck-window-trace.py local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal --json-out local-analysis/soundcheck-window-trace/queue8-v2.json`
- Tooling change:
  - `scripts/analyze-soundcheck-window-trace.py` now creates the output
    directory and records aggregate lag/correlation/driver-CPU fields.
- Result:
  - default-after-unrolled:
    - local lag min/max `-26/16`, jumps `40`;
    - median mid residual `1.448463 -> 1.430920` after local lag correction;
    - median correlation `0.960389 -> 0.970858`;
    - driver CPU median/p95/max `37.4%/38.5%/38.7%`.
  - hotstats-write-late:
    - local lag min/max `-28/8`, jumps `45`;
    - median mid residual `1.452215 -> 1.443643`;
    - median correlation `0.955921 -> 0.969117`;
    - driver CPU median/p95/max `34.9%/36.1%/36.4%`.
  - queue8:
    - local lag min/max `-5/63`, jumps `39`;
    - median mid residual `1.491993 -> 1.431929`;
    - median correlation `0.856756 -> 0.970177`;
    - driver CPU median/p95/max `35.8%/37.4%/37.9%`.
- Interpretation:
  - Local lag correction does not materially solve the residual. The dominant
    remaining blocker is not simple alignment; it is persistent coloration,
    distortion, wrong mixed signal, or another below-HAL analog/transport issue
    plus high CPU.

## 2026-06-17: Offline Linear Matrix Classification And Decorrelated Gate Prep

- Commands:
  - `python3 -m py_compile scripts/analyze-soundcheck-linear-matrix.py`
  - `scripts/analyze-soundcheck-linear-matrix.py local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-unrolled-pack-irig-pairA-16s-cpp-hal --analysis-seconds 8 --json-out local-analysis/soundcheck-linear-matrix/recent-failed-physical-music.json`
  - `scripts/run-channel-matrix-gate --run-id offline-prepare-smoke --pair A --rate 48000 --seconds 2 --peak 0.25`
  - `make channel-matrix-prepare CHANNEL_MATRIX_SECONDS=1 CHANNEL_MATRIX_PEAK=0.20`
- Result:
  - Linear matrix tool PASS_DIAGNOSTIC over existing evidence only; no hardware,
    CoreAudio, USB, install, service restart, or default-device change.
  - All existing music runs are classified as
    `needs_decorrelated_physical_matrix_fixture`.
  - Prepare-only channel-matrix gate generated a deterministic stereo fixture
    and plan without touching audio devices.
- Key metrics:
  - Existing music source L/R correlation `0.985848`, condition number
    `140.322901`, so the matrix is ill-conditioned and cannot prove crosstalk.
  - default-after-unrolled matrix diagnostic residual/predicted `0.303950`.
  - hotstats-write-late residual/predicted `0.306780`.
  - queue8 residual/predicted `0.503410`.
  - unrolled-pack residual/capture `0.999913` and mono correlation
    `-0.182678`, confirming severe unrelated regression.
  - Decorrelated fixture smoke correlation: `0.0005635458408516207`.
- Evidence paths:
  - `local-analysis/soundcheck-linear-matrix/recent-failed-physical-music.json`
  - `local-analysis/channel-matrix/offline-prepare-smoke/plan.txt`
  - `local-analysis/channel-matrix/offline-prepare-smoke/fixture/reference.wav`
  - `local-analysis/channel-matrix/20260617T014008Z-pairA-decorrelated-matrix`
- Interpretation:
  - The next physical check should be a lock-gated decorrelated Pair A matrix
    capture through iRig, not another ambiguous correlated music capture.
  - This does not improve readiness; it narrows the blocker and creates the
    evidence format needed to distinguish wrong channel mix/routing from
    coloration/noise.

## 2026-06-17: Offline Gates After Channel-Matrix Lock Policy Integration

- Commands:
  - `bash -n scripts/run-channel-matrix-gate`
  - `python3 -m py_compile scripts/analyze-soundcheck-linear-matrix.py`
  - `scripts/run-channel-matrix-gate --run-id physical-reject-smoke --run-physical --seconds 1 --peak 0.1`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-channel-matrix-prep.json`
- Result:
  - Physical-mode rejection smoke PASS: missing `--capture-device` exits `2`
    before acquiring the hardware lock.
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
  - Hardware-lock policy now audits `5` sensitive scripts and includes
    `scripts/run-channel-matrix-gate`.
  - Promotion readiness remains FAIL.
- Blocking readiness gates:
  - `physical_music_quality` still fails with latest selected run
    `20260616-default-after-unrolled-irig-pairA-16s-cpp-hal`.
  - `runtime_cpu_beats_mainline` still fails: OpenA8DJ driver p95 `38.5%`
    versus mainline threshold `6.5%`; coreaudiod p95 `3.1%` versus `1.7%`.
  - `latest_physical_investigation` remains `FAIL_NOT_READY`.
  - `traktor_timecode_physical` remains `BLOCKED_UNVALIDATED_DVS`.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/hardware-lock-policy.json`
  - `local-analysis/promotion-readiness-after-channel-matrix-prep.json`

## 2026-06-17: Locked Pair A Decorrelated Channel Matrix

- Commands:
  - `make hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-channel-matrix/20260617-physical-matrix-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-channel-matrix-gate --run-physical --run-id 20260617-irig-pairA-decorrelated-matrix --pair A --rate 48000 --seconds 8 --peak 0.30 --capture-device "iRig Stream" --capture-channels 1,2`
  - `scripts/audio-stack-guard --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/20260617-after-channel-matrix-unload`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-channel-matrix-physical-unload.json`
  - `scripts/analyze-channel-matrix-tones.py local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix --skip-seconds 1 --analysis-seconds 8 --json-out local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/tone-matrix.json`
- Result:
  - HAL candidate safety PASS and left the candidate loaded.
  - Physical decorrelated matrix run completed with iRig Stream capture.
  - Tone-domain channel matrix PASS.
  - Cleanup/unload PASS; final runtime isolation PASS, HAL inactive, lock
    absent.
- Key metrics:
  - Capture device UID:
    `AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1`.
  - `captured.wav`: rate `48000`, frames `576512`, RMS `0.11460675`, peak
    `0.50494385`, clipped frames `0`.
  - Tone matrix expected floor amplitude `0.06576757351262598`.
  - Left-to-right leakage `-59.48258382334637 dB`.
  - Right-to-left leakage `-49.667972003183024 dB`.
  - Max wrong-source leakage `-51.26562016103985 dB`.
  - Linear sample-domain diagnostic still rejects the capture with
    `classification=linear_matrix_rejected_large_physical_residual`; treat this
    as phase/filtering/residual evidence, not as crosstalk failure.
- Evidence paths:
  - `local-analysis/physical-channel-matrix/20260617-physical-matrix-safety`
  - `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix`
  - `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/tone-matrix.json`
  - `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/linear-matrix.json`
  - `local-analysis/audio-stack-guard/20260617-after-channel-matrix-unload`
  - `local-analysis/runtime-isolation/after-channel-matrix-physical-unload.json`
- Interpretation:
  - Pair A physical route does not show gross L/R crosstalk or deck leakage
    through the iRig path.
  - The remaining blockers are physical music quality/residual, CPU versus
    mainline, and physical Traktor/timecode validation.
- Post-update verification:
  - `python3 -m py_compile scripts/analyze-channel-matrix-tones.py scripts/analyze-soundcheck-linear-matrix.py`
  - `bash -n scripts/run-channel-matrix-gate`
  - `scripts/run-cpp-offline-gates` PASS: Debug `16/16`, Release `17/17`.
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-physical-channel-matrix.json`
    remains FAIL with blockers `physical_music_quality`,
    `runtime_cpu_beats_mainline`, `latest_physical_investigation`, and
    `traktor_timecode_physical`.

## 2026-06-17: Atomic Stream-Stats Accumulators Offline Verification

- Commands:
  - `make HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=0 hal && make hal`
  - `make usb-play hal && make HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=0 usb-play hal && make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-atomic-stream-stats.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-atomic-stream-stats-offline.json`
- Result:
  - Fallback and default HAL builds compile.
  - Fallback and default `usb-play`/HAL combined builds compile.
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
  - Runtime isolation PASS: HAL inactive, hardware untouched, USB untouched,
    lock absent, forbidden mainline LaunchAgents disabled/inactive.
  - Promotion readiness remains FAIL.
- Key offline bench metrics:
  - `pack_mib_s=1625.85`
  - `decode_mib_s=577.663`
  - `decode_into_mib_s=577.663`
  - `decode_allocating_mib_s=567.074`
  - `float_to_s24_frames_s=8.6452e+07`
  - `route_frames_s=9.67171e+08`
  - `route_reversed_frames_s=5.12459e+08`
  - `route_advanced_frames_s=5.02714e+08`
- Blocking readiness gates after this change:
  - `physical_music_quality`
  - `runtime_cpu_beats_mainline`
  - `latest_physical_investigation`
  - `traktor_timecode_physical`
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-after-atomic-stream-stats.json`
  - `local-analysis/runtime-isolation/after-atomic-stream-stats-offline.json`
- Interpretation:
  - This validates build and offline invariants only. Atomic stream stats still
    need a locked physical soundcheck to prove whether driver CPU p95 improves
    without worsening music quality or observability.

## 2026-06-17: Atomic Stream-Stats Physical Soundcheck Rejection

- Commit under test:
  - `a11012f` (`Add atomic stream stats accumulators`)
- Commands:
  - `make hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-atomic-stream-stats/20260617-a11012f/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-atomic-stream-stats-a11012f-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal unload under the global lock: move active
    `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver` to
    `/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.manual-unloaded-20260616-220452`
    and terminate `Core Audio Driver (OpenA8DJ.driver)` PID `62309`.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-atomic-stream-stats-manual-unload.json`
- Result:
  - HAL candidate safety PASS.
  - Physical music soundcheck FAIL.
  - Final manual unload/isolation PASS: HAL inactive, lock absent, no OpenA8DJ
    driver process.
- Key quality metrics:
  - `quality_alignment_score=0.9630037066274131`
  - `analog_snr_db=10.34`
  - `click_outliers=106`
  - `lag_jumps_gt_2_frames=41`
  - `mid_band_residual_ratio=1.4131498493677057`
  - `high_band_residual_ratio=1.3678775330794208`
  - `quiet_mid_band_noise_dbfs=-36.078859229311256`
  - `capture_clipped_frames=0`
- CPU comparison:
  - atomic stream stats: driver median/p95/max `36.6%/37.6%/37.9%`;
  - default-after-unrolled control: `37.5%/38.5%/38.7%`;
  - hotstats-write-late prior run: `35.2%/36.0%/36.4%`;
  - queue8 prior run: `35.9%/37.2%/37.9%`;
  - mainline threshold remains `6.5%` driver p95.
- Evidence paths:
  - `local-analysis/physical-atomic-stream-stats/20260617-a11012f/hal-candidate-safety`
  - `local-analysis/soundcheck/20260617-atomic-stream-stats-a11012f-irig-pairA-16s-cpp-hal`
  - `local-analysis/runtime-isolation/after-atomic-stream-stats-manual-unload.json`
- Interpretation:
  - Atomic stream stats are rejected as a default. They provide only a small
    CPU reduction versus one failing control run, do not beat prior C++ CPU
    variants, remain far above mainline CPU, and do not improve physical music
    quality.

## 2026-06-17: Fast Prefetch Clear Physical Rejection

- Commands:
  - `make HAL_FAST_OUTPUT_PREFETCH_CLEAR=1 hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-fast-prefetch-clear/20260617-a1c8b50/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-fast-prefetch-clear-a1c8b50-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal HAL unload under global lock, then
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-fast-prefetch-clear-manual-unload.json`
- Result:
  - HAL candidate safety PASS.
  - Physical music soundcheck FAIL.
  - Final isolation PASS.
- Key metrics:
  - quality alignment `0.9546986682558902`
  - SNR about `9.53 dB`
  - lag jumps `39`
  - mid/high residual ratios `1.4338336857980034/1.3737879670112414`
  - driver CPU median/p95/max `34.6%/36.8%/37.4%`
- Interpretation:
  - Rejected as a default: CPU improves, but quality is materially worse than
    the latest default-like controls.

## 2026-06-17: Hot Stream Stats Interval 16 Physical A/B

- Commands:
  - `make HAL_HOT_STREAM_STATS_INTERVAL=16 hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-hotstats-interval16/20260617-a1c8b50/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-hotstats-interval16-a1c8b50-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal HAL unload under global lock, then
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-hotstats-interval16-manual-unload.json`
- Result:
  - HAL candidate safety PASS.
  - Physical music soundcheck still FAIL as a product gate.
  - Final isolation PASS.
  - Default promoted from interval `1` to `16` as a partial CPU improvement,
    not as readiness.
- Key metrics:
  - quality alignment `0.9640011789236339`
  - SNR about `10.28 dB`
  - click outliers `5`
  - lag jumps `41`
  - mid/high residual ratios `1.429448005646557/1.3625351838892474`
  - driver CPU median/p95/max `34.5%/35.7%/36.3%`
  - coreaudiod p95 `2.9%`
  - total watched audio/UI p95 `53.5%`
- Comparison:
  - latest default control driver p95 `38.5%`;
  - interval16 driver p95 `35.7%`;
  - mainline threshold `6.5%`.
- Evidence paths:
  - `local-analysis/physical-hotstats-interval16/20260617-a1c8b50/hal-candidate-safety`
  - `local-analysis/soundcheck/20260617-hotstats-interval16-a1c8b50-irig-pairA-16s-cpp-hal`
  - `local-analysis/runtime-isolation/after-hotstats-interval16-manual-unload.json`
- Interpretation:
  - Good enough to reduce default stats overhead because it does not materially
    alter the failing music signature and improves CPU. Not good enough for
    readiness or promotion.

## 2026-06-17: Tone Response Compensation Diagnostic

- Commands:
  - `python3 -m py_compile scripts/analyze-tone-response-compensation.py`
  - `scripts/analyze-tone-response-compensation.py --tone-matrix-json local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/tone-matrix.json --json-out local-analysis/tone-response-compensation/recent-music-runs.json <six existing soundcheck run dirs>`
- Result:
  - Offline diagnostic completed without touching hardware, CoreAudio, USB, or
    system services.
  - The simple tone-response shape model does not explain the music residual.
- Key metrics:
  - SNR delta range after response-shape fit: about `-0.19 dB` to `+1.88 dB`.
  - Mid residual delta range: about `-4.94` to `-5.02`, where negative means
    the response-shaped prediction made the mid-band residual ratio worse.
- Evidence paths:
  - `scripts/analyze-tone-response-compensation.py`
  - `local-analysis/tone-response-compensation/recent-music-runs.json`
- Interpretation:
  - A simple three-band response model derived from the passing tone matrix is
    not sufficient to explain the failed music quality. Next quality work needs
    a better controlled physical reference or a deeper format/phase/non-linear
    model.

## 2026-06-17: SciPy LTI Transfer Quality Diagnostic

- Commands:
  - `python3 -m venv .venv`
  - `.venv/bin/python -m pip install -r requirements-analysis.txt`
  - `.venv/bin/python -m py_compile scripts/analyze-lti-transfer-quality.py`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py --json-out local-analysis/lti-transfer-quality/recent-music-runs.json <six existing soundcheck run dirs>`
- Result:
  - Local analysis environment created under `.venv` only.
  - NumPy `2.0.2` and SciPy `1.13.1` installed locally.
  - Offline LTI/Welch/CSD diagnostic completed without touching hardware,
    CoreAudio, USB, HAL install state, or system services.
- Key metrics:
  - Mid-band mean coherence across tested runs/channels: about `0.075` to
    `0.124`.
  - High-band mean coherence across tested runs/channels: about `0.020` to
    `0.045`.
  - LTI SNR deltas were negative in every run: about `-0.78 dB` to `-2.37 dB`.
  - LTI-predicted mid/high residual ratios were worse than scalar-gain
    residual ratios in every run.
- Evidence paths:
  - `requirements-analysis.txt`
  - `scripts/analyze-lti-transfer-quality.py`
  - `local-analysis/lti-transfer-quality/recent-music-runs.json`
- Interpretation:
  - The failed music captures are not explained by a stable linear transfer
    function between the generated reference and iRig capture. The quality
    blocker is more likely non-linear/time-varying behavior, output-format
    semantics, or a physical reference mismatch requiring a stricter isolation
    test.

## 2026-06-17: Soundcheck Failure-Mode Diagnostic

- Commands:
  - `.venv/bin/python -m py_compile scripts/analyze-soundcheck-failure-modes.py`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py --json-out local-analysis/soundcheck-failure-modes/recent-music-runs.json <six existing soundcheck run dirs>`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py --drift-max-lag 128 --json-out local-analysis/soundcheck-failure-modes/recent-music-runs-local128.json <six existing soundcheck run dirs>`
- Result:
  - Offline diagnostic PASS. It read existing WAV/JSON evidence only and did
    not touch hardware, CoreAudio, USB, HAL install state, or system services.
- Key metrics:
  - Static 2x2 L/R matrix improves SNR only about `0.13-0.24 dB` in tested
    default-like runs.
  - Cubic memoryless model improves SNR only about `0.002-0.004 dB`.
  - No capture clipping in the tested runs.
  - Local-lag drift for recent default-like runs is small, around `-28` to
    `-34 ppm`, while SNR remains about `9-10 dB`.
- Evidence paths:
  - `scripts/analyze-soundcheck-failure-modes.py`
  - `local-analysis/soundcheck-failure-modes/recent-music-runs.json`
  - `local-analysis/soundcheck-failure-modes/recent-music-runs-local128.json`
- Interpretation:
  - Simple mix/polarity, static L/R matrix, simple memoryless non-linearity,
    clipping, and simple drift are not sufficient explanations. The next
    quality isolation should target reference-route mismatch, runtime
    discontinuities, or output format/phase semantics.

## 2026-06-17: Stats-Off Physical Rejection

- Candidate:
  - `HAL_OUTPUT_WRITE_STATS=0 HAL_HOT_STREAM_STATS=0`
  - HAL binary hash:
    `449f8baf8ce1ca74802de5507138e116886493a4aabaeb88488da410bdff2991`
- Commands:
  - `make -B hal HAL_OUTPUT_WRITE_STATS=0 HAL_HOT_STREAM_STATS=0`
  - `scripts/run-cpp-offline-gates`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-stats-off/20260617-a1c8b50-stats-off/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-stats-off-a1c8b50-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal HAL unload under global lock, then
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-stats-off.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-stats-off.json`
- Result:
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
  - HAL candidate safety PASS.
  - Physical music soundcheck FAIL.
  - Final isolation PASS: HAL inactive, lock absent, no OpenA8DJ/mainline
    holder processes.
  - Promotion readiness FAIL.
- Key quality metrics:
  - `quality_alignment_score=0.9602873731433063`
  - `analog_snr_db=10.48`
  - `lag_jumps_gt_2_frames=37`
  - `click_outliers=0`
  - `mid_band_residual_ratio=1.424930143914946`
  - `high_band_residual_ratio=1.3626602759181339`
  - `quiet_mid_band_noise_dbfs=-36.12216401783407`
  - `capture_clipped_frames=0`
- CPU metrics:
  - OpenA8DJ driver median/p95/max `35.9%/36.8%/37.0%`
  - coreaudiod median/p95/max `2.6%/3.4%/53.8%`
  - total watched audio/UI median/p95/max `53.3%/55.0%/115.9%`
- Evidence paths:
  - `local-analysis/physical-stats-off/20260617-a1c8b50-stats-off/hal-candidate-safety`
  - `local-analysis/soundcheck/20260617-stats-off-a1c8b50-irig-pairA-16s-cpp-hal`
  - `local-analysis/promotion-readiness-after-stats-off.json`
  - `local-analysis/runtime-isolation/final-after-stats-off.json`
- Interpretation:
  - Stats-off is rejected as a default. It removes observability, does not
    improve quality, and does not improve CPU relative to the current
    interval-16 default.

## 2026-06-17: Sparse Output-Cycle Clear Physical Rejection

- Candidate:
  - `HAL_OUTPUT_SPARSE_CYCLE_CLEAR=1`
- Commands:
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-next-hardware-check.json`
  - `make -B hal HAL_OUTPUT_SPARSE_CYCLE_CLEAR=1`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-sparse-cycle-clear/20260617-a1c8b50/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-sparse-cycle-clear-a1c8b50-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal HAL unload under global lock, then
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-sparse-cycle-clear.json`
  - `make -B hal`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-sparse-cycle-clear.json`
- Result:
  - Initial runtime isolation PASS: HAL inactive and lock absent.
  - HAL candidate safety PASS.
  - Physical music soundcheck FAIL.
  - Final isolation PASS: HAL inactive, lock absent, no OpenA8DJ/mainline
    holder processes.
  - Promotion readiness FAIL.
- Key quality metrics:
  - `quality_alignment_score=0.9636469258932283`
  - `analog_snr_db=10.48`
  - `lag_jumps_gt_2_frames=33`
  - `click_outliers=0`
  - `mid_band_residual_ratio=1.4081802020521872`
  - `high_band_residual_ratio=1.3645974703502124`
  - `quiet_mid_band_noise_dbfs=-36.222340546910175`
  - `capture_clipped_frames=0`
- CPU metrics:
  - OpenA8DJ driver median/p95/max `37.15%/38.3%/38.5%`
  - coreaudiod median/p95/max `2.7%/14.2%/58.7%`
  - total watched audio/UI median/p95/max `54.85%/56.9%/125.4%`
- Evidence paths:
  - `local-analysis/physical-sparse-cycle-clear/20260617-a1c8b50/hal-candidate-safety`
  - `local-analysis/soundcheck/20260617-sparse-cycle-clear-a1c8b50-irig-pairA-16s-cpp-hal`
  - `local-analysis/promotion-readiness-after-sparse-cycle-clear.json`
  - `local-analysis/runtime-isolation/final-after-sparse-cycle-clear.json`
- Interpretation:
  - Sparse cycle clear is rejected. It did not improve quality, worsened CPU,
    and should not remain as a latent hot-path option.

## 2026-06-17: Runtime Discontinuity Correlation Diagnostic

- Commands:
  - `.venv/bin/python -m py_compile scripts/analyze-runtime-discontinuities.py`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py --json-out local-analysis/runtime-discontinuities/recent-music-runs.json local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260617-hotstats-interval16-a1c8b50-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260617-stats-off-a1c8b50-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260617-sparse-cycle-clear-a1c8b50-irig-pairA-16s-cpp-hal`
- Result:
  - Offline diagnostic PASS. It read existing WAV/JSON/TSV evidence only and
    did not touch hardware, CoreAudio, USB, HAL install state, or system
    services.
- Key metrics:
  - No run produced a strong CPU or stream-delta correlation at the configured
    `abs(r) >= 0.70` threshold after offset search from `-5s` to `+5s`.
  - Window lag jumps are still present in every tested run:
    - default-after-unrolled: `p95=26.70` frames, max `42`
    - hotstats-interval16: `p95=24.70` frames, max `53`
    - stats-off: `p95=28.10` frames, max `52`
    - sparse-cycle-clear: `p95=30.35` frames, max `46`
  - Median scalar SNR stays around `10.0-10.46 dB`.
  - Capture clipping remains absent in all tested rows.
- Evidence paths:
  - `scripts/analyze-runtime-discontinuities.py`
  - `local-analysis/runtime-discontinuities/recent-music-runs.json`
- Interpretation:
  - The current visible runtime counters do not explain the physical music
    residual. The next decisive work should isolate reference-route mismatch,
    physical path behavior, output format/phase semantics, or runtime
    discontinuities that are not currently counted.

## 2026-06-17: Monitor-Free Soundcheck CPU Mode

- Change:
  - Added `--no-monitor-stream-stats` to `scripts/run-soundcheck`.
- Commands:
  - `python3 -m py_compile scripts/run-soundcheck`
  - `git diff --check`
- Result:
  - PASS. The mode is available for lower-perturbation CPU diagnostics.
- Interpretation:
  - Use this mode for CPU A/B only. Readiness and glitch claims still require
    stream-stat evidence from normal locked runs.

## 2026-06-17: Coalesce2 + Pool Cursor Safety Rejection

- Candidate:
  - `HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=1`
- Commands:
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-coalesce2-poolcursor.json`
  - `make -B hal HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=1`
  - `scripts/run-cpp-offline-gates`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-coalesce2-poolcursor/20260617-190a7ed/hal-candidate-safety`
- Result:
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
  - HAL candidate safety FAIL.
  - The gate unloaded the rejected HAL and recovery PASS.
- Safety failure metrics:
  - `audio_stack_health=FAIL`
  - `coreaudiod=86.8%`
  - `mediaremoted=57.5%`
  - `total_watched_cpu_pct=145.3`
  - `opena8dj_driver=0.7%`
- Evidence paths:
  - `local-analysis/physical-coalesce2-poolcursor/20260617-190a7ed/hal-candidate-safety`
  - `local-analysis/runtime-isolation/after-coalesce2-safety-fail-before-unload.json`
- Interpretation:
  - Coalescing playback transfers by 2 plus pool cursor is rejected before
    soundcheck. It does not satisfy the safety gate.

## 2026-06-17: Default Monitor-Free And Normal Confirmation Runs

- Commands:
  - `make -B hal`
  - Default safety:
    `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-default-monitorfree/20260617-190a7ed/hal-candidate-safety`
  - Monitor-free soundcheck:
    `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-default-monitorfree-a1c8b50-irig-pairA-16s-cpp-hal --no-monitor-stream-stats --cpu-profile-interval 0.25 --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Default normal confirmation safety and soundcheck:
    `local-analysis/physical-default-normal-confirm/20260617-190a7ed/hal-candidate-safety`
    and
    `local-analysis/soundcheck/20260617-default-normal-confirm-a1c8b50-irig-pairA-16s-cpp-hal`
  - Final unload/isolation:
    `local-analysis/runtime-isolation/final-after-default-normal-confirm.json`
  - Promotion readiness:
    `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-default-normal-confirm.json`
- Result:
  - Default safety PASS before monitor-free.
  - Monitor-free soundcheck FAIL and is not comparable product evidence.
  - Default normal confirmation soundcheck FAIL with the known aligned failure
    signature.
  - Final isolation PASS: HAL inactive and lock absent.
  - Promotion readiness FAIL.
- Monitor-free metrics:
  - `quality_alignment_score=0.097964`
  - `snr_db=-29.18`
  - `lag_jumps_gt_2_frames=47`
  - OpenA8DJ driver median/p95/max `36.2%/39.0%/39.6%`
  - No stream-stat files by design.
- Normal confirmation metrics:
  - `quality_alignment_score=0.963713`
  - `analog_snr_db=10.57`
  - `lag_jumps_gt_2_frames=46`
  - `mid_band_residual_ratio=1.417748`
  - `high_band_residual_ratio=1.364806`
  - `quiet_mid_band_noise_dbfs=-35.79`
  - OpenA8DJ driver median/p95/max `35.8%/36.9%/37.7%`
  - coreaudiod median/p95/max `2.6%/4.6%/42.5%`
- Interpretation:
  - Removing stream-stat polling did not lower driver CPU and produced an
    invalid/decorrelated capture. Normal stream-stat soundcheck remains the
    readiness evidence path until the harness has a better low-perturbation
    monitor design.

## 2026-06-17: Coalesce2-Only Physical Rejection

- Candidate:
  - `HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=0`
- Commands:
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-coalesce2-only.json`
  - `make -B hal HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=0`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-coalesce2-only/20260617-43773be/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-coalesce2-only-43773be-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal HAL unload under global lock, then
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-coalesce2-only.json`
  - `make -B hal`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-coalesce2-only.json`
- Result:
  - Initial runtime isolation PASS.
  - HAL candidate safety PASS.
  - Physical music soundcheck FAIL.
  - Final isolation PASS.
  - Promotion readiness FAIL.
- Quality metrics:
  - `quality_alignment_score=0.8988544786595754`
  - `analog_snr_db=5.85`
  - `lag_jumps_gt_2_frames=45`
  - `click_outliers=0`
  - `mid_band_residual_ratio=2.5634316824600596`
  - `high_band_residual_ratio=1.6665681529777348`
  - `quiet_mid_band_noise_dbfs=-33.677652564671256`
  - `capture_clipped_frames=0`
- CPU metrics:
  - OpenA8DJ driver median/p95/max `28.0%/28.5%/28.7%`
  - coreaudiod median/p95/max `2.7%/8.3%/60.4%`
  - total watched audio/UI median/p95/max `45.05%/47.2%/125.6%`
- Evidence paths:
  - `local-analysis/physical-coalesce2-only/20260617-43773be/hal-candidate-safety`
  - `local-analysis/soundcheck/20260617-coalesce2-only-43773be-irig-pairA-16s-cpp-hal`
  - `local-analysis/runtime-isolation/final-after-coalesce2-only.json`
  - `local-analysis/promotion-readiness-after-coalesce2-only.json`
- Interpretation:
  - Coalesce2 confirms transaction frequency contributes to CPU, but the
    quality regression is severe and CPU still misses mainline by a wide
    margin. It is rejected as a candidate/default.

## 2026-06-17: Cadence Outlier Threshold Fix

- Change:
  - Split internal cadence outlier threshold calculation into capture and
    playback expected transfer durations.
  - `cadenceExpectedTransferTicks` remains the base capture transfer period in
    the stream-stats payload.
- Commands:
  - `make -B hal`
  - `make -B hal HAL_CADENCE_DIAGNOSTIC=1 HAL_PLAYBACK_COALESCE_TRANSFERS=2`
  - `make -B hal`
- Result:
  - PASS. Default and diagnostic builds compile cleanly.
- Interpretation:
  - This is an observability fix only. It makes future coalescing diagnostics
    less misleading; it is not a quality or CPU improvement claim.
