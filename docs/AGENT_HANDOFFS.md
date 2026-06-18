# Agent Handoffs

## 2026-06-18 - Archimedes - ISO8 CPU Hot-Path Scout

Mission:
- Read `/Users/fer/dev/audio8djcpp` only.
- Do not modify files.
- Do not touch hardware, CoreAudio, USB, drivers, or system services.
- Identify CPU reduction options that preserve ISO8 capture/playback cadence,
  playback coalesce=1, and observable stream stats.

Required warning given:
- "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."

Findings:
- Best next CPU direction is not more capture batching or prepared runtime.
- Top candidates are:
  1. batch capture decode before publishing to rings;
  2. replace mutex-backed rings with true SPSC/bulk-copy rings;
  3. preconfigure fixed ISO8 transfer/callback slots without per-submit ObjC
     work;
  4. rewrite playback packer as byte-exact ISO8 batch packer;
  5. keep observability but move hot stats to local counters / seqlock-style
     snapshots.
- `stats-off` is risky because it changes timing and also hides/breaks
  evidence; disabling output write stats can leave `_outputFramesWrittenAtomic`
  stale while snapshots still publish it.

Files affected by subagent: none.

Risks:
- Ring/batch decode changes can affect input ordering, timecode latency, and
  capture routing.
- Fixed transfer slots can introduce stale completion/lifecycle bugs.

Recommended next action:
- The ISO8-preserving input decode batch was implemented, passed offline gates,
  then failed physical iRig quality (`quality_alignment_score=0.112023`,
  `analog_snr_db=-20.50`, `lag_jumps_gt_2_frames=45`) without a meaningful CPU
  win. Do not continue this line as a product candidate.
- Next CPU work should move to fixed transfer lifecycle, packer efficiency, or
  stats publication while preserving the proven per-frame input ring timing.

## 2026-06-18 - Existing Subagent Results Integrated

Shared warning given/retained:
- "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."

Subagents/results used:
- Resource/performance review: identified that the remaining CPU/jitter gap was
  not throughput, but a fail-closed resource-superiority model tied to prepared
  submit reduction and hot-path timing. Integrated into
  `tools/transport_budget_model.cpp`, `scripts/run-cpp-offline-gates`, and
  `tools/evidence_schema_check.cpp`.
- Physical-window review: confirmed the safest next physical sequence remains
  non-Audio8 wired known-good route revalidation before any product A/B.
- HAL prepared-runtime review: confirmed default HAL should remain untouched for
  claims until the prepared path has real submit-cadence evidence.
- Audiophile-analysis review: confirmed dual C++/Python agreement and degraded
  analyzer self-tests are already present in the current tree.

Files affected by integration:
- `tools/transport_budget_model.cpp`
- `scripts/run-cpp-offline-gates`
- `tools/evidence_schema_check.cpp`
- `docs/DECISION_LOG.md`
- `docs/ARCHITECT_CONTEXT.md`
- `docs/TEST_EVIDENCE.md`
- `docs/AGENT_HANDOFFS.md`

Risks:
- The offline resource model is a hypothesis gate, not physical proof.
- Branch promotion remains blocked until validated route, same-session mainline
  vs C++ A/B, CPU/jitter superiority, strict audiophile gates, and
  Traktor/timecode physical evidence pass.

Recommended next action:
- Commit this gate hardening, rerun offline gates on clean HEAD, then only use a
  lock-gated physical window for route revalidation if a separate wired
  non-Audio8 output is visible.

## 2026-06-18 - Physical Route and DriverKit SDK Gap Subagents

Shared warning given:
- "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."

Subagents/results used:
- Hegel, Mainline Physical Evidence Archaeologist:
  read `/Users/fer/dev/opena8dj` in read-only mode and wrote
  `docs/MAINLINE_PHYSICAL_ROUTE_NOTES.md`.
- Volta, DriverKit Readiness Auditor:
  audited the C++ worktree and official Apple DriverKit documentation, then
  wrote `docs/DRIVERKIT_READINESS_GAP.md`.

Findings:
- Mainline's decisive physical evidence path is
  `Open Audio 8 DJ output -> external mixer -> mixer REC OUT -> iRig Stream
  input -> macOS capture`, with `iRig Stream` channels `1,2` at `48 kHz`.
- Software-only and no-iRig gates do not prove release, human-listening, or
  branch-promotion readiness.
- A real DriverKit/dext claim remains blocked by
  `real_driverkit_sdk_and_selected_xcode_missing`; the next safe software-only
  improvement is an opt-in build-only DriverKit SDK probe that does not install
  or activate a system extension.

Files affected:
- `docs/MAINLINE_PHYSICAL_ROUTE_NOTES.md`
- `docs/DRIVERKIT_READINESS_GAP.md`
- `docs/AGENT_HANDOFFS.md`
- `docs/ARCHITECT_CONTEXT.md`

Risks:
- Physical route evidence is historical and route-shape evidence; it is not a
  fresh pass for the current C++ candidate.
- DriverKit source scaffold and contracts are not a dext readiness claim until
  a real full-Xcode DriverKit SDK build-only probe passes.

Recommended next action:
- Keep physical/product claims blocked until the iRig/mixer route is freshly
  revalidated under lock, and add the DriverKit SDK build-only probe as an
  offline blocker-hardening task before any dext readiness statement.

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
| Pauli | Read-only HAL/USB transfer-ledger instrumentation audit. | findings integrated in this document |
| Pasteur | Read-only byte-format and diagnostic-capture analysis. | findings integrated in this document |
| Euler | Read-only physical evidence triage after diagnostic capture. | findings integrated in this document |
| Carver | Read-only steady CPU/sample audit after v5 profiling. | recommended output-only no-capture ISO experiment; integrated as opt-in only |
| Einstein | Read-only C++ versus mainline HAL CPU divergence audit. | findings integrated in this document |
| Ohm | Read-only capture-route isolation review after clean Direct USB / failed iRig evidence. | known-good source first, then Audio 8 direct-to-iRig; integrated below |
| Maxwell | Read-only route forensics after current direct USB/iRig evidence. | concluded failures are route/timebase/capture instability, not simple channel/polarity/clipping or digital payload corruption |
| Peirce | Read-only product-claim gate audit after route regression. | recommended hard blocker for clean direct USB payload plus failed physical capture; integrated into claim gates |
| Curie | Read-only audit of route revalidation scripts and gates before next hardware window. | recommended unambiguous device identity, reference WAV preflight, and dual audiophile analyzers for known-good route evidence |

## Findings Integrated

### Chief Architect Integration: Known-Good Route Evidence Hardening

- Date: 2026-06-18.
- Subagent dependency:
  - Curie audited `run-known-good-route-soundcheck`,
    `validate-known-good-route-request.py`, `physical-window-preflight`,
    `run-physical-superiority-window`, and route-readiness gates in read-only
    mode.
- Integrated action:
  - Added dual C++/Python audiophile WAV analysis to
    `scripts/run-known-good-route-soundcheck`.
  - Added reference WAV preflight before lock acquisition.
  - Made known-good output/capture selectors fail on ambiguous CoreAudio name
    matches.
  - Made promotion readiness require same-window known-good audiophile analyzer
    artifacts.
  - Extended promotion-window contract tests for missing audiophile artifacts
    and ambiguous known-good selectors.
- Files affected:
  - `scripts/run-known-good-route-soundcheck`
  - `scripts/validate-known-good-route-request.py`
  - `scripts/physical-window-preflight`
  - `scripts/evaluate-promotion-readiness.py`
  - `scripts/test-promotion-window-contract.py`
  - `tools/audiophile_analysis_stack_contract.cpp`
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/AGENT_HANDOFFS.md`
- Risk:
  - These changes only strengthen route-evidence quality. They do not prove
    that the current physical route is good, nor that C++ beats mainline.
- Next action:
  - Run full offline gates, then use the hardware lock only for a route-only
    known-good non-Audio8 -> iRig revalidation when a real wired source is
    available.

### Chief Architect Integration: Direct USB Wide-Lag Audiophile Analysis

- Date: 2026-06-18.
- Subagent dependency:
  - Maxwell classified the latest direct USB/iRig failure as physical
    route/timebase/capture instability after a clean internal USB payload.
  - Peirce recommended preserving that split as a hard product-claim blocker.
- Integrated action:
  - Updated `scripts/run-direct-usb-soundcheck` to generate
    `audiophile-wav-analysis-maxlag6.json` automatically for direct USB route
    diagnostics.
  - Updated `opena8djcpp_audiophile_analysis_stack_contract` to require that
    wide-lag direct USB analyzer path.
- Files affected:
  - `scripts/run-direct-usb-soundcheck`
  - `tools/audiophile_analysis_stack_contract.cpp`
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/AGENT_HANDOFFS.md`
- Risk:
  - This improves measurement reliability only. It does not prove better sound
    quality, lower CPU/resource use, routing completeness, or Timecode Vinyl
    readiness.
- Next action:
  - Run full offline gates from clean evidence, then only use the hardware lock
    for route revalidation with a known-good non-Audio8 source before any
    product A/B.

### Chief Architect Integration: HAL Prepared Runtime Binding Contract

- Date: 2026-06-18.
- Subagent dependency:
  - Mendel reviewed the active HAL path read-only and warned that a full
    prepared runtime rewrite is not yet the safe next slice. The key risks are
    transfer lifetime, single completion per physical submit, stop/cancel
    behavior, timestamp accounting, and observable submit counters.
- Integrated action:
  - Added `opena8djcpp_hal_prepared_runtime_binding_contract`.
  - Required it from migration, runtime, schema, static, and full offline gates.
  - Kept product and CPU claims blocked until lock-gated physical evidence
    proves accepted submit reduction and clean capture quality.
- Files affected:
  - `tools/hal_prepared_runtime_binding_contract.cpp`
  - `tools/hal_transport_runtime_gate.cpp`
  - `tools/prepared_transport_migration_gate.cpp`
  - `tools/evidence_schema_check.cpp`
  - `tools/static_policy_check.cpp`
  - `scripts/run-cpp-offline-gates`
  - `CMakeLists.txt`
- Risk:
  - The binding proves source geometry and observability only. It does not
    prove that Audio 8 DJ accepts the 64-transaction runtime profile, nor that
    CPU, jitter, routing, sound quality, or Timecode Vinyl beat mainline.
- Next action:
  - Run full offline gates, then only request a hardware window if a known-good
    wired capture route is available and the global hardware lock is acquired.

### Chief Architect Integration: HAL Prepared Runtime Build Profile

- Date: 2026-06-18.
- Subagent dependency:
  - Wegener's runtime audit identified that the active HAL path still directly
    enqueues USB requests and recommended a prepared-submit adapter contract
    before live mutation.
  - Lagrange's readiness audit kept product claims blocked until same-session
    physical A/B, Timecode Vinyl, CPU/resource, and route evidence exist.
- Integrated action:
  - Added default-off HAL prepared runtime build flags and `hal-prepared-runtime`.
  - Added `opena8djcpp_hal_prepared_runtime_source_contract`.
  - Updated migration/runtime/schema/static gates so the profile is
    opt-in-only, build-only, and still product-claim-blocked.
- Files affected:
  - `Makefile`
  - `src/hal/OpenA8DJUSB.m`
  - `tools/hal_prepared_runtime_source_contract.cpp`
  - `tools/hal_transport_runtime_gate.cpp`
  - `tools/prepared_transport_migration_gate.cpp`
  - `tools/evidence_schema_check.cpp`
  - `tools/static_policy_check.cpp`
  - `scripts/run-cpp-offline-gates`
- Risk:
  - The profile compiles but has not been installed, loaded, or physically
    measured. Do not treat it as better than mainline until lock-gated
    same-session A/B evidence proves lower CPU/resource use and no quality,
    routing, or Timecode Vinyl regression.
- Next action:
  - Run full offline gates from clean HEAD, then request a route-revalidation
    hardware window only if all evidence and lock conditions are satisfied.

### Ohm Capture Route Isolation Reviewer

- Agent: `019ed6ea-d814-7c92-9173-f606afb367ed`.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Mission:
  - Inspect current C++ evidence in read-only mode and identify the next
    minimum physical test to separate iRig/mixer/capture route, Audio 8 analog
    output/DAC, and reference/analyzer problems.
- Findings:
  - Do not run another HAL tweak as the next physical step.
  - Use a known-good non-Audio8 source playing the same deterministic reference
    through the existing mixer/REC OUT -> iRig route first.
  - If that passes, test Audio 8 Pair A directly into iRig without mixer/EQ
    ambiguity.
  - Existing gates can prove clean USB before the device and bad physical
    capture, but cannot separate Audio 8 DAC from mixer/iRig/cabling while all
    captures share the same post-DAC route.
- Files affected by integration:
  - `tools/irig_idle_capture_gate.cpp`.
  - `tools/direct_usb_path_attribution.cpp`.
  - `scripts/run-cpp-offline-gates`.
  - `docs/ARCHITECT_CONTEXT.md`.
  - `docs/DECISION_LOG.md`.
  - `docs/SUCCESS_METRICS.md`.
  - `docs/TEST_PLAN.md`.
  - `docs/TEST_EVIDENCE.md`.
- Next action:
  - Run a lock-gated known-good-source route test when a controllable external
    source is available. Do not claim DAC failure or product readiness before
    that separation.

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

### Carver

- Mission: read-only inspection of existing physical evidence and HAL/USB
  transport code after commit `056d29b` failed physically.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Confirmed the next high-value hypothesis is structural transport/pacing
    observability, not another isolated stats/lock tweak.
  - Highlighted that `056d29b` failed with SNR about `10.41 dB`, `43` lag
    jumps, residual mid/high about `1.43/1.36`, and driver CPU p95 about
    `37.5%`.
  - Confirmed existing evidence weakens byte order, crosstalk, late writes,
    clipping, and simple matrix explanations.
  - Recommended a preallocated transport ledger for OUT transfers and an
    offline replay using the real HAL transfer layout before another physical
    run.
- Integrated action:
  - Added transfer-pool fallback allocation counters to the HAL stream-stats
    payload, `opena8dj-control`, `run-soundcheck` TSV capture, and
    `analyze-stream-stats.py`.
- Risk:
  - This is observability, not a sound-quality fix. The next physical run must
    still be blocked until there is a new transport/cadence/device-state
    hypothesis, and any fallback allocations found during streaming must be
    treated as a CPU/latency defect.

### Pauli

- Mission: read-only HAL/USB transport audit for exact transfer-ledger
  instrumentation points.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Identified `queueCaptureTransfer`, `handleCaptureTransfer`,
    `queuePlaybackWithRequests`, and `handlePlaybackTransfer` as the exact
    transfer-ledger hooks.
  - Confirmed queue timestamps were missing, while completion timestamps,
    request/complete byte counts, in-flight playback count, output timeline
    read stats, fallback allocations, and drop/replay counters already existed.
  - Recommended a preallocated POD ring with atomic index and post-run export,
    avoiding `_streamStatsMutex`, `_diagnosticMutex`, logging, file I/O, and
    Objective-C allocation in the hot path.
- Integrated action:
  - Added fixed-size transfer-ledger instrumentation to the HAL and exported
    aggregate ledger counters through `opena8dj-control stream-stats`,
    `scripts/run-soundcheck`, and `scripts/analyze-stream-stats.py`.
- Risk:
  - The ledger is diagnostic instrumentation. It improves observability for the
    next physical run, but any final low-CPU claim must be repeated or
    controlled with instrumentation overhead accounted for.

### Pasteur

- Mission: read-only byte-format and diagnostic-capture analysis after the
  aggregate transfer-ledger run.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Found no strong evidence supporting a simple byte-order, start-byte,
    check-offset, or deck-mapping hypothesis.
  - Recommended a diagnostic HAL capture with big-endian output,
    `start_byte=4`, `check_offset=8`, unrolled pack disabled, and amplitude
    stats enabled.
  - Recommended analyzing `/tmp/opena8dj-output-packed-usb.raw` with
    `scripts/analyze-driver-capture.py` before any further physical format
    sweep.
- Integrated action:
  - Ran the diagnostic HAL capture and copied diagnostic files into
    `local-analysis/soundcheck/20260617-diag-pack-big-start4-irig-pairA-16s-cpp-hal`.
  - Confirmed output written, consumed, and packed USB bytes are perfect
    against the reference while the iRig analog capture still fails.
- Risk:
  - The byte-format layer is not the current dominant blocker. Future work
    should avoid random format changes unless backed by new evidence.

### Euler

- Mission: read-only physical evidence triage and next-test recommendation
  after diagnostic output bytes looked internally correct.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Recommended one controlled A/B with `HAL_OUTPUT_NATIVE=1` only if the
    diagnostic capture did not already prove a byte-order answer.
  - Recommended abandoning simple byte-order pursuit if native output did not
    materially improve physical quality.
- Integrated action:
  - Ran the native-output A/B under the hardware lock.
  - Native output was catastrophically rejected:
    `quality_alignment_score=0.003598`, SNR `-63.94 dB`,
    `520014` clipped capture frames.
  - The active native HAL had to be parked under lock because CoreAudio
    respawned the process after a direct kill.
- Risk:
  - Native output is unsafe for this hardware route. It should not be loaded
    again except as a deliberately isolated forensic test with explicit
    clipping/noise safeguards.

### Beauvoir

- Mission: read-only USB transport forensics after packed output bytes were
  proven internally correct but physical music still failed.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Recommended transaction-level transfer forensics: queue/complete host time,
    first-frame numbers, transaction indexes, request/complete counts, status,
    offsets, timestamps, and in-flight deltas.
  - Recommended explicit scheduling and capture-paced/fixed OUT A/B only as
    controlled physical diagnostics, not as readiness paths.
- Integrated action:
  - Added and tested playback payload guard instrumentation. It reported
    `0` mismatches in a failing physical music run, ruling out
    queue-to-completion payload mutation.
  - Ran explicit-scheduling and fixed-OUT physical A/Bs. Both are rejected by
    metrics and must not be promoted.
  - Added `--force-unload-opena8dj` to `scripts/audio-stack-guard` in the C++
    worktree for explicit post-test cleanup.
- Risk:
  - The remaining useful transport evidence is still aggregate. If the next
    physical run cannot explain the music timebase instability, add a bounded
    export of transaction-level ledger records rather than more blind knob
    sweeps.

### Architect Local Integration

- Mission: continue Beauvoir's transaction-level recommendation after agent
  thread limit prevented spawning a new sidecar agent.
- Status: completed for bounded full-ledger instrumentation and physical
  diagnosis; product readiness still failed.
- Result:
  - Added `build/opena8dj-control transfer-ledger [count]`.
  - Added HAL IPC export for a bounded latest-entry transfer ledger window.
  - Added `transfer-ledger-after.tsv` capture to
    `scripts/run-soundcheck --stream-stats-snapshots`.
  - Added `scripts/analyze-transfer-ledger.py` plus synthetic PASS evidence.
  - Extended the export to `transfer-ledger --all [--from sequence]` with a
    matching `startSequence` request field and a larger preallocated
    `131072`-entry HAL ledger ring.
  - Fixed the analyzer to parse real CLI headers, full-window ledgers, and
    distinguish capture zero-complete observations from playback transport
    failures.
  - Fixed `--all` row offset and bounded live dumps to the initial
    `latestSequence` so row count, declared count, and coverage match even
    while the HAL continues streaming.
  - Made the full transfer ledger diagnostic-only by default:
    `HAL_TRANSFER_LEDGER=0` for product builds, `HAL_TRANSFER_LEDGER=1` for
    explicit physical diagnosis.
  - Verified build, help surface, offline gates, runtime isolation, fixture
    analysis, and a locked physical bounded-ledger run.
  - Physical bounded-ledger run:
    `local-analysis/soundcheck/20260617-bounded-full-ledger-irig-pairA-12s-cpp-hal`.
    Product quality FAIL (`quality_alignment_score=0.960392`, SNR `10.37 dB`,
    `33` lag jumps), but ledger analysis PASS (`91647` contiguous rows,
    `overwritten=0`, no playback failed/short transactions, no sequence gaps).
