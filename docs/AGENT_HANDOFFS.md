# Agent Handoffs

Date: 2026-06-16

## Global Warning Given To Agents

PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj` o `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo puedes escribir en `/Users/fer/dev/audio8djcpp`. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorizacion de ventana.

## Agents

| Agent | Mission | Output |
| --- | --- | --- |
| Mainline C Archaeologist | Extract proven behavior from C/Objective-C mainline in read-only mode. | `docs/MAINLINE_LEARNINGS.md` |
| Rust Oracle Analyst | Extract gates, metrics, analyzers, and oracle behavior from Rust in read-only mode. | `docs/RUST_LEARNINGS.md` |
| Real-Time Performance Engineer | Define callback-safe data-plane architecture. | `docs/REALTIME_DESIGN.md` |
| QA/Metrics Engineer | Define offline gates, evidence format, thresholds, and readiness checklist. | `docs/SUCCESS_METRICS.md`, `docs/TEST_PLAN.md` |
| Build/Release Engineer | Define reproducible build strategy without system mutation. | `docs/BUILD.md` |
| iRig/Riff Recovery Subagent | Recover `iRig Stream` in CoreAudio under hardware lock after user authorization for USB/audio resets. | `local-analysis/hardware-quality/20260616-155613-riff-recovery-subagent` |
| Ramanujan | Read-only offline performance inspection for CPU/jitter optimization opportunities. | findings integrated in this document |
| Leibniz | Implement stronger offline timeline/jitter model. | `tools/jitter_model.cpp`, `local-analysis/cpp-offline/jitter-model.json` |
| Lorentz | Read-only HAL/USB CPU audit for cadence-preserving optimizations. | findings integrated in this document |
| Linnaeus | Read-only analysis of existing physical music failure evidence. | findings integrated in this document |
| Lagrange | Read-only promotion/readiness gap audit. | findings integrated in this document |

## Findings Integrated

### Mainline C Archaeologist

- The current mainline checkout has branch head `08745b7`, but working-tree docs/code contain later 0.3.135-era findings; use file contents and quality reports, not commit history alone.
- Mainline behavior to preserve includes CAIAQ vendor/product `17cc:1978`, control endpoints `0x01/0x81`, isochronous capture/playback `0x82/0x06`, Mode 2 32-byte groups, signed 24-bit big-endian samples, and 8-channel A/B/C/D surface.
- The safe current baseline is `0.3.135` as a no-iRig/software-only anchor, not a final audiophile physical-listening baseline.
- Risk: clean HAL/USB counters do not prove analog sound quality; later physical evidence and listening still matter.
- Next action: implement C++ packet pack/unpack tests against Mode 2 fixtures before any DriverKit runtime work.

### Rust Oracle Analyst

- Rust provides the useful oracle shape: PM success metrics, no-iRig software gate, timecode analyzer, DVS matrix smoke, pack bench, routing/input decode tests, and explicit blocked statuses.
- Rust candidate `3429796` is useful as a gate reference, not as permission to import Rust runtime.
- C++ should reuse Rust-style PASS/FAIL/BLOCKED semantics and compare packet/routing/timecode outputs where practical.
- Risk: Rust real-music locked runs do not automatically validate the C++ data plane.
- Next action: port the smallest high-value Rust oracle cases into C++ fixtures, starting with pack-sim matrix and routing.

### Real-Time Performance Engineer

- Data plane must be allocation-free and bounded after stream start.
- Callback and USB completion paths may do fixed copies/conversions, atomics, SPSC operations, timestamp arithmetic, and counter increments only.
- Separate Core Audio callback, USB stream queue, and control/diagnostic thread ownership.
- Risk: any Objective-C allocation, logging, IPC, device discovery, or config mutation in callback/completion path is a product bug.
- Next action: introduce preallocated ring/packet abstractions before USB transport code.

### QA/Metrics Engineer

