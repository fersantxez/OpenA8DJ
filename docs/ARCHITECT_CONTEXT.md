# OpenA8DJ C++/DriverKit Architect Context

Date: 2026-06-16
Worktree: `/Users/fer/dev/audio8djcpp`
Branch: `driverkit/cpp-redesign`

## Current State

- This is a separate C++/DriverKit redesign line for Native Instruments Audio 8 DJ.
- `/Users/fer/dev/opena8dj` is the C/Objective-C mainline and is read-only for this effort.
- `/Users/fer/dev/audio8djrust` is the Rust experiment and is read-only for this effort.
- 2026-06-17 status: C++ is not ready for branch promotion or physical
  readiness claims. Locked physical runs show a CPU-quality tradeoff, not an
  improvement over mainline. ISO8/current-cadence builds remain high CPU and
  still fail strict music quality; ISO64 and playback coalescing reduce CPU but
  collapse physical quality. The raw/reused completion-handler probe reduced
  driver CPU only slightly while still failing physical quality, so it remains
  rejected as a default. Runtime isolation after cleanup is clean.
- 2026-06-17/18 current hardware state: Audio 8 DJ is visible on USB and the
  C++ HAL candidate can be loaded safely enough to enumerate `Open Audio 8 DJ`
  as `8 in / 8 out`, but the candidate still fails physical quality and CPU.
  Locked pair-A soundchecks through iRig failed with quality scores
  `0.136314`, `0.603070`, and `0.495184` across real-music, controlled fixture,
  and driver-sampled fixture runs. The controlled run showed driver CPU around
  `21-22%`. A fresh iRig idle capture stayed clean at max RMS `-62.350199 dBFS`
  and max peak `-41.031139 dBFS`, so idle capture noise alone is not the
  explanation. The active driver sample again points to IOUSBHost async enqueue
  from capture/playback paths as the CPU blocker. The candidate was unloaded
  after diagnostics and audio-stack health passed.
- 2026-06-18 DriverKit SDK state is now measured by an executable offline gate:
  this machine has Command Line Tools selected at
  `/Library/Developer/CommandLineTools`, `xcrun --sdk driverkit --show-sdk-path`
  cannot locate the DriverKit SDK, no `/Applications/Xcode*.app` is installed,
  and `xcodes` CLI is installed and usable at version `2.0.2`. `aria2c` is now
  installed for faster future downloads, but `/Applications` currently has only
  about `12.641 GiB` free against the preflight's conservative `80 GiB` minimum
  for a full Xcode install. Real DriverKit/dext build and readiness claims
  remain blocked until full Xcode with DriverKit SDK is installed and selected.
- The archived iRig WAV frontier now has a C++ forensic gate. It found `61`
  physical runs with WAV evidence, deeply analyzed `12`, and found `0` strict
  audiophile/product candidates. The best analyzed run is
  `20260616-capture-detail-irig-pairA-8s-cpp-hal`: quality `0.978050`, SNR
  floor `9.845114 dB`, matrix explain `4.643718 dB`, lag stddev
  `10.974947` frames, and classification
  `variable_timebase_or_route_capture_instability`. This means the current
  physical frontier is not a simple static gain/crossfeed correction and does
  not prove better sound than mainline.
- A lock-gated hardware pass after commit `8cb4669` confirmed the devices are
  recoverable: iRig Stream and Audio 8 DJ were visible on USB, the C++ HAL
  safety gate loaded and enumerated `Open Audio 8 DJ` as `8 in / 8 out`, then
  it was unloaded after testing. The physical HAL soundcheck still failed:
  quality `0.962986`, SNR floor `10.317819 dB`, `26` lag jumps, driver CPU
  p95 `22.6%`, and coreaudiod p95 `32.4%`. A direct USB/iRig test also failed:
  quality `0.959037`, SNR floor `9.697139 dB`, native lag jumps `20`, and
  classification `uncorrelated_residual_or_capture_path_dominant`. This
  separates the problem: strong residual/noise exists even outside HAL, while
  HAL/CoreAudio still adds unacceptable CPU and lag behavior.
- Direct USB path attribution is now a compiled C++ gate. For the latest
  diagnostics run it proves the internal audio path is clean before the device:
  written/consumed/packed-USB alignment `1.000000`, USB SNR floor `999 dB`,
  USB check errors `0`, and panic flags `0`. The same run's iRig capture still
  fails quality and SNR, so the current dominant physical residual is after the
  packed USB payload: Audio 8 DJ hardware/DAC/analog route/mixer/iRig capture.
- A lock-gated iRig idle capture was added as route isolation evidence. With no
  playback, no HAL, no CoreAudio restart, no USB reset, and no default changes,
  iRig Stream recorded 12.01s at 48 kHz with max RMS `-66.94 dBFS`, max peak
  `-41.65 dBFS`, and max first-difference RMS `-68.87 dBFS`. The new
  `opena8djcpp_irig_idle_capture_gate` passes this as idle-noise evidence only:
  it reduces the likelihood that iRig is permanently noisy, but it does not
  distinguish Audio 8 analog output from mixer/REC OUT under signal.
- Stream-stats observability is now harder to drift: `make hal` rebuilds
  `build/opena8dj-control`, and the offline gate compares the HAL/control
  `OpenA8DJStreamStatsPayload` field list (`203` fields, `0` mismatches in the
  latest run). This removes stale-tool/payload-drift as an avoidable
  explanation before hardware runs.
- HAL runtime geometry is now observable through stream stats. Physical runs
  can attribute the active logical ISO size, capture ISO size, playback base
  ISO size, playback effective ISO size, playback coalesce count, capture queue
  depth, and playback queue target from the same stats payload used by
  `opena8dj-control`. The
  `opena8djcpp_hal_runtime_geometry_observability_contract` blocks physical
  quality/performance claims if this attribution surface drifts.
- Runtime submit observability is now symmetric enough for physical CPU
  comparisons: playback and capture submit counters are exposed through stream
  stats, `run-soundcheck` records them into `stream-stats-during.tsv`, and
  `scripts/analyze-stream-stats.py` summarizes submit rates plus expected
  capture/playback submit ratios from the active runtime geometry. This still
  does not prove CPU superiority; it makes the next lock-gated A/B capable of
  measuring it.
- 2026-06-17 hot-path timing is now opt-in (`HAL_HOT_PATH_TIMING=1`) and off
  by default. A locked iRig run showed physical quality still FAILs
  (`quality_alignment_score=0.970666`, SNR `10.78 dB`, `19` lag jumps) and
  showed the dominant callback-adjacent cost is USB enqueue/requeue, not
  sample packing: average ticks were capture handler `3755`, capture requeue
  `1826`, playback enqueue `1494`, playback fill `289`, playback completion
  `20`.
- Stream-stats analysis now exports raw and sampled transfer denominators
  separately. The corrected capture view for the same run is about `1000.35`
  raw capture transfers/s and `62.43` sampled transfers/s; among sampled
  transfers, capture has about `4.36` valid transactions and `3.64`
  zero-complete transactions per 8-slot transfer, with all `8.0` slots
  classified. Do not compare sampled transaction counters against raw transfer
  counts. Also do not assume the partial layout is automatically a bug: at
  48 kHz, queuing all 8 slots every millisecond would read roughly 88k output
  frames/s, while the observed partial layout corresponds to the measured
  ~48k output read rate.
- Raw capture/playback completion telemetry is now fixed. Locked short iRig
  runs changed reported playback submitted/completed from `8131/508` to
  `8123/8123`, then corrected capture/playback comparison to
  `captureTransfersCompleted=8137`, `playbackTransfersSubmitted=8129`,
  `playbackTransfersCompleted=8129`. The same fixed run still failed strict
  quality, and the earlier `16x` playback/capture conclusion is rejected as a
  sampling artifact.
- Direct USB timeline instrumentation now separates startup delay from analog
  output delay. A locked run measured first captured iRig energy `0.191420s`
  after the first direct USB write, while `OpenA8DJUSBStart` consumed about
  `4.24s` before the first write. Reset no-wait experiments are not promoted:
  no-settle failed start, `100ms` produced no captured energy, and `250ms` was
  not stable across short versus longer runs. Default remains reply-waiting
  reset until repeated evidence proves a safer control sequence.
- Reboot/autologin recovery is an unresolved operational gap. After the prior
  reboot, the session did not recover without user intervention as intended;
  this must be fixed before planned unattended test windows.
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
- 2026-06-17 promotion evidence is now stricter: a same-window
  non-Audio8 known-good route revalidation is mandatory in the physical
  promotion bundle. A route-only historical PASS or a `--skip-known-good`
  candidate run cannot satisfy branch promotion, even if later music/CPU
  artifacts are present.
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
- ISO capture invariant tooling now classifies the latest run as packetization
  PASS with a one-stop-transfer warning:
  `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`.
  Do not chase aggregate `captureTransactionErrors` as the primary defect
  unless status failures, short transfers, other-size transactions, or larger
  classified-slot gaps appear.
- A dedicated diagnostic build profile exists:
  `make hal-cadence-diagnostic`. It is for the next locked cadence/ledger
  evidence run only; restore product flags with `make -B hal` before any CPU
  or product-quality claim.
- Latest locked cadence diagnostic run:
  `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal`.
  - Quality still FAIL:
    `quality_alignment_score=0.958757`, SNR `10.09 dB`, mid/high residual
    `1.447622/1.366173`, quiet mid noise `-35.03 dBFS`, `27` lag jumps.
  - Promotion readiness still FAIL:
    `local-analysis/promotion-readiness-after-cadence-diagnostic.json`.
  - Ledger and payload evidence reject gross transport corruption:
    continuous ledger, no payload mismatches, no playback errors, no output
    underruns, no timeline resets, no late writes.
  - Cadence outliers are now visible:
    capture completion outliers `7`, playback completion outliers `8`.
  - Runtime correlation is weak, so completion jitter remains the next
    hypothesis to test, not a proven root cause.
  - Product HAL build was restored and final isolation passed:
    `local-analysis/runtime-isolation/after-cadence-diagnostic-unload.json`.
- Latest locked product timing probe:
  `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal`.
  - Built with `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1`.
  - Quality still FAIL:
    `quality_alignment_score=0.961360`, SNR floor `10.25 dB`, mid/high
    residual `1.425897/1.365001`, quiet mid noise `-35.03 dBFS`, `28` lag
    jumps.
  - Runtime CPU still fails mainline:
    driver p95 `21.8%`, `coreaudiod` p95 `12.2%`.
  - Capture ISO invariants PASS with stop-window warning.
  - Stream stats show no output active underruns, timeline resets, late writes,
    or lightweight completion delta outliers, but real audio lag jumps remain.
  - Failure analysis remains `timebase_or_alignment_instability`; fixed LTI/EQ
    correction worsens SNR.
  - Decision: reject this timing order as product default.
  - Product HAL build was restored and final isolation passed:
    `local-analysis/runtime-isolation/after-playback-before-capture-requeue-unload.json`.
