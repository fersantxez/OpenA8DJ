# OpenA8DJ C++/DriverKit Architect Context

Date: 2026-06-16
Worktree: `/Users/fer/dev/audio8djcpp`
Branch: `driverkit/cpp-redesign`

## Current State

- This is a separate C++/DriverKit redesign line for Native Instruments Audio 8 DJ.
- `/Users/fer/dev/opena8dj` is the C/Objective-C mainline and is read-only for this effort.
- `/Users/fer/dev/audio8djrust` is the Rust experiment and is read-only for this effort.
- The first implementation target is an offline C++20 core with no macOS, CoreAudio, USB, HAL, kext, dext, or hardware side effects.
- The initial core models the Audio 8 DJ surface as 8 inputs and 8 outputs arranged as stereo pairs A/B/C/D.
- The core now has executable Mode 2 packet gates: S24 big-endian conversion, all-start-byte round trip, no deck leakage for pair-A-only data, and Release throughput gates.
- The core now has an executable protocol snapshot gate. It freezes VID/PID
  `0x17cc:0x1978`, endpoints `0x01/0x81/0x82/0x06`, CAIAQ command ids,
  8-in/8-out surface, required `44100/48000` rates, deferred `88200/96000`
  rate codes, and the explicit Mode 2 distinction between 16-byte check
  cadence and 32-byte full frame.
- Routing now supports the Rust-oracle shape of passthrough, mute, inversion,
  side swap, and cross-deck pair mapping through a fixed `RouteEntry` array and
  prevalidated `RoutingPlan`. Current advanced routing evidence:
  `route_advanced_frames_s=4.41878e+08`.
- The Mode 2 output packer now has an external byte-for-byte parity gate
  against the inherited Python oracle. Current evidence:
  `local-analysis/cpp-offline/mode2-cross-oracle-parity.json`, `72` rows,
  `0` failures, `max_byte_mismatches=0`, `max_length_delta=0`,
  `total_check_errors=0`, `total_panic_flags=0`.
- The Mode 2 decode benchmark now distinguishes the allocating developer
  wrapper from the preallocated real-time path. Current preallocated decode
  evidence: `decode_into_mib_s=569.821`, `decode_into_output_overflows=0`,
  `decode_into_check_errors=0`, `decode_into_panic_flags=0`.
- The offline simulated output matrix now covers A/B/C/D at 44.1/48 kHz across
  dense, transient, and wideband deterministic program material at gains `1.0`
  and `0.5`. Current evidence:
  `local-analysis/cpp-offline/simulated-output-matrix.json`, `48` rows,
  `0` failures, minimum SNR `119.407 dB`, max residual ratio `1.07069e-06`,
  max leakage `-240 dBFS`.
- The core now has explicit input-profile contracts: playback leaves input
  decode and software lock off, while timecode-vinyl/CD-line/phono enable input
  decode with CAIAQ modes `0/1/2`, software lock, ground-lift intent, and
  identity A/B/C/D source map.
- The core now has a Mode 2 input decode API that writes through caller-owned
  scratch/output buffers. The offline DVS packet gate packs synthetic
  quadrature carriers into Mode 2 bytes, decodes them via the selected
  timecode profile, and verifies `24` profile/rate/deck rows with zero leakage
  and playback decode-off behavior.
- Offline timecode analysis now checks Rust-oracle thresholds for synthetic
  stereo carriers: RMS, balance, frequency error ppm, p95 period jitter,
  absolute correlation, and clipping. Current evidence:
  `local-analysis/cpp-offline/timecode-signal-analysis.json`, `8` rows, `0`
  failures.
- DriverKit shell is now an offline compiled contract, not only dead skeleton
  code. It validates lifecycle start/stop idempotency and the device model
  without requiring DriverKit SDK or activating a System Extension. Current
  evidence: `local-analysis/cpp-offline/driverkit-shell-contract.json`.
- Hardware-sensitive scripts now have mandatory lock enforcement before they
  can install/reload HAL, restart `coreaudiod`, play through Audio 8 DJ, or
  capture through an external recorder. Current evidence:
  `local-analysis/cpp-offline/hardware-lock-policy.json`, result `PASS`,
  `4` audited scripts, `0` missing requirements.