- Initial status language is `PASS`, `FAIL`, and blocked statuses such as `BLOCKED_HARDWARE_FORBIDDEN` and `BLOCKED_INSTALL_WINDOW`.
- Offline gates must cover packet fidelity, routing, sample-rate policy, timecode/DVS simulation, throughput, jitter proxies, and evidence JSON.
- Readiness requires comparison against mainline C and Rust oracle where practical.
- Risk: "compiles" is only a build-health signal, not a product metric.
- Next action: add machine-readable evidence output for CTest/offline gates.

### Build/Release Engineer

- CMake is the canonical build; Xcode should be generated, not hand-maintained as the source of truth.
- Default build/test must not run `sudo`, install HAL/dext, use `launchctl`, touch `coreaudiod`, claim USB, change defaults, or play/record audio.
- DriverKit stays prepared but not installed or loaded in the offline phase.
- Risk: package/install targets must remain opt-in and lock-gated later.
- Next action: add CI-style presets once the core has packet/routing gates beyond the initial contract test.

### iRig/Riff Recovery Subagent

- Mission: recover `iRig Stream` as a CoreAudio `2 in / 2 out` capture device
  without changing defaults, sample rate, buffer size, installing drivers, or
  rebooting.
- User later expanded authorization to include Audio 8 DJ reset and broader
  audio/USB resets if needed.
- Evidence: `local-analysis/hardware-quality/20260616-155613-riff-recovery-subagent`.
- Result: FAIL for iRig CoreAudio recovery.
- Objective cause from kernel logs:
  - macOS enumerated `0x1963/0059/0110 (iRig Stream / 2)` at `12 Mbps`;
  - kernel reported `device functionality blocked by transport restrictions`;
  - `iRig Stream@01100000` was not registered for matching.
- Actions: exact iRig reset, iRig reenumeration, authorized Audio 8 DJ reset,
  and controlled service restart attempts. Audio 8 DJ survived and remained
  visible as `Open Audio 8 DJ`, `8 in / 8 out`, `48000`.
- Risk: iRig is blocked by macOS accessory/transport policy before CoreAudio
  matching, so analog loopback quality is blocked until the accessory is
  authorized or reconnected in an allowed state.
- Next action: user must allow the accessory/port in macOS Privacy & Security
  accessory settings or accept the accessory prompt with the session unlocked;
  then rerun iRig presence verification under lock.

### Ramanujan

- Mission: inspect C++ core/tools/docs read-only for offline CPU/jitter
  optimization opportunities.
- Result: completed without edits or hardware/CoreAudio/USB commands.
- Key findings:
  - `tools/jitter_model.cpp` is too simple for the physical
    `lag_jumps_gt_2_frames=24` failure; add drift, callback jitter, stale/future
    gap, and phase-continuity scenarios.
  - `decode_mode2_usb_bytes_into` used per-byte modulo operations in the hot
    decode loop; replacing them with bounded counters should reduce CPU and
    variance.
  - `Mode2OutputPacker::fill_into` still has per-byte group decisions and is a
    future pack-side optimization target.
  - `tools/offline_bench.cpp` measured the allocating decode wrapper, not only
    the preallocated real-time decode path.
  - `float_to_s24` and non-identity routing should get dedicated benchmarks
    before any low-resource claim.
- Integrated actions:
  - `decode_mode2_usb_bytes_into` now uses transfer/group counters instead of
    per-byte modulo.
  - `opena8djcpp_offline_bench` now reports `decode_into_mib_s` separately from
    `decode_allocating_mib_s`.
  - `core_contract_tests` now compares preallocated decode against the
    allocating wrapper for transfer sizes `48`, `80`, `352` and start bytes
    `0..5`.
- Evidence:
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `docs/TEST_EVIDENCE.md`
- Risk:
  - This improves offline evidence and removes hot-loop arithmetic, but it does
    not address physical music SNR/noise, runtime CoreAudio CPU, or Traktor DVS
    evidence by itself.

### Leibniz

- Mission: implement stronger offline timeline/jitter model in
  `tools/jitter_model.cpp`.
