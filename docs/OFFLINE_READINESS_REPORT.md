# Offline Readiness Report

Date: 2026-06-16
Worktree: `/Users/fer/dev/audio8djcpp`
Branch: `driverkit/cpp-redesign`
Candidate commit: `389b59f` plus current mandatory hardware-lock policy changes

## Verdict

`OFFLINE_READY_FOR_LOCKED_PHYSICAL_WINDOW_REQUEST`

This candidate is ready to request a coordinated physical test window. It is not
claimed to be physically better than mainline until hardware capture, Traktor
validation, runtime CPU evidence, and listening evidence exist. The active HAL
is currently intentionally absent, so any physical window must first restore or
reinstall the candidate HAL under the global hardware lock.

## 2026-06-16 Full Simulated Output Matrix Update

The offline gate now includes a deterministic C++ simulated-output matrix
covering output pairs A/B/C/D at 44.1/48 kHz with dense, transient, and
wideband program material at gains `1.0` and `0.5`.

Current evidence:
`local-analysis/cpp-offline/simulated-output-matrix.json`

Result: PASS, `48` rows, `0` failures, minimum SNR `119.407 dB`, max residual
ratio `1.07069e-06`, max leakage `-240 dBFS`.

## 2026-06-16 Mode 2 Cross-Oracle Parity Update

The offline gate now includes a byte-for-byte comparison between the C++ Mode 2
output packer and the inherited Python oracle:
`local-analysis/cpp-offline/mode2-cross-oracle-parity.json`.

Current result: PASS, `72` rows, `0` failures, `max_byte_mismatches=0`,
`max_length_delta=0`, `total_check_errors=0`, and `total_panic_flags=0`.
The C++ Float32-to-S24 conversion now follows the oracle's explicit Float32
rounding path before quantizing to output S24.

## 2026-06-16 Mandatory Hardware Lock Policy Update

The offline gate now includes `opena8djcpp_hardware_lock_policy_check`, which
verifies that hardware-sensitive scripts cannot bypass the global lock.

Current evidence:
`local-analysis/cpp-offline/hardware-lock-policy.json`

Result: PASS, `4` audited scripts, `0` missing requirements. Covered scripts
include HAL candidate safety, direct Audio 8 DJ CoreAudio gates, and physical
soundcheck.

## 2026-06-16 Routing Fast Path Update

After the initial report, identity routing gained a dedicated fast path and
mapping validation was moved out of the per-frame loop. Offline gates still
pass. Same-session routing benchmark improved from about `5.66e+08 frames/s`
to a seven-run post-change median of `9.49e+08 frames/s`.

This update only strengthens the C++ offline data-plane case. It does not
change the physical-readiness status or authorize any claim about analog sound
quality.

## 2026-06-16 Decode Bitmask Update

Mode 2 decode now tracks per-stream byte presence with fixed 6-bit masks rather
than bool arrays. Offline gates still pass. Same-session decode benchmark
improved from about `532 MiB/s` to a nine-run post-change median of
`570.726 MiB/s`.

This update only affects the C++ offline packet data plane. It does not change
the installed HAL binary already used for the partial hardware install check.

## 2026-06-16 Preallocated Packer Benchmark Update

The offline benchmark now measures `Mode2OutputPacker::fill_into()` with a
preallocated output buffer, matching the real-time policy. The latest gate run
reported `1653.83 MiB/s`; nine-run post-change median was `1634.35 MiB/s`.

This is a benchmark-method correction and data-plane evidence improvement, not
a direct apples-to-apples speed claim against the previous allocation-heavy
benchmark number.

## Proved Requirements

| Requirement | Evidence | Status |
|---|---|---|
| isolated C++ worktree | `git worktree list`, `docs/CANDIDATE_MANIFEST.json` | PASS |
| no mainline/Rust writes for official work | final `git status` checks and static policy | PASS |
| reproducible offline build | `scripts/run-cpp-offline-gates` | PASS |
| executable core | CMake targets and CTest | PASS |
| executable DriverKit scaffold/model | `opena8djcpp_driverkit_surface_model` | PASS |
| 8 inputs / 8 outputs represented | core contract and surface model | PASS |
| A/B/C/D routing represented | core contract, packet matrix, DVS smoke | PASS |
| sample rates 44.1/48 kHz represented | policy, packet matrix, surface model | PASS |
| Mode 2 packet behavior tested | 72-row C++ matrix plus Python oracle and cross-oracle byte parity | PASS |
| simulated output matrix | `simulated-output-matrix.json` | PASS |
| timecode vinyl/CD-line/phono policy | timecode matrix | PASS |
| DVS/timecode deck isolation proxy | 24-row DVS signal smoke | PASS |
| realtime hot path allocation check | `realtime-audit.json` | PASS |
| jitter/timestamp model | `jitter-model.json` | PASS |
| performance floor | `offline-bench-release.json` | PASS |
| evidence schema | `evidence-schema.json` | PASS |
| safe physical plan with lock | `docs/PHYSICAL_TEST_WINDOW_PLAN.md` | PASS |

## Current Gate Summary

- Default CTest: 15/15 PASS.
- Release CTest: 16/16 PASS.
- Packet matrix: 72 rows, 0 failures.
- Simulated output matrix: 48 rows, 0 failures.
- Python Mode 2 oracle: all start bytes PASS, 0 check errors, 0 panic flags.
- Mode 2 cross-oracle byte parity: 72 rows, 0 byte mismatches.
- Timecode matrix: 4 profiles, 4 deck assignments, 0 failures.
- DVS signal smoke: 24 rows, 0 failures, zero leakage.
- Realtime audit: 0 hot-path allocations.
- DriverKit surface model: one 8-channel input stream and four stereo output streams.
- Jitter model: 0 regressions, max error 0.125 frames.
- Static policy: 0 forbidden hits in official offline gate path.
- Hardware lock policy: 4 audited scripts, 0 missing requirements.
- Evidence schema: 22 required files, 0 missing.

## Performance Evidence

- Mode 2 pack throughput: `1546.09 MiB/s`.
- Mode 2 preallocated decode throughput: `565.894 MiB/s`.
- Identity routing throughput: `999,440,000 frames/s`.
- Reversed routing throughput: `481,809,000 frames/s`.
- Advanced mute/invert/cross-deck routing throughput:
  `472,260,000 frames/s`.
- Check errors: `0`.
- Panic flags: `0`.

## External Blockers

These do not block offline readiness, but they block a real dext or physical
readiness claim:

- Full Xcode with DriverKit SDK is not installed/active on this machine.
- AudioDriverKit/DriverKit entitlements are not available in this environment.
- The active OpenA8DJ HAL is intentionally absent after runtime cleanup.
- Physical real-music quality, runtime CPU against C `0.3.135`, and physical
  Traktor/timecode vinyl are still failing or missing in the promotion gate.

## Claim Boundary

Allowed claim:

- The C++/DriverKit line is an offline-ready candidate for a coordinated
  physical test window.

Forbidden claim:

- It is better than mainline in physical sound quality, Traktor behavior, or
  runtime CPU until authorized physical evidence proves it.
