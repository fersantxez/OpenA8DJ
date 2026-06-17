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
  evidence: `decode_into_mib_s=588.188`, `decode_into_output_overflows=0`,
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
  `5` audited scripts, `0` missing requirements.
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
- Transfer-pool behavior now has both runtime observability and an offline C++
  model gate. Current offline evidence:
  `local-analysis/cpp-offline/transfer-pool-model.json`, result `PASS`, `6`
  rows, `0` failures, with `capture_pool_leak_rejected` and
  `playback_pool_leak_rejected` proving fallback-allocation scenarios are
  rejected before hardware testing.
- The HAL now has aggregate transfer-ledger instrumentation for the next
  physical diagnostic run. It records capture/playback queue and completion
  events into a fixed POD ring and exports aggregate counters through
  `opena8dj-control stream-stats` and `stream-stats-during.tsv`. This is
  observability only; CPU/readiness claims must account for instrumentation
  overhead.
- 2026-06-17 transfer-ledger physical diagnostics narrowed the current
  blocker. The default `a51ee29` music run still failed
  (`quality_alignment_score=0.964608`, SNR `10.48 dB`, lag jumps `36`) with no
  fallback allocations, no active underruns, and no queue/complete deltas
  explaining the failure. The diagnostic build then captured written,
  consumed, and packed output bytes for another failing run; those internal
  paths were perfect against the reference, and packed output USB decoded as
  Mode 2 `big/start4/check8` with `0` check errors, `0` panic flags, Pair A
  gain `0.5`, and no B/C/D leakage. The physical iRig capture still failed
  (`quality_alignment_score=0.963726`, SNR `10.51 dB`, lag jumps `40`).
- `HAL_OUTPUT_NATIVE=1` was physically rejected and must not be used as a
  candidate/default: quality alignment `0.003598`, SNR `-63.94 dB`, quiet noise
  `-8.87 dBFS`, and `520014` clipped capture frames. The active native HAL was
  parked under lock at
  `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver.disabled-20260617T035921Z-cpp-native-reject`
  after CoreAudio respawned the process.
- Current clean runtime state after the native rejection cleanup:
  `local-analysis/runtime-isolation/final-after-disable-native-reject.json`,
  result `PASS`, HAL inactive, lock absent, no OpenA8DJ process. Audio stack
  health also passed with watched audio CPU `0.0%`.
- The current strongest technical conclusion is negative but useful: the
  product failure is not explained by CoreAudio-to-HAL written frames, HAL
  consumed frames, basic Pair A routing, inactive deck leakage, start byte,
  check offset, big-endian Mode 2 packing, or USB check/panic flags in the
  bytes produced by the driver. The next hypothesis must move after packed
  output bytes: actual USB/device scheduling/state, hardware interpretation,
  analog route/reference path, or physical capture-route mismatch.
- 2026-06-17 bounded transfer-ledger physical run adds clean transaction
  evidence to the same negative conclusion. `transfer-ledger-after.tsv` from
  `local-analysis/soundcheck/20260617-bounded-full-ledger-irig-pairA-12s-cpp-hal`
  covers `91,647` contiguous entries, `overwritten=0`, with no sequence gaps,
  no playback failed/short transactions, no completion status errors, no
  playback first-frame regressions, and no stream-stat active underruns during
  the run. Product quality still fails: quality alignment `0.960392`, SNR
  `10.37 dB`, and `33` lag jumps. This blocks readiness and promotion.
- Full transfer-ledger recording is now diagnostic-only by default. Product
  builds use `HAL_TRANSFER_LEDGER=0`; physical ledger diagnosis must explicitly
  build with `HAL_TRANSFER_LEDGER=1`. CPU claims from ledger-enabled runs are
  invalid for product readiness.

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
- Operational debt:
  - Post-reboot Codex auto-resume/login recovery did not work reliably and
    remains a separate infrastructure bug to fix later.
  - During the 2026-06-17 physical diagnostic iteration, an accidental
    untracked file was briefly created in `/Users/fer/dev/opena8dj` and then
    immediately removed. Follow-up checks confirmed
    `/Users/fer/dev/opena8dj/scripts/analyze-channel-transients.py` is absent.

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
- Standalone `HAL_OUTPUT_PREFETCH_FRAMES=64` also failed:
  `quality_alignment_score=0.956371`, `snr_db=10.40`,
  `click_outliers=4`, `lag_jumps_gt_2_frames=48`,
  `mid_band_residual_ratio=1.431220`,
  `high_band_residual_ratio=1.365281`, quiet mid-band noise `-35.98 dBFS`,
  OpenA8DJ driver p95 `39.5%`.
- The local build was restored to default `HAL_OUTPUT_PREFETCH_FRAMES=256`.
- Current interpretation: parity knobs are not enough. The repeated pattern is
  no software underruns/late writes/timeline resets, stable output read rate,
  persistent capture transaction errors around `2.273` per capture transfer,
  high driver CPU, and bad analog residual/lag. Next work should inspect USB
  transaction request/complete semantics and capture error classification.