- Status: completed.
- Write ownership: `tools/jitter_model.cpp` and minimal docs only.
- Result:
  - `jitter-model.json` expanded from 2 rows to 8 rows.
  - New metrics include `lag_jumps_gt_2_frames`, `timeline_resets`,
    `elastic_drop_frames`, `elastic_replay_frames`, `phase_discontinuities`,
    and `regressions`.
  - Latest offline gate passed with `failures=0`, modeled lag jumps `4`,
    timeline resets `4`, elastic drops/replays `172/82`, phase
    discontinuities `0`, and regressions `0`.
- Evidence:
  - `local-analysis/cpp-offline/jitter-model.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`
- Risk:
  - Must remain offline-only and must not touch CoreAudio/USB/HAL.

### Linnaeus

- Mission: analyze existing physical music failure evidence read-only and rank
  likely causes for low SNR, high quiet noise, and lag jumps.
- Status: completed.
- Result:
  - Most likely issue class: timing/cadence instability in real music, supported
    by `lag_jumps_gt_2_frames=24`, lag from `3` to `-27` frames, and initial
    stream-stats `ERROR` rows before OK samples.
  - Second likely issue class: broadband analog/capture residual, supported by
    `snr_db_min=8.93`, `quiet_mid_band_noise_dbfs=-31.17`,
    `mid_band_residual_ratio=1.379896`, and `high_band_residual_ratio=1.347577`
    despite no clipping or click outliers.
  - Existing tone success and simulated oracle weaken byte-order/start-byte as
    the current dominant explanation.
  - Recommended offline follow-up: window trace comparing raw and per-window
    lag-corrected residuals.
- Integrated action:
  - Added `scripts/analyze-soundcheck-window-trace.py`.
  - Generated
    `local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4/window-trace.json`.
  - Local lag correction improved median mid-band residual by only `2.1%`,
    which means timing is real but probably not the whole music-quality failure.
- Risk:
  - No new physical evidence can be collected while the hardware lock is owned
    by the mainline autonomous supervisor.

### Lagrange

- Mission: audit current C++ candidate readiness gaps read-only, without
  writing files or touching hardware/CoreAudio/USB.
- Status: completed and closed.
- Result:
  - Confirmed promotion blockers are objective, not subjective: physical music
    quality fails, runtime CPU does not beat mainline, and Traktor/timecode
    vinyl has no physical lock evidence.
  - Identified additional non-promotion gaps: DriverKit shell was not yet a
    real dext driver, full 0.3.25 Traktor-facing parity was not proven, cadence
    and timing were still insufficiently explained, and the candidate still
    needed a reproducible commit.
- Integrated action:
  - Added pure C++ input-profile contracts for playback/timecode-vinyl/CD-line/
    phono, including CAIAQ input mode, decode enablement, software lock, ground
    lift intent, and identity source map.
  - Added `opena8djcpp_timecode_signal_analysis` using Rust-oracle thresholds:
    RMS, balance, frequency error, jitter p95, absolute correlation, and
    clipping.
  - Candidate was later frozen as commit `837461c`.
- Evidence:
  - `local-analysis/cpp-offline/timecode-matrix.json`
  - `local-analysis/cpp-offline/timecode-signal-analysis.json`
  - `local-analysis/promotion-readiness-current.json`
- Risk:
  - These additions strengthen offline DVS contracts only. They do not replace
    physical Traktor scope/lock, iRig music capture, runtime CPU, or a real
    DriverKit dext.

### Maxwell

- Mission: compare C++ HAL transport behavior against mainline read-only and
  identify high-value divergences after the ISO64 C++ physical regression.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed and integrated by architect.
- Result:
  - Confirmed ISO64/8/8/prefetch64 matched the mainline baseline profile, but
    C++ physical evidence rejected it with the current lifecycle code.
  - Identified missing lifecycle parity in C++: transfer-pool cursor selection,
    optional playback request coalescing, and configurable capture/playback
    queue ordering.
  - Recommended keeping start byte `4`, check offset `8`, big-endian packing,
    and gain `0.50` while lifecycle parity is tested.
