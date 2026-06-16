# C++/DriverKit Plan

## Phase 0: Isolation And Offline Core

Exit criteria:
- Dedicated worktree and branch exist.
- Core C++ builds without macOS-only dependencies.
- Offline contract tests pass.
- Living docs describe architecture, metrics, test plan, and readiness gaps.

## Phase 1: Brownfield Behavior Extraction

Exit criteria:
- USB packet format and channel order documented from mainline with file/line evidence.
- Rust gates and analyzers mapped into C++ equivalents or external comparison commands.
- Packet pack/unpack tests validate all 8 channels at 44.1 kHz and 48 kHz.

Current progress:
- Mode 2 S24 big-endian conversion and all-start-byte round-trip tests exist.
- Synthetic no-leakage gate exists for pair A into B/C/D.
- Remaining work: compare C++ byte output against mainline/Rust fixture output and add sample-rate-specific packet fixtures.

## Phase 2: Data Plane

Exit criteria:
- Preallocated packet buffers and ring buffers implemented.
- No malloc, blocking locks, per-buffer logging, disk I/O, synchronous IPC, or dynamic parsing in the real-time path.
- Synthetic throughput and jitter tests pass with reproducible evidence.

## Phase 3: DriverKit Shell

Exit criteria:
- AudioDriverKit classes are represented in an Xcode-capable skeleton.
- IOUserAudioDriver, IOUserAudioDevice, and IOUserAudioStream mapping is explicit.
- Entitlements, signing, activation, deactivation, and rollback plan are documented.
- No installation or activation occurs without explicit user window and lock.

## Phase 4: Offline Candidate Gate

Exit criteria:
- Routing matrix, packet fidelity, timecode profile policy, CPU/throughput, and jitter gates pass.
- Evidence is written under `local-analysis/cpp-offline/`.
- C++ results are compared against mainline and Rust oracle where practical.

Current progress:
- `scripts/run-cpp-offline-gates` runs default contract tests and Release performance tests.
- Release benchmark currently passes pack/decode/routing floors without hardware.
- Remaining work: timecode/DVS simulation, jitter accounting, allocation audit, and recorded fixture comparison.

## Phase 5: Physical Window Request

Exit criteria:
- Hardware plan names expected duration, actions, lock path, audio playback/recording status, install/reload status, default-device status, and evidence directory.
- Rollback/observability plan is ready.
- User explicitly grants a coordinated window.