- Current safety state after cleanup:
  HAL inactive, hardware lock absent, no C mainline or Rust mutation expected.
- Offline transfer model update after the timing probe:
  - `tools/transfer_pool_model.cpp` now gates transport-rate safety, not only
    transfer-pool fallback allocations.
  - `HAL_CAPTURE_PACED_OUT_LEAD>1` on the current implicit scheduling path is
    blocked from physical testing:
    lead2 ratio `2`, lead4 ratio `4`, lead64 ratio `64`.
  - Safe model rows keep playback queue ratio near `1`:
    default lead1 and mainline-like queue8.
  - Offline gates pass after the model change:
    Debug `17/17`, Release `18/18`.
  - Next viable timing work must preserve 1:1 capture/playback cadence before
    asking for hardware time.
- Latest locked CPU/hot-path probe:
  `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal`.
  - Built with `HAL_REUSE_ISOC_COMPLETIONS=1` only.
  - Quality still FAIL:
    `quality_alignment_score=0.961164`, SNR floor `9.98 dB`, mid/high
    residual `1.459843/1.377935`, quiet mid noise `-34.84 dBFS`, `25` lag
    jumps.
  - Runtime CPU still fails mainline:
    driver p95 `22.1%`, `coreaudiod` p95 `15.0%`.
  - Capture ISO invariants PASS with stop-window warning.
  - No gross output underruns, timeline resets, late writes, or transfer-pool
    fallback allocations.
  - Decision: reject reused ISO completion handlers as product default.
- Latest locked descriptor-layout probe:
  `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal`.
  - Built with `HAL_FAST_ISO_TRANSFER_CONFIG=1` only.
  - Quality still FAIL:
    `quality_alignment_score=0.959397`, SNR floor `10.19 dB`, mid/high
    residual `1.450623/1.368530`, quiet mid noise `-35.05 dBFS`, `35` lag
    jumps.
  - Runtime CPU still fails mainline:
    driver p95 `23.1%`, `coreaudiod` p95 `25.9%`.
  - Capture ISO invariants PASS with no warnings, but product quality and CPU
    still fail.
  - Decision: reject fast ISO transfer config as product default.
- Subagent synthesis:
  - Hume identified low-risk CPU flags to isolate; reused completion handlers
    and fast ISO transfer config have now both been physically rejected as
    defaults.
  - Carver ranked capture/reference route validation as the next lowest-risk
    high-value physical step because multiple current runs share a failing
    route signature. Do not claim audiophile quality until the capture route is
    independently validated.
- Offline route-signature comparison:
  - Evidence:
    `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature`.
  - A shared severely degraded route-family exists:
    mainline wait45, C++ inputdecode-off, and C++ ISO64/q8 StopIO all show
    quality around `0.68`, SNR around `-0.83 dB`, mid residual around `2.53`,
    high residual around `1.78`, and very low mid coherence around `0.02`.
  - Current C++ probes are a different failing family:
    quality around `0.96-0.97`, SNR around `10 dB`, residual around
    `1.4/1.36`, and persistent lag jumps.
  - Interpretation:
    the degraded route-family blocks quality comparisons, but current C++ still
    fails independently. Route validation is now a prerequisite for any
    audiophile claim.
- Latest inline inactive input decode bypass probe:
  `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal`.
  - Built with an earlier inactive-decode bypass in `handleCaptureTransfer`;
    the source change was removed after measurement.
  - Offline gates before hardware passed:
    Debug `17/17`, Release `18/18`.
  - Quality still FAIL:
    `quality_alignment_score=0.961965`, SNR floor `10.16 dB`, mid/high
    residual `1.429792/1.358387`, quiet mid noise `-35.03 dBFS`, `31` lag
    jumps.
  - Runtime CPU still fails mainline:
    driver p95 `22.1%`, `coreaudiod` p95 `41.3%`.
  - Final isolation passed:
    HAL inactive, hardware lock absent, no OpenA8DJ/mainline QA process
    detected.
  - Decision:
    reject this micro-optimization as a product change. The remaining blocker
    is still the quality/timebase/route problem, not inactive decode call
    overhead.
- Latest output sample time follower probe:
  `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal`.
  - Built with `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1`, preserving payload,
    queue geometry, capture-paced playback, coalesce `1`, and 1:1 cadence.
  - Quality still FAIL:
    `quality_alignment_score=0.962572`, SNR floor `9.94 dB`, mid/high
    residual `1.458736/1.377276`, quiet mid noise `-34.98 dBFS`, `28` lag
    jumps.
  - Runtime CPU still fails mainline and regressed:
    driver p95 `24.7%`, `coreaudiod` p95 `53.0%`.
  - Capture ISO invariants PASS and stream stats show no gross underruns,
    timeline resets, late writes, or pool fallback allocations.
  - Final isolation passed:
    HAL inactive, hardware lock absent, no OpenA8DJ/mainline QA process
    detected.
  - Decision:
    reject `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1` as a product default. The
    remaining useful path is independent route validation or a deeper
    USB/device transport state model.
- Current-family timebase window comparison:
  `local-analysis/timebase-window-comparison/20260617-current-family`.
  - Compared seven recent C++ Pair A/iRig music captures offline.
  - Local per-window lag correction improves mid-band residual by only
    `0-2%`.
  - Corrected mid residual medians remain around `1.41-1.48`.
  - Window lag jumps persist in every run: `22-35`.
  - Interpretation:
    the current failure is not just slow drift or a local offset problem. The
    next useful evidence must validate the physical route or expose a deeper
    USB/device transport-state mechanism.
- Practical mainline physical music floor comparison:
  `local-analysis/mainline-practical-floor/20260617-current-cpp-music-family/summary.json`.
  - Read-only mainline docs show the mixer REC OUT/iRig route was considered
    valid even though a time-warped Pair A music recheck measured quality
    `0.962043`, SNR `9.97 dB`, mid/high residual `1.637216/1.412494`.
  - Best current C++ streamusage run is close to the practical music floor:
    quality `0.971648`, mid residual `1.399655`, quiet mid `-35.20 dBFS`,
    lag jumps `28`, clipping `0`.
  - It still fails high residual:
    `1.358543` versus threshold `1.355`.
  - CPU remains a hard blocker independent of music-floor proximity.
  - Interpretation:
    track the practical floor to avoid chasing route artifacts blindly, but keep
    strict audiophile and better-than-mainline promotion gates closed.
- Native physical-run comparator:
  `local-analysis/physical-run-compare/20260617-profile-family.json`.
  - Compared four current profiling runs with practical quality+CPU thresholds.
  - Result: all runs fail. The steady v5 run fails badly
    (`quality_alignment_score=0.941259`, SNR floor `-11.59 dB`, mid/high
    residual `4.233590/3.665879`, `82` lag jumps, driver p95 `24.0%`).
  - Best short v4 still fails high residual and CPU:
    mid/high residual `1.456104/1.366229`, driver p95 `22.2%`.
  - The exact-PID steady sample shows the active USB queue dominated by
    capture/playback requeue into `IOUSBHostPipe` and `IOConnectCallAsyncMethod`.
    Packet filling is secondary in that profile.
  - Added an appended `playbackTransfersSubmitted` payload field because the
    previous `opena8dj-control` submitted counter used a cadence-diagnostic
    field and reported `0` in product profiling runs.
  - Interpretation:
    C++ is not ready. The next useful optimization has to reduce USB enqueue
    overhead or explain the timeline/route failure; packer micro-optimizations
    alone are not supported by the current sample.
- Added opt-in `HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1`:
  - Default remains `0`.
  - Goal: playback-only diagnostics can avoid keeping ISO IN active when input
    decode/DVS is inactive, while preserving HAL input representation and
    restarting capture if input decode becomes active.
  - Packaging fix: HAL `Contents/Info.plist` must be installed as `0644`;
    stale `0600` permissions caused CoreAudio to miss `org.opena8dj.Audio8DJ`
    even though USB saw the device.
  - Physical result: rejected for product. The first run submitted no playback
    transfers; after the write-side fill fix, playback resumed but quality
    remained bad (`quality_alignment_score=0.183990`, SNR floor `-21.45 dB`,
    mid/high residual `17.171794/11.452494`) and coreaudiod p95 rose to
    `28.3%`.
  - Interpretation:
    capture ISO completion pacing appears to be part of the practical playback
    timing model in this HAL path. Removing ISO IN is not a valid optimization
    unless a new transport scheduler preserves physical quality and total CPU.
- `HAL_IGNORE_OUTPUT_SAMPLE_TIME=1` was added and physically rejected as a
  product behavior.
  - Rationale:
    direct USB abs-deadline tests write contiguous output with
    `sampleTimeValid=false`; this tested whether HAL could gain the same benefit
    by ignoring CoreAudio `mOutputTime.mSampleTime`.
  - Evidence:
    `local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal`.
  - Result:
    HAL safety PASS, physical music FAIL (`quality_alignment_score=0.963508`,
    SNR floor `10.20 dB`, mid/high residual `1.440572/1.369361`, `32` lag
    jumps), CPU FAIL (driver p95 `22.6%`, coreaudiod p95 `44.7%`).
  - Final state:
    default build restored to `OPENA8DJ_IGNORE_OUTPUT_SAMPLE_TIME=0`, HAL
    unloaded, lock absent, runtime isolation PASS.
  - Interpretation:
    the simple sample-time-validity difference between direct USB and HAL is
    not sufficient. The remaining defect is still deeper timebase/transport
    behavior or physical-route interpretation, not solved by contiguous HAL
    timeline writes.
- Carver's direct-like queue/prefetch experiment was tested and physically
  rejected:
  - Build:
    `HAL_ISO_FRAMES=8 HAL_PLAYBACK_ISO_FRAMES=8 HAL_CAPTURE_QUEUE=64
    HAL_PLAYBACK_QUEUE=64 HAL_OUTPUT_PREFETCH_FRAMES=256`.
  - Evidence:
    `local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal`.
  - Result:
    Pair A matrix PASS with max wrong-source leakage about `-53.08 dB`, but
    physical music FAIL (`quality_alignment_score=0.966043`, SNR floor
    `10.15 dB`, mid/high residual `1.442529/1.373910`, `25` lag jumps), CPU
    FAIL (driver p95 `23.7%`, coreaudiod p95 `86.6%`).
  - Final state:
    default HAL rebuilt, HAL unloaded, lock absent, runtime isolation PASS.
  - Interpretation:
    queue/prefetch margin can preserve static routing but does not solve the
    audiophile gate. The next useful work must isolate the residual/continuity
    mechanism or reduce USB/CoreAudio overhead without degrading music.
