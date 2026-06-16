# Test Evidence

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