- Current local toolchain check confirms no DriverKit SDK/AudioDriverKit path
  is available under the active Command Line Tools developer directory or the
  searched Xcode path, so real dext compilation remains blocked on full Xcode
  plus signing/entitlement setup.
- The core now includes a fixed-capacity SPSC frame ring contract for the
  realtime data plane. Current realtime audit pushes and pops all `2815`
  decoded frames with `0` allocations and `0` remaining frames.
- The HAL from this worktree previously enumerated as `Open Audio 8 DJ` with
  `8 in / 8 out` at `48000`, and direct CoreAudio I/O plus short input captures
  on channels `1/2` and `7/8` passed under lock. As of the runtime cleanup, the
  active HAL path is intentionally absent and the bundle is parked at
  `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver.disabled-20260616T215913Z`, so
  no physical C++ test can run until the HAL is explicitly restored/reinstalled
  under lock.
- iRig Stream was recovered after macOS showed the accessory authorization
  prompt. The prompt was not exposed through Accessibility, but a CGEvent click
  at logical screen coordinate `(900,407)` pressed `Allow`; CoreAudio then
  listed `iRig Stream` as `2 in / 2 out` at `48000`.
- Physical Audio 8 DJ -> mixer REC OUT -> iRig captures now run again. The
  current best physical tone evidence is `sideband_ratio=0.000657`,
  `click_outliers=0`, no clipping, with `ISO5/q64/start_byte=4/big/gain0.50`
  and cadence diagnostics enabled.
- The same candidate still fails the strict real-music soundcheck:
  `quality_alignment_score=0.938154`, `analog_snr_db=8.93`,
  `mid_band_residual_ratio=1.379896`, `high_band_residual_ratio=1.347577`,
  `lag_jumps_gt_2_frames=24`, `click_outliers=0`. This is comparable to some
  historical mainline physical residual runs but is not a product PASS and not
  evidence that C++ is better than mainline.
- The promotion readiness gate currently returns `FAIL` and
  `branch_promotion_allowed=false`. C++ must not be moved to `main`, and C
  must not be moved to `Legacy`, until the gate returns PASS and the user
  authorizes the branch operation.
- 2026-06-16 locked physical USB investigation recovered the audio stack after
  a stuck Spotify process, installed the HAL candidate safely, and confirmed
  `iRig Stream` plus `Open Audio 8 DJ` visibility. The HAL music soundcheck
  then failed badly (`quality_alignment_score=0.670637`, `analog_snr_db=-0.59`,
  `lag_jumps_gt_2_frames=46`) despite zero underruns and zero panic flags.
  Direct USB tone sweeps also failed; iRig no-playback noise was low, so the
  failure is introduced by Audio 8 USB/playback transport rather than by an
  idle iRig/mixer noise floor. Runtime state after cleanup is safe: HAL
  inactive, lock absent, isolation audit PASS.
- The latest build/tooling correction makes `make usb-play` use the same
  `HAL_CFLAGS` as the HAL bundle and parameterizes
  `OPENA8DJ_OUTPUT_CHECK_OFFSET` with default `8`. This improves diagnostic
  fidelity only; it is not an audio-quality fix.
- 2026-06-16 follow-up comparison found the C++ HAL transport defaults had
  diverged from the latest mainline physical baseline: C++ used ISO5 with
  64/64 capture/playback queues, while mainline `0.3.135` uses ISO64 with 8/8
  queues and output prefetch 64. A locked ISO64 C++ HAL run was then worse than
  the ISO5 failure (`quality_alignment_score=0.051643`, `analog_snr_db=-31.90`,
  `lag_jumps_gt_2_frames=60`). C++ defaults are restored to the less-bad ISO5
  baseline while lifecycle-level divergences are investigated.
- Offline window-trace analysis of the failed physical music run shows
  `lag_jumps_gt_2_frames=24`, local lag from `3` to `-27` frames, and only
  `2.1%` median mid-band residual improvement after per-window lag correction.
  Timing/cadence is real, but the music-quality failure is not explained by
  timebase alone.