- Added direct USB music soundcheck tooling and ran the first lock-gated Pair A
  diagnostic:
  - Evidence:
    `local-analysis/direct-usb-soundcheck/20260617-direct-usb-plain-gain05-lead8192-pairA-12s`.
  - Result:
    direct USB selected Pair A with `plain-gain05` and lead `8192` failed hard
    on music (`quality_alignment_score=0.103211`, worst-channel SNR
    `-24.31 dB`, mid/high residual `17.114359/16.212469`, no clipping).
  - Interpretation:
    direct USB tone-matrix success does not generalize to real music and cannot
    be used as readiness evidence. The next product candidate must still beat
    HAL music/CPU gates directly.
- Physical latency is now a formal promotion gate:
  - Current evidence fails. Representative direct USB Pair A has
    `first_energy_seconds=5.25`, `best_correlation=-0.623648`,
    `aligned_snr_db=-7.78`, and `linear_fit_snr_db=-1.74`.
  - Explicit-scheduling fallback still fails: `first_energy_seconds=4.95`,
    `best_correlation=0.029593`, and `linear_fit_snr_db=-30.81`.
  - Promotion remains blocked regardless of offline PASS status.
- Explicit isochronous scheduling is rejected as a product direction:
  - Without fallback, queue saturation produces `queue_failures=2805` and
    `qfail_explicit=2805`.
  - With `HAL_EXPLICIT_SCHED_FAIL_FALLBACK=1`, fallback fires once and reduces
    queue failures to `135`, but physical quality still fails badly.
  - Default build restored to explicit scheduling off and fallback off.
- Direct USB marker probe:
  - The marker route captures strong signal with stable offset:
    mean `4.646000s`, std `0.001237s` after split-peak merging.
  - A lead `0` rerun also captures stable delayed markers:
    mean `4.900115s`, std `0.001250s`.
  - After subtracting wrapper record pre-roll and expected internal
    lead/startup silence, residual delay remains about `3.78s..4.13s`.
  - This argues against random drift or direct-player lead as the sole
    explanation.
  - It does not clear readiness because best-aligned SNR and residual remain
    poor (`aligned_snr_db=-2.33`, `linear_fit_snr_db=-0.64`,
    residual/capture `0.732560`).
  - USB diagnostic marker run:
    internal written, consumed, and packed USB buffers all align perfectly to
    reference (`alignment_score=1.000000`, lag `0`, SNR `999.00 dB`,
    USB check errors `0`), but external iRig marker remains delayed
    (`offset_mean=4.930875s`) and physical latency FAIL.
  - Current boundary:
    C++ sample timeline, consumed-buffer order, and Mode 2 packed bytes are
    not the observed marker failure. Remaining suspects are downstream:
    USB/device scheduling/state, Audio 8 DJ firmware/DAC interpretation,
    analog route, or external capture path.
  - `HAL_VALID_CAPTURE_OUT_LAYOUT=1` was tested as a direct USB marker
    diagnostic and rejected:
    marker mean `4.638750s`, std `0.001297s`, residual after expected offsets
    `3.773417s`, physical latency FAIL, and physical quality FAIL with SNR
    floor `-31.75 dB`.
  - Forced playback-profile control state was tested and rejected as a physical
    fix:
    control changed from `00:02:03:01:02:01` to `01:02:03:00:02:00`, but marker
    mean remained `4.667208s`, residual after expected offsets `3.807208s`,
    physical latency FAIL, and physical quality FAIL.
  - `HAL_SELECT_ALT0_BEFORE_ALT1=1` plus playback profile was tested:
    diagnostics confirmed `select_alt0_before_alt1=1`, marker latency improved
    to mean `0.405589s` with std `0.001256s`, and first energy was `0.65s`.
    This is a real state-reset clue, but physical quality/linearity still FAIL:
    `best_correlation=0.414578`, `aligned_snr_db=-3.99`,
    `linear_fit_snr_db=-6.76`, quality `0.858726`, SNR floor `-14.49 dB`.
  - The same alt0/playback-profile candidate was tested with real music and
    rejected as a complete product fix:
    quality `0.103674`, SNR floor `-24.25 dB`, mid/high residual
    `16.213903/15.560684`, clean transport counters, no clipping.
  - Default has been rebuilt back to `OPENA8DJ_VALID_CAPTURE_OUT_LAYOUT=0`.
  - Default has also been rebuilt back to
    `OPENA8DJ_SELECT_ALT0_BEFORE_ALT1=0`.
  - Playback profile remains a valid control-plane consistency fix, but not a
    readiness signal.
- Continuous output timeline reset fix:
  - The previous alt0/playback-profile music rejection hid a real C++ timeline
    defect. When direct USB writes were continuous but more than half a ring
    ahead of the reader, `OutputTimelineWrite` reset the timeline and inserted
    silence/underrun gaps in the middle of music.
  - The reset was observed at write frame `162048`, with read anomalies
    beginning around served frame `145600`.
  - The reset predicate now excludes continuous writes; discontinuous future
    writes can still reset.
  - Post-fix 12-second direct USB diagnostics are internally perfect:
    written, consumed, and packed USB output all align at `1.000000`, lag `0`,
    SNR `999.00 dB`, USB check errors `0`, panic flags `0`.
  - Physical iRig music quality improved to quality `0.957628`, SNR `9.38 dB`,
    mid/high residual `1.422297/1.413835`, lag jumps `0`, clipping `0`.
    This is still below audiophile thresholds and does not permit promotion.
  - Time-warped reanalysis still fails (`quality=0.961334`, SNR `10.41 dB`,
    mid/high residual `1.404391/1.367270`), so the remaining physical failure
    is not only the old destructive reset.
  - Current boundary:
    C++ write path, consumed path, and Mode 2 packed USB are now proven clean
    for the fixed direct-USB music run. Remaining blockers are physical
    output/capture quality, same-day mainline A/B, runtime CPU on a fixed HAL
    candidate, and Traktor/timecode physical validation.
- Decorrelated direct USB Pair A fixture status:
  - Evidence:
    `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag`.
  - Internal data plane PASS:
    written, consumed, and packed USB output all align at `1.000000`, lag `0`,
    SNR `999.00 dB`, USB `check_errors=0`, USB `panic_flags=0`.
  - Physical Pair A routing PASS:
    `max_wrong_source_leakage_db=-57.447168`,
    `left_to_right_leakage_db=-61.527228`,
    `right_to_left_leakage_db=-55.793274`, capture clipping `0`.
  - Physical waveform quality FAIL:
    `quality_alignment_score=0.721193`, SNR floor `-2.96 dB`, mid/high
    residual `2.117458/2.018361`, quiet mid-band noise `-21.77 dBFS`.
  - Readiness implication:
    the C++ direct USB diagnostic route now has strong evidence for packet
    correctness and Pair A routing, but it still does not prove audiophile
    quality, product HAL CPU, or Traktor/timecode behavior.
  - Current next objective:
    get same-day mainline-vs-C++ product HAL A/B evidence with the same
    validated capture route, plus fixed-candidate CPU and timecode evidence.
    Until that exists, do not move C++ to `main` and do not move C mainline to
    `Legacy`.
- Same-day product HAL A/B status:
  - Evidence:
    `local-analysis/mainline-ab/20260617-sameday-ab-085735/ab-comparison.json`.
  - Result:
    `FAIL_CPP_NOT_BETTER_THAN_MAINLINE`.
  - Both candidates failed absolute audiophile quality gates on the same
    fixture and iRig route.
  - C++ product HAL:
    quality `0.134709`, SNR `-12.66 dB`, mid/high residual
    `4.904891/4.494813`, lag jumps `18`, driver CPU p95 `23.2%`,
    coreaudiod p95 `20.5%`.
  - Mainline HAL:
    quality `0.246599`, SNR `-13.28 dB`, mid/high residual
    `5.774651/5.636904`, lag jumps `41`, driver CPU p95 `5.6%`,
    coreaudiod p95 `10.3%`.
  - Interpretation:
    C++ has useful partial wins in residual ratio and lag jumps, but it loses
    global quality alignment and CPU. The objective claim "better than
    mainline" is false for the current product HAL.
  - Current boundary:
    no branch promotion, no Legacy move, and no timecode readiness claim.
    Optimization must now target product HAL CPU and physical alignment/quality
    directly.
- Transport cadence subagent finding:
  - C++ ISO8 default versus mainline ISO64 default is the major build-geometry
    divergence, but the exact C++ ISO64/q8 candidate was already physically
    rejected (`quality_alignment_score` around `0.67`, SNR below `0 dB`).
  - Capture/playback `firstFrameNumber=0`, `IOUSBHostIsochronousTransferOptionsNone`,
    default completion handler reuse off, fast ISO config off, and audio-param
    reset/start intent are aligned enough that they are not the immediate
    divergence.
  - Next experiments must respect audio-rate math. Forcing full OUT request
    layouts from ISO8 capture-paced completions would over-read output audio;
    any smoothing/coalescing idea must prove average output frames/s,
    physical quality, and CPU against mainline.
- Offline rate-shape gate:
  - `tools/jitter_model.cpp` now includes `rate_shape_rows` so transport
    candidates can be rejected before hardware if their average output
    consumption cannot stay near the selected sample rate.
  - Current evidence:
    `local-analysis/cpp-offline/jitter-model.json` and
    `local-analysis/cpp-offline/current-offline-gates.json`.
  - The observed ISO8 partial capture-paced layout is rate-safe within the
    model: about `4.360721` playback transactions/ms at `352` bytes/request
    and `32` USB bytes/frame yields about `47967.9` frames/s, `-668 ppm`.
  - A forced full-8 ISO8 layout is rejected offline: it would consume about
    `88000` frames/s, `833333 ppm` high, before any sound-quality question.
  - A mainline-like ISO64/q8 rate shape is mathematically rate-safe but remains
    blocked as a C++ product default because the exact candidate was already
    physically rejected. Rate correctness alone is not a readiness claim.
- Rejected-default static policy:
  - `tools/static_policy_check.cpp` now audits Makefile HAL defaults for known
    rejected or diagnostic-only knobs, in addition to unsafe mutation strings.
  - The gate currently checks `21` defaults, including ISO64 promotion, playback
    coalescing, output-only mode, explicit scheduling, native/little-endian
    output, unrolled pack, fast prefetch clear, atomic stream stats, reused ISO
    completions, fast ISO config, output sample-time follower, ignore sample
    time, valid-capture OUT layout, hot-path timing, and alt0-before-alt1.
  - Purpose:
    prevent accidental reintroduction of physically rejected paths before they
    burn another hardware window. Passing this gate is not readiness.