- User-requested stale-holder cleanup was rerun live under lock. Evidence:
  `local-analysis/audio-stack-guard/20260616-kill-open-holders` and
  `local-analysis/runtime-isolation/current-after-kill-request-live.json`.
  Final state: lock absent, HAL inactive, no OpenA8DJ/mainline/soundcheck
  holder processes, forbidden mainline LaunchAgents disabled. The guard did not
  need to kill a real OpenA8DJ holder.
- Capture detail counters changed the interpretation of the stable aggregate
  capture error ratio. Recent detailed iRig runs pass ISO-slot invariants:
  status failures `0`, short transfers `0`, other-size transfers `0`, useful
  transactions `352` bytes each, and
  `expected + zero_complete == 5 * captureTransfers`. The aggregate ratio is
  empty ISO slots, not proof of USB status failure.
- One rejected variant remains diagnostically useful: `lifecycle-preopen`
  violates `classified_transactions == total_iso_slots`, so preopen lifecycle
  behavior stays rejected.
- Current blocker after this correction: physical analog residual/lag and
  driver CPU, not the decomposed zero-complete ISO slots.
- Promotion evaluator after commit `1c574cc` remains FAIL:
  `local-analysis/promotion-readiness-after-capture-invariants.json`.
  Blocking gates are `physical_music_quality`, `runtime_cpu_beats_mainline`,
  `latest_physical_investigation`, and `traktor_timecode_physical`.
- Integrated a low-risk HAL output packing CPU candidate: C++ now uses a
  mainline-style unrolled 16-byte Mode 2 `fillPlaybackBytes` fast path when
  `OPENA8DJ_OUTPUT_CHECK_OFFSET=8`, with the old generic path retained for
  diagnostic offsets. Offline gates remain PASS, but this is not a readiness
  claim until locked iRig quality and CPU improve.
- Locked iRig evidence rejected that candidate as a default. The unrolled
  output pack path is now opt-in only (`HAL_UNROLLED_OUTPUT_PACK=1`), default
  `0`. Evidence:
  `local-analysis/soundcheck/20260616-unrolled-pack-irig-pairA-16s-cpp-hal`
  failed with `quality_alignment_score=0.131043`, SNR `-18.43 dB`, and
  `lag_jumps_gt_2_frames=47`; driver CPU p95 stayed around `35.6%`.
  Post-failure unload and isolation passed.
- Promotion evaluator after unrolled-pack rejection remains FAIL:
  `local-analysis/promotion-readiness-after-unrolled-rejection.json`.
  The latest selected physical music and CPU evidence are both failing, so
  branch promotion and hardware readiness claims remain blocked.
- Default-restored control run with the same music file confirms the severe
  regression was specific to the opt-in unrolled candidate. The default still
  fails product gates: `quality_alignment_score=0.964049`, SNR `10.44 dB`,
  `lag_jumps_gt_2_frames=40`, driver p95 `38.5%`.
  Evidence:
  `local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal`.
- Window-trace analysis now makes the physical blocker narrower:
  `local-analysis/soundcheck-window-trace/default-after-unrolled-v2.json`,
  `hotstats-write-late-v2.json`, and `queue8-v2.json` show local lag correction
  improves median mid-band residual by only `0.6%` to `4.0%`. Median corrected
  correlation is around `0.969-0.971`, but corrected mid-band residual remains
  about `1.43x`. The main blocker is therefore not a simple alignment/drift
  issue; it is persistent coloration/distortion/mixed signal plus high driver
  CPU.
- Linear matrix analysis over the existing music captures is now reproducible:
  `local-analysis/soundcheck-linear-matrix/recent-failed-physical-music.json`.
  It shows the music source L/R channels are too correlated
  (`input_lr_correlation=0.985848`, condition number `140.3`) to prove
  crosstalk. Default-like runs still leave large residual after a 2x2 fit
  (`0.30x` to `0.50x` residual/predicted), so the next physical evidence must
  be a decorrelated channel-matrix capture.
- Added `scripts/run-channel-matrix-gate` and `make channel-matrix-prepare`.
  The default mode is prepare-only and does not touch audio hardware. The
  generated fixture smoke has L/R correlation about `0.00056`, suitable for a
  future lock-gated iRig Pair A matrix/crosstalk test.
- Locked Pair A decorrelated matrix was executed through iRig Stream:
  `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix`.
  Tone-domain analysis passes with no clipping and leakage below `-49 dB`
  (`left_to_right=-59.48 dB`, `right_to_left=-49.67 dB`,
  `max_wrong_source=-51.27 dB`). This reduces the likelihood that gross Pair A
  L/R crosstalk is causing the music failure. It does not clear readiness
  because physical music quality and runtime CPU remain failing, and physical
  Traktor/timecode remains unvalidated.