- Files affected:
  - `src/hal/OpenA8DJUSB.m`
  - `src/tools/opena8dj-control.c`
  - `scripts/run-soundcheck`
  - `scripts/analyze-transfer-ledger.py`
  - `core/tests/fixtures/transfer-ledger-full-window.tsv`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/DECISION_LOG.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/TEST_EVIDENCE.md`
  - `docs/AGENT_HANDOFFS.md`
- Risk:
  - Clean transaction transport evidence does not explain or fix physical music
    quality. The candidate remains `FAIL_NOT_READY`.
  - CPU evidence from ledger-enabled builds is diagnostic only. Re-measure
    product CPU after installing a `HAL_TRANSFER_LEDGER=0` candidate.
  - The stream-stats analyzer still flags a bounded-snapshot playback completion
    gap at the post-run edge; this is not a sound-quality pass/fail by itself
    and needs an active-playback window model before being promoted to a hard
    gate.
- Next action:
  - Stop blind byte-order/start-byte sweeps. Investigate the post-packed-byte
    failure: USB/device scheduling/state, analog route/reference mismatch, or a
    controlled mainline-vs-C++ physical route comparison with identical
    capture conditions.

### Pauli

- Mission: read-only audit of transfer-ledger semantics while the architect
  implemented full export fixes.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Flagged IPC struct mismatch risk for `startSequence` between HAL and CLI.
  - Flagged analyzer false-PASS risk for tail-only truncated evidence and
    `overwritten > 0`.
  - Flagged false-FAIL risk from treating all capture failed transactions as
    product failures.
  - Flagged post-tail active-underrun snapshots as unsafe hard failures without
    active-playback window context.
  - Recommended full merged ledger coverage with `overwritten=0`, no duplicate
    or missing sequences, category-aware capture semantics, and separate analog
    capture metrics.
- Integrated action:
  - IPC structs now match for `startSequence`.
  - `--all` full export plus analyzer coverage checks now require declared
    count/expected count/row count continuity and fail overwritten evidence.
  - Capture zero-complete observations are warnings, not playback transport
    failures.
  - Tail/post-playback active-underrun snapshots are warnings unless correlated
    to active playback by stream stats.
- Remaining risk:
  - `run-soundcheck` saves ledger evidence but does not yet make ledger analysis
    a hard integrated subgate. This should be added once the active-playback
    window model is explicit.

### Hume

- Mission: read-only performance audit after product ledger-off physical CPU
  failure.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Identified capture-paced USB playback with many small IOUSBHost transfers
    as the dominant CPU candidate.
  - Warned that coalescing proves transfer/completion frequency matters but is
    already rejected as a quality candidate.
  - Recommended symbol profiling and a future transport redesign that removes
    work from completion callbacks without coarsening playback cadence.
- Integrated action:
  - Ran a locked playback-only `sudo sample` window. The sample confirmed
    dominant CPU in `org.opena8dj.driver.usb`, especially capture and playback
    `IOUSBHostPipe enqueueIORequest...` stacks.
- Remaining risk:
  - No current knob both lowers CPU and preserves physical music quality. A new
    transport design is needed before another promotion attempt.

### Carver

- Mission: read-only quality/capture audit of latest product ledger-off Pair
  A/iRig evidence.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: completed.
- Result:
  - Classified the physical failure as unlikely to be simple DSP/packing/channel
    corruption because both channels fail similarly, capture is unclipped, and
    offline packed-byte gates already pass.
  - Recommended a controlled same-chain bypass comparison through a known-good
    output path to separate Audio 8 DJ USB/device behavior from analog
    capture/reference-route issues.
- Integrated action:
  - The architect did not run the bypass comparison yet; it remains a candidate
    future physical window after the CPU/hot-path design decision is clearer.
- Remaining risk:
  - Without a controlled mainline/C++ or known-good output A/B on the exact
    iRig route, analog/reference contribution is not fully isolated.

### Beauvoir

- Mission: read-only C++ vs mainline routing/output-path audit after the latest
  Pair A physical channel-matrix comparison showed C++ worse than mainline on
  max wrong-source leakage.
- Required safety warning given:
  "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana."
- Status: pending/no response at the time of this handoff update.
- Local architect finding while agent was pending:
  - The C++ harness requests selected-pair `IOProcStreamUsage` by default.
  - The read-only mainline `0.3.135` artifact likely rejects that property
    because its default HAL has `HAL_STREAM_USAGE=0`.
  - This is an uncontrolled harness/HAL interaction in the latest physical
    C++ vs mainline matrix comparison.
- Integrated action:
  - Added `audio-wav-play --no-stream-usage`.
  - Added `scripts/run-channel-matrix-gate --no-output-stream-usage`.
  - Added `scripts/run-soundcheck --no-output-stream-usage`.
- Remaining risk:
  - The next locked physical A/B must run the stream-usage-off C++ variant
    before concluding whether C++ leakage is a driver defect, a harness
    difference, or a capture-route artifact.

### Architect Local Continuation After Beauvoir Pending

- Status:
  - A new explorer spawn for this audit failed because the agent thread limit
    was reached.
  - The architect proceeded locally under the required hardware lock.
- Physical findings:
  - Default C++ with harness stream usage disabled improved Pair A leakage from
    `-35.36 dB` to `-39.72 dB`, proving stream usage mattered.
  - The result still failed `-45 dB` and remained worse than mainline
    `-42.58 dB`.
  - Mainline-config C++ recovered mainline-like physical output level but still
    failed Pair A matrix at `-40.57 dB`.
  - Mainline-config C++ failed real-music quality:
    `quality_alignment_score=0.678827`, SNR `-0.83 dB`, `42` lag jumps.
  - Stream stats did not show output underruns, active underruns, elastic
    drops, timeline resets, or late writes.
- Files affected:
  - Documentation only in this continuation:
    `docs/TEST_EVIDENCE.md`, `docs/DECISION_LOG.md`,
    `docs/ARCHITECT_CONTEXT.md`, `docs/PROMOTION_READINESS_STATUS.md`,
    `docs/AGENT_HANDOFFS.md`.
- Next recommended action:
  - Do not promote.
  - Do not chase volume alone.
  - Isolate the remaining residual path below current counters: USB/device
    scheduling, hidden packet/cadence interpretation, analog/capture topology,
    or a missing physical control-state difference.

### Architect Direct USB Follow-Up

- Status:
  - Completed locally under lock after the stream-usage probes.
- Findings:
  - Direct USB `opena8dj-usb-play-plain-gain05` still failed Pair A matrix:
    max wrong-source leakage `-44.78 dB`; R->L leakage `-29.97 dB`; no
    clipping.
  - The same run had strong L/R level asymmetry, with right expected max only
    `0.01005`.
  - Direct USB `opena8dj-usb-play` built with current HAL flags was worse:
    max wrong-source leakage `-13.19 dB`.
  - Final isolation after both runs was PASS: HAL inactive, lock absent.
- Risk:
  - Direct USB tools are not clean bypass oracles yet because they write the
    stereo WAV to all output pairs and do not prove selected-pair routing.
- Next recommended action:
  - Build or adapt a selected-pair direct USB diagnostic with explicit audio
    params/control-state logging before using direct USB results to separate
    HAL/CoreAudio from USB/device behavior.

### Architect Selected-Pair Direct USB Follow-Up

- Status:
  - Implemented selected-pair support in `src/tools/opena8dj-usb-play.m`.
  - Default remains `all`; new usage is `[wav] [A|B|C|D|all] [lead_frames]`.
- Findings:
  - Selected Pair A without lead failed:
    max wrong-source leakage `-35.28 dB`, R->L `-18.05 dB`.
  - Selected Pair A with `8192` lead frames improved L->R to `-46.82 dB`, but
    the right expected level fell below threshold and R->L stayed poor at
    `-16.05 dB`.
- Next recommended action:
  - Stop using the direct USB tool as a quality oracle until it logs and
    validates audio params, control state, and selected-pair packet cadence.

### Architect ISO Sweep And Product-Gate Continuation

- Status:
  - Continued locally. A prior attempt to spawn another explorer was blocked
    by the agent thread limit, so the architect integrated the next hardware
    gates directly under the global hardware lock.
- Files affected:
  - `Makefile`
  - `src/hal/OpenA8DJUSB.h`
  - `src/hal/OpenA8DJUSB.m`
  - `src/tools/opena8dj-usb-play.m`
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Added direct USB diagnostics and absolute-deadline pacing to remove a tool
    timing artifact from the previous direct USB evidence.
  - Direct selected-Pair-A ISO sweep shows short cadence is required:
    ISO8/10/12/14 pass the matrix; ISO16 fails.
  - ISO10/q8 HAL Pair A matrix passes at about `-52.30 dB` wrong-source
    leakage.
  - ISO10/q8 real music still fails: residual ratios
    `1.514509/1.396638`, `35` lag jumps, driver p95 `19.6%`.
  - ISO8/q8 remains the current default quality candidate because it has
    better real-music residual and fewer lag jumps than ISO10/q8, despite
    higher driver CPU.
  - Promotion readiness remains `FAIL` and
    `branch_promotion_allowed=false`.
- Risks:
  - Matrix success can hide real-music residual/timebase defects.
  - CPU remains far above the mainline budget.
  - A/B/C/D physical routing and Traktor/timecode vinyl are not yet validated.
- Next recommended action:
  - Keep ISO8/q8 as the default while investigating the music residual and
    lag-jump mechanism.
  - Optimize the USB completion cadence without moving into ISO16 or ISO64
    quality regressions.
  - Add physical A/B/C/D matrix and timecode/DVS gates only after real music
    and CPU improve enough to justify more hardware time.

### Architect Input-Decode-Gated Playback Probe

- Status:
  - Continued locally in the C++ worktree. No new subagent was spawned in this
    iteration because the prior explorer attempts had already hit the agent
    thread limit.
  - Physical commands were run under the global hardware lock and cleaned up.
- Files affected:
  - `Makefile`
  - `src/hal/OpenA8DJUSB.m`
  - `src/tools/audio-wav-play.c`
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/PROMOTION_READINESS_STATUS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - `HAL_INPUT_DECODE_ACTIVE_GATING=1` is now the default.
  - USB stream start now honors the input-decode-active gating flag instead of
    unconditionally enabling input decode when input decode support exists.
  - `audio-wav-play` disables input stream usage for playback-only probes while
    still selecting the requested output pair.
  - Offline gates passed after the change.
  - First HAL safety attempt failed on a startup CoreAudio CPU spike
    (`coreaudiod=160.3%`) and was safely cleaned up.
  - Safety retry with `--wait 8` passed, then the real-music soundcheck failed:
    quality `0.959187`, SNR `10.14 dB`, mid/high residual
    `1.467121/1.368783`, `30` lag jumps, driver p95 `24.2%`.
  - Offline failure analyzers reject input decode, static mix/polarity,
    clipping, fixed LTI/EQ, and simple nonlinearity as sufficient causes.
    The strongest current classification is timebase/alignment instability.
- Risks:
  - The current gross stream counters are clean, so the remaining defect may be
    hidden in device cadence, USB packet pacing, timestamping, or analog/capture
    timing rather than explicit underrun counters.
  - CoreAudio startup spikes can cause false safety failures if the window is
    too short, but the steady product CPU remains too high regardless.
- Next recommended action:
  - Keep the harness/control-plane fix.
  - Do not promote.
  - Instrument and optimize the timebase/cadence path next, with evidence that
    improves real-music residual and CPU together.
  - Do not expand to Traktor/timecode or full A/B/C/D physical gates until the
    real-music Pair A gate improves enough to justify more hardware time.

### Architect ISO-Invariant Tooling And Cadence Diagnostic Profile

- Status:
  - Completed locally without hardware access.
  - Diagnostic HAL build was compiled once, then the product HAL build was
    restored with normal diagnostic flags off.
- Files affected:
  - `Makefile`
  - `scripts/analyze-capture-iso-invariants.py`
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Latest inputdecode-gated capture ISO invariants now PASS with a one final
    stop/drain transfer warning.
  - ISO8/q8 capture ISO invariants PASS.
  - ISO10/q8 capture ISO invariants PASS with the same one-stop-transfer
    warning.
  - The aggregate capture error counter is zero-complete ISO slot
    packetization in these runs, not the current product-quality blocker.
  - `make hal-cadence-diagnostic` gives the next physical cadence run transfer
    ledger, cadence diagnostics, payload guard, amplitude stats, per-transfer
    hot stats, and atomic stat accumulators.
- Risks:
  - The diagnostic HAL profile has extra overhead and cannot be used for
    product CPU claims.
  - It must be followed by `make -B hal` before any product candidate CPU or
    quality run.
- Next recommended action:
  - Under lock, run a short diagnostic cadence capture only when a physical
    window is justified.
  - Use the ledger/cadence evidence to decide whether the next product change
    should target queue ordering, transfer lead/depth, explicit scheduling, or
    timeline write/read policy.

### Architect Cadence Diagnostic Physical Capture

- Status:
  - Completed under hardware lock.
  - HAL unloaded after the run.
  - Product HAL build restored with diagnostics off.
  - Runtime isolation PASS after cleanup.
- Files affected:
  - `scripts/analyze-capture-iso-invariants.py`
  - `scripts/analyze-transfer-ledger.py`
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/PROMOTION_READINESS_STATUS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Diagnostic soundcheck failed quality: quality `0.958757`, SNR `10.09 dB`,
    mid/high residual `1.447622/1.366173`, `27` lag jumps.
  - Transfer ledger is continuous with `48528` rows, no gaps, no overwritten
    entries, playback queue/complete delta `0`, max in-flight `8`.
  - Final capture `0xe00002eb` rows are stop-window aborts and are now
    classified as warning, not transport failure.
  - Payload guard checks had `0` mismatches.
  - Capture ISO invariants pass when stop-transfer gap is accounted for.
  - Completion outliers are now visible: capture `7`, playback `8`.
  - Runtime discontinuity analysis found no strong correlation; completion
    outlier deltas are weakly correlated with lag jumps.
- Risks:
  - Diagnostic run overhead invalidates product CPU comparisons except as a
    rejection signal.
  - Completion jitter is still a hypothesis, not a proven root cause.
- Next recommended action:
  - Implement a product-side timing experiment that reduces completion jitter
    without diagnostic overhead. Candidates to test offline first:
    queue playback before capture requeue, bounded capture-paced lead greater
    than one, and explicit scheduling with strict fallback.
  - Any physical run must preserve the Pair A matrix and show better real-music
    residual and CPU together before expanding to A/B/C/D or Traktor/timecode.

### Architect Playback-Before-Capture-Requeue Product Probe

- Status:
  - Completed under hardware lock.
  - HAL unloaded after the run.
  - Product HAL build restored.
  - Runtime isolation PASS after cleanup.
- Files affected:
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/PROMOTION_READINESS_STATUS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Product timing probe with
    `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1` failed quality:
    quality `0.961360`, SNR floor `10.25 dB`, mid/high residual
    `1.425897/1.365001`, `28` lag jumps.
  - Runtime CPU still fails mainline:
    driver p95 `21.8%`, `coreaudiod` p95 `12.2%`.
  - Capture ISO invariants pass with stop-window warning.
  - Lightweight stream stats show no output active underruns, timeline resets,
    late writes, or completion delta outliers, but real audio lag jumps remain.
  - Failure-mode analysis remains `timebase_or_alignment_instability`.
- Risks:
  - The lack of completion delta outliers in lightweight stats can be a false
    comfort metric; the quality gate is the source of truth.
  - Further timing probes must avoid increasing CPU while chasing lag jumps.
- Next recommended action:
  - Do not enable playback-before-capture-requeue by default.
  - Test a bounded capture-paced lead or explicit scheduling only after an
    offline model predicts fewer lag jumps without higher CPU.
  - Do not expand to full A/B/C/D or Traktor/timecode until Pair A real-music
    quality materially improves.

### Architect Transfer-Rate-Safe Lead Model

- Status:
  - Completed offline only.
  - No HAL install, no hardware, no CoreAudio mutation.
  - Offline gates PASS after the model change.
- Files affected:
  - `tools/transfer_pool_model.cpp`
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - The previous transfer model accepted `lead64` because it only checked pool
    fallback allocations.
  - The updated model records `playback_queue_ratio` and
    `transport_rate_safe`.
  - Default lead1 and mainline-like queue8 stay safe with playback queue ratio
    `1`.
  - Coalesce2 is unsafe with playback queue ratio `0.5`.
  - Implicit lead2, lead4, and lead64 are unsafe with ratios `2`, `4`, and
    `64`.
  - `scripts/run-cpp-offline-gates` passed: Debug `17/17`, Release `18/18`.
- Risks:
  - This is a model-level rejection, not a physical quality improvement.
  - Explicit scheduling may still be viable, but only with a stronger offline
    model and strict fallback because prior explicit-scheduling evidence was
    rejected.
- Next recommended action:
  - Do not physically test implicit `HAL_CAPTURE_PACED_OUT_LEAD>1`.
  - If pursuing scheduling, implement a cadence-preserving explicit scheduler
    model first, then require offline PASS before lock/hardware.

### Architect Reused ISO Completion Handlers Product Probe

- Status:
  - Completed under hardware lock.
  - HAL unloaded after the run.
  - Product HAL build restored.
  - Runtime isolation PASS after cleanup.
- Files affected:
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/PROMOTION_READINESS_STATUS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Isolated `HAL_REUSE_ISOC_COMPLETIONS=1` failed quality:
    quality `0.961164`, SNR floor `9.98 dB`, mid/high residual
    `1.459843/1.377935`, `25` lag jumps.
  - Runtime CPU still fails mainline:
    driver p95 `22.1%`, `coreaudiod` p95 `15.0%`.
  - Capture ISO invariants pass with stop-window warning.
  - Stream stats show no output active underruns, timeline resets, late writes,
    or transfer-pool fallback allocations.
- Risks:
  - The CPU problem remains dominated by IOUSBHost/transport behavior; small
    hot-path cleanup flags have not delivered mainline-level resource use.
  - Combining rejected knobs without a new model risks wasting hardware time.
- Next recommended action:
  - Keep `HAL_REUSE_ISOC_COMPLETIONS=0`.
  - Focus on a transport redesign or a stronger scheduler model that preserves
    1:1 cadence and can be proven offline before another physical run.

### Hume CPU/Hot-Path Explorer

- Status:
  - Completed read-only analysis.
  - No file edits, no hardware, no CoreAudio/USB mutation.
- Mission:
  - Identify CPU/hot-path changes that preserve 1:1 capture-paced cadence and
    can be tested offline before hardware.
- Findings:
  - Recommended isolating `HAL_FAST_ISO_TRANSFER_CONFIG=1` because it reuses
    stable ISO transaction layout without changing request counts or cadence.
  - Recommended isolating `HAL_REUSE_ISOC_COMPLETIONS=1` because it avoids
    per-transfer completion block creation without changing payload or cadence.
  - Suggested input-decode inactive bypass and a future serial/free-list pool
    redesign as deeper candidates.
- Integration result:
  - Reused ISO completion handlers were physically tested and rejected as a
    product default.
  - Fast ISO transfer config was physically tested and rejected as a product
    default.
  - Remaining Hume suggestions require code/model work before more hardware.

### Carver Evidence/Priority Explorer

- Status:
  - Completed read-only analysis.
  - No file edits, no hardware, no CoreAudio/USB mutation.
- Mission:
  - Rank the next physical test by evidence, risk, and required offline
    preconditions.
- Findings:
  - Ranked capture/reference route validation as the lowest-risk high-value
    next physical step because mainline and C++ share failing route signatures
    in several current iRig runs.
  - Ranked timebase/cadence after payload correctness as the next Audio 8 DJ
    target; ledger and payload evidence already reject byte corruption and
    gross underruns.
  - Ranked CPU/transport enqueue work as real but secondary until the route and
    quality gate are trustworthy.
- Integration result:
  - Do not claim audiophile superiority from the current iRig route.
  - Further CPU probes must be justified by a model or isolated implementation
    change; route validation is now the preferred next hardware window if a
    physically valid bypass path exists.

### Architect Fast ISO Transfer Config Product Probe

- Status:
  - Completed under hardware lock.
  - HAL unloaded after the run.
  - Product HAL build restored.
  - Runtime isolation PASS after cleanup.
- Files affected:
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/PROMOTION_READINESS_STATUS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Isolated `HAL_FAST_ISO_TRANSFER_CONFIG=1` failed quality:
    quality `0.959397`, SNR floor `10.19 dB`, mid/high residual
    `1.450623/1.368530`, `35` lag jumps.
  - Runtime CPU still fails mainline:
    driver p95 `23.1%`, `coreaudiod` p95 `25.9%`.
  - Capture ISO invariants pass with no warnings.
  - Stream stats show no output active underruns, timeline resets, late writes,
    or transfer-pool fallback allocations.
- Risks:
  - Descriptor-layout reuse is not the bottleneck; further small hot-path flags
    risk wasting physical windows.
- Next recommended action:
  - Keep `HAL_FAST_ISO_TRANSFER_CONFIG=0`.
  - Prefer capture-route validation or a deeper transport redesign model over
    more one-flag HAL hot-path probes.

### Architect Offline Route-Signature Comparison

- Status:
  - Completed offline only using existing captures.
  - No hardware, no CoreAudio/USB mutation, no HAL install.
- Files affected:
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/PROMOTION_READINESS_STATUS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Mainline wait45, C++ inputdecode-off, and C++ ISO64/q8 StopIO form a
    shared degraded route-family:
    quality about `0.68`, SNR about `-0.83 dB`, mid/high residual about
    `2.53/1.78`, and mid coherence about `0.02`.
  - Current C++ probes form a different failing family:
    quality about `0.96-0.97`, SNR about `10 dB`, residual about `1.4/1.36`,
    and persistent lag jumps.
  - Fixed LTI/EQ and static L/R/polarity remain insufficient explanations.
- Risks:
  - A broken or changing physical capture/reference route can make C and C++
    comparisons misleading.
  - This does not make the C++ candidate ready; it only blocks claims based on
    degraded route comparisons.
- Next recommended action:
  - Validate the capture/reference route with a known-good bypass if available.
  - Otherwise, focus on a transport/timebase redesign and require the current
    C++ family to pass strict music quality before timecode/full routing.

### Architect Inline Inactive Decode Bypass Product Probe

- Status:
  - Completed under hardware lock.
  - HAL unloaded after the run.
  - Product HAL build restored after removing the attempted source change.
  - Runtime isolation PASS after cleanup.
- Files affected:
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/PROMOTION_READINESS_STATUS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Isolated inline inactive input decode bypass failed quality:
    quality `0.961965`, SNR floor `10.16 dB`, mid/high residual
    `1.429792/1.358387`, `31` lag jumps.
  - Runtime CPU still fails mainline:
    driver p95 `22.1%`, `coreaudiod` p95 `41.3%`.
  - Capture ISO invariants pass; stream stats show no gross underruns,
    timeline resets, late writes, or transfer-pool fallback allocations.
- Risks:
  - The bottleneck is not inactive decode dispatch overhead. Keeping this
    change would add hot-path divergence without a measured product benefit.
- Next recommended action:
  - Keep the source reverted.
  - Stop spending hardware windows on isolated micro-optimizations unless a new
    model predicts a measurable quality or CPU outcome.
  - Prioritize capture/reference route validation or transport/timebase
    redesign evidence.

### Carver Follow-Up Failure-Family Analysis

- Status:
  - Completed read-only.
  - No file edits, no hardware, no CoreAudio/USB mutation.
- Mission:
  - Re-rank the current physical quality failure after the inline inactive
    decode bypass run.
- Findings:
  - The current C++ failure family remains quality around `0.96`, SNR around
    `10 dB`, residual around `1.4/1.36`, and persistent lag jumps.
  - Byte packing/routing is a low-priority explanation for this family because
    previous packed-output and payload-guard evidence ruled out simple payload
    corruption.
  - Timebase/cadence or USB/device state after byte preparation remains the
    highest-priority implementation hypothesis.
  - Route/capture-chain validity is still a claim blocker because older
    mainline/C++ captures share a separate degraded family around
    `0.68/-0.83 dB`.
- Integration result:
  - The next accepted physical probe must either validate the route with a
    known-good bypass or change a deeper transport/timebase mechanism while
    preserving payload and 1:1 cadence.

### Architect Output Sample Time Follower Product Probe

- Status:
  - Completed under hardware lock.
  - HAL unloaded after the run.
  - Product HAL build restored with `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=0`.
  - Runtime isolation PASS after cleanup.
- Files affected:
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/PROMOTION_READINESS_STATUS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Isolated `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1` failed quality:
    quality `0.962572`, SNR floor `9.94 dB`, mid/high residual
    `1.458736/1.377276`, `28` lag jumps.
  - Runtime CPU still fails mainline:
    driver p95 `24.7%`, `coreaudiod` p95 `53.0%`.
  - Capture ISO invariants pass; stream stats show no gross underruns,
    timeline resets, late writes, or transfer-pool fallback allocations.
- Risks:
  - Small timeline following is not sufficient; further shallow timebase knobs
    can add CPU without product quality benefit.
- Next recommended action:
  - Keep `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=0`.
  - Either validate the physical capture/reference route independently or
    redesign the USB/device transport state model with new observability before
    another hardware window.

### Architect Current-Family Timebase Window Comparison

- Status:
  - Completed offline only using existing captures.
  - No hardware, no CoreAudio/USB mutation, no HAL install.
- Files affected:
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Local per-window lag correction improves mid-band residual by only about
    `0-2%` across seven current-family C++ Pair A/iRig captures.
  - Corrected mid residual medians remain around `1.41-1.48`.
  - Window lag jumps remain `22-35`.
  - This rejects "simple local lag/slow drift" as a sufficient explanation.
- Risks:
  - Without independent route validation, further driver changes can be
    evaluated against a potentially misleading physical capture route.
  - Without deeper transport observability, USB/device-state hypotheses remain
    plausible but unproven.
- Next recommended action:
  - Prefer known-good route validation if physically possible.
  - Otherwise add observability or redesign around the USB/device transport
    state that occurs after byte payload preparation.

### Architect Practical Mainline Music Floor Comparison

- Status:
  - Completed offline only using C++ evidence and read-only mainline docs.
  - No hardware, no CoreAudio/USB mutation, no mainline writes.
- Files affected:
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Mainline docs consider the mixer REC OUT/iRig route valid, but valid-route
    time-warped music still measured around quality `0.962` and SNR `10 dB`.
  - Best current C++ streamusage run nearly reaches the practical music floor:
    quality, mid residual, quiet noise, lag jumps, and clipping pass the
    practical thresholds.
  - It still fails high residual by a narrow margin:
    `1.358543 > 1.355`.
  - CPU remains far above mainline, so near music-floor performance cannot
    justify promotion.
- Risks:
  - Overweighting the strict absolute SNR gate can hide useful relative
    progress.
  - Overweighting the practical floor can undercut the audiophile goal.
- Next recommended action:
  - Track both gates:
    strict audiophile gate for readiness, practical floor for regression and
    prioritization.
  - Focus next on high-band residual and CPU, with route validation still
    required before any final sound-quality claim.

### Carver Steady CPU Hotspot Audit

- Status:
  - Completed read-only.
  - No hardware, no CoreAudio/USB mutation, no mainline/Rust writes.
- Mission:
  - Audit the exact-PID v5 steady driver sample and rank the next CPU
    optimization with the best chance of moving driver p95 toward `<12%`.
- Findings:
  - The dominant sampled path is capture/playback transfer requeue through
    `IOUSBHostPipe` and `IOConnectCallAsyncMethod`, not Mode 2 packing.
  - `fillPlaybackBytes` is secondary in the sample, and inactive input decode
    is not the decisive CPU path.
  - Recommended an output-only no-capture ISO experiment for playback-only
    operation, while preserving HAL input representation and restoring capture
    when input/DVS activates.
- Integrated action:
  - Added opt-in `HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1`.
  - Default remains `0`.
  - The variant compiles and the local build was restored to the default HAL.
- Risks:
  - Fixed OUT pacing has already been physically rejected in the current HAL.
    This new variant can only become useful if it lowers CPU without repeating
    that quality failure.
  - If the hardware effectively relies on capture completions as the playback
    clock, this experiment may reduce CPU while degrading sound.
- Next recommended action:
  - Do not promote this mode by build success.
  - If physically tested, require HAL safety PASS, reliable submitted/completed
    counters, driver p95 moving materially toward `<=12%`, no underruns/late
    writes/fallback allocations, and physical music not worse than the
    practical floor.

### Architect Ignore HAL Output Sample-Time Experiment

- Status:
  - Completed and physically rejected.
  - Mainline and Rust remained read-only.
  - Hardware lock was used through the safety/soundcheck scripts.
- Mission:
  - Test whether the direct USB abs-deadline/contiguous-write behavior could be
    approximated in HAL by ignoring CoreAudio `mOutputTime.mSampleTime`.
- Files affected:
  - `Makefile`
  - `src/hal/OpenA8DJHAL.c`
  - `docs/BUILD.md`
  - `docs/DECISION_LOG.md`
  - `docs/TEST_EVIDENCE.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - `HAL_IGNORE_OUTPUT_SAMPLE_TIME=1` builds and passes HAL candidate safety.
  - Locked Pair A/iRig music still fails:
    quality `0.963508`, SNR floor `10.20 dB`, mid/high residual
    `1.440572/1.369361`, `32` lag jumps.
  - CPU still fails:
    driver p95 `22.6%`, coreaudiod p95 `44.7%`.
  - Failure-mode analysis remains `timebase_or_alignment_instability`; LTI,
    polarity, matrix, and simple nonlinear fits do not explain the capture.
- Risks:
  - Keeping this as anything but an explicit diagnostic would mislead future
    readiness claims.
  - The direct USB matrix success still needs a deeper explanation than
    sample-time validity alone.
- Next recommended action:
  - Keep `HAL_IGNORE_OUTPUT_SAMPLE_TIME=0`.
  - Focus next on deeper USB/device transport timing or independent route
    validation, not another local `sampleTime` tweak.

### Carver Direct-USB/HAL Pacing Differential Audit

- Status:
  - Completed read-only.
  - No hardware, audio, CoreAudio, USB mutation, or file edits.
- Mission:
  - Explain why direct USB `absdeadline-gain05` passes Pair A matrix while HAL
    ISO8/ISO10 still fails physical music and CPU.
- Findings:
  - `absdeadline` alone is not proven as the winning factor: direct
    `absdeadline-halflags` also uses absolute deadline but fails matrix.
  - The direct winner differs by more than pacing:
    direct USB outside HAL/CoreAudio, `plain-gain05`, queue target `64`,
    large lead, and startup/prefetch margin.
  - HAL ISO8/ISO10 already pass Pair A matrix; product failure is physical music
    continuity/residual/CPU, not static routing.