- Timebase family analysis:
  - Added `scripts/analyze-timebase-family.py` as an offline-only aggregator
    over existing `analyze-soundcheck-window-trace.py` JSON outputs.
  - Current C++ family evidence:
    `local-analysis/timebase-window-comparison/20260617-current-family/timebase-family.json`.
    Across `7` current-family C++ traces, all runs have lag jumps and residual
    after local lag correction. Maximum linear drift is only about `40 ppm`, so
    linear drift alone does not explain the failure; the stronger blocker is
    discontinuous/local alignment plus remaining mid-band residual. The
    classifier now reports `analysis_result=PASS`, `stability_result=FAIL`.
  - Same-day A/B evidence:
    `local-analysis/mainline-ab/20260617-sameday-ab-085735/timebase-ab.json`.
    The C++ A/B run has a large mostly fixed lag around `-265..-285` frames;
    correcting local lag improves median mid-band residual from about `3.05` to
    `0.89`. The mainline A/B run remains worse after local correction, with
    drift around `-129 ppm`, `41` lag jumps, and corrected mid residual about
    `5.63`. The A/B family is still `stability_result=FAIL`.
  - Interpretation:
    raw alignment metrics can underrate a candidate when fixed latency is wrong,
    but C++ still cannot be promoted because current-family physical runs still
    show lag jumps, residual after local correction, high CPU, and no physical
    timecode/Traktor proof.
- Output-cycle flush alignment:
  - Mainline writes mixed output to the USB timeline from `EndIOOperation`,
    while C++ had also flushed early from `WriteMix` once expected streams were
    present. That early flush can change when a cycle enters the output
    timeline even when the CoreAudio `sampleTime` is the same.
  - C++ now defaults `HAL_FLUSH_OUTPUT_IN_WRITE_MIX=0`, matching mainline's
    end-of-cycle flush behavior. The old behavior remains available only as an
    explicit diagnostic flag.
  - This is a candidate timing change, not readiness evidence. It needs a
    locked same-route physical music/CPU A/B before any quality or performance
    claim.
  - Locked Pair A/iRig evidence at commit `a3dd76a` rejected it as a standalone
    fix: quality `0.962241`, SNR `10.29 dB`, `23` lag jumps, corrected mid
    residual median `1.413201`, driver CPU p95 about `22.4%`, and promotion
    readiness `FAIL`.
  - Post-run state was cleaned up: HAL unloaded, no OpenA8DJ driver PID, audio
    stack guard PASS.
- Fractional time-warp residual diagnostic:
  - Added `scripts/analyze-fractional-time-warp.py` after Arendt's residual
    hypothesis.
  - Current best C++ captures reject fractional delay/time-warp as the dominant
    residual explanation:
    - ISO10/q8 scalar/matrix SNR improvement `0.725/0.705 dB`.
    - ISO8/q8 scalar/matrix SNR improvement `0.372/0.349 dB`.
    - ISO12/q8 scalar/matrix SNR improvement `0.929/0.914 dB`.
    - Raw/reused completions scalar/matrix SNR improvement `0.727/0.718 dB`.
  - The threshold for a partial explanation is `3.0 dB`; all current best C++
    runs are below it. LTI transfer modeling also fails to improve them and
    shows very low mid/high coherence.
  - Same-session degraded A/B captures do improve under warp, but with low
    delay scores and existing fixture-degraded classification. This is not a
    readiness signal.
  - Evidence:
    `local-analysis/offline-diagnostics/20260617-fractional-time-warp-multi.json`,
    `local-analysis/offline-diagnostics/20260617-lti-transfer-multi.json`, and
    `local-analysis/offline-diagnostics/20260617-failure-modes-multi.json`.
  - Updated implication:
    alignment/capture post-correction is not the product path. The blocking
    work remains HAL USB enqueue CPU, physical route validation, and a same-
    session reference that is healthy enough to support audiophile claims.
- Transport budget gate:
  - Added compiled model `tools/transport_budget_model.cpp`, now wired into
    CTest and `scripts/run-cpp-offline-gates`.
  - Current frontier:
    - best quality family `iso5_q64`: quality `0.978050`, estimated USB enqueue
      calls/s `400`, median driver CPU p95 `36.9%`.
    - current middle families `iso8_q8`, `iso10_q8`, `iso12_q8`: quality below
      `0.98` and driver CPU p95 `16.6-22.4%`.
    - CPU-near family `iso64_q8`: estimated enqueue calls/s `31.25`, median
      driver CPU p95 `6.3%`, but quality only `0.686712`.
  - The gate reports `product_candidate_exists=false`: no observed family
    simultaneously satisfies quality, driver CPU, and zero-lag-jump gates.
  - Meaning:
    more hardware should not be spent on simple cadence ladder variants unless
    the implementation changes the quality/CPU frontier, not just its position
    along the old tradeoff curve.
- Prepared DriverKit transport contract:
  - Added compiled model `tools/driverkit_prepared_transport_contract.cpp`.
  - The contract encodes the next CPU architecture direction: HAL steady-state
    must perform `0` direct USB requeues while the backend owns prepared slot
    requeue, cadence remains `1x`, timestamps stay ordered, and A/B/C/D plus
    timecode semantics remain represented.
  - Negative scenarios reject HAL direct requeue, steady-state fallback
    allocations, coalesced completion gaps, and timestamp reorder.
  - This is offline architecture evidence only. It is not a real DriverKit dext,
    not a hardware pass, and not better-than-mainline proof.
- Core prepared transport backend:
  - Added `PreparedTransportBackend` to the pure C++ core.
  - It models HAL-facing playback writes and capture reads through SPSC rings,
    while backend completions own prepared-slot requeue counters and timestamp
    ordering.
  - `opena8djcpp_driverkit_prepared_transport_contract` now uses this core type
    directly, so the gate validates reusable product-path code instead of a
    standalone simulation.
  - Still missing:
    packet batch integration, real USB/DriverKit adapter, physical CPU/quality
    evidence, and same-session mainline comparison.
- Prepared transport packet/ring contract:
  - Added `opena8djcpp_prepared_transport_packet_contract`.
  - It uses real Mode2 packet pack/decode logic with `PreparedTransportBackend`
    batch APIs: capture USB bytes decode into the backend capture ring,
    playback frames come out of the backend playback ring, then playback bytes
    are packed and decoded again.
  - Current offline evidence reports `131` capture frames and `131` playback
    frames decoded with `0` check errors, `0` prefix mismatches, `0` HAL steady
    requeues, and `0` fallback allocations.
  - Still missing:
    routing/timecode-profile batch policy over the backend, real USB adapter,
    and physical proof against mainline.
- Prepared transport routing/timecode contract:
  - Added `opena8djcpp_prepared_transport_routing_timecode_contract`.
  - It validates S24 playback routing through the prepared backend and validates
    timecode-vinyl, timecode-cd-line, and phono profiles for decks A/B/C/D after
    Mode2 decode and backend capture-ring traversal.
  - Current evidence:
    `12` profile/deck rows PASS, playback routing PASS with `0` mismatches,
    `0` HAL steady requeues, and `0` fallback allocations.
  - Still missing:
    real DriverKit/USB adapter, physical quality and CPU proof, recovery, and
    same-session mainline comparison.
- Prepared transport recovery contract:
  - Added `opena8djcpp_prepared_transport_recovery_contract`.
  - It rejects invalid configs, rejects false `product_safe` before start,
    verifies operations after `stop()` fail without mutating sentinels, verifies
    restart clears stale capture/playback frames, and verifies counters plus
    timestamp history reset for the new stream session.
  - `PreparedTransportBackend::safety()` now requires `started_`; zero counters
    on a never-started backend are not readiness evidence.
  - Still missing:
    real DriverKit/USB adapter, physical quality and CPU proof, and
    same-session mainline comparison.
- Offline DriverKit runtime bridge:
  - `AudioDriverSkeleton` now owns stream configuration and a
    `PreparedTransportBackend`.
  - Added `opena8djcpp_driverkit_runtime_contract`, which verifies that a
    DriverKit-style shell can reject invalid stream configs, start a valid
    48 kHz stream, move playback/capture frame batches through the prepared
    backend, and shut down without leaving an active product-safe stream.
  - This is still not a dext, not signed, not installed, and not hardware
    evidence. It is the next offline executable boundary.
- DriverKit extension scaffold:
  - Added `driverkit/extension/` with non-installing templates for
    `Info.plist`, entitlements, `OpenA8DJAudioDriver.iig`,
    `OpenA8DJAudioDevice.iig`, and future SDK binding sources.
  - Added `opena8djcpp_driverkit_extension_scaffold_contract` to validate
    dext metadata, AudioDriverKit class shape, USB match IDs
    `0x17cc:0x1978`, entitlements, runtime binding intent, and default-build
    exclusion.
  - This is not a signed, installed, activated, or runnable dext.
- C++ loopback quality analyzer:
  - Added compiled gate `opena8djcpp_loopback_quality_analysis`.
  - It reads WAV pair references and either WAV pair captures or raw f32
    interleaved captures for a selected stereo pair.
  - Metrics: alignment, gain, correlation, SNR, residual RMS/peak, peak level,
    and robust click outliers.
  - The offline selftest passes a clean delayed/gain-shifted loopback and
    intentionally rejects a degraded loopback with residual distortion,
    cross-feed, and a click.
  - This is an analyzer/readiness prerequisite, not product proof. The actual
    claim still requires locked physical same-session captures versus mainline.
- C++ channel leakage tone contract:
  - Added compiled gate `opena8djcpp_channel_leakage_tone_contract`.
  - It exercises A/B/C/D at 44.1 kHz and 48 kHz through real Mode 2 pack/decode
    and measures tone leakage into wrong source/inactive decks.
  - Clean digital rows PASS; injected inactive-deck leakage is rejected.
  - This strengthens offline no-leakage coverage but does not replace physical
    matrix capture with the actual Audio 8 DJ analog path.
- Subagent Zeno recommendation:
  - Highest-value next metric is a C++ analyzer over stored physical capture
    directories, porting the Python tone-matrix/quality fields into a compiled
    offline tool without opening audio devices.
- C++ capture matrix quality analyzer:
  - Added `opena8djcpp_capture_matrix_quality_analysis`.
  - It reads stored `fixture/reference.wav` and `captured.wav` directories,
    then reports alignment, per-channel gain/correlation/SNR, residuals,
    clicks, clipping, expected tone floor, and decorrelated-tone leakage.
  - Selftest PASSes: clean synthetic capture accepted, degraded capture
    rejected for SNR/correlation/click/leakage/clipping.
  - A stored physical direct-USB capture can pass routing/leakage while failing
    global SNR/correlation, reinforcing that no single metric is enough for
    audiophile readiness.

