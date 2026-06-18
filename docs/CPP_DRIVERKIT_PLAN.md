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

## 2026-06-18 Human-Test RC Closure Plan

Hard deadline:
- 15:00 America/New_York is the operational target for a first human-test RC.
- At 12:48 EDT the remaining runway is roughly 2.2 hours, not six hours.
- The six-hour target remains useful only as a stabilization ceiling if the
  route or evidence blocks the 15:00 cut.

Current truth at 12:48 EDT:
- C++ worktree is `/Users/fer/dev/audio8djcpp` on
  `driverkit/cpp-redesign`, commit `1b09b12`.
- `build/OpenA8DJ-0.3.25.pkg` and `build/OpenA8DJ-0.3.25.dmg` exist.
- Offline gates have passed post-commit: Debug CTest `83/83`, Release CTest
  `84/84`, evidence schema PASS with `93` required files, and provenance
  freshness PASS for `1b09b12`.
- CoreAudio currently exposes `iRig Stream` and `Open Audio 8 DJ` as `8 in /
  8 out`; the audio stack is idle-healthy. No wired non-Audio8, non-built-in
  known-good output is visible for objective route revalidation.
- The active blocker is measurement-route validity, not iRig absence.

15:00 RC strategy:
1. Freeze scope to a human-test RC, not a superiority release.
   The RC may be installable and complete enough for a controlled human test,
   but it cannot claim better sound, lower CPU, full Timecode Vinyl readiness,
   or mainline replacement unless same-session physical evidence proves it.
2. Re-run the fast offline gate and package/hash check after every code or
   packaging change. No package is eligible for human testing if offline
   evidence is stale against `HEAD`.
3. Keep the installed-driver path conservative. Do not install, unload,
   reinstall, or restart audio services outside a hardware lock and explicit
   window. Prefer one deliberate install/use cycle over repeated driver churn.
4. First physical gate is route revalidation:
   Audio 8 DJ output -> known-good external capture path -> iRig Stream input.
   If no external known-good output appears, only diagnostic same-device or
   existing-evidence analysis is allowed; product claims remain forbidden.
5. Second physical gate is same-session C++ vs mainline A/B using the same
   music/reference, level, capture channel pair, route, duration, and analyzer
   thresholds. CPU must be sampled in the same window.
6. Third physical gate is Timecode Vinyl smoke: input channel presence,
   timecode-profile/routing behavior, no deck leakage, and no CPU spike. This
   can be a smoke gate for human RC; it is not a full DVS certification.

Timebox policy:
- If route validation is still blocked at 14:00 EDT, freeze the deliverable as
  a diagnostic installable RC plus written physical-window plan; do not spend
  the last hour on transport tuning.
- If route validation passes by 14:00 EDT, use the remaining window only for
  same-session C++/mainline A/B and CPU/submit-cadence evidence.
- If A/B passes but Traktor/timecode cannot be run safely before 15:00 EDT,
  label the result `human-test-rc-audio-routing-only`; do not claim Timecode
  Vinyl readiness.
- If Timecode Vinyl smoke also passes, label the result
  `human-test-rc-timecode-smoke-passed`, still not `mainline-superior` unless
  quality and CPU metrics beat mainline in the same session.

Must-have before a human-test RC can be offered:
- Clean worktree or explicit dirty-state manifest.
- Current package and installed/candidate hash recorded.
- Offline gates passing against current commit.
- Audio stack health PASS before and after the physical window.
- iRig and Audio 8 visible immediately before the run.
- Lock acquired for all playback/recording/install/reload/reset actions.
- Evidence directory containing command log, device inventory, CPU sample,
  WAV analysis, route manifest, and PASS/FAIL summary.
- Explicit final label: `human-test-rc`, not `mainline-superior`.

Abort conditions:
- iRig disappears or changes into an ambiguous capture state.
- Audio 8 disappears or exposes fewer than 8 inputs / 8 outputs.
- CoreAudio or driver CPU exceeds the written threshold before playback.
- Any driver install/reload requires an OS prompt that cannot be completed
  safely in the current window.
- Route validation fails, clips, has high timing instability, or cannot
  identify the capture/output devices unambiguously.
- C++ is not at least as stable as mainline in the same-session A/B.

Claims allowed at 15:00 if the gates pass:
- "Installable human-test RC generated."
- "Offline gates passed at commit X."
- "Physical route validated in evidence directory Y."
- "C++ candidate matched or beat mainline on measured metrics A/B/C in the
  same session" only for metrics that actually passed.

Claims still forbidden without stronger evidence:
- "Audiophile quality is guaranteed."
- "Better than mainline overall."
- "Timecode Vinyl fully certified."
- "Ready to move C++ to main."
- "Ready to delete or demote mainline C."