- Recommended experiment:
  - Test HAL with capture-paced playback retained but direct-like margin:
    `HAL_ISO_FRAMES=8 HAL_PLAYBACK_ISO_FRAMES=8 HAL_CAPTURE_QUEUE=64
    HAL_PLAYBACK_QUEUE=64 HAL_OUTPUT_PREFETCH_FRAMES=256
    HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=0`.
- Risk:
  - Queue/prefetch margin may increase latency and may not reduce CPU.
  - If it only improves matrix, it does not solve product readiness.

### Architect ISO8 Queue64 Prefetch256 Follow-Up

- Status:
  - Completed and physically rejected.
  - Mainline and Rust remained read-only.
  - Hardware lock was used through the physical safety and gate scripts.
- Mission:
  - Execute Carver's direct-like margin recommendation with capture-paced HAL
    playback retained.
- Files affected:
  - `docs/DECISION_LOG.md`
  - `docs/TEST_EVIDENCE.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - HAL candidate safety PASS.
  - Pair A matrix PASS:
    max wrong-source leakage `-53.079 dB`, L->R leakage `-58.221 dB`, R->L
    leakage `-51.442 dB`.
  - Physical music FAIL:
    quality `0.966043`, SNR floor `10.15 dB`, mid/high residual
    `1.442529/1.373910`, `25` lag jumps.
  - CPU FAIL:
    driver p95 `23.7%`, coreaudiod p95 `86.6%`.
  - Stream stats did not show underruns, timeline resets, late writes, elastic
    drops/replays, or pool fallback allocations.
- Risk:
  - Matrix PASS can be misleading if treated as product quality; the music and
    CPU gates remain failing.
- Next recommended action:
  - Stop sweeping queue/prefetch margin blindly.
  - Next useful evidence is either an independent direct-vs-HAL music capture
    using the same fixture/reference, or a transport scheduler/profile change
    that reduces USB/CoreAudio enqueue overhead while preserving music quality.

### Architect Direct USB Music Diagnostic

- Status:
  - Completed and rejected as readiness evidence.
  - Hardware lock was used.
  - Mainline and Rust remained read-only.
- Mission:
  - Build a same-route iRig music wrapper for direct USB playback so direct
    tone-matrix success cannot be over-interpreted.
- Files affected:
  - `scripts/run-direct-usb-soundcheck`
  - `Makefile`
  - `tools/hardware_lock_policy_check.cpp`
  - `docs/DECISION_LOG.md`
  - `docs/TEST_EVIDENCE.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/AGENT_HANDOFFS.md`
- Findings:
  - Direct USB selected Pair A with `plain-gain05`, lead `8192`, real music,
    and iRig capture fails:
    quality `0.103211`, worst-channel SNR `-24.31 dB`, mid/high residual
    `17.114359/16.212469`, no clipping.
  - Direct playback completed and captured, so this is not simply missing
    device visibility or recorder failure.
  - Failure-mode/LTI analysis rejects simple matrix/polarity/nonlinear/LTI
    explanations.
- Risk:
  - Direct tone-matrix results remain easy to misuse. They are only static
    routing diagnostics unless a real-music soundcheck also passes.
- Next recommended action:
  - Keep optimizing the HAL product path against real music and CPU gates.
  - Use direct USB only for narrow falsification, not as a product fallback.

### Gibbs USB Scheduling Explorer

- Status:
  - Completed read-only.
  - No files edited; no hardware/audio/CoreAudio/USB actions.
- Mission:
  - Explain explicit scheduling failure and direct USB physical latency.
- Findings:
  - Explicit scheduling uses nonzero `firstFrameNumber` based on
    `frameNumberWithTime` plus a small lead.
  - The observed failure family is not `too-old`/`too-new`; later
    instrumentation showed `qfail_last=0xe00002be`, `qfail_other`, and queue
    saturation.
  - Direct USB process timing is immediate, so the several-second physical
    latency is not explained by process startup or recorder startup.
- Files affected by architect follow-up:
  - `src/hal/OpenA8DJUSB.h`
  - `src/hal/OpenA8DJUSB.m`
  - `src/tools/opena8dj-usb-play.m`
  - `src/tools/opena8dj-control.c`
  - `Makefile`
  - `scripts/analyze-physical-latency.py`
  - `scripts/evaluate-promotion-readiness.py`
- Risks:
  - Explicit scheduling fallback reduces queue-failure storms but can hide the
    underlying transport failure if treated as a product fix.
- Next recommended action:
  - Do not sweep explicit scheduling further until the physical route/timebase
    failure is isolated with a prompt, high-SNR latency gate.

### Leibniz QA/Metrics Evidence Explorer

- Status:
  - Completed read-only.
  - No files edited; no hardware/audio/CoreAudio/USB actions.
- Mission:
  - Identify gaps in evidence/documentation around latest latency and readiness
    blockers.
- Findings:
  - Physical latency findings were not formalized as a gate.
  - Promotion-readiness evidence needed to include physical latency, not only
    old physical music/CPU/investigation JSON.
  - Traktor/timecode physical evidence remains absent and must block promotion.
- Files affected by architect follow-up:
  - `scripts/analyze-physical-latency.py`
  - `scripts/evaluate-promotion-readiness.py`
  - `docs/SUCCESS_METRICS.md`
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
- Next recommended action:
  - Keep `branch_promotion_allowed=false` until physical latency, physical
    music, CPU, and Traktor/timecode all pass with current evidence.

### Boole Physical Latency Explorer

- Status:
  - Completed read-only.
  - No files edited; no hardware/audio/CoreAudio/USB actions.
- Mission:
  - Determine whether direct-player lead, target latency, or startup silence
    can explain the stable multi-second marker delay.
- Findings:
  - `lead_frames=8192` is about `0.1707s` at 48 kHz.
  - `target_latency=8192` is about `0.1707s`.
  - Observed `startup_silence=4544` is about `0.0947s`.
  - Even with the wrapper's `0.6s` recorder pre-roll, these terms are far
    below the observed `4.646s` marker offset.
  - The next useful discriminator is to collect internal USB diagnostics for
    the same marker and compare consumed/packed buffers against the iRig
    capture.
- Files affected by architect follow-up:
  - `scripts/analyze-latency-marker-peaks.py`
  - `scripts/evaluate-promotion-readiness.py`
  - `docs/SUCCESS_METRICS.md`
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
- Next recommended action:
  - Run one lock-gated marker probe with `--collect-usb-diagnostics`.
  - If internal raw buffers are aligned while iRig is delayed, move diagnosis
    downstream of software timeline/packing.

### Turing Mainline Baseline Evidence Explorer

- Status:
  - Completed read-only.
  - No files edited; no hardware/audio/CoreAudio/USB actions.
- Mission:
  - Confirm historical iRig/mixer capture route and extract comparable
    mainline physical metrics.
- Findings:
  - Historical physical route is confirmed:
    Open Audio 8 DJ output -> external mixer -> mixer REC OUT -> iRig Stream
    input -> macOS capture.
  - Mainline baseline also failed strict audiophile music thresholds even when
    the route was working:
    music SNR around `10..11 dB`, alignment below `0.98`, and lag jumps above
    `0`.
  - Later mainline docs also show real iRig availability failures after reset,
    so route/device availability and audio-quality failure must be separated.
- Risk:
  - A C++ candidate can be better than recent mainline in isolated tone metrics
    but still not be acceptable product quality.
- Next recommended action:
  - Require same-day A/B physical evidence or a stronger route-independent
    oracle before any claim that C++ beats mainline.

### Poincare USB Device-State Explorer

- Status:
  - Completed read-only.
  - No files edited; no hardware/audio/CoreAudio/USB actions.
- Mission:
  - Identify remaining state/control/USB hypotheses after internal marker
    buffers and packed Mode 2 bytes aligned perfectly.
- Findings:
  - Remaining high-value hypotheses are downstream of PCM packing:
    ISO OUT cadence/layout, USB alternate-setting/device state reset,
    transport policy differences, playback control bytes, and physical route.
  - `HAL_VALID_CAPTURE_OUT_LAYOUT=1` and prior offset-fix runs already reject
    simple output-layout changes.
  - Playback control state was inconsistent: the C++ offline playback profile
    expected CAIAQ input mode `1` and software lock off, while direct USB ran
    markers with control bytes `00:02:03:01:02:01`.
- Files affected by architect follow-up:
  - `src/tools/opena8dj-control.c`
  - `src/hal/OpenA8DJUSB.h`
  - `src/hal/OpenA8DJUSB.m`
  - `src/tools/opena8dj-usb-play.m`
  - `scripts/run-direct-usb-soundcheck`
  - `scripts/summarize-physical-runs.py`
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/ARCHITECT_CONTEXT.md`
- Follow-up result:
  - Forced playback-profile marker applied control bytes
    `01:02:03:00:02:00`, but still failed:
    marker mean `4.667208s`, physical latency FAIL, quality FAIL.
- Next recommended action:
  - Test USB alternate-setting reset/close-open state behavior under lock with
    a marker, then compare against same-day mainline C if the route still
    fails.

### Noether Same-Day Mainline A/B Safety Planner

- Status:
  - Completed planning only.
  - No hardware/audio/CoreAudio/USB action executed.
  - No HAL installed, unloaded, or reloaded.
  - No defaults, sample rates, buffer sizes, USB devices, or services changed.
  - Mainline `/Users/fer/dev/opena8dj` was read only; Rust was not touched.
- Mandatory warning given:
  - "PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana."
- Mission:
  - Prepare an exact, reversible plan for a physical same-day A/B comparison
    against the C/Obj-C mainline if the architect chooses to temporarily activate
    mainline under the global hardware lock.
- Mainline read-only findings:
  - `make install-hal` delegates to `scripts/test-hal-candidate-safety` and can
    install a HAL bundle with safety checks, but its default evidence path is
    inside mainline `local-analysis`; do not run it directly for the C++ A/B
    campaign.
  - `scripts/install-signed-build.sh` mutates system state directly:
    `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver`,
    `/usr/local/bin/opena8dj-control`, `/usr/local/bin/opena8dj-midid`,
    `/Library/LaunchAgents/org.opena8dj.midid.plist`, and restarts
    `coreaudiod`. Do not use it for the A/B because it writes mainline evidence
    and has broader mutations than needed.
  - Package install/uninstall scripts also mutate `/usr/local/bin`,
    `/Library/LaunchAgents`, `/Library/Documentation/OpenA8DJ`, active HAL, and
    restart `coreaudiod`; avoid the PKG path for a measurement-only same-day A/B.
  - `scripts/test-hal-candidate-safety` shows the minimal safe system mutation
    model: move any active OpenA8DJ HAL to `/Library/Audio/Plug-Ins/HAL.disabled`,
    copy candidate to `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver`, ad-hoc sign
    in place, restart `coreaudiod`, enumerate, run guard, then unload unless
    explicitly leaving loaded.
  - `scripts/run-soundcheck` records from a named capture device and plays via
    Core Audio, but it writes under its repo root by default. For this plan, use
    only C++ worktree tools/wrappers and evidence paths.
- Required evidence root:
  - `/Users/fer/dev/audio8djcpp/local-analysis/mainline-ab/<timestamp>/`
- Safe plan, no execution in this subagent:
  1. Preflight while unlocked or under a short lock check: confirm no active
     OpenA8DJ HAL, or record exact active HAL hash/version and destination where
     it will be parked. Confirm Audio 8 DJ and iRig Stream are visible using
     passive enumeration only.
  2. Create the A/B evidence root in the C++ worktree only. Write `manifest.txt`
     with timestamp, operator, machine, branch names, git hashes, iRig route,
     cable route, capture channels, requested sample rate, buffer size, and
     exact no-default-change policy.
  3. Snapshot the mainline candidate read-only by copying
     `/Users/fer/dev/opena8dj/build/OpenA8DJ.driver` or an explicitly selected
     archived mainline `OpenA8DJ.driver` into
     `<evidence>/inputs/mainline/OpenA8DJ.driver`. If no trustworthy built
     bundle exists, abort; do not build inside mainline.
  4. Snapshot the C++ candidate into `<evidence>/inputs/cpp/`, using a C++ built
     artifact or direct USB tool manifest. Do not install C++ HAL unless the
     architect explicitly changes this A/B to HAL-vs-HAL.
  5. Acquire the global lock:
     `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"`.
     If busy and owner PID is alive, abort and report owner; do not wait while
     holding the lock.
  6. Record pre-install state in `<evidence>/preflight/`: current HAL presence,
     active HAL hash/version if present, Core Audio device list, iRig visibility,
     Audio 8 DJ USB visibility, watched CPU, and current default input/output
     names for audit only. Do not change defaults.
  7. Install mainline from the C++ evidence snapshot with the minimal safety
     model: move active OpenA8DJ HAL to `<evidence>/system-backup/` metadata plus
     `/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.ab-preexisting-*`,
     copy snapshot to `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver`, strip xattrs,
     ad-hoc sign in place if required, restart `coreaudiod`, then wait for
     enumeration. Do not install mainline tools or LaunchAgent unless the exact
     test requires them.
  8. Run mainline HAL physical measurements from the C++ worktree:
     marker latency, real music soundcheck, CPU profile, stream/status snapshot
     if available, and physical music quality gate. Store everything under
     `<evidence>/mainline/`.
  9. Unload mainline immediately after measurement: move active
     `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver` to
     `/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.ab-mainline-*`, restart
     `coreaudiod`, and verify Core Audio enumeration/CPU recovery.
  10. Run the C++ candidate measurement in the same lock window and same physical
      route. Prefer direct USB C++ if that is the candidate under test; only
      install C++ HAL if the architect chooses a HAL-vs-HAL comparison. Store
      everything under `<evidence>/cpp/`.
  11. Run postflight: unload any OpenA8DJ HAL unless the architect explicitly
      requests leaving one active, restart Core Audio once if a HAL was active,
      verify iRig and Audio 8 DJ visibility, verify watched CPU is back to idle,
      and write `postflight/state.json`.
  12. Release the lock immediately.
  13. Run offline comparison after lock release: normalize by fixture hash,
      capture device, capture channels, sample rate, duration, and same-day
      timestamp; write `comparison.json`, `comparison.md`, and PASS/FAIL fields.
- Mutations that would occur during the authorized A/B:
  - System HAL directory:
    `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver` copied/moved/removed.
  - Disabled HAL parking directory:
    `/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.*` created.
  - Active HAL bundle xattrs/signature may be changed after copying.
  - `coreaudiod` restarted after install and after unload.
  - Audio 8 DJ playback and iRig capture consume physical audio route.
  - CPU measurements are sensitive while playback/capture are running.
- Mutations intentionally avoided:
  - No writes to `/Users/fer/dev/opena8dj`.
  - No writes to `/Users/fer/dev/audio8djrust`.
  - No mainline `local-analysis` artifacts.
  - No package install.
  - No `/usr/local/bin` tool overwrite unless separately authorized.
  - No LaunchAgent install/bootstrap unless separately authorized.
  - No default input/output change.
  - No sample-rate/buffer change except what the candidate driver exposes for
    its own stream during the test.
  - No USB reset unless a separate recovery window is authorized.
- Evidence to capture for each side:
  - Candidate source path, copied snapshot hash, active installed hash, version,
    build string, signing status, and git status/hash from source repo.
  - Passive USB inventory proving Audio 8 DJ and iRig Stream visibility.
  - Core Audio enumeration before install, after install, after measurement, and
    after unload.
  - Marker latency metrics: first energy, marker offset mean/std, paired peaks,
    correlation, linear-fit SNR, residual/capture RMS.
  - Real music metrics: quality alignment, SNR floor, mid/high residual ratios,
    click outliers, lag jumps, clipping, quiet-window noise, coloration, and
    CPU/residual coupling.
  - CPU profile: OpenA8DJ driver avg/p95, coreaudiod p95, audio services,
    player, recorder, WindowServer, and total watched CPU.
  - Transport/driver counters when available: underruns/overruns, queue
    failures, transfer errors, stream stats, control bytes/profile.
  - Raw artifacts: reference WAV hash, captured WAV, play log, record log,
    metrics JSON, summary text, lock owner record, install/unload logs.
- PASS condition for a claim that C++ beats mainline:
  - Same-day route-valid measurements exist for both sides.
  - C++ passes absolute product gates.
  - C++ is not merely less bad than a failing mainline; it must pass the
    audiophile thresholds in `docs/SUCCESS_METRICS.md`.
  - C++ beats or matches mainline on CPU, jitter/latency stability, underruns,
    channel/routing correctness, and timecode-relevant signal integrity.
  - Postflight leaves Core Audio, USB visibility, and iRig capture healthy.
- Risks:
  - Mainline source tree is dirty and may not correspond to a known release.
    Use an explicitly selected built/archived bundle and record hash, or abort.
  - Installing HAL can hang enumeration or drive `coreaudiod` hot; recovery must
    unload OpenA8DJ and re-check audio stack before continuing.
  - Running mainline wrappers would write into mainline; do not use them.
  - Installing `/usr/local/bin/opena8dj-control` from mainline can overwrite C++
    tools or alter later diagnostics; avoid for the minimal A/B.
  - Physical route can drift between tests; keep both measurements in one lock
    window with unchanged cabling, gain, capture channels, and fixture.
  - A HAL-vs-direct-USB comparison is useful diagnostically but not a fair driver
    product comparison. Label it clearly unless both candidates use the same
    Core Audio path.
- Restoration policy:
  - Default final state after the A/B is OpenA8DJ HAL unloaded unless the
    architect explicitly requests otherwise.
  - Any pre-existing active HAL is parked with hash/version metadata, not
    destroyed.
  - If postflight health fails, unload active OpenA8DJ, restart `coreaudiod`,
    collect guard logs, and stop; do not proceed to more audio tests.
- Recommended architect decision:
  - Only run this if the next C++ candidate has already passed offline gates and
    a direct physical route sanity check. The A/B is expensive and mutates
    system audio state; it should answer promotion-readiness, not debug basic
    route failure.

## Subagent: Offline Music/Marker Failure Analyst - 2026-06-17

- Mandatory warning received and followed:
  `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
  instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
  /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
  escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
  sin lock global y sin autorización de ventana.`
- Scope:
  - Read-only offline analysis of existing evidence under
    `local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/`
    and
    `local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s/`.
  - No hardware, USB, Core Audio, HAL install/reload, defaults, or mainline/Rust
    mutation.
- Findings:
  - The alt0+playback-profile marker gate is a latency-only partial pass:
    `marker-peak-summary.json` has `offset_mean_seconds=0.405589`,
    `offset_std_seconds=0.001256`, and `paired_peaks=4`, but
    `physical-latency.json` still fails signal integrity with
    `best_correlation=0.414578`, `aligned_snr_db=-3.99`,
    `linear_fit_snr_db=-6.76`, and
    `linear_residual_over_capture_rms=0.908807`.
  - The music gate's stored global alignment is wrong for this dense/repetitive
    track. `metrics.json` uses `capture_start=34316`, `reference_start=81`, but
    an offline local scan of the existing WAVs finds the best mono correlation
    at `capture_start=21852`, i.e. `-12464` frames / `-0.259667s` from the
    stored start. At the stored alignment mono correlation is only `0.062356`;
    at the corrected start it rises to `0.943492`.
  - The corrected music alignment is close to the marker timing:
    `(21852 - 81) / 48000 = 0.4536s`, versus marker mean `0.4056s`.
    Therefore the apparent music failure is partly an analyzer alignment
    failure, not purely transport failure.
  - Correcting alignment is not enough for audiophile readiness. At the best
    corrected start, a 2x2 static matrix fit still gives only
    `snr_matrix=8.84dB`, residual/capture RMS `0.340`, and predicted/capture
    RMS `0.941`. This is far below the product threshold and still points to
    analog transfer, route/capture, gain/EQ, or remaining device-state quality
    problems.
  - Polarity and simple L/R swap are not sufficient explanations. Existing
    `failure-modes.json` already reports
    `static_lr_mix_or_polarity_not_sufficient` and
    `simple_memoryless_nonlinearity_not_sufficient`; the offline scan agrees.
  - Clipping is not the cause: music `capture_clipped_frames=0`, marker
    `capture_clipped_frames=0`, and capture peaks remain below full scale.
  - The music capture is strongly mono-correlated (`capture LR corr=0.9872`,
    reference LR corr `0.9868`) and the track itself is highly repetitive, which
    makes the current global music alignment fragile. Use decorrelated or
    watermarked music/test content for alignment before judging residuals.
- Next minimal tests:
  1. Offline first: add or run an analyzer mode that anchors music alignment
     from marker timing or from a wide normalized correlation scan, then reports
     both raw and corrected quality.
  2. Offline first: create a deterministic decorrelated music-like fixture with
     low-level watermark/transients every few seconds, so L/R route, polarity,
     drift, and residual can be measured without ambiguous dance-music loops.
  3. Hardware only after lock: rerun direct USB alt0+playback-profile with
     `--collect-usb-diagnostics` on the same deterministic fixture to prove
     whether USB packed/consumed audio is clean while analog/iRig residual
     remains high.
  4. If residual remains high with corrected alignment and clean USB diagnostics,
     isolate the external route: same iRig/mixer gain with a known non-Audio8
     source, then same direct Audio 8 output with a simple swept/decorrelated
     fixture.

## Architect Integration: Continuous Timeline Reset Fix - 2026-06-17

- Trigger:
  - Darwin's offline analysis showed that music alignment was fragile but not
    sufficient to explain the poor residual.
  - Architect reran alt0/playback-profile music with USB diagnostics and found
    a 12-second internal mismatch despite short packed probes passing.
- Root cause:
  - `OutputTimelineWrite` reset continuous writes when producer lead exceeded
    half the ring. The failing run reset at write frame `162048` and inserted
    mid-music silence/underrun gaps around served frame `145600`.
- Integrated code change:
  - Continuous writes at `ring->maxWrittenFrame + 1` no longer trigger the
    future-gap reset.
  - File changed: `src/hal/OpenA8DJUSB.m`.
- Verification:
  - Post-fix direct USB diagnostic run:
    `local-analysis/direct-usb-soundcheck/20260617-no-continuous-reset-alt0-music-pairA-12s-usbdiag/`.
  - Internal written/consumed/packed USB all pass for 12 seconds:
    alignment `1.000000`, lag `0`, SNR `999.00 dB`, USB check errors `0`,
    panic flags `0`.
  - Physical iRig music improved but remains product FAIL:
    quality `0.957628`, SNR `9.38 dB`, mid/high residual
    `1.422297/1.413835`.
  - Runtime isolation PASS:
    `local-analysis/runtime-isolation/20260617-after-continuous-reset-fix.json`.
- Risk:
  - The fixed direct USB path proves packet/data-plane integrity, but not
    audiophile physical readiness.
  - The HAL CPU and same-day mainline comparison still need fresh evidence on
    the fixed candidate.
- Next recommended action:
  - Run a same-day mainline/C++ physical A/B only from C++ evidence snapshots
    and under lock, or first isolate the iRig/mixer route with a known-good
    source. Do not promote to main.

## Architect Integration: Decorrelated Direct USB Boundary - 2026-06-17

- Trigger:
  - After the continuous timeline reset fix, direct USB real-music diagnostics
    were internally perfect but physical iRig quality still failed. A
    decorrelated fixture was needed to remove ambiguous dense-music alignment.
- Evidence:
  - Run:
    `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag`.
  - Promotion:
    `local-analysis/promotion-readiness/20260617-after-decorrelated-direct-usb.json`.
  - Isolation:
    `local-analysis/runtime-isolation/20260617-after-decorrelated-direct-usb.json`.
- Integrated finding:
  - Internal written, consumed, and packed USB outputs are exact for 12 seconds:
    alignment `1.000000`, lag `0`, SNR `999.00 dB`, USB `check_errors=0`,
    USB `panic_flags=0`.
  - Physical Pair A routing/crosstalk passes on the decorrelated fixture:
    `max_wrong_source_leakage_db=-57.447168`, with no clipping.
  - Physical waveform quality still fails badly:
    quality `0.721193`, SNR floor `-2.96 dB`, mid/high residual
    `2.117458/2.018361`.
- Risk:
  - Do not let the Pair A routing PASS become a readiness claim. It is a useful
    isolation result, not proof that the product HAL beats mainline or that DVS
    is ready.
  - Same-day mainline A/B and fixed-candidate HAL CPU remain mandatory before
    any branch promotion discussion.
- Next recommended action:
  - If hardware is available, run a lock-gated same-day product HAL A/B with the
    same decorrelated fixture and music gate against mainline, using evidence
    rooted in `/Users/fer/dev/audio8djcpp/local-analysis` and leaving
    `/Users/fer/dev/opena8dj` read-only.
  - If avoiding system mutation, improve offline/C++ analysis only; it cannot
    close the physical quality, CPU, or Traktor/timecode gates.

## Rawls: Post-Decorrelated Evidence Audit - 2026-06-17

- Mission:
  - Independently audit the current evidence after
    `20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag`.
  - Scope was read-only/offline; no hardware, CoreAudio, USB reset, driver
    install, or mainline/Rust mutation.