### 2026-06-17 Native Product Superiority Comparator

- Added native C++ product comparison evidence:
  `local-analysis/cpp-offline/physical-run-product-superiority.json`.
- The comparator reads existing soundcheck evidence only and checks the latest
  complete candidate against mainline reference thresholds for music quality,
  SNR, residuals, quiet noise, lag jumps, click outliers, clipping, driver CPU,
  and `coreaudiod` CPU.
- Current result is deliberately blocking:
  `branch_promotion_supported=false`.
- Latest selected candidate
  `20260617-iso12q8-irig-pairA-12s-cpp-hal` fails quality and CPU:
  quality `0.963395`, SNR floor `9.675760 dB`, lag jumps `32`, click outliers
  `1`, driver CPU p95 `16.6%`, and `coreaudiod` p95 `35.4%`.
- The selected run has `fixture/reference.wav` and `captured.wav`; the
  comparator marks native WAV reanalysis as `AVAILABLE_NOT_YET_USED`, so the
  next implementation target is a native C++ reanalysis of those WAVs.

### 2026-06-17 Native Soundcheck WAV Quality

- Added `opena8djcpp_soundcheck_wav_quality`.
- It reopens stored soundcheck WAVs and computes native C++ alignment,
  left/right SNR, mid/high residual ratios, quiet mid noise, lag jumps, clicks,
  and clipping.
- Latest analyzed run:
  `20260617-iso12q8-irig-pairA-12s-cpp-hal`.
- Native alignment is now consistent with Python:
  `reference_start=81`, `capture_start=6228`, `alignment_lag=6147`.
- Parity against recorded metrics is `PASS` for the first six broad
  comparisons, but this remains analyzer progress only. Product quality and CPU
  still fail.
- Offline gates pass, but this does not improve physical sound; it improves the
  integrity of the readiness claim.

### 2026-06-17 Current Evidence After Native Attribution

- Worktree: `/Users/fer/dev/audio8djcpp`.
- Branch: `driverkit/cpp-redesign`.
- Latest offline bundle command: `scripts/run-cpp-offline-gates`.
- Offline bundle: PASS.
- Debug CTest: `30/30` passed.
- Release CTest: `31/31` passed.
- Product comparator: FAIL, `branch_promotion_supported=false`.
- Selected product run:
  `20260617-iso12q8-irig-pairA-12s-cpp-hal`.
- Native WAV reanalysis is now used by the comparator for the selected run.
- Native WAV metrics:
  quality `0.953641`, SNR floor `8.797298 dB`, mid/high residual
  `1.685303/1.580494`, quiet residual `-34.694516 dBFS`, lag jumps `32`,
  clicks `0`, clipping `0`.
- Residual attribution:
  `uncorrelated_residual_or_capture_path_dominant`.
  Timing explain is only `0.728741 dB`; routing matrix explain is only
  `0.150521 dB`; source L/R correlation is `0.986751`.
- CPU split:
  driver p95 total and after 5s are both `16.6%`.
  `coreaudiod` p95 total is `35.4%`, but after 5s is `2.1%`.
- Transport/callback context:
  capture and playback rates are both about `666.809545` transfers/s.
  Callback attribution status is
  `external_process_cpu_only_hot_path_timing_absent`.

Current technical truth:
- The line is stronger as an evidence system, but not as a product candidate.
- The sustained driver CPU issue is real.
- The current evidence does not yet tell which internal callback segment owns
  that sustained CPU.
- The stored WAV failure is not explained by simple timing correction or a
  simple L/R matrix.
- Branch promotion, hardware readiness, and Traktor/timecode readiness remain
  blocked.

Next highest-value work:
- Reduce sustained driver CPU with callback/hot-path attribution.
- Generate or reuse decorrelated physical evidence to separate routing/capture
  path from stereo-music correlation.
- Keep native WAV attribution in the promotion path and tighten parity only
  after it matches Python behavior across multiple stored runs.

### 2026-06-17 Native Hot-Path Timing Attribution

- Added `opena8djcpp_hot_path_timing_analysis`.
- It reads stored hot-path timing summaries only and does not touch hardware.
- Latest selected stored diagnostic evidence:
  `local-analysis/hot-path-timing/20260617T140410Z-sampled-denom/stream-stats-summary.json`.
- Attribution:
  `fixed_queue_requeue_enqueue_dominant`.
- Average ticks in selected evidence:
  capture handler `3755.083542`, capture requeue `1825.690345`,
  playback queue `1862.696473`, playback enqueue `1493.531516`,
  playback fill `289.256253`.
- Fixed queue/requeue/enqueue to playback fill ratio: `17.914629`.

Current implication:
- The safest CPU work should target fixed transport queue/requeue/enqueue
  overhead or callback cadence before touching audio packing/math.
- The selected hot-path evidence is diagnostic and does not prove the current
  product candidate is ready or better than mainline.

### 2026-06-17 Native Quality Root-Cause Gate

- Added `opena8djcpp_quality_root_cause_analysis`.
- It reads stored evidence only:
  - route-signature summary;
  - timebase-family summary;
  - packed USB diagnostic output;
  - selected current candidate metrics;
  - hot-path timing analysis.
- Current classification:
  - `digital_payload_clean`;
  - `shared_fixture_or_capture_path_unhealthy`;
  - `candidate_physical_quality_fails`;
  - `lag_present_but_not_sufficient_explanation`;
  - `fixed_transport_queue_requeue_enqueue_cpu_suspect`.
- Key metrics:
  - USB packed output alignment `1.000000`, check errors `0`, panic flags `0`.
  - Mainline route quality/SNR in degraded shared route:
    `0.680798` / `-0.828880 dB`.
  - C++ ISO64 route quality/SNR in same degraded signature:
    `0.686712` / `-0.841473 dB`.
  - Selected C++ candidate quality/SNR:
    `0.963395` / `9.675760 dB`.
  - Timebase max lag jumps `35`; lag correction median mid-ratio improvement
    only `0.011021`.

Current implication:
- The next physical work must validate the analog route/capture path and run a
  same-session mainline/C++ comparison before any audiophile-quality claim.
- Internal packet cleanliness does not prove analog sound quality.

### 2026-06-17 Native Timecode Readiness Gate

- Added `opena8djcpp_timecode_readiness_gate`.
- It aggregates existing offline timecode/DVS evidence and the promotion
  evaluator's physical Traktor status.
- Current result:
  - offline timecode pass: `true`;
  - matrix failures: `0`;
  - timecode signal rows: `8`;
  - DVS packet rows: `24`;
  - prepared transport profile/deck rows: `12`;
  - physical status: `BLOCKED_UNVALIDATED_DVS`;
  - product timecode ready: `false`.

Current implication:
- C++ has useful synthetic DVS/timecode coverage, but it still has no physical
  Traktor/timecode-vinyl lock evidence. Timecode readiness remains blocked.

### 2026-06-17 Prepared Transport Migration Gate

- Added `opena8djcpp_prepared_transport_migration_gate`.
- It aggregates existing prepared transport, packet, routing/timecode,
  recovery, scheduler, runtime bridge, hot-path timing, root-cause, product
  comparison, and promotion evidence.
- Current focused result:
  - migration candidate supported: `true`;
  - product ready: `false`;
  - branch promotion supported: `false`;
  - physical A/B required before claim: `true`;
  - fixed queue/requeue/enqueue to playback-fill ratio: `17.914629`.

Current implication:
- Prepared transport is now the objectively supported next performance
  direction, but only as an offline migration candidate.
- It does not prove audiophile quality, Traktor/timecode vinyl readiness, or
  superiority over mainline.

### 2026-06-17 Prepared Transport Pressure Gate

- Added `opena8djcpp_prepared_transport_pressure_gate`.
- It runs a long offline pressure model over:
  - sample rates `44100` and `48000`;
  - decks A/B/C/D;
  - 10 seconds per row;
  - `3,684,000` total S24 frames.
- Current focused result:
  - failures: `0`;
  - HAL steady requeues: `0`;
  - fallback allocations: `0`;
  - ring overruns/underruns: `0`;
  - capture/playback mismatches: `0`;
  - packet check errors/panic flags/output overflows: `0`.

Current implication:
- Prepared transport migration is now backed by longer offline pressure
  evidence at the mandatory 44.1/48 kHz rates.
- This still does not touch hardware and still cannot prove physical sound
  quality or CPU superiority.

### 2026-06-17 DriverKit Runtime Contract V2

- Strengthened `opena8djcpp_driverkit_runtime_contract`.
- The DriverKit shell now clears stream configuration on driver stop, so a
  restart cannot reuse stale stream state without reconfiguration.
- The runtime contract now covers:
  - invalid sample rate, buffer, and transport configs;
  - stop/restart/reconfigure lifecycle;
  - 44.1/48 kHz pressure through `AudioDriverSkeleton`;
  - `184,200` total runtime-shell frames;
  - zero HAL steady requeues and zero fallback allocations.

Current implication:
- The offline DriverKit shell is closer to a real lifecycle boundary.
- It still is not a built/installed dext and cannot prove hardware behavior.

### 2026-06-17 Capture Route Health Gate

- Added `opena8djcpp_capture_route_health_gate`.
- This is an offline diagnostic over existing evidence only; it touches no
  hardware, CoreAudio, USB, driver install, defaults, or services.
- It formalizes a key distinction:
  - diagnostic gate execution can PASS;
  - measurement validity for promotion can still be false.
- Current evidence reports:
  - digital payload clean;
  - shared capture route unhealthy;
  - candidate capture below reference threshold;
  - capture lag unstable;
  - candidate not better than mainline reference.

Current implication:
- The next physical run must first revalidate the capture route under lock.
- Existing physical quality evidence cannot support branch promotion or claims
  of better sound quality, timecode readiness, or lower CPU.

### 2026-06-17 Prepared Hot Path Batch Publication

- Optimized `SpscFrameRing::push_many` and `pop_many` to publish indices once
  per batch instead of once per frame.
- Added offline publication counters to the prepared transport model.
- Added `opena8djcpp_driverkit_prepared_hotpath_contract`.
- The new contract runs 10 seconds at 44.1 kHz and 48 kHz through the
  DriverKit shell with iso8 batches:
  - `921,000` total frames;
  - zero HAL steady requeues;
  - zero fallback allocations;
  - zero ring faults;
  - four ring publications per period;
  - 8x fewer index publications than scalar per-frame publication.

Current implication:
- This directly targets the fixed requeue/enqueue CPU suspect in offline
  architecture.