- 2026-06-17 operational cleanup found no live OpenA8DJ/HAL/direct USB/capture
  or app playback process to kill; the hardware lock was acquired for the audit
  and released afterward. Evidence:
  `local-analysis/runtime-isolation/kill-open-holders-20260617T002910Z`.
- C++ now exposes `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM`, default `1`, matching
  current behavior while allowing a future locked one-factor test with the
  reset skipped. This is testability infrastructure only, not a quality fix.
- Added offline stream-stats summarization for existing soundcheck snapshots.
  Current latest calibrated run summary:
  `local-analysis/stream-stats/default-minus16-summary.json`, result
  `DIAGNOSTIC_FLAGS`, with `stream_stats_timeouts`,
  `output_write_stats_unobservable`, output read rate about `48009.4` fps, and
  no active-underrun/timeline/panic flag. Across prior runs, ISO64 is a clear
  outlier for capture transaction error ratio, matching its physical rejection.
- Reset-off (`HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0`) was rejected at the HAL
  candidate safety gate before any audio playback/capture: CoreAudio
  enumeration passed, but `coreaudiod=115.1%` and watched audio CPU `130.0%`.
  Recovery unloaded the HAL and post-audit passed with lock absent.
- Build flag hygiene is now stricter: `hal` and `usb-play` depend on a HAL
  flags stamp so changing `HAL_*` build variables cannot silently reuse stale
  binaries for physical evidence.

## Known Baseline Inputs

- Mainline C/Objective-C remains the source for proven USB behavior, HAL surface behavior, physical quality results, and recovery scripts.
- Rust remains a read-only oracle for gates, metrics, analyzers, simulators, routing tests, and benchmark ideas.
- Apple AudioDriverKit is the target shell for the eventual macOS driver extension.

## Open Questions

- Exact USB packet pack/unpack contract to adopt from mainline after archaeology is complete.
- Which Rust gates can be reused verbatim as external oracle commands versus reimplemented as C++ tests.
- DriverKit transport split: AudioDriverKit-only surface plus USBDriverKit transport, or staging through a transport abstraction until entitlement/build constraints are resolved.
- Physical readiness thresholds still need measured C++ evidence before any hardware window request.

## Hardware Readiness Gaps

- Direct Audio 8 DJ CoreAudio presence and input capture have passed, but this
  is not analog sound-quality proof.
- iRig Stream is currently recovered and visible to CoreAudio. If it regresses
  again, first check for the macOS accessory authorization prompt and the kernel
  log phrase `device functionality blocked by transport restrictions`.
- Removing cadence diagnostics from the current HAL variant caused a local
  CoreAudio enumeration hang during a tone attempt. The system was recovered by
  reinstalling the `ISO5/q64/start_byte=4` cadence-diagnostic variant and
  restarting `coreaudiod`. Treat the no-diagnostic build as suspect until
  reproduced with a watchdog.
- Runtime isolation audit now exists at `scripts/runtime-isolation-audit`.
  Current quiescent evidence:
  `local-analysis/runtime-isolation/current.json`, result `PASS`, lock absent,
  mainline services disabled, active HAL absent, no OpenA8DJ process detected.
- DriverKit shell is only an offline lifecycle/model contract; no real dext is
  installed, activated, or currently buildable on this machine.
- Offline tests cover initial surface/routing, Mode 2 packet fidelity,
  synthetic no-leakage, input-profile Mode 2 decode, jitter model, static
  policy, and Release throughput.
- Still missing before physical readiness: real-music quality improvement over
  the mainline baseline, stable lag/jitter accounting, CPU/resource comparison
  against mainline, Traktor-grade DVS analyzer gate on real input, DriverKit
  SDK/dext build, and user listening.
- Completed subagents:
  - `Leibniz`: completed stronger offline timeline/jitter model.
  - `Linnaeus`: completed read-only analysis of existing physical music failure
    evidence.
  - `Lagrange`: completed read-only promotion/readiness gap audit.