- Integrated action:
  - Added build knobs:
    `HAL_PLAYBACK_COALESCE_TRANSFERS`,
    `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE`, and
    `HAL_TRANSFER_POOL_CURSOR`.
  - Added `queueCapturePacedPlaybackWithRequests` and pending playback request
    reset state in the C++ HAL.
  - Kept new knobs neutral by default pending locked physical evidence.
- Evidence:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-current.json`
- Risk:
  - This work prepares a better-controlled physical candidate. It does not by
    itself improve the failed analog music quality, runtime CPU, or physical
    Traktor/timecode evidence.

### Averroes

- Mission: read-only mainline CPU/quality archaeology sidecar after several
  C++ HAL lifecycle variants failed physical or safety gates.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorizacion de ventana."
- Status: completed.
- Result:
  - Confirmed mainline had no new committed changes beyond `08745b7`; useful
    evidence is in the dirty 0.3.135 worktree and local docs.
  - Ranked high-priority unported items:
    background preopen / stop-isoc / stop grace lifecycle, atomic
    `outputFramesWritten`, fast output prefetch clear, hot stream stats gating,
    reset audio params before stream, reusable ISO completion handlers, and
    strict/legacy transfer-pool variants.
  - Recommended not retesting already rejected C++ variants: ISO64/q8,
    input-decode active gating, queue-before, and transfer-pool cursor.
- Integrated action:
  - Ported lifecycle flags, fast prefetch clear, and atomic output-write stats
    behind build flags.
  - Physical evidence rejected lifecycle-preopen defaults and fastclear/write
    stats defaults in C++; they remain available only behind explicit flags.
- Evidence:
  - `local-analysis/soundcheck/20260616-lifecycle-preopen-irig-pairA-16s-cpp-hal`
  - `local-analysis/soundcheck/20260616-fastclear-writestats-irig-pairA-16s-cpp-hal`
- Risk:
  - Mainline parity knobs do not automatically transfer safely to the C++ HAL
    lifecycle. Every default still needs physical proof.

### Zeno

- Mission: read-only HAL safety failure analysis for the pool-cursor run where
  `coreaudiod` reached `172.2%` CPU during load while the OpenA8DJ driver stayed
  near idle.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorizacion de ventana."
- Status: completed.
- Result:
  - Confirmed pool-cursor failure should be treated as a real HAL load safety
    failure, not as permission to run soundcheck.
  - Ranked the leading hypothesis as CoreAudio/HAL lifecycle interaction during
    load/enumeration: `coreaudiod` spiked while the OpenA8DJ driver process was
    nearly idle.
  - Noted that pool-cursor is only directly used in transfer-pool checkout, so
    the load-time spike is correlated with the build but not proven as direct
    execution of that cursor path.
  - Recommended correcting `HAL_OUTPUT_AMPLITUDE_STATS` drift before further
    candidates.
- Integrated action:
  - Kept `HAL_TRANSFER_POOL_CURSOR=0` by default.
  - Set `HAL_OUTPUT_AMPLITUDE_STATS=0` by default and tested it; physical run
    still failed, so it remains an overhead cleanup, not a quality fix.
- Evidence:
  - `local-analysis/hal-candidate-safety/20260616-pool-cursor-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-ampstats-off-irig-pairA-16s-cpp-hal`
- Risk:
  - No further transfer-pool cursor physical tests should run until the load
    safety failure has a bounded explanation.

### Descartes

- Mission: compare mainline C/Obj-C and C++ USB start/control sequence
  read-only, especially `AUDIO_PARAMS`, `READ_IO`/`WRITE_IO`, direct
  `usb-play`, and build-flag differences.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorizacion de ventana."
- Status: completed.
- Result:
  - Confirmed HAL USB start/control order is nearly identical between mainline
    and C++: open/configure/interface/alt-setting, `GET_DEVICE_INFO`,
    `READ_IO`, optional default `WRITE_IO`, reset-style `AUDIO_PARAMS`, and
    real stream params.
  - Confirmed C++ always sends reset-style `AUDIO_PARAMS 0xff`, while mainline
    exposes it behind `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM`.
  - Confirmed `usb-play` source calls the same
    `OpenA8DJUSBStart`/`OpenA8DJUSBWriteOutput`/`OpenA8DJUSBStop` engine in
    both trees.
  - Found the direct-tool build difference: mainline `usb-play` compiles
    `OpenA8DJUSB.m` with `CFLAGS`, while C++ previously compiled direct
    `usb-play` with `HAL_CFLAGS`.
- Integrated action:
  - Added `make usb-play-plain` and `make usb-play-plain-gain05` in the C++
    worktree.
  - Ran locked direct USB physical diagnostics with both tools; both failed
    quality, so `HAL_CFLAGS` contamination is not the sole explanation.
- Evidence:
  - `local-analysis/direct-usb-soundcheck/20260616-plaincflags-minus16-music`
  - `local-analysis/direct-usb-soundcheck/20260616-plaincflags-gain05-minus16-music`
- Risk:
  - The remaining blocker is deeper than a direct-tool build flag mismatch.
    More physical sweeps should wait for transfer/cadence/device-state evidence.

## Integration Notes

- No subagent may declare readiness independently.
- Architect owns final prioritization, integration, and go/no-go claims.
- Disjoint write ownership was assigned to prevent document conflicts.

### Halley

- Mission: read-only comparison of mainline `/Users/fer/dev/opena8dj` and C++
  `/Users/fer/dev/audio8djcpp` USB/HAL transport, scheduling, counters, and
  build defaults to explain C++ physical quality/CPU failures.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp si necesitas dejar una nota, pero
  preferimos respuesta final sin editar. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorizacion de ventana."
- Status: completed.
- Result:
  - Found C++ `HAL_OUTPUT_WRITE_STATS=0` caused an unobservable mutex update:
    `writeOutput` updated `outputFramesWritten` through stream-stats mutex, but
    snapshots overwrote the field from `_outputFramesWrittenAtomic`.
  - Found mainline exposes `outputLateWriteFrames` and
    `outputLateWriteBatches`, while C++ dropped late writes silently.
  - Found C++ still uses queue depth `64/64` versus mainline `8/8`.
  - Found C++ output prefetch is `256` versus mainline `64`.
  - Confirmed mainline has hot stream-stats gates/intervals while C++ did not.
  - Flagged input/control-plane decode divergences for later timecode/control
    validation.
- Integrated action:
  - Added C++ hot stream-stats gate/interval.
  - Restored atomic output-write stats default to `1`.
  - Added late-write counters to HAL and `opena8dj-control`.
- Evidence:
  - `local-analysis/build-flags/hot-stats-off-build.log`
  - `local-analysis/build-flags/output-write-stats-off-build.log`
  - `local-analysis/offline-gates-after-hot-stats-output-write.log`
- Risk:
  - These changes improve observability and remove one hot-path mutex, but do
    not prove physical quality. Next locked tests must verify CPU, late-write
    counters, and iRig quality before promotion.

### Harvey

- Mission: compare mainline and C++ capture transaction counters read-only and
  explain whether the stable aggregate capture error ratio is a real transport
  failure.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorizacion de ventana."
- Status: completed.
- Result:
  - Mainline and C++ classify capture transactions the same way:
    non-success status is a status failure, `completeCount == 0` is a
    zero-complete transaction, and only expected-size transactions are useful.
  - Recent C++ evidence shows status failures `0`, short/other-size transfers
    `0`, and the aggregate error count equals zero-complete transactions.
  - With ISO5, expected plus zero-complete transactions exactly match
    `5 * captureTransfers` in recent detailed runs.
- Integrated action:
  - Added `scripts/analyze-capture-iso-invariants.py`.
  - Recorded recent PASS and historical UNKNOWN/FAIL split in docs.
- Evidence:
  - `local-analysis/stream-stats/capture-iso-invariants-recent-v3.json`
  - `local-analysis/stream-stats/capture-iso-invariants-all-irig-v3.json`
- Risk:
  - This is not a quality fix. The remaining blocker is analog residual/lag and
    high driver CPU versus mainline.

### Raman

- Mission: inspect existing C++ tools and identify a safe lock-gated path for a
  Pair A decorrelated L/R matrix and crosstalk test.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorizacion de ventana."
- Status: completed.
- Result:
  - Confirmed `scripts/run-soundcheck` is the existing lock-gated wrapper but
    is optimized for music/global quality, not L/R matrix reporting.
  - Confirmed `audio-wav-play` preserves stereo L/R when routing to A/B/C/D.
  - Confirmed `audio-record` captures a selected physical stereo input pair.
  - Identified `scripts/generate-loopback-reference.py` as the right
    decorrelated stereo fixture source.
  - Identified gaps: `audio-pair-tone` emits same-tone L/R, and
    `analyze-tone-capture.py` collapses to mono, so neither can prove L/R
    crosstalk.
- Integrated action:
  - Added `scripts/run-channel-matrix-gate`.
  - Added `make channel-matrix-prepare`.
  - Added `scripts/analyze-soundcheck-linear-matrix.py`.
- Evidence:
  - `local-analysis/channel-matrix/offline-prepare-smoke`
  - `local-analysis/soundcheck-linear-matrix/recent-failed-physical-music.json`
- Risk:
  - The physical matrix run still requires explicit hardware window, iRig route
    confirmation, and hardware lock. No physical matrix evidence exists yet.

### Tesla

- Mission: read-only scout for next low-risk CPU/quality improvements without
  repeating rejected knobs.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorizacion de ventana."
- Status: completed.
- Result:
  - Do not repeat already rejected knobs: input decode active gating, queue
    before capture requeue, transfer-pool cursor, preopen/stop-ISOC, fast
    prefetch clear, unrolled output pack, queue 8/8, prefetch 64, and reset-off.
  - Recommended next CPU order:
    1. `HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=1` to remove completion-path mutex
       while preserving observability;
    2. `HAL_OUTPUT_TIMELINE_CHUNK_IO=1` to reduce per-frame timeline work under
       lock;
    3. `HAL_INPUT_DECODE_COUNTER_FASTPATH=1` for modulo-free input decode,
       guarded by DVS/timecode evidence.
- Integrated action:
  - No CPU code change yet. The immediate integration was the matrix gate,
    because current quality evidence is ambiguous about physical mix/routing.
- Risk:
  - `HAL_STREAM_STATS_ATOMIC_ACCUMULATORS` is the lowest-risk CPU candidate, but
    it still needs offline monotonicity/snapshot checks before any physical run.

### Nietzsche

- Mission: read-only HAL hot-path audit after atomic stream stats were
  rejected, focused on CPU changes that preserve audio bytes and USB cadence.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Recommended increasing hot stream stats interval as the next low-risk CPU
    experiment.
  - Flagged remaining CPU hotspots: stream-stats accounting, transfer-pool
    scan/cursor behavior, output packing, timeline/prefetch locks, and input
    decode.
- Integrated action:
  - Tested and promoted `HAL_HOT_STREAM_STATS_INTERVAL=16` as a partial CPU
    improvement only.
- Risk:
  - CPU remains far above mainline, and physical music quality still fails.

### Feynman

- Mission: read-only analysis of physical evidence to decide whether the
  failing music captures are caused by CPU/timing pressure or a signal-quality
  model mismatch.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Found no strong correlation between residual windows and driver CPU.
  - Noted that stream stats during active playback looked cleaner than the
    after-stop drain counters.
  - Recommended offline EQ/LTI/reference-route diagnostics before more blind
    HAL changes.
- Integrated action:
  - Added tone-response and LTI diagnostics.
- Risk:
  - The root cause remains unresolved; LTI/EQ rejection narrows the search but
    does not prove a fix.

### Wegener

- Mission: read-only signal-quality audit over existing C++ physical evidence
  and analysis scripts.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Confirmed the current physical music failure is stable across many runs:
    SNR around `9.5-10.5 dB`, residual mid/high around `1.4x`, and lag jumps
    around `39-48`.
  - Confirmed LTI and tone-response compensation do not explain the residual.
  - Weakened simple L/R mix, swap, polarity, and crosstalk explanations.
  - Kept reference-route mismatch and runtime/timeline discontinuity as the
    most useful next hypotheses.
- Integrated action:
  - Added `scripts/analyze-soundcheck-failure-modes.py`.
- Risk:
  - The next decisive evidence likely requires a more controlled physical
    reference-route test under lock.

### Gauss

- Mission: read-only CPU/hot-path audit for low-risk changes after interval16,
  separating telemetry-only changes from changes that may alter audio quality.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Recommended first testing stats-off/diagnostic-off variants that do not
    touch payload bytes or USB cadence.
  - Prioritized future lower-risk CPU work around batched stats publication and
    HAL cycle-buffer clear optimization.
  - Warned against reusing `iso64`, unrolled pack, reset-audio-params-off, or
    aggressive prefetch clear as readiness candidates based on existing
    evidence.
- Integrated action:
  - Tested `HAL_OUTPUT_WRITE_STATS=0 HAL_HOT_STREAM_STATS=0` physically and
    rejected it: CPU p95 stayed `36.8%`, quality still failed, and observability
    was reduced.
- Risk:
  - Remaining CPU work must target actual transfer/timeline/packing cost, not
    superficial telemetry removal.

### Hume

- Mission: read-only HAL/USB audit after repeated quality and CPU failures,
  focused on runtime discontinuities, transaction cadence, and soundcheck
  measurement perturbation.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorizacion de ventana."
- Status: completed.
- Result:
  - Identified capture-paced playback with many small isochronous transactions
    as a more plausible CPU source than HAL cycle-buffer clear.
  - Highlighted locks/copies in USB timeline, transfer-pool checkout, and
    sampled stream stats.
  - Noted that `run-soundcheck` stream-stat polling can perturb CPU profiles
    and that `stream-stats-after` underruns are post-playback/drain evidence,
    not active music-glitch proof.
  - Kept sample-time/reference mismatch as a live hypothesis, but not due to
    iRig sample-rate mismatch in recent 48 kHz runs.
- Integrated action:
  - Added `--no-monitor-stream-stats` diagnostic mode.
  - Tested `HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=1`;
    it failed HAL safety and was rejected before soundcheck.
  - Added playback burst cadence rows to `opena8djcpp_jitter_model` so the
    coalesce2 CPU win is blocked offline when it doubles playback completion
    spacing relative to capture cadence.
- Risk:
  - Transaction-count reduction is still likely important for CPU, but the
    first coalescing variants either destabilized CoreAudio on load or failed
    physical music quality. Future variants need smaller, separately isolated
    changes that preserve playback cadence and pass safety gates.

### Lorentz

- Mission: read-only HAL/USB CPU audit for low-risk changes that preserve USB
  cadence, payload bytes, sample rate, defaults, routing, and physical quality.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Confirmed hot stream stats should remain sampled rather than disabled;
    default build already uses `HAL_HOT_STREAM_STATS_INTERVAL=16`.
  - Identified per-stream/per-frame input stats locking as a safe CPU target:
    preserve input decode and stats math, but aggregate locally per capture
    transfer and merge once.
  - Identified transfer-pool free-list work and HAL double-copy reduction as
    future candidates, both requiring stricter offline simulators before any
    physical run.
- Integrated action:
  - Merged output timeline start-frame resolution into the timeline write lock.
  - Replaced per-sample input stats mutex locking with stack-local aggregation
    plus one merge per transfer.
- Risk:
  - These are callback-overhead changes only; they do not prove better audio
    or lower physical runtime CPU until a locked hardware A/B run passes.