- It is still not a physical CPU win until the runtime candidate is tested
  under lock against mainline.

### 2026-06-17 Physical Evidence Frontier

- Added `opena8djcpp_physical_evidence_frontier`.
- The tool scans existing `local-analysis/soundcheck/**/metrics.json` and
  matching `cpu-profile.tsv` evidence.
- Current scan:
  - `61` physical/soundcheck runs;
  - `12` inferred families;
  - `0` quality-passing runs;
  - `0` strict CPU-passing runs;
  - `0` product-candidate runs.
- Best existing quality run:
  - `local-analysis/soundcheck/20260616-capture-detail-irig-pairA-8s-cpp-hal`;
  - quality alignment `0.978050`;
  - SNR floor `9.845114 dB`;
  - driver p95 `36.7%`.

Current implication:
- Existing physical evidence contains no run that can prove C++ superiority.
- The next useful physical step remains capture-route revalidation plus a
  same-session A/B, not branch promotion.

### 2026-06-17 Known-Good Route Soundcheck Harness

- Added `scripts/run-known-good-route-soundcheck`.
- Extended `src/tools/audio-wav-play.c` so locked diagnostics can play a WAV
  through an explicitly selected non-Audio8 CoreAudio output device without
  changing defaults and without changing that device's sample rate.
- The harness records the physical route through the selected capture device,
  normally `iRig Stream`, then runs the existing soundcheck analyzer.
- Safety rules encoded in the wrapper:
  - it refuses OpenA8DJ / Open Audio 8 DJ as the known-good output source;
  - it requires explicit output device or UID and explicit capture device;
  - it uses the global hardware lock;
  - it does not install, load, unload, restart, reset USB, or change defaults.

Current implication:
- The next physical quality step can now isolate the shared mixer/REC OUT ->
  iRig route from the Audio 8 DJ output path.
- This harness has not been physically executed yet. It adds measurement
  control, not product readiness.

### 2026-06-17 Physical Superiority Window Runner

- Added `scripts/run-physical-superiority-window`.
- The runner is dry-run by default and refuses physical/system work unless
  `--execute` is present.
- Execution order is now explicit:
  - known-good non-Audio8 route -> iRig validation;
  - mainline HAL candidate safety install/reload;
  - mainline Audio 8 DJ music soundcheck with CPU and stream evidence;
  - mainline unload;
  - C++ HAL candidate safety install/reload;
  - C++ Audio 8 DJ music soundcheck with CPU and stream evidence;
  - native C++ WAV quality analysis for both captures;
  - same-session mainline/C++ physical comparison;
  - promotion-readiness evaluator against the C++ evidence;
  - unload active candidate unless explicitly left loaded.
- Safety encoded:
  - global hardware lock held for the whole window;
  - explicit capture device and explicit known-good source required;
  - explicit mainline and C++ HAL bundles required for full execution;
  - OpenA8DJ / Audio 8 refused as the known-good source;
  - no default-device changes, USB reset, reboot, Traktor launch, or arbitrary
    sample-rate changes.

Current implication:
- There is now a single auditable command shape for the next physical evidence
  window, and it requires same-session mainline/C++ A/B evidence for any
  superiority claim.
- `--candidate-only` is diagnostic-only and intentionally writes a blocked
  same-session comparison result.
- `--skip-known-good` is diagnostic-only and blocks a successful runner exit;
  route health is mandatory for a promotion-quality physical window.
- The promotion evaluator now requires the same window's
  `same-session-physical-compare.json`, so stale or fixed-reference evidence
  cannot support branch promotion.
- No physical route, HAL install, playback, capture, or CoreAudio restart was
  run by adding the runner.
- Product readiness remains blocked until the runner produces passing
  same-session evidence and the result beats mainline thresholds.

### 2026-06-17 Physical Window Preflight

- Added `scripts/physical-window-preflight`.
- The physical superiority runner now calls this read-only preflight before
  acquiring the hardware lock or installing/reloading HAL candidates.
- It checks:
  - `iRig Stream` visible in CoreAudio with capture channels;
  - `Audio 8 DJ` visible on USB as Native Instruments VID/PID;
  - mainline and C++ HAL bundle paths exist;
  - reference/music fixtures exist;
  - known-good output is explicit, visible, and not Audio 8;
  - hardware lock is free.
- Current observed state:
  - `iRig Stream` visible in CoreAudio and USB;
  - `Audio 8 DJ` visible on USB;
  - `Open Audio 8 DJ` absent from CoreAudio until a HAL candidate is loaded;
  - C++ and mainline HAL bundles are present.

Current implication:
- The next hardware window can fail fast before lock if the capture route,
  USB device, bundles, fixture files, or known-good output are missing.
- Preflight PASS is not evidence of route health. The locked known-good route
  capture remains mandatory before judging mainline or C++ audio quality.

### 2026-06-17 Route-Only iRig Output Attempt

- Ran a lock-gated route-only check with `iRig Stream` as both explicit
  non-Audio8 output and capture device.
- No HAL candidate was installed, loaded, unloaded, or reloaded.
- The runner played and recorded successfully, but the capture was essentially
  idle/noise instead of the deterministic reference:
  - captured RMS about `-68.1 dBFS`;
  - reference RMS about `-21.2/-22.1 dBFS`;
  - alignment score about `0.004`;
  - SNR about `-36.4 dB`.

Current implication:
- The current system sees iRig and Audio 8 DJ, but `iRig Stream` output is not
  a valid known-good source into the shared iRig capture route.
- Do not run or trust a promotion-quality mainline/C++ physical A/B until a
  real non-Audio8 source is physically routed into the same mixer/REC OUT ->
  iRig path and the known-good route gate passes.
- This is a route/cabling/source blocker, not evidence that C++ quality is
  worse or better than mainline.

### 2026-06-17 Diagnostic Mainline Physical Attempt

- Ran a lock-gated diagnostic physical window with `--skip-known-good`.
- Mainline HAL candidate safety passed, then mainline Pair A soundcheck failed
  before any C++ candidate was loaded:
  - `quality_alignment_score=0.125194`;
  - `analog_snr_db=-12.77`;
  - `lag_jumps_gt_2_frames=40`;
  - `mid_band_cpu_corr=0.969575 source=opena8dj_driver`;
  - `capture_clipped_frames=0`.
- Because known-good route validation was skipped and mainline failed, no
  same-session C++ comparison exists from this window.
- The runner's normal final cleanup left mainline HAL loaded because
  `audio-stack-guard --recover --unload-opena8dj` only unloads during recovery
  and the stack health check passed. A lock-gated force-unload then moved the
  active HAL out of `/Library/Audio/Plug-Ins/HAL` and killed the
  `OpenA8DJ.driver` process.

Current implication:
- Current hardware evidence still blocks C++ promotion, but it also shows the
  mainline candidate is not currently passing the physical quality gate.
- Fixed `scripts/run-physical-superiority-window` cleanup to force-unload the
  active HAL when `--leave-loaded` is absent.
- Do not continue to C++ superiority claims until the known-good route is
  valid and the runner completes same-session mainline/C++ evidence.

### 2026-06-17 C++ Bundle And Direct USB Diagnostics

- Found that `build/OpenA8DJ.driver` could exist without
  `Contents/MacOS/OpenA8DJHAL`.
- Added `scripts/check-hal-bundle-complete` and made the offline gate build
  and verify the HAL bundle.
- Split Makefile flag stamps so rebuilding `build/opena8dj-usb-play` with
  diagnostic HAL flags does not delete the HAL executable.
- After `make hal`, the C++ HAL bundle loads and enumerates `Open Audio 8 DJ`
  as `8 in / 8 out`.
- C++ candidate-only physical diagnostic still fails quality:
  - `quality_alignment_score=0.074422`;
  - `analog_snr_db=-7.07`;
  - `lag_jumps_gt_2_frames=37`;
  - `mid_band_cpu_corr=0.474238 source=coreaudiod`.
- Direct USB playback diagnostics, with no HAL install/load, show internally
  perfect USB payload (`alignment=1.0`, check errors `0`, panic flags `0`) but
  failing external iRig capture (`quality_alignment_score` about `0.72-0.74`).

Current implication:
- C++ has moved from "bundle not executable" to "physically loadable but
  quality-failing".
- Packet packing is not the current dominant explanation; direct USB payload
  evidence is clean.
- The next meaningful work is to separate physical route/analog capture
  failure from HAL runtime scheduling, then improve the C++ HAL only against
  a validated capture path.

## 2026-06-17 Expanded C++ Capture Forensics

- Integrated Lorentz's read-only signal-forensics conclusion into the offline
  gate surface.
- `opena8djcpp_physical_capture_forensics` now scans:
  - `local-analysis/soundcheck`;
  - `local-analysis/direct-usb-soundcheck`;
  - `local-analysis/physical-superiority-window`.
- The analyzer remains offline-only and touches no audio devices, USB,
  CoreAudio, HAL installation, or hardware.
- Current expanded result:
  - archived WAV candidates: `81`;
  - direct USB WAV candidates: `16`;
  - physical-window WAV candidates: `3`;
  - analyzed runs: `21`;
  - strict quality candidates: `0`;
  - best direct USB run:
    `local-analysis/direct-usb-soundcheck/20260617T183629Z-pairA-12s-after-hal-fail`;
  - best direct USB quality/SNR: `0.959037` / `9.697139 dB`;
  - best physical-window quality/SNR: `0.125194` / `-12.774687 dB`.

Current implication:
- No installed package or additional analysis dependency is justified yet:
  the next decisive evidence is a valid physical route capture, not a new
  parser or DSP library.
- Packet packing is low-priority until direct USB internal payload evidence
  contradicts the current `alignment=1.0`, `check_errors=0`, `panic_flags=0`
  result.
- Promotion remains blocked because no run proves audiophile-quality external
  capture or same-session mainline/C++ superiority.

## 2026-06-17 Timecode Readiness Evidence Tightening

- `opena8djcpp_timecode_matrix` now emits schema
  `opena8djcpp.timecode-matrix.v1` and `row_count=8`.
- `opena8djcpp_timecode_readiness_gate` now exposes:
  - offline profiles: `timecode-vinyl`, `timecode-cd-line`, `phono`,
    `disabled`;
  - decks: `A/B/C/D`;
  - sample rates covered by the offline DVS path: `44100` and `48000`;
  - `mode2_packet_decode=true`;
  - `prepared_transport_path=true`.
- The same gate still reports `product_timecode_ready=false` and
  `physical_traktor_timecode_blocked=true`.
- Remaining physical requirements are now machine-readable:
  - `real_traktor_scope_lock`;
  - `physical_timecode_vinyl_decks`;
  - `same_session_mainline_cpp_dvs_comparison`;
  - `validated_capture_route`.

