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

## Integration Notes

- No subagent may declare readiness independently.
- Architect owns final prioritization, integration, and go/no-go claims.
- Disjoint write ownership was assigned to prevent document conflicts.