- Atomic stream-stats accumulators now exist as an experimental opt-in flag,
  but are rejected as the default. Locked iRig physical evidence for commit
  `a11012f` showed driver CPU p95 `37.6%`, still far above the mainline
  threshold `6.5%`, with music quality still failing and click outliers
  increased to `106`. The default is back to
  `HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=0`.
- Promotion evaluator after the atomic stream-stats offline check remains FAIL:
  `local-analysis/promotion-readiness-after-atomic-stream-stats.json`.
  Blocking gates remain `physical_music_quality`,
  `runtime_cpu_beats_mainline`, `latest_physical_investigation`, and
  `traktor_timecode_physical`.
- Cleanup after the rejected physical run required a manual minimal HAL unload
  because the guard health check passed and therefore did not enter recovery.
  Final isolation PASS:
  `local-analysis/runtime-isolation/after-atomic-stream-stats-manual-unload.json`.
- `HAL_FAST_OUTPUT_PREFETCH_CLEAR=1` was physically rejected:
  `local-analysis/soundcheck/20260617-fast-prefetch-clear-a1c8b50-irig-pairA-16s-cpp-hal`.
  It lowered driver CPU p95 to `36.8%` but worsened quality alignment to
  `0.954699` and SNR to about `9.53 dB`.
- `HAL_HOT_STREAM_STATS_INTERVAL=16` is now the preferred default CPU cleanup.
  It does not change audio bytes or USB scheduling, improved driver CPU p95 to
  `35.7%` in the locked Pair A/iRig run, and kept the broad failing music
  signature similar to default. It is not readiness: physical music quality and
  mainline CPU gates still fail.
- A local analysis venv was created under `.venv` using
  `requirements-analysis.txt` (`numpy==2.0.2`, `scipy==1.13.1`) for precise
  offline FFT/CSD/Welch diagnostics.
- Offline tone-response and LTI diagnostics do not support a simple EQ/linear
  transfer explanation for the music failures:
  - `local-analysis/tone-response-compensation/recent-music-runs.json`
  - `local-analysis/lti-transfer-quality/recent-music-runs.json`
  The LTI run shows very low mid/high coherence and negative SNR deltas after
  transfer-function prediction. Next quality work should isolate non-linear,
  time-varying, output-format, or reference-route mismatch causes.
- Added a stronger offline failure-mode classifier:
  `scripts/analyze-soundcheck-failure-modes.py`.
  Evidence:
  `local-analysis/soundcheck-failure-modes/recent-music-runs.json` and
  `local-analysis/soundcheck-failure-modes/recent-music-runs-local128.json`.
  It rejects simple L/R swap/polarity/static matrix and simple memoryless
  cubic non-linearity as sufficient explanations. With local lag search,
  recent drift is small (`~ -28` to `-34 ppm`, except rejected `queue8`),
  while scalar/matrix/cubic SNR stays around `9-10 dB`. The blocker is still
  a physical reference-route/runtime discontinuity/format issue, not an EQ
  calibration.
- Physical stats-off candidate was tested and rejected:
  `HAL_OUTPUT_WRITE_STATS=0 HAL_HOT_STREAM_STATS=0`.
  Evidence:
  `local-analysis/physical-stats-off/20260617-a1c8b50-stats-off/hal-candidate-safety`
  and
  `local-analysis/soundcheck/20260617-stats-off-a1c8b50-irig-pairA-16s-cpp-hal`.
  It passed install safety and offline gates, but physical music still failed
  (`quality_alignment_score=0.960287`, SNR `10.48 dB`,
  `lag_jumps_gt_2_frames=37`, mid/high residual
  `1.424930/1.362660`) and driver CPU p95 worsened to `36.8%`. Therefore it is
  not a default and not a useful CPU optimization.
- Final post-run isolation after stats-off PASS:
  `local-analysis/runtime-isolation/final-after-stats-off.json`. HAL inactive,
  lock absent, and no OpenA8DJ/mainline holder processes were detected.
- Physical sparse output-cycle clear candidate was tested and rejected:
  `HAL_OUTPUT_SPARSE_CYCLE_CLEAR=1`. Evidence:
  `local-analysis/physical-sparse-cycle-clear/20260617-a1c8b50/hal-candidate-safety`
  and
  `local-analysis/soundcheck/20260617-sparse-cycle-clear-a1c8b50-irig-pairA-16s-cpp-hal`.
  It passed install safety, but physical music still failed
  (`quality_alignment_score=0.963647`, SNR `10.48 dB`,
  `lag_jumps_gt_2_frames=33`, mid/high residual `1.408180/1.364597`) and
  driver CPU p95 worsened to `38.3%`. The code experiment was removed rather
  than kept as a disabled flag.
