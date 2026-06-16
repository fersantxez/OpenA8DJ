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
- Offline window-trace analysis of the failed physical music run shows
  `lag_jumps_gt_2_frames=24`, local lag from `3` to `-27` frames, and only
  `2.1%` median mid-band residual improvement after per-window lag correction.
  Timing/cadence is real, but the music-quality failure is not explained by
  timebase alone.

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
- Physical validation must follow `docs/PHYSICAL_TEST_WINDOW_PLAN.md`, use the
  global hardware lock, and avoid default-device/sample-rate/buffer changes
  unless explicitly part of a documented window.
