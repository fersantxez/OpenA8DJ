# Offline Readiness Report

Date: 2026-06-16
Worktree: `/Users/fer/dev/audio8djcpp`
Branch: `driverkit/cpp-redesign`
Base commit: `08745b7`

## Verdict

`OFFLINE_READY_FOR_PHYSICAL_WINDOW_REQUEST`

This candidate is ready to request a coordinated physical test window. It is not
claimed to be physically better than mainline until hardware capture, Traktor
validation, and listening evidence exist.

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
| Mode 2 packet behavior tested | 72-row C++ matrix plus Python oracle | PASS |
| timecode vinyl/CD-line/phono policy | timecode matrix | PASS |
| DVS/timecode deck isolation proxy | 24-row DVS signal smoke | PASS |
| realtime hot path allocation check | `realtime-audit.json` | PASS |
| jitter/timestamp model | `jitter-model.json` | PASS |
| performance floor | `offline-bench-release.json` | PASS |
| evidence schema | `evidence-schema.json` | PASS |
| safe physical plan with lock | `docs/PHYSICAL_TEST_WINDOW_PLAN.md` | PASS |

## Current Gate Summary

- Default CTest: 8/8 PASS.
- Release CTest: 9/9 PASS.
- Packet matrix: 72 rows, 0 failures.
- Python Mode 2 oracle: all start bytes PASS, 0 check errors, 0 panic flags.
- Timecode matrix: 4 profiles, 4 deck assignments, 0 failures.
- DVS signal smoke: 24 rows, 0 failures, zero leakage.
- Realtime audit: 0 hot-path allocations.
- DriverKit surface model: one 8-channel input stream and four stereo output streams.
- Jitter model: 0 regressions, max error 0.125 frames.
- Static policy: 0 forbidden hits in official offline gate path.
- Evidence schema: 15 required files, 0 missing.

## Performance Evidence

- Mode 2 pack throughput: `1653.83 MiB/s` in the latest preallocated-buffer
  gate run; nine-run median `1634.35 MiB/s`.
- Mode 2 decode throughput: `577.374 MiB/s` in the latest gate run; nine-run
  post-change median `570.726 MiB/s`.
- Routing throughput: `961,852,000 frames/s` in the latest gate run; seven-run
  post-change median `949,223,000 frames/s`.
- Check errors: `0`.
- Panic flags: `0`.

## External Blockers

These do not block offline readiness, but they block a real dext or physical
readiness claim:

- Full Xcode with DriverKit SDK is not installed/active on this machine.
- AudioDriverKit/DriverKit entitlements are not available in this environment.
- No hardware/CoreAudio/USB/Traktor/iRig/listening test has been authorized or run.

## Claim Boundary

Allowed claim:

- The C++/DriverKit line is an offline-ready candidate for a coordinated
  physical test window.

Forbidden claim:

- It is better than mainline in physical sound quality, Traktor behavior, or
  runtime CPU until authorized physical evidence proves it.