Current implication:
- The C++ core has stronger offline DVS/timecode evidence, but no Traktor or
  vinyl readiness claim is allowed until the physical route is valid and real
  Traktor/timecode scope evidence exists.

## 2026-06-17 Post-4118eab Physical Route State

- After commit `4118eab`, CoreAudio listed `iRig Stream` as a `2 in / 2 out`
  device at 48 kHz.
- Raw USB registry showed:
  - `iRig Stream` from IK Multimedia at 12 Mbps;
  - `Audio 8 DJ` from Native Instruments, vendor/product `17cc:1978`, at
    480 Mbps.
- A lock-gated Direct USB run was executed:
  - run:
    `local-analysis/direct-usb-soundcheck/20260617T201948Z-post4118eab-irig-route`;
  - output: Audio 8 DJ Pair A via direct USB player;
  - capture: `iRig Stream` channels `1,2`;
  - no HAL install/load, default-device change, CoreAudio restart, USB reset,
    or reboot.
- The external capture failed:
  - `quality_alignment_score=0.699393`;
  - `snr_db_min=-1.877419`;
  - residual ratios `1.908435` / `1.782892`;
  - no clipping and no lag jumps in the top-level metric.
- The same run's internal diagnostics were perfect:
  - written, consumed, and packed USB alignment `1.000000`;
  - USB check errors `0`;
  - USB panic flags `0`.
- Refreshed attribution:
  - `opena8djcpp_direct_usb_path_attribution` now sees `9` direct USB runs,
    with `5` internally clean runs whose external capture still failed.
  - Latest attribution:
    `post_usb_device_analog_or_capture_route_dominant`.
  - `opena8djcpp_capture_route_health_gate` still reports
    `measurement_valid_for_promotion=false`.

Current implication:
- The system can currently see both Audio 8 DJ and iRig, and can run a locked
  physical diagnostic, but the capture path is not valid for promotion.
- Further C++ packet-format work is low-priority unless new evidence breaks
  the current internal USB cleanliness result.
- The decisive next physical requirement remains a validated route/capture
  path followed by same-session mainline/C++ comparison.

## 2026-06-17 Capture Route Gate Tightening

- `opena8djcpp_capture_route_health_gate` now consumes
  `direct-usb-path-attribution.json`.
- It emits:
  - `direct_usb_internal_clean=true`;
  - `direct_usb_capture_failed=true`;
  - `direct_usb_capture_failed_after_clean_payload=true`;
  - `direct_usb_attribution=post_usb_device_analog_or_capture_route_dominant`.
- The gate now adds `direct_usb_capture_failed_after_clean_payload` to
  `promotion_blockers`.
- It also emits required physical experiments:
  - known-good non-Audio8 source into the same iRig capture route;
  - Audio 8 DJ direct-to-iRig without mixer/EQ if physically possible;
  - same-session mainline/C++ physical A/B on the validated route;
  - Traktor timecode vinyl scope on the validated route.

Current implication:
- A clean Direct USB payload is now integrated into the blocker model instead
  of sitting as separate context.
- The current route cannot support audiophile, timecode, CPU, or branch
  promotion claims until `direct_usb_capture_failed_after_clean_payload=false`
  and `measurement_valid_for_promotion=true`.

## 2026-06-17 QA Tightening After Metrics Review

- Independent read-only QA/Metrics review found that the first Direct USB route
  blocker integration still had four readiness risks:
  - route-health gate could consume stale Direct USB/soundcheck/physical
    evidence inside a full offline run;
  - promotion readiness did not consume route-health or Direct USB attribution
    as first-class gates;
  - top-level offline `PASS` could be misread as product readiness;
  - Direct USB metric parsing was not anchored to `latest_run`.
- Integrated fixes:
  - `scripts/run-cpp-offline-gates` now regenerates consumed physical evidence
    and Direct USB attribution before route-health evaluation.
  - `scripts/evaluate-promotion-readiness.py` now fails promotion explicitly on
    invalid capture-route measurement or Direct USB capture failure after clean
    payload.
  - `current-offline-gates.json` now separates `diagnostic_status` from
    `product_readiness_status`, `branch_promotion_allowed`, and
    `physical_measurement_valid_for_promotion`.
  - `opena8djcpp_capture_route_health_gate` now reads Direct USB metrics from
    the `latest_run` object.

Current implication:
- A stale or nested Direct USB artifact cannot silently weaken the blocker.
- Product readiness remains `NOT_READY` until route health is valid and a
  same-session mainline/C++ physical A/B passes on that valid route.

## 2026-06-17 Current Hardware/Route Context

- Global hardware lock:
  - absent at read-only inspection time;
  - no owner PID to kill or clear.
- Device visibility:
  - iRig Stream is visible over USB and CoreAudio as 2-in/2-out at 48 kHz.
  - Audio 8 DJ is visible over USB/IORegistry but is not visible as a
    CoreAudio device.
- Historical route reference:
  - The read-only mainline route proof
    `vlc-long-route-proof-20260612-163849` passes sanity guardrails:
    48 kHz, about 50 seconds, 46 active seconds, zero clipped frames, and
    healthy music-level RMS/peak values.
  - This proves the old route evidence exists and is usable as a regression
    anchor.
- Current route status:
  - `current_measurement_valid_for_promotion=false`.
  - Direct USB internal payload remains clean, but current iRig capture still
    fails after that clean payload.
  - iRig idle capture is clean enough to avoid blaming idle noise alone.
- Open gap:
  - Revalidate the live physical route with a known-good non-Audio8 source.
  - Restore or intentionally install/register the Audio 8 DJ CoreAudio path
    only in a lock-held recovery window.
  - Run same-session mainline/C++ quality and CPU comparison only after the
    route gate passes.

## 2026-06-17 HAL Safety Context

- A lock-gated C++ HAL safety window passed at
  `local-analysis/hal-candidate-safety/20260617T205049Z-cpp-candidate-safety`.
- During the loaded guard, CoreAudio enumerated:
  - `iRig Stream`, `2 in / 2 out`, 48 kHz;
  - `Open Audio 8 DJ`, `8 in / 8 out`, 48 kHz,
    UID `org.opena8dj.Audio8DJ`.
- The candidate was unloaded at the end and runtime isolation confirmed HAL
  inactive and lock absent.
- `opena8djcpp_hal_candidate_safety_gate` now converts this into offline
  readiness evidence.
- Current implication:
  - CoreAudio enumeration is recoverable through the C++ HAL safety flow.
  - No sound-quality, CPU, timecode, or branch-promotion claim follows from
    this. The dominant blocker remains live route validation and same-session
    physical A/B evidence.

## 2026-06-17 Current Physical Quality/CPU Context

- Current verified state:
  - iRig Stream is visible in CoreAudio as `2 in / 2 out` at 48 kHz after
    physical windows.
  - The global hardware lock is absent after completed runs.
  - C++ HAL safety can pass and enumerate Audio 8 DJ as 8x8 when loaded.
- Current physical evidence:
  - Same-session mainline/C++ windows now complete even when either soundcheck
    fails.
  - Both mainline and C++ are failing the current iRig physical route quality
    gates, but C++ is not objectively better.
  - C++ driver CPU p95 is roughly `22-24%` in current default/raw windows,
    versus roughly `5-6%` for mainline in the same-session windows.
  - Simple playback coalescing reduces transfer count and CPU but breaks audio
    quality, so it is not a product path.