- Final post-run isolation after sparse clear PASS:
  `local-analysis/runtime-isolation/final-after-sparse-cycle-clear.json`. HAL
  inactive, lock absent, and no OpenA8DJ/mainline holder processes were
  detected.

## 2026-06-17 Current Iteration: HAL Hot-Path Lock Reduction Rejected Physically

- Commit `056d29b` reduces mutex pressure without changing USB cadence or
  payload bytes: output timeline start-frame resolution now happens under the
  existing write lock, and input stats merge once per capture transfer instead
  of locking for every stream sample.
- Build and offline gates passed after the change:
  `make -B hal usb-play`, `make -B hal HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1`, and
  `scripts/run-cpp-offline-gates`.
- Locked HAL candidate safety passed and left the HAL loaded for one controlled
  Pair A/iRig music soundcheck.
- The physical soundcheck still failed:
  `quality_alignment_score=0.964558`, `analog_snr_db=10.41`,
  `lag_jumps_gt_2_frames=43`, mid/high residual ratios
  `1.430949/1.358723`, quiet mid-band noise `-35.90 dBFS`, no clipping, and
  no channel click outliers.
- CPU did not improve enough to matter: OpenA8DJ driver p95 was `37.5%`,
  while the mainline C reference is about `6.5%`. `coreaudiod` p95 reached
  `60.3%` and total watched audio/UI p95 reached `121.1%`.
- Recovery required a manual HAL park because `audio-stack-guard --recover
  --unload-opena8dj` did not fully unload the active candidate. The HAL was
  moved to
  `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver.disabled-20260617T031430Z`,
  `coreaudiod` was restarted, and final isolation passed at
  `local-analysis/runtime-isolation/after-hotpath-manual-unload.json`.
- Current promotion status remains `FAIL` with
  `branch_promotion_allowed=false`. C++ must not be promoted to `main`, C must
  not be moved to `Legacy`, and this candidate must not be described as better
  than mainline.
- Next work should stop spending physical windows on superficial stats/lock
  cleanup as standalone candidates. The live blocker is still below-HAL
  transport/cadence/device-state behavior or a reference-route/format mismatch
  that the current offline byte-layout gates do not expose.
- Added `scripts/analyze-runtime-discontinuities.py` to correlate existing
  soundcheck WAV windows against CPU and stream-stats telemetry without
  touching hardware. Evidence:
  `local-analysis/runtime-discontinuities/recent-music-runs.json`.
  The diagnostic found no strong CPU or stream-counter correlation with
  residual/lag/SNR in four recent Pair A/iRig music runs, while window lag
  jumps and ~10 dB scalar SNR persist. This weakens blind CPU-tuning as the
  next quality path and raises priority for controlled reference-route,
  physical path, output format/phase, or uncounted runtime-discontinuity tests.
- Added `--no-monitor-stream-stats` to `scripts/run-soundcheck` for
  lower-perturbation CPU A/B runs. This is diagnostic-only; readiness still
  requires stream-stat evidence.
- `HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=1` was tested
  after Hume's read-only audit suggested transaction count as the likely CPU
  driver. It passed offline gates but failed HAL safety on load with
  `coreaudiod=86.8%`, `mediaremoted=57.5%`, and total watched CPU `145.3%`.
  The default build passed safety immediately afterward, so this candidate is
  rejected before soundcheck.
- Default monitor-free soundcheck was attempted to separate CPU from
  observability overhead, but the capture was decorrelated
  (`quality_alignment_score=0.097964`, SNR `-29.18 dB`) and driver CPU p95
  was worse (`39.0%`). A normal default confirmation run immediately returned
  to the known failing aligned signature (`quality_alignment_score=0.963713`,
  SNR `10.57 dB`, lag jumps `46`, driver p95 `36.9%`). Monitor-free evidence
  is not product evidence yet.
- `HAL_PLAYBACK_COALESCE_TRANSFERS=2` without pool cursor passed HAL safety and
  reduced driver CPU p95 to `28.5%`, confirming transaction frequency matters.
  It is still rejected because physical music quality regressed sharply:
  `quality_alignment_score=0.898854`, SNR `5.85 dB`, mid/high residual
  `2.563432/1.666568`, and lag jumps `45`. Coalescing is therefore not a
  direct path to readiness without a deeper cadence/timeline redesign.
- Cadence diagnostics had a threshold bug for coalesced playback: the expected
  transfer ticks were based on capture/default transfer size even when playback
  coalesced multiple transfers. This is now fixed internally for outlier
  accounting. It is observability only and does not change audio behavior.
- Offline jitter modeling now also includes playback burst cadence rows. The
  gate records coalesce2 as an expected unsafe burst profile: completion count
  drops to `0.5x`, but playback completion spacing doubles from `64` to `128`
  frames (`completion_gap_ratio=2.0`). That matches the physical coalesce2
  rejection and prevents CPU-only wins from bypassing cadence/quality gates.