- Findings:
  - Confirms direct USB data-plane PASS:
    `written_alignment_score=1.000000`,
    `consumed_alignment_score=1.000000`,
    `usb_alignment_score=1.000000`, SNR `999.00 dB`,
    `usb_check_errors=0`, `usb_panic_flags=0`.
  - Confirms physical Pair A routing PASS:
    `max_wrong_source_leakage_db=-57.447168`,
    `left_to_right_leakage_db=-61.527228`,
    `right_to_left_leakage_db=-55.793274`,
    `expected_floor_amplitude=0.147371`, clipping `0`.
  - Confirms physical quality FAIL:
    `quality_alignment_score=0.721193`, SNR `-2.96 dB`,
    mid/high residual `2.117458/2.018361`,
    quiet mid-band noise `-21.77 dBFS`.
  - Confirms promotion blockers:
    `latest_music_cpu_pair`, `physical_latency_alignment`,
    `physical_music_quality`, `runtime_cpu_beats_mainline`,
    `latest_physical_investigation`, and `traktor_timecode_physical`.
- Risks noted:
  - `evaluate-promotion-readiness.py` selects some evidence by latest mtime.
    `latest_music_cpu_pair` catches music/CPU family mismatch, but latency,
    marker, USB, and matrix evidence can still come from different candidate
    families.
  - `evidence:*` gates mostly prove file existence; semantic gates must remain
    the authority for readiness.
  - `metrics.json` should persist `result` or `verdict` consistently instead of
    relying on wrapper output plus threshold inference.
- Minimum objective next measurement:
  - Same-day C++ vs mainline product HAL A/B in the same physical route, using
    the same fixture/music, sample rate, buffer/capture device, and comparator.
  - Required artifacts for both drivers: music `metrics.json`, same-run
    `cpu-profile.tsv`, decorrelated `tone-matrix.json`, runtime isolation, and
    a comparison report proving C++ quality is at least mainline and CPU is no
    worse than mainline.

## Architect Integration: Same-Day Mainline A/B Rejection - 2026-06-17

- Trigger:
  - Rawls identified same-day product HAL A/B as the minimum objective evidence
    needed before any branch promotion.
- Safety:
  - Hardware lock used for HAL load/reload, CoreAudio restart, playback, and
    iRig capture.
  - Mainline remained read-only; the mainline HAL bundle was copied into
    `/Users/fer/dev/audio8djcpp/local-analysis`.
  - Final force-unload cleanup left HAL inactive and lock absent.
- Evidence:
  - Root:
    `local-analysis/mainline-ab/20260617-sameday-ab-085735`.
  - Comparison:
    `local-analysis/mainline-ab/20260617-sameday-ab-085735/ab-comparison.json`.
- Findings:
  - C++ product HAL safety load PASS, soundcheck FAIL:
    quality `0.134709`, SNR `-12.66 dB`, mid/high residual
    `4.904891/4.494813`, lag jumps `18`, driver CPU p95 `23.2%`,
    coreaudiod p95 `20.5%`.
  - Mainline HAL retry safety load PASS, soundcheck FAIL:
    quality `0.246599`, SNR `-13.28 dB`, mid/high residual
    `5.774651/5.636904`, lag jumps `41`, driver CPU p95 `5.6%`,
    coreaudiod p95 `10.3%`.
  - C++ has partial wins in residual ratios, lag jumps, and SNR floor.
  - C++ loses global quality alignment and CPU, so it does not beat mainline.
- Decision:
  - `FAIL_CPP_NOT_BETTER_THAN_MAINLINE`.
  - Do not move C++ to `main`.
  - Do not move C mainline to `Legacy`.
- Next recommended action:
  - Focus on product HAL CPU and physical alignment/quality. Direct USB packet
    correctness is no longer the dominant blocker for branch promotion.

## Einstein: C++ Versus Mainline HAL CPU Divergence Audit - 2026-06-17

- Mission:
  - Inspect C++ and mainline HAL behavior read-only and explain why C++ physical
    runs consume much more OpenA8DJ driver CPU than the same-day mainline A/B.
  - No edits, no hardware, no CoreAudio, no USB, no installs.
- Warning:
  - PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj` o
    `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo puedes
    escribir en `/Users/fer/dev/audio8djcpp`. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.
- Findings:
  - Primary CPU suspect: C++ default transfer shape was ISO8/current-cadence,
    while mainline evidence used ISO64-like lower completion pressure. C++
    driver p95 stayed around `23%`; mainline was around `5.6%`.
  - C++ had disabled fast unrolled output packing and prefetch clearing by
    default while mainline uses the faster output path.
  - Disabled transfer-ledger Objective-C messages still existed at call sites
    before the prune and were unnecessary callback-adjacent work.
  - Capture-paced playback magnifies per-transfer cost.
  - Input decode is not the primary CPU suspect for the playback-heavy
    evidence.
- Integrated actions:
  - Tested ISO64+unrolled and coalesce2+unrolled under lock.
  - Rejected both as defaults because they reduce CPU while collapsing physical
    quality.
  - Pruned disabled transfer-ledger call sites as callback hygiene only.
- Files affected:
  - None by the subagent directly.
  - Architect integration touched `src/hal/OpenA8DJUSB.m` and documentation in
    this worktree only.
- Risks:
  - The CPU gap is real, but a CPU-only fix can be actively harmful if it
    changes USB/audio cadence.
  - `playbackTransfersSubmitted` is not trustworthy as readiness evidence until
    its counter contract is fixed.
- Next recommended action:
  - Fix stream-stats/control-tool observability first, then continue
    cadence-preserving CPU reductions. Do not retry ISO64/coalescing as product
    candidates without a new physical-quality hypothesis.

## Confucius: Transport Cadence Analyst - 2026-06-17

- Mission:
  - Compare C++ transport/cadence behavior with read-only mainline and explain
    the corrected zero-complete capture pattern.
- Warning:
  - PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj` o
    `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo puedes
    escribir en `/Users/fer/dev/audio8djcpp`. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.
- Current integration status:
  - Completed. Notes written to
    `local-analysis/subagent-transport-cadence-notes.txt`.
  - Integrated with architect-side stream-stats denominator correction and rate
    analysis.
- Findings:
  - Mainline default build geometry is ISO64/q8 while the current C++ default
    geometry is ISO8/q8.
  - Capture and playback `firstFrameNumber`, isochronous options, completion
    reuse defaults, fast ISO config defaults, and audio-param reset/start
    intent are not the strongest divergences.
  - Capture-paced playback propagates only non-zero capture completions into
    playback requests. With ISO8, that gives roughly 4-5 playback
    transactions per transfer in the latest physical run.
- Architect interpretation:
  - Exact ISO64/q8 has already been physically rejected for C++ quality, even
    though it reduces CPU.
  - The partial ISO8 layout also matches output-rate math; forcing all 8 slots
    would over-read audio. The next candidate must preserve ~48 kHz output
    consumption while improving quality and CPU.
- Files affected by architect while waiting:
  - `Makefile`
  - `src/hal/OpenA8DJUSB.m`
  - `src/tools/opena8dj-control.c`
  - `scripts/run-soundcheck`
  - `scripts/analyze-stream-stats.py`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/DECISION_LOG.md`
  - `docs/TEST_EVIDENCE.md`
- Next recommended action:
  - Do not repeat plain ISO64/q8. Investigate pacing/rate-preserving transport
    smoothing only if it can be modelled offline first and then measured
    under lock against physical quality and CPU.

## Socrates: HAL Hot-Path CPU Scout - 2026-06-17

- Mission:
  - Read-only audit of the C++ HAL hot path for CPU candidates that preserve
    the already modelled rate shape and avoid rejected transport knobs.
- Warning:
  - PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj` o
    `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo puedes
    escribir en `/Users/fer/dev/audio8djcpp`. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.
- Current integration status:
  - Completed. No files changed by the subagent.
- Findings:
  - Highest-priority candidate:
    reuse ISO transfer layout/request geometry when sample rate, request list,
    and transaction count are unchanged. This should not change payload bytes,
    ISO8/q8/default pacing, coalescing, lead, or rate shape.
  - Second candidate:
    reuse completion handlers in pooled transfers, but only with strict lifetime
    handling. This reduces block creation risk but has concurrency/lifetime
    hazards.
  - Third candidate:
    build a new byte-parity-proven specialized HAL packer rather than enabling
    the physically rejected existing `HAL_UNROLLED_OUTPUT_PACK` path.
  - Do not candidate ISO64/q8, coalescing, output-only, full-8 ISO8 layout,
    fixed OUT pacing, `VALID_CAPTURE_OUT_LAYOUT`, or loss of observability as
    primary fixes.
- Architect integration:
  - Added a static rejected-default policy gate before attempting another CPU
    optimization, so accidental promotion of previously rejected defaults fails
    offline.
- Next recommended action:
  - Implement the ISO layout reuse path behind offline/contract evidence first,
    then require locked physical evidence for hot-path timing, quality, CPU,
    stream stats, and rate shape before any readiness claim.

## Mencius: HAL Timebase And Sample-Time Scout - 2026-06-17

- Mission:
  - Read-only comparison of mainline HAL timing, zero timestamp, output
    `sampleTime`, timeline write, and pacing behavior against C++.
- Warning:
  - PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj` o
    `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo puedes
    escribir en `/Users/fer/dev/audio8djcpp`. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.
- Current integration status:
  - Completed. No files changed by the subagent.
- Findings:
  - Mainline public HAL time is anchored to `mach_absolute_time()` by default;
    `HAL_USB_ZERO_TIMESTAMP` is `0`, so USB zero timestamp is not the likely
    explanation for current lag jumps unless logs prove otherwise.
  - Mainline and C++ both consume `mOutputTime.mSampleTime` and use the same
    `8192/4096/8192` output latency constants.
  - C++ had one relevant timing divergence: it could flush output inside
    `WriteMix` once expected streams were present, while mainline waits until
    `EndIOOperation`.
  - C++ also has a continuity exception in timeline write that mainline lacks;
    existing diagnostic evidence must determine whether timeline resets are
    actually present before changing that path.
- Architect integration:
  - Added `HAL_FLUSH_OUTPUT_IN_WRITE_MIX`, default `0`, to align C++ with
    mainline end-of-cycle flush timing.
  - Added the default to static policy checks so the early flush cannot become
    product default accidentally.
- Next recommended action:
  - Test the new output-flush timing under the hardware lock with same-route
    real-music quality, window-trace timebase classification, CPU profile, and
    promotion-readiness evidence. Do not claim improvement from compilation.

## Russell: Analog Residual Scout - 2026-06-17

- Mission:
  - Read-only analysis of existing physical artifacts to rank whether the
    persistent residual is route/capture/iRig/mixer, variable latency, fixed
    spectral/gain distortion, or USB/HAL cadence.
- Warning:
  - PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj` o
    `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo puedes
    escribir en `/Users/fer/dev/audio8djcpp`. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.
- Current integration status:
  - Completed. No files changed by the subagent.
  - Architect integrated the findings into
    `scripts/summarize-physical-product-evidence.py` and
    `local-analysis/physical-product/20260617-product-evidence-summary.json`.
- Findings:
  - Mainline and C++ ISO64/q8 can fail similarly on the shared iRig route,
    which means that route cannot be treated as an audiophile reference unless
    same-session fixture health is proven.
  - Current C++ runs show real lag jumps, but local lag correction alone does
    not remove the residual.
  - USB packed payload evidence is clean, so the strongest next hypotheses are
    physical route/fixture state, time-varying latency, and HAL/USB cadence
    behavior rather than simple byte-packing corruption.
  - Static L/R matrix and simple nonlinear gain models are weak explanations
    for the key captures.
- Architect integration:
  - Installed NumPy/SciPy into the ignored local `.venv/` for offline analysis.
  - Ran `analyze-soundcheck-failure-modes.py` on key existing captures and
    confirmed static matrix/nonlinearity improvements are below `0.25 dB`.
  - Added a physical-product summary gate that keeps same-session C++ vs
    mainline comparisons separate from best-global C++ runs.
- Next recommended action:
  - Do not spend more hardware windows on blind cadence sweeps. Either validate
    the physical fixture in a same-session reference test or reduce product HAL
    CPU without changing the currently best quality family.

## Arendt: Fractional Residual Hypothesis Scout - 2026-06-17

- Mission:
  - Read-only review of current evidence to find an actionable physical-quality
    hypothesis beyond ISO/coalescing/cadence sweeps.
- Warning:
  - PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj` o
    `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo puedes
    escribir en `/Users/fer/dev/audio8djcpp`. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.
- Current integration status:
  - Completed. No files changed by the subagent.
  - Architect converted the hypothesis into an offline diagnostic script and
    evidence run.
- Findings:
  - USB internal evidence is clean while iRig physical capture still fails,
    so the remaining residual could plausibly have been fractional delay,
    group delay, analog route/capture response, or physical device state.
  - The fixture decorrelation evidence shows Pair A routing/leakage is broadly
    correct, so simple channel swap/leakage is not enough to explain failure.
  - Do not repeat ISO64, coalescing, unrolled packer, gain/polarity/L/R matrix,
    or integer-lag-only tests as standalone fixes.
- Architect integration:
  - Added `scripts/analyze-fractional-time-warp.py`.
  - Ran LTI transfer, failure-mode, and fractional time-warp diagnostics over
    six existing C++/mainline physical captures.
  - Current best C++ captures reject fractional time-warp as dominant: all
    scalar/matrix SNR improvements are below `1 dB`, far below the `3 dB`
    partial-explanation threshold.
- Risks:
  - The degraded same-session A/B captures still show large warp sensitivity,
    so future same-session comparisons must include fixture-health checks
    before product claims.
- Next recommended action:
  - Focus on reducing HAL USB enqueue CPU while preserving the best current
    quality family, or run a locked same-session fixture reference when a new
    physical window is justified.

## Noether: Transport CPU Frontier Scout - 2026-06-17

- Mission:
  - Read-only audit for the next CPU/transport implementation hypothesis that
    reduces HAL/USB enqueue CPU without repeating rejected knobs or breaking
    quality.
- Warning:
  - PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj` o
    `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo puedes
    escribir en `/Users/fer/dev/audio8djcpp`. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.
- Current integration status:
  - Completed. No files changed by the subagent.
  - Architect integrated the recommendation into the transport budget frontier
    model and DriverKit transport direction.
- Findings:
  - The only credible next candidate is a prepared DriverKit/transport backend:
    keep cadence, bytes, routing, DVS, input decode, and USB packet semantics
    fixed while moving steady-state isochronous requeue work out of the HAL
    callback path.
  - Do not repeat rejected knobs: coalescing, ISO64/q8, pool cursor, input
    decode gating, stats-off, atomic stats, fast ISO config, or raw/reused
    completion handlers.
  - Offline proof should precede hardware: simulated DriverKit transport must
    show zero direct HAL steady-state `IOUSBHostPipe` enqueue/requeue work, no
    fallback allocations, stable cadence, and preserved DVS/timecode gates.
  - Risk remains high for timecode/input if a ring loses order, timestamps, or
    A/B/C/D channel identity.
- Next recommended action:
  - Implement an offline DriverKit transport contract/model first, then only
    request a locked physical window if all offline gates pass.

## Sagan: Prepared Transport Recovery Reviewer - 2026-06-17

- Mission:
  - Read-only review of `PreparedTransportBackend` lifecycle invariants and
    recommended checks for a recovery contract.
- Warning:
  - PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj` o
    `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo puedes
    escribir en `/Users/fer/dev/audio8djcpp`. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.
- Current integration status:
  - Completed. No files changed by the subagent.
  - Architect integrated the findings into
    `tools/prepared_transport_recovery_contract.cpp` and
    `PreparedTransportBackend::safety()`.
- Findings:
  - Existing tests proved stable flow but not restart as a barrier against
    stale frames, old counters, or old timestamp history.
  - A never-started backend could look product-safe if safety stayed
    counter-only; that needed an explicit rejection.
  - The recovery gate should cover invalid configs, post-stop operation
    rejection, restart counter reset, timestamp-history reset, stale-frame
    isolation, and explicit invalid-restart policy.
- Next recommended action:
  - Keep recovery as an offline gate and require the future real DriverKit/USB
    adapter to satisfy the same contract before any locked hardware recovery
    window.
## 2026-06-17: Zeno - Metrics Explorer

Prompt summary:
- Read-only inspection of `/Users/fer/dev/audio8djcpp` scripts/docs around
  channel matrix, leakage, loopback quality, and physical quality gates.
- Same mandatory safety warning: no mutation in `/Users/fer/dev/opena8dj` or
  `/Users/fer/dev/audio8djrust`; no hardware/audio/CoreAudio/USB.

Findings:
- Existing C++ already covers digital routing/leakage in
  `tools/simulated_output_matrix.cpp`, basic loopback analysis in
  `tools/loopback_quality_analysis.cpp`, and synthetic routing/timecode leakage
  in `tools/prepared_transport_routing_timecode_contract.cpp`.
- The highest-value next metric is a C++ analyzer over stored physical capture
  directories, not another hardware run.
- Fields to port from Python:
  `left_to_right_leakage_db`, `right_to_left_leakage_db`,
  `max_wrong_source_leakage_db <= -45.0`, `expected_floor_amplitude >= 0.005`,
  `capture_clipped_frames == 0`, plus quality alignment, SNR, click outliers,
  lag jumps, band residuals, quiet-band noise, and optional CPU coupling.

Files affected by subagent:
- None. Read-only only.

Integration:
- Added a narrower C++ digital tone-leakage contract in this pass.
- Implemented the next recommended analyzer as
  `opena8djcpp_capture_matrix_quality_analysis`.
- The first version reads existing `fixture/reference.wav` and `captured.wav`
  without touching hardware and reports alignment, SNR/correlation, clicks,
  clipping, and tone-domain leakage.

## 2026-06-17: Dalton - Capture Matrix Analyzer Worker

Prompt summary:
- Implement a bounded C++ analyzer over stored run directories with
  `fixture/reference.wav` and `captured.wav`.
- Same mandatory safety warning: no mutation in `/Users/fer/dev/opena8dj` or
  `/Users/fer/dev/audio8djrust`; no hardware/audio/CoreAudio/USB.

Files affected by subagent:
- `CMakeLists.txt`
- `tools/capture_matrix_quality_analysis.cpp`

Integration:
- Reviewed the patch, ran focused compile/selftest/CTest, wired it into the
  full offline gate and evidence schema, and added documentation.
- The analyzer selftest PASSes and the full gate includes it.

Risk:
- The tool duplicates some WAV parsing and alignment logic from
  `tools/loopback_quality_analysis.cpp`; a later cleanup should extract a
  shared offline WAV/quality helper after behavior settles.
- Current real-capture analysis can pass routing/leakage while failing global
  SNR/correlation. That is expected and must not be over-interpreted.

## 2026-06-17 Analysis Subagent: Product Evidence Priorities

- Agent: Aquinas (`019ed681-f474-7192-a237-5af635ca67c2`).
- Prompt summary:
  - Read-only audit of `/Users/fer/dev/audio8djcpp` to identify the most
    valuable measurement improvements for audiophile readiness.
  - Mandatory warning included: do not touch `/Users/fer/dev/opena8dj` or
    `/Users/fer/dev/audio8djrust`, only write in `/Users/fer/dev/audio8djcpp`,
    and do not touch hardware/audio/CoreAudio/USB without the global lock and
    authorized window.
- Findings:
  - Highest-value work is not more Mode2/packer work; the bottleneck is
    real-music quality, timebase/lag behavior, leakage matrix evidence, CPU,
    and physical timecode validation.
  - Recommended first priority: reinforce or migrate real-music quality and
    failure-mode analysis into C++ with objective parity to existing Python
    metrics.
  - Recommended guardrail: reject CPU-only wins because previous ISO-family
    runs can reduce CPU while failing quality and lag.
- Integrated action:
  - Added native product-superiority reporting to
    `tools/physical_run_compare.cpp`.
  - Wired the comparator into CMake, CTest, `scripts/run-cpp-offline-gates`,
    static policy, and evidence schema.
- Files affected by integration:
  - `tools/physical_run_compare.cpp`
  - `CMakeLists.txt`
  - `scripts/run-cpp-offline-gates`
  - `tools/evidence_schema_check.cpp`
  - `tools/static_policy_check.cpp`
- Risk:
  - The comparator currently reads existing analyzer metrics rather than
    reimplementing WAV analysis itself. Next step is deeper C++ parity for
    `analyze-soundcheck-capture.py` metrics.
- Next action:
  - Implement or reinforce native real-music/WAV metric parity for alignment,
    SNR, residual bands, quiet noise, click outliers, clipping, and lag jumps.

## 2026-06-17 Native WAV Reanalysis Integration

- Agent: Architect.
- Files affected:
  - `tools/soundcheck_wav_quality.cpp`
  - `CMakeLists.txt`
  - `scripts/run-cpp-offline-gates`
  - `tools/evidence_schema_check.cpp`
  - `tools/static_policy_check.cpp`
  - docs listed in this handoff.
- Result:
  - Added a first-slice native C++ analyzer over stored soundcheck WAV pairs.
  - Fixed the alignment search to use Python-style coarse/fine lag search after
    an initial false periodic maximum.
  - Current analyzer parity is broad but passing for core alignment/SNR/residual
    metrics.
- Risk:
  - It is not a full replacement for Python yet. Time-warp, CPU coupling,
    exact quiet-window behavior, and stricter click parity remain.
- Next action:
  - Tighten parity and port remaining `analyze-soundcheck-capture.py` logic
    into a reusable C++ audio-quality module.

## 2026-06-17 Subagent Integration: Residual And CPU Failure Analysis

- Agents:
  - Jason (`019ed694-1165-74f3-884b-2b4b432919df`), Sound Quality Failure
    Analyst.
  - Beauvoir (`019ed694-369c-78f2-8ee9-3fc4a64182b8`),
    Real-Time/CPU Evidence Analyst.
- Mandatory warning included for both:
  - PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj` o
    `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo puedes
    escribir en `/Users/fer/dev/audio8djcpp`. No tocar
    hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.
- Jason findings:
  - The stored ISO12/q8 failure is not explained by clipping, click outliers,
    L/R swap, polarity, simple L/R matrix, or simple nonlinear distortion.
  - Timing instability exists, but available offline analysis shows it does not
    explain the dominant residual.
  - Recommended native `residual_attribution` with explicit timing, routing,
    residual/capture-path, and fixture-correlation metrics.
- Beauvoir findings:
  - `opena8dj_driver` CPU p95 around `16.6%` is sustained; after 5s it remains
    about `16.6%`.
  - `coreaudiod` p95 around `35.4%` is dominated by an early transient; after
    5s it drops near `2.1%`.
  - Existing evidence is process-level CPU, not direct callback attribution,
    so future work needs hot-path timing or equivalent callback breakdown.
- Integrated action:
  - Added `residual_attribution` to
    `tools/soundcheck_wav_quality.cpp`.
  - Updated `tools/physical_run_compare.cpp` to use matching native WAV
    reanalysis and report post-5s CPU.
  - Added attribution and stable CPU fields to
    `scripts/run-cpp-offline-gates`.
- Result:
  - Full offline gates PASS.
  - Product comparator remains FAIL and branch promotion remains unsupported.
- Next action:
  - Reduce sustained driver CPU and obtain decorrelated physical evidence before
    any further readiness or branch-promotion claim.

## 2026-06-17 Subagent Integration: Sustained CPU Root-Cause Scout

- Agent: Ptolemy (`019ed69c-cd01-7e30-bd77-a29a950dd7fe`).
- Mandatory warning included:
  - PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en `/Users/fer/dev/opena8dj` o
    `/Users/fer/dev/audio8djrust`. Esos worktrees son READ ONLY. Solo puedes
    escribir en `/Users/fer/dev/audio8djcpp`. No tocar
    hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.
- Findings:
  - ISO12/q8 sustains about `666.8` capture and playback transfers per second.
  - OpenA8DJ driver CPU remains around `16.6%` after startup, while
    `coreaudiod` stabilizes much lower.
  - Existing ISO12/q8 evidence lacks hot-path timing samples, so the CPU
    failure is process-level, not callback-attributed.
  - Most likely suspects are fixed per-transfer queue/requeue/enqueue costs,
    not audio math.
  - Previously tried reuse/fast-transfer flags should not be promoted blindly;
    they need locked revalidation with quality gates.
- Integrated action:
  - `tools/physical_run_compare.cpp` now emits capture/playback transfer rates
    and callback attribution status.
  - `scripts/run-cpp-offline-gates` includes those fields in the summary.
- Next action:
  - Create a locked hot-path-timing physical window only after deciding the
    exact diagnostic build. Until then, no CPU root-cause claim beyond
    process-level evidence.
## 2026-06-17 Reused Subagent: Russell Analog Residual Scout

- Agent:
  - Russell (`019ed60d-b361-7853-a909-cd131a750d0a`).
- Mission:
  - Read stored C++ evidence and rank analog residual/root-cause hypotheses.
  - No edits, no hardware, no CoreAudio/USB/system changes.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.`
- Findings integrated:
  - Packed USB payload can be clean while analog capture quality fails.
  - Shared route/capture evidence is degraded for both mainline and C++ in one
    stored comparison.
  - Lag exists but does not explain enough residual by itself.
  - Cadence/control remains plausible while simple L/R matrix, polarity, gain,
    or fixed EQ are weak explanations.
- Files affected by integration:
  - `tools/quality_root_cause_analysis.cpp`.
  - `docs/ARCHITECT_CONTEXT.md`.
  - `docs/DECISION_LOG.md`.
  - `docs/SUCCESS_METRICS.md`.
  - `docs/TEST_PLAN.md`.
  - `docs/TEST_EVIDENCE.md`.