- Current runtime state:
  - Hardware lock is absent.
  - The live `git fsmonitor--daemon` that had `/Users/fer/dev/opena8dj` open
    was terminated, and a follow-up `lsof +D /Users/fer/dev/opena8dj` showed no
    remaining handles.
  - `NIHardwareAgent` was terminated; no current `NIHardwareAgent`, `OpenA8DJ`,
    Traktor, ffmpeg, or sox process is detected by the scoped process check.
  - Mainline LaunchAgents `org.opena8dj.midid`,
    `com.fer.opena8dj.autonomous-audio-qa`, and
    `com.fer.opena8dj.safe-replug-watch` are disabled.
  - Active OpenA8DJ HAL bundle is absent. Run
    `scripts/runtime-isolation-audit --expect-hal active` after any authorized
    reinstall/restore and before physical testing.
  - Current inactive-state runtime isolation evidence:
    `local-analysis/runtime-isolation/current.json`, result `PASS`.
- Physical validation must follow `docs/PHYSICAL_TEST_WINDOW_PLAN.md`, use the
  global hardware lock, and avoid default-device/sample-rate/buffer changes
  unless explicitly part of a documented window.

## 2026-06-16 Current Iteration: Lifecycle Parity Candidate

- The HAL/USB transport now has the mainline lifecycle knobs needed for the next
  controlled physical test without changing the current less-bad default
  profile: playback request coalescing, capture-vs-playback queue order, and
  transfer-pool cursor selection are build-time parameters.
- Defaults remain conservative after ISO64 was physically rejected:
  `ISO5`, capture queue `64`, playback queue `64`, output prefetch `256`,
  start byte `4`, check offset `8`, gain `0.50`, output amplitude stats off.
- Build and offline gates passed after the lifecycle changes:
  `make usb-play hal`, `scripts/run-cpp-offline-gates` with Debug `15/15` and
  Release `16/16`.
- Promotion remains blocked by evidence, not opinion. Current evaluator result:
  `FAIL`, `branch_promotion_allowed=false`, because physical music quality,
  runtime CPU, latest physical investigation, and physical Traktor/timecode
  validation still fail.
- Hardware/process status before this iteration: hardware lock absent; no
  OpenA8DJ process detected; only normal `coreaudiod` and `usbaudiod` daemons
  were alive, so no process was killed.

Next technical target:
- Run one locked physical HAL candidate after committing this lifecycle parity
  work. If quality remains bad, prioritize input-decode activation parity and
  capture-paced playback request accounting before another byte-layout sweep.

## 2026-06-16 Current Iteration: Input Decode Activation Parity

- Locked lifecycle-parity physical soundcheck at commit `e0ad0a0` still failed:
  `quality_alignment_score=0.934891`, `analog_snr_db=8.73`,
  `lag_jumps_gt_2_frames=25`, mid-band residual ratio `1.397074`,
  high-band residual ratio `1.352348`, quiet mid-band noise `-31.10 dBFS`,
  OpenA8DJ driver CPU p95 `36.0%`.
- The lifecycle candidate was unloaded after failure; post-unload isolation
  passed with HAL inactive, lock absent, no OpenA8DJ process, and iRig still
  visible.
- C++ now ports mainline-style input decode activation:
  `OpenA8DJUSBSetInputDecodeActive`, HAL `PrepareInputCycle` activation,
  StopIO deactivation, and USB capture decode bypass while no input client is
  active.
- Build and offline gates passed after this change:
  `make usb-play hal`, `scripts/run-cpp-offline-gates` with Debug `15/15` and
  Release `16/16`.
- This is a CPU/work-reduction candidate for output-only playback. It is not a
  readiness claim until a locked iRig run proves lower CPU and better or equal
  analog quality.
- Locked physical retest with active gating enabled was a regression:
  `quality_alignment_score=0.028314`, `analog_snr_db=-28.18`,
  `lag_jumps_gt_2_frames=52`, OpenA8DJ driver CPU p95 `41.8%`,
  CoreAudio p95 `21.8%`. The HAL was unloaded and post-unload isolation passed.