- Open technical blocker:
  - The C++ data plane appears to be submitting playback at the capture callback
    cadence, around 1000 submissions/s in observed runs. The required fix is a
    pacing/transport redesign that keeps input/timecode support while reducing
    callback and enqueue overhead.
  - Read-only subagent review confirmed mainline obtains lower CPU primarily
    from a larger effective ISO transfer cadence (`HAL_ISO_FRAMES=64` in
    mainline versus C++ default ISO8), not from explicit scheduling or reused
    completion handlers.
  - A playback-completion-paced C++ probe reduced CPU but broke quality, so the
    next implementation must preserve the physical timing/layout assumptions
    instead of simply batching transfers.
  - A default C++ HAL run with integrated `sudo -n sample` confirms the active
    CPU path is IOUSBHost async enqueue from `handleCaptureTransfer` /
    `queuePlaybackWithRequests` and `queueCaptureTransfer`, not packet packing
    or routing. Evidence:
    `local-analysis/profiling/20260617T215041Z-default-sudo-sample/soundcheck/driver-sample/analysis.json`.
  - The same run failed physical quality badly (`quality_alignment_score=0.260184`,
    SNR about `-11.89 dB`, `lag_jumps_gt_2_frames=38`), so it is CPU attribution
    evidence only, not a better candidate.
  - Subagent Linnaeus mapped the offline prepared transport contracts to HAL
    integration points. The next real implementation should move toward prepared
    transport/DriverKit slot ownership and avoid steady HAL direct requeue work.
  - Subagent Pascal warned that a prepared-transport bridge without real
    `enqueueIORequestWithData` cadence reduction would be decorative accounting.
    In response, the HAL now has an opt-in
    `HAL_CAPTURE_PACED_PLAYBACK_REFILL=1` experiment. It keeps capture active
    for input/timecode but allows playback to refill through the independent
    playback queue path so coalesced OUT transfers can be tested without
    disabling capture-paced operation entirely.
  - The experiment compiled and full offline gates passed, but it has no
    physical quality or CPU superiority evidence yet and is not enabled by
    default.
  - Lock-gated physical testing of
    `HAL_CAPTURE_PACED_PLAYBACK_REFILL=1 HAL_PLAYBACK_COALESCE_TRANSFERS=2
    HAL_PLAYBACK_QUEUE=4` rejected that experiment: driver CPU p95 improved to
    about `18.2%`, and playback submissions fell to `3351`, but quality
    collapsed (`quality_alignment_score=0.157019`, SNR `-21.74 dB`,
    `lag_jumps_gt_2_frames=35`). The final guard unloaded the HAL and reported
    audio stack health PASS.
  - After rejecting that HAL experiment, the DriverKit offline shell was
    reinforced so it now validates stream memory descriptors, monotonic zero
    timestamps, and stopped-only configuration changes. The runtime contract
    reports `io_memory_descriptors=5`, `io_memory_total_bytes=4096`,
    `zero_timestamp_updates=2`, `zero_timestamp_regressions=0`,
    `configuration_changes=1`, `rejected_configuration_changes=1`, and both
    44.1 kHz / 48 kHz pressure rows PASS. Full offline gates still pass
    (`43/43` Debug, `44/44` Release, evidence schema PASS).
  - This DriverKit reinforcement improves architectural correctness but does
    not change product readiness. The next product-critical implementation must
    reduce real IOUSBHost enqueue cadence without changing the physical timing
    contract that the hardware/capture route needs for clean audio.
  - Added an offline saved-tone audiophile gate. It reports the saved C++ tone
    candidate is not worse than the selected 0.3.24 mainline tone on THD and
    sideband ratio (`candidate sideband=0.010323`, THD `0.000451`, clipped
    frames `0`), but `physical_measurement_valid_for_promotion=false`; this is
    extra distortion evidence only, not a product claim.
  - Bacon's read-only scheduler review converged with the existing evidence:
    the next low-CPU design must preserve logical ISO8 audio slots while
    batching real USB submit/enqueue work under that layer. Do not repeat
    independent HAL coalescing/refill probes unless new evidence changes the
    risk calculus.
  - The prepared slot scheduler contract now models that requirement directly.
    The safe batch row keeps `logical_audio_periods=256` and
    `backend_slot_completions=512` while reducing modeled USB submits to `66`
    (`usb_submit_reduction_ratio=8`) with zero HAL requeues, fallback
    allocations, logical gap violations, or slot order errors. Migration PASS
    now includes `logical_iso8_usb_submit_batching_supported=PASS`.
  - A pure C++ fake runtime adapter now wraps the prepared scheduler and
    exposes the same runtime-facing counters. Its stable row keeps
    `logical_audio_periods=256`, `backend_slot_completions=512`,
    `usb_submit_calls=66`, and `usb_submit_reduction_ratio=8`; negative rows
    reject unbatched submits, logical gaps, slot-order errors, HAL requeues,
    and fallback allocations. The migration gate now requires this adapter via
    `runtime_adapter_batched_submit_counters_exposed=PASS`.
  - The prepared USB submit planner now models the descriptor content behind
    that counter contract. Its stable row maps `528` logical capture/playback
    slots into `66` submit descriptors (`33` capture, `33` playback), with
    `stable_total_bytes=185856`, zero partial submits, zero descriptor
    overflows, zero ordering errors, and zero timestamp regressions. The
    migration gate now requires `usb_submit_descriptor_plan_safe=PASS`.
  - The descriptor plan is now tied to actual Mode2 payload validation.
    `opena8djcpp_usb_submit_payload_contract` packs and decodes every prepared
    descriptor offline and requires `66` descriptors, `185856` bytes, `5808`
    Mode2 payload frames, zero check errors, zero panic flags, zero output
    overflows, zero prefix mismatches, zero descriptor mismatches, zero
    direction-order errors, and zero timestamp mismatches. This corrected the
    earlier ambiguity between `528` logical ISO8 slots and `5808` Mode2 payload
    frames.
  - The DriverKit skeleton now binds stream lifecycle to the same prepared USB
    submit planner. `opena8djcpp_driverkit_usb_submit_binding_contract` runs
    `256` ISO8 periods through the DriverKit shell, validates transport frame
    identity, then requires the DriverKit-facing binding to expose `528`
    logical slots, `66` submit calls/descriptors, `185856` bytes, `5808` Mode2
    payload frames, and zero Mode2/payload/timestamp/direction errors. The
    migration gate now requires `driverkit_usb_submit_binding_safe=PASS`.
  - The DriverKit USB boundary now has a request lifecycle contract.
    `opena8djcpp_driverkit_usb_request_lifecycle_contract` feeds the
    DriverKit-generated descriptors into a preallocated request pool and
    requires `66` submit calls, `66` completions, `66` recycles, max `4` live
    requests, `185856` completed bytes, `5808` completed frames, zero fallback
    allocations, zero invalid completions, zero stale completions, and explicit
    rejection of pool-pressure/stale-handle scenarios. The migration gate now
    requires `driverkit_usb_request_lifecycle_safe=PASS`.
  - Current full offline evidence after this integration: Debug CTest `49/49`,
    Release CTest `50/50`, evidence schema `required_files=50`,
    `missing_files=0`. Promotion remains blocked.
  - The DriverKit skeleton now owns the modeled USB request pool at runtime and
    distinguishes normal completion from shutdown cancellation. The new
    `opena8djcpp_driverkit_usb_request_shutdown_contract` stops with `3` live
    requests, cancels all `3`, leaves `0` live requests, rejects a late
    completion after cancel, and proves restart-after-cancel safety. Migration
    now requires `driverkit_usb_request_shutdown_safe=PASS`.
  - Current full offline evidence after this shutdown integration: Debug CTest
    `50/50`, Release CTest `51/51`, evidence schema `required_files=51`,
    `missing_files=0`. Promotion remains blocked.
  - Added an offline physical-window readiness gate. It consumes only existing
    evidence and reports:
    `ready_for_route_revalidation_window=true`,
    `ready_for_product_physical_ab=false`, and
    `ready_for_branch_promotion=false`.
    The only allowed next window type is route revalidation, not product A/B.
  - Current full offline evidence after this gate: Debug CTest `51/51`,
    Release CTest `52/52`, evidence schema `required_files=52`,
    `missing_files=0`. The run touched no hardware, USB, CoreAudio, driver
    install/activation, service restart, default device, sample-rate, or buffer
    setting.
  - Current route-revalidation inspection sees `iRig Stream`,
    `MacBook Air Microphone`, and `MacBook Air Speakers`. The built-in speakers
    are now rejected as a known-good route source by default because an acoustic
    path cannot validate the wired mixer/REC OUT -> iRig route. The runner
    blocks this case before lock acquisition.
  - The physical-window readiness gate now uses structured evidence JSON field
    reads for critical decisions instead of broad text matching. Current full
    offline evidence after this hardening: Debug CTest `52/52`, Release CTest
    `53/53`, evidence schema `required_files=52`, `missing_files=0`. Promotion
    remains blocked.
  - The offline evidence schema check now also uses structured reads for the
    summary and manifest fields that control no-touch safety, promotion
    blockers, prepared transport counters, DriverKit USB request counters, and
    physical-window readiness. Current full offline evidence remains Debug
    CTest `52/52`, Release CTest `53/53`, evidence schema `required_files=52`,
    `missing_files=0`.
  - The evidence JSON parser contract is now a first-class offline artifact:
    `local-analysis/cpp-offline/evidence-json-contract.json`. Current full
    offline evidence after adding the artifact: Debug CTest `52/52`, Release
    CTest `53/53`, evidence schema `required_files=53`, `missing_files=0`.
  - Promotion readiness now has an explicit single-bundle gate. Music, CPU,
    tone, latency, marker, USB integrity, matrix, and same-session comparison
    evidence must all come from the same lock-gated
    `local-analysis/physical-superiority-window/<id>` tree. Current evidence
    fails this correctly with `product_window=null`.
  - Diagnostic PASS semantics are now machine-checked. The new
    `opena8djcpp_diagnostic_pass_semantics_gate` requires analyzer PASS
    artifacts to carry explicit non-product-readiness fields for soundcheck,
    physical comparison, offline timecode, migration, physical-window
    readiness, and promotion evaluation. Current full offline evidence after
    integration: Debug CTest `53/53`, Release CTest `54/54`, evidence schema
    `required_files=54`, `missing_files=0`.
  - Audiophile quality claims now have an explicit guard. The new
    `opena8djcpp_product_quality_claim_gate` requires real-music superiority,
    current route-valid tone evidence, route validity, and branch-promotion
    allowance before any quality claim can be allowed. Current full offline
    evidence: Debug CTest `54/54`, Release CTest `55/55`, evidence schema
    `required_files=55`, `missing_files=0`; `quality_claim_allowed=false`.
  - Evidence freshness is now a readiness concern. A subagent audit found that
    a stale `base_commit` could leave schema PASS attached to an older commit.
    The new provenance gate requires `current-offline-gates.json` to match HEAD
    and a clean claimable worktree before current-candidate claims are allowed.
  - HAL runtime transport claims are now guarded separately. The new
    `opena8djcpp_hal_transport_runtime_gate` reads the HAL source plus existing
    evidence and reports that the prepared transport model supports an `8x`
    submit reduction offline, but the loadable HAL still performs direct
    IOUSBHost enqueue work and has no integrated prepared-submit runtime. This
    blocks any CPU or audiophile superiority claim until actual runtime enqueue
    cadence is reduced.
- Toolchain blocker:
  - This machine currently has Command Line Tools selected at
    `/Library/Developer/CommandLineTools`; `xcrun --sdk driverkit
    --show-sdk-path` cannot locate the DriverKit SDK. A real dext build needs
    full Xcode/DriverKit SDK or an equivalent configured build host.
- Operational blocker:
  - Post-reboot automatic recovery/login back into Codex did not work in the
    earlier reboot attempt and must be fixed separately before relying on
    unattended reboot recovery.
- Readiness truth:
  - Not ready for branch promotion.
  - Not ready to claim audiophile quality.
  - Not ready to claim better CPU/performance than mainline.
  - Timecode vinyl remains unproven physically.
  - A new opt-in HAL capture batching path exists:
    `HAL_ISO_FRAMES=8 HAL_CAPTURE_ISO_FRAMES=64 HAL_CAPTURE_QUEUE=8
    HAL_PLAYBACK_ISO_FRAMES=8`. It keeps the default logical ISO8 cadence and
    only batches physical capture USB transfers when explicitly requested.
    This is a diagnostic/performance candidate, not product proof.
  - `opena8djcpp_hal_logical_capture_batching_contract` guards that the build
    exposes the capture physical-transfer flag, defaults remain legacy-safe,
    capture pool/queue/timing use the physical size, and capture-paced playback
    does not truncate larger capture completions.
  - `opena8djcpp_hal_runtime_geometry_observability_contract` guards that HAL
    and `opena8dj-control` expose the active runtime geometry in stream stats:
    `logicalIsoFramesPerTransfer`, `captureIsoFramesPerTransfer`,
    `playbackBaseIsoFramesPerTransfer`, `playbackIsoFramesPerTransfer`,
    `playbackCoalesceTransfers`, `captureQueueDepth`, and
    `playbackQueueTarget`.
  - `opena8djcpp_hal_transport_runtime_gate` now also requires runtime submit
    observability: capture/playback submit counters must exist in HAL, be
    printed by `opena8dj-control`, be captured by `run-soundcheck`, and be
    summarized by `scripts/analyze-stream-stats.py`.
  - Lock-gated HAL safety evidence now exists for the opt-in capture-batched
    candidate. Run
    `local-analysis/hal-candidate-safety/20260618T012357Z-capture-iso64-safety`
    loaded the candidate, enumerated `Open Audio 8 DJ` as 8 in / 8 out,
    preserved `iRig Stream`, then unloaded the HAL and left CoreAudio clean.
    This is install/reload safety only, not audio quality or CPU superiority.