- Next action:
  - Keep promotion blocked until route health and same-session mainline/C++
    physical comparison pass under lock.

## 2026-06-17 Subagent: Euclid Performance Direction Scout

- Agent:
  - Euclid (`019ed6b0-47d3-7050-a22d-b80939959203`).
- Mission:
  - Read stored evidence and identify the safest next performance direction
    that does not repeat physically rejected HAL flags.
  - No edits, no hardware, no CoreAudio/USB/system changes.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.`
- Findings:
  - Do not repeat isolated HAL flag experiments as product candidates:
    ISO64/coalesce, native output format, no audio-param reset, sample-time
    follower, ignore sample time, fast ISO config, reused/raw completions,
    stats-off, and input-decode bypass all have negative physical or CPU
    evidence.
  - Packed USB payload cleanliness is not the current product blocker.
  - Best next performance direction is prepared transport/backend-owned
    requeue: reduce steady-state IOUSBHost/Objective-C queue/requeue/enqueue
    cost while preserving bytes, cadence, routing, sample rate, and timecode.
  - Required offline gates before hardware:
    prepared transport PASS, static policy PASS, no rejected defaults,
    `hal_steady_requeues=0`, fallback allocations `0`, completion gap within
    bounded threshold, timestamp regressions `0`, routing/timecode safe.
- Files affected by integration:
  - `docs/AGENT_HANDOFFS.md`.
- Next action:
  - Move toward prepared transport implementation evidence, not more HAL
    micro-flag probes.

## 2026-06-17 Subagent: Goodall Prepared Migration Gate Reviewer

- Agent:
  - Goodall (`019ed6b6-9e3f-7dc0-af72-61056b76e967`).
- Mission:
  - Review what a useful `prepared_transport_migration_gate` must require.
  - No edits, no hardware, no CoreAudio/USB/system changes.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.`
- Findings integrated:
  - The migration gate must remain an offline migration gate, not readiness.
  - It must require zero HAL steady-state requeues, stable backend lead, no
    fallback allocation, strict cadence, monotonic timestamps, clean recovery,
    routing A/B/C/D, preserved offline timecode, and explicit physical-readiness
    blocking.
  - It must not allow synthetic timecode PASS to imply Traktor/timecode-vinyl
    readiness.
- Files affected by integration:
  - `tools/prepared_transport_migration_gate.cpp`.
  - `core/include/opena8djcpp/prepared_transport.hpp`.
  - `core/src/prepared_transport.cpp`.
  - `tools/prepared_slot_scheduler_contract.cpp`.
  - `scripts/run-cpp-offline-gates`.
  - `docs/ARCHITECT_CONTEXT.md`.
  - `docs/DECISION_LOG.md`.
  - `docs/TEST_EVIDENCE.md`.
- Next action:
  - Bind prepared transport into the runtime candidate path, then run a
    lock-gated same-session A/B hardware window only after offline gates pass.

## 2026-06-17 Reused Subagent: Ramanujan Route Harness Scout

- Agent:
  - Ramanujan (`019ed6f3-1e16-7343-a88f-b7c993f26ae3`).
- Mission:
  - Inspect existing local soundcheck tooling read-only and identify the
    smallest safe change to test a known-good non-Audio8 source through the
    iRig capture route.
  - No hardware, no CoreAudio/USB reset, no driver install/load, no default
    device changes.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.`
- Findings:
  - Existing `scripts/run-soundcheck` is lock gated, but playback was fixed to
    the OpenA8DJ/CoreAudio Audio 8 output path.
  - `src/tools/audio-wav-play.c` needed explicit device name/UID selection to
    drive a controlled non-Audio8 route test.
  - Explicit-device playback should refuse hidden default-device changes and
    sample-rate changes.
- Integrated action:
  - Added explicit `--device` / `--device-uid` playback support.
  - Added `scripts/run-known-good-route-soundcheck`.
  - Added hardware-lock policy coverage for the wrapper.
- Files affected:
  - `src/tools/audio-wav-play.c`.
  - `scripts/run-known-good-route-soundcheck`.
  - `tools/hardware_lock_policy_check.cpp`.
  - `docs/ARCHITECT_CONTEXT.md`.
  - `docs/DECISION_LOG.md`.
  - `docs/TEST_PLAN.md`.
  - `docs/TEST_EVIDENCE.md`.
  - `docs/BUILD.md`.
- Next action:
  - Use the wrapper only in an authorized physical window with a real
    non-Audio8 output routed into the same iRig capture chain.

## 2026-06-17 Subagent: Hume Readiness Gate Reviewer

- Agent:
  - Hume (`019ed707-c4c3-7ef3-9b94-cc15b9156e1a`).
- Mission:
  - Read-only review of the same-session mainline/C++ physical comparison
    contract.
  - Search for readiness loopholes around `--candidate-only`,
    `--skip-known-good`, exit codes, and documentation.
  - No hardware, CoreAudio, USB, driver install, or service changes.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Initial integration status:
  - Findings integrated.
- Findings:
  - `opena8djcpp_physical_run_compare` could report branch promotion support
    from fixed historical thresholds instead of an explicit same-session
    mainline run.
  - `scripts/evaluate-promotion-readiness.py` did not consume
    `same-session-physical-compare.json`.
  - `--skip-known-good` was labeled diagnostic but was not exit-blocking.
  - Hardware-lock policy audited strings but not these readiness invariants.
- Integrated action:
  - `opena8djcpp_physical_run_compare` now supports branch promotion only with
    an explicit baseline run.
  - Promotion readiness now requires a same-window same-session comparison.
  - `--skip-known-good` now writes blocked evidence and prevents success.
  - Hardware-lock policy now checks blocked candidate-only/skipped-route
    markers.
- Files affected:
  - `tools/physical_run_compare.cpp`.
  - `scripts/evaluate-promotion-readiness.py`.
  - `scripts/run-physical-superiority-window`.
  - `tools/hardware_lock_policy_check.cpp`.
  - `docs/ARCHITECT_CONTEXT.md`.
  - `docs/DECISION_LOG.md`.
  - `docs/TEST_PLAN.md`.
  - `docs/BUILD.md`.
  - `docs/PHYSICAL_TEST_WINDOW_PLAN.md`.
- Next action:
  - Re-run offline gates and keep promotion blocked until real physical
    same-session evidence exists.

## 2026-06-17 Subagent: Lorentz Signal Forensics

- Agent:
  - Lorentz (`019ed730-9337-7c02-a98b-fc1cd9e3fbe0`).
- Mission:
  - Read-only forensics over existing physical/direct-USB evidence.
  - Determine whether the current audiophile-quality blocker is packet
    packing, direct USB payload, HAL timing, CPU, or external route/capture.
  - No hardware, audio, USB, CoreAudio, driver install, service change, or
    system mutation.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorizacion de ventana.`
- Findings:
  - The current dominant blocker is the external physical route/capture, not
    Mode-2 packet packing.
  - Direct USB internal artifacts are clean in the latest diagnostic run:
    written, consumed, and packed USB alignment `1.000000`, USB check errors
    `0`, USB panic flags `0`, and USB SNR floor `999 dB`.
  - The same direct USB run still fails external iRig capture with
    `quality_alignment_score=0.738457`, `snr_db_min=-0.637949`, and
    mid/high residual ratios `1.681576/1.664308`.
  - HAL C++ remains secondary until route validation is clean; current HAL
    physical evidence shows low SNR, lag jumps, and high CPU.
- Integrated action:
  - Expanded `opena8djcpp_physical_capture_forensics` to include archived
    direct USB and physical-superiority-window captures, not only HAL
    soundcheck captures.
  - Added direct USB and physical-window forensic summaries to
    `scripts/run-cpp-offline-gates`.
- Files affected:
  - `tools/physical_capture_forensics.cpp`.
  - `scripts/run-cpp-offline-gates`.
  - `docs/AGENT_HANDOFFS.md`.
  - `docs/ARCHITECT_CONTEXT.md`.
  - `docs/DECISION_LOG.md`.
  - `docs/TEST_EVIDENCE.md`.
- Next action:
  - Re-run offline gates, then use a lock-gated physical route revalidation
    only when a real non-Audio8 output is physically routed into the same
    mixer/REC OUT -> iRig chain. Do not claim C++ superiority before that
    route gate passes.

## 2026-06-17 Subagent: Laplace QA/Metrics Reviewer

- Agent:
  - Laplace (`019ed747-8792-77c1-8b5d-068f4bd39b6d`).
- Mission:
  - Read-only review of the new capture-route/Direct-USB gate semantics.
  - Verify whether the gate prevents false audiophile, performance, timecode,
    and branch-promotion claims when the physical capture route is invalid.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - `run-cpp-offline-gates` generated `capture-route-health-gate.json` before
    regenerating the Direct USB attribution, soundcheck WAV quality, and
    physical product comparison evidence that the gate consumes.
  - `evaluate-promotion-readiness.py` did not consume
    `capture-route-health-gate.json` or `direct-usb-path-attribution.json` as
    first-class promotion blockers.
  - The top-level offline summary exposed analyzer `PASS` but lacked an equally
    visible product-readiness status.
  - `capture_route_health_gate` parsed JSON keys globally instead of anchoring
    Direct USB metrics to `latest_run`.
- Integrated action:
  - Reordered offline evidence generation so Direct USB attribution and
    consumed physical-quality artifacts are current before route-health
    evaluation.
  - Added explicit promotion gates for capture-route validity and
    Direct-USB-failed-after-clean-payload.
  - Added top-level offline summary fields for diagnostic status, product
    readiness, promotion permission, physical measurement validity, and route
    blockers.
  - Scoped Direct USB parsing to the `latest_run` object.
- Files affected:
  - `scripts/run-cpp-offline-gates`.
  - `scripts/evaluate-promotion-readiness.py`.
  - `tools/capture_route_health_gate.cpp`.
  - `tools/evidence_schema_check.cpp`.
  - `docs/AGENT_HANDOFFS.md`.
  - `docs/ARCHITECT_CONTEXT.md`.
  - `docs/DECISION_LOG.md`.
  - `docs/TEST_EVIDENCE.md`.
  - `docs/TEST_PLAN.md`.
  - `docs/SUCCESS_METRICS.md`.
- Next action:
  - Re-run full offline gates and require
    `product_readiness_status=FAIL/NOT_READY` plus
    `branch_promotion_allowed=false` until a validated physical route and
    same-session mainline/C++ comparison exist.

## 2026-06-17 Subagent: Tesla iRig/Lock Recovery Analyst

- Agent:
  - Tesla (`019ed752-8782-7340-a711-954932a4026f`).
- Mission:
  - Read-only inspection of hardware lock, obvious stale test processes, USB
    visibility, and CoreAudio visibility for iRig Stream and Audio 8 DJ.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - Global hardware lock directory is absent; no owner PID exists.
  - No obvious persistent test process was found for `audio-record`,
    `soundcheck`, `ffmpeg`, `sox`, `afplay`, `vlc`, `traktor`, or OpenA8DJ
    tooling beyond normal `coreaudiod`.
  - USB/IORegistry sees:
    - `iRig Stream`, IK Multimedia, serial `152349`;
    - `Audio 8 DJ`, Native Instruments, `idVendor=6092`,
      `idProduct=6520`, serial `SN-HKM6Q6EDKP`.
  - CoreAudio sees `iRig Stream` as 2-in/2-out at 48 kHz.
  - CoreAudio does not currently expose `Audio 8 DJ`, `Open Audio 8 DJ`, or
    `OpenA8DJ`.
  - `system_profiler SPUSBDataType` returned empty USB output while `ioreg`
    saw both devices; use `ioreg` as the current USB visibility source.
- Risk:
  - Audio 8 DJ USB presence does not mean the HAL/CoreAudio path is alive.
  - iRig is available for capture, but a promotion-quality route still needs a
    known-good non-Audio8 route validation under lock.
- Next action:
  - At the start of any physical window, acquire the global lock and record a
    passive snapshot of lock, process, USB, and CoreAudio state before any
    recovery. Focus recovery on Audio 8 DJ HAL/CoreAudio registration rather
    than blind USB resets.

## 2026-06-17 Subagent: Carver Audio8/iRig Current-State Inspector

- Agent:
  - Carver (`019ed759-04e4-7690-a7af-9f0d8a024d99`).
- Mission:
  - Read-only current-state inspection of the global lock, USB visibility,
    CoreAudio devices, and relevant test/audio processes while the architect
    worked on HAL safety evidence.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - Lock was absent at the final read.
  - During the architect's HAL safety run, the lock briefly existed with owner
    PID `39562` running `scripts/test-hal-candidate-safety`; it disappeared
    after the run completed.
  - USB/IORegistry saw:
    - `iRig Stream`, IK Multimedia, vendor/product `0x1963/0x0059`, serial
      `152349`;
    - `Audio 8 DJ`, Native Instruments, vendor/product `0x17cc/0x1978`,
      serial `SN-HKM6Q6EDKP`.
  - Final CoreAudio state saw iRig Stream plus built-in devices, but not
    `Open Audio 8 DJ`, because the safety harness unloaded the HAL candidate.
  - During the intermediate window, `Core Audio Driver (OpenA8DJ.driver)` was
    visible; it was gone by the final read.
  - No lingering `audio-record`, `audio-wav-play`, `soundcheck`, `ffmpeg`,
    `sox`, VLC, Traktor, or OpenA8DJ process remained at final read.
- Integrated action:
  - Added `opena8djcpp_hal_candidate_safety_gate` to preserve the HAL safety
    run as machine-readable evidence.
- Next action:
  - Use the HAL safety result as a precondition only. The next physical step is
    still route validation and same-session mainline/C++ A/B under lock.

## 2026-06-17 Subagent: Meitner Hardware Route Sentinel

- Agent:
  - Meitner (`019ed761-dc08-76f2-9724-6cd29676c5a7`).
- Mission:
  - Read-only verification of lock state, iRig visibility, Audio 8 DJ
    visibility, HAL loaded state, and obvious lingering audio/test processes.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - Lock absent.
  - iRig Stream visible over USB/CoreAudio as a 48 kHz 2x2 device.
  - Audio 8 DJ visible over USB.
  - OpenA8DJ HAL was not loaded at final read.
  - No suspicious lingering `ffmpeg`, `sox`, Traktor, Native Instruments,
    OpenA8DJ, or soundcheck processes were found.
- Files:
  - `local-analysis/route-sentinel/20260617T210043Z-route-sentinel.md`.
- Integrated action:
  - Proceeded with locked physical windows using iRig as capture.
  - Later physical evidence showed the route/device was visible and stable
    enough for capture attempts, but not promotion-quality.
- Next action:
  - Keep using route sentinel checks before physical windows, but do not treat
    device visibility as sound-quality readiness.

## 2026-06-17 Subagent: Gauss C++ Transport Pacing Analyst

- Agent:
  - Gauss (`019ed787-212e-7943-b2e3-cfc85591ff95`).
- Mission:
  - Read-only analysis of the C++ HAL data plane to explain high submission
    count and propose a safer pacing direction.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - C++ default ISO8/capture-paced mode naturally produces about one playback
    submission per capture completion, roughly 1000/s.
  - Simple coalescing is insufficient because it changes timing/layout without
    preserving the hardware assumptions.
  - Recommended direction is a deliberate playback scheduler with target lead
    in microframes while capture remains active for input/timecode.
- Integrated action:
  - Ran a lock-gated playback-completion-paced probe with coalesce=2/q4.
  - Probe reduced submissions/CPU but failed physical quality, so the simple
    switch was rejected.

## 2026-06-17 Subagent: Hooke Mainline Transport Comparator

- Agent:
  - Hooke (`019ed787-73b1-7330-90fe-fb95f54032c0`).
- Mission:
  - Read-only comparison of mainline and C++ transport behavior.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - Mainline and C++ share the capture-paced concept.
  - Mainline's lower CPU is best explained by `HAL_ISO_FRAMES=64` in its
    Makefile, versus C++ default ISO8.
  - Explicit scheduling and reused completion handlers are off in both defaults
    and do not explain the observed CPU gap.
  - Stats/logging are not the primary CPU gap.
- Integrated action:
  - CPU work is now framed as reducing IOUSBHost submissions while preserving
    the timing quality that naive ISO64/coalescing probes fail to preserve.

## 2026-06-17 Subagent: Linnaeus Prepared Transport Mapping

- Agent:
  - Linnaeus (`019ed78e-7db8-75a1-b5a4-c6e83b6eff5f`).
- Mission:
  - Read-only review of prepared transport, prepared slot scheduler, and HAL
    integration points.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - `PreparedTransportBackend` already models the desired split: HAL publishes
    and consumes rings while backend completion owns transport period work.
  - `PreparedSlotScheduler` and prepared hot-path contracts reject HAL direct
    steady requeue, fallback allocations, coalesced completion gaps, starvation,
    and unsafe publication rates.
  - HAL integration points are `workerLoop`, `queueCaptureTransfer`,
    `handleCaptureTransfer`, `queueCapturePacedPlaybackWithRequests`,
    `queuePlaybackWithRequests`, and the `writeOutput`/`fillPlaybackBytes`
    payload boundary.
- Integrated action:
  - Combined Linnaeus' mapping with a new privileged process sample from the
    default C++ HAL. The sample confirms the CPU bottleneck is IOUSBHost async
    enqueue cadence, aligning with the prepared transport direction.
- Risk:
  - Prepared transport contracts are still offline. They are not physical
    readiness and cannot justify promotion until bound to the runtime and
    measured against mainline.

## 2026-06-17 Subagent: Pascal Prepared Transport Runtime Reality Check

- Agent:
  - Pascal (`019ed798-3e9c-7f41-8f84-a94f82051179`).
- Mission:
  - Read-only review of prepared transport contracts and the smallest path to a
    runtime implementation that is measurable instead of decorative accounting.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - Existing prepared contracts enforce zero fallback allocations, zero HAL
    steady requeues, bounded completion gap, clean routing/timecode, and batch
    ring publication reduction.
  - A bridge that only calls `PreparedTransportBackend` while the HAL still
    performs the same `enqueueIORequestWithData` cadence would be simulation,
    not a CPU fix.
  - The first useful runtime change must visibly change real queue pressure or
    fail when real fallback/queue pressure remains unchanged.
- Integrated action:
  - Added an opt-in HAL experiment,
    `HAL_CAPTURE_PACED_PLAYBACK_REFILL=1`, that keeps capture/timecode active
    but refills playback using the independent playback queue path. This can
    reduce playback enqueue cadence when combined with coalesced playback
    transfers while leaving the default path unchanged.
- Risk:
  - This is still a HAL experiment, not DriverKit slot ownership. It must pass
    HAL candidate safety and same-session physical A/B before it can influence
    readiness.

## 2026-06-17 Subagent: Bacon Prepared USB Slot Scheduler Review

- Agent:
  - Bacon (`019ed7a8-db6f-7a83-834c-678a16ef2e36`).
- Mission:
  - Read-only review of the next viable low-CPU data-plane direction after
    independent playback refill/coalescing failed physical sound quality.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - The HAL still performs the expensive work around direct
    `enqueueIORequestWithData` calls from capture and playback queue paths.
  - Existing offline prepared transport models already express the safer
    separation: HAL/CoreAudio reads and writes bounded rings; backend owns
    prepared slots and requeue.
  - The next useful implementation should not be another HAL coalescing probe.
    It should preserve logical ISO8 audio slots while batching real USB submit
    work below that logical cadence.
- Integrated action:
  - The next architecture target is now a real prepared USB slot scheduler with
    separate counters for logical audio periods, USB submit calls, backend slot
    completions, slot order errors, timestamp regressions, and HAL steady
    requeues.
- Risk:
  - This superficially resembles ISO64, which failed physically. The quality
    requirement is that batching must not change logical slot cadence,
    timestamps, routing, lead, capture/timecode handling, or playback refill
    timing.

## 2026-06-17 Subagent: Plato Runtime Adapter Engineer

- Agent:
  - Plato (`019ed7b6-c445-7a31-82ac-c26580d0b696`).
- Mission:
  - Implement, if feasible, a pure offline fake runtime adapter that exposes
    the logical ISO8 to USB submit batching contract through runtime-facing
    counters.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Files affected:
  - `core/include/opena8djcpp/runtime_adapter.hpp`.
  - `core/src/runtime_adapter.cpp`.
  - `tools/runtime_adapter_contract.cpp`.
  - `CMakeLists.txt`.
  - `tools/static_policy_check.cpp`.
  - `docs/TEST_EVIDENCE.md`.
- Findings and result:
  - Added `FakeRuntimeAdapter` on top of `PreparedSlotScheduler`.
  - `opena8djcpp_runtime_adapter_contract` PASS exposes
    `logical_audio_periods=256`, `backend_slot_completions=512`,
    `usb_submit_calls=66`, and `usb_submit_reduction_ratio=8`.
  - Negative rows reject unbatched submits, logical gaps, slot-order errors,
    HAL requeues, and fallback allocations.
- Integrated action by architect:
  - Added the runtime adapter evidence to `scripts/run-cpp-offline-gates`.
  - Required `runtime-adapter-contract.json` in `evidence_schema_check`.
  - Made `prepared-transport-migration-gate` require
    `runtime_adapter_batched_submit_counters_exposed=PASS`.
- Risk:
  - This is still a fake runtime boundary. The next implementation must bind
    equivalent counters to real DriverKit/USB work before any lock-gated
    hardware candidate can claim CPU direction.

## 2026-06-17 Subagent: Pasteur USB Shutdown Auditor

- Agent:
  - Pasteur (`019ed7de-a957-7f62-84f6-d130065e7f22`).
- Mission:
  - Read-only audit of the next offline blocker after commit `f863300`.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - Descriptor, payload, binding, and request pool contracts existed, but
    request lifecycle still lived partly as a standalone model.
  - The next offline blocker was shutdown/cancel/restart ownership: stop with
    live requests, explicit cancellation accounting, late completions rejected,
    and restart after cancel.
- Integrated action:
  - Added skeleton-owned request pool behavior and
    `opena8djcpp_driverkit_usb_request_shutdown_contract`.
  - The migration gate now requires `driverkit_usb_request_shutdown_safe=PASS`.
- Risk:
  - This remains offline DriverKit-shell modeling. It does not prove real
    USBDriverKit submits, dext installation, physical audio quality, CPU
    superiority, or Traktor/timecode vinyl readiness.

## 2026-06-17 Subagent: Galileo Physical Window Gate Auditor

- Agent:
  - Galileo (`019ed7ee-c72d-7ac1-b39a-f48d7c8a4d2c`).
- Mission:
  - Read-only audit of the new physical-window readiness gate and integration.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - No direct authorization path was found for product physical A/B or branch
    promotion.
  - High-risk integration issue: the first version could execute before
    regenerating some source evidence, allowing a stale standalone PASS.
  - Additional risk: schema did not explicitly require branch-promotion false,
    restrictive window types, and the blocked-claim string.
- Integrated action:
  - `opena8djcpp_physical_window_readiness_gate` now reads source evidence
    directly instead of relying on `current-offline-gates.json` for critical
    decisions.
  - `scripts/run-cpp-offline-gates` now regenerates promotion, migration, and
    hardware-lock evidence before emitting
    `physical-window-readiness-gate.json`.
  - `opena8djcpp_evidence_schema_check` now requires
    `ready_for_branch_promotion=false`, `ROUTE_REVALIDATION_ONLY`,
    `NO_PROMOTION_AB_UNTIL_ROUTE_PASS`, and the explicit blocked claim.
- Risk:
  - The gate still uses text checks over JSON artifacts. That is acceptable as
    a narrow offline guard for this slice, but a future hardening pass should
    move critical evidence gates to a small structured JSON reader.
## 2026-06-17 Kant - Evidence Integrity Review

- Subagent:
  - `Kant`
  - Agent id: `019ed803-8499-7193-8721-f35cbca91912`
- Mission:
  - Read-only review of the C++/DriverKit line for the next objective blocker
    in gates, metrics, and evidence integrity.
  - Explicitly forbidden from touching mainline, Rust, hardware, audio,
    CoreAudio, or USB.
- Findings:
  - Highest priority risk: `scripts/evaluate-promotion-readiness.py` selected
    latest evidence per type without requiring one shared run id, candidate,
    baseline, or physical route. Only music and CPU were paired by folder.
  - Additional risks: several diagnostic tools print PASS while product
    readiness remains false; timecode is offline-only; the audiophile tone gate
    is too narrow to prove real-music quality by itself.
- Action taken:
  - Added `single_physical_promotion_evidence_bundle` to the promotion
    evaluator.
  - Added `single_physical_promotion_evidence_bundle_missing` to offline hard
    blockers.
  - Full offline gates passed after integration: Debug CTest `52/52`, Release
    CTest `53/53`, evidence schema `required_files=53`, `missing_files=0`.
  - Added `opena8djcpp_diagnostic_pass_semantics_gate` so diagnostic PASS
    artifacts must explicitly say why they are not product readiness or branch
    promotion evidence.
  - Full offline gates passed after diagnostic semantics integration: Debug
    CTest `53/53`, Release CTest `54/54`, evidence schema
    `required_files=54`, `missing_files=0`.