- Therefore `HAL_INPUT_DECODE_ACTIVE_GATING` now defaults to `0`. The API stays
  in the tree as an explicit experiment only; it is not part of the current
  physical candidate.
- Lifecycle experiment `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1` also
  failed physically: `quality_alignment_score=0.674742`,
  `analog_snr_db=-0.41`, `lag_jumps_gt_2_frames=21`, OpenA8DJ driver CPU p95
  `36.0%`, CoreAudio p95 `74.0%`. Keep the default at `0`.
- Current process/lock audit before the next hardware step found no live
  hardware-lock owner, no active HAL bundle, no OpenA8DJ driver process, and no
  mainline QA/soundcheck process to kill. The only `/Users/fer/dev/opena8dj`
  handles were the Codex runtime cwd context, which must remain alive for this
  session.
- Transfer-pool cursor experiment `HAL_TRANSFER_POOL_CURSOR=1` passed offline
  gates but failed the HAL candidate safety gate before soundcheck:
  `coreaudiod` reached `172.2%` CPU while the OpenA8DJ driver itself was `0.1%`.
  The safety script unloaded the HAL and recovery passed. Keep the default at
  `0` and do not test this variant physically until the load-time CoreAudio
  spike is explained.
- Output amplitude stats now default to off (`HAL_OUTPUT_AMPLITUDE_STATS=0`) to
  match mainline/Rust defaults and remove nonessential per-frame diagnostic work
  from the output hot path. Re-enable only for targeted diagnostic runs.
- Mainline-style preopen/stop-ISOC lifecycle code is ported but rejected as a
  default for this C++ HAL after physical regression:
  `quality_alignment_score=0.159859`, `analog_snr_db=-16.87`,
  `lag_jumps_gt_2_frames=59`. Defaults are back to
  `HAL_BACKGROUND_PREOPEN_ON_INIT=0` and `HAL_STOP_ISOC_ON_STOP=0`.
- Fast prefetch clear remains rejected after an isolated physical run regressed
  to `quality_alignment_score=-0.048481`, `analog_snr_db=-32.06`,
  `lag_jumps_gt_2_frames=46`.
- `HAL_OUTPUT_WRITE_STATS` is now restored to `1` by default as a separate
  atomic-only/mainline-parity cleanup. The prior physical rejection changed it
  together with fast prefetch clear, while the new read-only analysis showed
  `HAL_OUTPUT_WRITE_STATS=0` paid a mutex per output write and then lost the
  value when `streamStatsSnapshot` overwrote it from the atomic counter.
- C++ now also exposes mainline-style `outputLateWriteFrames` and
  `outputLateWriteBatches` counters. This is observability for the next locked
  physical run, not a readiness claim.

## 2026-06-16 Current Iteration: Calibrated HAL And Direct USB Diagnostics

- Stale hardware-holder cleanup passed: lock absent, HAL inactive, no
  OpenA8DJ driver process, no soundcheck/capture/playback processes, and
  mainline supervisors disabled. Codex/node cwd handles on the read-only
  mainline path were intentionally left alive because they are not hardware
  holders.
- Calibrated `-16 dB` HAL soundcheck with iRig still failed:
  `quality_alignment_score=0.960076`, `analog_snr_db=2.71`,
  `lag_jumps_gt_2_frames=35`, mid-band residual ratio `1.565287`,
  high-band residual ratio `1.461400`, no capture clipping.
- CPU is still worse than mainline: C++ driver avg/p95 `30.73%/36.70%` in the
  calibrated HAL run, while mainline `0.3.135` documents driver p95 around
  `6.5%`.
- Descartes found that C++ direct `usb-play` had been built with `HAL_CFLAGS`,
  while mainline direct `usb-play` uses plain `CFLAGS`. C++ now has explicit
  `usb-play-plain` and `usb-play-plain-gain05` targets.
- Direct USB plain-CFLAGS physical diagnostics both failed:
  plain CFLAGS clipped badly and scored `quality_alignment_score=0.186400`;
  plain CFLAGS with only `OPENA8DJ_OUTPUT_GAIN=0.50f` still scored
  `quality_alignment_score=0.023502`.