- HAL hot-path hygiene now removes two avoidable lock costs without changing
  USB cadence or payload: output timeline start-frame resolution is folded into
  the single timeline write lock, and input stats are aggregated locally per
  capture transfer before one locked merge. Offline gates pass, but this is not
  a physical performance/readiness claim until locked A/B evidence exists.
- Carver's read-only audit converged on the same next step: do not spend
  another physical window on a standalone micro-optimization. Add transport
  observability first, especially around OUT transfer queue/completion cadence,
  implicit scheduling, in-flight state, and whether the transfer pool ever
  falls back to allocating a new transfer while streaming.
- C++ now records `captureTransferPoolFallbackAllocations` and
  `playbackTransferPoolFallbackAllocations` in stream stats. These counters
  are expected to stay at `0` in any candidate run. A nonzero value means the
  preallocated pool was exhausted and the HAL allocated transfer objects in the
  streaming path, which is a CPU/latency blocker before any quality claim.
- Commit `bff59cc` added playback payload guard instrumentation. A locked
  physical Pair A/iRig run produced about `1600` guard checks/s and `0`
  mismatches while music still failed (`quality_alignment_score=0.958179`,
  SNR `10.29 dB`, lag jumps `22`). Queue-to-completion payload mutation is
  therefore not the current dominant blocker.
- Explicit scheduling with USB clock anchor is rejected for this HAL path:
  alignment `0.025535`, SNR `-33.82 dB`, `14` click outliers, playback
  completion rate about `23/s`, and timeline resets `35`.
- Fixed OUT pacing is rejected: alignment `-0.153805`, SNR `-28.47 dB`, lag
  jumps `40`, and mid/high residual ratios `39.366597/25.403255`.
- A fresh default 1 kHz physical tone is mixed evidence, not readiness:
  `sideband_ratio=0.006623`, strongest sideband `1060 Hz` at `-42.74 dB`,
  residual ratio `0.456797`, click outliers `40`. This beats the historical
  final `0.3.24` sideband floor but does not beat the best mainline floor
  `0.004942` and fails click/strongest-sideband targets.
- Current model diagnostics on the new evidence reject simple explanations:
  static L/R mix, polarity, tone-response compensation, LTI transfer fitting,
  and simple memoryless non-linearity do not explain the aligned default
  music failure. The current signature is timebase/alignment instability or a
  physical/reference mismatch not represented by the offline byte oracle.
- Operational isolation note: a second accidental `apply_patch` hit
  `/Users/fer/dev/opena8dj/scripts/audio-stack-guard` while adding
  `--force-unload-opena8dj`. Only the newly added `force-unload` lines were
  removed immediately. A follow-up grep over the mainline diff confirmed no
  `force-unload` lines remain. The mainline still contains pre-existing
  unrelated local modifications and was not reset.
- Added bounded transaction-level transfer ledger export in the C++ HAL/control
  path:
  - `build/opena8dj-control transfer-ledger [count]` returns the latest stable
    ledger entries as TSV.
  - `scripts/run-soundcheck --stream-stats-snapshots` now captures
    `transfer-ledger-after.tsv`.
  - `scripts/analyze-transfer-ledger.py` summarizes that TSV to JSON and fails
    on sequence gaps, completion status errors, failed/short transactions,
    active underrun frames, playback first-frame regressions, and obvious
    playback queue/complete imbalance.
  - The streaming path still only writes into the preallocated circular ledger;
    IPC/file output happens only on explicit control request after/during a
    run. This is diagnostic infrastructure, not a sound-quality improvement
    claim.