- Files affected by integration:
  - `scripts/evaluate-promotion-readiness.py`
  - `scripts/run-cpp-offline-gates`
  - `tools/evidence_schema_check.cpp`
  - `tools/diagnostic_pass_semantics_gate.cpp`
  - `CMakeLists.txt`
  - `docs/TEST_EVIDENCE.md`
  - `docs/DECISION_LOG.md`
  - `docs/ARCHITECT_CONTEXT.md`
  - `docs/SUCCESS_METRICS.md`
  - `docs/AGENT_HANDOFFS.md`
- Remaining risk:
  - Product readiness is still blocked until the route is valid and one
    same-session physical bundle proves mainline-vs-C++ quality, CPU,
    routing, and Traktor/timecode-vinyl behavior.

## 2026-06-17 Product Quality Claim Guard

- Subagent input:
  - Kant's review noted that the audiophile tone gate was too narrow to prove
    real-music quality by itself.
- Integrated action:
  - Added `opena8djcpp_product_quality_claim_gate`.
  - The gate blocks quality claims unless real-music same-session superiority,
    route-valid tone, route validity, and promotion allowance all pass.
- Evidence:
  - Full offline gates: Debug CTest `54/54`, Release CTest `55/55`, evidence
    schema `required_files=55`, `missing_files=0`.
  - Current gate state: `quality_claim_allowed=false`.
- Risk:
  - This is still an offline evidence guard, not proof of sound quality. It
    prevents false claims until a valid lock-gated physical bundle exists.

## 2026-06-17 Subagent: Boyle Evidence Provenance Audit

- Agent:
  - Boyle (`019ed80d-16f5-73e0-a042-9be1a78ca39a`)
- Mission:
  - Read-only audit of the next objective blocker that could allow a false
    quality/performance/readiness claim.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Finding:
  - `current-offline-gates.json` could report PASS for an older `base_commit`
    than HEAD, so stale evidence could be attributed to the current candidate.
- Integrated action:
  - Added `opena8djcpp_evidence_provenance_freshness_gate`.
  - Added `candidate_evidence_matches_claimed_commit` to promotion evaluation.
  - The full offline runner now records worktree dirtiness and embeds
    provenance status into `current-offline-gates.json`.
- Risk:
  - Historical physical artifacts still may not carry per-artifact commit IDs.
    The next physical promotion bundle should stamp candidate commit/build ID
    into every generated artifact.

## 2026-06-17 Subagent: Cicero Physical Promotion Window Audit

- Agent:
  - Cicero (`019ed819-733b-7230-bc2e-177a1a0eae1e`)
- Mission:
  - Read-only audit of `scripts/run-physical-superiority-window`,
    `scripts/physical-window-preflight`,
    `scripts/run-known-good-route-soundcheck`, and
    `scripts/evaluate-promotion-readiness.py`.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - The promotion evaluator did not require `known-good-route` evidence in the
    same physical window as music/CPU/tone/latency/matrix/same-session
    artifacts.
  - A built-in/acoustic diagnostic known-good route could be too easy to
    confuse with promotion evidence if not explicitly checked.
  - Preflight rejected Audio 8 by argument text but not by the resolved
    CoreAudio device identity.
- Integrated action:
  - Added `same_window_known_good_route_revalidated` and
    `physical_window_not_diagnostic` gates to promotion evaluation.
  - Added `known_good_route` to the required physical promotion bundle.
  - Added `scripts/test-promotion-window-contract.py`, an offline synthetic
    regression test for missing route evidence and diagnostic-window rejection.
  - Hardened `physical-window-preflight` to reject resolved known-good devices
    that are actually OpenA8DJ/Audio 8.
  - Follow-up integration added `scripts/validate-known-good-route-request.py`
    and wired it into `scripts/run-known-good-route-soundcheck` after hardware
    lock acquisition, so direct standalone route checks also reject resolved
    Audio 8 outputs.
- Risk:
  - This closes a false-positive promotion path. It does not provide the
    missing physical evidence; the next real step remains a locked known-good
    wired non-Audio8 route revalidation followed by same-session mainline/C++
    physical A/B.

## 2026-06-17/18 Subagent: Raman Physical Window Auditor

- Agent:
  - Raman (`019ed827-dc26-7693-a8a3-60ddb566472a`)
- Mission:
  - Read-only audit of the safest next physical command for closing
    `same_window_known_good_route_revalidation_missing`.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Finding:
  - The promotable route-only command is
    `scripts/run-physical-superiority-window --execute --route-only`, but only
    with an explicit wired non-Audio8 output that is not `iRig Stream` and not
    built-in speakers.
  - Expected route:
    `<wired non-Audio8 CoreAudio output> -> mixer/REC OUT route -> iRig Stream`.
  - Abort if the lock is busy, iRig is not visible with 2 inputs, the known-good
    output is missing, resolves to Audio 8, is built-in/acoustic, is the same
    device as capture, or capture appears virtual.
- Integrated action:
  - Current CoreAudio enumeration exposed no promotable known-good output:
    `iRig Stream`, `MacBook Air Speakers`, and, while loaded, `Open Audio 8 DJ`
    only.
  - Because route revalidation could not be promotion-valid, the architect ran
    diagnostic C++ HAL soundchecks instead, documented the negative quality/CPU
    evidence, and unloaded the failed candidate.
- Risk:
  - A real non-Audio8 wired output is still required before a promotion-valid
    known-good route or same-session mainline/C++ A/B can be run.

## 2026-06-18 Subagent: Halley Evidence Gap Auditor

- Agent:
  - Halley (`019ed858-5396-7f30-a769-346cd65321cf`)
- Mission:
  - Read-only audit of remaining objective gaps before claiming audiophile
    quality, functionality, timecode vinyl readiness, or performance
    superiority over mainline.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - Offline gates are broad, but product readiness still fails correctly.
  - Current route evidence is not valid for promotion because the shared capture
    path is unhealthy, direct USB payload evidence is internally clean while
    physical capture fails after the payload, and no current wired non-Audio8
    known-good source is visible.
  - Same-session mainline/C++ A/B, sustained CPU superiority, and physical
    Traktor/timecode vinyl evidence remain missing.
  - The ISO64 capture HAL safety load proves install/enumeration/unload safety
    only; it does not prove quality, routing, timecode, or CPU superiority.
- Integrated action:
  - Added HAL runtime geometry observability so the next physical evidence can
    attribute active ISO and queue settings to the exact candidate under test.
- Next action:
  - Run only a lock-gated known-good wired non-Audio8 route revalidation first.
    Do not run product A/B or promotion gates until that route passes.

## 2026-06-18 Subagent: Nietzsche Runtime Submit Audit

- Agent:
  - Nietzsche (`019ed85f-785e-7961-b611-514d0b5c562c`)
- Mission:
  - Read-only audit of the lowest-risk next step for objective runtime
    performance evidence against mainline.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - Capture and playback both still use direct `IOUSBHost` enqueue calls in the
    HAL hot path.
  - Playback already had a submitted-transfer counter; capture lacked the
    symmetric counter needed to compare submit cadence against mainline.
  - Changing runtime USB cadence before stronger measurement is higher risk
    because it may alter completion cadence, first-frame scheduling, in-flight
    depth, lag stability, or timecode behavior.
- Integrated action:
  - Added capture submit observability and wired capture/playback submit rates
    through stream stats, `run-soundcheck`, the stream-stats analyzer, and the
    HAL transport runtime gate.
- Next action:
  - Use the next lock-gated physical window to capture same-session mainline vs
    C++ submit rates, hot-path enqueue/requeue ticks, CPU p95, quality WAV
    metrics, routing, and then Traktor/timecode vinyl evidence once the route
    is valid.

## 2026-06-18 Subagents: Singer HAL Runtime Map and Ampere DriverKit Path

- Agents:
  - Singer (`019ed86e-28f5-7413-9911-961ed7d7a48f`)
  - Ampere (`019ed86e-57d9-7c13-ae5d-1f1dc33c809c`)
- Mission:
  - Singer: map the current HAL/USB runtime enqueue path and identify the
    minimum safe measurement or cadence-reduction change.
  - Ampere: rank prepared-transport and DriverKit components by readiness for
    real runtime integration.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - The current HAL runtime still uses direct `IOUSBHostPipe
    enqueueIORequestWithData` for capture and playback. The bulk/control path
    has another enqueue call, but it is not the isochronous audio hot path.
  - Capture `captureTransfersSubmitted` previously counted queue attempts even
    when IOUSBHost rejected the request, while playback counted only accepted
    submits. This could corrupt future resource-comparison evidence.
  - The closest path to real low-CPU runtime is `AudioDriverSkeleton` plus
    `PreparedTransportBackend`, `PreparedUsbSubmitPlanner`, and
    `PreparedUsbRequestPool`. The `FakeRuntimeAdapter` is useful as a contract
    witness, not as final runtime.
  - DriverKit product integration remains blocked by real `StartIO`/`StopIO`,
    stream memory, timestamp, configuration-change, USBDriverKit pipe, and
    completion-race binding gaps.
- Integrated action:
  - Capture submit accounting was changed to success-only and explicit
    capture/playback submit-attempt counters were added for future evidence.
  - Runtime and physical-comparison contracts now require attempts, accepted
    submits, and submit-failure observability through the evidence chain.
- Next action:
  - Add a compile/static DriverKit binding gate that fails while
    `OpenA8DJAudioDevice` remains pass-through/stub, then add a fake
    USBDriverKit async request interface with late-callback/cancel/restart race
    coverage before any prepared transport hardware window.

## 2026-06-18 Subagent: Banach Offline Analysis Precision

- Agent:
  - Banach (`019ed889-50ec-71b0-aafc-0fc936e5ac36`)
- Mission:
  - Inspect the C++ worktree and recommend the next minimal offline analysis
    package that improves objective audiophile-quality and performance
    precision without hardware.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings:
  - The current offline gates are broad, but they do not yet separate linear
    route coloration, nonlinear distortion, timing instability, statistical
    confidence, and cost per runtime event with enough precision for a
    superiority claim.
  - No new dependency is needed for the first analysis package because the
    current Python stack already has `numpy` and `scipy`.
  - Useful optional installs later: `soundfile`/`libsndfile` for problematic
    24-bit/float/BWF WAV IO, and `soxr` only for mixed-sample-rate comparisons.
- Recommended next metrics:
  - LTI residual by band with coherence and post-correction residual thresholds.
  - Fractional time-warp/wow/flutter bounds.
  - Runtime pressure by event and residual/telemetry correlation.
  - Same-window bootstrap confidence intervals against mainline and known-good.
  - DVS/timecode stress margin with crosstalk, imbalance, drift, and dropout
    cases.
- Integrated action:
  - Recorded the metric package in `docs/SUCCESS_METRICS.md` as the next
    analysis-gate direction.

## 2026-06-17 Subagent Reuse Attempt: Banach DVS Stress Audit

- Agent:
  - Banach (`019ed889-50ec-71b0-aafc-0fc936e5ac36`)
- Mission:
  - Audit the new DVS/timecode stress-margin gate and thresholds for offline
    defensibility.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Coordination note:
  - Spawning a new QA subagent failed because the agent thread limit was already
    reached. Banach was reused through `send_input` instead.
- Status:
  - Completed.
- Findings:
  - Thresholds are defensible for a synthetic offline DVS routing/decode margin
    guard only.
  - The gate does not prove real Traktor-grade timecode vinyl behavior because
    it uses sine/quadrature carriers, not the actual proprietary vinyl encoding,
    direction/speed semantics, wow/flutter, or scope-lock behavior.
  - `deck_swap` needed to be separated from `false_accept`.
  - `timecode_readiness_gate` should fail when offline timecode evidence fails,
    while still keeping product readiness blocked without physical evidence.
- Integrated action:
  - `deck_swap` now compares active and inactive correlated tone levels and
    reports `min_inactive_to_active_tone_gap_db`.
  - `timecode_readiness_gate` now returns FAIL if offline evidence fails, but
    still reports `product_timecode_ready=false` until physical evidence exists.

## 2026-06-18 Subagent Limit and Pascal Runtime Notes

- Attempted new subagent:
  - Runtime/HAL performance scout for real USB submit reduction.
- Result:
  - Spawn failed because the thread had reached the subagent limit.
- Reused agent context:
  - Pascal (`019ed798-3e9c-7f41-8f84-a94f82051179`) already had a completed
    runtime/HAL performance analysis.
- Safety warning supplied in attempted spawn:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Findings reused:
  - The real HAL runtime still pays `enqueueIORequestWithData` in
    `queueCaptureTransfer` and `queuePlaybackWithRequests`.
  - Prepared transport accounting is only meaningful if future patches reduce
    real submit cadence or expose that cadence as still unreduced.
  - The current correct blocker remains: no CPU/resource superiority claim while
    HAL runtime directly requeues USB work.
- Integrated action this turn:
  - Added a separate high-precision offline WAV analyzer instead of pretending
    prepared transport already improves physical runtime.
  - Kept `opena8djcpp_hal_transport_runtime_gate` blocking runtime superiority.
- Next recommended action:
  - Implement real prepared submit runtime or DriverKit USB transport binding,
    then run a lock-gated same-session physical CPU/quality comparison.

## 2026-06-18 Subagent: Banach Analysis Dependency Audit

- Agent:
  - Banach (`019ed889-50ec-71b0-aafc-0fc936e5ac36`)
- Mission:
  - Audit which physical/audiophile analysis scripts still depend on Python or
    external tools, which already have C++ equivalents, and which C++
    conversions would most improve reproducibility and precision.