- Added `opena8djcpp_mode2_mainline_layout_parity`, an independent unrolled
  mainline-style byte-layout gate. It passed `132` rows and is now part of the
  offline gates; Debug is `16/16` and Release is `17/17`.
- Current interpretation: the blocker is not only a HAL callback problem, not
  only `HAL_CFLAGS` contamination of direct USB tools, and not only output
  gain. A simple Mode 2 byte-layout mismatch is also less likely after the new
  parity gate. Continue below-HAL transport/cadence/device-state investigation
  before more physical sweeps.

## 2026-06-17 Current Iteration: Hot Stats And Late-Write Observability

- Current lock/process audit after the kill request passed:
  `local-analysis/runtime-isolation/current-after-kill-request.json` shows lock
  absent, HAL inactive, no OpenA8DJ process, and disabled mainline launch
  agents. No process was killed because no real OpenA8DJ/soundcheck/test-hal
  holder was alive; `coreaudiod` and `usbaudiod` were left alone.
- Halley completed a read-only mainline/C++ comparison. Highest-priority
  findings:
  - C++ output write stats default was paying an unobservable mutex path.
  - C++ lacked late-write counters present in mainline.
  - C++ queue depth is still `64/64` while mainline defaults to `8/8`.
  - C++ output prefetch is still `256` while mainline defaults to `64`.
  - C++ hot completion stats had no gate/interval even though mainline does.
- Integrated changes:
  - Added `HAL_HOT_STREAM_STATS` and `HAL_HOT_STREAM_STATS_INTERVAL`.
  - Set `HAL_OUTPUT_WRITE_STATS=1` by default.
  - Added `outputLateWriteFrames` and `outputLateWriteBatches` to HAL/control
    stats.
- Verification:
  - `make usb-play hal` passed.
  - `make HAL_HOT_STREAM_STATS=0 usb-play hal` passed.
  - `make HAL_OUTPUT_WRITE_STATS=0 usb-play hal` passed.
  - `scripts/run-cpp-offline-gates` passed Debug `16/16`, Release `17/17`.
- Next physical candidate remains not ready to promote. It needs a locked
  safety/music run and must beat mainline CPU and physical quality before any
  branch promotion is considered.
- Locked physical test of commit `5e6fab7` passed HAL safety but failed music
  quality:
  `quality_alignment_score=0.962133`, `snr_db=10.24`,
  `click_outliers=29`, `lag_jumps_gt_2_frames=45`,
  `mid_band_residual_ratio=1.443461`,
  `high_band_residual_ratio=1.362932`, quiet mid-band noise `-35.91 dBFS`.
- CPU remains too high: OpenA8DJ driver avg/p95 `31.58%/36.00%`, coreaudiod
  avg/p95 `4.70%/7.00%`.
- The new write-stats path is observable and text stream snapshots showed
  `outputLateWriteFrames=0` / `outputLateWriteBatches=0`, so late writes do not
  explain this failure.
- Post-failure cleanup moved the active HAL to `HAL.disabled`, restarted
  CoreAudio, and runtime isolation passed with HAL inactive, lock absent, and no
  OpenA8DJ driver process.
- Tooling gap fixed after the run: `run-soundcheck` now writes late-write
  counters into `stream-stats-during.tsv`, and `analyze-stream-stats.py` tracks
  them and flags nonzero late writes.
- Standalone `queue8` physical run (`HAL_CAPTURE_QUEUE=8`,
  `HAL_PLAYBACK_QUEUE=8`) also failed:
  `quality_alignment_score=0.964133`, `snr_db=10.22`,
  `lag_jumps_gt_2_frames=39`, `mid_band_residual_ratio=1.422599`,
  `high_band_residual_ratio=1.365050`, quiet mid-band noise `-36.08 dBFS`.
- `queue8` did improve click outliers to `0` and coreaudiod p95 to `3.1%`, but
  OpenA8DJ driver CPU worsened to p95 `37.2%`. Treat it as a clue, not a
  default. The local build was restored to default `64/64`.
- Current next one-factor candidate: `HAL_OUTPUT_PREFETCH_FRAMES=64` with
  default queues restored.