- Latest verification after the transfer-ledger export:
  - `make -B hal build/opena8dj-control`: PASS.
  - `python3 -m py_compile scripts/run-soundcheck`: PASS.
  - `scripts/run-cpp-offline-gates`: PASS, debug `17/17`, release `18/18`.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out
    local-analysis/runtime-isolation/after-transfer-ledger-run-soundcheck-hook.json`:
    PASS, HAL inactive, lock absent, no OpenA8DJ HAL process.

## 2026-06-17 Runtime CPU And Stream-Usage Update

- Product ledger-off physical run proves the diagnostic transfer ledger is not
  the CPU root cause. With `HAL_TRANSFER_LEDGER=0`, Pair A/iRig music still
  fails: `quality_alignment_score=0.971414`, SNR `10.52 dB`, `27` lag jumps,
  and driver p95 about `36.9%`.
- Stream-usage support was enabled and `audio-wav-play` now declares its
  selected output pair through `IOProcStreamUsage`. This is correct HAL
  behavior, but it only moved music alignment to `0.971648` and did not clear
  CPU or residual gates.
- `sudo -n sample` during playback-only produced the first useful symbol
  profile. The active CPU is dominated by the USB dispatch queue and IOUSBHost
  async enqueue work from capture/playback completions, not by routing, sample
  conversion, transfer ledger, or generic DSP.
- Already rejected knobs remain rejected: playback coalesce2 lowers CPU but
  damages physical quality; input-decode active gating regressed badly; pool
  cursor failed HAL safety. The earlier ISO64/q8 rejection has been superseded
  by the 2026-06-17 retest with stream usage and StopIO shutdown.
- Current decision pressure is a transport/hot-path redesign that preserves
  fine playback cadence while reducing IOUSBHost/Objective-C enqueue cost.
  CPU-only wins are not acceptable without physical music quality proof.

## 2026-06-17 ISO64/q8 And Physical Baseline Update

- Mainline `0.3.135` was measured from the read-only artifact
  `/Users/fer/dev/opena8dj/build/OpenA8DJ.driver` without writing to the
  mainline worktree. A short-wait safety attempt failed due transient
  CoreAudio/mediaremoted CPU; a 45 s stabilization attempt passed.
- Current physical iRig route is not quality-valid for promotion. Mainline and
  C++ ISO64/q8 both failed the same Pair A music gate with very low alignment
  and SNR:
  - mainline: `quality_alignment_score=0.680798`, SNR `-0.83 dB`,
    `39` lag jumps;
  - C++ ISO64/q8 StopIO: `quality_alignment_score=0.686712`,
    SNR `-0.84 dB`, `35` lag jumps.
- CPU comparison is still valid enough to guide performance work:
  - prior C++ ISO5/queue64 stream-usage run: driver p95 `37.2%`;
  - C++ ISO64/q8 StopIO: driver p95 `9.8%`;
  - mainline `0.3.135`: driver p95 `6.0%`.
- C++ default now adopts mainline geometry:
  `HAL_ISO_FRAMES=64`, `HAL_CAPTURE_QUEUE=8`, `HAL_PLAYBACK_QUEUE=8`,
  `HAL_OUTPUT_PREFETCH_FRAMES=64`.
- C++ default now sets `HAL_STOP_ISOC_ON_STOP=1`; this fixed the final
  post-playback state from `streaming=yes` with active underruns to
  `streaming=no`, `outputUnderruns=0`, and `outputActiveUnderruns=0`.
- The ported completion-reuse/strict-pool/legacy-slot/QoS/fast-ISO switches
  remain disabled by default because the combined physical run did not improve
  CPU and left post-run active-underrun evidence.
- Promotion remains forbidden. Current blockers:
  physical music quality route fails for both mainline and C++, C++ CPU still
  does not beat mainline, physical Traktor/timecode vinyl validation is absent,
  and the A/B/C/D physical matrix is absent.

## 2026-06-17 Input Decode Control Update

- C++ now mirrors the mainline input decode control policy:
  input decode is off by default for playback/output-only use and is enabled
  explicitly by timecode vinyl, CD-line, and phono profiles.
- Offline gates remain PASS after the change:
  Debug `17/17`, Release `18/18`, with `dvs_packet_input_decode` reporting
  `playback_decode_off=PASS`.
- The comparable physical Pair A/iRig dense run confirms a driver CPU
  improvement:
  - mainline `0.3.135`: driver p95 `6.0%`, coreaudiod p95 `8.0%`;
  - previous C++ ISO64/q8 StopIO: driver p95 `9.8%`, coreaudiod p95 `11.5%`;
  - C++ input-decode-off with stereo iRig capture `1,2`: driver p95 `6.3%`,
    coreaudiod p95 `43.2%`.
- The change does not solve sound quality:
  `quality_alignment_score=0.680121`, SNR `-0.83 dB`,
  `lag_jumps_gt_2_frames=42`. This is essentially the same degraded iRig route
  class as the current mainline/C++ ISO64 runs, not audiophile evidence.
- Current state after testing: HAL inactive, hardware lock absent, audio stack
  health PASS.
- Incident to fix in process:
  one manual preflight used the wrong lock helper function names and proceeded
  to enumerate/control without an effective lock. One initial soundcheck also
  used `--capture-channels 2`, causing `audio-record` to duplicate iRig channel
  2 as `channels=2,2`; that run is superseded by the corrected `1,2` evidence.
  Future manual hardware commands must use `opena8dj_acquire_hardware_lock` or,
  preferably, scripts with built-in lock handling.
- Current blockers before any branch promotion or hardware readiness claim:
  physical music quality PASS, coreaudiod CPU below mainline/threshold,
  physical Traktor/timecode vinyl validation, physical A/B/C/D matrix, and a
  resolved explanation for the degraded iRig route.

## 2026-06-17 HAL Input I/O Diagnostic

- `HAL_INPUT_IO` is now an explicit Makefile knob for the existing HAL macro
  `OPENA8DJ_ENABLE_INPUT_IO`; default remains `1`.
- The `HAL_INPUT_IO=0` diagnostic build is rejected for physical testing:
  `test-hal-candidate-safety` failed with `required_device_missing`.
- Evidence showed the OpenA8DJ HAL process existed, but CoreAudio did not
  enumerate UID `org.opena8dj.Audio8DJ`. That means the no-input HAL variant
  cannot be used to measure product CPU or quality.
- Recovery after the failed safety run completed successfully: HAL inactive,
  lock absent, audio stack PASS.
- The next CPU investigation should not remove HAL input I/O wholesale. It
  needs a runtime/control-plane approach that preserves device enumeration,
  8-input surface, and DVS/timecode readiness.
- Latest same-route Pair A channel-matrix evidence rejects both the current
  C++ HAL and the read-only mainline `0.3.135` artifact for physical routing
  proof. C++ is worse than mainline on max wrong-source leakage:
  `-35.36 dB` versus mainline `-42.58 dB` against the `-45 dB` threshold.
  Evidence:
  `local-analysis/channel-matrix/20260617-inputdecode-default-pairA-chmatrix`
  and `local-analysis/channel-matrix/20260617-mainline-pairA-chmatrix`.
  This blocks any claim that C++ routing, physical functionality, or sound
  quality is better than mainline.
- The next physical A/B must control `IOProcStreamUsage`. The C++ harness now
  supports `--no-output-stream-usage` in both channel-matrix and soundcheck
  scripts. This is needed because C++ exposes stream usage by default, while
  the read-only mainline `0.3.135` artifact likely rejects the property. The
  control does not change driver behavior or readiness; it makes the next
  measurement more precise.

## 2026-06-17 Stream Usage And Level-Control Findings

- Current commit anchor before this evidence: `dfa6575`.
- Hardware state verified before testing:
  `iRig Stream` was visible in CoreAudio as a 2-channel USB capture device, and
  `Audio 8 DJ` was visible in USB with Native Instruments vendor/product data.
  Audio 8 DJ was not visible in CoreAudio until the C++ HAL was installed.
- Default C++ with playback harness stream usage disabled:
  Pair A max wrong-source leakage improved from `-35.36 dB` to `-39.72 dB`.
  This proves stream usage was an uncontrolled measurement factor, but the run
  still failed the `-45 dB` threshold and remained worse than mainline
  `-42.58 dB`.
- A C++ build with mainline-like defaults
  (`HAL_STREAM_USAGE=0`, `HAL_FAST_OUTPUT_PREFETCH_CLEAR=1`,
  `HAL_BACKGROUND_PREOPEN_ON_INIT=1`, hot stats interval `1`) recovered
  mainline-like output level in the Pair A matrix. It still failed routing:
  max wrong-source leakage `-40.57 dB`, left-to-right `-40.57 dB`,
  right-to-left `-40.09 dB`, no clipping.
- The same mainline-config C++ build failed real-music quality:
  `quality_alignment_score=0.678827`, SNR `-0.83 dB`,
  mid residual ratio `2.536563`, high residual ratio `1.779982`,
  `lag_jumps_gt_2_frames=42`, no clipping.
- Stream stats for that music run showed no output underruns, active underruns,
  elastic drops, timeline resets, or late writes. The remaining defect is not
  explained by current underrun/reset counters or by simple output level.
- Final state after cleanup:
  `local-analysis/runtime-isolation/post-parity-soundcheck-unload-final.json`
  is PASS with HAL inactive, lock absent, and no OpenA8DJ process.
- Conclusion:
  C++ remains blocked from readiness, promotion, and any claim of better
  physical quality/functionality/performance than mainline. The next technical
  work must isolate the residual path below current counters: USB/device
  scheduling, hidden packet/cadence interpretation, analog/capture topology, or
  a missing physical control/state difference.

## 2026-06-17 Direct USB Isolation Update

- Direct USB playback with `opena8dj-usb-play-plain-gain05` bypassed HAL
  publication and still failed the Pair A matrix:
  max wrong-source leakage `-44.78 dB`, R->L `-29.97 dB`, no clipping.
- That run was not level-balanced: left expected max `0.09584`, right expected
  max `0.01005`. It is not proof of routing quality even though L->R was near
  the threshold.
- Direct USB playback with the current HAL-flag build was worse:
  max wrong-source leakage `-13.19 dB` and both expected channel levels near
  the minimum threshold.
- Final isolation after both runs PASS: HAL inactive and lock absent.
- Implication:
  HAL/CoreAudio alone is not a sufficient root cause. The remaining work must
  inspect direct engine audio params/control state, selected-pair semantics,
  USB packet cadence as interpreted by the device, and the physical analog
  route. The current direct USB tools are diagnostics, not readiness evidence.

## 2026-06-17 Selected-Pair Direct USB Update

- `src/tools/opena8dj-usb-play.m` now supports selected output pair
  `A|B|C|D|all` and optional `lead_frames`. Default behavior remains `all`
  with zero lead.
- Selected Pair A direct USB with `opena8dj-usb-play-plain-gain05` still
  failed: max wrong-source leakage `-35.28 dB`, R->L `-18.05 dB`, no clipping.
- Selected Pair A with `8192` lead frames improved L->R to `-46.82 dB`, but
  right expected level dropped below threshold (`0.00366`) and R->L was still
  `-16.05 dB`.
- Conclusion:
  selected-pair support improved the diagnostic shape but did not create a
  valid direct bypass oracle. The next direct-engine task is not more physical
  repetition; it is explicit logging/modeling of `AUDIO_PARAMS`, output stream
  byte cadence, and device control state before/during direct playback.

## 2026-06-17 Current Product Truth After ISO Sweep

- Worktree and branch remain isolated:
  `/Users/fer/dev/audio8djcpp` on `driverkit/cpp-redesign`.
- Mainline `/Users/fer/dev/opena8dj` and Rust
  `/Users/fer/dev/audio8djrust` remain read-only from this C++ effort.
- Offline gates are green:
  Debug `17/17`, Release `18/18`.
- Direct USB selected-Pair-A sweep:
  - ISO8/q8 PASS, max wrong-source leakage about `-53.55 dB`.
  - ISO10 PASS, max wrong-source leakage about `-54.23 dB`.
  - ISO12 PASS, max wrong-source leakage about `-50.44 dB`.
  - ISO14 PASS, max wrong-source leakage about `-50.00 dB`.
  - ISO16 FAIL, max wrong-source leakage about `-44.02 dB`.
- HAL product Pair A matrix:
  - ISO8/q8 PASS, max wrong-source leakage about `-52 dB`.
  - ISO10/q8 PASS, max wrong-source leakage `-52.30 dB`.
- Real-music product quality still fails:
  - ISO8/q8: alignment `0.964724`, SNR `10.00 dB`,
    mid/high residual `1.432051/1.356290`, `29` lag jumps.
  - ISO10/q8: alignment `0.969379`, SNR `10.18 dB`,
    mid/high residual `1.514509/1.396638`, `35` lag jumps.
- Runtime CPU still fails mainline:
  - ISO8/q8 driver p95 `23.1%`.
  - ISO10/q8 driver p95 `19.6%`.
  - Mainline budget remains around driver p95 `<= 6.5%`.
- Current default remains ISO8/q8 because it is the stronger quality candidate
  than ISO10/q8 on real music, even though ISO10/q8 reduces driver CPU.
- Product state:
  - Not ready for hardware readiness claim.
  - Not ready for branch promotion.
  - Not proven better than mainline.
  - Physical Traktor/timecode vinyl remains unvalidated.
  - Full A/B/C/D physical routing remains unvalidated.
- Final runtime state after the latest locked gate:
  HAL inactive, hardware lock absent, no OpenA8DJ process expected.

## 2026-06-17 Current Product Truth After Input-Decode-Gated Probe

- Worktree and branch remain isolated:
  `/Users/fer/dev/audio8djcpp` on `driverkit/cpp-redesign`.
- Mainline `/Users/fer/dev/opena8dj` and Rust
  `/Users/fer/dev/audio8djrust` remain read-only from this C++ effort.
- `HAL_INPUT_DECODE_ACTIVE_GATING=1` is now the default and output-only
  playback probes disable input stream usage.
- This is a harness/control-plane correctness fix, not a product-quality win.
- Offline gates remain green after the change:
  Debug `17/17`, Release `18/18`.
- Latest locked physical soundcheck:
  `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal`.
- Result remains FAIL:
  - `quality_alignment_score=0.959187`.
  - SNR `10.14 dB`.
  - mid/high residual ratios `1.467121/1.368783`.
  - quiet mid noise `-35.11 dBFS`.
  - `lag_jumps_gt_2_frames=30`.
  - driver p95 `24.2%`.
  - `coreaudiod` p95 `21.9%`.
- Stream counters after the run did not show output underruns, active
  underruns, elastic drops/replays, timeline resets, late writes, or playback
  errors. The audible/metric failure is below the current gross underrun
  counters.
- Offline analysis of the captured run points to
  `timebase_or_alignment_instability`:
  drift estimate `-180.6 ppm`, lag span `1645` frames, and weak CPU/stream
  counter correlation.
- Static L/R mix, polarity, simple memoryless nonlinearity, capture clipping,
  and fixed LTI/EQ correction are rejected as sufficient explanations.
- Promotion readiness remains FAIL:
  `local-analysis/promotion-readiness-after-inputdecode-gated-usage.json`
  returns `branch_promotion_allowed=false`.
- Current default remains ISO8/q8 as the stronger quality candidate, but it is
  not a readiness candidate.
- Next technical target:
  isolate and improve USB/device timebase, cadence, scheduling, and packet
  pacing without regressing the passing Pair A matrix evidence.
- Current safety state after cleanup:
  HAL inactive, hardware lock absent, no C mainline or Rust mutation expected.