- Safety warning supplied:
  - `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
- Status:
  - Completed read-only. No files, hardware, audio, CoreAudio, or USB were
    touched by the subagent.
- Findings:
  - The C++ tree now has strong native coverage for WAV analysis,
    soundcheck-quality analysis, physical forensics, direct-USB attribution,
    and the audiophile precision claim guard.
  - The biggest remaining Python/SciPy dependencies that affect future
    superiority evidence are:
    - `scripts/analyze-lti-transfer-quality.py`;
    - `scripts/analyze-fractional-time-warp.py`;
    - `scripts/analyze-runtime-discontinuities.py`.
  - `opena8djcpp_audiophile_precision_claim_gate` currently consumes the JSON
    artifacts from those Python analyses, so schema-compatible C++ replacements
    can reduce dependency risk without changing the claim contract.
- Integrated action:
  - Added `opena8djcpp_audiophile_analysis_stack_contract` to lock the current
    dual-analyzer WAV stack as fail-closed while Python remains an oracle.
  - Recorded the next migration order: LTI transfer quality first, fractional
    time-warp second, runtime discontinuity/residual correlation third.
- Risks:
  - Migrating SciPy spectral analysis to C++ must preserve numeric behavior
    closely enough that old evidence remains interpretable. Until parity tests
    exist, Python should stay as a cross-check rather than be removed.
- Next recommended action:
  - Add a schema-compatible C++ `lti_transfer_quality` tool and a parity gate
    against the existing Python output on saved evidence.

## 2026-06-18 Architect Integration: Existing Subagent Queue After ee3f3c1

- Architect status:
  - New spawn attempt for a dedicated LTI parity analyst failed because the
    thread had reached the subagent limit.
  - Existing completed subagents were queried and their results were integrated
    instead of duplicating work.
- Safety:
  - No subagent was authorized to touch hardware/audio/CoreAudio/USB.
  - Mainline `/Users/fer/dev/opena8dj` and Rust
    `/Users/fer/dev/audio8djrust` remain read-only learning sources.
- Ampere (`019ed86e-57d9-7c13-ae5d-1f1dc33c809c`):
  - Finding: the closest runtime path is `AudioDriverSkeleton` plus
    `PreparedTransportBackend`, `PreparedUsbSubmitPlanner`, and
    `PreparedUsbRequestPool`; the extension source still needs real DriverKit
    SDK/runtime binding and USBDriverKit completion behavior before hardware
    readiness.
  - Next action: compile-only DriverKit SDK binding gate, then fake async
    USBDriverKit request interface with cancellation/restart races.
- Banach (`019ed889-50ec-71b0-aafc-0fc936e5ac36`):
  - Finding: the top remaining claim-critical Python/SciPy dependencies are
    LTI transfer quality, fractional time-warp, and runtime discontinuity
    correlation.
  - Integrated action: `opena8djcpp_lti_transfer_quality_parity_gate` now
    prevents C++ LTI from replacing Python/SciPy until numerical parity passes.
- Pascal (`019ed798-3e9c-7f41-8f84-a94f82051179`):
  - Finding: prepared transport must become real HAL/DriverKit submit cadence
    reduction, not accounting-only.
  - Next action: add a bridge/wrapper around real capture/playback submit paths
    only when it exposes actual submit-attempt/queued counters and fails closed
    if enqueue pressure is not reduced.
- Pasteur (`019ed7de-a957-7f62-84f6-d130065e7f22`):
  - Finding: request shutdown/cancel/restart lifecycle remains a key offline
    blocker.
  - Next action: require late-completion-after-stop and restart-after-cancel
    behavior in the request lifecycle gate before any new physical candidate.
- Singer (`019ed86e-28f5-7413-9911-961ed7d7a48f`):
  - Finding: HAL runtime still directly calls `enqueueIORequestWithData` in the
    audio path; current counters can attribute submit cadence, but not claim
    reduction yet.
  - Next action: introduce default-off wrappers before changing cadence.
- Raman (`019ed827-dc26-7693-a8a3-60ddb566472a`):
  - Finding: the next physical command should be route-only known-good
    non-Audio8 output to iRig capture, producing evidence under
    `local-analysis/physical-superiority-window/...`.
  - Next action: only after explicit lock-gated window, run route
    revalidation; do not run mainline-vs-C++ A/B until route passes.

## 2026-06-18 Architect Integration: Native Time-Warp Port

- Safety:
  - No subagent was authorized to touch hardware/audio/CoreAudio/USB.
  - Mainline `/Users/fer/dev/opena8dj` and Rust
    `/Users/fer/dev/audio8djrust` remained read-only.
- Avicenna (`019ed8f6-406c-79e1-b2e0-bf28b50a5b02`):
  - Mission: identify the next highest-value offline analyzer/oracle after LTI
    parity.
  - Finding: port `scripts/analyze-fractional-time-warp.py` to native C++ and
    add a parity gate against saved Python/SciPy evidence. Current thresholds:
    partial improvement `3.0 dB`, strong improvement `6.0 dB`, max lag `64`
    frames, window `0.25s`, hop `0.125s`, median filter `5`.
  - Integrated action: added `opena8djcpp_fractional_time_warp` and
    `opena8djcpp_fractional_time_warp_parity_gate`, then integrated them into
    CMake/CTest, the offline runner, schema check, and docs.
  - Risk: passing analyzer parity does not prove physical route validity,
    sound quality, CPU/resource superiority, or branch-promotion readiness.
  - Follow-up integrated by architect: added
    `opena8djcpp_runtime_discontinuity_analysis` and
    `opena8djcpp_runtime_discontinuity_parity_gate`, with Debug/Release
    focused parity passing against the saved route-validation bundle.
  - Next action: move from analyzer parity to runtime prepared-submit
    integration.
- Planck (`019ed8f6-66de-7a40-bf67-55d91551b70f`):
  - Mission: identify why performance/resource superiority over mainline
    remains blocked.
  - Finding: prepared submit and request-pool contracts exist, but real runtime
    evidence is still missing; `hal-transport-runtime-gate.json` still reports
    prepared submit not integrated into runtime. DriverKit extension sources
    still contain placeholder/future binding points and artificial timestamp
    behavior.
  - Integrated action: no runtime code changed in this pass; the architect kept
    the time-warp analyzer work separate from runtime/performance changes.
  - Risk: lowering submit count can reduce CPU but still break timing or sound;
    physical A/B must prove both resource use and audio quality.
  - Next action: integrate prepared submit into runtime as a compile-only,
    no-hardware slice before requesting any lock-gated physical comparison.

## 2026-06-18 Architect Integration: HAL Prepared-Submit Adapter

- Safety:
  - Subagents were given the required warning:
    `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
    instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
    /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
    escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
    sin lock global y sin autorización de ventana.`
  - Mainline and Rust remained read-only.
  - No hardware, audio, CoreAudio, USB runtime, driver install/reload, default
    device change, playback, capture, sample-rate change, or buffer-size change
    was performed.
- Wegener (`019ed90e-0cee-7231-a4a9-038fba701235`):
  - Mission: inspect the runtime/prepared-submit blocker.
  - Finding: HAL still calls `enqueueIORequestWithData` directly for capture
    and playback, while the prepared path exists only in offline/core and
    DriverKit shell models.
  - Recommendation: add a HAL prepared-submit adapter contract before changing
    the live queue path.
  - Integrated action: added
    `opena8djcpp_hal_prepared_submit_adapter_contract`, integrated it into
    CMake/CTest, the offline runner, schema check, static policy, and
    `prepared_transport_migration_gate`.
  - Risk: real runtime binding can still break buffer lifetime, first-frame
    scheduling, completion timing, or physical quality.
- Lagrange (`019ed90e-286e-72d0-b9a7-8dadda476a82`):
  - Mission: summarize objective gaps for audiophile quality, functionality,
    Timecode Vinyl, and low resource use.
  - Finding: offline gates are healthy, but product claims remain blocked by
    invalid current capture route evidence, missing same-session physical A/B,
    missing physical Traktor/timecode vinyl validation, DriverKit SDK absence,
    and worse current physical CPU than mainline.
  - Integrated action: kept the new adapter contract as an offline bridge only;
    no readiness claim was made.
  - Next action: after this contract is committed and full offline gates are
    fresh, bind the adapter default-off to real HAL or DriverKit USB submit
    paths, then request a route-revalidation-only lock window before product
    A/B.

## 2026-06-18 Architect Integration: Prepared Runtime Candidate Artifact

- Safety:
  - Subagents used only read-only/codebase analysis. Mainline
    `/Users/fer/dev/opena8dj` and Rust `/Users/fer/dev/audio8djrust` remained
    read-only.
  - No hardware, audio playback, recording, CoreAudio mutation, USB reset,
    driver install/reload, default-device change, sample-rate change, or
    buffer-size change was performed.
- Planck (`019ed8f6-66de-7a40-bf67-55d91551b70f`):
  - Finding: performance superiority remains blocked until prepared submit is
    integrated into a real runtime path and measured physically.
  - Integrated action: created a separated prepared-runtime HAL artifact so the
    next physical comparison can use an explicit candidate instead of a silently
    mutated default build.
- Avicenna (`019ed8f6-406c-79e1-b2e0-bf28b50a5b02`):
  - Finding: analyzer precision is strong enough to keep migrating Python
    oracles to native C++, but physical claims still require valid same-session
    WAV evidence.
  - Integrated action: kept this pass focused on runtime-candidate
    reproducibility, not analyzer threshold changes.
- Mendel (`019ed926-b245-7521-8e39-319e9f7810bd`):
  - Finding: changing the HAL hot path directly is high risk until lifetime,
    completion, timestamp, and request-pool behavior are modeled.
  - Integrated action: the prepared-runtime candidate remains opt-in and
    default-off; the normal HAL bundle is rebuilt after candidate creation.
- Architect route check:
  - Current route-only preflight rejected the available MacBook speaker path as
    built-in/acoustic output. The iRig Stream is visible, but there is no valid
    wired known-good non-Audio8 output into it yet.
  - Next action: acquire the global hardware lock for route-only revalidation
    after a valid wired output route is available. Product A/B remains blocked.

## 2026-06-18 Faraday: HAL Prepared Runtime Dispatch

- Safety:
  - Faraday (`019ed93a-7431-7ad0-a4be-3d34046045c3`) was given the required
    isolation warning and performed read-only inspection only.
  - Mainline and Rust remained read-only. No hardware/audio/CoreAudio/USB
    mutation was performed.
- Mission:
  - Identify the smallest safe slice to move HAL prepared runtime from build
    artifact toward a real runtime submit path.
- Finding:
  - The safest slice is an opt-in HAL submit adapter around
    `queueCaptureTransfer` and `queuePlaybackWithRequests`, preserving submit
    attempts, success-only submitted counters, in-flight playback accounting,
    completion-owned recycling, and physical transaction timestamps.
  - A direct `PreparedUsbRequestPool` C++ binding would require a larger
    Objective-C++ bridge and should wait until the current prepared geometry
    has physical submit-counter evidence.
- Integrated action:
  - Added default-off prepared submit dispatch helpers in `OpenA8DJUSB.m`.
  - Updated source/migration/runtime/schema gates so
    `prepared_runtime_dispatch_path_present=true` is required.
- Risk:
  - Offline gates cannot prove IOUSBHost accepts the prepared cadence, nor that
    64-transaction submits avoid timing faults, short transfers, or sound
    degradation.
- Next action:
  - Commit and rerun full offline gates.
  - Then route-only revalidation under lock before any prepared-runtime
    physical comparison.
## 2026-06-18 Descartes: Prepared USB Runtime Submitter

Subagent:
- `019ed968-26f9-7fc0-8820-447b4604a34b` (`Descartes`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Find the smallest offline/runtime contract that moves the prepared USB submit
  performance path closer to a real HAL/DriverKit implementation without
  touching hardware.

Finding:
- The missing reusable component was a core runtime submitter between
  `PreparedUsbSubmitPlanner` and `PreparedUsbRequestPool`.
- Existing evidence already proved `528` logical slots can become `66` USB
  submits with `33` capture and `33` playback descriptors, but much of that was
  encoded in `tools/hal_prepared_submit_adapter_contract.cpp` rather than a
  core API.

Integrated action:
- Added `PreparedUsbRuntimeSubmitter` in core.
- Added `opena8djcpp_prepared_usb_runtime_submit_contract`.
- Updated the migration gate and evidence schema to require the new contract.

Risks:
- This is still offline model evidence only.
- The HAL prepared-runtime path still needs binding to this core submitter and
  lock-gated physical validation before any CPU/resource claim.

Next action:
- Bind the core submitter into the opt-in HAL prepared runtime path, preserving
  default HAL behavior and all current claim blockers.

## 2026-06-18 Huygens: HAL Runtime Bridge Boundary

Subagent:
- `019ed97a-1def-7393-b53b-0258a535970e` (`Huygens`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Review the smallest safe way to bind the prepared USB submitter into the HAL
  runtime without touching hardware.

Finding:
- Do not include C++ directly from `OpenA8DJUSB.m`; it is Objective-C.
- The HAL needs an opt-in C ABI / Obj-C++ bridge with explicit async request
  handles, completion-owned recycling, cancel/drain behavior, and separate
  counters.
- The default HAL must remain untouched and the prepared path must stay
  build-only until lock-gated evidence exists.

Integrated action:
- Added `PreparedUsbAsyncRuntime` in core.
- Added `OpenA8DJPreparedRuntimeBridge.h/.mm`.
- Made `make hal-prepared-runtime` compile per-language objects and link C++
  only for the prepared profile.
- Updated binding, migration, schema, and offline-runner gates.

Risks:
- No physical USB submit cadence, CPU, audio quality, or timecode-vinyl behavior
  is proven by this step.
- Latest HAL safety evidence is still failing due audio-stack CPU.

Next action:
- Commit this bridge, rerun full offline gates from the clean HEAD, then request
  only a lock-gated route/safety window before any product A/B.

## 2026-06-18 Schrodinger: HAL Physical Timing Split

Subagent:
- `019ed9f8-7802-7cd0-b905-5eaa84551eb1` (`Schrodinger`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Explain why direct USB can show zero lag jumps while HAL physical capture
  still shows lag jumps and low SNR.

Findings:
- The current failure should be split into two issues:
  - HAL/CoreAudio/USB timing mismatch and capture-paced playback can introduce
    lag instability.
  - The physical/iRig route still has low SNR/residual even when direct USB
    payloads are internally clean.
- Existing HAL stream stats show the USB clock anchor remains invalid/fallback
  in recent runs; direct USB does not prove the HAL timebase is sound.

Integrated action:
- Used the finding to avoid a blind quality claim and to keep the next physical
  window diagnostic-only.

Risk:
- A physical run may still fail because the route/SNR problem is independent of
  HAL submit cadence.

Next action:
- For the next lock-gated diagnostic, record direct USB and HAL evidence with
  dense stream stats and compare written/consumed/packed/captured paths.

## 2026-06-18 Bohr: HAL CPU Enqueue Reduction Options

Subagent:
- `019ed9f8-58e6-7711-9740-eeb62e7da36c` (`Bohr`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Identify lower-risk ways to reduce HAL CPU without repeating rejected
  physical experiments.

Findings:
- The main CPU hotspot is IOUSBHost async enqueue cadence, not packing.
- Lowest-risk next option: batch capture transfers while preserving playback
  ISO8 cadence. This should reduce capture submits without repeating the
  playback coalescing failure.
- Do not repeat global ISO64, playback coalescing, raw/reused completion-only
  probes, stats-off, output-only no-capture, ignore-sample-time, flush-write, or
  prepared runtime as default.

Integrated action:
- Implemented opt-in `hal-capture-batch-diagnostic`.
- Updated `queueCapturePacedPlaybackWithRequests` so large capture completions
  are split into logical playback ISO8 submits when playback coalescing is off.
- Strengthened `opena8djcpp_hal_logical_capture_batching_contract`.

Risk:
- Capture batching could still affect DVS/timecode input granularity or HAL
  capture timing. It needs physical metrics before any claim.

Next action:
- Commit and rerun freshness gates, then run a short lock-gated diagnostic only
  if the lock is available.

## 2026-06-18 Maxwell: Physical Route Forensics

Subagent:
- `019eda08-00f3-7113-935a-94c25a322761` (`Maxwell`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Determine whether the current catastrophic physical results are explained by
  C++ output corruption, the capture route, timebase instability,
  channel/polarity, clipping, or a known-good route gap.

Findings:
- The capture-batch failure is not useful as a CPU win and is dominated by bad
  physical quality evidence.
- Simple L/R swap, polarity, clipping, static matrix, and simple nonlinearity
  explanations do not rescue the current captures.
- Current route evidence points to route/timebase instability, not a clean proof
  of digital output corruption.
- No separate wired non-Audio8 known-good output is currently available for a
  promotion-valid same-window route check.

Integrated action:
- Ran iRig idle and direct USB route isolation under lock.
- Confirmed iRig idle is quiet enough at rest and direct USB internal written,
  consumed, written-vs-consumed, and decoded USB payloads are perfect.
- Documented that the analog iRig capture of the direct USB signal still fails
  strict audiophile/product thresholds and blocks further claims.

Risks:
- The current physical route can produce misleading optimization results:
  digital USB can be clean while analog capture metrics fail badly.
- Timecode Vinyl and branch-promotion claims remain blocked because they require
  stable same-window physical evidence.

Next action:
- Revalidate the physical route/timebase before any more HAL performance
  probes. Prefer a separate wired non-Audio8 known-good output into the iRig, or
  a controlled route-only Audio 8 DJ diagnostic with explicit claim blockers.

## 2026-06-18 Peirce: Claim Gate Hardening Review

Subagent:
- `019eda12-dce5-7ca2-8d9c-01f56a9c1300` (`Peirce`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Inspect, read-only, which offline gate should be reinforced so clean direct
  USB internals plus failed analog iRig capture cannot support a product claim.

Findings:
- `capture-route-health-gate` already classifies the route symptom correctly.
- `physical-window-readiness-gate` already blocks product A/B until route
  revalidation.
- The missing preservation point was `product-quality-claim-gate`: it only
  reported the generic route blocker and did not carry
  `direct_usb_capture_failed_after_clean_payload` as a hard causal blocker.

Integrated action:
- Added the hard blocker to `product-quality-claim-gate`.
- Added schema requirements for direct USB audiophile fields and the hard
  product-quality blocker.
- Regenerated offline evidence; diagnostic gates pass and product readiness
  remains blocked.

Next action:
- Keep physical work limited to route revalidation until the blocker is cleared
  by fresh evidence, not by code changes alone.

## 2026-06-18 Hypatia: Wide-Lag Analyzer Disagreement

Subagent:
- `019ed9e0-3d8f-7840-8562-fbb4ccb034bf` (`Hypatia`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Inspect why legacy `metrics.json` quality could disagree with the native C++
  and audiophile WAV analyzers on saved physical music runs.

Findings:
- A very wide `--max-lag` could let the Python analyzer lock onto a later
  repeated segment of music, producing misleading metrics.
- Bounded alignment and the C++/audiophile analyzers agreed on a more plausible
  lag in the saved run family.
- Product decisions should fail closed on analyzer disagreement.

Integrated action:
- Confirmed `run-physical-superiority-window` now uses bounded `--max-lag
  48000`.
- Hardened `scripts/analyze-soundcheck-capture.py` to emit and fail on
  `alignment_ambiguous=1` when a wide search conflicts with bounded alignment.
- Added `opena8djcpp_soundcheck_alignment_guard` CTest coverage.

Risks:
- This improves analyzer truthfulness but does not fix the physical route/SNR
  problem or prove the C++ driver is better than mainline.

Next action:
- Rerun full offline gates and keep all product claims blocked until route
  validity, same-session A/B, CPU/resource, and Timecode Vinyl gates pass.

## 2026-06-18 Reused Subagents: Claim/Resource Gap Review

Subagents:
- `019ed9e0-3d8f-7840-8562-fbb4ccb034bf` (`Hypatia`)
- `019ed9f8-58e6-7711-9740-eeb62e7da36c` (`Bohr`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Missions:
- Hypatia: read-only review of audiophile/quality gates for false-positive
  claim risk.
- Bohr: read-only review of offline performance/resource gates for the gap that
  still prevents objective CPU/jitter superiority over mainline.

Findings:
- Hypatia found that both audiophile analyzers could pass without explicit
  alignment-score gating and that the physical comparator required both JSONs
  but not numerical agreement between C++ and Python.
- Bohr found that offline performance gates model submit reduction and hot-path
  shape, but still do not provide a calibrated CPU/jitter superiority model
  against the mainline p95 budgets; physical same-session evidence remains
  mandatory.

Integrated action:
- Added `min_alignment_score` to both audiophile WAV analyzers.
- Added C++/Python dual-oracle agreement gates for alignment score, lag, SNR
  floor, and delay p95 in `physical_run_compare`.
- Hardened `audiophile_analysis_stack_contract` so those checks stay required.

Next action:
- Add a separate resource-superiority model gate that consumes prepared-runtime
  submit ratios, hot-path timing, and mainline CPU budgets, while still
  blocking final CPU/resource claims until lock-gated same-session physical A/B
  evidence exists.

## 2026-06-18 Pauli: Mainline Metrics Archaeology Refresh

Subagent:
- `019eda9d-29ff-7902-88da-741f3b9a4c30` (`Pauli`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Read `/Users/fer/dev/opena8dj` in read-only mode and extract current
  objective baselines for sound quality, routing, Timecode Vinyl, CPU/resource
  use, and iRig route evidence.

Findings:
- Mainline `0.3.135` remains the primary no-iRig/digital CPU baseline:
  simulated SNR `75.22 dB`, residual `0.000669`, click outliers `0`, and
  driver CPU p95 around `6.6-6.8%`.
- Mainline `0.3.25` remains the functional/timecode topology baseline:
  8 inputs, 8 outputs, A/B/C/D surface, and `timecode-vinyl` profile behavior.
- The actual `scripts/physical-music-quality-gate` defaults are stricter than
  older prose docs: alignment `>=0.970`, lag jumps `<=3`, CPU/noise
  correlation `<=0.08`, mid residual `<=1.38`, high residual `<=1.32`.
- Known physical route remains `Open Audio 8 DJ -> external mixer -> mixer REC
  OUT -> iRig Stream -> macOS capture`; software loopback is not promotion
  evidence.

Integrated action:
- Updated `docs/MAINLINE_BASELINE_METRICS.md` to make the script defaults
  authoritative over looser historical prose.
- Hardened tone-quality claim gates so saved-tone PASS does not imply
  historical physical-tone superiority.

Risks:
- Existing C++ physical evidence still does not beat mainline on strict
  physical quality, CPU/resource use, route validity, or Timecode Vinyl.

Next action:
- Keep hardware work focused on same-window route validation with a separate
  wired non-Audio8 known-good source before any C++ vs mainline A/B claim.

## 2026-06-18 Fermat: Rust Oracle Refresh

Subagent:
- `019eda9d-4142-7b53-969d-adc1659d637b` (`Fermat`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Read `/Users/fer/dev/audio8djrust` in read-only mode and extract reusable
  gates, analyzers, thresholds, and DVS/timecode oracle behavior for C++.

Findings:
- Rust no-iRig gates remain the best immediate software oracle: mode-2
  pack/unpack, simulated A/B/C/D output, routing matrix, DVS matrix, and
  synthetic timecode analysis.
- Rust thresholds to preserve include pack throughput `>=100 MiB/s`,
  `>=1,000,000 frames/s`, `check_errors=0`, `panic_flags=0`, timecode RMS
  `>=0.05`, balance `<=1 dB`, frequency error `<=50 ppm`, jitter p95
  `<=2 frames`, abs correlation `>=0.95`, clips `0`, and DVS C/D leakage
  `<=0.0001 RMS`.
- Rust PM schema keeps `PASS`, `FAIL`, `NOT_READY`, and `BLOCKED_*` distinct;
  C++ must preserve this semantic split so diagnostic gates cannot become
  readiness claims.

Integrated action:
- Existing C++ offline gates already cover most Rust no-iRig oracle families.
- Added this refresh to the living handoff and Rust learning docs as the
  oracle checklist for remaining gaps.

Risks:
- Rust no-iRig PASS still does not prove DAC quality, iRig route validity,
  Traktor scope, physical vinyl/CD-line behavior, or human listening.

Next action:
- Keep C++ DVS/timecode physical readiness blocked until Traktor/vinyl
  evidence exists under the hardware lock after route validation.

## 2026-06-18 Aristotle: ISO8-Preserving CPU Optimization Review

Subagent:
- `019edb01-22a4-7552-8e27-77ebdd089788` (`Aristotle`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Identify the next CPU optimization with the lowest risk after physical
  rejection of capture batching above ISO8 and input decode batch publication.

Findings:
- Recommended optimizing prepared runtime observability/lifecycle overhead:
  avoid refreshing/copying merged request-pool counters on every
  submit/completion, and instead materialize snapshots on demand.
- Explicitly rejected reopening capture batching or input decode publication.
- Recommended preserving ISO8/ISO8/coalesce1, per-frame timing, packet bytes,
  input ring publication timing, and Timecode Vinyl semantics.

Integrated action:
- Implemented lazy counter snapshots in `PreparedUsbAsyncRuntime` and
  `PreparedUsbRuntimeSubmitter`.
- Extended offline contracts to prove snapshots still report in-flight and
  final counters correctly.

Risks:
- Expected CPU benefit is moderate and offline-only until the prepared runtime
  path is bound to real HAL/USB and measured. It is not a superiority claim.

Next action:
- Keep future CPU work in the same class: fixed lifecycle/observability
  reductions that do not perturb audio cadence or ring publication timing.

## 2026-06-18 Godel: Human-Test Release Path Review

Subagent:
- `019edb07-908e-75d3-848c-96c8c66065bc` (`Godel`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Identify the installable path for a first human-test candidate under the
  15:00 America/New_York milestone.

Findings:
- The viable path today is HAL bundle / PKG / DMG, not DriverKit/dext.
- DriverKit real install is blocked by missing full Xcode, missing DriverKit
  SDK, missing `iig`, insufficient free disk space, and missing real
  entitlements/provisioning.
- Existing build path is `make dist`; existing install paths mutate the system
  and must be lock-gated.

Integrated action:
- Added `docs/HUMAN_TEST_CANDIDATE_2026-06-18.md` with HAL candidate build,
  lock-gated install/smoke, rollback, stop conditions, and allowed claims.

Risks:
- The package version remains `0.3.25`; the human-test candidate must be
  identified by commit/hash/evidence, not public semantic version.
- A HAL candidate can be human-tested today, but it is not DriverKit readiness.

Next action:
- Build `make dist`, hash artifacts, then run lock-gated safety/quality smoke
  before any human listening.

## 2026-06-18 Kierkegaard: Input/Output Ring SPSC Review

Subagent:
- `019edb27-8f80-7071-86f0-6949e21a6e29` (`Kierkegaard`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Review whether the HAL input/output rings can migrate to SPSC/bulk without
  changing packet format, routing, sample rate, USB cadence, or Timecode
  semantics.

Findings:
- Input ring is the safer first migration target: producer/consumer shape is
  simple and the relevant functions are `RingWrite`, `RingRead`, `RingClear`,
  `appendInputByte`, `readInput`, and input-decode toggles.
- Output timeline must not be replaced by a generic FIFO. It preserves
  `sampleTime`, preroll, late writes, high-water drops, startup silence, and
  elastic playback behavior.
- Any SPSC migration must preserve the 8-channel frame shape, input
  transforms, Mode2 packing, sample-rate policy, and ISO8 cadence.

Integrated action:
- Added an opt-in `HAL_INPUT_SPSC_RING=1` diagnostic build path and an offline
  contract.
- Kept the output timeline and default input ring unchanged.

Risks:
- The input SPSC path is physically untested and must not become default until
  input routing, DVS/timecode behavior, latency, and quality are verified under
  lock.

Next action:
- Run full offline gates after commit; if clean, only then consider a short
  lock-gated SPSC diagnostic smoke with strict unload/recovery.

## 2026-06-18 Bernoulli: 15:00 EDT Human-Test Readiness Review

Subagent:
- `019edb43-5f35-7610-a38a-9a61ce0dcfc9` (`Bernoulli`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Review, read-only, the blockers and minimum gates for a first human-test
  candidate by 15:00 America/New_York.

Findings:
- Recommended a controlled HAL/PKG diagnostic path for today, not DriverKit/dext
  and not a product-quality claim.
- Identified reproducible identity as the first blocker: current source,
  evidence, package hashes, and commit must match.
- DriverKit remains blocked by tooling, SDK, `iig`, disk, and
  entitlements/provisioning.
- Current iRig route is not promotable: no known-good non-Audio8 route is
  available, same-device iRig diagnostics fail, and physical music metrics fail
  for current C++, ISO5 diagnostic, and same-session mainline.
- CPU/resource, Traktor/timecode physical lock, full A/B/C/D physical routing,
  and same-session mainline comparison remain gaps.

Integrated action:
- Updated the human-test candidate runbook with a 11:06-15:00 EDT
  stabilization plan.
- Updated direct USB and capture-forensics tools so evidence under
  `local-analysis/human-test-candidate` participates in offline claim gates.
- Regenerated offline diagnostic evidence under `local-analysis/cpp-offline`.

Risks:
- A human diagnostic listen may still be useful, but it must not be labeled as
  product readiness or superiority while capture-route metrics are blocked.

Next action:
- Freeze a reproducible candidate with offline gates and package hashes, then
  only run lock-gated hardware smoke if the route and rollback preflights pass.

## 2026-06-18 Mill: iRig Route Recovery / Driver-vs-Route Split

Subagent:
- `019edb50-7899-7f42-87a2-7f4bada77505` (`Mill`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Read-only route diagnosis and minimum plan to separate iRig/cable/capture
  failure from Audio 8 driver failure.

Findings:
- Best separator is `scripts/run-known-good-route-soundcheck` with an explicit
  wired non-Audio8 output into iRig capture; same-device Audio 8 loopback is
  diagnostic only and not valid for promotion.
- Existing evidence already points at route/capture as suspect: mainline also
  failed in the same session, direct USB internals were clean, and physical
  iRig capture failed after the clean payload.
- Recommended order:
  1. Validate iRig route with known non-Audio8 wired source.
  2. Repeat direct USB Audio 8 without HAL and keep USB-internal vs iRig
     capture separated.
  3. Sweep A/B/C/D matrix to detect wrong physical pair or no-signal route.
  4. Only then classify timebase/no-signal failure modes.

Integrated action:
- Added `opena8djcpp_physical_route_matrix_contract`, which consumes the final
  A/B/C/D sweep and classifies the current route as
  `all_audio8_pairs_no_useful_correlated_capture`.

Risks:
- Without a non-Audio8 wired known-good output, the iRig route cannot support
  product-quality, CPU/resource, Timecode Vinyl, or branch-promotion claims.

Next action:
- Use the hardware lock only for route validation with a known non-Audio8 wired
  source, or keep the 15:00 candidate explicitly diagnostic-only.

## 2026-06-18 Kierkegaard: Remaining 15:00 EDT Stabilization Plan

Subagent:
- `019edb27-8f80-7071-86f0-6949e21a6e29` (`Kierkegaard`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Produce a 15:00 EDT stabilization plan from the current C++ worktree state.

Findings:
- The only defensible target is a frozen installable HAL/PKG diagnostic RC, not
  a DriverKit/dext or product-readiness candidate.
- Current gates classify `product_human_test_allowed=false`,
  `route_revalidation_ready=false`, `timecode_vinyl_human_test_allowed=false`,
  `cpu_superiority_claim_allowed=false`, and `branch_promotion_allowed=false`.
- The next critical step is not more transport tuning; it is a lock-gated
  known-good route validation after a wired non-Audio8 output appears.

Integrated action:
- Kept the watcher/readiness work fail-closed and diagnostic-only.
- Preserved the product claim blockers in the offline evidence schema.

Risks:
- A human listen before route validation would mix driver behavior, cable path,
  iRig capture, and physical routing into one non-attributable signal.

Next action:
- Freeze and verify the diagnostic RC, then only move to physical validation
  if the known-good route watcher reports READY.

## 2026-06-18 Bernoulli: Metrics Gap Audit Refresh

Subagent:
- `019edb43-5f35-7610-a38a-9a61ce0dcfc9` (`Bernoulli`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Audit what metrics can and cannot support a claim that C++ is better than
  mainline in sound quality, Timecode Vinyl, CPU, and stability.

Findings:
- Offline/model metrics are strong, including simulated routing, timecode
  profiles, DVS smoke, and clean direct USB internals.
- Physical/product metrics remain failing or missing: same-session route
  validation, C++/mainline A/B, CPU superiority, physical Timecode Vinyl,
  and current known-good non-Audio8 route validation.
- Current same-device route metrics cannot support product claims because the
  route/capture evidence is classified diagnostic-only.

Integrated action:
- Kept watcher PASS semantics diagnostic: a blocked route watcher can pass as
  a classifier but cannot unlock product listening.

Risks:
- Any claim of superiority before same-window physical A/B would be an
  evidence error even if the offline gate is green.

Next action:
- Re-run offline gates post-commit, then use only lock-gated route validation
  to generate the next physical evidence.

## 2026-06-18 Mill: Installable RC and Rollback Review

Subagent:
- `019edb50-7899-7f42-87a2-7f4bada77505` (`Mill`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Review whether the current C++ candidate is installable end-to-end and what
  rollback/observability gaps remain.

Findings:
- The HAL/PKG/DMG path is installable for a private diagnostic RC, but it is
  not a dext/DriverKit product candidate.
- Rollback can uninstall the diagnostic candidate and restart CoreAudio under
  authorization, but it does not automatically restore a previous mainline HAL.
- The active diagnostic HAL hash is a useful identity anchor, but it does not
  prove route quality, CPU superiority, or Timecode Vinyl behavior.

Integrated action:
- Added the known-good route watcher to shorten and de-risk the next physical
  window before any human product listen is considered.

Risks:
- Installing or uninstalling still touches CoreAudio and must remain under the
  hardware/audio lock and an explicit window.

Next action:
- Keep the RC diagnostic-only until a validated known-good route and rollback
  plan exist for the exact same physical window.

## 2026-06-18 Kepler: Physical Route Captain For 15:00 RC

Subagent:
- `019edb93-1623-7ce0-be5b-bab0a28d6753` (`Kepler`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Inspect the C++ route/evidence scripts read-only and produce an operational
  six-hour physical-route plan for a controlled human-test RC.

Findings:
- `iRig Stream` and `Open Audio 8 DJ` are visible, and the hardware lock is
  free.
- Audio 8 is visible as an 8-in / 8-out CoreAudio device.
- The blocker is not iRig absence; it is lack of a wired non-Audio8,
  non-built-in known-good output source for objective route validation.
- Built-in speakers, iRig-as-both-output-and-capture, virtual devices, and
  Audio 8 as the known-good source must stay diagnostic-only.

Integrated action:
- Added the route-blocked 15:00 RC strategy to `docs/CPP_DRIVERKIT_PLAN.md`.
- Added `scripts/human-test-rc-status` so the current RC state can be
  regenerated as one read-only JSON summary.

Risks:
- Running product listening before known-good route validation would produce a
  non-attributable result and could falsely blame or clear the driver.

Next action:
- Provision a wired non-Audio8/non-built-in output into the iRig capture path,
  rerun the watcher, then run only the generated lock-gated command.

## 2026-06-18 Herschel: Metrics Compression For 15:00 RC

Subagent:
- `019edb93-2b9d-71b0-8521-5f7cebda9249` (`Herschel`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Compress the remaining 2026-06-18 work into a 15:00 America/New_York
  decision plan with must-have metrics, abort conditions, and allowed claims.

Findings:
- `current-offline-gates.json` is PASS at commit `d41b2d3`, but product human
  testing, Timecode Vinyl human testing, CPU superiority, and branch promotion
  are all still disallowed.
- Current artifacts support a diagnostic HAL/PKG RC only.
- The next useful metric is not another HAL tuning result; it is a lock-gated
  known-good physical route validation followed by same-session C++/mainline
  A/B if route validation passes.

Integrated action:
- Kept the 15:00 output taxonomy explicit: route-blocked, diagnostic human
  test, limited human test needing A/B, or product claim allowed only with
  actual same-session physical evidence.

Risks:
- Treating offline Timecode/DVS gates as physical Timecode Vinyl readiness
  would overclaim. Physical Traktor/timecode remains a separate gate.

Next action:
- Use `local-analysis/cpp-offline/human-test-rc-status.json` as the live RC
  decision packet before any hardware window.

## 2026-06-18 Kuhn: Human RC Auditor

Subagent:
- `019edbb1-d0fb-7592-b4a3-443e2c61ed95` (`Kuhn`)

Required warning given:
- `PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.`

Mission:
- Audit whether a human-test RC can honestly be declared before 15:00 EDT.

Findings:
- The current candidate is a `diagnostic-installable-rc`, not a product human
  RC.
- Offline gates and packaging are ready, but the route/capture evidence blocks
  product listening, Timecode Vinyl physical testing, same-session mainline/C++
  A/B, and CPU superiority claims.
- DriverKit/deXt is not viable on this host today because the real DriverKit
  SDK/selected full Xcode prerequisites are missing. The practical candidate
  today is HAL/PKG.

Integrated action:
- Added a route-contamination analysis gate so the downstream clean-USB /
  failed-capture condition is visible in `current-offline-gates.json` instead
  of only in narrative docs.

Risks:
- Any human listening before route validation can be a false negative from the
  external capture/monitoring path rather than the driver.

Next action:
- Keep the 15:00 decision taxonomy explicit: diagnostic RC if route remains
  blocked; limited product human test only after a wired non-Audio8 known-good
  source validates the iRig route under lock.

## 2026-06-18 Architect Continuation: Fail-Closed Human RC Status

Subagent:
- Main architect, no new subagent.

Required warning:
- Continued under the standing project rule: `/Users/fer/dev/opena8dj` and
  `/Users/fer/dev/audio8djrust` are read-only; only
  `/Users/fer/dev/audio8djcpp` is writable; no hardware/audio/CoreAudio/USB
  action without lock and window authorization.

Mission:
- Continue toward objective superiority without pretending the diagnostic RC is
  product-ready.

Findings:
- Live inventory still shows iRig and Audio 8, but no non-Audio8/non-built-in
  known-good output. The blocker is route/capture validation, not compilation.

Integrated action:
- Updated `scripts/human-test-rc-status` to consume route-contamination and
  Timecode physical-window evidence and return
  `DIAGNOSTIC_RC_ARTIFACTS_READY_ROUTE_CONTAMINATED` with product audio windows
  explicitly disallowed.

Risks:
- Human listening through the current route is likely non-attributable because
  the route is contaminated after a clean USB boundary.

Next action:
- Do not run product listening or Timecode Vinyl. First validate a wired
  non-Audio8 known-good output into iRig under lock; only then run same-session
  mainline/C++ A/B and CPU/submit comparison.

## 2026-06-18 Architect Continuation: Final Objective Gate

Subagent:
- Main architect, no new subagent.

Required warning:
- Continued under the standing project rule: `/Users/fer/dev/opena8dj` and
  `/Users/fer/dev/audio8djrust` are read-only; only
  `/Users/fer/dev/audio8djcpp` is writable; no hardware/audio/CoreAudio/USB
  action without lock and window authorization.

Mission:
- Turn the user's full objective into an executable fail-closed readiness
  evaluator before the 15:00 EDT human-test decision.

Findings:
- At 13:20 EDT the remaining window is less than two hours, not six.
- The candidate can be packaged and reviewed as an installable diagnostic RC,
  but it cannot honestly be called better than mainline without validated
  physical route evidence, same-session A/B quality, CPU/resource comparison,
  and Timecode Vinyl physical proof.

Integrated action:
- Added `scripts/evaluate-final-objective-readiness.py` and a fixture test.
- Integrated final objective fields into `scripts/run-cpp-offline-gates` and
  `opena8djcpp_evidence_schema_check`.

Risks:
- A human test performed through the currently contaminated route can create a
  false product failure or false recovery signal.

Next action:
- Run focused checks, commit the final objective gate, then regenerate the full
  offline bundle. If a known-good output becomes visible before 15:00, use the
  planned lock-gated route-validation and same-session A/B path; otherwise
  freeze the diagnostic RC and evidence package.

## 2026-06-18 Architect Continuation: Human-Test RC Packet

Subagent:
- Main architect, no new subagent.

Required warning:
- Continued under the standing project rule: `/Users/fer/dev/opena8dj` and
  `/Users/fer/dev/audio8djrust` are read-only; only
  `/Users/fer/dev/audio8djcpp` is writable; no hardware/audio/CoreAudio/USB
  action without lock and window authorization.

Mission:
- Make the next human/diagnostic window operable from one evidence packet
  instead of scattered JSON and docs.

Findings:
- A read-only watcher at 17:27Z still found iRig and Audio 8 visible, but no
  valid non-Audio8/non-built-in output for route validation.
- DriverKit remains blocked by missing full Xcode/DriverKit SDK on this host.

Integrated action:
- Added `scripts/build-human-test-rc-packet.py` and fixture test.
- Integrated the packet into `scripts/run-cpp-offline-gates` and
  `opena8djcpp_evidence_schema_check`.

Risks:
- The packet can make a diagnostic RC easier to review; it must not be
  interpreted as product audio readiness.

Next action:
- Regenerate the full offline bundle and commit if the schema requires the
  packet successfully. Then use the packet's watcher command until a valid
  route appears.

## 2026-06-18 Architect Continuation: Objective External Readiness Audit

Subagent:
- Existing subagent audit results were reused; a new spawn attempt was rejected
  because the agent thread limit was already reached.

Required warning:
- Continued under the standing project rule: `/Users/fer/dev/opena8dj` and
  `/Users/fer/dev/audio8djrust` are read-only; only
  `/Users/fer/dev/audio8djcpp` is writable; no hardware/audio/CoreAudio/USB
  action without lock and window authorization.

Mission:
- Add a fail-closed audit that separates candidate-internal evidence from
  external prerequisites for human product testing and branch promotion.

Findings:
- Existing subagents agreed that the current deliverable can be an installable
  diagnostic RC, but not a product-quality RC or mainline-superior candidate.
- External blockers remain decisive: dirty mainline reference, missing full
  Xcode/DriverKit SDK, insufficient disk for Xcode, missing wired non-Audio8
  known-good route, no same-session mainline/C++ A/B, no physical Timecode
  window, and no final objective proof.

Integrated action:
- Added `scripts/audit-objective-external-readiness.py` and a fixture test.
- Integrated external readiness fields into `scripts/run-cpp-offline-gates` and
  `opena8djcpp_evidence_schema_check`.

Risks:
- A six-hour stabilization target can produce a useful RC only if the label is
  honest. Calling the current diagnostic package a product or Timecode Vinyl
  RC would exceed the evidence.

Next action:
- Regenerate the full offline bundle and use
  `objective_external_readiness_status` as a hard blocker for promotion and
  product human audio. If a wired known-good route becomes available, run only
  the lock-gated route validation first.

## 2026-06-18 Architect Continuation: Human Packet External Gate

Subagent:
- Main architect, no new subagent.

Required warning:
- Continued under the standing project rule: `/Users/fer/dev/opena8dj` and
  `/Users/fer/dev/audio8djrust` are read-only; only
  `/Users/fer/dev/audio8djcpp` is writable; no hardware/audio/CoreAudio/USB
  action without lock and window authorization.

Mission:
- Ensure the operator-facing human-test packet cannot hide external blockers
  already found by the objective readiness audit.

Findings:
- `current-offline-gates.json` already had objective external readiness fields,
  but `human-test-rc-packet.json` did not carry the same hard blocker in its
  own decision object.

Integrated action:
- Added an `external_readiness` block to `scripts/build-human-test-rc-packet.py`.
- The packet promotion label now requires both `objective_achieved=true` and
  `external_readiness.objective_ready=true`.
- The summary schema now requires the packet to report
  `external_readiness_status=BLOCKED` and all external permission booleans
  false in the current evidence state.

Risks:
- Without this propagation, an operator could read the packet and miss dirty
  mainline/toolchain/route blockers that are visible elsewhere.

Next action:
- Regenerate the full offline bundle and keep the packet as the single handoff
  for any future lock-gated route validation window.

## 2026-06-18 Architect Continuation: Source-Reference Baseline Pivot

Subagent:
- Main architect, no new subagent.

Required warning:
- Continued under the standing project rule: `/Users/fer/dev/opena8dj` and
  `/Users/fer/dev/audio8djrust` are read-only; only
  `/Users/fer/dev/audio8djcpp` is writable; no hardware/audio/CoreAudio/USB
  action without lock and window authorization.

Mission:
- Apply the operator correction that the baseline reference is the original
  file or generated tone, not a separately wired non-Audio8 output.

Integrated action:
- Updated the physical-window planner, RC packet, external readiness audit,
  final objective evaluator, route-contamination analyzer, and schema checks to
  use source-reference policy.
- The next lock-gated human-baseline command is now
  `lock_gated_source_reference_mainline_cpp_ab`.
- Active policy: original file/tone -> Audio 8 DJ output -> iRig capture,
  same-session mainline C vs C++ comparison, with CPU/resource sampling and
  Timecode Vinyl still required before any product/superiority claim.

Risks:
- This prepares the baseline window; it does not prove quality, CPU superiority,
  or Timecode Vinyl readiness.
- Mainline dirty state and missing full DriverKit SDK remain external blockers
  for final objective and Legacy/main promotion.

Next action:
- Run the lock-gated source-reference A/B window when the user opens the
  hardware window, then evaluate quality/CPU evidence before any claim.

## 2026-06-18 Architect Continuation: 15:00 Diagnostic Functional RC

Subagents:
- Physical Quality Evidence Analyst.
- Prepared Runtime Performance Analyst.

Required warning:
- Both subagents received: "PROHIBIDO tocar, editar, formatear, generar
  archivos, limpiar, resetear, instalar o mutar cualquier cosa en
  /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son
  READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar
  hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana."

Mission:
- Re-prioritize the 15:00 EDT target toward functionality and stability rather
  than audiophile-quality or CPU superiority.

Findings:
- The default C++ HAL improves over the same-session mainline on alignment and
  SNR because its transport geometry is coherent and avoids mainline panic
  flags, but it still fails quality thresholds. The remaining quality blocker
  looks like timing/pacing and route/capture residual, not basic packet packing.
- Prepared-runtime is not suitable for the 15:00 RC. Its failure looks like a
  CoreAudio enumeration/load CPU storm, not a visible spin inside the
  `OpenA8DJ.driver` process. The next safe performance experiment is a
  prepared-lite profile, likely `slots-per-submit=2`, not the current 8x batch.

Integrated action:
- Kept the default HAL as the diagnostic RC candidate.
- Ran a lock-gated `--leave-loaded` safety smoke and left the default HAL active
  after it passed.
- Ran short source-reference functional smokes through Audio 8 DJ to iRig.
- Documented the result as `diagnostic-functional-rc` only.

Risks:
- Physical quality still fails strict thresholds.
- Timecode Vinyl remains unproven physically.
- CPU/resource superiority over mainline is not proven.
- The active HAL is suitable only for controlled diagnostic listening with
  rollback ready, not for product claims.

Next action:
- For the human window, use the active default HAL only as a diagnostic
  functional RC.
- For later perfection, test an opt-in USB-clock-anchor candidate for timing and
  a prepared-lite candidate for CPU, each under separate lock-gated windows.

## 2026-06-18 Architect Continuation: 15:00 Stability Freeze

Subagent:
- Integration Readiness Auditor.

Required warning:
- The subagent received: "PROHIBIDO tocar, editar, formatear, generar
  archivos, limpiar, resetear, instalar o mutar cualquier cosa en
  /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son
  READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar
  hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana."

Mission:
- Keep the 15:00 EDT candidate focused on functionality and stability, while
  preserving quality/CPU experiments as isolated follow-up candidates.

Findings:
- The Makefile targets are opt-in and are not part of `all` or the normal
  `hal` target.
- The USB-clock builder requires a candidate path separate from
  `build/OpenA8DJ.driver`, writes `product_claim_allowed=false`, and restores
  the normal HAL after copying the experiment.
- Risk remains if a build is interrupted while `build/OpenA8DJ.driver` is
  temporarily experimental; therefore `make dist` is required before treating
  the default distribution HAL as the RC again.

Integrated action:
- Added `make hal-usb-clock-candidate` for a separate USB-clock-anchor timing
  bundle.
- Added `make hal-prepared-lite-candidate` for a separate lower-risk
  prepared-submit CPU bundle.
- Rebuilt both candidates sequentially, then regenerated the distribution RC
  with `make dist`.
- Updated the human-test candidate document so the 15:00 policy clearly
  prioritizes functionality and stability over perfection.

Risks:
- The new candidates are build-only and untested physically.
- They must not be installed over the active diagnostic RC unless a separate
  lock-gated window is explicitly chosen.
- They do not change the current label: `diagnostic-functional-rc`.

Next action:
- Preserve the default RC for the 15:00 diagnostic human baseline.
- After the baseline, run one experimental physical window at a time, starting
  with USB-clock-anchor if timing/quality is the next blocker, or prepared-lite
  if CPU/resource consumption becomes the limiting blocker.

## 2026-06-18 Architect Continuation: USB-Clock Rejection

Subagent:
- Main architect, no new subagent.

Required warning:
- Continued under the standing rule: `/Users/fer/dev/opena8dj` and
  `/Users/fer/dev/audio8djrust` are read-only; only
  `/Users/fer/dev/audio8djcpp` is writable; no hardware/audio/CoreAudio/USB
  action without lock and window authorization.

Mission:
- Test whether the opt-in USB-clock-anchor candidate improves timing/quality
  enough to replace or challenge the default diagnostic RC.

Findings:
- Initial USB-clock physical A/B was blocked by packaging: JSON evidence inside
  the `.driver` bundle made codesign fail with `unsealed contents present in
  the bundle root`.
- Moving candidate JSON to `build/hal-candidates/` fixed safety loading.
- Same-session A/B then rejected USB-clock: it was worse than mainline C on
  quality alignment, SNR floor, residuals, lag jumps, driver CPU p95, and
  CoreAudio CPU p95.
- Immediate default restore can show a transient `coreaudiod` spike; a `--wait
  20` restore passed and left the default RC active.

Integrated action:
- Candidate builders now reject JSON output inside candidate bundles.
- Makefile targets write JSON under `build/hal-candidates/`.
- Documented USB-clock as rejected for now.

Risks:
- The root timing-quality problem is still unsolved.
- The prepared-lite candidate remains untested physically.
- Product readiness, Timecode Vinyl certification, CPU superiority, and branch
  promotion remain blocked.

Next action:
- Keep the default diagnostic RC active for human baseline.
- If continuing optimization, prefer prepared-lite only if CPU/resource
  pressure is the bottleneck; otherwise return to measured timing/root-cause
  analysis before creating another timing candidate.

## 2026-06-18 Architect Continuation: Physical Stream Summary Wiring

Subagent:
- Physical Evidence Wiring Auditor.

Required warning:
- The subagent received: "PROHIBIDO tocar, editar, formatear, generar
  archivos, limpiar, resetear, instalar o mutar cualquier cosa en
  /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son
  READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar
  hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana."

Mission:
- Explain why latest same-session comparisons had
  `stream_summary_present=false` despite `stream-stats-during.tsv` existing.

Findings:
- `run-soundcheck` produced `stream-stats-during.tsv` and
  `transfer-ledger-after.tsv`.
- `physical_run_compare.cpp` reads `stream-stats-summary.json`, not the raw TSV.
- `run-physical-superiority-window` ran the comparator without generating that
  summary JSON.

Integrated action:
- `run-physical-superiority-window` now generates `stream-stats-summary.json`
  and `transfer-ledger-analysis.json` after each soundcheck.
- `opena8djcpp_physical_submit_comparison_contract`,
  `run-cpp-offline-gates`, and `evidence_schema_check` now require this wiring.

Risks:
- This is attribution/tooling, not an audio fix.
- The next physical A/B can now compare transport/submit cadence correctly, but
  quality, CPU superiority, Timecode Vinyl, and branch promotion remain blocked.

Next action:
- Use the updated physical window runner for the next candidate; do not accept
  CPU/performance claims from windows missing `stream-stats-summary.json`.

## 2026-06-18 Explorer: Playback Scheduler HAL Insertion Point

Subagent:
- James.

Required warning:
- The subagent received: "PROHIBIDO tocar, editar, formatear, generar
  archivos, limpiar, resetear, instalar o mutar cualquier cosa en
  /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son
  READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar
  hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana."

Mission:
- Inspect `/Users/fer/dev/audio8djcpp` read-only and identify the safest
  insertion point for an opt-in playback scheduler runtime path without
  changing defaults or touching hardware.

Findings:
- The stable default remains guarded by `HAL_PREPARED_USB_SUBMIT_RUNTIME ?= 0`.
- The safest HAL hook is not `submitPlaybackTransfer`; that is already the
  submit/observability boundary. The safer future hook is the capture-paced
  playback refill path before `queuePlaybackWithRequests`, with all real submit
  work still flowing through the existing prepared-runtime bridge.
- A new scheduler path must stay default-off, preserve capture ISO8 continuity,
  keep completion ownership unchanged, and block all claims until lock-gated
  physical A/B.

Integrated action:
- Added a pure C++ runtime-binding model before touching HAL. It joins the
  playback lead scheduler to a preallocated request pool and proves capture
  stays single-period while playback batches to `33` runtime submits for `264`
  logical slots.

Risks:
- This is still not installed or physically measured.
- The next implementation step must expose the binding as an opt-in HAL
  candidate only, not as the stable default.

Next action:
- Implement `HAL_PLAYBACK_LEAD_SCHEDULER` or equivalent default-off HAL binding
  around capture-paced playback refill, then run lock-gated source-reference
  A/B before any CPU or quality claim.

## 2026-06-18 Explorer: DriverKit Prepared Transport Gap

Subagent:
- Chandrasekhar.

Required warning:
- The subagent received: "PROHIBIDO tocar, editar, formatear, generar
  archivos, limpiar, resetear, instalar o mutar cualquier cosa en
  /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son
  READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar
  hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana."

Mission:
- Inspect DriverKit/prepared transport and identify what exists, what is
  missing for an executable DriverKit candidate, and the lowest-risk next
  transport change.

Findings:
- Prepared USB submit planner, request pool, async runtime, and DriverKit
  skeleton contracts already model 8:1 submit reduction offline.
- The real DriverKit extension is still scaffold: no real `IOUserAudioDevice`
  stream binding, no `IOMemoryDescriptor`/timestamp implementation, and no
  USBDriverKit endpoint/request binding.
- The existing gap gate still blocks real DriverKit runtime readiness.

Integrated action:
- Updated transport direction so physical CPU work no longer points at
  repeating the rejected playback-scheduler HAL candidate.
- Kept DriverKit/USB runtime as the strategic path for reducing true
  IOUSBHost enqueue overhead, after an offline adapter/probe closes the
  endpoint binding gap.

Risks:
- DriverKit remains non-executable as a physical candidate until the SDK build
  and USBDriverKit endpoint binding exist.

Next action:
- Implement an offline DriverKit USB submit backend/adapter that maps one
  prepared descriptor to one async USB request with completion-owned lifecycle,
  then run a build-only DriverKit SDK probe before any DriverKit install or
  activation.

## 2026-06-18 Explorer: HAL USB Enqueue Hot Path

Subagent:
- Helmholtz.

Required warning:
- The subagent received: "PROHIBIDO tocar, editar, formatear, generar
  archivos, limpiar, resetear, instalar o mutar cualquier cosa en
  /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son
  READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar
  hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana."

Mission:
- Inspect current HAL/USB runtime and identify a low-risk default-off patch
  that improves performance evidence without repeating rejected knobs.

Findings:
- Hot path is `handleCaptureTransfer` -> `queueCaptureTransfer` ->
  `submitCaptureTransfer` -> `IOUSBHostPipe enqueueIORequestWithData`, with
  playback enqueue as the secondary path.
- Existing rejected knobs should not be repeated without a new implementation:
  stats-off, playback coalescing alone, output-only/no-capture, reused/raw
  completion handlers, input decode batching, and capture batching.
- Current hot-path timing measured playback enqueue but only broad capture
  requeue, not capture enqueue itself.

Integrated action:
- Added default-off `hotPathCaptureEnqueue*` observability through HAL IPC,
  control output, soundcheck TSV, stream-stats analyzer, physical comparator
  detection, and the transport runtime gate.

Risks:
- This improves attribution only. It does not reduce CPU by itself and does
  not justify installing the rebuilt HAL.

Next action:
- Use the new fields in a dedicated hot-timing diagnostic window only after a
  new transport candidate or explicit diagnostic window is justified.

## 2026-06-18 Dewey: CPU Completion-Cost Review

- Mission:
  - Read-only analysis of why C++ CPU remains high even though capture submits
    are lower than mainline.
  - Warning given: PROHIBIDO tocar, editar, formatear, generar archivos,
    limpiar, resetear, instalar o mutar cualquier cosa en
    `/Users/fer/dev/opena8dj` o `/Users/fer/dev/audio8djrust`; solo escribir en
    `/Users/fer/dev/audio8djcpp`; no tocar hardware/audio/CoreAudio/USB sin
    lock global y ventana autorizada.
- Findings:
  - Lower submit count does not prove lower CPU because the C++ completion path
    still does decode, validation, capture requeue, playback queue/fill, stream
    stats, timeline access, and Objective-C/IOUSBHost enqueue work.
  - Current physical CPU attribution is too coarse when hot-path timing samples
    are absent.
  - Do not repeat already rejected knobs: USB-clock/zero timestamp, large
    capture batching, playback coalescing as a CPU shortcut, atomic stream
    stats as default, or reused completions as readiness evidence.
- Files affected by architect after handoff:
  - `scripts/build-hal-hotpath-diagnostic-candidate`
  - `scripts/run-cpp-offline-gates`
  - `tools/evidence_schema_check.cpp`
  - `Makefile`
  - docs describing the diagnostic-only candidate.
- Next action recommended:
  - Use the hot-path diagnostic bundle only in a lock-gated diagnostic window to
    collect nonzero timing attribution before choosing another CPU candidate.

## 2026-06-18 Hubble: Physical Quality/Capture Review

- Mission:
  - Read-only analysis of the latest physical quality/capture failure.
  - Same safety warning as above.
- Findings:
  - The latest physical quality failure points more strongly at post-USB
    route/capture contamination than at payload digital corruption.
  - Direct USB evidence remains clean while physical capture fails; the route
    gate classifies the current shared route as unhealthy.
  - CPU/scheduling remains a blocker, but the latest evidence does not prove it
    is the primary cause of the observed physical residual/noise.
- Files affected:
  - None by the subagent.
- Next action recommended:
  - Revalidate the physical route under lock before using any human/product
    listening result for promotion. Treat current route-based audio quality
    measurements as diagnostic unless the route is revalidated.

## 2026-06-18 Euler: Persistent USB Slot Runtime Map

- Mission:
  - Read-only inspection of current transport/gate code after prepared-lite was
    physically rejected.
  - Required warning given: PROHIBIDO tocar, editar, formatear, generar
    archivos, limpiar, resetear, instalar o mutar cualquier cosa en
    `/Users/fer/dev/opena8dj` o `/Users/fer/dev/audio8djrust`; esos worktrees
    son READ ONLY; solo escribir en `/Users/fer/dev/audio8djcpp`; no tocar
    hardware/audio/CoreAudio/USB sin lock global y ventana autorizada.
- Findings:
  - The best next core boundary is beside `PreparedUsbAsyncRuntime`, not in
    `OpenA8DJUSB.m` or DriverKit runtime first.
  - Existing `UsbSubmitDescriptor` and `PreparedUsbRequestPool` should be
    reused, but the next model must prove persistent request/slot identity,
    stable per-direction arrays, complete-owned lifecycle, zero fallback
    allocation, stale completion rejection, and bounded live requests.
  - Reducing submit count without slot identity and quality/CPU proof risks
    repeating prepared-lite.
- Files affected by architect after handoff:
  - `core/include/opena8djcpp/persistent_usb_transport.hpp`
  - `core/src/persistent_usb_transport.cpp`
  - `tools/persistent_usb_transport_contract.cpp`
  - `CMakeLists.txt`
  - `scripts/run-cpp-offline-gates`
  - `tools/transport_budget_model.cpp`
  - `tools/evidence_schema_check.cpp`
  - Docs and evidence entries.
- Next action recommended:
  - Bind the persistent transport model to an opt-in HAL/DriverKit candidate
    only after the offline contract remains clean post-commit; then run a
    lock-gated source-reference A/B before any product claim.
## 2026-06-18 - Human Rejection Recovery Subagents

Shared safety prompt used for both subagents:

> PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear,
> instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o
> /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes
> escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB
> sin lock global y sin autorización de ventana.

Subagent: output/packing explorer.

- Mission: inspect HAL output path after the human report of idle CPU noise and
  metallic playback.
- Findings:
  - Offline packet tests share the same `start4/check8` premise as the HAL, so
    they can pass while the physical device rejects the phase audibly.
  - Last-frame replay can hide underruns from counters while causing audible
    stale output in gaps.
  - Big-endian 24-bit output remains the best-supported path; native/little
    endian was already physically rejected.
  - `NaN/Inf` should be converted to silence before quantization.
- Integrated actions:
  - Added `OPENA8DJ_STRICT_IDLE_SILENCE`.
  - Added `isfinite(sample)` guard in `FloatToOutputI24`.
  - Built a separate `start2` diagnostic candidate but did not load it first.

Subagent: input/Traktor explorer.

- Mission: inspect HAL/CoreAudio input path after Traktor Scratch Control
  showed no signal on A/B/C/D.
- Findings:
  - The known Traktor-compatible baseline is one 8-channel input stream and
    four stereo output streams.
  - Input should not be changed to four stereo streams without evidence.
  - `inputDecodeEnabled=false` or an unapplied hardware profile would make
    `ReadInput` deliver zeros.
  - Real runtime snapshots are needed to distinguish "Traktor did not ask" from
    "HAL returned zeros" from "USB decode has no data."
- Integrated actions:
  - Built `OpenA8DJ-traktor-recovery-streamusage0.driver` with input `1x8`,
    output `4x2`, strict idle silence, and touched-output flushing.
  - Applied and verified `timecode-vinyl`: input mode `0`, software lock on,
    input decode on, identity source map.
  - Captured nonzero input stats on A/B/C/D before human retest.
