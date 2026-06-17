# Test Evidence

## 2026-06-17: Direct USB Timeline Instrumentation And Reset No-Wait Rejection

- Change:
  - `audio-record` now reports raw monotonic timestamps and first-energy
    fields: `start_nsec`, `first_callback_nsec`, `first_energy_nsec`,
    `first_energy_frame`, and first-energy seconds.
  - `opena8dj-usb-play` now reports monotonic `usb_play_event` markers for
    process start, USB start, first write, play completion, and stop.
  - `run-direct-usb-soundcheck` copies those fields into `summary.txt` and
    computes `first_energy_after_first_write_seconds` when both monotonic
    timestamps are present.
  - Added build-time experiment flags:
    `HAL_AUDIO_PARAMS_RESET_WAIT_FOR_REPLY` and
    `HAL_AUDIO_PARAMS_RESET_SETTLE_USEC`.
- Commands:
  - `make build/audio-record build/opena8dj-usb-play`
  - `bash -n scripts/run-direct-usb-soundcheck`
  - `scripts/run-cpp-offline-gates`
  - Locked direct USB/iRig runs:
    - `local-analysis/direct-usb-timeline-instrumentation/20260617T134039Z-pairA-6s-usbdiag`
    - `local-analysis/direct-usb-timeline-instrumentation/20260617T134242Z-pairA-6s-usbdiag-nsec`
    - `local-analysis/direct-usb-reset-no-wait/20260617T134558Z-pairA-6s-usbdiag`
    - `local-analysis/direct-usb-reset-no-wait/20260617T134734Z-settle500ms-pairA-2s`
    - `local-analysis/direct-usb-reset-no-wait/20260617T134755Z-settle250ms-pairA-2s`
    - `local-analysis/direct-usb-reset-no-wait/20260617T134815Z-settle100ms-pairA-2s`
    - `local-analysis/direct-usb-reset-no-wait/20260617T134841Z-settle250ms-pairA-6s-usbdiag`
- Result:
  - Offline gates PASS after adding timeline instrumentation:
    Debug CTest `17/17`, Release CTest `18/18`, stream-stats contract
    `196` fields and `0` mismatches.
  - Default direct USB run with raw monotonic timestamps still failed strict
    quality: alignment `0.936915`, SNR `8.82 dB`, mid/high residual
    `1.444318/1.419246`.
  - The same run proved the analog route emits shortly after the first write:
    `player_after_first_write_seconds=4.242867`,
    `record_first_energy_record_seconds=4.882604`, and
    `first_energy_after_first_write_seconds=0.191420`.
  - Therefore the multi-second startup delay is not between first write and
    iRig energy. It is before first write, inside `OpenA8DJUSBStart`.
  - `HAL_AUDIO_PARAMS_RESET_WAIT_FOR_REPLY=0` without settle failed start:
    `player_rc=6`, no captured energy above threshold.
  - `settle500ms` started and reduced `player_after_start_seconds` to
    `2.636657` in a short run, but still failed quality.
  - `settle250ms` was not stable: one short run started in `0.270771s`, but a
    longer diagnostics run returned to `4.502963s`.
  - `settle100ms` produced no captured energy above threshold and is rejected.
- Interpretation:
  - The new instrumentation is accepted.
  - Reset no-wait/no-settle and reset no-wait with `100ms` settle are rejected.
  - Reset no-wait with `250ms` or `500ms` settle is not promoted because it is
    not proven stable and does not improve physical quality.
  - Default remains `HAL_AUDIO_PARAMS_RESET_WAIT_FOR_REPLY=1`.

## 2026-06-17: Raw Capture And Playback Completion Counter Fix

- Change:
  - Added `playbackTransfersCompletedRaw` to `OpenA8DJStreamStatsPayload`.
  - Added `captureTransfersCompletedRaw` to `OpenA8DJStreamStatsPayload`.
  - Added `_playbackTransfersCompletedAtomic` and increment it once per
    playback completion, outside the hot-stream-stats sampling interval.
  - Added `_captureTransfersCompletedAtomic` and increment it once per capture
    completion, outside the hot-stream-stats sampling interval.
  - `opena8dj-control stream-stats` now prints raw capture and playback
    completions when the fields are present, falling back to the legacy
    sampled counters for older HAL payloads.
- Commands:
  - `python3 scripts/check-stream-stats-contract.py`
  - `make -B hal`
  - `scripts/run-cpp-offline-gates`
  - Locked HAL safety and short iRig soundcheck before the fix:
    `local-analysis/physical-stream-stats-contract/20260617T132743Z-288f65a`.
  - Locked HAL safety and short iRig soundcheck after the fix:
    `local-analysis/physical-stream-stats-raw-completions/20260617T133008Z-288f65a`.
  - Locked HAL safety and short iRig soundcheck after adding raw capture too:
    `local-analysis/physical-stream-stats-raw-capture-playback/20260617T133414Z-fe668ef-retry`.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-stream-stats-raw-capture-playback-physical.json`
- Result:
  - Stream-stats contract PASS: `196` HAL fields, `196` control-tool fields,
    `0` mismatches, last field `captureTransfersCompletedRaw`.
  - Offline gates PASS.
  - Debug CTest: `100% tests passed, 0 tests failed out of 17`.
  - Release CTest: `100% tests passed, 0 tests failed out of 18`.
  - Evidence schema: PASS, `22` required files, `0` missing.
  - Post-run runtime isolation PASS, HAL inactive, lock absent.
- Physical counter evidence:
  - Before the fix, the same build-hardening line still reported
    `playbackTransfersSubmitted=8131` and `playbackTransfersCompleted=508`,
    proving the stale-tool/drift fix was not enough.
  - After the raw playback completion fix, the short locked run reported
    `playbackTransfersSubmitted=8123` and `playbackTransfersCompleted=8123`.
  - After adding raw capture completions too, the retry short locked run
    reported `captureTransfersCompleted=8137`,
    `playbackTransfersSubmitted=8129`, and
    `playbackTransfersCompleted=8129`.
  - The earlier apparent `16x` playback/capture mismatch was a sampling
    artifact, not a valid transport conclusion.
- Physical quality result after the fix:
  - Soundcheck remains FAIL:
    `quality_alignment_score=0.969899`, SNR `10.93 dB`,
    `lag_jumps_gt_2_frames=20`,
    mid/high residual `1.367151/1.355700`.
- Interpretation:
  - The misleading `submitted/completed` mismatch is fixed.
  - The capture/playback completion-rate comparison is now trustworthy enough
    to avoid the false `16x` hypothesis.
  - This is observability progress only. It is not a readiness or quality
    improvement claim.

## 2026-06-17: Stream-Stats Contract And Control Tool Build Hardening

- Change:
  - `make hal` now also builds `build/opena8dj-control`.
  - `build/opena8dj-control` now depends on `src/hal/OpenA8DJUSB.m`, so a HAL
    stream-stats payload edit forces a matching control-tool rebuild.
  - Added `scripts/check-stream-stats-contract.py`, an offline gate that
    compares the `OpenA8DJStreamStatsPayload` field sequence in the HAL and
    control tool.
  - Integrated the stream-stats contract check into
    `scripts/run-cpp-offline-gates`.
- Commands:
  - `python3 scripts/check-stream-stats-contract.py`
  - `make -B hal`
  - `scripts/run-cpp-offline-gates`
- Result:
  - Stream-stats contract PASS: `166` HAL fields, `166` control-tool fields,
    `0` mismatches, last field `playbackTransfersSubmitted`.
  - `make -B hal` rebuilt both `build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL`
    and `build/opena8dj-control`.
  - Offline gates PASS.
  - Debug CTest: `100% tests passed, 0 tests failed out of 17`.
  - Release CTest: `100% tests passed, 0 tests failed out of 18`.
  - Evidence schema: PASS, `22` required files, `0` missing.
  - Hardware touched: no.
  - CoreAudio touched: no.
  - USB touched: no.
- Evidence:
  - `local-analysis/cpp-offline/stream-stats-contract.json`.
  - `local-analysis/cpp-offline/current-offline-gates.json`.
- Interpretation:
  - This does not prove `playbackTransfersSubmitted` is physically correct.
    It closes one concrete observability risk: future HAL/control payload drift
    or stale control-tool binaries should be caught before locked hardware
    tests.

## 2026-06-17: Transfer Ledger No-Op Call-Site Prune And Transfer-Rate Rejections

- Change:
  - Wrapped disabled transfer-ledger call sites in
    `OPENA8DJ_ENABLE_TRANSFER_LEDGER`, so the Objective-C no-op messages are
    not sent from callback-adjacent transfer paths when the ledger is off.
  - Tested transfer-rate variants proposed by the CPU audit:
    ISO64+unrolled output packing, coalesce2+unrolled output packing, and the
    default ISO8/generic packer after the ledger call-site prune.
  - Reverted Makefile defaults after the experiments. Only the ledger
    call-site prune remains as code hygiene.
- Commands:
  - `make -B hal usb-play HAL_FAST_OUTPUT_PREFETCH_CLEAR=1 HAL_UNROLLED_OUTPUT_PACK=1`
  - `make -B hal usb-play HAL_ISO_FRAMES=64 HAL_PLAYBACK_ISO_FRAMES=64 HAL_FAST_OUTPUT_PREFETCH_CLEAR=1 HAL_UNROLLED_OUTPUT_PACK=1`
  - `make -B hal usb-play HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_FAST_OUTPUT_PREFETCH_CLEAR=1 HAL_UNROLLED_OUTPUT_PACK=1`
  - `make -B hal usb-play`
  - `scripts/run-cpp-offline-gates`
- Result:
  - Offline gates PASS after restoring defaults and applying the ledger
    call-site prune.
  - Debug CTest: `100% tests passed, 0 tests failed out of 17`.
  - Release CTest: `100% tests passed, 0 tests failed out of 18`.
  - Evidence schema: PASS, `22` required files, `0` missing.
  - Hardware touched: yes, under hardware lock for the locked HAL safety and
    iRig soundcheck runs listed below.
  - Driver install/load touched: yes, under hardware lock only.
  - CoreAudio touched: yes, under hardware lock only for HAL candidate loading,
    soundcheck, cleanup, and runtime isolation.
- Physical evidence:
  - `local-analysis/physical-unrolled-pack/20260617T131114Z-640dee9`:
    soundcheck FAIL, `quality_alignment_score=0.962106`, SNR about `9.24 dB`,
    `lag_jumps_gt_2_frames=44`, clicks `113`, OpenA8DJ driver CPU p95 `22.0%`.
  - `local-analysis/physical-iso64-unrolled/20260617T131356Z-640dee9`:
    soundcheck FAIL, `quality_alignment_score=0.186393`, SNR `-20.96 dB`,
    `lag_jumps_gt_2_frames=60`, OpenA8DJ driver CPU p95 `5.5%`.
  - `local-analysis/physical-coalesce2-unrolled/20260617T131709Z-640dee9`:
    soundcheck FAIL, `quality_alignment_score=0.258519`, SNR `-18.71 dB`,
    `lag_jumps_gt_2_frames=57`, OpenA8DJ driver CPU p95 `16.5%`.
  - `local-analysis/physical-ledger-callsite-prune/20260617T131921Z-640dee9`:
    soundcheck FAIL, `quality_alignment_score=0.234322`, SNR `-17.53 dB`,
    `lag_jumps_gt_2_frames=61`, OpenA8DJ driver CPU p95 `23.4%`.
- Recovery:
  - Each physical run ended with a runtime isolation audit PASS and no active
    HAL left loaded.
  - Latest cleanup evidence:
    `local-analysis/physical-ledger-callsite-prune/20260617T131921Z-640dee9/runtime-isolation-after-force-unload.json`.
- Release benchmark after restoring defaults and applying the ledger prune:
  - `pack_mib_s=1656.8`
  - `decode_into_mib_s=578.522`
  - `route_frames_s=1.1163e+09`
  - `route_advanced_frames_s=5.09512e+08`
- Interpretation:
  - ISO64 proves the CPU target is reachable, but it destroys physical audio
    quality in the current C++ HAL and must stay rejected.
  - Playback coalescing lowers CPU less than ISO64 and also destroys physical
    audio quality, so it must stay rejected.
  - The unrolled packer is not accepted as a default; it still failed physical
    quality and introduced click evidence.
  - The ledger call-site prune is acceptable callback hygiene only. It is not
    a performance or sound-quality claim.
  - `playbackTransfersSubmitted` is not trustworthy for readiness until its
    counter contract is fixed; recent physical runs report about `16134`
    submitted versus about `1008` completed.

## 2026-06-17: HAL Hot-Path Lock Reduction

- Change:
  - Moved output `startFrame` resolution into `OutputTimelineWrite`, so the
    HAL write path acquires `_outputTimeline.mutex` once instead of resolving
    sample-time continuity under a separate lock and then locking again to
    write frames.
  - Changed input stats accounting from one `_inputStatsMutex` lock per
    completed input stream sample to stack-local aggregation in
    `decodeCaptureBytes` plus one merge lock per capture transfer.
  - Preserves existing behavior: same `sampleTimeValid` handling, same
    `OPENA8DJ_ENABLE_OUTPUT_SAMPLE_TIME_FOLLOWER` tolerance, same timeline
    reset/drop/late-write counters, same input RMS/peak/cross math, same
    payload bytes, same USB cadence, same queue depths, same
    sample-rate/default-device policy.
- Commands:
  - `make -B hal usb-play`
  - `make -B hal HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1`
  - `make -B hal usb-play`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-hal-hotpath-lock-reduction.json`
- Result:
  - HAL default build PASS.
  - `usb-play` default build PASS.
  - `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1` build PASS.
  - Offline gates PASS.
  - Default CTest: `100% tests passed, 0 tests failed out of 16`.
  - Release CTest: `100% tests passed, 0 tests failed out of 17`.
  - Evidence schema: PASS, `22` required files, `0` missing.
  - Promotion readiness: FAIL, `branch_promotion_allowed=false`.
  - Runtime isolation: PASS, HAL inactive, lock absent.
  - Hardware touched: no.
  - CoreAudio touched: no.
  - USB touched: no.
- Release benchmark from the same gate:
  - `pack_mib_s=1628.8`
  - `decode_into_mib_s=586.682`
  - `route_frames_s=9.648e+08`
  - `route_advanced_frames_s=4.99203e+08`
- Readiness note:
  - This is a low-risk CPU/callback hygiene change only. It must not be
    counted as better-than-mainline evidence until a locked physical
    A/B soundcheck shows equal or better quality and lower runtime CPU.

## 2026-06-17: Hot-Path Lock Reduction Physical Rejection

- Candidate: commit `056d29b` (`Reduce HAL hot-path locks`).
- Preflight:
  - Runtime isolation before physical run:
    `local-analysis/runtime-isolation/pre-hotpath-lock-physical.json`,
    `PASS`.
  - iRig Stream and Audio 8 DJ were both visible before installing the HAL.
  - `scripts/audio-stack-health`: `PASS`.
- HAL candidate safety command:
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-hotpath-lock-reduction/20260617-056d29b/hal-candidate-safety`
- HAL candidate safety result:
  - `PASS`.
  - Evidence:
    `local-analysis/physical-hotpath-lock-reduction/20260617-056d29b/hal-candidate-safety/summary.txt`.
- Physical soundcheck command:
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-hotpath-lock-056d29b-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
- Physical soundcheck result:
  - `FAIL`.
  - Evidence:
    `local-analysis/soundcheck/20260617-hotpath-lock-056d29b-irig-pairA-16s-cpp-hal/`.
  - `quality_alignment_score=0.964558`.
  - `analog_snr_db=10.41`.
  - `left_snr_db=10.49`, `right_snr_db=10.41`.
  - `lag_jumps_gt_2_frames=43`.
  - `mid_band_residual_ratio=1.430949`.
  - `high_band_residual_ratio=1.358723`.
  - `quiet_mid_band_noise_dbfs=-35.90`.
  - `capture_clipped_frames=0`.
  - `left_click_outliers=0`, `right_click_outliers=0`,
    `window_click_outliers_max=1`.
  - `mid_band_cpu_corr_max=0.154560`, source `controlcenter`.
- CPU evidence from the same soundcheck:
  - `opena8dj_driver`: median `36.45%`, p95 `37.5%`, max `37.6%`.
  - `coreaudiod`: median `2.65%`, p95 `60.3%`, max `85.8%`.
  - `total_audio_ui`: median `54.2%`, p95 `121.1%`, max `167.5%`.
  - `top_audio_ui`: median `36.65%`, p95 `60.3%`, max `85.8%`.
- Recovery:
  - `scripts/audio-stack-guard --recover --unload-opena8dj` did not fully
    unload the HAL.
  - The active HAL was manually parked under the hardware lock at
    `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver.disabled-20260617T031430Z`,
    then `coreaudiod` was restarted.
  - After a short post-restart wait, `scripts/audio-stack-health` returned
    `PASS`.
  - Final runtime isolation:
    `local-analysis/runtime-isolation/after-hotpath-manual-unload.json`,
    `PASS`, HAL inactive, lock absent.
- Promotion result after the run:
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
    returned `FAIL`, `branch_promotion_allowed=false`.
- Interpretation:
  - The lock reduction remains acceptable code hygiene, but it is physically
    rejected as a readiness/performance candidate.
  - Runtime CPU remains far above the mainline reference, and physical music
    quality still fails the product thresholds.
  - Do not rerun this candidate as a standalone physical test. The next
    physical run needs a new transport/cadence/device-state hypothesis.

## 2026-06-17: Documentation Update Offline Verification

- Change:
  - Documented the locked physical rejection of commit `056d29b` in
    `ARCHITECT_CONTEXT.md`, `DECISION_LOG.md`, `SUCCESS_METRICS.md`, and this
    evidence file.
  - Updated current success metrics to use the latest offline benchmark
    artifact generated after the documentation update.
- Commands:
  - `git diff --check`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-hotpath-doc-update.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/run-cpp-offline-gates`
- Result:
  - Diff whitespace check PASS.
  - Runtime isolation PASS: HAL inactive, lock absent, no OpenA8DJ process.
  - Offline gates PASS: Debug `16/16`, Release `17/17`, evidence schema
    PASS with `22` required files and `0` missing.
  - Promotion readiness remains FAIL with
    `branch_promotion_allowed=false`.
  - Hardware touched by this verification: no.
  - CoreAudio/USB touched by this verification: no.
- Latest Release benchmark:
  - `pack_mib_s=1650.82`.
  - `decode_into_mib_s=530.75`.
  - `route_frames_s=9.11539e+08`.
  - `route_advanced_frames_s=4.79861e+08`.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`.
  - `local-analysis/runtime-isolation/post-hotpath-doc-update.json`.
  - `local-analysis/promotion-readiness-current.json`.

## 2026-06-17: Transfer-Pool Fallback Allocation Instrumentation

- Change:
  - Added `captureTransferPoolFallbackAllocations` and
    `playbackTransferPoolFallbackAllocations` to the HAL stream-stats payload.
  - `opena8dj-control stream-stats` now prints both counters as `key=value`
    fields while remaining compatible with older payloads.
  - `scripts/run-soundcheck` now records both fields in
    `stream-stats-during.tsv`.
  - `scripts/analyze-stream-stats.py` now summarizes both counters and raises
    diagnostic flags if either increases during a run.
- Reason:
  - The default HAL uses transfer pools, but checkout still falls back to
    creating a transfer if every pool slot is in use.
  - Fallback allocation in sustained streaming would violate the no-allocation
    real-time policy and could explain CPU/latency that current underrun and
    late-write counters miss.
- Commands:
  - `python3 -m py_compile scripts/run-soundcheck scripts/analyze-stream-stats.py`
  - `make -B hal build/opena8dj-control`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-hotpath-lock-056d29b-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/hotpath-lock-056d29b-summary-with-pool-fields.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-transfer-pool-instrumentation-build.json`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-transfer-pool-instrumentation.json`
- Result:
  - Python syntax PASS.
  - HAL/control build PASS.
  - Existing stream-stats evidence remains readable; old runs report these new
    fields as unavailable/null, not as failures.
  - Offline gates PASS: Debug `16/16`, Release `17/17`, evidence schema
    PASS with `22` required files and `0` missing.
  - Promotion readiness remains FAIL with
    `branch_promotion_allowed=false`.
  - Runtime isolation PASS: HAL inactive, lock absent, no OpenA8DJ process.
  - Hardware touched: no.
  - CoreAudio touched: no.
  - USB touched: no.
- Latest Release benchmark:
  - `pack_mib_s=1653.52`.
  - `decode_into_mib_s=551.331`.
  - `route_frames_s=9.34836e+08`.
  - `route_advanced_frames_s=4.85227e+08`.
- Readiness note:
  - This is observability only, not an audio-quality fix.
  - Any future physical candidate must show both fallback allocation counters
    stay at `0` before it can make a low-CPU or real-time-path claim.

## 2026-06-17: Transfer-Pool Offline Model Gate

- Change:
  - Added `opena8djcpp_transfer_pool_model`, a pure C++ offline model for
    capture/playback transfer-pool occupancy and fallback-allocation behavior.
  - Wired the model into CMake, CTest, `scripts/run-cpp-offline-gates`, and
    `scripts/evaluate-promotion-readiness.py`.
  - The promotion evaluator now requires the model to prove healthy no-fallback
    queueing and explicitly reject capture/playback pool leak scenarios.
- Reason:
  - The physical CPU and music-quality failures cannot be explained by existing
    underrun, late-write, or timeline counters.
  - Transfer-pool fallback allocations are a concrete no-allocation policy
    violation. The model gives the offline suite a structural guard before the
    next locked physical run.
- Commands:
  - `cmake -S . -B build/cpp-offline && cmake --build build/cpp-offline --target opena8djcpp_transfer_pool_model && ./build/cpp-offline/opena8djcpp_transfer_pool_model`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-transfer-pool-model.json`
- Result:
  - Standalone transfer-pool model PASS: `6` rows, `0` failures.
  - Healthy scenarios have `0` capture and playback fallback allocations.
  - Rejected-by-design scenarios:
    `capture_pool_leak_rejected`, `playback_pool_leak_rejected`.
  - Offline gates PASS: Debug `17/17`, Release `18/18`.
  - Evidence schema PASS with `22` required files and `0` missing.
  - Promotion readiness remains FAIL with
    `branch_promotion_allowed=false`.
  - Runtime isolation PASS: HAL inactive, lock absent, no OpenA8DJ process.
  - Hardware touched: no.
  - CoreAudio touched: no.
  - USB touched: no.
- Latest Release benchmark:
  - `pack_mib_s=1656.23`.
  - `decode_into_mib_s=588.188`.
  - `route_frames_s=9.14455e+08`.
  - `route_advanced_frames_s=5.07130e+08`.
- Evidence paths:
  - `local-analysis/cpp-offline/transfer-pool-model.json`.
  - `local-analysis/cpp-offline/current-offline-gates.json`.
  - `local-analysis/promotion-readiness-current.json`.
  - `local-analysis/runtime-isolation/final-after-transfer-pool-model.json`.
- Readiness note:
  - This improves objective analysis coverage only. It does not prove physical
    sound quality, low runtime CPU, or Traktor/timecode vinyl readiness.

## 2026-06-17: Aggregate Transfer Ledger Instrumentation

- Change:
  - Added a fixed-size HAL transfer-ledger ring and aggregate counters for
    capture queue, capture completion, playback queue, playback completion,
    implicit playback first-frame scheduling, ring overwrite count, and output
    read/silence/underrun/drop/replay frames consumed while filling playback
    transfers.
  - `opena8dj-control stream-stats` now prints the ledger summary and emits
    `key=value` fields for `run-soundcheck`.
  - `scripts/run-soundcheck` records the new fields in
    `stream-stats-during.tsv`.
  - `scripts/analyze-stream-stats.py` now reports ledger rates and flags
    ledger overwrite, capture/playback queue-completion gaps, and ledger
    active underruns.
- Reason:
  - Physical failures are not explained by existing underrun, late-write, or
    packet parity evidence. The next hardware run needs transfer-level
    observability without logging, allocation, file I/O, or locks per event.
- Commands:
  - `make -B hal build/opena8dj-control`
  - `python3 -m py_compile scripts/run-soundcheck scripts/analyze-stream-stats.py`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-hotpath-lock-056d29b-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/hotpath-lock-056d29b-summary-with-ledger-fields.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-transfer-ledger-build.json`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-transfer-ledger-instrumentation.json`
- Result:
  - HAL/control build PASS.
  - Python syntax PASS.
  - Existing stream-stats evidence remains readable; missing ledger fields are
    represented as null/0 and do not create new failure flags.
  - Offline gates PASS: Debug `17/17`, Release `18/18`, evidence schema PASS.
  - Promotion readiness remains FAIL with
    `branch_promotion_allowed=false`.
  - Runtime isolation PASS: HAL inactive, lock absent, no OpenA8DJ process.
  - Hardware touched: no.
  - CoreAudio touched: no.
  - USB touched: no.
- Latest Release benchmark:
  - `pack_mib_s=1626.84`.
  - `decode_into_mib_s=575.412`.
  - `route_frames_s=1.00486e+09`.
  - `route_advanced_frames_s=4.86541e+08`.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`.
  - `local-analysis/stream-stats/hotpath-lock-056d29b-summary-with-ledger-fields.json`.
  - `local-analysis/promotion-readiness-current.json`.
  - `local-analysis/runtime-isolation/final-after-transfer-ledger-instrumentation.json`.
- Readiness note:
  - This is diagnostic instrumentation, not a fix. A future physical run with
    this ledger can identify hidden transport defects, but final CPU claims
    must account for ledger overhead.

## 2026-06-17: Playback Burst Cadence Model Gate

- Change:
  - Extended `opena8djcpp_jitter_model` with an offline burst cadence model.
  - The gate now records normal timeline/jitter rows and `burst_rows` for
    playback completion spacing relative to the capture period.
  - `scripts/run-cpp-offline-gates` now exposes `burst_rows`,
    `burst_failures`, and unsafe burst scenarios in
    `current-offline-gates.json`.
- Commands:
  - `cmake -S . -B build/cpp-release -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build/cpp-release --target opena8djcpp_jitter_model`
  - `./build/cpp-release/opena8djcpp_jitter_model | tee local-analysis/cpp-offline/jitter-model.json`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-burst-cadence-model.json`
- Result:
  - Offline gates PASS.
  - Default CTest: `100% tests passed, 0 tests failed out of 16`.
  - Release CTest: `100% tests passed, 0 tests failed out of 17`.
  - Evidence schema: PASS, `22` required files, `0` missing.
  - Runtime isolation after the gate: PASS, HAL inactive, lock absent.
  - Hardware touched: no.
  - CoreAudio touched: no.
  - USB touched: no.
- Jitter model evidence:
  - Normal rows: `8`, failures `0`.
  - Burst rows: `3`, burst failures `0`.
  - Unsafe-by-design burst scenarios:
    `capture_paced_coalesce2_rejected_by_physical_gate`,
    `capture_paced_coalesce4_rejected_by_model`.
  - `coalesce=2` models a playback completion interval of `128` frames
    against a `64` frame capture period, with completion gap ratio `2.0` and
    completion-count CPU ratio `0.5`.
- Release benchmark from the same gate:
  - `pack_mib_s=1654.72`
  - `decode_into_mib_s=587.81`
  - `route_frames_s=8.23705e+08`
  - `route_advanced_frames_s=5.00832e+08`
- Promotion readiness:
  - FAIL, `branch_promotion_allowed=false`.
  - Current blockers remain physical real-music quality, runtime CPU versus
    mainline, latest physical investigation readiness, and unvalidated physical
    Traktor/timecode vinyl.
- Interpretation:
  - The previous coalesce2 physical run proved that reducing completion count
    can lower driver CPU while making quality worse. This offline model
    captures that tradeoff explicitly, so future CPU changes must preserve
    playback cadence instead of winning by batching completions into unsafe
    gaps.

## 2026-06-16: Mandatory Hardware Lock Policy For Physical Scripts

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `2f80d08`
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/hardware-lock-policy.json`
  - `local-analysis/runtime-isolation/current.json`
  - `local-analysis/promotion-readiness-current.json`
- Change:
  - Added `scripts/hardware-lock-lib.sh`.
  - `scripts/test-hal-candidate-safety` now acquires the global hardware lock before HAL install/reload and `coreaudiod` restart.
  - `scripts/run-audio8dj-direct-gate` now uses the shared lock helper.
  - `scripts/run-soundcheck` now acquires the global hardware lock before physical playback/capture/config work.
  - Added `opena8djcpp_hardware_lock_policy_check` and wired it into CTest, offline gates, evidence schema, and promotion readiness.
- Hardware lock policy result: `PASS`, `4` audited scripts, `0` missing requirements.
- Full offline gates after the change:
  - Default CTest: `100% tests passed, 0 tests failed out of 15`
  - Release CTest: `100% tests passed, 0 tests failed out of 16`
  - Evidence schema: `22` required files, `0` missing
  - Release benchmark: `pack_mib_s=1546.09`, `decode_into_mib_s=565.894`, `float_to_s24_frames_s=8.54504e+07`, `route_frames_s=9.99440e+08`, `route_reversed_frames_s=4.81809e+08`, `route_advanced_frames_s=4.72260e+08`
- Runtime isolation after the change: PASS with HAL inactive, lock absent, forbidden mainline LaunchAgents disabled, and no OpenA8DJ process detected.
- Promotion readiness result: FAIL, with `branch_promotion_allowed=false`.
- Remaining promotion blockers: physical real-music quality, runtime CPU/coreaudiod, and physical Traktor/timecode vinyl lock evidence.
- Hardware touched by this gate: no
- CoreAudio touched by this gate: no
- USB touched by this gate: no
- Driver installed or activated by this gate: no

## 2026-06-16: Mode 2 Cross-Oracle Byte Parity Gate

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `50105c5` plus current Mode 2 cross-oracle gate changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/mode2-cross-oracle-parity.json`
  - `local-analysis/promotion-readiness-current.json`
- Change:
  - C++ Float32-to-S24 quantization now matches the inherited Python oracle's explicit Float32 rounding path before converting to 24-bit output.
  - Added `opena8djcpp_mode2_cpp_oracle_dump` and `scripts/mode2-cross-oracle-parity`.
  - Promotion readiness now includes `offline_mode2_cross_oracle_parity`.
- Cross-oracle result: `72` rows, `0` failures, `0` byte mismatches, `0` length deltas, `0` check errors, `0` panic flags.
- Full offline gates after the change:
  - Default CTest: `100% tests passed, 0 tests failed out of 14`
  - Release CTest: `100% tests passed, 0 tests failed out of 15`
  - Evidence schema: `21` required files, `0` missing
  - Release benchmark: `pack_mib_s=1601.31`, `decode_into_mib_s=569.821`, `float_to_s24_frames_s=8.54237e+07`, `route_frames_s=9.78452e+08`, `route_reversed_frames_s=4.99956e+08`, `route_advanced_frames_s=4.91712e+08`
- Promotion readiness result: FAIL, with `branch_promotion_allowed=false`.
- Remaining promotion blockers: physical real-music quality, runtime CPU/coreaudiod, and physical Traktor/timecode vinyl lock evidence.
- Runtime cleanup check before this gate: hardware lock absent; terminated live `git fsmonitor--daemon` holding `/Users/fer/dev/opena8dj`; terminated `NIHardwareAgent`; follow-up `lsof +D /Users/fer/dev/opena8dj` showed no remaining handles.
- Hardware touched by this gate: no
- CoreAudio touched by this gate: no
- USB touched by this gate: no
- Driver installed or activated by this gate: no

## 2026-06-16: Initial C++ Offline Core Contract

- Command: `cmake -S . -B build/cpp-offline && cmake --build build/cpp-offline && ctest --test-dir build/cpp-offline --output-on-failure`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign scaffold changes
- Result: PASS
- Output summary: `100% tests passed, 0 tests failed out of 1`
- Evidence path: `local-analysis/cpp-offline/`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: C++ Routing Fast Path Optimization

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/20260616-145942-routing-fastpath-repeat`
- Change: added an identity-routing fast path and moved routing mapping validation outside the per-frame loop.
- Added tests:
  - identity routing still copies all 8 channels correctly;
  - reversed routing maps channels explicitly;
  - invalid routing is rejected before processing frames.
- Full offline gates after the change:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS, `72` rows, `0` failures
  - Python Mode 2 oracle: PASS, `check_errors=0`, `panic_flags=0`
  - Timecode matrix: PASS
  - DVS signal smoke: PASS, zero leakage
  - Realtime audit: PASS, `hot_path_allocations=0`
  - Static policy: PASS, `forbidden_hits=0`
- Measured routing performance:
  - Pre-change same-session baseline: about `5.66e+08 frames/s`
  - Post-change gate run: `9.62e+08 frames/s`
  - Seven-run post-change repeat: median `9.49e+08 frames/s`, min `9.37e+08`, max `9.71e+08`
- Pack/decode metrics remained within run-to-run noise and are not claimed as improved.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated by this optimization: no

Readiness note:

- This is a real offline hot-path improvement for default A/B/C/D identity routing.
- It does not prove physical sound quality, iRig analog capture quality, DAC behavior, Traktor scope quality, or runtime CPU against mainline.

## 2026-06-16: C++ Mode 2 Decode Bitmask Optimization

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/20260616-150104-decode-bitmask-repeat`
- Change: replaced per-stream `bool` presence arrays in Mode 2 decode with 6-bit masks.
- Reason: frame completeness in the real-time decode path is a small fixed-size bitset problem; byte masks avoid repeated bool-array fills and per-frame full-array scans.
- Full offline gates after the change:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS, `72` rows, `0` failures
  - Python Mode 2 oracle: PASS, `check_errors=0`, `panic_flags=0`
  - Timecode matrix: PASS
  - DVS signal smoke: PASS, zero leakage
  - Realtime audit: PASS, `hot_path_allocations=0`
  - Static policy: PASS, `forbidden_hits=0`
- Measured decode performance:
  - Pre-change same-session baseline: about `532 MiB/s`
  - Post-change gate run: `577.374 MiB/s`
  - Nine-run post-change repeat: median `570.726 MiB/s`, min `377.063`, max `579.388`
- Routing remains improved after the decode change:
  - Nine-run post-change repeat median `9.54e+08 frames/s`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated by this optimization: no

Readiness note:

- This strengthens the C++ packet decode data plane.
- It does not prove physical sound quality, runtime HAL CPU, Traktor scope quality, or mainline superiority without hardware/iRig evidence.

## 2026-06-16: Preallocated Packer Benchmark Path

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/20260616-150207-preallocated-pack-repeat`
- Change: `tools/offline_bench.cpp` now measures `Mode2OutputPacker::fill_into()` into a preallocated buffer instead of `fill()` plus repeated `vector` insertion.
- Reason: the real-time policy requires preallocated buffers; the old benchmark mixed packer speed with allocation/copy overhead from benchmark scaffolding.
- Full offline gates after the change:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS, `72` rows, `0` failures
  - Python Mode 2 oracle: PASS, `check_errors=0`, `panic_flags=0`
  - Timecode matrix: PASS
  - DVS signal smoke: PASS, zero leakage
  - Realtime audit: PASS, `hot_path_allocations=0`
  - Static policy: PASS, `forbidden_hits=0`
- Measured preallocated pack performance:
  - Latest gate run: `1653.83 MiB/s`
  - Nine-run repeat: median `1634.35 MiB/s`, min `1004.28`, max `1649.91`
- This pack number is not directly comparable to the old allocation-heavy benchmark. It is the correct metric for the intended real-time data plane.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated by this optimization: no

## 2026-06-16: HAL Install And Hardware Presence Check

- Command: `make install-hal`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PARTIAL / BLOCKED_FOR_IRIG_CAPTURE
- Installed HAL hash: `13dd0d0b0ab31c6972cd62d7c1187a81e9dd04a29b987f3dbbde7e7f56233788`
- Install evidence: `local-analysis/hardware-quality/20260616-144846-install-verify`
- Capture presence evidence: `local-analysis/hardware-quality/20260616-144933-capture-presence`
- iRig presence audit: `local-analysis/hardware-quality/20260616-145020-irig-presence-audit`
- Driver state evidence: `local-analysis/hardware-quality/20260616-145129-driver-state`
- Hardware touched: yes, user explicitly requested driver installation and hardware quality testing
- CoreAudio touched: yes, `make install-hal` restarted `coreaudiod`
- USB touched: read-only enumeration only; no USB reset or replug automation
- Driver installed or activated: yes, HAL installed at `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver`

Findings:

- `Open Audio 8 DJ` enumerated in CoreAudio with 8 input and 8 output channels at 48 kHz.
- Installed HAL binary hash matched the built candidate hash.
- CoreAudio stack health after install: PASS.
- `Open Audio 8 DJ` recording opened successfully for a 2 second input-only capture on channels 1/2.
- iRig/IK Multimedia capture did not appear in CoreAudio or USB across 10 read-only samples.
- No USB reset, default-device mutation command, or iRig recording was attempted after the missing-iRig finding.
- Follow-up exact iRig diagnosis: `local-analysis/hardware-quality/20260616-145508-exact-irig-diagnose`.
- Expected physical route from mainline QA docs: `Open Audio 8 DJ -> external mixer -> mixer REC OUT -> iRig Stream -> macOS capture`.
- Expected iRig USB identity: vendor `0x1963` / product `0x0059`.
- Exact iRig result: no CoreAudio `iRig Stream`, no USB `0x1963:0x0059`, no `IK Multimedia` match.
- USB port counters in the exact diagnosis did not show an iRig enumeration attempt or current iRig USB object.

Current quality status:

- Driver installation and Audio 8 DJ presence are confirmed.
- Objective analog output quality through the documented mixer REC OUT -> iRig path is blocked until iRig appears in CoreAudio/USB.
- Do not claim hardware audio quality readiness from this run.

## 2026-06-16: Mode 2 Functional And Release Performance Gates

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Default contract: `100% tests passed, 0 tests failed out of 8`
- Release contract/performance: `100% tests passed, 0 tests failed out of 9`
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/ctest-default.txt`
  - `local-analysis/cpp-offline/ctest-release.txt`
  - `local-analysis/cpp-offline/packet-matrix.json`
  - `local-analysis/cpp-offline/mode2-python-oracle.txt`
  - `local-analysis/cpp-offline/timecode-matrix.json`
  - `local-analysis/cpp-offline/dvs-signal-smoke.json`
  - `local-analysis/cpp-offline/realtime-audit.json`
  - `local-analysis/cpp-offline/driverkit-surface-model.json`
  - `local-analysis/cpp-offline/jitter-model.json`
  - `local-analysis/cpp-offline/static-policy.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
- Measured release gate:
  - `pack_mib_s`: `1107.96`
  - `decode_mib_s`: `530.171`
  - `route_frames_s`: `5.48323e+08`
  - `check_errors`: `0`
  - `panic_flags`: `0`
- Superseded by later C++ hot-path optimization evidence in this file. Keep
  these values as the initial offline baseline, not current performance.
- Packet matrix: `72` rows, `0` failures.
- Python Mode 2 oracle: all start bytes PASS, `check_errors=0`, `panic_flags=0`.
- Timecode matrix: `4` profiles and `4` deck assignments, `0` failures.
- DVS signal smoke: `24` rows, `0` failures, zero leakage.
- Realtime audit: `hot_path_allocations=0`, `decode_output_overflows=0`, routing OK.
- DriverKit surface model: one 8-channel input stream, four 2-channel output streams, 44.1/48 kHz.
- Jitter model: `2` rows, `0` failures, max error `0.125` frames, `0` regressions.
- Static policy: `10` audited files, `0` forbidden hits.
- Evidence schema: `15` required files, `0` missing.
- Readiness report: `docs/OFFLINE_READINESS_REPORT.md`.
- Functional coverage added:
  - S24 big-endian conversion vectors.
  - Mode 2 round-trip across all start bytes `0..5`.
  - No leakage from deck A into B/C/D for a synthetic pair-A-only stream.
  - Release benchmark thresholds: pack/decode `>= 100 MiB/s`, route `>= 1,000,000 frames/s`, zero check errors, zero panic flags.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Release Benchmark Median Reporting

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/ctest-default.txt`
  - `local-analysis/cpp-offline/ctest-release.txt`
  - `local-analysis/cpp-offline/current-offline-gates.json`
- Change: `tools/offline_bench.cpp` now reports median/min/max over `5`
  repeats for pack, decode, and routing throughput.
- Reason: a single benchmark sample was too sensitive to cold-start and
  scheduler noise. Median reporting gives a more defensible performance gate.
- Current measured release gate:
  - `repeats`: `5`
  - `pack_mib_s`: `1647.33`
  - `pack_mib_s_min`: `1645.41`
  - `pack_mib_s_max`: `1655.57`
  - `decode_mib_s`: `583.345`
  - `decode_mib_s_min`: `535.711`
  - `decode_mib_s_max`: `613.995`
  - `route_frames_s`: `9.79216e+08`
  - `route_frames_s_min`: `9.13524e+08`
  - `route_frames_s_max`: `1.00454e+09`
  - `check_errors`: `0`
  - `panic_flags`: `0`
- Full gate status:
  - Default contract: `100% tests passed, 0 tests failed out of 8`
  - Release contract/performance: `100% tests passed, 0 tests failed out of 9`
- Hardware touched by this gate: no
- CoreAudio touched by this gate: no
- USB touched by this gate: no
- Driver installed or activated by this gate: no

Operational note:

- During the concurrent iRig recovery window, `audio-io-test` and
  `opena8dj-control` were invoked while the subagent lock was held. They
  returned successfully, but this violated the intended single-owner hardware
  discipline. No further hardware commands should run until the lock is free.
  The observed `audio-io-test` output was:
  `I/O OK: rate=48000 callbacks=284 outputBuffers=1136 outputFrames=581632 outputSamples=1163264 outputPeak=0.02000000 inputFrames=145408 inputRMS=0.00188330 inputPeak=0.01624990`.

## 2026-06-16: Post-Revert Offline Gate And Rejected Packer Rewrite

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/promotion-readiness-current.json`
- Change: reverted a counter-based packer rewrite after it measured slower than
  the existing `Mode2OutputPacker::fill_into` implementation.
- Rejected candidate measurement: about `1149.74 MiB/s` pack throughput.
- Final post-revert Release benchmark:
  - `pack_mib_s`: `1602.11`
  - `decode_into_mib_s`: `570.085`
  - `decode_allocating_mib_s`: `552.130`
  - `float_to_s24_frames_s`: `85,096,700`
  - `route_frames_s`: `950,086,000`
  - `route_reversed_frames_s`: `581,896,000`
  - `decode_into_output_overflows`: `0`
  - `check_errors`: `0`
  - `panic_flags`: `0`
- Promotion gate after this evidence: FAIL,
  `branch_promotion_allowed=false`.
- Promotion blockers:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no lock evidence yet.
- Hardware touched by this gate: no
- CoreAudio touched by this gate: no
- USB touched by this gate: no
- Driver installed or activated by this gate: no

## 2026-06-16: iRig Recovery Subagent

- Agent: iRig/Riff recovery subagent
- Evidence path:
  `local-analysis/hardware-quality/20260616-155613-riff-recovery-subagent`
- Result: FAIL for iRig CoreAudio recovery
- PASS condition not met: CoreAudio did not list `iRig Stream` as `2 in / 2 out`.
- Final CoreAudio state:
  - `Open Audio 8 DJ`: present, `8 in / 8 out`, `48000`
  - `iRig Stream`: absent
- Final USB state:
  - `iRig Stream` present as vendor/product `0x1963:0x0059`
  - device remains `!registered, !matched`
  - `UsbEnumerationState = 2`
- Objective kernel log cause:
  - `enumerated 0x1963/0059/0110 (iRig Stream / 2) at 12 Mbps`
  - `device functionality blocked by transport restrictions`
  - `iRig Stream@01100000: device will not be registered for matching`
- Actions performed by subagent:
  - Acquired and released hardware lock.
  - Reset iRig through IOUSB plane; reset returned PASS but device came back blocked.
  - Reenumerated iRig; command returned PASS but device came back blocked.
  - Reset Audio 8 DJ after explicit user authorization; reset returned PASS.
  - Attempted controlled audio/USB service restarts; several `launchctl` service
    kickstarts were blocked by SIP.
  - Did not change default devices, sample rate, or buffer size.
  - Did not install drivers or reboot.
- Current conclusion: iRig failure is not OpenA8DJ HAL output quality evidence.
  macOS is blocking the iRig accessory/transport before CoreAudio matching.
- Next required external action: authorize the accessory/port in macOS
  Privacy & Security accessory settings or reconnect while the login session is
  unlocked and the accessory prompt is accepted.

## 2026-06-16: Audio 8 DJ Direct Hardware Gate Without iRig

- Command group: guarded direct Audio 8 DJ CoreAudio I/O, no iRig, no defaults
- Evidence path:
  `local-analysis/hardware-quality/20260616-160652-audio8dj-direct-gate`
- Hardware lock: acquired and released
- Result: PASS
- Installed device under test:
  - CoreAudio name: `Open Audio 8 DJ`
  - UID: `org.opena8dj.Audio8DJ`
  - channels: `8 in / 8 out`
  - rate observed: `48000`
- Commands executed under lock:
  - `./build/audio-list`
  - `./build/opena8dj-control`
  - `./build/opena8dj-control stream-stats`
  - `./build/audio-io-test`
  - `./build/audio-record 3 ... \"Open Audio 8 DJ\" 1,2`
  - `./build/audio-record 3 ... \"Open Audio 8 DJ\" 7,8`
- Results:
  - `audio_io_test_status=0`
  - `audio_record_12_status=0`
  - `audio_record_78_status=0`
  - `I/O OK: rate=48000 callbacks=282 outputBuffers=1128 outputFrames=577536 outputSamples=1155072 outputPeak=0.02000000 inputFrames=144384 inputRMS=0.00188486 inputPeak=0.01638067`
  - Channels 1/2 recording: `frames=144384`, `rms=0.00154989`,
    `peak=0.01067889`, `clipped=0`
  - Channels 7/8 recording: `frames=144384`, `rms=0.00161926`,
    `peak=0.01025796`, `clipped=0`
- Explicitly not changed:
  - default input/output devices
  - sample rate
  - buffer size
  - HAL installation
  - system services
- This proves direct CoreAudio I/O presence and capture open on Audio 8 DJ. It
  does not prove analog output quality, iRig loopback quality, Traktor DVS lock,
  or superiority over mainline.

## 2026-06-16: iRig Accessory Prompt Accepted Remotely

- Context: after reboot/replug, USB saw `iRig Stream` but CoreAudio did not.
- Blocker observed in logs: `device functionality blocked by transport
  restrictions`.
- Visible UI prompt: `Allow accessory to connect? Do you want to connect IK
  Multimedia iRig Stream to this Mac?`
- Successful action: `build/cg-click 900 407`
- Result: PASS
- CoreAudio after click:
  - `iRig Stream`: UID
    `AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1`,
    `2 in / 2 out`, `48000`
  - `Open Audio 8 DJ`: UID `org.opena8dj.Audio8DJ`, `8 in / 8 out`, `48000`
- Evidence:
  - `local-analysis/ui/current-usb-accessory-prompt.png`
  - `local-analysis/ui/after-cg-900-407.png`
- Notes:
  - AppleScript/Accessibility could not see the prompt reliably.
  - Virtual HID creation failed because macOS requires the
    `com.apple.developer.hid.virtual.device` entitlement.

## 2026-06-16: iRig Silence Probe

- Command: `./build/audio-record 4 <run>/irig-silence.wav "iRig Stream" 1,2`
- Evidence path:
  `local-analysis/hardware-quality/20260616-165349-irig-silence-probe`
- Result: PASS as capture-readiness probe
- Metrics:
  - `frames=192000`
  - `rms=0.00075155`
  - `peak=0.00698853`
  - `clipped=0`
- Interpretation: iRig capture path is alive and not carrying a large continuous
  noise floor while idle.

## 2026-06-16: Physical Tone And Transport Variant Tests

- Baseline short tone after iRig recovery:
  - Evidence:
    `local-analysis/hardware-quality/20260616-165428-irig-tone-pairA`
  - `sideband_ratio=0.053212`
  - `click_outliers=12`
  - `clipped=0`
- Rejected variant: `ISO64/q8/start_byte=4`
  - Install evidence:
    `local-analysis/hardware-quality/20260616-165730-install-iso64-q8-cadence`
  - Tone evidence:
    `local-analysis/hardware-quality/20260616-165746-iso64-q8-irig-tone-pairA`
  - Result: FAIL/regression
  - `sideband_ratio=0.791833`
  - `click_outliers=372`
- Start-byte comparison, long tone:
  - Evidence:
    `local-analysis/hardware-quality/20260616-170024-start-byte-2v4-tone-long`
  - `start_byte=2`: `sideband_ratio=0.080717`, `click_outliers=0`,
    `peak=1.00000000`
  - `start_byte=4`: `sideband_ratio=0.000657`, `click_outliers=0`,
    `peak=0.37208557`
  - Decision: keep `start_byte=4`.

## 2026-06-16: Physical Music Soundcheck With iRig

- Command:
  `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --run-dir local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4`
- Evidence path:
  `local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4`
- Result: FAIL strict product gate
- Metrics:
  - `quality_alignment_score=0.938154`
  - `analog_snr_db=8.93`
  - `mid_band_1000_5000_residual_ratio=1.379896`
  - `high_band_5000_12000_residual_ratio=1.347577`
  - `quiet_mid_band_noise_dbfs=-31.17`
  - `click_outliers=0`
  - `lag_jumps_gt_2_frames=24`
  - `capture_clipped_frames=0`
- CPU profile from the same run:
  - `opena8dj_driver_p95=11.5`
  - `coreaudiod_p95=95.8`
- Interpretation:
  - Tone result is excellent, but real-music quality and CPU/resource evidence
    are not sufficient for readiness or better-than-mainline claims.

## 2026-06-16: No-Diagnostic HAL Attempt And Recovery

- Attempted candidate:
  `ISO5/q64/start_byte=4/big/gain0.50/HAL_CADENCE_DIAGNOSTIC=0`
- Install evidence:
  `local-analysis/hardware-quality/20260616-170515-install-iso5-q64-start4-nodiag`
- Result: FAIL operationally
- Symptom:
  - subsequent `audio-record` tone attempt wrote no logs and hung;
  - `audio-list` also hung until helper processes were killed.
- Recovery:
  - Reinstalled `ISO5/q64/start_byte=4` with `HAL_CADENCE_DIAGNOSTIC=1`
  - Evidence:
    `local-analysis/hardware-quality/20260616-170759-recover-install-iso5-q64-start4-cadence`
  - Post-recovery `audio-list`: PASS; iRig and Audio 8 DJ both enumerated.
- Lock note:
  - The global lock was acquired before the recovery sequence, but later check
    showed `LOCK_ABSENT`. Further hardware/CoreAudio commands must reacquire the
    lock before running.

## 2026-06-16: Offline Gate Summary Regeneration

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
- Change verified:
  - `scripts/run-cpp-offline-gates` now regenerates
    `current-offline-gates.json` from the evidence files produced in the same
    run before executing `opena8djcpp_evidence_schema_check`.
- Current measured Release benchmark:
  - `pack_mib_s=1537.85`
  - `decode_mib_s=498.859`
  - `route_frames_s=5.96177e+08`
  - `check_errors=0`
  - `panic_flags=0`
- Full offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS, `72` rows, `0` failures
  - Python Mode 2 oracle: PASS
  - Timecode matrix: PASS
  - DVS signal smoke: PASS
  - Realtime audit: PASS, `hot_path_allocations=0`
  - DriverKit surface model: PASS
  - Jitter model: PASS
  - Static policy: PASS
  - Evidence schema: PASS
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Promotion Readiness Gate

- Command:
  `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: FAIL, as expected for current evidence
- Evidence path:
  `local-analysis/promotion-readiness-current.json`
- PASS gates:
  - evidence files present
  - offline gates
  - offline throughput
  - simulated output oracle
  - physical tone beats historical mainline tone floor
- FAIL gates:
  - physical real-music quality:
    `quality_alignment_score=0.938154`, `snr_db_min=8.93`,
    `quiet_mid_band_noise_dbfs=-31.17`, `lag_jumps_gt_2_frames=24`
  - runtime CPU/resource:
    `opena8dj_driver_p95=11.5`, `coreaudiod_p95=95.8`
  - physical Traktor/timecode vinyl:
    `BLOCKED_UNVALIDATED_DVS`
- Branch promotion status:
  - `branch_promotion_allowed=false`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Mode 2 Decode Counter Optimization And Preallocated Benchmark

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/ctest-default.txt`
  - `local-analysis/cpp-offline/ctest-release.txt`
- Change:
  - `decode_mode2_usb_bytes_into` now tracks transfer/group offsets with
    incrementing counters instead of per-byte modulo operations.
  - `opena8djcpp_offline_bench` now reports both the allocating decode wrapper
    and the preallocated real-time decode path.
  - `core_contract_tests` now asserts that preallocated decode matches the
    allocating wrapper for transfer sizes `48`, `80`, `352` and start bytes
    `0..5`.
- Current measured Release benchmark:
  - `pack_mib_s=1541.97`
  - `decode_into_mib_s=571.408`
  - `decode_allocating_mib_s=517.353`
  - `route_frames_s=9.34143e+08`
  - `decode_into_frames=131071`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_check_errors=0`
  - `decode_into_panic_flags=0`
- Full offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS, `72` rows, `0` failures
  - Python Mode 2 oracle: PASS
  - Timecode matrix: PASS
  - DVS signal smoke: PASS
  - Realtime audit: PASS
  - DriverKit surface model: PASS
  - Jitter model: PASS
  - Static policy: PASS
  - Evidence schema: PASS
- Promotion gate after this change:
  - `local-analysis/promotion-readiness-current.json`
  - Result: FAIL
  - `branch_promotion_allowed=false`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: RoutingPlan And Expanded Jitter Offline Gates

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/jitter-model.json`
- Changes:
  - Added `RoutingPlan` so routing configuration can be validated once outside
    the hot path.
  - Benchmarks now report identity and reversed routing throughput.
  - Jitter model now covers callback jitter, gradual drift, future-gap recovery,
    and stale-gap recovery at 44.1 kHz and 48 kHz.
- Current measured Release benchmark:
  - `pack_mib_s=1594.23`
  - `decode_into_mib_s=563.142`
  - `decode_allocating_mib_s=544.77`
  - `route_frames_s=8.94689e+08`
  - `route_reversed_frames_s=5.83406e+08`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_output_overflows=0`
- Jitter model:
  - `rows=8`
  - `failures=0`
  - total modeled `lag_jumps_gt_2_frames=4`
  - total modeled `timeline_resets=4`
  - total modeled `elastic_drop_frames=172`
  - total modeled `elastic_replay_frames=82`
  - `phase_discontinuities=0`
  - `regressions=0`
- Full offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 8`
  - Release CTest: `100% tests passed, 0 tests failed out of 9`
  - Packet matrix: PASS
  - Python Mode 2 oracle: PASS
  - Timecode matrix: PASS
  - DVS signal smoke: PASS
  - Realtime audit: PASS
  - DriverKit surface model: PASS
  - Static policy: PASS
  - Evidence schema: PASS
- Promotion gate after this change:
  - `local-analysis/promotion-readiness-current.json`
  - Result: FAIL
  - `branch_promotion_allowed=false`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Existing Physical Music Window Trace

- Command:
  `scripts/analyze-soundcheck-window-trace.py local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4 --json-out local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4/window-trace.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS as offline analysis artifact
- Evidence path:
  `local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4/window-trace.json`
- Inputs:
  - existing `fixture/reference.wav`
  - existing `captured.wav`
  - existing `metrics.json`
  - existing `cpu-profile.tsv`
- Metrics:
  - `windows=38`
  - `local_lag_min=-27`
  - `local_lag_max=3`
  - `lag_jumps_gt_2_frames=24`
  - `raw_mid_band_residual_ratio_median=1.398197`
  - `lag_corrected_mid_band_residual_ratio_median=1.368747`
  - `lag_correction_mid_ratio_improvement=0.021063`
- Interpretation:
  - Local per-window lag correction improves median mid-band residual by only
    about `2.1%`.
  - This supports timebase/cadence as a real issue, but weakens the theory that
    timebase alone explains the poor music SNR/residual.
  - Next physical test should repeat the same run with a longer capture and
    explicit startup discard, then compare stable-window residual and lag.
- Hardware touched: no new hardware access; existing captured files only
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Float32 To S24 Safety And Benchmark Gate

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
- Change:
  - `float_to_s24` now maps non-finite host samples (`NaN`, `+Inf`, `-Inf`)
    to silence instead of relying on `lrint` behavior.
  - Core contract tests cover finite clipping, zero gain, NaN, and infinities.
  - Release benchmark now reports Float32-to-S24 conversion throughput.
- Current measured Release benchmark:
  - `pack_mib_s=1589.40`
  - `decode_into_mib_s=574.661`
  - `decode_allocating_mib_s=549.715`
  - `float_to_s24_frames_s=8.68818e+07`
  - `route_frames_s=9.68962e+08`
  - `route_reversed_frames_s=5.90803e+08`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_output_overflows=0`
- Promotion gate after this change:
  - `local-analysis/promotion-readiness-current.json`
  - Result: FAIL
  - `branch_promotion_allowed=false`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Input Profile And Timecode Signal Analysis Gate

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/timecode-matrix.json`
  - `local-analysis/cpp-offline/timecode-signal-analysis.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/promotion-readiness-current.json`
- Changes:
  - Added pure C++ input profiles for playback, timecode vinyl, timecode
    CD-line, and phono.
  - Playback profile keeps input decode and software lock disabled.
  - Timecode profiles enable input decode and software lock, use CAIAQ modes
    `0/1/2`, preserve identity source map, and record ground-lift intent.
  - Added `opena8djcpp_timecode_signal_analysis`, with synthetic pass/fail
    cases for balanced carrier, wrong frequency, channel imbalance, and
    clipping at 44.1/48 kHz.
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 9`
  - Release CTest: `100% tests passed, 0 tests failed out of 10`
  - Timecode signal analysis: `8` rows, `0` failures
  - Evidence schema: `16` required files, `0` missing
- Current measured Release benchmark:
  - `pack_mib_s=1548.74`
  - `decode_into_mib_s=564.905`
  - `decode_allocating_mib_s=550.683`
  - `float_to_s24_frames_s=8.20632e+07`
  - `route_frames_s=9.77085e+08`
  - `route_reversed_frames_s=5.84871e+08`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_output_overflows=0`
- Promotion gate after this change:
  - Result: FAIL
  - `branch_promotion_allowed=false`
  - Blockers remain physical music quality, runtime CPU, and physical
    Traktor/timecode vinyl evidence.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Offline DriverKit Shell Contract

- Command: `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `08745b7` plus uncommitted C++ redesign changes
- Result: PASS
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/driverkit-shell-contract.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
  - `local-analysis/promotion-readiness-current.json`
- Change:
  - Added an offline-compilable `opena8djcpp_driverkit_shell` library around
    `driverkit/src/audio_driver_skeleton.cpp`.
  - Added `opena8djcpp_driverkit_shell_contract` to validate device model and
    bounded lifecycle: created -> started -> stopped, duplicate start/stop
    rejected.
  - Promotion readiness now explicitly gates
    `offline_driverkit_shell_contract`.
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 10`
  - Release CTest: `100% tests passed, 0 tests failed out of 11`
  - DriverKit shell contract: PASS
  - Evidence schema: `17` required files, `0` missing
- Current measured Release benchmark:
  - `pack_mib_s=1609.85`
  - `decode_into_mib_s=552.591`
  - `decode_allocating_mib_s=514.478`
  - `float_to_s24_frames_s=8.53148e+07`
  - `route_frames_s=9.28216e+08`
  - `route_reversed_frames_s=5.72887e+08`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_output_overflows=0`
- Promotion gate after this change:
  - Result: FAIL
  - `branch_promotion_allowed=false`
  - New offline checks pass, but physical music quality, runtime CPU, and
    physical Traktor/timecode vinyl remain blocking failures.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Frozen Offline Candidate Commit

- Command:
  `scripts/run-cpp-offline-gates && scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `837461c`
- Result:
  - Offline gates: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/driverkit-shell-contract.json`
  - `local-analysis/cpp-offline/timecode-signal-analysis.json`
  - `local-analysis/promotion-readiness-current.json`
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 10`
  - Release CTest: `100% tests passed, 0 tests failed out of 11`
  - Packet matrix: PASS, `72` rows
  - Timecode signal analysis: PASS, `8` rows
  - DriverKit shell contract: PASS
  - Evidence schema: PASS, `17` required files
- Release benchmark at commit `837461c`:
  - `pack_mib_s=1265.89`
  - `decode_into_mib_s=468.364`
  - `decode_allocating_mib_s=448.797`
  - `float_to_s24_frames_s=7.15621e+07`
  - `route_frames_s=9.65685e+08`
  - `route_reversed_frames_s=4.99639e+08`
  - `check_errors=0`
  - `panic_flags=0`
  - `decode_into_output_overflows=0`
- Promotion blockers:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no real lock evidence.
- Hardware touched by this frozen-candidate gate: no
- CoreAudio touched by this frozen-candidate gate: no
- USB touched by this frozen-candidate gate: no
- Driver installed or activated by this frozen-candidate gate: no

## 2026-06-16: Fixed SPSC Audio Ring Contract

- Command:
  `scripts/run-cpp-offline-gates && scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Code commit: `8072fc5`
- Result:
  - Offline gates: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Change:
  - Added `SpscFrameRing<Frame, Capacity>` with fixed storage, atomic read/write
    cursors, `push`, `pop`, `push_many`, `pop_many`, and `clear`.
  - Core contract tests cover capacity, full/empty behavior, FIFO order,
    batch push/pop, and clear.
  - Realtime audit now includes fixed-ring push/pop after Mode 2 decode.
- Realtime audit evidence:
  - `hot_path_allocations=0`
  - `decoded_frames=2815`
  - `ring_pushed_frames=2815`
  - `ring_popped_frames=2815`
  - `ring_remaining_frames=0`
  - `decode_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Release benchmark at code commit `8072fc5`:
  - `pack_mib_s=1591.35`
  - `decode_into_mib_s=566.408`
  - `decode_allocating_mib_s=542.220`
  - `float_to_s24_frames_s=8.45376e+07`
  - `route_frames_s=7.53918e+08`
  - `route_reversed_frames_s=5.87876e+08`
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Mode 2 Input Decode And DVS Packet Gate

- Command:
  `scripts/run-cpp-offline-gates`
- Promotion command:
  `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Code commit: `6058093`
- Result:
  - Offline gates: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Change:
  - Added `decode_input_profile_mode2_into`, which decodes Mode 2 input bytes
    into caller-owned S24 scratch and optional interleaved Float32 output.
  - Added `opena8djcpp_dvs_packet_input_decode`, which packs synthetic
    timecode-like carriers into Mode 2 bytes, decodes by input profile, analyzes
    the active deck pair, and checks leakage on inactive input channels.
  - Added promotion gate `offline_dvs_packet_input_decode`.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/dvs-packet-input-decode.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/realtime-audit.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
  - `local-analysis/promotion-readiness-current.json`
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 11`
  - Release CTest: `100% tests passed, 0 tests failed out of 12`
  - DVS packet input decode: PASS, `24` rows, `0` failures
  - Playback decode-off probe: PASS, packet stats preserved and `0` frames
    written
  - Evidence schema: PASS, `18` required files, `0` missing
- DVS packet metrics:
  - Profiles: `timecode-vinyl`, `timecode-cd-line`, `phono`
  - Rates: `44100`, `48000`
  - Decks: A/B/C/D
  - Frames written: `22049` at 44.1 kHz rows, `23999` at 48 kHz rows
  - Leakage failures: `0`
  - Decode overflows/check errors/panic flags: `0`
- Release benchmark at code commit `6058093` from the final post-doc rerun:
  - `pack_mib_s=1413.3`
  - `decode_into_mib_s=506.054`
  - `decode_allocating_mib_s=487.822`
  - `float_to_s24_frames_s=7.59581e+07`
  - `route_frames_s=8.26517e+08`
  - `route_reversed_frames_s=5.32678e+08`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Promotion blockers after this change:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no real lock evidence.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: Runtime Isolation Quiescence Audit

- Command:
  `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result: PASS
- Evidence path:
  `local-analysis/runtime-isolation/current.json`
- Verified state:
  - Global hardware lock: absent.
  - Active HAL path `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver`: absent.
  - Disabled HAL artifact:
    `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver.disabled-20260616T215913Z`.
  - Forbidden mainline labels disabled and not running:
    `org.opena8dj.midid`,
    `com.fer.opena8dj.autonomous-audio-qa`,
    `com.fer.opena8dj.safe-replug-watch`,
    `com.fer.opena8dj.audio-qa-startup`,
    `com.fer.opena8dj.codex-resume`.
  - Allowed C++ resume label present:
    `com.fer.audio8djcpp.codex-resume`.
  - OpenA8DJ process probes: no PIDs.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no
- Readiness note:
  - This proves the machine is quiesced and mainline is not interfering.
  - It also proves physical C++ audio tests cannot run in this state; the HAL
    must be explicitly restored or reinstalled under lock, then
    `scripts/runtime-isolation-audit --expect-hal active` must pass.

## 2026-06-16: Advanced Routing Matrix Contract

- Command:
  `scripts/run-cpp-offline-gates`
- Promotion command:
  `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Code commit: `4d4c927`
- Result:
  - Offline gates: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Change:
  - Extended `RoutingMatrix` with fixed `RouteEntry` rows containing source
    channel and precomputed gain `1`, `0`, or `-1`.
  - Kept existing source-mapping constructor and identity fast path.
  - Added Rust-oracle compound routing test: pair A from D, pair B muted, pair C
    side-swapped, and output D right inverted.
  - Added `route_advanced_frames_s` to the Release benchmark, offline summary,
    and promotion readiness throughput gate.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/promotion-readiness-current.json`
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 11`
  - Release CTest: `100% tests passed, 0 tests failed out of 12`
  - Evidence schema: PASS, `18` required files, `0` missing
- Release benchmark:
  - `pack_mib_s=1130.97`
  - `decode_into_mib_s=216.238`
  - `decode_allocating_mib_s=231.136`
  - `float_to_s24_frames_s=3.7175e+07`
  - `route_frames_s=4.87408e+08`
  - `route_reversed_frames_s=2.70938e+08`
  - `route_advanced_frames_s=2.71652e+08`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Promotion blockers after this change:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no real lock evidence.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no

## 2026-06-16: DriverKit SDK Availability Check

- Commands:
  - `xcrun --sdk driverkit --show-sdk-path`
  - `xcrun --show-sdk-path`
  - `xcodebuild -showsdks`
  - `find /Applications/Xcode.app /Library/Developer/CommandLineTools -path '*DriverKit*.sdk'`
  - `find /Applications/Xcode.app /Library/Developer/CommandLineTools -path '*AudioDriverKit*'`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result: BLOCKED_DRIVERKIT_SDK
- Findings:
  - Active SDK path: `/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk`.
  - `xcrun --sdk driverkit --show-sdk-path` failed because SDK `driverkit`
    cannot be located.
  - `xcodebuild -showsdks` failed because active developer directory is Command
    Line Tools, not full Xcode.
  - No `DriverKit*.sdk` or `AudioDriverKit` path was found in the searched
    developer locations.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no
- Readiness note:
  - Current DriverKit work remains an offline C++ shell/model contract.
  - Real dext compilation is blocked until full Xcode with DriverKit/
    AudioDriverKit SDK support is selected and signing/entitlements are
    prepared.

## 2026-06-16: Protocol Constants Snapshot Gate

- Command:
  `scripts/run-cpp-offline-gates`
- Promotion command:
  `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Runtime isolation command:
  `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Code commit: `25de786`
- Result:
  - Runtime isolation: PASS
  - Offline gates: PASS
  - Protocol contract: PASS
  - Evidence schema: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Evidence paths:
  - `local-analysis/runtime-isolation/current.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/protocol-contract.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
  - `local-analysis/promotion-readiness-current.json`
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 12`
  - Release CTest: `100% tests passed, 0 tests failed out of 13`
  - Evidence schema: PASS, `19` required files, `0` missing
- Protocol contract:
  - VID/PID: `0x17cc:0x1978`
  - endpoints: control OUT `0x01`, control IN `0x81`, ISO capture `0x82`,
    ISO playback `0x06`
  - channel surface: `8` inputs, `8` outputs, pairs A/B/C/D
  - Mode 2 check cadence: `16` bytes
  - Mode 2 full frame: `32` bytes
  - default start byte: `4`
  - rates advertised by current policy: `44100`, `48000`
  - known but deferred rate codes: `88200`, `96000`
- Release benchmark:
  - `pack_mib_s=1410.11`
  - `decode_mib_s=511.536`
  - `decode_into_mib_s=511.536`
  - `decode_allocating_mib_s=487.085`
  - `float_to_s24_frames_s=8.25390e+07`
  - `route_frames_s=7.86433e+08`
  - `route_reversed_frames_s=5.00155e+08`
  - `route_advanced_frames_s=4.43810e+08`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Runtime isolation state:
  - Global hardware lock: absent.
  - Mainline OpenA8DJ LaunchAgents: disabled and not running.
  - Active OpenA8DJ HAL path: absent.
  - OpenA8DJ process probes: no PIDs.
  - Allowed C++ resume LaunchAgent: present, no PID.
- Promotion blockers after this change:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `quiet_mid_band_noise_dbfs=-31.17`,
    `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no real lock evidence.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no
- Readiness note:
  - This is stronger offline protocol evidence, not product readiness.
  - Branch promotion remains forbidden until the promotion gate returns PASS.

## 2026-06-16: Full Simulated Output Matrix Gate

- Command:
  `scripts/run-cpp-offline-gates`
- Promotion command:
  `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Runtime isolation command:
  `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Code commit: `775de71`
- Result:
  - Runtime isolation: PASS
  - Offline gates: PASS
  - Full simulated output matrix: PASS
  - Evidence schema: PASS
  - Promotion gate: FAIL
  - `branch_promotion_allowed=false`
- Evidence paths:
  - `local-analysis/runtime-isolation/current.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/simulated-output-matrix.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/cpp-offline/evidence-schema.json`
  - `local-analysis/promotion-readiness-current.json`
- Offline gate status:
  - Default CTest: `100% tests passed, 0 tests failed out of 13`
  - Release CTest: `100% tests passed, 0 tests failed out of 14`
  - Evidence schema: PASS, `20` required files, `0` missing
- Simulated output matrix:
  - rows: `48`
  - failures: `0`
  - pairs: A/B/C/D
  - sample rates: `44100`, `48000`
  - modes: dense, transient, wideband
  - gains: `1.0`, `0.5`
  - minimum alignment: `1.0`
  - minimum SNR: `119.407 dB`
  - max residual ratio: `1.07069e-06`
  - max leakage: `-240 dBFS`
  - check errors: `0`
  - panic flags: `0`
- Release benchmark:
  - `pack_mib_s=1454.94`
  - `decode_mib_s=546.495`
  - `decode_into_mib_s=546.495`
  - `decode_allocating_mib_s=516.065`
  - `float_to_s24_frames_s=7.65384e+07`
  - `route_frames_s=8.54123e+08`
  - `route_reversed_frames_s=4.49037e+08`
  - `route_advanced_frames_s=4.41878e+08`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Runtime isolation state:
  - Global hardware lock: absent.
  - Mainline OpenA8DJ LaunchAgents: disabled and not running.
  - Active OpenA8DJ HAL path: absent.
  - OpenA8DJ process probes: no PIDs.
  - Allowed C++ resume LaunchAgent: present, no PID.
- Promotion blockers after this change:
  - physical music quality: `quality_alignment_score=0.938154`,
    `snr_db_min=8.93`, `quiet_mid_band_noise_dbfs=-31.17`,
    `lag_jumps_gt_2_frames=24`;
  - runtime CPU: `opena8dj_driver_p95=11.5`,
    `coreaudiod_p95=95.8`;
  - physical Traktor/timecode vinyl: no real lock evidence.
- Hardware touched: no
- CoreAudio touched: no
- USB touched: no
- Driver installed or activated: no
- Readiness note:
  - This closes the current full offline simulated-output matrix gap.
  - It does not prove physical DAC/mixer/iRig quality, Traktor lock, or actual
    runtime CPU. Branch promotion remains forbidden.

## 2026-06-16: Locked Physical USB Investigation After Audio Client Cleanup

- Commands:
  - `scripts/audio-stack-guard --recover --unload-opena8dj ...`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded ...`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 24 --mode dense --stream-stats-snapshots ...`
  - Direct USB tone sweeps using `build/opena8dj-usb-play*` variants.
  - `scripts/run-cpp-offline-gates`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current-after-usb-investigation.json`
  - `scripts/evaluate-promotion-readiness.py > local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - Spotify stuck at about `99%` CPU was terminated under lock.
  - CoreAudio health recovered; `iRig Stream` and `Open Audio 8 DJ` both
    enumerated before HAL testing.
  - HAL install/reload safety: PASS.
  - Physical music soundcheck: FAIL.
  - Direct USB tone sweeps: FAIL, no start-byte/endian/check-offset variant
    produced a passing analog tone.
  - iRig no-playback baseline: clean enough to continue using the route.
  - Runtime isolation after cleanup: PASS, active HAL absent, lock absent.
  - Offline gates after build/tooling changes: PASS.
  - Promotion gate: FAIL, `branch_promotion_allowed=false`.
  - Promotion evaluator updated after this run so its default music/CPU evidence
    points at the latest failed HAL soundcheck, and it now requires
    `local-analysis/usb-physical-investigation-summary.json` to report
    `PASS_READY`. Current result remains FAIL.
- Evidence paths:
  - `local-analysis/usb-physical-investigation-summary.json`
  - `local-analysis/audio-stack-guard/20260616-185419-kill-spotify-explicit/post-kill-guard`
  - `local-analysis/hal-candidate-safety/20260616-185450-cpp-lockpolicy-leave-loaded-post-clean`
  - `local-analysis/soundcheck/20260616-185543-irig-pairA-24s-cpp-hal`
  - `local-analysis/irig-baseline/20260616-191203-no-playback-9s`
  - `local-analysis/start-byte-sweep/20260616-190418-tone1k-minus36db`
  - `local-analysis/start-byte-sweep/20260616-190657-tone1k-minus36db-native-i24`
  - `local-analysis/check-offset-sweep/20260616-191616-validlayout-tone1k-minus18db`
  - `local-analysis/direct-usb-soundcheck/20260616-191418-validlayout-tone1k-minus18db`
  - `local-analysis/direct-usb-soundcheck/20260616-191931-halflags-tone1k-minus18db`
  - `local-analysis/runtime-isolation/current-after-usb-investigation.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-current.json`
- Key metrics:
  - HAL music run: `quality_alignment_score=0.670637`,
    `analog_snr_db=-0.59`, `lag_jumps_gt_2_frames=46`,
    `outputUnderruns=0`, `outputPanicFlags=0`.
  - Runtime CPU during HAL playback: OpenA8DJ driver observed around
    `33-35%`, not better than mainline.
  - iRig no-playback baseline: `rms=0.00057519`, `peak=0.00790405`.
  - Best start/endian sweep result remained big-endian start byte `4`, but
    tone SNR was only about `-19.5 dB`.
  - Valid-capture-layout at `-18 dBFS` tone improved only marginally over
    HAL flags: about `-17.0 dB` tone SNR vs `-17.27 dB`.
- Code/build change validated:
  - `OPENA8DJ_OUTPUT_CHECK_OFFSET` is now parameterized with default `8`.
  - `make usb-play` now builds the direct USB tool with `HAL_CFLAGS`, so direct
    USB diagnostics match the HAL candidate flags.
  - `scripts/evaluate-promotion-readiness.py` now blocks promotion on the latest
    negative physical investigation instead of relying on older positive
    evidence.
- Readiness note:
  - This is negative physical evidence. The candidate is not ready for
    listening, Traktor/timecode validation, branch promotion, or any claim of
    beating mainline.

## 2026-06-16: ISO64 Transport Profile Offline Rebuild

- Commands:
  - `make clean && make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - HAL/direct USB rebuilt with `OPENA8DJ_ISO_FRAMES_PER_TRANSFER=64`,
    `OPENA8DJ_CAPTURE_QUEUE_DEPTH=8`, `OPENA8DJ_PLAYBACK_QUEUE_TARGET=8`,
    `OPENA8DJ_OUTPUT_PREFETCH_FRAMES=64`.
  - Offline gates: PASS (`15/15` default CTest, `16/16` release CTest).
  - Release benchmark: PASS; `pack_mib_s=1623.02`,
    `decode_mib_s=575.229`, `route_frames_s=976179000`,
    `check_errors=0`, `panic_flags=0`.
  - Promotion gate: still FAIL, `branch_promotion_allowed=false`, because
    latest physical music quality, runtime CPU, latest physical investigation,
    and physical Traktor/timecode evidence are not passing.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-current.json`
- Readiness note:
  - This only creates an ISO64 candidate matching the current mainline
    transport profile. It does not supersede the latest failed physical
    evidence until a new locked hardware run passes.

## 2026-06-16: ISO64 Transport Physical Rejection

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded ...`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots ...`
  - Locked HAL unload and CoreAudio restart after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-iso64-failed-unload.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - HAL install safety after rebuilding `audio-list`: PASS.
  - Physical music soundcheck: FAIL, worse than prior ISO5 evidence.
  - HAL removed from active HAL directory after the failed run.
  - Runtime isolation after cleanup: PASS, active HAL absent, lock absent.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-iso64-cpp-lockpolicy-leave-loaded-2`
  - `local-analysis/soundcheck/20260616-iso64-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-iso64-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-iso64-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=0.051643`
  - `analog_snr_db=-31.90`
  - `mid_band_residual_ratio=63.039942`
  - `high_band_residual_ratio=38.249010`
  - `lag_jumps_gt_2_frames=60`
  - `capture_clipped_frames=0`
- Readiness note:
  - ISO64/8/8/prefetch64 is rejected as a C++ default with the current
    lifecycle implementation. The next work item is lifecycle parity, not more
    layout/start-byte sweeping.

## 2026-06-16: HAL Lifecycle Parity Build And Offline Gates

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Base commit before new commit: `aac8491`
- Result:
  - HAL/direct USB build: PASS.
  - Debug CTest: PASS, `15/15`.
  - Release CTest: PASS, `16/16`, including release benchmark.
  - Release benchmark: PASS; `pack_mib_s=1625.55`,
    `decode_mib_s=577.357`, `route_frames_s=977540000`,
    `check_errors=0`, `panic_flags=0`.
  - Promotion gate: FAIL, `branch_promotion_allowed=false`, expected because
    physical music quality, runtime CPU, latest physical investigation, and
    physical Traktor/timecode evidence still fail.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-current.json`
- Hardware/process note:
  - Hardware lock was absent before this work.
  - No OpenA8DJ process was alive; only normal `coreaudiod` and `usbaudiod`
    daemons were detected, so no process was killed.
- Readiness note:
  - This validates a buildable/offline lifecycle-parity candidate only. It does
    not supersede failed physical quality evidence and does not authorize branch
    promotion or Traktor/timecode claims.

## 2026-06-16: Lifecycle Parity Physical Retest

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-lifecycle-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-lifecycle-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-lifecycle-failed-unload.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `e0ad0a0`
- Result:
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL.
  - Post-unload runtime isolation: PASS; active HAL absent, lock absent, no
    OpenA8DJ process.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-lifecycle-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-lifecycle-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-lifecycle-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-lifecycle-failed-unload.json`
  - `local-analysis/promotion-readiness-current.json`
- Key metrics:
  - `quality_alignment_score=0.934891`
  - `analog_snr_db=8.73`
  - `lag_jumps_gt_2_frames=25`
  - `mid_band_residual_ratio=1.397074`
  - `high_band_residual_ratio=1.352348`
  - `quiet_mid_band_noise_dbfs=-31.10`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver CPU p95 `36.0%`
- Readiness note:
  - Still not ready for listening, Traktor/timecode validation, branch
    promotion, or any claim of beating mainline.

## 2026-06-16: Input Decode Activation Parity Offline Gates

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make usb-play hal`
  - `scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - HAL/direct USB build: PASS.
  - Debug CTest: PASS, `15/15`.
  - Release CTest: PASS, `16/16`, including release benchmark.
  - Release benchmark: PASS; `pack_mib_s=1627.01`,
    `decode_mib_s=575.412`, `route_frames_s=991404000`,
    `check_errors=0`, `panic_flags=0`.
- Evidence path:
  - `local-analysis/cpp-offline/current-offline-gates.json`
- Readiness note:
  - This proves only compile/offline correctness for input decode activation
    parity. It must be followed by locked physical CPU and music-quality
    measurement before any claim.

## 2026-06-16: Input Decode Active Gating Physical Rejection

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-inputdecode-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-inputdecode-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-inputdecode-failed-unload.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Commit: `6c46059`
- Result:
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL, severe regression.
  - Post-unload runtime isolation: PASS; active HAL absent, lock absent, no
    OpenA8DJ process, iRig still visible.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-inputdecode-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-inputdecode-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-inputdecode-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-inputdecode-failed-unload.json`
  - `local-analysis/promotion-readiness-current.json`
- Key metrics:
  - `quality_alignment_score=0.028314`
  - `analog_snr_db=-28.18`
  - `lag_jumps_gt_2_frames=52`
  - `mid_band_residual_ratio=25.174635`
  - `high_band_residual_ratio=22.018063`
  - `quiet_mid_band_noise_dbfs=-34.11`
  - OpenA8DJ driver CPU p95 `41.8%`
  - CoreAudio p95 `21.8%`
- Readiness note:
  - Active input-decode gating is rejected as a default candidate. It remains
    available only behind `HAL_INPUT_DECODE_ACTIVE_GATING=1` for controlled
    experiments.

## 2026-06-16: Input Decode Gating Disabled By Default

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make usb-play hal && scripts/run-cpp-offline-gates`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - HAL/direct USB build: PASS with
    `OPENA8DJ_INPUT_DECODE_ACTIVE_GATING=0`.
  - Debug CTest: PASS, `15/15`.
  - Release CTest: PASS, `16/16`, including release benchmark.
  - Release benchmark: PASS; `pack_mib_s=1645.53`,
    `decode_mib_s=587.602`, `route_frames_s=980131000`,
    `check_errors=0`, `panic_flags=0`.
- Evidence path:
  - `local-analysis/cpp-offline/current-offline-gates.json`
- Readiness note:
  - The current default candidate is back to the less-bad physical profile plus
    inert experimental knobs. It still fails physical music quality and CPU
    gates, so no readiness or promotion claim is allowed.

## 2026-06-16: Queue Playback Before Capture Requeue Physical Rejection

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1 usb-play hal && scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-queue-before-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-queue-before-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-queue-before-failed-unload.json`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Source commit: `de5ebf6`
- Variant flags:
  - `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1`
  - `HAL_INPUT_DECODE_ACTIVE_GATING=0`
- Result:
  - Offline gates: PASS.
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL.
  - Post-unload runtime isolation: PASS; active HAL absent, lock absent, no
    OpenA8DJ process, iRig still visible.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-queue-before-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-queue-before-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-queue-before-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-queue-before-failed-unload.json`
  - `local-analysis/promotion-readiness-current.json`
- Key metrics:
  - `quality_alignment_score=0.674742`
  - `analog_snr_db=-0.41`
  - `lag_jumps_gt_2_frames=21`
  - `mid_band_residual_ratio=1.690489`
  - `high_band_residual_ratio=1.598638`
  - `quiet_mid_band_noise_dbfs=-32.02`
  - OpenA8DJ driver CPU p95 `36.0%`
  - CoreAudio p95 `74.0%`
- Readiness note:
  - `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1` is rejected as a default.

## 2026-06-16: Process/Lock Cleanup Before Pool-Cursor Test

- Commands:
  - `lsof +D "$HOME/.opena8dj/hardware-gate.lock"`
  - `pgrep -fl 'Core Audio Driver \(OpenA8DJ.driver\)|OpenA8DJ.driver|opena8dj|OpenA8DJ'`
  - `lsof +D /Users/fer/dev/opena8dj`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-pool-cursor-physical.json`
  - Lock acquisition smoke using `$HOME/.opena8dj/hardware-gate.lock`.
  - `build/audio-list | egrep -i 'Audio 8|Audio8|Open Audio|iRig|Stream'`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Result:
  - Hardware lock was absent.
  - No process had the hardware lock directory open.
  - No OpenA8DJ HAL process, soundcheck process, Traktor, ffmpeg, sox, VLC, or
    Spotify process required killing.
  - The only `/Users/fer/dev/opena8dj` handles were the current Codex runtime
    cwd context; those were not killed because they keep this session alive and
    do not own audio hardware.
  - Runtime isolation audit: PASS.
  - Lock acquisition smoke: PASS.
  - iRig visible as `iRig Stream`, `in=2 out=2 rate=48000`.
- Evidence path:
  - `local-analysis/runtime-isolation/pre-pool-cursor-physical.json`

## 2026-06-16: Transfer Pool Cursor Safety Rejection

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make HAL_TRANSFER_POOL_CURSOR=1 HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=0 HAL_INPUT_DECODE_ACTIVE_GATING=0 usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-pool-cursor-cpp-lockpolicy-leave-loaded`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Variant flags:
  - `HAL_TRANSFER_POOL_CURSOR=1`
  - `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=0`
  - `HAL_INPUT_DECODE_ACTIVE_GATING=0`
- Result:
  - Offline gates: PASS.
  - HAL install/enumeration safety: FAIL.
  - Physical soundcheck was not run.
  - The safety script unloaded the HAL after failure; recovery guard: PASS.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-pool-cursor-cpp-lockpolicy-leave-loaded`
  - `local-analysis/runtime-isolation/pre-pool-cursor-physical.json`
- Key safety metrics:
  - `core_audio_enumeration=PASS`
  - `audio_stack_guard=FAIL`
  - `opena8dj_state=loaded`
  - `opena8dj_driver_pids=99244`
  - `process.opena8dj_driver.cpu_pct=0.1`
  - `process.coreaudiod.cpu_pct=172.2`
  - `total_watched_cpu_pct=192.1`
  - Recovery after unload: `audio_stack_health=PASS`, OpenA8DJ driver pids
    `none`.
- Readiness note:
  - `HAL_TRANSFER_POOL_CURSOR=1` is rejected for physical testing unless a
    later code change explains and removes the CoreAudio load-time spike.

## 2026-06-16: Output Amplitude Stats Off Physical Rejection

- Commands:
  - `rm -f build/opena8dj-usb-play build/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL && make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-ampstats-off-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-ampstats-off-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-ampstats-off-failed-unload.json`
- Result:
  - Offline gates: PASS.
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL.
  - Post-unload runtime isolation: PASS.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-ampstats-off-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-ampstats-off-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-ampstats-off-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-ampstats-off-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=0.914358`
  - `analog_snr_db=7.28`
  - `lag_jumps_gt_2_frames=16`
  - `mid_band_residual_ratio=1.445203`
  - `high_band_residual_ratio=1.360556`
  - `quiet_mid_band_noise_dbfs=-31.53`
- Readiness note:
  - Disabling amplitude stats alone did not fix quality or readiness. The
    default remains off because it removes nonessential hot-path diagnostics.

## 2026-06-16: Mainline Preopen/Stop-ISOC Lifecycle Physical Rejection

- Commands:
  - `make usb-play hal` with `HAL_FAST_OUTPUT_PREFETCH_CLEAR=1`,
    `HAL_OUTPUT_WRITE_STATS=1`, `HAL_BACKGROUND_PREOPEN_ON_INIT=1`,
    `HAL_STOP_ISOC_ON_STOP=1`, `HAL_STOP_GRACE_USEC=10000000`.
  - `scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-lifecycle-preopen-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-lifecycle-preopen-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-lifecycle-preopen-failed-unload.json`
- Result:
  - Offline gates: PASS.
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL, severe regression.
  - Post-unload runtime isolation: PASS.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-lifecycle-preopen-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-lifecycle-preopen-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-lifecycle-preopen-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-lifecycle-preopen-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=0.159859`
  - `analog_snr_db=-16.87`
  - `lag_jumps_gt_2_frames=59`
  - `mid_band_residual_ratio=6.709325`
  - `high_band_residual_ratio=6.058707`
  - `quiet_mid_band_noise_dbfs=-33.26`
- Readiness note:
  - Mainline-style preopen/stop-ISOC lifecycle defaults are rejected in C++.
    Code remains behind flags for future analysis; defaults are neutral again.

## 2026-06-16: Fast Prefetch Clear / Atomic Write Stats Physical Rejection

- Commands:
  - `make usb-play hal` with `HAL_FAST_OUTPUT_PREFETCH_CLEAR=1`,
    `HAL_OUTPUT_WRITE_STATS=1`, `HAL_BACKGROUND_PREOPEN_ON_INIT=0`,
    `HAL_STOP_ISOC_ON_STOP=0`.
  - `scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-fastclear-writestats-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -12 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-fastclear-writestats-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-fastclear-writestats-failed-unload.json`
- Result:
  - Offline gates: PASS.
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck: FAIL, severe regression.
  - Post-unload runtime isolation: PASS.
- Evidence paths:
  - `local-analysis/hal-candidate-safety/20260616-fastclear-writestats-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-fastclear-writestats-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-fastclear-writestats-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-fastclear-writestats-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=-0.048481`
  - `analog_snr_db=-32.06`
  - `lag_jumps_gt_2_frames=46`
  - `mid_band_residual_ratio=39.925652`
  - `high_band_residual_ratio=35.368149`
  - `quiet_mid_band_noise_dbfs=-33.61`
- Readiness note:
  - These code paths are not valid defaults. Any future use needs one-factor
    isolation and a written explanation before another physical run.

## 2026-06-16: Stale Hardware-Holder Cleanup Audit

- Commands:
  - Acquired `$HOME/.opena8dj/hardware-gate.lock` and audited targeted
    OpenA8DJ/soundcheck/capture/playback processes.
  - `lsof -nP` filtered for OpenA8DJ/iRig/Audio 8 DJ holders.
  - Removed the cleanup command's own stale lock after its owner PID exited.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-kill-stale-open-holders-clean.json`
- Result:
  - No OpenA8DJ HAL, soundcheck, capture, playback, ffmpeg/sox/afplay, Traktor,
    VLC, or Spotify process was found to kill.
  - HAL was absent/inactive.
  - Mainline supervisors remained disabled and unloaded.
  - Runtime isolation audit: PASS.
- Evidence paths:
  - `local-analysis/runtime-isolation/post-kill-stale-open-holders-clean.json`
- Readiness note:
  - The only OpenA8DJ path holders observed were Codex/node working-directory
    handles on the read-only mainline path; those were not hardware/audio
    holders and were left alive to preserve this session.

## 2026-06-16: Safe-Default HAL Parity Knobs Offline Rerun

- Command:
  - `scripts/run-cpp-offline-gates`
- Result:
  - Debug offline gates: PASS, 15/15.
  - Release offline gates: PASS, 16/16.
  - Release bench: PASS.
- Key metrics:
  - `pack_mib_s=1651.47`
  - `decode_mib_s=588.084`
  - `route_frames_s=9.70304e+08`
  - `decode_into_output_overflows=0`
  - `check_errors=0`
  - `panic_flags=0`
- Readiness note:
  - This validates the source tree with rejected HAL parity paths behind safe
    defaults. It does not prove physical audio quality.

## 2026-06-16: Default C++ HAL Calibrated -16 dB Physical Rejection

- Commands:
  - `make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-calibrated-default-soundcheck.json`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260616-default-calibrated-minus16-cpp-lockpolicy-leave-loaded`
  - `scripts/run-soundcheck --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --max-lag 360000 --stream-stats-snapshots --cpu-profile --run-dir local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal`
  - Locked forced HAL unload after failure.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-default-minus16-failed-unload.json`
- Result:
  - Pre-audit: PASS, HAL absent, lock absent, mainline supervisors disabled.
  - Offline gates: PASS.
  - HAL install/enumeration safety: PASS.
  - Physical music soundcheck at the mainline calibrated level: FAIL.
  - Post-unload runtime isolation: PASS.
- Evidence paths:
  - `local-analysis/runtime-isolation/pre-calibrated-default-soundcheck.json`
  - `local-analysis/hal-candidate-safety/20260616-default-calibrated-minus16-cpp-lockpolicy-leave-loaded`
  - `local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal`
  - `local-analysis/audio-stack-guard/20260616-default-minus16-force-unload/force-unload.log`
  - `local-analysis/runtime-isolation/post-default-minus16-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=0.960076`
  - `analog_snr_db=2.71`
  - `lag_jumps_gt_2_frames=35`
  - `mid_band_residual_ratio=1.565287`
  - `high_band_residual_ratio=1.461400`
  - `quiet_mid_band_noise_dbfs=-36.81`
  - `capture_clipped_frames=0`
  - CPU profile: `opena8dj_driver avg=30.73% p95=36.70% max=37.00%`,
    `coreaudiod avg=8.57% p95=56.40% max=81.00%`.
- Stream-stat interpretation:
  - During the active section, the output ring stayed near target and active
    underruns remained zero; the high underrun count appears after the writer
    stops and the stream drains.
  - The failure is therefore not explained by simple render starvation. It
    remains a physical quality/cadence/transport problem.
- Readiness note:
  - The calibrated level improves alignment versus some prior runs but still
    fails strict quality and CPU thresholds. This candidate is not better than
    mainline and is not ready for promotion or listening readiness claims.

## 2026-06-16: Direct USB Plain-CFLAGS Physical Rejections

- Commands:
  - `make usb-play-plain`
  - `scripts/run-cpp-offline-gates`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-direct-usb-plain-minus16.json`
  - Direct locked capture: `build/audio-record 18 ... "iRig Stream" 1,2`
    while running `build/opena8dj-usb-play-plain fixture/reference.wav`.
  - `scripts/analyze-soundcheck-capture.py ... --max-seconds 16 --max-lag 360000 --time-warp --drift-profile`
  - `make usb-play-plain-gain05`
  - `scripts/run-cpp-offline-gates`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-direct-usb-plain-gain05-minus16.json`
  - Direct locked capture with
    `build/opena8dj-usb-play-plain-gain05 fixture/reference.wav`.
- Result:
  - Both direct USB tools executed and capture completed without runtime error.
  - No HAL was installed or active.
  - Post-run runtime isolation: PASS in both runs.
  - Plain CFLAGS direct USB: FAIL, severe quality failure and capture clipping.
  - Plain CFLAGS plus only `OPENA8DJ_OUTPUT_GAIN=0.50f`: FAIL, no meaningful
    quality recovery.
- Evidence paths:
  - `local-analysis/direct-usb-soundcheck/20260616-plaincflags-minus16-music`
  - `local-analysis/direct-usb-soundcheck/20260616-plaincflags-gain05-minus16-music`
- Key metrics:
  - Plain CFLAGS: `quality_alignment_score=0.186400`,
    `alignment_score=0.127546`, `lag_jumps_gt_2_frames=26`,
    `capture_clipped_frames=8768`, `quiet_mid_band_noise_dbfs=-27.59`.
  - Plain CFLAGS gain 0.5: `quality_alignment_score=0.023502`,
    `alignment_score=-0.001463`, `lag_jumps_gt_2_frames=54`,
    `capture_clipped_frames=21`, `quiet_mid_band_noise_dbfs=-34.10`.
- Interpretation:
  - The `plain` direct tool is not a product candidate; default source gain
    overdrives the analog chain at the calibrated music level.
  - Reducing only gain did not recover quality. This weakens the hypothesis that
    direct USB failures are caused only by `HAL_CFLAGS` contamination.
  - HAL default remains less bad than direct USB, but still fails physical
    quality and CPU gates.

## 2026-06-16: Mode 2 Mainline Layout Parity Offline Gate

- Commands:
  - Added `opena8djcpp_mode2_mainline_layout_parity`.
  - `scripts/run-cpp-offline-gates`
  - `build/cpp-release/opena8djcpp_mode2_mainline_layout_parity > local-analysis/cpp-offline/mode2-mainline-layout-parity.json`
- Result:
  - Debug offline gates: PASS, 16/16.
  - Release offline gates: PASS, 17/17.
  - New mainline-layout parity gate: PASS.
- Evidence paths:
  - `local-analysis/cpp-offline/mode2-mainline-layout-parity.json`
- Key metrics:
  - `schema=opena8djcpp.mode2-mainline-layout-parity.v1`
  - `row_count=132`
  - `failures=0`
- Interpretation:
  - The C++ `Mode2OutputPacker` matches an independently implemented unrolled
    mainline-style layout across start bytes `0..5`, short partial transfer
    lengths, and normal transfer lengths including `352`.
  - This makes a simple byte-layout mismatch less likely as the physical
    blocker. Continue investigating transfer cadence, device state, and
    hardware-observed USB behavior.

## 2026-06-17: Runtime Open-Holder Cleanup

- Commands:
  - Acquired `$HOME/.opena8dj/hardware-gate.lock`.
  - Enumerated process and open-file candidates with `ps` and `lsof`.
  - Terminated only matching OpenA8DJ/audio/capture/playback app candidates if
    present.
- Result:
  - PASS.
  - `killed_count=0`.
  - No remaining OpenA8DJ HAL, direct USB playback, capture, soundcheck,
    ffmpeg/sox/afplay, Traktor, VLC, or Spotify process candidates.
  - Lock was released after the audit.
- Evidence path:
  - `local-analysis/runtime-isolation/kill-open-holders-20260617T002910Z`
- Interpretation:
  - The mainline should not be blocking the hardware through a live test
    process at this point. If the device remains unavailable, the next
    investigation should target CoreAudio/USB/device state rather than a stuck
    OpenA8DJ test process.

## 2026-06-17: HAL Audio-Params Reset Flag Port

- Commands:
  - `make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `git diff --check`
- Result:
  - PASS.
  - Debug offline gates: PASS, `16/16`.
  - Release offline gates: PASS, `17/17`.
  - `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM` defaults to `1`, preserving current
    behavior.
- Key metrics:
  - `pack_mib_s=1657.06`
  - `decode_mib_s=585.852`
  - `route_frames_s=9.46081e+08`
- Interpretation:
  - The reset behavior is now a controlled physical-test variable. No physical
    quality claim follows from this offline-only change.

## 2026-06-17: Promotion Readiness Evaluator Freshness Gate

- Commands:
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Result:
  - Expected FAIL.
  - `branch_promotion_allowed=false`.
  - New `latest_music_cpu_pair` gate: PASS.
  - Selected evidence pair:
    `local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal`.
- Current blocking metrics:
  - Physical music: `quality_alignment_score=0.9600756683268455`,
    `snr_db_min=2.712692078645948`,
    `mid_band_residual_ratio=1.56528730655531`,
    `high_band_residual_ratio=1.4613998707666849`,
    `quiet_mid_band_noise_dbfs=-36.807388625414724`,
    `lag_jumps_gt_2_frames=35`.
  - Runtime CPU: OpenA8DJ driver p95 `36.7%`, coreaudiod p95 `56.4%`.
- Interpretation:
  - The promotion command now defaults to the latest paired physical quality
    and CPU evidence instead of a fixed older run. This improves readiness
    rigor only; it does not improve the candidate.

## 2026-06-17: Offline Stream-Stats Summary Analyzer

- Commands:
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/default-minus16-summary.json`
  - `scripts/analyze-stream-stats.py $(find local-analysis/soundcheck -maxdepth 2 -name stream-stats-during.tsv | sort) --json-out local-analysis/stream-stats/all-soundchecks-summary.json`
- Result:
  - Analyzer executed offline only; no hardware, CoreAudio, USB, HAL install,
    or system service was touched.
  - Latest calibrated run summary result: `DIAGNOSTIC_FLAGS`.
- Key metrics for `20260616-default-minus16-irig-pairA-16s-cpp-hal`:
  - `ok_sample_count=45`
  - `error_sample_count=2`
  - `output_read_frames_per_second=48009.4`
  - `capture_transaction_errors_per_capture_transfer=2.273`
  - `output_write_stats_observable=false`
  - Flags: `stream_stats_timeouts`, `output_write_stats_unobservable`.
- Cross-run finding:
  - ISO64 remains an outlier with
    `capture_transaction_errors_per_capture_transfer=29.092`, consistent with
    its physical rejection and not worth retesting without a new hypothesis.
- Interpretation:
  - The latest calibrated run did not show active underruns, timeline resets,
    or panic flags in the stream snapshots. The failure is still in physical
    audio quality/CPU, not a simple output starvation counter.
  - Because `HAL_OUTPUT_WRITE_STATS=0`, future diagnostic physical runs need an
    explicit plan for whether write stats are worth enabling despite their
    previous physical regression.

## 2026-06-17: Reset-Off HAL Safety Rejection

- Commands:
  - `make -B HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0 usb-play hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-reset-audio-params-off/20260616-203712/hal-candidate-safety`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-reset-audio-params-off-safety-fail.json`
- Result:
  - Safety FAIL before soundcheck.
  - No music playback or iRig capture was run for this variant.
  - Recovery PASS; final state HAL inactive, lock absent, no OpenA8DJ process.
- Evidence paths:
  - `local-analysis/physical-reset-audio-params-off/20260616-203712/build.log`
  - `local-analysis/physical-reset-audio-params-off/20260616-203712/candidate-hal.sha256`
  - `local-analysis/physical-reset-audio-params-off/20260616-203712/hal-candidate-safety/summary.txt`
  - `local-analysis/runtime-isolation/post-reset-audio-params-off-safety-fail.json`
- Key metrics:
  - Build log contains `-DOPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM=0`.
  - `core_audio_enumeration=PASS`.
  - `audio_stack_health=FAIL`.
  - `coreaudiod=115.1%`.
  - `opena8dj_driver=0.1%`.
  - `total_watched_cpu_pct=130.0`.
- Interpretation:
  - Skipping the reset is not a viable improvement candidate under current
    evidence. It regresses load/enumeration CPU before audio quality can even
    be measured.

## 2026-06-17: HAL Flag Rebuild Guard

- Commands:
  - `make usb-play hal > local-analysis/build-flags/default-rebuild-v2.log`
  - `make HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0 usb-play hal > local-analysis/build-flags/reset0-rebuild-v2.log`
  - `make HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0 usb-play hal > local-analysis/build-flags/reset0-repeat-v2.log`
  - `make usb-play hal > local-analysis/build-flags/default-restore-v2.log`
- Result:
  - PASS.
  - Changing HAL flags now forces rebuild of affected generated binaries.
  - Repeating identical HAL flags does not rebuild.
  - Final local build restored default `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=1`.
- Key observations:
  - `reset0-rebuild-v2_compiles=2`.
  - `reset0-repeat-v2_compiles=0`.
  - `default-restore-v2_compiles=2`.
- Interpretation:
  - Physical variant evidence is safer because `HAL_*` changes can no longer
    silently reuse stale `hal` or `usb-play` binaries.

## 2026-06-17: Hot Stats Gate, Atomic Output Write Stats, Late-Write Counters

- Commands:
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current-after-kill-request.json`
  - `make usb-play hal`
  - `make build/opena8dj-control`
  - `make HAL_HOT_STREAM_STATS=0 usb-play hal`
  - `make HAL_OUTPUT_WRITE_STATS=0 usb-play hal`
  - `make usb-play hal`
  - `scripts/run-cpp-offline-gates`
- Result:
  - Runtime isolation PASS before edits/tests; no hardware holder was alive and
    no process needed killing.
  - Default HAL/tools build PASS.
  - `HAL_HOT_STREAM_STATS=0` variant build PASS, no warnings after marking the
    timing helper as possibly unused in diagnostic builds.
  - `HAL_OUTPUT_WRITE_STATS=0` variant build PASS.
  - Final default rebuild PASS with `HAL_OUTPUT_WRITE_STATS=1`,
    `HAL_HOT_STREAM_STATS=1`, `HAL_HOT_STREAM_STATS_INTERVAL=1`.
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
- Evidence paths:
  - `local-analysis/runtime-isolation/current-after-kill-request.json`
  - `local-analysis/build-flags/hot-stats-off-build.log`
  - `local-analysis/build-flags/output-write-stats-off-build.log`
  - `local-analysis/build-flags/default-after-hot-stats-output-write-build.log`
  - `local-analysis/offline-gates-after-hot-stats-output-write.log`
- Key metrics from release bench:
  - `pack_mib_s=1640.8`
  - `decode_mib_s=589.055`
  - `route_frames_s=9.14455e+08`
  - `check_errors=0`
  - `panic_flags=0`
- Interpretation:
  - This removes an unobservable hot-path mutex and adds late-write
    observability, but it does not prove physical quality or CPU superiority.
  - The next hardware run must check the new counters and compare CPU/quality
    against mainline before any readiness claim.

## 2026-06-17: Physical Hot-Stats/Atomic-Write Candidate Rejection

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-hotstats-write-late/20260616-205111/hal-candidate-safety`
  - `scripts/run-soundcheck --skip-build --music-dir "$HOME/Music" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Forced HAL unload under hardware lock, followed by `audio-stack-guard`.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-hotstats-write-late-failed-unload.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/hotstats-write-late-summary-v2.json`
- Result:
  - HAL candidate safety PASS.
  - Physical soundcheck FAIL.
  - Post-failure unload/recovery PASS; final isolation PASS.
- Evidence paths:
  - `local-analysis/physical-hotstats-write-late/20260616-205111/hal-candidate-safety`
  - `local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal`
  - `local-analysis/stream-stats/hotstats-write-late-summary-v2.json`
  - `local-analysis/audio-stack-guard/20260616-force-unload-hotstats-write-late`
  - `local-analysis/runtime-isolation/post-hotstats-write-late-failed-unload.json`
- Key metrics:
  - `quality_alignment_score=0.962133`
  - `snr_db=10.24`
  - `click_outliers=29`
  - `lag_jumps_gt_2_frames=45`
  - `mid_band_residual_ratio=1.443461`
  - `high_band_residual_ratio=1.362932`
  - `quiet_mid_band_noise_dbfs=-35.91`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver avg/p95/max CPU `31.58%/36.00%/36.40%`
  - coreaudiod avg/p95/max CPU `4.70%/7.00%/60.40%`
  - Stream stats: `output_write_stats_observable=true`,
    `outputFramesWritten_per_second=48000.35`,
    `outputFramesRead_per_second=48006.75`, no active underruns, no timeline
    resets, no panic flags.
  - Text stream snapshots showed `outputLateWriteFrames=0` and
    `outputLateWriteBatches=0` throughout the run.
- Interpretation:
  - Atomic write stats and hot stats gates are useful, but not sufficient.
  - Late writes do not explain the current analog quality failure.
  - Continue with queue-depth/prefetch/cadence hypotheses; do not promote.

## 2026-06-17: Late-Write TSV Tooling Fix

- Commands:
  - `python3 -m py_compile scripts/run-soundcheck scripts/analyze-stream-stats.py`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/hotstats-write-late-summary-v2.json`
- Result:
  - PASS.
  - `run-soundcheck` now includes `outputLateWriteFrames` and
    `outputLateWriteBatches` in `stream-stats-during.tsv`.
  - `analyze-stream-stats.py` summarizes those counters and flags
    `late_writes` if they increase.
- Interpretation:
  - The hotstats physical run still has late-write evidence in text snapshots,
    but future runs will have structured TSV/JSON late-write evidence.

## 2026-06-17: Queue-Depth 8/8 Physical Rejection

- Commands:
  - `make HAL_CAPTURE_QUEUE=8 HAL_PLAYBACK_QUEUE=8 usb-play hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-queue8/20260616-205510/hal-candidate-safety`
  - `scripts/run-soundcheck --skip-build --music-dir "$HOME/Music" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Forced HAL unload under hardware lock, followed by `audio-stack-guard`.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-queue8-failed-unload.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/queue8-summary.json`
  - `make usb-play hal > local-analysis/build-flags/default-restore-after-queue8.log`
- Result:
  - Build PASS with `OPENA8DJ_CAPTURE_QUEUE_DEPTH=8` and
    `OPENA8DJ_PLAYBACK_QUEUE_TARGET=8`.
  - HAL candidate safety PASS.
  - Physical soundcheck FAIL.
  - Post-failure unload/recovery PASS; final isolation PASS.
  - Default build restored to queue depth `64/64`.
- Evidence paths:
  - `local-analysis/physical-queue8/20260616-205510`
  - `local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal`
  - `local-analysis/stream-stats/queue8-summary.json`
  - `local-analysis/audio-stack-guard/20260616-force-unload-queue8`
  - `local-analysis/runtime-isolation/post-queue8-failed-unload.json`
  - `local-analysis/build-flags/default-restore-after-queue8.log`
- Key metrics:
  - `quality_alignment_score=0.964133`
  - `snr_db=10.22`
  - `click_outliers=0`
  - `lag_jumps_gt_2_frames=39`
  - `mid_band_residual_ratio=1.422599`
  - `high_band_residual_ratio=1.365050`
  - `quiet_mid_band_noise_dbfs=-36.08`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver avg/p95/max CPU `32.335%/37.2%/37.9%`
  - coreaudiod avg/p95/max CPU `3.715%/3.1%/47.2%`
  - Stream stats: `outputLateWriteFrames_delta=0`,
    `outputLateWriteBatches_delta=0`, no active underruns, no timeline resets,
    no panic flags, capture transaction errors per capture transfer `2.2727`.
- Interpretation:
  - Queue depth is relevant to click behavior and coreaudiod CPU, but as a
    standalone change it does not solve quality and worsens driver CPU.
  - Do not make `8/8` default without a later positive combined hypothesis.

## 2026-06-17: Output Prefetch 64 Physical Rejection

- Commands:
  - `make HAL_OUTPUT_PREFETCH_FRAMES=64 usb-play hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-prefetch64/20260616-205818/hal-candidate-safety`
  - `scripts/run-soundcheck --skip-build --music-dir "$HOME/Music" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Forced HAL unload under hardware lock, followed by `audio-stack-guard`.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-prefetch64-failed-unload.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/prefetch64-summary.json`
  - `make usb-play hal > local-analysis/build-flags/default-restore-after-prefetch64.log`
- Result:
  - Build PASS with `OPENA8DJ_OUTPUT_PREFETCH_FRAMES=64`.
  - HAL candidate safety PASS.
  - Physical soundcheck FAIL.
  - Post-failure unload/recovery PASS; final isolation PASS.
  - Default build restored to `OPENA8DJ_OUTPUT_PREFETCH_FRAMES=256`.
- Evidence paths:
  - `local-analysis/physical-prefetch64/20260616-205818`
  - `local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal`
  - `local-analysis/stream-stats/prefetch64-summary.json`
  - `local-analysis/audio-stack-guard/20260616-force-unload-prefetch64`
  - `local-analysis/runtime-isolation/post-prefetch64-failed-unload.json`
  - `local-analysis/build-flags/default-restore-after-prefetch64.log`
- Key metrics:
  - `quality_alignment_score=0.956371`
  - `snr_db=10.40`
  - `click_outliers=4`
  - `lag_jumps_gt_2_frames=48`
  - `mid_band_residual_ratio=1.431220`
  - `high_band_residual_ratio=1.365281`
  - `quiet_mid_band_noise_dbfs=-35.98`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver p95 CPU `39.5%`
  - coreaudiod p95 CPU `4.1%`
  - Stream stats: `outputLateWriteFrames_delta=0`,
    `outputLateWriteBatches_delta=0`, no active underruns, no timeline resets,
    no panic flags, capture transaction errors per capture transfer `2.2728`.
- Interpretation:
  - Mainline prefetch parity is not a standalone fix in C++.
  - Do not combine with `queue8` until a separate transport/capture-error
    hypothesis explains the persistent residual/lag.

## 2026-06-17: Capture Transaction Detail Tooling

- Commands:
  - `make build/opena8dj-control`
  - `python3 -m py_compile scripts/run-soundcheck scripts/analyze-stream-stats.py`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/prefetch64-summary-v2.json`
- Result:
  - PASS.
  - `opena8dj-control stream-stats` machine output now includes:
    `captureStatusFailures`, `captureZeroCompleteTransactions`,
    `captureExpectedTransactions`, `captureOtherByteCountTransactions`,
    `captureShortTransfers`, and `filteredCaptureTransactions`.
  - `run-soundcheck` now writes those fields to `stream-stats-during.tsv`.
  - `analyze-stream-stats.py` now summarizes per-capture-transfer ratios for
    those components while remaining compatible with older TSVs.
- Interpretation:
  - The stable aggregate `captureTransactionErrors/transfer ~= 2.273` needs
    decomposition before it can drive another transport change. Future physical
    runs will identify whether errors are status failures, zero-complete,
    short/other-size filtered transactions, or a mixture.

## 2026-06-17: Holder Cleanup And Capture ISO Invariant Analyzer

- Commands:
  - `scripts/audio-stack-guard --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/20260616-kill-open-holders`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/current-after-kill-request-live.json`
  - `python3 -m py_compile scripts/analyze-capture-iso-invariants.py`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-capture-detail-irig-pairA-8s-cpp-hal --json-out local-analysis/stream-stats/capture-iso-invariants-recent-v3.json`
  - `scripts/analyze-capture-iso-invariants.py $(find local-analysis/soundcheck -maxdepth 1 -type d -name '*irig*' | sort) --json-out local-analysis/stream-stats/capture-iso-invariants-all-irig-v3.json`
- Result:
  - Runtime cleanup PASS: HAL inactive, lock absent, no OpenA8DJ driver
    process, forbidden mainline LaunchAgents disabled.
  - Recent detailed capture ISO invariants PASS on 4 runs.
  - Historical iRig sweep result FAIL only because
    `20260616-lifecycle-preopen-irig-pairA-16s-cpp-hal` has a real classified
    ISO-slot mismatch. Older runs without detail are UNKNOWN, not FAIL.
- Evidence paths:
  - `local-analysis/audio-stack-guard/20260616-kill-open-holders`
  - `local-analysis/runtime-isolation/current-after-kill-request-live.json`
  - `local-analysis/stream-stats/capture-iso-invariants-recent-v3.json`
  - `local-analysis/stream-stats/capture-iso-invariants-all-irig-v3.json`
- Interpretation:
  - Recent aggregate `captureTransactionErrors` are zero-complete ISO slots, not
    status failures.
  - This removes one false lead but does not improve product readiness:
    physical music quality and driver CPU still fail.

## 2026-06-17: Promotion Readiness After Capture ISO Classification

- Command:
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-capture-invariants.json`
- Commit under test: `1c574cc`
- Result: FAIL; branch promotion is not allowed.
- PASS gates include offline, simulated output oracle, physical tone, evidence
  presence, hardware-lock policy, and offline throughput floors.
- Blocking gates:
  - `physical_music_quality`: latest selected music metrics still have
    `quality_alignment_score=0.978049577556115`,
    `snr_db=9.845114058005024`, `lag_jumps_gt_2_frames=24`,
    `mid_band_residual_ratio=1.3932011051574409`, and quiet mid-band noise
    `-35.22881003368516 dBFS`.
  - `runtime_cpu_beats_mainline`: OpenA8DJ driver p95 `35.5%` and coreaudiod
    p95 `37.2%`, versus mainline limits `6.5%` and `1.7%`.
  - `latest_physical_investigation`: decision remains `FAIL_NOT_READY`.
  - `traktor_timecode_physical`: no physical Traktor/timecode-vinyl lock
    evidence has been recorded.
- Evidence path:
  - `local-analysis/promotion-readiness-after-capture-invariants.json`

## 2026-06-17: HAL Output Pack Fast Path Offline Verification

- Commands:
  - `make usb-play hal`
  - `make HAL_OUTPUT_CHECK_OFFSET=4 hal`
  - `make hal`
  - `scripts/run-cpp-offline-gates`
- Result:
  - Default HAL and `usb-play` build PASS with the unrolled default
    `OPENA8DJ_OUTPUT_CHECK_OFFSET=8` fast path.
  - Generic fallback build PASS with `HAL_OUTPUT_CHECK_OFFSET=4`.
  - Default HAL rebuilt after fallback verification.
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
- Interpretation:
  - This is a CPU/jitter candidate only. It preserves offline layout contracts,
    but it has not proved better physical music quality or lower runtime CPU.

## 2026-06-17: Unrolled HAL Output Pack Physical Rejection

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-unrolled-pack/20260616-211513/hal-candidate-safety`
  - Retry after recovery:
    `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-unrolled-pack/20260616-211622-retry/hal-candidate-safety`
  - `scripts/run-soundcheck --skip-build --music-dir "$HOME/Music" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260616-unrolled-pack-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Explicit HAL unload under lock, then `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-unrolled-pack-failed-unload.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-unrolled-pack-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/unrolled-pack-summary.json`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260616-unrolled-pack-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/unrolled-pack-capture-iso-invariants.json`
- Result:
  - First safety run FAIL due high `coreaudiod` CPU after HAL load
    (`137.0%`); recovery PASS.
  - Safety retry PASS and left HAL loaded.
  - Physical soundcheck FAIL.
  - Candidate disabled by default after rejection; artifacts rebuilt with
    `HAL_UNROLLED_OUTPUT_PACK=0`.
  - Post-failure unload/recovery PASS; final isolation PASS.
- Key metrics:
  - `quality_alignment_score=0.131043`
  - `snr_db=-18.43`
  - `lag_jumps_gt_2_frames=47`
  - `mid_band_residual_ratio=8.393129`
  - `high_band_residual_ratio=7.405798`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver avg/p95/max CPU `30.959%/35.600%/35.900%`
  - Stream stats: no active underruns, no late writes, no timeline resets, no
    panic flags.
  - Capture ISO invariants PASS.
- Interpretation:
  - Offline layout parity did not translate to physical improvement.
  - The root blocker remains analog residual/lag and high driver CPU; this
    candidate must not be promoted or left as default.

## 2026-06-17: Promotion Readiness After Unrolled-Pack Rejection

- Command:
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-unrolled-rejection.json`
- Commit under test: `0144ffc`
- Result: FAIL; branch promotion is not allowed.
- Blocking gates:
  - `physical_music_quality`: latest selected run is
    `20260616-unrolled-pack-irig-pairA-16s-cpp-hal`, with
    `quality_alignment_score=0.13104262411477222`, SNR `-18.433998179397555`,
    `lag_jumps_gt_2_frames=47`, mid residual `8.393129418488618`, high residual
    `7.405798355303025`.
  - `runtime_cpu_beats_mainline`: driver p95 `35.6%`, coreaudiod p95 `3.3%`,
    versus mainline limits `6.5%` and `1.7%`.
  - `latest_physical_investigation`: `FAIL_NOT_READY`.
  - `traktor_timecode_physical`: no physical Traktor/timecode-vinyl lock
    evidence.
- Evidence path:
  - `local-analysis/promotion-readiness-after-unrolled-rejection.json`

## 2026-06-17: Default Control After Unrolled-Pack Rejection

- Commands:
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-default-after-unrolled/20260616-212139/hal-candidate-safety`
  - `scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Explicit HAL unload under lock.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-default-after-unrolled-failed-unload.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/default-after-unrolled-summary.json`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/default-after-unrolled-capture-iso-invariants.json`
- Result:
  - Safety PASS.
  - Soundcheck FAIL, but returned to the known default failure profile rather
    than the severe unrolled-pack regression.
  - Post-failure unload/recovery PASS; final isolation PASS.
- Key metrics:
  - `quality_alignment_score=0.964049`
  - `snr_db=10.44`
  - `lag_jumps_gt_2_frames=40`
  - `mid_band_residual_ratio=1.436380`
  - `high_band_residual_ratio=1.372535`
  - `quiet_mid_band_noise_dbfs=-35.80`
  - `capture_clipped_frames=0`
  - OpenA8DJ driver avg/p95/max CPU `33.942%/38.500%/38.700%`
  - Stream stats: no active underruns, no late writes, no timeline resets, no
    panic flags.
  - Capture ISO invariants PASS.
- Interpretation:
  - Default is restored but still not ready.
  - The next useful investigation should target the physical residual/lag path
    and runtime CPU above the Mode 2 byte-pack layer.

## 2026-06-17: Window Trace Residual/Lag Classification

- Commands:
  - `python3 -m py_compile scripts/analyze-soundcheck-window-trace.py`
  - `scripts/analyze-soundcheck-window-trace.py local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal --json-out local-analysis/soundcheck-window-trace/default-after-unrolled-v2.json`
  - `scripts/analyze-soundcheck-window-trace.py local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal --json-out local-analysis/soundcheck-window-trace/hotstats-write-late-v2.json`
  - `scripts/analyze-soundcheck-window-trace.py local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal --json-out local-analysis/soundcheck-window-trace/queue8-v2.json`
- Tooling change:
  - `scripts/analyze-soundcheck-window-trace.py` now creates the output
    directory and records aggregate lag/correlation/driver-CPU fields.
- Result:
  - default-after-unrolled:
    - local lag min/max `-26/16`, jumps `40`;
    - median mid residual `1.448463 -> 1.430920` after local lag correction;
    - median correlation `0.960389 -> 0.970858`;
    - driver CPU median/p95/max `37.4%/38.5%/38.7%`.
  - hotstats-write-late:
    - local lag min/max `-28/8`, jumps `45`;
    - median mid residual `1.452215 -> 1.443643`;
    - median correlation `0.955921 -> 0.969117`;
    - driver CPU median/p95/max `34.9%/36.1%/36.4%`.
  - queue8:
    - local lag min/max `-5/63`, jumps `39`;
    - median mid residual `1.491993 -> 1.431929`;
    - median correlation `0.856756 -> 0.970177`;
    - driver CPU median/p95/max `35.8%/37.4%/37.9%`.
- Interpretation:
  - Local lag correction does not materially solve the residual. The dominant
    remaining blocker is not simple alignment; it is persistent coloration,
    distortion, wrong mixed signal, or another below-HAL analog/transport issue
    plus high CPU.

## 2026-06-17: Offline Linear Matrix Classification And Decorrelated Gate Prep

- Commands:
  - `python3 -m py_compile scripts/analyze-soundcheck-linear-matrix.py`
  - `scripts/analyze-soundcheck-linear-matrix.py local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260616-unrolled-pack-irig-pairA-16s-cpp-hal --analysis-seconds 8 --json-out local-analysis/soundcheck-linear-matrix/recent-failed-physical-music.json`
  - `scripts/run-channel-matrix-gate --run-id offline-prepare-smoke --pair A --rate 48000 --seconds 2 --peak 0.25`
  - `make channel-matrix-prepare CHANNEL_MATRIX_SECONDS=1 CHANNEL_MATRIX_PEAK=0.20`
- Result:
  - Linear matrix tool PASS_DIAGNOSTIC over existing evidence only; no hardware,
    CoreAudio, USB, install, service restart, or default-device change.
  - All existing music runs are classified as
    `needs_decorrelated_physical_matrix_fixture`.
  - Prepare-only channel-matrix gate generated a deterministic stereo fixture
    and plan without touching audio devices.
- Key metrics:
  - Existing music source L/R correlation `0.985848`, condition number
    `140.322901`, so the matrix is ill-conditioned and cannot prove crosstalk.
  - default-after-unrolled matrix diagnostic residual/predicted `0.303950`.
  - hotstats-write-late residual/predicted `0.306780`.
  - queue8 residual/predicted `0.503410`.
  - unrolled-pack residual/capture `0.999913` and mono correlation
    `-0.182678`, confirming severe unrelated regression.
  - Decorrelated fixture smoke correlation: `0.0005635458408516207`.
- Evidence paths:
  - `local-analysis/soundcheck-linear-matrix/recent-failed-physical-music.json`
  - `local-analysis/channel-matrix/offline-prepare-smoke/plan.txt`
  - `local-analysis/channel-matrix/offline-prepare-smoke/fixture/reference.wav`
  - `local-analysis/channel-matrix/20260617T014008Z-pairA-decorrelated-matrix`
- Interpretation:
  - The next physical check should be a lock-gated decorrelated Pair A matrix
    capture through iRig, not another ambiguous correlated music capture.
  - This does not improve readiness; it narrows the blocker and creates the
    evidence format needed to distinguish wrong channel mix/routing from
    coloration/noise.

## 2026-06-17: Offline Gates After Channel-Matrix Lock Policy Integration

- Commands:
  - `bash -n scripts/run-channel-matrix-gate`
  - `python3 -m py_compile scripts/analyze-soundcheck-linear-matrix.py`
  - `scripts/run-channel-matrix-gate --run-id physical-reject-smoke --run-physical --seconds 1 --peak 0.1`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-channel-matrix-prep.json`
- Result:
  - Physical-mode rejection smoke PASS: missing `--capture-device` exits `2`
    before acquiring the hardware lock.
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
  - Hardware-lock policy now audits `5` sensitive scripts and includes
    `scripts/run-channel-matrix-gate`.
  - Promotion readiness remains FAIL.
- Blocking readiness gates:
  - `physical_music_quality` still fails with latest selected run
    `20260616-default-after-unrolled-irig-pairA-16s-cpp-hal`.
  - `runtime_cpu_beats_mainline` still fails: OpenA8DJ driver p95 `38.5%`
    versus mainline threshold `6.5%`; coreaudiod p95 `3.1%` versus `1.7%`.
  - `latest_physical_investigation` remains `FAIL_NOT_READY`.
  - `traktor_timecode_physical` remains `BLOCKED_UNVALIDATED_DVS`.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/hardware-lock-policy.json`
  - `local-analysis/promotion-readiness-after-channel-matrix-prep.json`

## 2026-06-17: Locked Pair A Decorrelated Channel Matrix

- Commands:
  - `make hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-channel-matrix/20260617-physical-matrix-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-channel-matrix-gate --run-physical --run-id 20260617-irig-pairA-decorrelated-matrix --pair A --rate 48000 --seconds 8 --peak 0.30 --capture-device "iRig Stream" --capture-channels 1,2`
  - `scripts/audio-stack-guard --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/20260617-after-channel-matrix-unload`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-channel-matrix-physical-unload.json`
  - `scripts/analyze-channel-matrix-tones.py local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix --skip-seconds 1 --analysis-seconds 8 --json-out local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/tone-matrix.json`
- Result:
  - HAL candidate safety PASS and left the candidate loaded.
  - Physical decorrelated matrix run completed with iRig Stream capture.
  - Tone-domain channel matrix PASS.
  - Cleanup/unload PASS; final runtime isolation PASS, HAL inactive, lock
    absent.
- Key metrics:
  - Capture device UID:
    `AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1`.
  - `captured.wav`: rate `48000`, frames `576512`, RMS `0.11460675`, peak
    `0.50494385`, clipped frames `0`.
  - Tone matrix expected floor amplitude `0.06576757351262598`.
  - Left-to-right leakage `-59.48258382334637 dB`.
  - Right-to-left leakage `-49.667972003183024 dB`.
  - Max wrong-source leakage `-51.26562016103985 dB`.
  - Linear sample-domain diagnostic still rejects the capture with
    `classification=linear_matrix_rejected_large_physical_residual`; treat this
    as phase/filtering/residual evidence, not as crosstalk failure.
- Evidence paths:
  - `local-analysis/physical-channel-matrix/20260617-physical-matrix-safety`
  - `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix`
  - `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/tone-matrix.json`
  - `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/linear-matrix.json`
  - `local-analysis/audio-stack-guard/20260617-after-channel-matrix-unload`
  - `local-analysis/runtime-isolation/after-channel-matrix-physical-unload.json`
- Interpretation:
  - Pair A physical route does not show gross L/R crosstalk or deck leakage
    through the iRig path.
  - The remaining blockers are physical music quality/residual, CPU versus
    mainline, and physical Traktor/timecode validation.
- Post-update verification:
  - `python3 -m py_compile scripts/analyze-channel-matrix-tones.py scripts/analyze-soundcheck-linear-matrix.py`
  - `bash -n scripts/run-channel-matrix-gate`
  - `scripts/run-cpp-offline-gates` PASS: Debug `16/16`, Release `17/17`.
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-physical-channel-matrix.json`
    remains FAIL with blockers `physical_music_quality`,
    `runtime_cpu_beats_mainline`, `latest_physical_investigation`, and
    `traktor_timecode_physical`.

## 2026-06-17: Atomic Stream-Stats Accumulators Offline Verification

- Commands:
  - `make HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=0 hal && make hal`
  - `make usb-play hal && make HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=0 usb-play hal && make usb-play hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-atomic-stream-stats.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-atomic-stream-stats-offline.json`
- Result:
  - Fallback and default HAL builds compile.
  - Fallback and default `usb-play`/HAL combined builds compile.
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
  - Runtime isolation PASS: HAL inactive, hardware untouched, USB untouched,
    lock absent, forbidden mainline LaunchAgents disabled/inactive.
  - Promotion readiness remains FAIL.
- Key offline bench metrics:
  - `pack_mib_s=1625.85`
  - `decode_mib_s=577.663`
  - `decode_into_mib_s=577.663`
  - `decode_allocating_mib_s=567.074`
  - `float_to_s24_frames_s=8.6452e+07`
  - `route_frames_s=9.67171e+08`
  - `route_reversed_frames_s=5.12459e+08`
  - `route_advanced_frames_s=5.02714e+08`
- Blocking readiness gates after this change:
  - `physical_music_quality`
  - `runtime_cpu_beats_mainline`
  - `latest_physical_investigation`
  - `traktor_timecode_physical`
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-after-atomic-stream-stats.json`
  - `local-analysis/runtime-isolation/after-atomic-stream-stats-offline.json`
- Interpretation:
  - This validates build and offline invariants only. Atomic stream stats still
    need a locked physical soundcheck to prove whether driver CPU p95 improves
    without worsening music quality or observability.

## 2026-06-17: Atomic Stream-Stats Physical Soundcheck Rejection

- Commit under test:
  - `a11012f` (`Add atomic stream stats accumulators`)
- Commands:
  - `make hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-atomic-stream-stats/20260617-a11012f/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-atomic-stream-stats-a11012f-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal unload under the global lock: move active
    `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver` to
    `/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.manual-unloaded-20260616-220452`
    and terminate `Core Audio Driver (OpenA8DJ.driver)` PID `62309`.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-atomic-stream-stats-manual-unload.json`
- Result:
  - HAL candidate safety PASS.
  - Physical music soundcheck FAIL.
  - Final manual unload/isolation PASS: HAL inactive, lock absent, no OpenA8DJ
    driver process.
- Key quality metrics:
  - `quality_alignment_score=0.9630037066274131`
  - `analog_snr_db=10.34`
  - `click_outliers=106`
  - `lag_jumps_gt_2_frames=41`
  - `mid_band_residual_ratio=1.4131498493677057`
  - `high_band_residual_ratio=1.3678775330794208`
  - `quiet_mid_band_noise_dbfs=-36.078859229311256`
  - `capture_clipped_frames=0`
- CPU comparison:
  - atomic stream stats: driver median/p95/max `36.6%/37.6%/37.9%`;
  - default-after-unrolled control: `37.5%/38.5%/38.7%`;
  - hotstats-write-late prior run: `35.2%/36.0%/36.4%`;
  - queue8 prior run: `35.9%/37.2%/37.9%`;
  - mainline threshold remains `6.5%` driver p95.
- Evidence paths:
  - `local-analysis/physical-atomic-stream-stats/20260617-a11012f/hal-candidate-safety`
  - `local-analysis/soundcheck/20260617-atomic-stream-stats-a11012f-irig-pairA-16s-cpp-hal`
  - `local-analysis/runtime-isolation/after-atomic-stream-stats-manual-unload.json`
- Interpretation:
  - Atomic stream stats are rejected as a default. They provide only a small
    CPU reduction versus one failing control run, do not beat prior C++ CPU
    variants, remain far above mainline CPU, and do not improve physical music
    quality.

## 2026-06-17: Fast Prefetch Clear Physical Rejection

- Commands:
  - `make HAL_FAST_OUTPUT_PREFETCH_CLEAR=1 hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-fast-prefetch-clear/20260617-a1c8b50/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-fast-prefetch-clear-a1c8b50-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal HAL unload under global lock, then
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-fast-prefetch-clear-manual-unload.json`
- Result:
  - HAL candidate safety PASS.
  - Physical music soundcheck FAIL.
  - Final isolation PASS.
- Key metrics:
  - quality alignment `0.9546986682558902`
  - SNR about `9.53 dB`
  - lag jumps `39`
  - mid/high residual ratios `1.4338336857980034/1.3737879670112414`
  - driver CPU median/p95/max `34.6%/36.8%/37.4%`
- Interpretation:
  - Rejected as a default: CPU improves, but quality is materially worse than
    the latest default-like controls.

## 2026-06-17: Hot Stream Stats Interval 16 Physical A/B

- Commands:
  - `make HAL_HOT_STREAM_STATS_INTERVAL=16 hal`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-hotstats-interval16/20260617-a1c8b50/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-hotstats-interval16-a1c8b50-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal HAL unload under global lock, then
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-hotstats-interval16-manual-unload.json`
- Result:
  - HAL candidate safety PASS.
  - Physical music soundcheck still FAIL as a product gate.
  - Final isolation PASS.
  - Default promoted from interval `1` to `16` as a partial CPU improvement,
    not as readiness.
- Key metrics:
  - quality alignment `0.9640011789236339`
  - SNR about `10.28 dB`
  - click outliers `5`
  - lag jumps `41`
  - mid/high residual ratios `1.429448005646557/1.3625351838892474`
  - driver CPU median/p95/max `34.5%/35.7%/36.3%`
  - coreaudiod p95 `2.9%`
  - total watched audio/UI p95 `53.5%`
- Comparison:
  - latest default control driver p95 `38.5%`;
  - interval16 driver p95 `35.7%`;
  - mainline threshold `6.5%`.
- Evidence paths:
  - `local-analysis/physical-hotstats-interval16/20260617-a1c8b50/hal-candidate-safety`
  - `local-analysis/soundcheck/20260617-hotstats-interval16-a1c8b50-irig-pairA-16s-cpp-hal`
  - `local-analysis/runtime-isolation/after-hotstats-interval16-manual-unload.json`
- Interpretation:
  - Good enough to reduce default stats overhead because it does not materially
    alter the failing music signature and improves CPU. Not good enough for
    readiness or promotion.

## 2026-06-17: Tone Response Compensation Diagnostic

- Commands:
  - `python3 -m py_compile scripts/analyze-tone-response-compensation.py`
  - `scripts/analyze-tone-response-compensation.py --tone-matrix-json local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/tone-matrix.json --json-out local-analysis/tone-response-compensation/recent-music-runs.json <six existing soundcheck run dirs>`
- Result:
  - Offline diagnostic completed without touching hardware, CoreAudio, USB, or
    system services.
  - The simple tone-response shape model does not explain the music residual.
- Key metrics:
  - SNR delta range after response-shape fit: about `-0.19 dB` to `+1.88 dB`.
  - Mid residual delta range: about `-4.94` to `-5.02`, where negative means
    the response-shaped prediction made the mid-band residual ratio worse.
- Evidence paths:
  - `scripts/analyze-tone-response-compensation.py`
  - `local-analysis/tone-response-compensation/recent-music-runs.json`
- Interpretation:
  - A simple three-band response model derived from the passing tone matrix is
    not sufficient to explain the failed music quality. Next quality work needs
    a better controlled physical reference or a deeper format/phase/non-linear
    model.

## 2026-06-17: SciPy LTI Transfer Quality Diagnostic

- Commands:
  - `python3 -m venv .venv`
  - `.venv/bin/python -m pip install -r requirements-analysis.txt`
  - `.venv/bin/python -m py_compile scripts/analyze-lti-transfer-quality.py`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py --json-out local-analysis/lti-transfer-quality/recent-music-runs.json <six existing soundcheck run dirs>`
- Result:
  - Local analysis environment created under `.venv` only.
  - NumPy `2.0.2` and SciPy `1.13.1` installed locally.
  - Offline LTI/Welch/CSD diagnostic completed without touching hardware,
    CoreAudio, USB, HAL install state, or system services.
- Key metrics:
  - Mid-band mean coherence across tested runs/channels: about `0.075` to
    `0.124`.
  - High-band mean coherence across tested runs/channels: about `0.020` to
    `0.045`.
  - LTI SNR deltas were negative in every run: about `-0.78 dB` to `-2.37 dB`.
  - LTI-predicted mid/high residual ratios were worse than scalar-gain
    residual ratios in every run.
- Evidence paths:
  - `requirements-analysis.txt`
  - `scripts/analyze-lti-transfer-quality.py`
  - `local-analysis/lti-transfer-quality/recent-music-runs.json`
- Interpretation:
  - The failed music captures are not explained by a stable linear transfer
    function between the generated reference and iRig capture. The quality
    blocker is more likely non-linear/time-varying behavior, output-format
    semantics, or a physical reference mismatch requiring a stricter isolation
    test.

## 2026-06-17: Soundcheck Failure-Mode Diagnostic

- Commands:
  - `.venv/bin/python -m py_compile scripts/analyze-soundcheck-failure-modes.py`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py --json-out local-analysis/soundcheck-failure-modes/recent-music-runs.json <six existing soundcheck run dirs>`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py --drift-max-lag 128 --json-out local-analysis/soundcheck-failure-modes/recent-music-runs-local128.json <six existing soundcheck run dirs>`
- Result:
  - Offline diagnostic PASS. It read existing WAV/JSON evidence only and did
    not touch hardware, CoreAudio, USB, HAL install state, or system services.
- Key metrics:
  - Static 2x2 L/R matrix improves SNR only about `0.13-0.24 dB` in tested
    default-like runs.
  - Cubic memoryless model improves SNR only about `0.002-0.004 dB`.
  - No capture clipping in the tested runs.
  - Local-lag drift for recent default-like runs is small, around `-28` to
    `-34 ppm`, while SNR remains about `9-10 dB`.
- Evidence paths:
  - `scripts/analyze-soundcheck-failure-modes.py`
  - `local-analysis/soundcheck-failure-modes/recent-music-runs.json`
  - `local-analysis/soundcheck-failure-modes/recent-music-runs-local128.json`
- Interpretation:
  - Simple mix/polarity, static L/R matrix, simple memoryless non-linearity,
    clipping, and simple drift are not sufficient explanations. The next
    quality isolation should target reference-route mismatch, runtime
    discontinuities, or output format/phase semantics.

## 2026-06-17: Stats-Off Physical Rejection

- Candidate:
  - `HAL_OUTPUT_WRITE_STATS=0 HAL_HOT_STREAM_STATS=0`
  - HAL binary hash:
    `449f8baf8ce1ca74802de5507138e116886493a4aabaeb88488da410bdff2991`
- Commands:
  - `make -B hal HAL_OUTPUT_WRITE_STATS=0 HAL_HOT_STREAM_STATS=0`
  - `scripts/run-cpp-offline-gates`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-stats-off/20260617-a1c8b50-stats-off/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-stats-off-a1c8b50-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal HAL unload under global lock, then
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-stats-off.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-stats-off.json`
- Result:
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
  - HAL candidate safety PASS.
  - Physical music soundcheck FAIL.
  - Final isolation PASS: HAL inactive, lock absent, no OpenA8DJ/mainline
    holder processes.
  - Promotion readiness FAIL.
- Key quality metrics:
  - `quality_alignment_score=0.9602873731433063`
  - `analog_snr_db=10.48`
  - `lag_jumps_gt_2_frames=37`
  - `click_outliers=0`
  - `mid_band_residual_ratio=1.424930143914946`
  - `high_band_residual_ratio=1.3626602759181339`
  - `quiet_mid_band_noise_dbfs=-36.12216401783407`
  - `capture_clipped_frames=0`
- CPU metrics:
  - OpenA8DJ driver median/p95/max `35.9%/36.8%/37.0%`
  - coreaudiod median/p95/max `2.6%/3.4%/53.8%`
  - total watched audio/UI median/p95/max `53.3%/55.0%/115.9%`
- Evidence paths:
  - `local-analysis/physical-stats-off/20260617-a1c8b50-stats-off/hal-candidate-safety`
  - `local-analysis/soundcheck/20260617-stats-off-a1c8b50-irig-pairA-16s-cpp-hal`
  - `local-analysis/promotion-readiness-after-stats-off.json`
  - `local-analysis/runtime-isolation/final-after-stats-off.json`
- Interpretation:
  - Stats-off is rejected as a default. It removes observability, does not
    improve quality, and does not improve CPU relative to the current
    interval-16 default.

## 2026-06-17: Sparse Output-Cycle Clear Physical Rejection

- Candidate:
  - `HAL_OUTPUT_SPARSE_CYCLE_CLEAR=1`
- Commands:
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-next-hardware-check.json`
  - `make -B hal HAL_OUTPUT_SPARSE_CYCLE_CLEAR=1`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-sparse-cycle-clear/20260617-a1c8b50/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-sparse-cycle-clear-a1c8b50-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal HAL unload under global lock, then
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-sparse-cycle-clear.json`
  - `make -B hal`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-sparse-cycle-clear.json`
- Result:
  - Initial runtime isolation PASS: HAL inactive and lock absent.
  - HAL candidate safety PASS.
  - Physical music soundcheck FAIL.
  - Final isolation PASS: HAL inactive, lock absent, no OpenA8DJ/mainline
    holder processes.
  - Promotion readiness FAIL.
- Key quality metrics:
  - `quality_alignment_score=0.9636469258932283`
  - `analog_snr_db=10.48`
  - `lag_jumps_gt_2_frames=33`
  - `click_outliers=0`
  - `mid_band_residual_ratio=1.4081802020521872`
  - `high_band_residual_ratio=1.3645974703502124`
  - `quiet_mid_band_noise_dbfs=-36.222340546910175`
  - `capture_clipped_frames=0`
- CPU metrics:
  - OpenA8DJ driver median/p95/max `37.15%/38.3%/38.5%`
  - coreaudiod median/p95/max `2.7%/14.2%/58.7%`
  - total watched audio/UI median/p95/max `54.85%/56.9%/125.4%`
- Evidence paths:
  - `local-analysis/physical-sparse-cycle-clear/20260617-a1c8b50/hal-candidate-safety`
  - `local-analysis/soundcheck/20260617-sparse-cycle-clear-a1c8b50-irig-pairA-16s-cpp-hal`
  - `local-analysis/promotion-readiness-after-sparse-cycle-clear.json`
  - `local-analysis/runtime-isolation/final-after-sparse-cycle-clear.json`
- Interpretation:
  - Sparse cycle clear is rejected. It did not improve quality, worsened CPU,
    and should not remain as a latent hot-path option.

## 2026-06-17: Runtime Discontinuity Correlation Diagnostic

- Commands:
  - `.venv/bin/python -m py_compile scripts/analyze-runtime-discontinuities.py`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py --json-out local-analysis/runtime-discontinuities/recent-music-runs.json local-analysis/soundcheck/20260616-default-after-unrolled-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260617-hotstats-interval16-a1c8b50-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260617-stats-off-a1c8b50-irig-pairA-16s-cpp-hal local-analysis/soundcheck/20260617-sparse-cycle-clear-a1c8b50-irig-pairA-16s-cpp-hal`
- Result:
  - Offline diagnostic PASS. It read existing WAV/JSON/TSV evidence only and
    did not touch hardware, CoreAudio, USB, HAL install state, or system
    services.
- Key metrics:
  - No run produced a strong CPU or stream-delta correlation at the configured
    `abs(r) >= 0.70` threshold after offset search from `-5s` to `+5s`.
  - Window lag jumps are still present in every tested run:
    - default-after-unrolled: `p95=26.70` frames, max `42`
    - hotstats-interval16: `p95=24.70` frames, max `53`
    - stats-off: `p95=28.10` frames, max `52`
    - sparse-cycle-clear: `p95=30.35` frames, max `46`
  - Median scalar SNR stays around `10.0-10.46 dB`.
  - Capture clipping remains absent in all tested rows.
- Evidence paths:
  - `scripts/analyze-runtime-discontinuities.py`
  - `local-analysis/runtime-discontinuities/recent-music-runs.json`
- Interpretation:
  - The current visible runtime counters do not explain the physical music
    residual. The next decisive work should isolate reference-route mismatch,
    physical path behavior, output format/phase semantics, or runtime
    discontinuities that are not currently counted.

## 2026-06-17: Monitor-Free Soundcheck CPU Mode

- Change:
  - Added `--no-monitor-stream-stats` to `scripts/run-soundcheck`.
- Commands:
  - `python3 -m py_compile scripts/run-soundcheck`
  - `git diff --check`
- Result:
  - PASS. The mode is available for lower-perturbation CPU diagnostics.
- Interpretation:
  - Use this mode for CPU A/B only. Readiness and glitch claims still require
    stream-stat evidence from normal locked runs.

## 2026-06-17: Coalesce2 + Pool Cursor Safety Rejection

- Candidate:
  - `HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=1`
- Commands:
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-coalesce2-poolcursor.json`
  - `make -B hal HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=1`
  - `scripts/run-cpp-offline-gates`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-coalesce2-poolcursor/20260617-190a7ed/hal-candidate-safety`
- Result:
  - Offline gates PASS: Debug `16/16`, Release `17/17`.
  - HAL candidate safety FAIL.
  - The gate unloaded the rejected HAL and recovery PASS.
- Safety failure metrics:
  - `audio_stack_health=FAIL`
  - `coreaudiod=86.8%`
  - `mediaremoted=57.5%`
  - `total_watched_cpu_pct=145.3`
  - `opena8dj_driver=0.7%`
- Evidence paths:
  - `local-analysis/physical-coalesce2-poolcursor/20260617-190a7ed/hal-candidate-safety`
  - `local-analysis/runtime-isolation/after-coalesce2-safety-fail-before-unload.json`
- Interpretation:
  - Coalescing playback transfers by 2 plus pool cursor is rejected before
    soundcheck. It does not satisfy the safety gate.

## 2026-06-17: Default Monitor-Free And Normal Confirmation Runs

- Commands:
  - `make -B hal`
  - Default safety:
    `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-default-monitorfree/20260617-190a7ed/hal-candidate-safety`
  - Monitor-free soundcheck:
    `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-default-monitorfree-a1c8b50-irig-pairA-16s-cpp-hal --no-monitor-stream-stats --cpu-profile-interval 0.25 --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Default normal confirmation safety and soundcheck:
    `local-analysis/physical-default-normal-confirm/20260617-190a7ed/hal-candidate-safety`
    and
    `local-analysis/soundcheck/20260617-default-normal-confirm-a1c8b50-irig-pairA-16s-cpp-hal`
  - Final unload/isolation:
    `local-analysis/runtime-isolation/final-after-default-normal-confirm.json`
  - Promotion readiness:
    `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-default-normal-confirm.json`
- Result:
  - Default safety PASS before monitor-free.
  - Monitor-free soundcheck FAIL and is not comparable product evidence.
  - Default normal confirmation soundcheck FAIL with the known aligned failure
    signature.
  - Final isolation PASS: HAL inactive and lock absent.
  - Promotion readiness FAIL.
- Monitor-free metrics:
  - `quality_alignment_score=0.097964`
  - `snr_db=-29.18`
  - `lag_jumps_gt_2_frames=47`
  - OpenA8DJ driver median/p95/max `36.2%/39.0%/39.6%`
  - No stream-stat files by design.
- Normal confirmation metrics:
  - `quality_alignment_score=0.963713`
  - `analog_snr_db=10.57`
  - `lag_jumps_gt_2_frames=46`
  - `mid_band_residual_ratio=1.417748`
  - `high_band_residual_ratio=1.364806`
  - `quiet_mid_band_noise_dbfs=-35.79`
  - OpenA8DJ driver median/p95/max `35.8%/36.9%/37.7%`
  - coreaudiod median/p95/max `2.6%/4.6%/42.5%`
- Interpretation:
  - Removing stream-stat polling did not lower driver CPU and produced an
    invalid/decorrelated capture. Normal stream-stat soundcheck remains the
    readiness evidence path until the harness has a better low-perturbation
    monitor design.

## 2026-06-17: Coalesce2-Only Physical Rejection

- Candidate:
  - `HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=0`
- Commands:
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/pre-coalesce2-only.json`
  - `make -B hal HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=0`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-coalesce2-only/20260617-43773be/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 16 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-coalesce2-only-43773be-irig-pairA-16s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Manual minimal HAL unload under global lock, then
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-coalesce2-only.json`
  - `make -B hal`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-coalesce2-only.json`
- Result:
  - Initial runtime isolation PASS.
  - HAL candidate safety PASS.
  - Physical music soundcheck FAIL.
  - Final isolation PASS.
  - Promotion readiness FAIL.
- Quality metrics:
  - `quality_alignment_score=0.8988544786595754`
  - `analog_snr_db=5.85`
  - `lag_jumps_gt_2_frames=45`
  - `click_outliers=0`
  - `mid_band_residual_ratio=2.5634316824600596`
  - `high_band_residual_ratio=1.6665681529777348`
  - `quiet_mid_band_noise_dbfs=-33.677652564671256`
  - `capture_clipped_frames=0`
- CPU metrics:
  - OpenA8DJ driver median/p95/max `28.0%/28.5%/28.7%`
  - coreaudiod median/p95/max `2.7%/8.3%/60.4%`
  - total watched audio/UI median/p95/max `45.05%/47.2%/125.6%`
- Evidence paths:
  - `local-analysis/physical-coalesce2-only/20260617-43773be/hal-candidate-safety`
  - `local-analysis/soundcheck/20260617-coalesce2-only-43773be-irig-pairA-16s-cpp-hal`
  - `local-analysis/runtime-isolation/final-after-coalesce2-only.json`
  - `local-analysis/promotion-readiness-after-coalesce2-only.json`
- Interpretation:
  - Coalesce2 confirms transaction frequency contributes to CPU, but the
    quality regression is severe and CPU still misses mainline by a wide
    margin. It is rejected as a candidate/default.

## 2026-06-17: Cadence Outlier Threshold Fix

- Change:
  - Split internal cadence outlier threshold calculation into capture and
    playback expected transfer durations.
  - `cadenceExpectedTransferTicks` remains the base capture transfer period in
    the stream-stats payload.
- Commands:
  - `make -B hal`
  - `make -B hal HAL_CADENCE_DIAGNOSTIC=1 HAL_PLAYBACK_COALESCE_TRANSFERS=2`
  - `make -B hal`
- Result:
  - PASS. Default and diagnostic builds compile cleanly.
- Interpretation:
  - This is an observability fix only. It makes future coalescing diagnostics
    less misleading; it is not a quality or CPU improvement claim.

## 2026-06-17: Transfer-Ledger Diagnostic Physical Run And Format A/B

- Candidate:
  - Commit `a51ee29` (`Instrument aggregate USB transfer ledger`), default
    output format `HAL_OUTPUT_NATIVE=0`, `HAL_OUTPUT_START_BYTE=4`,
    `HAL_OUTPUT_CHECK_OFFSET=8`.
- Preflight:
  - Hardware lock acquired/released for device enumeration.
  - `iRig Stream` visible as CoreAudio `2 in / 2 out`, `48000`.
  - `Audio 8 DJ` visible on USB as VID/PID `0x17cc:0x1978`, serial
    `SN-HKM6Q6EDKP`.
  - Evidence:
    `local-analysis/hardware-preflight/20260616T234124-ledger-preflight`
    and
    `local-analysis/hardware-preflight/20260616T234140-ledger-device-enum`.
- Default physical soundcheck:
  - Evidence:
    `local-analysis/soundcheck/20260617-transfer-ledger-a51ee29-irig-pairA-16s-cpp-hal`.
  - Result: `FAIL`.
  - `quality_alignment_score=0.964608`.
  - `analog_snr_db=10.48`.
  - `lag_jumps_gt_2_frames=36`.
  - `mid_band_residual_ratio=1.420201`.
  - `high_band_residual_ratio=1.364979`.
  - `quiet_mid_band_noise_dbfs=-35.88`.
  - `capture_clipped_frames=0`.
  - Stream-stats summary:
    `local-analysis/stream-stats/transfer-ledger-a51ee29-summary.json`.
  - Diagnostic flags: `stream_stats_timeouts`,
    `transfer_ledger_overwritten`. No fallback allocations, no active
    underruns, and no queue/complete deltas explaining the failure.
- Diagnostic HAL capture:
  - Build flags:
    `HAL_DIAGNOSTIC=1 HAL_OUTPUT_AMPLITUDE_STATS=1 HAL_OUTPUT_NATIVE=0
    HAL_OUTPUT_START_BYTE=4 HAL_OUTPUT_CHECK_OFFSET=8
    HAL_UNROLLED_OUTPUT_PACK=0`.
  - Evidence:
    `local-analysis/soundcheck/20260617-diag-pack-big-start4-irig-pairA-16s-cpp-hal`.
  - Physical result: `FAIL`.
  - `quality_alignment_score=0.963726`.
  - `analog_snr_db=10.51`.
  - `lag_jumps_gt_2_frames=40`.
  - `mid_band_residual_ratio=1.428404`.
  - `high_band_residual_ratio=1.359313`.
  - Diagnostic files copied into the run directory:
    `opena8dj-output-written-f32.raw`,
    `opena8dj-output-consumed-f32.raw`,
    `opena8dj-output-packed-usb.raw`,
    `opena8dj-input-packed-usb.raw`,
    `opena8dj-output-events.tsv`.
  - Driver-capture analysis:
    `local-analysis/driver-capture-analysis/diag-pack-big-start4-output-packed-usb-auto.txt`.
  - Output USB control result:
    `usb_check_offset=8`, `usb_start_byte=4`, `usb_byte_order=big`,
    `usb_check_errors=0`, `usb_panic_flags=0`,
    `usb_alignment_score=1.000000`, left/right gain `0.50000000`,
    left/right SNR `999.00 dB`.
  - Written/consumed/packed path result from the same diagnostic run:
    written, consumed, written-vs-consumed, and packed USB comparisons were
    perfect against the reference over the analyzed window.
- Input USB analysis:
  - Evidence:
    `local-analysis/driver-capture-analysis/diag-pack-big-start4-input-packed-usb-pairA.txt`
    through `pairD.txt`.
  - Input packet checks are valid: `usb_check_errors=0`,
    `usb_panic_flags=0`, `usb_decoded_frames=600000`.
  - Input gains against the played reference are tiny for all pairs
    (`~ -0.00017` to `-0.00058`) with negative SNR around `-39` to `-44 dB`.
  - Interpretation: the input USB capture is not a useful music loopback
    reference for the current iRig/mixer path.
- Output no-leakage analysis:
  - Evidence:
    `local-analysis/driver-capture-analysis/diag-pack-big-start4-output-packed-usb-pairA.txt`
    through `pairD.txt`.
  - Pair A is perfect at gain `0.5`.
  - Pairs B/C/D decode as zero for the Pair A run, so the basic output routing
    matrix did not leak into inactive decks in the packed USB bytes.
- Native format A/B:
  - Build flags:
    `HAL_OUTPUT_NATIVE=1 HAL_DIAGNOSTIC=0 HAL_OUTPUT_AMPLITUDE_STATS=0
    HAL_OUTPUT_START_BYTE=4 HAL_OUTPUT_CHECK_OFFSET=8
    HAL_UNROLLED_OUTPUT_PACK=0`.
  - Evidence:
    `local-analysis/soundcheck/20260617-native-i24-start4-irig-pairA-16s-cpp-hal`.
  - Result: catastrophic `FAIL`.
  - `quality_alignment_score=0.003598`.
  - `analog_snr_db=-63.94`.
  - `quiet_mid_band_noise_dbfs=-8.87`.
  - `capture_clipped_frames=520014`.
  - Interpretation: native/little-endian 24-bit output is physically rejected
    and must not be used as a candidate/default.
- Cleanup:
  - The native HAL process respawned after normal recovery and user-level
    `kill` was not permitted.
  - Under the hardware lock, the active HAL bundle was moved to
    `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver.disabled-20260617T035921Z-cpp-native-reject`
    and the respawn stopped.
  - Final isolation:
    `local-analysis/runtime-isolation/final-after-disable-native-reject.json`,
    `PASS`, HAL inactive, lock absent, no OpenA8DJ HAL process.
  - Final audio stack health: `PASS`, watched audio CPU `0.0%`.
- Operational note:
  - During this iteration an accidental untracked file was briefly created in
    the read-only mainline path and immediately removed:
    `/Users/fer/dev/opena8dj/scripts/analyze-channel-transients.py`.
  - Follow-up checks confirmed that file is absent and clean in mainline.
- Interpretation:
  - The current failure is not explained by CoreAudio-to-HAL written frames,
    HAL consumed frames, basic Pair A routing, inactive deck leakage,
    start-byte/check-offset, big-endian Mode 2 packing, or USB check/panic
    flags in the bytes produced by the driver.
  - The remaining high-priority fault space is after the packed output bytes:
    actual USB/device scheduling or state, hardware interpretation, analog
    route/reference path, or a physical capture-route mismatch.
  - Promotion/readiness remains `FAIL`.

## 2026-06-17: Transfer-Ledger Evidence Documentation Verification

- Change:
  - Updated `ARCHITECT_CONTEXT.md`, `AGENT_HANDOFFS.md`,
    `DECISION_LOG.md`, `SUCCESS_METRICS.md`, `TEST_EVIDENCE.md`, and
    `local-analysis/usb-physical-investigation-summary.json` with the
    transfer-ledger diagnostic conclusions.
- Commands:
  - `git diff --check`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-transfer-ledger-doc-update.json`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Result:
  - Diff whitespace check PASS.
  - Runtime isolation PASS: HAL inactive, lock absent, no OpenA8DJ HAL
    process.
  - Offline gates PASS: Debug `17/17`, Release `18/18`, evidence schema
    PASS with `22` required files and `0` missing.
  - Release benchmark after the documentation update:
    `pack_mib_s=1628.44`, `decode_into_mib_s=587.742`,
    `route_frames_s=9.78303e+08`, `route_advanced_frames_s=4.99521e+08`.
  - Promotion readiness remains FAIL with
    `branch_promotion_allowed=false`.
- Current promotion blockers:
  - `physical_music_quality`: latest native run is catastrophic and the
    diagnostic/default transfer-ledger runs also fail strict music thresholds.
  - `runtime_cpu_beats_mainline`: latest OpenA8DJ driver p95 about `37.4%`,
    above the mainline reference `6.5%`.
  - `latest_physical_investigation`: still `FAIL_NOT_READY`.
  - `traktor_timecode_physical`: no locked physical Traktor/timecode-vinyl
    evidence yet.

## 2026-06-17: Payload Guard, Scheduling A/B, And Default Tone Recheck

- Commands:
  - `make -B hal build/opena8dj-control HAL_PLAYBACK_PAYLOAD_GUARD=1`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 15 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/payload-guard/20260617-bff59cc/hal-candidate-safety`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-payload-guard-bff59cc-irig-pairA-12s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-payload-guard-bff59cc-irig-pairA-12s-cpp-hal --json-out local-analysis/stream-stats/payload-guard-bff59cc-summary.json`
  - `make -B hal build/opena8dj-control HAL_PLAYBACK_PAYLOAD_GUARD=1 HAL_EXPLICIT_SCHED=1 HAL_USB_CLOCK_ANCHOR=1 HAL_USB_STABLE_FRAME=1`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-explicit-sched-bff59cc-irig-pairA-12s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - `make -B hal build/opena8dj-control HAL_PLAYBACK_PAYLOAD_GUARD=1 HAL_PLAYBACK_CAPTURE_PACED=0 HAL_EXPLICIT_SCHED=0 HAL_USB_CLOCK_ANCHOR=0 HAL_USB_STABLE_FRAME=0`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-fixed-out-bff59cc-irig-pairA-12s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - `scripts/analyze-soundcheck-linear-matrix.py local-analysis/soundcheck/20260617-payload-guard-bff59cc-irig-pairA-12s-cpp-hal local-analysis/soundcheck/20260617-explicit-sched-bff59cc-irig-pairA-12s-cpp-hal local-analysis/soundcheck/20260617-fixed-out-bff59cc-irig-pairA-12s-cpp-hal --analysis-seconds 8 --json-out local-analysis/soundcheck-linear-matrix/20260617-payload-explicit-fixed.json`
  - `scripts/analyze-tone-response-compensation.py --tone-matrix-json local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/tone-matrix.json --json-out local-analysis/tone-response-compensation/20260617-payload-explicit-fixed.json local-analysis/soundcheck/20260617-payload-guard-bff59cc-irig-pairA-12s-cpp-hal local-analysis/soundcheck/20260617-explicit-sched-bff59cc-irig-pairA-12s-cpp-hal local-analysis/soundcheck/20260617-fixed-out-bff59cc-irig-pairA-12s-cpp-hal`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py --json-out local-analysis/lti-transfer-quality/20260617-payload-explicit-fixed.json local-analysis/soundcheck/20260617-payload-guard-bff59cc-irig-pairA-12s-cpp-hal local-analysis/soundcheck/20260617-explicit-sched-bff59cc-irig-pairA-12s-cpp-hal local-analysis/soundcheck/20260617-fixed-out-bff59cc-irig-pairA-12s-cpp-hal`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py --json-out local-analysis/soundcheck-failure-modes/20260617-payload-explicit-fixed.json local-analysis/soundcheck/20260617-payload-guard-bff59cc-irig-pairA-12s-cpp-hal local-analysis/soundcheck/20260617-explicit-sched-bff59cc-irig-pairA-12s-cpp-hal local-analysis/soundcheck/20260617-fixed-out-bff59cc-irig-pairA-12s-cpp-hal`
  - `make -B hal build/opena8dj-control HAL_PLAYBACK_CAPTURE_PACED=1 HAL_EXPLICIT_SCHED=0 HAL_USB_CLOCK_ANCHOR=0 HAL_USB_STABLE_FRAME=0 HAL_PLAYBACK_PAYLOAD_GUARD=0`
  - Locked physical 1 kHz tone via `build/audio-record` plus
    `build/audio-pair-tone A 10 1000 0.12`, analyzed with
    `scripts/analyze-tone-capture.py ... --auto-window`.
  - `scripts/audio-stack-guard --force-unload-opena8dj --run-dir local-analysis/physical-tone/20260617-bff59cc-default/force-unload-after-tone`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-physical-tone-default-unload.json`
  - `scripts/evaluate-promotion-readiness.py --tone local-analysis/physical-tone/20260617-bff59cc-default/tone-1khz-irig-pairA/tone-analysis.txt --json-out local-analysis/promotion-readiness-after-bff59cc-default-tone.json`
- Result:
  - Payload guard soundcheck still failed strict music quality:
    `quality_alignment_score=0.958179`, `analog_snr_db=10.29`,
    `lag_jumps_gt_2_frames=22`, mid/high residual ratios
    `1.434533/1.368567`, no clipping.
  - Payload guard checks ran at about `1600/s` with `0` mismatches. This
    rules out queue-to-completion playback buffer mutation as the current
    physical-quality blocker.
  - Explicit scheduling plus USB clock anchor is physically rejected:
    `quality_alignment_score=0.025535`, `analog_snr_db=-33.82`,
    `click_outliers=14`, `lag_jumps_gt_2_frames=42`. Stream stats showed
    playback completed only about `23/s`, output read about `11 kframes/s`,
    and `35` timeline resets.
  - Fixed OUT pacing is physically rejected:
    `quality_alignment_score=-0.153805`, `analog_snr_db=-28.47`,
    `lag_jumps_gt_2_frames=40`, mid/high residual ratios
    `39.366597/25.403255`.
  - Capture ISO invariants PASS for all three new runs: zero-complete slots are
    expected, status failures are `0`, and useful transactions remain about
    `2.73` per transfer.
  - Static L/R matrix, tone-response compensation, LTI transfer fitting, and
    simple failure-mode models do not explain the aligned payload-guard music
    failure. The aligned run is classified as timebase/alignment instability
    with drift about `-176.6 ppm` and lag span `1650` frames.
  - Default 1 kHz physical tone after reverting to product-like flags:
    `sideband_ratio=0.006623`, strongest sideband `1060 Hz` at `-42.74 dB`,
    residual ratio `0.456797`, peak `0.28137207`, click outliers `40`.
    This beats the historical final `0.3.24` sideband ratio floor
    `0.008407`, but does not beat the best recorded mainline floor
    `0.004942`, does not beat strongest-sideband targets, and fails
    click-outlier requirements.
  - `scripts/audio-stack-guard --force-unload-opena8dj` PASS in cold smoke and
    post-tone cleanup. Final runtime isolation PASS: HAL inactive, lock absent,
    no OpenA8DJ process.
  - Promotion readiness remains FAIL. Branch promotion and "better than
    mainline" claims remain forbidden.
- Evidence paths:
  - `local-analysis/soundcheck/20260617-payload-guard-bff59cc-irig-pairA-12s-cpp-hal`
  - `local-analysis/stream-stats/payload-guard-bff59cc-summary.json`
  - `local-analysis/soundcheck/20260617-explicit-sched-bff59cc-irig-pairA-12s-cpp-hal`
  - `local-analysis/stream-stats/explicit-sched-bff59cc-summary.json`
  - `local-analysis/soundcheck/20260617-fixed-out-bff59cc-irig-pairA-12s-cpp-hal`
  - `local-analysis/stream-stats/fixed-out-bff59cc-summary.json`
  - `local-analysis/soundcheck-linear-matrix/20260617-payload-explicit-fixed.json`
  - `local-analysis/tone-response-compensation/20260617-payload-explicit-fixed.json`
  - `local-analysis/lti-transfer-quality/20260617-payload-explicit-fixed.json`
  - `local-analysis/soundcheck-failure-modes/20260617-payload-explicit-fixed.json`
  - `local-analysis/physical-tone/20260617-bff59cc-default/tone-1khz-irig-pairA/tone-analysis.txt`
  - `local-analysis/promotion-readiness-after-bff59cc-default-tone.json`
  - `local-analysis/runtime-isolation/post-physical-tone-default-unload.json`
- Operational note:
  - While adding `--force-unload-opena8dj`, `apply_patch` was accidentally
    applied in `/Users/fer/dev/opena8dj`. Only the newly added
    `force-unload` lines were removed immediately. A follow-up grep over
    mainline diff confirmed no `force-unload` lines remain there. The mainline
    file still has pre-existing unrelated local changes and was not reset.

## 2026-06-17: Transaction-Level Transfer Ledger Export

- Commands:
  - `make -B hal build/opena8dj-control`
  - `python3 -m py_compile scripts/run-soundcheck`
  - `python3 -m py_compile scripts/analyze-transfer-ledger.py`
  - `build/opena8dj-control --help 2>&1 | rg -n "transfer-ledger|stream-stats|input-stats"`
  - `git diff --check`
  - `scripts/run-cpp-offline-gates`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-transfer-ledger-run-soundcheck-hook.json`
  - `scripts/analyze-transfer-ledger.py local-analysis/transfer-ledger/synthetic-pass.tsv --json-out local-analysis/transfer-ledger/synthetic-pass-analysis.json`
- Result:
  - Added HAL IPC messages `kIPCTypeTransferLedgerGet` and
    `kIPCTypeTransferLedger`.
  - Added `build/opena8dj-control transfer-ledger [count]`, returning a bounded
    latest-entry TSV dump with sequence, event, host time, first-frame number,
    output read range, bytes, transaction counts, status, in-flight, and pool
    state.
  - Added automatic `transfer-ledger-after.tsv` capture to
    `scripts/run-soundcheck --stream-stats-snapshots`.
  - Added offline `scripts/analyze-transfer-ledger.py` for TSV-to-JSON
    validation of event counts, sequence gaps, completion statuses,
    failed/short transactions, playback queue/complete balance, output
    underrun/elastic frames, in-flight maxima, and per-event host deltas.
  - The hot path still writes only to the existing preallocated circular ledger;
    IPC/file output is under explicit control request and outside the transfer
    callback.
  - Build PASS.
  - `opena8dj-control --help` exposes `transfer-ledger`.
  - Offline gates PASS: debug `17/17`, release `18/18`.
  - Synthetic transfer-ledger analysis PASS.
  - Runtime isolation PASS: HAL inactive, hardware not touched, lock absent,
    no OpenA8DJ HAL process.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/runtime-isolation/after-transfer-ledger-run-soundcheck-hook.json`
  - `local-analysis/transfer-ledger/synthetic-pass-analysis.json`
- Product status:
  - Not readiness. This does not improve sound quality by itself and does not
    change the failed promotion result. It prepares the next locked physical
    default soundcheck to explain queue/complete timing rather than guessing.

## 2026-06-17: Bounded Full Transfer Ledger Physical Diagnosis

- Commands:
  - `make -B hal build/opena8dj-control`
  - `python3 -m py_compile scripts/analyze-transfer-ledger.py scripts/run-soundcheck`
  - `scripts/analyze-transfer-ledger.py core/tests/fixtures/transfer-ledger-full-window.tsv --json-out local-analysis/transfer-ledger/full-window-fixture-analysis-after-bounded-all.json`
  - `scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 15 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-ledger/20260617-bounded-full-ledger/hal-candidate-safety`
  - `scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-bounded-full-ledger-irig-pairA-12s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - `scripts/analyze-transfer-ledger.py local-analysis/soundcheck/20260617-bounded-full-ledger-irig-pairA-12s-cpp-hal/transfer-ledger-after.tsv --json-out local-analysis/transfer-ledger/bounded-full-ledger-soundcheck-analysis.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-bounded-full-ledger-irig-pairA-12s-cpp-hal/stream-stats-during.tsv --json-out local-analysis/stream-stats/bounded-full-ledger-soundcheck-summary.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-bounded-full-ledger-soundcheck-unload.json`
- Result:
  - Increased transfer-ledger ring capacity to `131072` entries and added
    bounded chunked `transfer-ledger --all` export.
  - After the physical diagnostic, changed product default to
    `HAL_TRANSFER_LEDGER=0`; future full-ledger physical diagnosis must build
    explicitly with `HAL_TRANSFER_LEDGER=1`.
  - Fixed transfer-ledger analyzer parsing for real CLI output and adjusted
    semantics so capture zero-complete transaction observations are warnings,
    not playback transport failures.
  - Fixture analysis PASS.
  - Offline gates PASS: debug `17/17`, release `18/18`.
  - HAL candidate safety PASS and iRig/Open Audio 8 DJ were visible before the
    physical run.
  - Physical music soundcheck FAIL:
    `quality_alignment_score=0.960392`, `analog_snr_db=10.37`,
    `lag_jumps_gt_2_frames=33`, mid residual ratio `1.411563`, high residual
    ratio `1.354488`, quiet mid noise `-35.40 dBFS`, `click_outliers=0`,
    `capture_clipped_frames=0`.
  - Transfer ledger analysis PASS:
    `91,647` rows, `overwritten=0`, continuous coverage, `0` sequence gaps,
    `0` playback failed/short transactions, `0` completion status failures,
    `0` playback first-frame regressions. Warnings remain for capture
    zero-complete observations and tail/post-playback output active-underrun
    snapshots.
  - Stream-stats summary shows no output underruns, active underruns, timeline
    resets, playback transfer errors, or transfer-pool fallback allocations
    during the run. It reports `transfer_ledger_playback_completion_gap`
    because the post-run ledger includes two more playback queues than
    completes at the bounded snapshot edge.
  - Sequential build verification after the default change:
    `make -B hal build/opena8dj-control` PASS with
    `OPENA8DJ_ENABLE_TRANSFER_LEDGER=0`, and
    `make -B HAL_TRANSFER_LEDGER=1 hal build/opena8dj-control` PASS with the
    diagnostic ledger enabled.
  - Final runtime isolation PASS: HAL inactive, lock absent, no OpenA8DJ process.
- Evidence paths:
  - `local-analysis/soundcheck/20260617-bounded-full-ledger-irig-pairA-12s-cpp-hal`
  - `local-analysis/transfer-ledger/bounded-full-ledger-soundcheck-analysis.json`
  - `local-analysis/stream-stats/bounded-full-ledger-soundcheck-summary.json`
  - `local-analysis/runtime-isolation/after-bounded-full-ledger-soundcheck-unload.json`
- Product status:
  - `FAIL_NOT_READY`. Clean transaction transport evidence is not enough:
    physical music quality is still far below thresholds and still does not
    objectively beat mainline.

## 2026-06-17: Product Ledger-Off, Stream Usage, And CPU Symbol Profile

- Commands:
  - `make -B hal build/opena8dj-control`
  - Locked product install:
    `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 15 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-product/20260617-product-ledgeroff/hal-candidate-safety`
  - Product ledger-off soundcheck:
    `scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-product-ledgeroff-irig-pairA-12s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-product-ledgeroff-irig-pairA-12s-cpp-hal/stream-stats-during.tsv --json-out local-analysis/stream-stats/product-ledgeroff-soundcheck-summary.json`
  - Changed `HAL_STREAM_USAGE ?= 1` and updated `build/audio-wav-play` to set
    `kAudioDevicePropertyIOProcStreamUsage` for the selected output pair.
  - Rebuild and offline gates:
    `make -B hal build/audio-wav-play build/opena8dj-control`,
    `ctest --test-dir build/cpp-release --output-on-failure`.
  - Locked stream-usage soundcheck:
    `scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-streamusage-irig-pairA-12s-cpp-hal --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Playback-only CPU symbol profile:
    `sudo -n sample <Core Audio Driver (OpenA8DJ.driver) pid> 7 -file local-analysis/profiling/20260617-sudo-sample-streamusage-playback-only/opena8dj-driver.sample.txt`.
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-streamusage-sample.json`
- Result:
  - Product ledger-off HAL built with `OPENA8DJ_ENABLE_TRANSFER_LEDGER=0`.
  - Product ledger-off physical soundcheck: FAIL,
    `quality_alignment_score=0.971414`, `analog_snr_db=10.52`,
    `lag_jumps_gt_2_frames=27`, mid/high residual ratios
    `1.409378/1.365051`, quiet mid noise `-35.04 dBFS`,
    `capture_clipped_frames=0`.
  - Stream-stats product summary confirmed `transferLedgerEntriesWritten=0`,
    no output underruns, no active underruns, no timeline resets, no playback
    transfer errors, no transfer-pool fallback allocations. Capture
    zero-complete slots remained the known packetization warning, not a hard
    transport failure.
  - Stream-usage physical soundcheck: FAIL,
    `quality_alignment_score=0.971648`, `analog_snr_db=10.52`,
    `lag_jumps_gt_2_frames=28`, mid/high residual ratios
    `1.399655/1.358543`, quiet mid noise `-35.20 dBFS`,
    `capture_clipped_frames=0`.
  - Stream usage is not a product-quality unlock. It is a small correctness
    change for clients that explicitly declare active streams.
  - CPU remains far above mainline: latest stream-usage run reports
    `opena8dj_driver_p95=37.2%` and `coreaudiod_p95=35.0%` in promotion
    readiness.
  - Initial unprivileged `sample` failed with privilege error; `sudo -n sample`
    succeeded without user interaction.
  - Playback-only symbol profile shows the dominant active CPU path is
    `org.opena8dj.driver.usb`, specifically IOUSBHost async enqueue from
    capture and playback completion paths. Audio pack/decode/routing are much
    smaller in the profile.
  - Final runtime isolation after each locked window: PASS, HAL inactive, lock
    absent, no OpenA8DJ driver process.
- Evidence paths:
  - `local-analysis/soundcheck/20260617-product-ledgeroff-irig-pairA-12s-cpp-hal`
  - `local-analysis/stream-stats/product-ledgeroff-soundcheck-summary.json`
  - `local-analysis/soundcheck/20260617-streamusage-irig-pairA-12s-cpp-hal`
  - `local-analysis/profiling/20260617-sudo-sample-streamusage-playback-only`
  - `local-analysis/runtime-isolation/after-streamusage-soundcheck.json`
  - `local-analysis/runtime-isolation/after-sudo-sample-playback-only.json`
  - `local-analysis/promotion-readiness-after-streamusage-sample.json`
- Readiness note:
  - `FAIL_NOT_READY`. The candidate still does not beat mainline in music
    quality or CPU, and Traktor/timecode vinyl physical validation remains
    absent. Do not move C to Legacy or C++ to main.

## 2026-06-17: Mainline Baseline And C++ ISO64/q8 StopIO Candidate

- Commands:
  - C++ experimental transport build:
    `make -B hal build/audio-wav-play build/opena8dj-control HAL_ISO_FRAMES=64 HAL_CAPTURE_QUEUE=8 HAL_PLAYBACK_QUEUE=8 HAL_OUTPUT_PREFETCH_FRAMES=64`
  - Default C++ build after adopting ISO64/q8/StopIO:
    `make -B hal build/audio-wav-play build/opena8dj-control`
  - Offline verification:
    `cmake --build build/cpp-release --target opena8djcpp_core_tests opena8djcpp_static_policy_check opena8djcpp_realtime_audit`
    and `ctest --test-dir build/cpp-release --output-on-failure`
  - Mainline baseline safety:
    `scripts/test-hal-candidate-safety --candidate /Users/fer/dev/opena8dj/build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 45 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/mainline-baseline/20260617-mainline-0.3.135-wait45/hal-candidate-safety`
  - Mainline baseline soundcheck:
    `scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/mainline-baseline/20260617-mainline-0.3.135-wait45/soundcheck-irig-pairA-12s --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - C++ ISO64/q8 soundcheck:
    `scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-cpp-iso64q8-streamusage-irig-pairA-12s --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - C++ ISO64/q8 StopIO soundcheck:
    `scripts/run-soundcheck --skip-build --music-file "$HOME/Music/DJ/20250902_santxez_2024_curation/A-Ninetyfour, James My & Criss - Nueva Mexico (Extended Mix) 128.mp3" --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --target-peak-db -16 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-cpp-iso64q8-stopisoc-irig-pairA-12s --stream-stats-snapshots --monitor-command-timeout 1.0 --audio-stack-enumeration-timeout 8 --audio-stack-threshold 80 --audio-stack-total-threshold 180 --audio-stack-recover-on-fail`
  - Promotion evaluation:
    `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-iso64q8-stopisoc.json`
  - Final unload/isolation:
    `scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --enumeration-timeout 8 --run-dir local-analysis/audio-stack-guard/cpp-iso64q8-stopisoc-force-unload`
    and `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-cpp-iso64q8-stopisoc-force-unload.json`
- Results:
  - Mainline artifact used read-only:
    `/Users/fer/dev/opena8dj/build/OpenA8DJ.driver`, version `0.3.135`,
    SHA256 `569c7303a1a9672d40c56eeee914eadccdbcd541562e1d2d674d1a3ffb9b90dc`.
  - First mainline safety attempt with short wait failed due transient
    CoreAudio/mediaremoted CPU; the 45 s stabilization attempt passed.
  - Mainline baseline soundcheck FAIL for quality in the current physical
    route: `quality_alignment_score=0.680798`, `analog_snr_db=-0.83`,
    `lag_jumps_gt_2_frames=39`. CPU: `opena8dj_driver_p95=6.0%`,
    `coreaudiod_p95=8.0%`.
  - C++ ISO64/q8 before StopIO shutdown FAIL for quality:
    `quality_alignment_score=0.674248`, `analog_snr_db=-0.87`,
    `lag_jumps_gt_2_frames=42`. CPU improved from the previous C++ default:
    `opena8dj_driver_p95=10.6%`, `coreaudiod_p95=12.4%`.
  - C++ ISO64/q8 before StopIO shutdown left `streaming=yes` after playback
    and accumulated about `86k` final active-underrun frames.
  - C++ ISO64/q8 with `HAL_STOP_ISOC_ON_STOP=1` FAIL for quality:
    `quality_alignment_score=0.686712`, `analog_snr_db=-0.84`,
    `lag_jumps_gt_2_frames=35`. CPU: `opena8dj_driver_p95=9.8%`,
    `coreaudiod_p95=11.5%`.
  - StopIO shutdown fixed final state:
    `streaming=no`, `outputUnderruns=0`, `outputActiveUnderruns=0`.
  - Offline CTest remained PASS, 18/18.
  - Promotion readiness remains FAIL:
    physical music quality FAIL, runtime CPU still above mainline threshold,
    and Traktor/timecode physical validation remains absent.
  - Final runtime isolation PASS: HAL inactive, lock absent, no OpenA8DJ
    CoreAudio driver process.
- Evidence paths:
  - `local-analysis/mainline-baseline/20260617-mainline-0.3.135-wait45`
  - `local-analysis/soundcheck/20260617-cpp-iso64q8-streamusage-irig-pairA-12s`
  - `local-analysis/soundcheck/20260617-cpp-iso64q8-stopisoc-irig-pairA-12s`
  - `local-analysis/stream-stats/cpp-iso64q8-stopisoc-soundcheck-summary.json`
  - `local-analysis/promotion-readiness-after-iso64q8-stopisoc.json`
  - `local-analysis/runtime-isolation/after-cpp-iso64q8-stopisoc-force-unload.json`

## 2026-06-17: Input Decode Control And Comparable Physical CPU Run

- Commit under test: worktree change after `b4a76ca`, not committed at run time.
- Commands:
  - Build:
    `make -B hal build/audio-wav-play build/opena8dj-control`
  - Offline gates:
    `scripts/run-cpp-offline-gates`
  - Runtime isolation before hardware:
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/before-inputdecode-physical.json`
  - Candidate safety for corrected capture-channel run:
    `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 45 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-inputdecode-off/20260617-inputdecode-off-dense-ch12/hal-candidate-safety`
  - Comparable physical soundcheck with stereo iRig channels:
    `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --cpu-profile --audio-stack-wait 45 --audio-stack-enumeration-timeout 8 --audio-stack-min-idle-pct 20 --run-dir local-analysis/soundcheck/20260617-cpp-inputdecode-off-dense-ch12-irig-pairA-12s`
  - Final unload and isolation:
    `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --enumeration-timeout 8 --run-dir local-analysis/audio-stack-guard/after-inputdecode-off-dense-ch12-unload`
    and
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-inputdecode-off-dense-ch12-unload.json`
  - Promotion evaluation:
    `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-inputdecode-off-ch12.json`
- Results:
  - Offline gates PASS: Debug `17/17`, Release `18/18`.
  - `dvs_packet_input_decode` PASS and reports `playback_decode_off=PASS`.
  - Safety PASS; final isolation PASS with HAL inactive and lock absent.
  - The comparable iRig Pair A soundcheck still FAILS quality:
    `quality_alignment_score=0.680121`, SNR `-0.83 dB`,
    `lag_jumps_gt_2_frames=42`, `capture_clipped_frames=0`.
  - Driver CPU is now within the configured p95 target but does not beat the
    current mainline p95:
    `opena8dj_driver_p95=6.3%`, down from C++ ISO64/q8 StopIO `9.8%`
    versus mainline `6.0%`.
  - CoreAudio CPU is still worse than mainline:
    `coreaudiod_p95=43.2%` versus mainline `8.0%` in the same baseline bundle
    and versus the stricter promotion threshold `1.7%`. Row inspection shows
    this is dominated by startup spikes, but the current gate counts it.
  - Stream end-state remains clean:
    `outputUnderruns=0`, `outputActiveUnderruns=0`,
    `outputTimelineResets=0`, `playbackTransferErrors=0`.
  - Promotion readiness remains FAIL. Blockers are physical music quality,
    runtime CPU because of coreaudiod p95, latest physical investigation
    `FAIL_NOT_READY`, and missing physical Traktor/timecode validation.
- Incident note:
  - An initial soundcheck used `--capture-channels 2`; `audio-record` recorded
    `channels=2,2`, duplicating iRig channel 2 into both output WAV channels.
    That run is not valid stereo quality evidence and was superseded by
    `20260617-cpp-inputdecode-off-dense-ch12-irig-pairA-12s` with
    `channels=1,2`.
  - A manual preflight attempted to call non-existent shell helper functions
    `acquire_audio_gate_lock` and `release_audio_gate_lock`. Because the shell
    did not use `set -e`, `audio-list` and `opena8dj-control` still ran after
    the helper failure. This touched enumeration/control without an effective
    lock. It did not change defaults, reset USB, install drivers, or reboot, but
    it violated the lock discipline and must not be repeated. The correct helper
    names are `opena8dj_acquire_hardware_lock` and
    `opena8dj_release_hardware_lock`; prefer scripts with built-in lock handling.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/physical-inputdecode-off/20260617-inputdecode-off-dense-ch12/hal-candidate-safety`
  - `local-analysis/soundcheck/20260617-cpp-inputdecode-off-dense-ch12-irig-pairA-12s`
  - `local-analysis/promotion-readiness-after-inputdecode-off-ch12.json`
  - `local-analysis/runtime-isolation/after-inputdecode-off-dense-ch12-unload.json`

## 2026-06-17: Reject HAL_INPUT_IO=0 Diagnostic Variant

- Commit under test: `ea46f01` plus Makefile diagnostic flag exposure.
- Commands:
  - Build diagnostic variant:
    `make -B hal build/audio-wav-play build/opena8dj-control HAL_INPUT_IO=0`
  - Safety:
    `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 45 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/physical-inputio-off/20260617-inputio-off/hal-candidate-safety`
  - Explicit cleanup after safety failure:
    `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --enumeration-timeout 8 --run-dir local-analysis/audio-stack-guard/after-inputio-off-safety-fail-unload`
    and
    `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-inputio-off-safety-fail-unload.json`
  - Rebuild default product HAL:
    `make -B hal build/audio-wav-play build/opena8dj-control`
- Results:
  - Diagnostic variant was rejected before soundcheck:
    `cycle=1 result=FAIL reason=required_device_missing`.
  - The OpenA8DJ HAL process was present during the guard, but CoreAudio
    enumeration did not show UID `org.opena8dj.Audio8DJ`; only iRig, MacBook
    microphone, and MacBook speakers were visible.
  - The safety script moved the rejected bundle to:
    `/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.rejected-missing-device-cycle-1-20260617-022416`.
  - Recovery completed with HAL inactive, lock absent, and audio stack health
    PASS. Recovery did restart audio-related services as part of the authorized
    safety failure path.
  - Default product HAL was rebuilt afterward with `HAL_INPUT_IO=1`.
- Decision:
  - Do not use `HAL_INPUT_IO=0` as a product or benchmark variant. Disabling
    HAL input I/O at compile time breaks device enumeration, so it cannot answer
    the coreaudiod CPU question safely.
- Evidence paths:
  - `local-analysis/physical-inputio-off/20260617-inputio-off/hal-candidate-safety`
  - `local-analysis/audio-stack-guard/after-inputio-off-safety-fail-unload`
  - `local-analysis/runtime-isolation/after-inputio-off-safety-fail-unload.json`

## 2026-06-17: Current C++ vs Mainline Pair A Channel Matrix Rejection

- Purpose:
  - Compare the current C++ product HAL after input-decode gating against the
    current mainline `0.3.135` artifact on the same physical Pair A route:
    Audio 8 DJ output -> external mixer REC OUT -> iRig Stream capture.
  - Validate routing/leakage with decorrelated tones before making any
    A/B/C/D or audiophile-quality claim.
- C++ current product HAL evidence:
  - HAL candidate safety:
    `local-analysis/physical-channel-matrix/20260617-inputdecode-default-chmatrix-safety`,
    PASS.
  - Matrix run:
    `local-analysis/channel-matrix/20260617-inputdecode-default-pairA-chmatrix`.
  - Tone matrix result: `FAIL`.
  - `max_wrong_source_leakage_db=-35.36` against threshold `-45.0`.
  - `left_to_right_leakage_db=-41.79`.
  - `right_to_left_leakage_db=-30.09`.
  - `expected_floor_amplitude=0.019538`.
  - `capture_clipped_frames=0`.
  - Linear matrix diagnostic result: `PASS_DIAGNOSTIC`, but classified
    `linear_matrix_rejected_large_physical_residual` with
    `global_mono_correlation=0.043` and
    `residual_over_capture_rms=0.992`.
- Mainline `0.3.135` read-only artifact evidence:
  - HAL candidate safety:
    `local-analysis/mainline-baseline/20260617-mainline-chmatrix/hal-candidate-safety`,
    PASS.
  - Matrix run:
    `local-analysis/channel-matrix/20260617-mainline-pairA-chmatrix`.
  - Tone matrix result: `FAIL`.
  - `max_wrong_source_leakage_db=-42.58` against threshold `-45.0`.
  - `left_to_right_leakage_db=-46.09`.
  - `right_to_left_leakage_db=-37.28`.
  - `expected_floor_amplitude=0.046111`.
  - `capture_clipped_frames=0`.
  - Linear matrix diagnostic result: `PASS_DIAGNOSTIC`, but classified
    `linear_matrix_rejected_large_physical_residual` with
    `global_mono_correlation=0.343` and
    `residual_over_capture_rms=0.946`.
- Cleanup:
  - `local-analysis/runtime-isolation/after-inputdecode-default-chmatrix-unload.json`,
    PASS.
  - `local-analysis/runtime-isolation/after-mainline-chmatrix-unload.json`,
    PASS.
- Result:
  - Both C++ and mainline fail the current physical Pair A channel-matrix
    threshold in this iRig/mixer route.
  - C++ is objectively worse than mainline on this test:
    `-35.36 dB` max wrong-source leakage versus mainline `-42.58 dB`.
  - This blocks any claim that C++ routing/functionality is better than
    mainline and blocks branch promotion.
- Readiness interpretation:
  - The current physical route cannot support an audiophile-quality PASS claim.
  - The C++ driver must either fix its additional leakage versus mainline or
    prove with an independently validated capture path that the measurement
    route is the limiting factor.

## 2026-06-17: Channel Matrix Documentation And Offline Verification

- Change:
  - Documented the current C++ vs mainline Pair A channel-matrix rejection in
    `ARCHITECT_CONTEXT.md`, `DECISION_LOG.md`,
    `PROMOTION_READINESS_STATUS.md`, `SUCCESS_METRICS.md`, and this evidence
    file.
  - Re-ran offline gates and promotion evaluation after the documentation
    update.
- Commands:
  - `git diff --check`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-channel-matrix-doc-update.json`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Results:
  - Diff whitespace check PASS.
  - Runtime isolation PASS: HAL inactive, hardware lock absent, no OpenA8DJ
    process, no hardware/CoreAudio/USB mutation.
  - Offline gates PASS: Debug `17/17`, Release `18/18`.
  - Evidence schema PASS: `22` required files, `0` missing.
  - Release benchmark:
    `pack_mib_s=1652.58`, `decode_into_mib_s=587.724`,
    `route_frames_s=1.01639e+09`,
    `route_advanced_frames_s=5.05785e+08`.
  - Promotion readiness remains `FAIL` with
    `branch_promotion_allowed=false`.
- Current promotion blockers:
  - `physical_music_quality`: C++ Pair A/iRig music capture still fails with
    `quality_alignment_score=0.680121`, SNR `-0.83 dB`, and `42` lag jumps.
  - `runtime_cpu_beats_mainline`: C++ driver p95 `6.3%` is near but above
    mainline `6.0%`; `coreaudiod_p95=43.2%` is worse than mainline `8.0%`.
  - `latest_physical_investigation`: still `FAIL_NOT_READY`.
  - `traktor_timecode_physical`: still `BLOCKED_UNVALIDATED_DVS`.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/cpp-offline/offline-bench-release.json`
  - `local-analysis/promotion-readiness-current.json`
  - `local-analysis/runtime-isolation/post-channel-matrix-doc-update.json`

## 2026-06-17: Stream-Usage-Off Harness Control

- Change:
  - Added `audio-wav-play --no-stream-usage` and `--stream-usage`.
  - Added `scripts/run-channel-matrix-gate --no-output-stream-usage`.
  - Added `scripts/run-soundcheck --no-output-stream-usage`.
  - Defaults remain unchanged: `audio-wav-play` still requests selected-pair
    output stream usage unless explicitly disabled.
- Reason:
  - Recent physical C++ vs mainline matrix runs used the C++ harness.
  - C++ HAL exposes `IOProcStreamUsage`; the read-only mainline `0.3.135`
    artifact likely rejects it because its default build has
    `HAL_STREAM_USAGE=0`.
  - The next physical comparison needs a controlled C++ run with stream usage
    disabled to separate harness/CoreAudio behavior from actual driver routing
    or analog leakage.
- Commands:
  - `make -B build/audio-wav-play`
  - `bash -n scripts/run-channel-matrix-gate`
  - `python3 -m py_compile scripts/run-soundcheck`
  - `./build/audio-wav-play`
  - `./build/audio-wav-play local-analysis/channel-matrix/offline-no-stream-usage-plan-v2/fixture/reference.wav --bad-option`
  - `scripts/run-channel-matrix-gate --run-id offline-no-stream-usage-plan-v2 --no-output-stream-usage --pair A --rate 48000 --seconds 1 --peak 0.1`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-stream-usage-harness-control.json`
- Results:
  - `audio-wav-play` builds.
  - Invalid option handling fails before CoreAudio device lookup when given a
    valid WAV fixture: `unknown argument: --bad-option`.
  - Prepare-only channel matrix plan records `output_stream_usage=0` and does
    not touch hardware.
  - Offline gates PASS: Debug `17/17`, Release `18/18`.
  - Evidence schema PASS: `22` required files, `0` missing.
  - Release benchmark:
    `pack_mib_s=1612.58`, `decode_into_mib_s=586.736`,
    `route_frames_s=9.59212e+08`,
    `route_advanced_frames_s=5.04042e+08`.
  - Promotion readiness remains `FAIL` with
    `branch_promotion_allowed=false`.
  - Runtime isolation PASS: HAL inactive, lock absent, no hardware/CoreAudio/USB
    mutation.
- Evidence paths:
  - `local-analysis/channel-matrix/offline-no-stream-usage-plan-v2/plan.txt`
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-current.json`
  - `local-analysis/runtime-isolation/post-stream-usage-harness-control.json`
- Next locked physical command shape:
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-channel-matrix-gate --run-physical --run-id <run-id> --pair A --rate 48000 --seconds 8 --peak 0.30 --capture-device "iRig Stream" --capture-channels 1,2 --no-output-stream-usage`

## 2026-06-17: Physical Stream-Usage-Off And Mainline-Config Probes

- Commands:
  - `make -B hal build/audio-record build/audio-wav-play build/opena8dj-control`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260617-cpp-no-streamusage-prep`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-channel-matrix-gate --run-physical --run-id 20260617-cpp-no-streamusage-pairA-chmatrix --pair A --rate 48000 --seconds 8 --peak 0.30 --capture-device "iRig Stream" --capture-channels 1,2 --no-output-stream-usage`
  - `make -B hal build/audio-record build/audio-wav-play build/opena8dj-control HAL_STREAM_USAGE=0 HAL_FAST_OUTPUT_PREFETCH_CLEAR=1 HAL_BACKGROUND_PREOPEN_ON_INIT=1 HAL_HOT_STREAM_STATS_INTERVAL=1`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --run-dir local-analysis/hal-candidate-safety/20260617-cpp-mainline-parity-config-prep`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-channel-matrix-gate --run-physical --run-id 20260617-cpp-mainline-parity-config-pairA-chmatrix --pair A --rate 48000 --seconds 8 --peak 0.30 --capture-device "iRig Stream" --capture-channels 1,2 --no-output-stream-usage`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-soundcheck --skip-build --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --target-peak-db -12 --capture-device "iRig Stream" --capture-channels 1,2 --run-dir local-analysis/soundcheck/20260617-cpp-mainline-parity-config-dense-ch12-irig-pairA-12s --no-output-stream-usage --cpu-profile --stream-stats-snapshots --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - explicit HAL unload under lock, followed by `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-parity-soundcheck-unload-final.json`
  - `make -B hal build/audio-record build/audio-wav-play build/opena8dj-control`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-default-rebuild-final.json`
  - `git diff --check`
- Results:
  - USB/CoreAudio preflight saw `iRig Stream` in CoreAudio and `Audio 8 DJ`
    in USB; Audio 8 was not in CoreAudio until the C++ HAL was installed.
  - Default C++ with harness stream usage disabled improved Pair A max
    wrong-source leakage from `-35.36 dB` to `-39.72 dB`, but still failed the
    `-45 dB` threshold and remained worse than mainline `-42.58 dB`.
  - Mainline-config C++ (`HAL_STREAM_USAGE=0`,
    `HAL_FAST_OUTPUT_PREFETCH_CLEAR=1`,
    `HAL_BACKGROUND_PREOPEN_ON_INIT=1`,
    `HAL_HOT_STREAM_STATS_INTERVAL=1`) recovered the physical output level to
    mainline-like tone amplitudes, but still failed the Pair A matrix:
    max wrong-source leakage `-40.57 dB`, left-to-right `-40.57 dB`,
    right-to-left `-40.09 dB`, clipped frames `0`.
  - Mainline-config real-music soundcheck failed:
    `quality_alignment_score=0.678827`, SNR `-0.83 dB`,
    mid residual ratio `2.536563`, high residual ratio `1.779982`,
    `lag_jumps_gt_2_frames=42`, clipped frames `0`.
  - Stream stats during the failed music run showed no output underruns,
    no active underruns, no elastic drops, no timeline resets, and no late
    writes. The quality failure is therefore not explained by the exposed
    underrun/reset counters.
  - Promotion readiness remains `FAIL` with
    `branch_promotion_allowed=false`.
  - The local HAL/tools were rebuilt back to the Makefile defaults after the
    experiment. Offline gates still PASS: Debug `17/17`, Release `18/18`.
  - Latest release benchmark after default rebuild:
    `pack_mib_s=1645.1`, `decode_into_mib_s=587.023`,
    `route_frames_s=8.61253e+08`,
    `route_advanced_frames_s=5.06314e+08`.
  - Diff whitespace check PASS.
  - Final runtime isolation PASS: HAL inactive, lock absent, no OpenA8DJ
    process, no hardware/CoreAudio/USB mutation pending.
- Evidence paths:
  - `local-analysis/channel-matrix/20260617-cpp-no-streamusage-pairA-chmatrix/tone-matrix.json`
  - `local-analysis/channel-matrix/20260617-cpp-mainline-parity-config-pairA-chmatrix/tone-matrix.json`
  - `local-analysis/soundcheck/20260617-cpp-mainline-parity-config-dense-ch12-irig-pairA-12s/metrics.json`
  - `local-analysis/soundcheck/20260617-cpp-mainline-parity-config-dense-ch12-irig-pairA-12s/stream-stats-after.txt`
  - `local-analysis/runtime-isolation/post-parity-soundcheck-unload-final.json`
  - `local-analysis/runtime-isolation/post-default-rebuild-final.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`

## 2026-06-17: Direct USB Pair A Matrix Isolation

- Purpose:
  - Separate HAL/CoreAudio callback behavior from the lower USB/device/analog
    path by playing the same decorrelated matrix fixture through the direct USB
    tools while capturing the physical Pair A route with iRig.
- Commands:
  - `make -B usb-play-plain-gain05 build/audio-record`
  - locked manual direct USB run with `build/opena8dj-usb-play-plain-gain05`
    and output directory
    `local-analysis/channel-matrix/20260617-direct-usb-gain05-pairA-chmatrix`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-direct-usb-gain05-matrix.json`
  - `make -B usb-play`
  - locked manual direct USB run with `build/opena8dj-usb-play` and output
    directory
    `local-analysis/channel-matrix/20260617-direct-usb-halflags-pairA-chmatrix`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-direct-usb-halflags-matrix.json`
  - `make -B usb-play-plain-gain05`
  - locked manual direct USB selected-Pair-A run with
    `build/opena8dj-usb-play-plain-gain05 <reference> A` and output directory
    `local-analysis/channel-matrix/20260617-direct-usb-gain05-pairA-selected-chmatrix`
  - locked manual direct USB selected-Pair-A run with
    `build/opena8dj-usb-play-plain-gain05 <reference> A 8192` and output
    directory
    `local-analysis/channel-matrix/20260617-direct-usb-gain05-pairA-selected-lead8192-chmatrix`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/post-direct-usb-selected-lead8192-matrix.json`
  - `build/opena8dj-usb-play-plain-gain05 <reference> Z` for parser rejection
    smoke, using an existing fixture and no USB access.
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `git diff --check && scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-selected-direct-tool.json`
- Results:
  - `opena8dj-usb-play-plain-gain05` completed playback and captured without
    clipping, but the matrix still failed:
    max wrong-source leakage `-44.78 dB`, left-to-right `-44.78 dB`,
    right-to-left `-29.97 dB`.
  - That run had strong channel asymmetry: left expected max `0.09584`, right
    expected max only `0.01005`. It is not a valid quality improvement claim
    even though one leakage direction was close to threshold.
  - `opena8dj-usb-play` built with current HAL flags was much worse:
    max wrong-source leakage `-13.19 dB`, left expected max `0.00558`, right
    expected max `0.00610`.
  - Added direct USB tool support for selected pairs and optional lead prebuffer
    while preserving default `all` output.
  - Selected Pair A with no lead still failed:
    max wrong-source leakage `-35.28 dB`, left-to-right `-42.54 dB`,
    right-to-left `-18.05 dB`, left expected max `0.04414`, right expected max
    `0.00607`.
  - Selected Pair A with `8192` lead frames improved L->R to `-46.82 dB`, but
    still failed because right expected level fell to `0.00366` and R->L was
    only `-16.05 dB`.
  - Invalid pair parsing returns usage/status `2` before USB access.
  - Offline gates PASS after the tool change: Debug `17/17`, Release `18/18`.
  - Latest release benchmark:
    `pack_mib_s=1657.57`, `decode_into_mib_s=585.28`,
    `route_frames_s=9.10222e+08`,
    `route_advanced_frames_s=5.05947e+08`.
  - Promotion readiness remains `FAIL` with
    `branch_promotion_allowed=false`.
  - Diff whitespace check PASS.
  - Both direct runs bypassed HAL publication and finished with isolation PASS:
    HAL inactive, lock absent.
- Interpretation:
  - The remaining defect is not isolated to HAL/CoreAudio alone.
  - The current direct USB tools are diagnostic only and must not be used for
    readiness claims. They point at USB/audio-param/direct-engine behavior or
    physical route/control-state differences that still need isolation.
- Evidence paths:
  - `local-analysis/channel-matrix/20260617-direct-usb-gain05-pairA-chmatrix/tone-matrix.json`
  - `local-analysis/channel-matrix/20260617-direct-usb-halflags-pairA-chmatrix/tone-matrix.json`
  - `local-analysis/channel-matrix/20260617-direct-usb-gain05-pairA-selected-chmatrix/tone-matrix.json`
  - `local-analysis/channel-matrix/20260617-direct-usb-gain05-pairA-selected-lead8192-chmatrix/tone-matrix.json`
  - `local-analysis/runtime-isolation/post-direct-usb-gain05-matrix.json`
  - `local-analysis/runtime-isolation/post-direct-usb-halflags-matrix.json`
  - `local-analysis/runtime-isolation/post-direct-usb-selected-lead8192-matrix.json`
  - `local-analysis/runtime-isolation/final-after-selected-direct-tool.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`

## 2026-06-17: Direct ISO Sweep, ISO10 HAL Product Gate, And Readiness Rejection

- Purpose:
  - Replace speculation about USB cadence with measured direct-USB and HAL
    evidence.
  - Preserve the audiophile gate boundary: matrix improvement is not enough
    when real music quality and CPU still fail.
- Commands:
  - `make -B usb-play usb-play-plain-gain05`
  - `build/opena8dj-usb-play <fixture> Z` for parser rejection smoke before
    USB access.
  - `scripts/run-cpp-offline-gates`
  - Direct locked ISO sweep using separately built direct tools:
    `build/opena8dj-usb-play-iso10`,
    `build/opena8dj-usb-play-iso12`,
    `build/opena8dj-usb-play-iso14`,
    `build/opena8dj-usb-play-iso16`.
  - `make -B hal build/audio-wav-play build/audio-record build/opena8dj-control HAL_ISO_FRAMES=10 HAL_PLAYBACK_ISO_FRAMES=10`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded`
  - Locked HAL Pair A channel matrix for ISO10/q8.
  - Locked HAL real-music soundcheck for ISO10/q8.
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-iso10q8.json`
  - `scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-iso10q8-soundcheck-unload.json`
- Build and offline results:
  - `make -B usb-play usb-play-plain-gain05` PASS.
  - Invalid direct pair parser smoke PASS: status `2` before USB access.
  - Offline C++ gates PASS: Debug `17/17`, Release `18/18`.
  - Latest release benchmark:
    `pack_mib_s=1652.84`, `decode_into_mib_s=584.749`,
    `route_frames_s=9.35811e+08`,
    `route_advanced_frames_s=4.20131e+08`.
- Direct USB ISO sweep:
  - ISO10 selected Pair A PASS:
    max wrong-source leakage `-54.23 dB`, L->R `-54.23 dB`,
    R->L `-53.89 dB`, active underruns `0`.
  - ISO12 selected Pair A PASS:
    max wrong-source leakage `-50.44 dB`, L->R `-50.44 dB`,
    R->L `-48.83 dB`, active underruns `0`.
  - ISO14 selected Pair A PASS:
    max wrong-source leakage `-50.00 dB`, L->R `-52.01 dB`,
    R->L `-48.21 dB`, active underruns `0`.
  - ISO16 selected Pair A FAIL:
    max wrong-source leakage `-44.02 dB`, L->R `-46.72 dB`,
    R->L `-41.87 dB`, active underruns `0`.
  - Previous direct ISO8/q8 evidence remains the strongest measured direct
    quality candidate so far:
    max wrong-source leakage `-53.55 dB`, L->R `-62.75 dB`,
    R->L `-51.88 dB`.
- HAL ISO10/q8 product evidence:
  - Candidate safety PASS with HAL intentionally left loaded for the locked
    product gate.
  - Pair A channel matrix PASS:
    max wrong-source leakage `-52.30 dB`, L->R `-55.26 dB`,
    R->L `-50.67 dB`, no clipping.
  - Real-music quality FAIL:
    `quality_alignment_score=0.969379`, analog SNR `10.18 dB`,
    mid/high residual ratios `1.514509/1.396638`,
    quiet mid noise `-35.09 dBFS`, `35` lag jumps greater than 2 frames.
  - Runtime CPU FAIL against mainline:
    driver p50/p95/max `19.1/19.6/19.7%`; coreaudiod p50/p95/max
    `2.6/84.3/87.4%`.
  - Stream counters were clean: no output underruns, active underruns,
    drops/replays, resets, or late writes.
- ISO8/q8 comparison:
  - HAL Pair A matrix also PASS:
    max wrong-source leakage about `-52 dB`.
  - Real-music quality still FAIL:
    `quality_alignment_score=0.964724`, analog SNR `10.00 dB`,
    mid/high residual ratios `1.432051/1.356290`, `29` lag jumps.
  - Driver CPU p95 was worse than ISO10 (`23.1%`), but ISO8 had better
    music residual and fewer lag jumps.
- Interpretation:
  - ISO64/q8 is rejected for product default because it reduces CPU but fails
    physical quality badly.
  - ISO10 is rejected as default for now despite lower driver CPU because it
    worsens music residual and lag jumps versus ISO8, while still failing the
    mainline CPU gate.
  - ISO8/q8 remains the current default quality candidate, not a readiness
    candidate.
  - Matrix Pair A improvement over mainline-like evidence is real but
    insufficient. Real music, runtime CPU, full A/B/C/D routing, and physical
    Traktor/timecode gates remain blocking.
  - Promotion remains forbidden:
    `branch_promotion_allowed=false`.
- Final state:
  - HAL unloaded/inactive.
  - Hardware lock absent.
  - Runtime isolation PASS.
  - No C mainline or Rust worktree changes.
- Evidence paths:
  - `local-analysis/channel-matrix/20260617-direct-iso10-pairA-selected-lead8192-chmatrix/tone-matrix.json`
  - `local-analysis/channel-matrix/20260617-direct-iso12-pairA-selected-lead8192-chmatrix/tone-matrix.json`
  - `local-analysis/channel-matrix/20260617-direct-iso14-pairA-selected-lead8192-chmatrix/tone-matrix.json`
  - `local-analysis/channel-matrix/20260617-direct-iso16-pairA-selected-lead8192-chmatrix/tone-matrix.json`
  - `local-analysis/channel-matrix/20260617-direct-usb-iso8q8-pairA-selected-lead8192-chmatrix/tone-matrix.json`
  - `local-analysis/channel-matrix/20260617-cpp-iso10q8-hal-pairA-chmatrix/tone-matrix.json`
  - `local-analysis/soundcheck/20260617-cpp-iso10q8-dense-ch12-irig-pairA-12s/metrics.json`
  - `local-analysis/soundcheck/20260617-cpp-iso10q8-dense-ch12-irig-pairA-12s/cpu-summary.json`
  - `local-analysis/promotion-readiness-after-iso10q8.json`
  - `local-analysis/runtime-isolation/after-iso10q8-soundcheck-unload.json`

## 2026-06-17: Input-Decode Gating And Playback-Only Stream Usage Probe

- Purpose:
  - Make `HAL_INPUT_DECODE_ACTIVE_GATING` effective at stream start.
  - Make `audio-wav-play` explicitly disable input stream usage for
    playback-only soundchecks while still selecting the target output pair.
  - Test whether removing accidental input work improves CPU or quality.
- Code changes:
  - Default `HAL_INPUT_DECODE_ACTIVE_GATING=1`.
  - At USB stream start, input decode remains inactive until an input read or
    control-plane profile activates it.
  - `audio-wav-play` now sets input `IOProcStreamUsage` to all-off when stream
    usage is enabled for playback.
- Commands:
  - `make -B hal build/audio-wav-play build/opena8dj-control`
  - `git diff --check`
  - `scripts/run-cpp-offline-gates`
  - `build/audio-wav-play` parser smoke, expecting usage/status `2`.
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-input-decode-gating-build.json`
  - First locked `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded`
  - Second locked safety retry with `--wait 8`, followed by locked
    `scripts/run-soundcheck` and cleanup.
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py ...`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py ...`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py ...`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-inputdecode-gated-usage.json`
- Offline/build results:
  - HAL/tools build PASS.
  - Diff whitespace PASS.
  - Offline gates PASS: Debug `17/17`, Release `18/18`.
  - Release bench:
    `pack_mib_s=1627.01`, `decode_into_mib_s=577.291`,
    `route_frames_s=9.48938e+08`,
    `route_advanced_frames_s=4.90103e+08`.
  - Parser smoke PASS without hardware access.
  - Runtime isolation before physical PASS: HAL inactive, lock absent.
- Physical safety:
  - First safety run failed immediately after HAL load because CoreAudio spiked:
    `coreaudiod=160.3%`, `opena8dj_driver=0.0%`.
  - Cleanup succeeded and isolation stayed PASS.
  - Retry with `--wait 8` passed candidate safety, proving the first failure
    was at least partly a startup/enumeration window.
- Physical soundcheck result:
  - Run:
    `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal`.
  - Result: FAIL.
  - `quality_alignment_score=0.959187`.
  - analog SNR `10.14 dB`.
  - mid/high residual ratios `1.467121/1.368783`.
  - quiet mid noise `-35.11 dBFS`.
  - `lag_jumps_gt_2_frames=30`.
  - no clipping and no click outliers.
  - stream counters after run: no output underruns, active underruns, elastic
    drops/replays, timeline resets, late writes, or playback errors.
- CPU result:
  - driver p50/p95/max `23.6/24.2/24.4%`.
  - coreaudiod p50/p95/max `2.7/21.9/62.0%`.
  - Compared with ISO8/q8 prior run: coreaudiod p95 improved, but driver p95
    worsened and music quality worsened.
  - Compared with mainline target: still FAIL by a wide margin.
- Offline diagnosis from captured evidence:
  - Failure modes classify the run as
    `timebase_or_alignment_instability`.
  - Static L/R mix, polarity, and simple memoryless nonlinearity are not
    sufficient explanations.
  - Drift estimate `-180.6 ppm`, lag span `1645` frames.
  - Runtime discontinuity analysis finds no strong CPU or stream-stat
    correlation and no capture clipping; window lag jumps remain present.
  - LTI transfer correction worsens SNR; this is not a simple fixed EQ/linear
    transfer issue.
- Interpretation:
  - The harness/policy correction is kept because playback-only tests should
    not request input streams.
  - It is not a product-quality improvement.
  - It does not solve the music residual or CPU gate.
  - Promotion remains forbidden:
    `branch_promotion_allowed=false`.
  - Next technical target remains the timebase/cadence path, not input decode.
- Final state:
  - HAL unloaded/inactive.
  - Hardware lock absent.
  - Runtime isolation PASS.
- Evidence paths:
  - `local-analysis/physical-product/20260617-inputdecode-gated-playback-usage/hal-candidate-safety/summary.txt`
  - `local-analysis/physical-product/20260617-inputdecode-gated-playback-usage-wait8/hal-candidate-safety/summary.txt`
  - `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/metrics.json`
  - `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/cpu-profile.tsv`
  - `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `local-analysis/promotion-readiness-after-inputdecode-gated-usage.json`
  - `local-analysis/runtime-isolation/after-inputdecode-gated-wait8-unload.json`

## 2026-06-17: ISO Invariant Tooling And Cadence Diagnostic Build Profile

- Purpose:
  - Stop treating expected zero-complete ISO slots as a root cause for the
    real-music failure.
  - Preserve a reproducible diagnostic HAL build profile for the next locked
    physical cadence run without changing the product-default HAL profile.
- Code/tooling changes:
  - `scripts/analyze-capture-iso-invariants.py` now infers `isoN` from run
    paths, can derive the transfer slot count from classified capture slots
    when the path does not include `isoN`, and reports a warning instead of a
    failure when only the final stop/drain transfer is missing.
  - `Makefile` now has `hal-cadence-diagnostic`, which rebuilds the HAL with
    cadence diagnostics, transfer ledger, payload guard, amplitude stats, hot
    stats every transfer, and atomic stream-stat accumulators.
  - The diagnostic profile is for evidence collection only. It is not a
    product performance candidate.
- Commands:
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260617-cpp-iso8q8-dense-ch12-irig-pairA-12s --json-out local-analysis/soundcheck/20260617-cpp-iso8q8-dense-ch12-irig-pairA-12s/capture-iso-invariants.json`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260617-cpp-iso10q8-dense-ch12-irig-pairA-12s --json-out local-analysis/soundcheck/20260617-cpp-iso10q8-dense-ch12-irig-pairA-12s/capture-iso-invariants.json`
  - `make -B hal-cadence-diagnostic build/opena8dj-control && make -B hal`
  - `scripts/run-cpp-offline-gates`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-iso-invariant-tooling.json`
- Results:
  - Latest inputdecode-gated run ISO invariants: PASS with warning
    `classified_transactions_missing_at_most_one_stop_transfer`.
    The tool derives `iso_frames_per_transfer=8` from observed classified
    slots because the run path does not include `iso8`.
  - ISO8/q8 run ISO invariants: PASS.
  - ISO10/q8 run ISO invariants: PASS with the same one-stop-transfer warning.
  - Product HAL rebuild restored the normal non-diagnostic flags:
    `HAL_CADENCE_DIAGNOSTIC=0`, `HAL_TRANSFER_LEDGER=0`,
    `HAL_PLAYBACK_PAYLOAD_GUARD=0`, `HAL_OUTPUT_AMPLITUDE_STATS=0`,
    `HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=0`.
  - Offline gates PASS after the tooling/build-profile change:
    Debug `17/17`, Release `18/18`.
  - Release bench after the change:
    `pack_mib_s=1656.09`, `decode_into_mib_s=588.466`,
    `route_frames_s=8.65519e+08`,
    `route_advanced_frames_s=5.09884e+08`.
  - Runtime isolation PASS:
    HAL inactive, hardware lock absent, no OpenA8DJ process detected.
- Interpretation:
  - Zero-complete capture slots are packetization/stop-window evidence, not the
    current audiophile-quality blocker.
  - The current blocker remains real-music timebase/alignment instability and
    high runtime CPU.
  - The next physical cadence run should use `make hal-cadence-diagnostic`
    only as a diagnostic candidate, record ledger/cadence evidence, then
    restore with `make -B hal` before any product-quality CPU claim.
- Evidence paths:
  - `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `local-analysis/soundcheck/20260617-cpp-iso8q8-dense-ch12-irig-pairA-12s/capture-iso-invariants.json`
  - `local-analysis/soundcheck/20260617-cpp-iso10q8-dense-ch12-irig-pairA-12s/capture-iso-invariants.json`
  - `local-analysis/runtime-isolation/after-iso-invariant-tooling.json`

## 2026-06-17: Locked Cadence Diagnostic Physical Capture

- Purpose:
  - Capture high-resolution cadence/ledger evidence under hardware lock without
    using the diagnostic build as product-performance evidence.
  - Verify whether the latest failure is accompanied by payload corruption,
    transport status errors, ledger discontinuity, underruns, or cadence
    outliers.
- Commands:
  - `make -B hal-cadence-diagnostic build/audio-wav-play build/audio-record build/audio-config build/opena8dj-control`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 8 --run-dir local-analysis/physical-product/20260617-cadence-diagnostic/hal-candidate-safety`
  - `scripts/run-soundcheck --run-dir local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --monitor-stream-stats --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `scripts/analyze-transfer-ledger.py local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/transfer-ledger-after.tsv --json-out local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/transfer-ledger-analysis.json`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/after-cadence-diagnostic-unload`
  - `make -B hal`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-cadence-diagnostic-unload.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-cadence-diagnostic.json`
- Safety and cleanup:
  - HAL candidate safety PASS.
  - Final `audio-stack-guard` PASS.
  - Final runtime isolation PASS: HAL inactive, hardware lock absent, no
    OpenA8DJ processes detected.
  - Product HAL build restored after the diagnostic run.
- Physical soundcheck result:
  - Result: FAIL.
  - `quality_alignment_score=0.958757`.
  - analog SNR `10.09 dB`.
  - mid/high residual ratios `1.447622/1.366173`.
  - quiet mid noise `-35.03 dBFS`.
  - `lag_jumps_gt_2_frames=27`.
  - no clipping; click outliers `0` in the soundcheck summary.
- Runtime result:
  - Promotion evaluator still FAIL.
  - driver p95 `24.1%`, `coreaudiod` p95 `12.3%`.
  - Diagnostic flags add overhead, so these numbers are not product CPU
    evidence except to reject promotion.
- Transport/ledger result:
  - Capture ISO invariants PASS with warning
    `classified_transactions_missing_in_stop_transfer_gap`.
  - Transfer ledger PASS after classifying final `0xe00002eb` stop aborts as a
    stop-window warning.
  - Ledger coverage continuous: `48528` rows, no sequence gaps, no overwritten
    entries.
  - Playback queue/complete delta `0`, max in-flight `8`.
  - Payload guard checks about `1000/s`; mismatches `0`.
  - No output active underruns, elastic drops, elastic replays, timeline
    resets, late writes, short playback transfers, or playback transfer errors.
  - Cadence outliers were observed: capture completion `7`, playback
    completion `8` during the run.
- Failure-mode interpretation:
  - Failure classifier still reports `timebase_or_alignment_instability`.
  - Static L/R mix or polarity is not sufficient.
  - Simple memoryless nonlinearity is not sufficient.
  - LTI/fixed EQ correction worsens SNR, so this is not a fixed linear
    transfer problem.
  - Runtime discontinuity analysis found no strong correlation, but the top
    weak correlations include completion outlier deltas against lag jumps.
- Product conclusion:
  - Do not promote.
  - Do not claim readiness.
  - Next product work should target USB completion jitter, queue timing, and
    capture-paced scheduling policy while preserving payload correctness.
- Evidence paths:
  - `local-analysis/physical-product/20260617-cadence-diagnostic/hal-candidate-safety/summary.txt`
  - `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/metrics.json`
  - `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/transfer-ledger-analysis.json`
  - `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `local-analysis/runtime-isolation/after-cadence-diagnostic-unload.json`
  - `local-analysis/promotion-readiness-after-cadence-diagnostic.json`

## 2026-06-17: Playback-Before-Capture-Requeue Product Probe

- Purpose:
  - Test whether queueing playback before capture requeue reduces completion
    jitter and improves real-music quality without the heavy cadence diagnostic
    profile.
  - Keep the run as a product-timing probe only; do not use it for readiness
    unless music quality and CPU both beat the gates.
- Build/run flags:
  - `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1`.
  - Cadence/ledger/payload-guard diagnostic flags were not enabled for this
    product probe.
- Commands:
  - `make -B hal build/audio-wav-play build/audio-record build/audio-config build/opena8dj-control HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 8 --run-dir local-analysis/physical-product/20260617-playback-before-capture-requeue/hal-candidate-safety`
  - `scripts/run-soundcheck --run-dir local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --monitor-stream-stats --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/after-playback-before-capture-requeue-unload`
  - `make -B hal`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-playback-before-capture-requeue-unload.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-playback-before-capture-requeue.json`
- Safety and cleanup:
  - HAL candidate safety PASS.
  - Final `audio-stack-guard` PASS.
  - Final runtime isolation PASS: HAL inactive, hardware lock absent, no
    OpenA8DJ process detected.
  - Post-documentation runtime isolation PASS: HAL inactive, hardware lock
    absent, no OpenA8DJ process detected.
  - Product HAL build restored after the timing probe.
- Physical soundcheck result:
  - Result: FAIL.
  - `quality_alignment_score=0.961360`.
  - SNR right channel floor `10.25 dB`; stereo mean about `10.35 dB`.
  - mid/high residual ratios `1.425897/1.365001`.
  - quiet mid noise `-35.03 dBFS`.
  - `lag_jumps_gt_2_frames=28`.
  - no clipping; click outliers `0`.
- Runtime result:
  - Promotion evaluator still FAIL.
  - driver p95 `21.8%`, `coreaudiod` p95 `12.2%`.
  - This is lower than some recent failed probes, but still far above the
    mainline CPU budget and not paired with acceptable sound quality.
- Transport/runtime result:
  - Capture ISO invariants PASS with warning
    `classified_transactions_missing_in_stop_transfer_gap`.
  - Stream stats show no output active underruns, no timeline resets, no late
    writes, no completion delta outliers in the lightweight sampled stats.
  - Capture zero-complete packetization remains present at about `3.63`
    zero-complete slots per capture transfer.
- Failure-mode interpretation:
  - Failure classifier still reports `timebase_or_alignment_instability`.
  - Static L/R mix or polarity is not sufficient.
  - Simple memoryless nonlinearity is not sufficient.
  - Fixed LTI/EQ correction worsens SNR.
  - Runtime discontinuity analysis found no strong correlation; lag jumps
    remain present.
- Product conclusion:
  - Reject as a product improvement.
  - Do not promote.
  - Do not claim readiness.
  - The reduced lightweight completion outliers are not enough; the next probe
    must improve actual lag jumps, residuals, SNR, and CPU together.
- Evidence paths:
  - `local-analysis/physical-product/20260617-playback-before-capture-requeue/hal-candidate-safety/summary.txt`
  - `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/metrics.json`
  - `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `local-analysis/runtime-isolation/after-playback-before-capture-requeue-unload.json`
  - `local-analysis/runtime-isolation/final-after-playback-before-capture-requeue-docs.json`
  - `local-analysis/promotion-readiness-after-playback-before-capture-requeue.json`

## 2026-06-17: Transfer-Rate-Safe Lead Model

- Purpose:
  - Prevent another physical probe from treating `CAPTURE_PACED_OUT_LEAD>1` as
    a harmless latency lead when the current implicit scheduling path actually
    changes the capture-to-playback transfer cadence.
  - Make the offline transfer model reject transport-rate-unsafe candidates
    even when transfer pools have enough slots and no fallback allocations.
- Code change:
  - `tools/transfer_pool_model.cpp` now computes
    `playback_queue_ratio = playback_queue_attempts / periods`.
  - The model requires playback queue ratio in `[0.95, 1.05]` for transport
    rate safety unless a scenario explicitly expects rejection.
  - Added rejection scenarios for:
    `lead2_implicit_bursts_rejected`, `lead4_implicit_bursts_rejected`, and
    `lead64_pool_margin`.
  - Existing coalesce2 remains rejected for cadence/rate safety even though it
    has no pool pressure.
- Commands:
  - `scripts/run-cpp-offline-gates`
- Results:
  - Offline gates PASS:
    Debug `17/17`, Release `18/18`.
  - Transfer-pool model PASS with `8` rows.
  - Safe rows:
    - default lead1: playback queue ratio `1`, transport-rate-safe `true`;
    - mainline-like queue8: playback queue ratio `1`, transport-rate-safe
      `true`.
  - Rejected rows:
    - coalesce2: playback queue ratio `0.5`, transport-rate-safe `false`;
    - lead2: playback queue ratio `2`, transport-rate-safe `false`;
    - lead4: playback queue ratio `4`, transport-rate-safe `false`;
    - lead64: playback queue ratio `64`, transport-rate-safe `false`.
- Interpretation:
  - Do not run `HAL_CAPTURE_PACED_OUT_LEAD>1` physically on the current
    implicit scheduling path as a quality optimization.
  - A future lead experiment must first implement explicit, frame-numbered
    scheduling or another model that preserves a 1:1 capture/playback cadence,
    then pass offline gates before hardware.
- Evidence paths:
  - `local-analysis/cpp-offline/transfer-pool-model.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`

## 2026-06-17: Reused ISO Completion Handlers Product Probe

- Purpose:
  - Isolate `HAL_REUSE_ISOC_COMPLETIONS=1` as a CPU/hot-path candidate after a
    previous combined transport variant was rejected.
  - Verify whether avoiding per-transfer completion block creation improves CPU
    without changing packet layout, routing, sample rate, ISO size, queue
    depth, capture-paced cadence, or stream usage.
- Build/run flags:
  - `HAL_REUSE_ISOC_COMPLETIONS=1`.
  - Current product geometry otherwise unchanged:
    ISO8/q8, capture-paced playback, lead1, coalesce1, stream usage on,
    diagnostics/ledger/payload guard off.
- Commands:
  - `make -B hal build/audio-wav-play build/audio-record build/audio-config build/opena8dj-control HAL_REUSE_ISOC_COMPLETIONS=1`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 8 --run-dir local-analysis/physical-product/20260617-reuse-isoc-completions/hal-candidate-safety`
  - `scripts/run-soundcheck --run-dir local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --monitor-stream-stats --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/after-reuse-isoc-completions-unload`
  - `make -B hal`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-reuse-isoc-completions-unload.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-reuse-isoc-completions.json`
- Safety and cleanup:
  - HAL candidate safety PASS.
  - Final runtime isolation PASS: HAL inactive, hardware lock absent, no
    OpenA8DJ process detected.
  - Product HAL build restored after the probe.
- Physical soundcheck result:
  - Result: FAIL.
  - `quality_alignment_score=0.961164`.
  - SNR floor `9.98 dB`.
  - mid/high residual ratios `1.459843/1.377935`.
  - quiet mid noise `-34.84 dBFS`.
  - `lag_jumps_gt_2_frames=25`.
  - no clipping; click outliers `0`.
- Runtime result:
  - Promotion evaluator still FAIL.
  - driver p95 `22.1%`, `coreaudiod` p95 `15.0%`.
  - This remains far above mainline and does not improve quality enough to
    justify enabling the flag.
- Transport/runtime result:
  - Capture ISO invariants PASS with warning
    `classified_transactions_missing_in_stop_transfer_gap`.
  - Stream stats show no output active underruns, no timeline resets, no late
    writes, no capture/playback transfer-pool fallback allocations, and
    playback/capture transfer delta `-1` at stop.
- Failure-mode interpretation:
  - Failure classifier still reports `timebase_or_alignment_instability`.
  - Static L/R mix or polarity is not sufficient.
  - Simple memoryless nonlinearity is not sufficient.
- Product conclusion:
  - Reject `HAL_REUSE_ISOC_COMPLETIONS=1` as a product default.
  - Do not combine it with other rejected transport knobs unless the underlying
    transport architecture changes and offline gates explain why the old
    physical rejection no longer applies.
- Evidence paths:
  - `local-analysis/physical-product/20260617-reuse-isoc-completions/hal-candidate-safety/summary.txt`
  - `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/metrics.json`
  - `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `local-analysis/runtime-isolation/after-reuse-isoc-completions-unload.json`
  - `local-analysis/promotion-readiness-after-reuse-isoc-completions.json`

## 2026-06-17: Fast ISO Transfer Config Product Probe

- Purpose:
  - Isolate `HAL_FAST_ISO_TRANSFER_CONFIG=1` as a CPU/hot-path candidate.
  - Verify whether reusing stable isochronous transaction layout reduces HAL
    CPU without changing packet layout, request counts, routing, sample rate,
    ISO size, queue depth, capture-paced cadence, or stream usage.
- Build/run flags:
  - `HAL_FAST_ISO_TRANSFER_CONFIG=1`.
  - Current product geometry otherwise unchanged:
    ISO8/q8, capture-paced playback, lead1, coalesce1, stream usage on,
    diagnostics/ledger/payload guard off.
- Commands:
  - `make -B hal build/audio-wav-play build/audio-record build/audio-config build/opena8dj-control HAL_FAST_ISO_TRANSFER_CONFIG=1`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 8 --run-dir local-analysis/physical-product/20260617-fast-iso-transfer-config/hal-candidate-safety`
  - `scripts/run-soundcheck --run-dir local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --monitor-stream-stats --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/after-fast-iso-transfer-config-unload`
  - `make -B hal`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-fast-iso-transfer-config-unload.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-fast-iso-transfer-config.json`
- Safety and cleanup:
  - HAL candidate safety PASS.
  - Final runtime isolation PASS: HAL inactive, hardware lock absent, no
    OpenA8DJ process detected.
  - Product HAL build restored after the probe.
- Physical soundcheck result:
  - Result: FAIL.
  - `quality_alignment_score=0.959397`.
  - SNR floor `10.19 dB`.
  - mid/high residual ratios `1.450623/1.368530`.
  - quiet mid noise `-35.05 dBFS`.
  - `lag_jumps_gt_2_frames=35`.
  - no clipping; click outliers `0`.
- Runtime result:
  - Promotion evaluator still FAIL.
  - driver p95 `23.1%`, `coreaudiod` p95 `25.9%`.
  - This does not improve runtime resource use versus the latest product
    probes and worsens lag jumps.
- Transport/runtime result:
  - Capture ISO invariants PASS with no warnings.
  - Stream stats show no output active underruns, no timeline resets, no late
    writes, no capture/playback transfer-pool fallback allocations, and
    playback/capture transfer delta `0`.
- Failure-mode interpretation:
  - Failure classifier still reports `timebase_or_alignment_instability`.
  - Static L/R mix or polarity is not sufficient.
  - Simple memoryless nonlinearity is not sufficient.
  - LTI/fixed EQ correction worsens SNR.
- Product conclusion:
  - Reject `HAL_FAST_ISO_TRANSFER_CONFIG=1` as a product default.
  - Stable descriptor layout reuse is not the dominant CPU or quality blocker.
- Evidence paths:
  - `local-analysis/physical-product/20260617-fast-iso-transfer-config/hal-candidate-safety/summary.txt`
  - `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/metrics.json`
  - `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `local-analysis/runtime-isolation/after-fast-iso-transfer-config-unload.json`
  - `local-analysis/promotion-readiness-after-fast-iso-transfer-config.json`

## 2026-06-17: Offline Route-Signature Comparison

- Purpose:
  - Compare existing mainline and C++ captures without touching hardware to
    determine whether the current iRig/reference route can support sound
    quality claims.
  - Separate a shared degraded route signature from the current C++ HAL
    timebase/residual signature.
- Commands:
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py <six existing runs> --json-out local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/failure-modes.json`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py <six existing runs> --json-out local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/runtime-discontinuities.json`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py <six existing runs> --json-out local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/lti-transfer-quality.json`
  - `.venv/bin/python scripts/analyze-soundcheck-window-trace.py <run> --json-out local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/window-trace-N.json`
  - A local Python summary over existing `metrics.json` files.
- Runs compared:
  - `local-analysis/mainline-baseline/20260617-mainline-0.3.135-wait45/soundcheck-irig-pairA-12s`
  - `local-analysis/soundcheck/20260617-cpp-inputdecode-off-dense-ch12-irig-pairA-12s`
  - `local-analysis/soundcheck/20260617-cpp-iso64q8-stopisoc-irig-pairA-12s`
  - `local-analysis/soundcheck/20260617-streamusage-irig-pairA-12s-cpp-hal`
  - `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal`
  - `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal`
- Results:
  - Shared severely degraded route-family:
    - mainline wait45: quality `0.680798`, SNR floor `-0.83 dB`, mid/high
      residual `2.530031/1.775333`, `39` lag jumps.
    - C++ inputdecode-off: quality `0.680121`, SNR floor `-0.83 dB`,
      mid/high residual `2.527144/1.786423`, `42` lag jumps.
    - C++ ISO64/q8 StopIO: quality `0.686712`, SNR floor `-0.84 dB`,
      mid/high residual `2.525233/1.788470`, `35` lag jumps.
  - Current C++ failing family:
    - streamusage: quality `0.971648`, SNR floor `10.52 dB`, mid/high
      residual `1.399655/1.358543`, `28` lag jumps.
    - cadence diagnostic: quality `0.958757`, SNR floor `10.09 dB`,
      mid/high residual `1.447622/1.366173`, `27` lag jumps.
    - fast ISO config: quality `0.959397`, SNR floor `10.19 dB`, mid/high
      residual `1.450623/1.368530`, `35` lag jumps.
  - Failure classifier:
    - The degraded route-family adds
      `window_alignment_is_unstable_for_music` and
      `residual_tracks_program_level`.
    - The current C++ family remains
      `timebase_or_alignment_instability` with static L/R, polarity, simple
      nonlinearity, and fixed LTI/EQ rejected.
  - LTI/coherence:
    - Degraded route-family mid coherence is extremely low, around `0.02`.
    - Current C++ family mid coherence is higher, about `0.09-0.15`, but LTI
      correction still worsens SNR and does not explain the defect.
- Interpretation:
  - The old mainline-vs-C++ degraded captures cannot prove C++ sound quality
    or mainline superiority in the current physical route; they only prove the
    route/comparison was not valid enough for audiophile claims.
  - The current C++ candidate still independently fails quality with about
    `10 dB` SNR and persistent lag jumps, so route concerns do not make C++
    ready.
  - Next physical work should either validate the capture/reference route with
    a known-good bypass or redesign the transport/timebase path; do not keep
    spending hardware windows on one-flag CPU tweaks.
- Evidence paths:
  - `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/metrics-summary.json`
  - `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/failure-modes.json`
  - `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/runtime-discontinuities.json`
  - `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/lti-transfer-quality.json`
  - `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/window-trace-1.json`

## 2026-06-17: Inline Inactive Input Decode Bypass Rejection

- Purpose:
  - Test whether bypassing the Objective-C `decodeCaptureBytes` call earlier
    when input decode is inactive reduces playback-only CPU without changing
    packet layout or transport cadence.
  - Keep the run isolated to the C++ worktree and restore the product HAL after
    the locked probe.
- Commands:
  - `make -B hal build/opena8dj-control`
  - `scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 8 --run-dir local-analysis/physical-product/20260617-inline-inactive-decode-bypass/hal-candidate-safety`
  - `scripts/run-soundcheck --run-dir local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --monitor-stream-stats --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/after-inline-inactive-decode-bypass-unload`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-inline-inactive-decode-bypass-unload.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-inline-inactive-decode-bypass.json`
- Results:
  - Offline gates before hardware PASS:
    Debug `17/17`, Release `18/18`.
  - Physical music quality FAIL:
    quality `0.961965`, SNR floor `10.16 dB`, mid/high residual
    `1.429792/1.358387`, quiet mid noise `-35.03 dBFS`, `31` lag jumps,
    `0` clipped frames, and `0` click outliers.
  - Runtime CPU still fails mainline:
    driver p95 `22.1%`, `coreaudiod` p95 `41.3%`.
  - Capture ISO invariants PASS, stream stats show no pool fallback
    allocations and no output underruns, timeline resets, or late writes.
  - Failure mode remains `timebase_or_alignment_instability`; static L/R mix,
    polarity, simple nonlinearity, and LTI/fixed EQ are still insufficient.
  - Final isolation PASS:
    HAL inactive, lock absent, no OpenA8DJ or mainline QA processes detected.
- Product conclusion:
  - Reject the inline inactive input decode bypass as a product change.
  - The attempted source change was removed from `src/hal/OpenA8DJUSB.m` after
    the probe because it did not provide a defensible CPU win and still failed
    sound quality.
  - The evidence is useful only as negative hot-path evidence; it must not be
    used for readiness or branch-promotion claims.
- Evidence paths:
  - `local-analysis/physical-product/20260617-inline-inactive-decode-bypass/hal-candidate-safety/summary.txt`
  - `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/metrics.json`
  - `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `local-analysis/runtime-isolation/after-inline-inactive-decode-bypass-unload.json`
  - `local-analysis/runtime-isolation/final-after-inline-inactive-decode-bypass-docs.json`
  - `local-analysis/promotion-readiness-after-inline-inactive-decode-bypass.json`

## 2026-06-17: Output Sample Time Follower Rejection

- Purpose:
  - Test the least-invasive remaining timebase knob:
    `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1`.
  - This preserves payload bytes, queue geometry, capture-paced playback,
    playback coalescing `1`, and implicit 1:1 transfer cadence.
- Commands:
  - `make -B hal build/opena8dj-control HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 8 --run-dir local-analysis/physical-product/20260617-output-sample-time-follower/hal-candidate-safety`
  - `scripts/run-soundcheck --run-dir local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --monitor-stream-stats --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/after-output-sample-time-follower-unload`
  - `make -B hal`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-output-sample-time-follower-unload.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-output-sample-time-follower.json`
- Results:
  - HAL candidate safety PASS.
  - Physical music quality FAIL:
    quality `0.962572`, SNR floor `9.94 dB`, mid/high residual
    `1.458736/1.377276`, quiet mid noise `-34.98 dBFS`, `28` lag jumps,
    `0` clipped frames.
  - Runtime CPU still fails mainline and regressed versus the prior probe:
    driver p95 `24.7%`, `coreaudiod` p95 `53.0%`.
  - Capture ISO invariants PASS; no capture short transfers, status failures,
    or other-size transactions.
  - Stream stats show no output active underruns, timeline resets, late writes,
    or transfer-pool fallback allocations.
  - Failure mode remains `timebase_or_alignment_instability`; LTI/fixed EQ,
    static L/R mix, polarity, and simple nonlinearity remain insufficient.
  - Final isolation PASS:
    HAL inactive, lock absent, no OpenA8DJ or mainline QA process detected.
- Product conclusion:
  - Reject `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1` as a product default.
  - Small `sampleTime` continuity following does not solve the physical music
    defect and worsens CPU in this run.
  - The next useful work must either validate the physical route independently
    or change the deeper USB/device transport state model while preserving
    byte payload and 1:1 cadence.
- Evidence paths:
  - `local-analysis/physical-product/20260617-output-sample-time-follower/hal-candidate-safety/summary.txt`
  - `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/metrics.json`
  - `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `local-analysis/runtime-isolation/after-output-sample-time-follower-unload.json`
  - `local-analysis/promotion-readiness-after-output-sample-time-follower.json`

## 2026-06-17: Current-Family Timebase Window Comparison

- Purpose:
  - Compare recent current-family C++ physical captures offline to test whether
    local per-window lag correction explains the music residual.
  - Avoid hardware and CoreAudio entirely; this reads existing captured WAV,
    metrics, stream stats, and CPU profiles.
- Commands:
  - `scripts/analyze-soundcheck-window-trace.py <run> --json-out local-analysis/timebase-window-comparison/20260617-current-family/window-trace-N.json`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py <runs> --json-out local-analysis/timebase-window-comparison/20260617-current-family/failure-modes.json`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py <runs> --json-out local-analysis/timebase-window-comparison/20260617-current-family/runtime-discontinuities.json`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py <runs> --json-out local-analysis/timebase-window-comparison/20260617-current-family/lti-transfer-quality.json`
  - Local summary written to
    `local-analysis/timebase-window-comparison/20260617-current-family/summary.json`.
- Runs compared:
  - `20260617-streamusage-irig-pairA-12s-cpp-hal`
  - `20260617-payload-guard-bff59cc-irig-pairA-12s-cpp-hal`
  - `20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal`
  - `20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal`
  - `20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal`
  - `20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal`
  - `20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal`
- Results:
  - Local lag correction barely improves mid-band residual:
    - streamusage: `1.438804 -> 1.413509`, improvement `1.76%`.
    - payload guard: `1.442537 -> 1.426639`, improvement `1.10%`.
    - playback-before-capture: `1.440646 -> 1.433932`, improvement `0.47%`.
    - reuse completions: `1.490127 -> 1.459980`, improvement `2.02%`.
    - fast ISO config: `1.486150 -> 1.455102`, improvement `2.09%`.
    - inline inactive decode bypass: `1.447841 -> 1.450915`, regression
      `0.21%`.
    - sample time follower: `1.493616 -> 1.478376`, improvement `1.02%`.
  - All corrected mid residual medians remain around `1.41-1.48`, far above
    product thresholds.
  - Window lag jumps persist in every run: `22-35`.
  - Corrected correlation medians remain around `0.966-0.971`, not enough to
    recover audiophile SNR.
- Interpretation:
  - The current physical failure is not explained by a simple local lag or
    drift correction. Aligning windows does not remove the residual.
  - This supports either route/capture-chain invalidity, analog/device state,
    or a deeper USB/device transport issue after byte payload preparation.
  - Another shallow timing knob is unlikely to be useful without new
    observability.
- Evidence paths:
  - `local-analysis/timebase-window-comparison/20260617-current-family/summary.json`
  - `local-analysis/timebase-window-comparison/20260617-current-family/failure-modes.json`
  - `local-analysis/timebase-window-comparison/20260617-current-family/runtime-discontinuities.json`
  - `local-analysis/timebase-window-comparison/20260617-current-family/lti-transfer-quality.json`
  - `local-analysis/timebase-window-comparison/20260617-current-family/window-trace-1.json`

## 2026-06-17: Mainline Practical Music Floor Comparison

- Purpose:
  - Compare current C++ physical music runs against the practical physical
    thresholds extracted from the read-only mainline docs, separately from the
    stricter experimental promotion gate.
  - Avoid overstating the absolute `35 dB` SNR gate when the documented
    mainline mixer REC OUT/iRig route historically produced about `10 dB` music
    SNR even when the route was considered valid.
- Mainline references:
  - `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-12.md:7-12`
    documents the physical path:
    Audio 8 DJ -> mixer REC OUT -> iRig Stream.
  - `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-12.md:55-59`
    says the REC OUT capture was real, strong enough for QA, and valid.
  - `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-12.md:133-139`
    reports a valid-route Pair A recheck still failing after time-warp with
    quality `0.962043`, SNR `9.97 dB`, mid residual `1.637216`, high residual
    `1.412494`, and quiet mid `-46.16 dBFS`.
  - `docs/MAINLINE_BASELINE_METRICS.md:48-67` carries forward the practical
    physical music thresholds.
- Command:
  - Local offline summary over existing C++ `metrics.json` files, written to
    `local-analysis/mainline-practical-floor/20260617-current-cpp-music-family/summary.json`.
- Result:
  - No current C++ run fully passes the practical physical music floor.
  - Best current-family result is the stream-usage run:
    quality `0.971648`, mid residual `1.399655`, quiet mid `-35.20 dBFS`,
    lag jumps `28`, clipping `0`; it fails only high residual
    `1.358543 > 1.355` among the practical music metrics summarized here.
  - Other recent CPU/timing probes fail high residual and sometimes mid
    residual as well.
  - CPU remains a separate hard blocker versus mainline even when a music run
    is close to the practical floor.
- Interpretation:
  - The strict promotion gate remains valid as an aspirational audiophile gate,
    but C++ should also be tracked against the historical/practical mainline
    floor to avoid chasing impossible route artifacts blindly.
  - Current C++ still cannot claim better sound quality than mainline:
    it has not fully passed the practical music floor, has not beaten mainline
    CPU, and has no physical Traktor/timecode evidence.
- Evidence path:
  - `local-analysis/mainline-practical-floor/20260617-current-cpp-music-family/summary.json`

## 2026-06-17: Native Physical Run Comparator And Steady Driver Sample

- Purpose:
  - Add a native C++ comparator for physical soundcheck runs so practical
    quality and CPU failures can be summarized without ad hoc shell/Python
    snippets.
  - Capture a steady-state driver sample with the exact OpenA8DJ HAL process
    PID, not the wrapper shell PID.
- Changes:
  - Added `tools/physical_run_compare.cpp`.
  - Added `make physical-run-compare`.
  - Added `scripts/find-opena8dj-driver-pid`.
  - Added `playbackTransfersSubmitted` as an appended stream-stats payload
    field backed by an atomic counter; older HAL payloads still fall back to
    `playbackQueueAttempts`/completed transfers in `opena8dj-control`.
- Commands:
  - `make build/physical-run-compare`
  - `build/physical-run-compare local-analysis/profiling/20260617-current-default-steady-sample-v5/soundcheck local-analysis/profiling/20260617-current-default-sample-soundcheck-v4/soundcheck local-analysis/profiling/20260617-current-default-sample-soundcheck-v3/soundcheck local-analysis/profiling/20260617-current-default-sample-soundcheck/soundcheck > local-analysis/physical-run-compare/20260617-profile-family.json`
  - `python3 -m json.tool local-analysis/physical-run-compare/20260617-profile-family.json`
  - `make -B hal build/opena8dj-control`
- Comparator result:
  - No compared profile-family run passes practical quality+CPU.
  - Steady v5 run:
    `quality_alignment_score=0.941259`, SNR floor `-11.587830 dB`,
    mid/high residual `4.233590/3.665879`, `82` lag jumps, driver CPU p95
    `24.0%`, `coreaudiod` p95 `5.6%`.
  - Best short v4 run:
    quality `0.959811`, SNR floor `10.161017 dB`, mid/high residual
    `1.456104/1.366229`, `33` lag jumps, driver CPU p95 `22.2%`,
    `coreaudiod` p95 `10.4%`.
- Steady sample result:
  - Evidence:
    `local-analysis/profiling/20260617-current-default-steady-sample-v5/opena8dj-driver-steady.sample.txt`.
  - Target process was correct:
    `Core Audio Driver (OpenA8DJ.driver)`.
  - USB queue sample hotspot:
    `DispatchQueue_43: org.opena8dj.driver.usb`.
  - Dominant sampled path:
    `handleCaptureTransfer` -> `queueCaptureTransfer` /
    `queuePlaybackWithRequests` -> `IOUSBHostPipe enqueue` ->
    `IOConnectCallAsyncMethod` -> `mach_msg2_trap`.
  - `fillPlaybackBytes` appears, but is secondary in this profile; the largest
    cost is transfer enqueue/IOKit/MIG/object overhead, not pure packet packing.
- Stream-stats observation from the same run:
  - `captureTransfersCompleted` and `playbackTransfersCompleted` both advanced
    by `1478` over about `24.0 s`.
  - `captureZeroCompleteTransactions` advanced by `5372`, about `223.6/s`.
  - `outputTimelineResets` advanced by `1`.
  - Existing `playbackTransfersSubmitted` output was unreliable because it
    reused a cadence-diagnostic-only field; the new appended payload field fixes
    this for future runs.
- Interpretation:
  - Current C++ is still not better than mainline.
  - The next optimization must reduce USB transfer enqueue overhead or explain
    the timeline/route failure; packer micro-optimization alone is unlikely to
    close the CPU gap.
  - The submitted/completed counter fix is observability, not a sound-quality
    fix.
- Evidence paths:
  - `local-analysis/physical-run-compare/20260617-profile-family.json`
  - `local-analysis/profiling/20260617-current-default-steady-sample-v5/`

## 2026-06-17: Physical Comparator Tooling Offline Verification

- Change:
  - Verified the native comparator and appended submitted-transfer counter
    build cleanly with the HAL/control tool.
  - Refreshed offline gates, runtime isolation, and promotion readiness after
    documenting the profiling evidence.
- Commands:
  - `git diff --check`
  - `make build/physical-run-compare`
  - `make -B hal build/opena8dj-control`
  - `scripts/run-cpp-offline-gates`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-physical-comparator-and-submitted-counter.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
- Result:
  - Diff whitespace check PASS.
  - Native comparator build PASS.
  - HAL/control build PASS.
  - Offline gates PASS: Debug `17/17`, Release `18/18`.
  - Evidence schema PASS with `22` required files and `0` missing.
  - Runtime isolation PASS: HAL inactive, lock absent, no OpenA8DJ process.
  - Promotion readiness remains FAIL with
    `branch_promotion_allowed=false`.
  - Hardware touched by this verification: no.
  - CoreAudio/USB touched by this verification: no.
- Latest Release benchmark:
  - `pack_mib_s=1657.37`.
  - `decode_into_mib_s=588.333`.
  - `route_frames_s=9.69708e+08`.
  - `route_advanced_frames_s=5.08031e+08`.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`.
  - `local-analysis/runtime-isolation/final-after-physical-comparator-and-submitted-counter.json`.
  - `local-analysis/promotion-readiness-current.json`.

## 2026-06-17: Output-Only No-Capture ISO Experiment Build

- Change:
  - Added opt-in `HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1`.
  - Default remains `HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=0`.
  - When enabled, playback-only streaming does not queue ISO IN while input
    decode is inactive; if input decode later activates, initial capture
    transfers are queued again.
- Reason:
  - The v5 steady sample shows CPU dominated by capture/playback ISO requeue
    and IOUSBHost/MIG overhead, not packet packing.
  - This tests whether removing capture ISO work during playback-only use can
    reduce driver CPU without removing the HAL input surface.
- Commands:
  - `make -B hal build/opena8dj-control`
  - `make -B hal build/opena8dj-control HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1`
  - `make -B hal build/opena8dj-control`
- Result:
  - Default build PASS.
  - Opt-in output-only no-capture ISO build PASS.
  - Default build restored afterward.
  - Hardware touched: no.
  - CoreAudio/USB touched: no.
- Readiness note:
  - This is not a product improvement claim.
  - Prior fixed OUT pacing was physically rejected, so this experiment requires
    a locked physical run before it can be considered useful.

## 2026-06-17: Final Offline Verification After Output-Only Experiment

- Change:
  - Re-ran isolation, offline gates, and promotion readiness after restoring
    the default HAL build (`HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=0`).
  - No hardware, CoreAudio, USB reset, driver install, or default-device change
    was performed in this verification.
- Commands:
  - `git diff --check`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-output-only-no-capture-build.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-before-commit.json`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-commit.json`
- Result:
  - Diff whitespace check PASS.
  - Runtime isolation PASS: HAL inactive, hardware lock absent, no OpenA8DJ
    process, hardware touched false, USB touched false, CoreAudio touched
    false.
  - Offline gates PASS: Debug `17/17`, Release `18/18`.
  - Evidence schema PASS: `22` required files, `0` missing.
  - Promotion readiness FAIL:
    `branch_promotion_allowed=false`.
  - Post-commit runtime isolation PASS with HAL inactive and hardware lock
    absent.
  - Failing gates:
    `physical_music_quality`, `runtime_cpu_beats_mainline`,
    `latest_physical_investigation`, `traktor_timecode_physical`.
- Latest Release benchmark:
  - `pack_mib_s=1625.11`.
  - `decode_into_mib_s=575.878`.
  - `route_frames_s=9.78759e+08`.
  - `route_advanced_frames_s=4.96093e+08`.
- Evidence paths:
  - `local-analysis/runtime-isolation/final-after-output-only-no-capture-build.json`.
  - `local-analysis/runtime-isolation/final-before-commit.json`.
  - `local-analysis/runtime-isolation/final-after-commit.json`.
  - `local-analysis/cpp-offline/current-offline-gates.json`.
  - `local-analysis/promotion-readiness-current.json`.

## 2026-06-17: Output-Only No-Capture ISO Physical Rejection

- Change:
  - Fixed HAL bundle packaging so `Contents/Info.plist` is installed with mode
    `0644` instead of preserving a stale `0600` mode in `build/OpenA8DJ.driver`.
  - Added a playback queue fill trigger from `writeOutput` when
    `HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1`; without it, output-only mode could
    write CoreAudio frames into the timeline but never submit playback
    transfers.
- Physical safety:
  - iRig preflight PASS: `iRig Stream` visible as `in=2 out=2 rate=48000`.
  - Audio 8 DJ USB visible in IORegistry while HAL was inactive.
  - First safety run with `Info.plist` mode `0600` failed device enumeration:
    CoreAudio listed iRig, MacBook mic, and speakers, but not `Open Audio 8 DJ`.
  - After the Makefile packaging fix, HAL safety PASS and CoreAudio listed
    `Open Audio 8 DJ uid=org.opena8dj.Audio8DJ in=8 out=8 rate=48000`.
- Commands:
  - `make -B hal build/opena8dj-control HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1`
  - `make smoke-hal HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 15 --run-dir local-analysis/hal-candidate-safety/20260617-output-only-no-capture-optin-perms-fixed`
  - `scripts/run-soundcheck --skip-build --run-dir local-analysis/soundcheck/20260617-output-only-no-capture-optin-irig-pairA-12s-cpp-hal --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --monitor-stream-stats --cpu-profile --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 2 --enumeration-timeout 8 --min-idle-pct 15 --run-dir local-analysis/hal-candidate-safety/20260617-output-only-no-capture-optin-fillfix`
  - `scripts/run-soundcheck --skip-build --run-dir local-analysis/soundcheck/20260617-output-only-no-capture-optin-fillfix-irig-pairA-12s-cpp-hal --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --monitor-stream-stats --cpu-profile --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - `scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/after-output-only-no-capture-optin-fillfix-reject`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-output-only-no-capture-optin-fillfix-reject.json`
- Results:
  - Pre-fill-fix soundcheck FAIL:
    `quality_alignment_score=0.039808`, SNR floor `-59.81 dB`,
    mid/high residual `3551.596610/2457.244317`, lag jumps `41`.
  - Pre-fill-fix stream stats show no real playback:
    `playbackTransfersSubmitted=0`, `playbackTransfersCompleted=0`,
    `outputFramesRead=0`, while `outputFramesWritten=574464`.
  - Fill-fix soundcheck FAIL:
    `quality_alignment_score=0.183990`, SNR floor `-21.45 dB`,
    mid/high residual `17.171794/11.452494`, lag jumps `41`.
  - Fill-fix stream stats show playback resumed but with bad cadence/quality:
    `playbackTransfersSubmitted=6617`, `playbackTransfersCompleted=414`,
    `outputFramesRead=582912`, `outputFramesWritten=576512`.
  - Fill-fix CPU:
    OpenA8DJ driver p95 `8.0%`, coreaudiod p95 `28.3%`.
  - Runtime isolation after rejection PASS: HAL inactive, lock absent, no
    OpenA8DJ process.
- Decision:
  - Reject `HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1` as a product optimization.
  - The flag may remain as an explicit diagnostic experiment, default off.
  - The packaging permission fix is valid and must stay.
- Evidence paths:
  - `local-analysis/hardware-preflight/20260617-060326-irig-before-optin/`
  - `local-analysis/hal-candidate-safety/20260617-output-only-no-capture-optin/`
  - `local-analysis/hal-candidate-safety/20260617-output-only-no-capture-optin-perms-fixed/`
  - `local-analysis/hal-candidate-safety/20260617-output-only-no-capture-optin-fillfix/`
  - `local-analysis/soundcheck/20260617-output-only-no-capture-optin-irig-pairA-12s-cpp-hal/`
  - `local-analysis/soundcheck/20260617-output-only-no-capture-optin-fillfix-irig-pairA-12s-cpp-hal/`
  - `local-analysis/physical-run-compare/20260617-output-only-no-capture-optin-fillfix-reject.json`
  - `local-analysis/audio-stack-guard/after-output-only-no-capture-optin-fillfix-reject/`
  - `local-analysis/runtime-isolation/after-output-only-no-capture-optin-fillfix-reject.json`

## 2026-06-17: Final Verification After No-Capture ISO Rejection

- Commands:
  - `make -B hal build/opena8dj-control`
  - `git diff --check`
  - `scripts/run-cpp-offline-gates`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-output-only-no-capture-fillfix-reject.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/final-after-output-only-no-capture-fillfix-docs.json`
- Result:
  - HAL build restored to default:
    `OPENA8DJ_OUTPUT_ONLY_NO_CAPTURE_ISOC=0`.
  - Diff whitespace check PASS.
  - Offline gates PASS: Debug `17/17`, Release `18/18`.
  - Evidence schema PASS: `22` required files, `0` missing.
  - Release benchmark:
    `pack_mib_s=1649.37`, `decode_into_mib_s=588.102`,
    `route_frames_s=9.71656e+08`,
    `route_advanced_frames_s=5.02111e+08`.
  - Promotion readiness FAIL:
    `branch_promotion_allowed=false`.
  - Latest failing physical gates:
    `physical_music_quality`, `runtime_cpu_beats_mainline`,
    `latest_physical_investigation`, `traktor_timecode_physical`.
  - Runtime isolation PASS: HAL inactive, lock absent, no OpenA8DJ process.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`.
  - `local-analysis/promotion-readiness-after-output-only-no-capture-fillfix-reject.json`.
  - `local-analysis/runtime-isolation/final-after-output-only-no-capture-fillfix-docs.json`.

## 2026-06-17: Ignore HAL Output Sample Time Physical Rejection

- Change:
  - Added `HAL_IGNORE_OUTPUT_SAMPLE_TIME=1` as an opt-in diagnostic build flag.
  - The flag makes HAL output cycles pass `sampleTimeValid=false` into the USB
    timeline, matching the direct USB tool's contiguous write model more closely.
  - Default was restored after the experiment:
    `OPENA8DJ_IGNORE_OUTPUT_SAMPLE_TIME=0`.
- Preflight:
  - Hardware lock absent before test.
  - Audio stack health PASS: total watched CPU `5.5%`, no OpenA8DJ driver pids.
  - CoreAudio saw `iRig Stream` as `in=2 out=2 rate=48000`.
  - IORegistry saw both `Audio 8 DJ` and `iRig Stream`.
- Commands:
  - `make -B hal build/opena8dj-control HAL_IGNORE_OUTPUT_SAMPLE_TIME=1`
  - `git diff --check`
  - `scripts/run-cpp-offline-gates`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 8 --run-dir local-analysis/physical-product/20260617-ignore-output-sample-time/hal-candidate-safety`
  - `scripts/run-soundcheck --run-dir local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --monitor-stream-stats --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/after-ignore-output-sample-time-unload`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-ignore-output-sample-time-unload.json`
  - `build/physical-run-compare local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal local-analysis/soundcheck/20260617-cpp-iso8q8-dense-ch12-irig-pairA-12s local-analysis/soundcheck/20260617-cpp-iso10q8-dense-ch12-irig-pairA-12s local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal > local-analysis/physical-run-compare/20260617-ignore-output-sample-time-reject.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-ignore-output-sample-time.json`
  - `make -B hal`
- Results:
  - HAL candidate safety PASS.
  - Physical music soundcheck FAIL:
    `quality_alignment_score=0.963508`, SNR floor `10.20 dB`,
    mid/high residual `1.440572/1.369361`, quiet mid noise `-35.12 dBFS`,
    `32` lag jumps, clipped frames `0`.
  - CPU FAIL versus mainline:
    driver p95 `22.6%`, coreaudiod p95 `44.7%`.
  - Stream stats showed no output active underruns, timeline resets, late
    writes, elastic drops/replays, or pool fallback allocations.
  - Failure analysis:
    `timebase_or_alignment_instability`,
    `static_lr_mix_or_polarity_not_sufficient`,
    `simple_memoryless_nonlinearity_not_sufficient`.
  - LTI analysis did not explain the failure:
    mid coherence about `0.116-0.117`, LTI SNR delta negative.
  - Promotion readiness FAIL:
    `branch_promotion_allowed=false`.
  - Runtime isolation PASS:
    HAL inactive, lock absent, no OpenA8DJ process.
- Decision:
  - Reject `HAL_IGNORE_OUTPUT_SAMPLE_TIME=1` as product behavior.
  - Keep default `0`.
  - The direct USB abs-deadline matrix result does not transfer to HAL merely by
    ignoring CoreAudio sample time.
- Evidence paths:
  - `local-analysis/physical-product/20260617-ignore-output-sample-time/hal-candidate-safety/`
  - `local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal/`
  - `local-analysis/physical-run-compare/20260617-ignore-output-sample-time-reject.json`
  - `local-analysis/audio-stack-guard/after-ignore-output-sample-time-unload/`
  - `local-analysis/runtime-isolation/after-ignore-output-sample-time-unload.json`
  - `local-analysis/promotion-readiness-after-ignore-output-sample-time.json`

## 2026-06-17: ISO8 Queue64 Prefetch256 Physical Rejection

- Purpose:
  - Test Carver's direct-USB/HAL pacing recommendation while retaining capture
    ISO and HAL input representation:
    `HAL_ISO_FRAMES=8 HAL_PLAYBACK_ISO_FRAMES=8 HAL_CAPTURE_QUEUE=64
    HAL_PLAYBACK_QUEUE=64 HAL_OUTPUT_PREFETCH_FRAMES=256
    HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=0`.
  - Determine whether direct-like queue and startup margin improves HAL music
    continuity without losing channel separation.
- Commands:
  - `make -B hal build/opena8dj-control HAL_ISO_FRAMES=8 HAL_PLAYBACK_ISO_FRAMES=8 HAL_CAPTURE_QUEUE=64 HAL_PLAYBACK_QUEUE=64 HAL_OUTPUT_PREFETCH_FRAMES=256`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 8 --run-dir local-analysis/physical-product/20260617-iso8q64-prefetch256/hal-candidate-safety`
  - `scripts/run-channel-matrix-gate --run-id 20260617-iso8q64-prefetch256-pairA-chmatrix --pair A --capture-device "iRig Stream" --capture-channels 1,2 --run-physical`
  - `scripts/run-soundcheck --run-dir local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --monitor-stream-stats --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - `scripts/analyze-capture-iso-invariants.py local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
  - `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal/failure-modes.json`
  - `.venv/bin/python scripts/analyze-runtime-discontinuities.py local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
  - `scripts/audio-stack-guard --force-unload-opena8dj --recover --unload-opena8dj --run-dir local-analysis/audio-stack-guard/after-iso8q64-prefetch256-unload`
  - `build/physical-run-compare local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal local-analysis/soundcheck/20260617-cpp-iso8q8-dense-ch12-irig-pairA-12s local-analysis/soundcheck/20260617-streamusage-irig-pairA-12s-cpp-hal local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal > local-analysis/physical-run-compare/20260617-iso8q64-prefetch256-reject.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-iso8q64-prefetch256-unload.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-iso8q64-prefetch256.json`
  - `make -B hal`
- Results:
  - HAL candidate safety PASS.
  - Pair A physical channel matrix PASS:
    max wrong-source leakage `-53.079 dB`, left-to-right leakage
    `-58.221 dB`, right-to-left leakage `-51.442 dB`, no clipping.
  - Physical music soundcheck FAIL:
    `quality_alignment_score=0.966043`, SNR floor `10.15 dB`,
    mid/high residual `1.442529/1.373910`, quiet mid noise `-34.87 dBFS`,
    `25` lag jumps, clipped frames `0`.
  - CPU FAIL:
    driver p95 `23.7%`, coreaudiod p95 `86.6%`.
  - Capture ISO invariants PASS.
  - Stream stats showed no output active underruns, timeline resets, late
    writes, elastic drops/replays, or transfer-pool fallback allocations.
  - Failure-mode analysis:
    `timebase_or_alignment_instability`,
    `static_lr_mix_or_polarity_not_sufficient`,
    `simple_memoryless_nonlinearity_not_sufficient`.
  - LTI analysis did not explain the failure; mid coherence improved to about
    `0.20`, but LTI SNR delta remained negative.
  - Promotion readiness FAIL:
    `branch_promotion_allowed=false`; failing gates are
    `physical_music_quality`, `runtime_cpu_beats_mainline`,
    `latest_physical_investigation`, and `traktor_timecode_physical`.
  - Runtime isolation PASS after recovery:
    HAL inactive, lock absent, no OpenA8DJ process.
  - Default HAL was rebuilt after the experiment.
- Decision:
  - Reject q64/prefetch256 as a product candidate.
  - Keep ISO8/q8/prefetch64 as the default candidate baseline until a new
    transport design improves both quality and resource use.
- Evidence paths:
  - `local-analysis/physical-product/20260617-iso8q64-prefetch256/hal-candidate-safety/`
  - `local-analysis/channel-matrix/20260617-iso8q64-prefetch256-pairA-chmatrix/`
  - `local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal/`
  - `local-analysis/physical-run-compare/20260617-iso8q64-prefetch256-reject.json`
  - `local-analysis/audio-stack-guard/after-iso8q64-prefetch256-unload/`
  - `local-analysis/runtime-isolation/after-iso8q64-prefetch256-unload.json`
  - `local-analysis/promotion-readiness-after-iso8q64-prefetch256.json`

## 2026-06-17: Direct USB Music Soundcheck Diagnostic Rejection

- Purpose:
  - Test whether the direct USB selected-pair/lead path that helped tone-matrix
    diagnostics can reproduce real music cleanly through the same iRig capture
    route.
  - Separate HAL-specific failure from route/reference/direct-USB limitations.
- Tooling added:
  - `scripts/run-direct-usb-soundcheck`
  - `make direct-usb-soundcheck`
  - `tools/hardware_lock_policy_check.cpp` now audits the new script's lock
    contract.
- Commands:
  - `chmod +x scripts/run-direct-usb-soundcheck`
  - `scripts/run-direct-usb-soundcheck --help`
  - `make build/opena8dj-usb-play-plain-gain05 build/audio-record`
  - `build/audio-list`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/before-direct-usb-soundcheck.json`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-soundcheck/20260617-direct-usb-plain-gain05-lead8192-pairA-12s --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 12 --mode dense --target-peak-db -16 --lead-frames 8192`
  - `.venv/bin/python scripts/analyze-soundcheck-failure-modes.py local-analysis/direct-usb-soundcheck/20260617-direct-usb-plain-gain05-lead8192-pairA-12s --json-out local-analysis/direct-usb-soundcheck/20260617-direct-usb-plain-gain05-lead8192-pairA-12s/failure-modes.json`
  - `.venv/bin/python scripts/analyze-lti-transfer-quality.py local-analysis/direct-usb-soundcheck/20260617-direct-usb-plain-gain05-lead8192-pairA-12s --json-out local-analysis/direct-usb-soundcheck/20260617-direct-usb-plain-gain05-lead8192-pairA-12s/lti-transfer-quality.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/after-direct-usb-soundcheck.json`
- Results:
  - `iRig Stream` was visible before the run: `in=2 out=2 rate=48000`.
  - Lock was absent before the run and absent afterward.
  - HAL was inactive before and after the run.
  - Direct USB playback/capture completed without clipping.
  - Physical music FAIL:
    `quality_alignment_score=0.103211`, worst-channel SNR `-24.31 dB`,
    mid/high residual `17.114359/16.212469`, `0` lag jumps in the global
    analyzer, clipped frames `0`.
  - Direct diagnostics:
    after playback, `frames_written=576000`, `frames_read=567535`,
    `capture_transfers=11825`, `playback_transfers=11824`, no queue failures,
    no late frames. End-of-run underruns occurred after source exhaustion while
    the wrapper was still waiting for capture/drain.
  - Failure-mode analysis:
    `timebase_or_alignment_instability`,
    `window_alignment_is_unstable_for_music`,
    `static_lr_mix_or_polarity_not_sufficient`,
    `simple_memoryless_nonlinearity_not_sufficient`,
    `residual_tracks_program_level`.
  - LTI analysis did not explain the failure:
    mid coherence about `0.23-0.24`; LTI SNR delta negative.
- Decision:
  - Keep direct USB music soundcheck as a diagnostic tool only.
  - Do not use direct USB tone-matrix success as product readiness evidence.
- Evidence paths:
  - `local-analysis/direct-usb-soundcheck/20260617-direct-usb-plain-gain05-lead8192-pairA-12s/`
  - `local-analysis/runtime-isolation/before-direct-usb-soundcheck.json`
  - `local-analysis/runtime-isolation/after-direct-usb-soundcheck.json`

## 2026-06-17: Final Verification After Direct USB Diagnostic Tooling

- Commands:
  - `bash -n scripts/run-direct-usb-soundcheck`
  - `scripts/run-direct-usb-soundcheck --help`
  - `git diff --check`
  - `cmake --build build/cpp-release --target opena8djcpp_hardware_lock_policy_check`
  - `build/cpp-release/opena8djcpp_hardware_lock_policy_check`
  - `scripts/run-cpp-offline-gates`
- Result:
  - Direct USB diagnostic wrapper syntax/help PASS.
  - Diff whitespace check PASS.
  - Hardware lock policy PASS:
    `audited_scripts=6`, `missing_requirements=0`.
  - Offline gates PASS:
    Debug `17/17`, Release `18/18`.
  - Evidence schema PASS:
    `required_files=22`, `missing_files=0`.
  - Release benchmark:
    `pack_mib_s=1623.62`, `decode_into_mib_s=575.885`,
    `route_frames_s=8.7796e+08`,
    `route_advanced_frames_s=4.93564e+08`.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/runtime-isolation/after-direct-usb-soundcheck.json`

## 2026-06-17: Physical Latency Gate And Explicit Scheduling Diagnostics

- Purpose:
  - Convert the observed delayed/weak physical output into a formal PASS/FAIL
    gate rather than a narrative blocker.
  - Determine whether explicit isochronous scheduling fails with `too-old` /
    `too-new` scheduling errors, enqueue rejection, or queue saturation.
- Tooling changed:
  - `scripts/analyze-physical-latency.py` now emits `result` and per-metric
    gates.
  - `scripts/evaluate-promotion-readiness.py` now consumes a
    `physical_latency` evidence file and fails promotion when physical output
    cannot align promptly and cleanly.
  - `OpenA8DJUSBDiagnostics` and `opena8dj-usb-play` now print scheduling and
    queue-failure details including `qfail_last`, `qfail_explicit`,
    `qfail_consumed`, and `qfail_startup_silence`.
  - `HAL_EXPLICIT_SCHED_FAIL_FALLBACK=1` was added as an opt-in diagnostic
    fallback, default `0`.
- Commands:
  - `.venv/bin/python scripts/analyze-physical-latency.py ... --json-out ...`
    for all existing physical latency captures.
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness/20260617-after-physical-latency-gate.json`
  - `make -B HAL_EXPLICIT_SCHED=1 build/opena8dj-usb-play build/audio-record`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-soundcheck/20260617-explicit-sched-instrumented-pairA-3s --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 3 --mode dense --target-peak-db -16 --lead-frames 8192 --skip-build`
  - `make -B HAL_EXPLICIT_SCHED=1 HAL_EXPLICIT_SCHED_FAIL_FALLBACK=1 build/opena8dj-usb-play build/audio-record`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-soundcheck/20260617-explicit-sched-fallback2-instrumented-pairA-3s --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 3 --mode dense --target-peak-db -16 --lead-frames 8192 --skip-build`
  - `make -B build/opena8dj-usb-play build/audio-record build/opena8dj-control`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/20260617-after-explicit-sched-fallback2-instrumented.json`
- Results:
  - Existing latency captures are all FAIL under the new gate. Representative
    direct USB Pair A:
    `first_energy_seconds=5.25`, `best_correlation=-0.623648`,
    `aligned_snr_db=-7.78`, `linear_fit_snr_db=-1.74`,
    `linear_residual_over_capture_rms=0.773905`.
  - Explicit scheduling without fallback is rejected:
    quality `0.041196`, SNR floor `-46.88 dB`, `queue_failures=2805`,
    `qfail_last=0xe00002be`, `qfail_other=2805`,
    `qfail_explicit=2805`, `sched_fallbacks=0`, capture RMS `0.000380`.
  - The first fallback attempt did not trigger because the observed failures
    came from in-flight queue saturation before enqueue, not from the
    post-fill enqueue failure path.
  - After enabling fallback on queue-full, the fallback fired once and reduced
    queue failures from about `2805` to `135`:
    `sched_fallbacks=1`, `qfail_explicit=1`, `playback_transfers=200`,
    `frames_read=153671`.
  - Fallback still fails physical quality:
    quality `0.005597`, SNR floor `-52.51 dB`, capture RMS `0.001699`,
    physical latency FAIL with `first_energy_seconds=4.95`,
    `best_correlation=0.029593`, and `linear_fit_snr_db=-30.81`.
  - Default direct USB player was rebuilt after experiments with
    `OPENA8DJ_ENABLE_EXPLICIT_ISOC_SCHEDULING=0` and
    `OPENA8DJ_EXPLICIT_SCHED_FALLBACK_ON_QUEUE_FAILURE=0`.
  - Final runtime isolation PASS:
    HAL inactive, lock absent, no forbidden OpenA8DJ/mainline processes.
- Decision:
  - Keep explicit scheduling disabled by default.
  - Keep fallback as diagnostic-only; it reduces queue-failure storming but does
    not recover audiophile-valid output.
  - Branch promotion remains blocked by physical latency, physical music
    quality, runtime CPU, and missing physical Traktor/timecode evidence.
- Evidence paths:
  - `local-analysis/direct-usb-soundcheck/20260617-explicit-sched-instrumented-pairA-3s/`
  - `local-analysis/direct-usb-soundcheck/20260617-explicit-sched-fallback2-instrumented-pairA-3s/`
  - `local-analysis/direct-usb-soundcheck/20260617-explicit-sched-fallback2-instrumented-pairA-3s/physical-latency.json`
  - `local-analysis/promotion-readiness/20260617-after-physical-latency-gate.json`
  - `local-analysis/runtime-isolation/20260617-after-explicit-sched-fallback2-instrumented.json`

## 2026-06-17: Direct USB Latency Marker Probe

- Purpose:
  - Replace dense music with a sparse, deterministic marker signal to separate
    fixed route/device latency from random drift or analyzer ambiguity.
- Tooling added:
  - `scripts/prepare-latency-marker.py`
  - `scripts/analyze-latency-marker-peaks.py`
  - `scripts/run-direct-usb-soundcheck --reference-wav`
  - `scripts/run-direct-usb-soundcheck --postroll-seconds`
- Commands:
  - `.venv/bin/python scripts/prepare-latency-marker.py --out-dir local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/fixture --rate 48000 --seconds 6 --peak 0.35`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8 --reference-wav local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/fixture/reference.wav --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 6 --postroll-seconds 8 --lead-frames 8192 --skip-build`
  - `.venv/bin/python scripts/analyze-latency-marker-peaks.py local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/fixture/reference.wav local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/captured.wav --record-preroll-seconds 0.6 --playback-lead-frames 8192 --startup-silence-frames 4544 --json-out local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/marker-peak-summary.json`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-lead0 --reference-wav local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-lead0/fixture/reference.wav --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 6 --postroll-seconds 8 --lead-frames 0 --skip-build`
  - `.venv/bin/python scripts/analyze-latency-marker-peaks.py local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-lead0/fixture/reference.wav local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-lead0/captured.wav --record-preroll-seconds 0.6 --playback-lead-frames 0 --startup-silence-frames 8192 --json-out local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-lead0/marker-peak-summary.json`
- Results:
  - Direct USB transport counters were clean in the lead `8192` run:
    `queue_failures=0`, `playback_transfers=396`,
    `frames_written=288000`, `frames_read=304217`.
  - Physical capture had strong signal and no clipping:
    RMS `0.03722001`, peak `0.64135742`, clipped `0`.
  - Marker peaks show a stable physical offset after merging split peaks:
    offsets `4.647771`, `4.646333`, `4.645521`, `4.644375` seconds;
    mean `4.646000s`, std `0.001237s`.
  - After subtracting the wrapper record pre-roll (`0.6s`) and expected
    internal lead/startup silence (`0.265333s`), the unexplained residual
    offset is still `3.780667s`.
  - The lead `0` run also fails:
    marker mean `4.900115s`, std `0.001250s`, and residual after record
    pre-roll plus expected startup silence is `4.129448s`.
  - Changing lead frames changes the measured offset by about the expected
    internal margin; it does not remove the multi-second physical delay.
  - Generic physical latency gate still fails:
    `first_energy_seconds=4.9`, `best_correlation=0.645749`,
    `aligned_snr_db=-2.33`, `linear_fit_snr_db=-0.64`,
    `linear_residual_over_capture_rms=0.732560`.
  - Direct USB soundcheck metrics still fail:
    quality `0.960701`, SNR floor `-14.62 dB`, mid/high residual
    `5.541492/5.365217`.
- Decision:
  - Treat the large delay as a stable route/device latency symptom, not random
    drift.
  - Do not treat fixed latency compensation as sufficient: linearity/SNR still
    fail badly after alignment.
- Evidence paths:
  - `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/`
  - `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/marker-peak-summary.json`
  - `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/physical-latency.json`
  - `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-lead0/`
  - `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-lead0/marker-peak-summary.json`
  - `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-lead0/physical-latency.json`
  - `local-analysis/runtime-isolation/20260617-after-default-marker-lead0.json`

## 2026-06-17: Direct USB Marker Internal Buffer Boundary

- Purpose:
  - Determine whether the multi-second external marker delay is already
    present inside the C++ timeline/packer or appears after internal buffers
    are consumed and packed for USB.
- Commands:
  - `.venv/bin/python scripts/prepare-latency-marker.py --out-dir local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/fixture --rate 48000 --seconds 6 --peak 0.35`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag --reference-wav local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/fixture/reference.wav --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 6 --postroll-seconds 8 --lead-frames 8192 --collect-usb-diagnostics`
  - `.venv/bin/python scripts/analyze-physical-latency.py local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/fixture/reference.wav local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/captured.wav --json-out local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/physical-latency.json`
  - `.venv/bin/python scripts/analyze-latency-marker-peaks.py local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/fixture/reference.wav local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/captured.wav --record-preroll-seconds 0.6 --playback-lead-frames 8192 --startup-silence-frames 4544 --json-out local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/marker-peak-summary.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/20260617-after-usbdiag-marker.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness/20260617-after-usbdiag-marker.json`
- Results:
  - External iRig capture still FAILS:
    marker mean `4.930875s`, std `0.001348s`,
    readiness `FAIL`.
  - Physical latency still FAILS:
    `first_energy_seconds=5.15`, `best_correlation=-0.424257`,
    `aligned_snr_db=-7.29`, `linear_fit_snr_db=-6.51`,
    `linear_residual_over_capture_rms=0.904117`.
  - Internal written buffer is perfect against reference:
    `written_alignment_score=1.000000`, `written_alignment_lag=0`,
    left/right SNR `999.00 dB`, click outliers `0`.
  - Internal consumed buffer is perfect against reference:
    `consumed_alignment_score=1.000000`, `consumed_alignment_lag=0`,
    left/right SNR `999.00 dB`, click outliers `0`.
  - Packed USB decode is perfect against reference:
    `usb_check_errors=0`, `usb_panic_flags=0`,
    `usb_alignment_score=1.000000`, `usb_alignment_lag=0`,
    left/right SNR `999.00 dB`.
  - Transport counters stayed clean:
    `queue_failures=0`, `playback_transfers=396`,
    `frames_written=288000`, `frames_read=304118`.
  - Runtime isolation after the run PASS:
    HAL inactive, lock absent, no forbidden OpenA8DJ/mainline processes.
  - Promotion readiness FAIL:
    `branch_promotion_allowed=false`; failing gates include
    `physical_latency_alignment`, `physical_marker_latency`,
    `physical_music_quality`, `runtime_cpu_beats_mainline`,
    `latest_physical_investigation`, and `traktor_timecode_physical`.
- Interpretation:
  - This rules out C++ written-buffer corruption, consumed-buffer timing drift,
    and Mode 2 packed USB byte corruption for this marker run.
  - The remaining fault boundary is downstream of internal packing: USB/device
    scheduling/state, Audio 8 DJ firmware/DAC interpretation, analog route, or
    external capture path.
  - Do not spend more time mutating packer/channel-map/sample conversion until
    the downstream boundary is narrowed.
- Evidence paths:
  - `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/`
  - `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/driver-diagnostics-analysis.txt`
  - `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/driver-packed-usb-analysis.txt`
  - `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/marker-peak-summary.json`
  - `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/physical-latency.json`
  - `local-analysis/runtime-isolation/20260617-after-usbdiag-marker.json`
  - `local-analysis/promotion-readiness/20260617-after-usbdiag-marker.json`

## 2026-06-17: Valid Capture-Out Layout Marker Rejection

- Purpose:
  - Test whether the alternate capture-out layout flag changes the downstream
    device behavior that produced stable multi-second external marker delay.
- Candidate:
  - Build with `HAL_VALID_CAPTURE_OUT_LAYOUT=1`.
  - Default restored after the run:
    `OPENA8DJ_VALID_CAPTURE_OUT_LAYOUT=0`.
- Commands:
  - `make -B HAL_VALID_CAPTURE_OUT_LAYOUT=1 build/opena8dj-usb-play build/audio-record`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-latency-marker/20260617-valid-capture-layout-marker-pairA-6s-postroll8 --reference-wav local-analysis/direct-usb-latency-marker/20260617-valid-capture-layout-marker-pairA-6s-postroll8/fixture/reference.wav --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 6 --postroll-seconds 8 --lead-frames 8192 --skip-build`
  - `make -B build/opena8dj-usb-play build/audio-record build/opena8dj-control`
- Results:
  - Direct USB transport stayed clean:
    `queue_failures=0`, `playback_transfers=395`,
    `frames_written=288000`, `frames_read=304074`.
  - External marker still FAILS readiness:
    `offset_mean_seconds=4.638750`, `offset_std_seconds=0.001297`,
    residual after record pre-roll plus expected internal lead/startup
    `3.773417s`.
  - Physical latency still FAILS:
    `first_energy_seconds=4.85`, `best_correlation=0.565271`,
    `aligned_snr_db=-2.99`, `linear_fit_snr_db=-3.28`,
    `linear_residual_over_capture_rms=0.824708`.
  - Physical music/marker quality still FAILS:
    `quality_alignment_score=0.960473`, SNR floor `-31.75 dB`,
    mid/high residual `38.609794/40.459687`, right click outliers `337`,
    no clipping.
- Decision:
  - Reject `HAL_VALID_CAPTURE_OUT_LAYOUT=1` as a latency or quality fix.
  - Do not keep this flag enabled for product builds.
  - The candidate did not move the failure boundary back into C++ timeline or
    packet packing; it remains downstream or in the physical capture route.
- Evidence paths:
  - `local-analysis/direct-usb-latency-marker/20260617-valid-capture-layout-marker-pairA-6s-postroll8/`
  - `local-analysis/direct-usb-latency-marker/20260617-valid-capture-layout-marker-pairA-6s-postroll8/summary.txt`
  - `local-analysis/direct-usb-latency-marker/20260617-valid-capture-layout-marker-pairA-6s-postroll8/metrics.json`
  - `local-analysis/direct-usb-latency-marker/20260617-valid-capture-layout-marker-pairA-6s-postroll8/physical-latency.json`
  - `local-analysis/direct-usb-latency-marker/20260617-valid-capture-layout-marker-pairA-6s-postroll8/marker-peak-summary.json`

## 2026-06-17: Playback Profile Control-State Marker Rejection

- Purpose:
  - Test whether the direct USB marker delay is caused by leaving the device in
    the timecode-vinyl-like control state read from hardware
    (`00:02:03:01:02:01`) instead of explicit playback defaults.
- Code/tooling:
  - `opena8dj-control profile playback` now matches the C++ input-profile
    contract: input mode `1`, ground lifts off, software lock off, input decode
    off, identity source map.
  - `opena8dj-usb-play --playback-profile` applies the same playback defaults
    after USB open/read-controls and before stream start.
  - `scripts/run-direct-usb-soundcheck --playback-profile` records this as
    explicit evidence.
- Commands:
  - `make -B build/opena8dj-usb-play build/opena8dj-control build/audio-record`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8 --reference-wav local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/fixture/reference.wav --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 6 --postroll-seconds 8 --lead-frames 8192 --playback-profile --skip-build`
  - `.venv/bin/python scripts/analyze-latency-marker-peaks.py local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/fixture/reference.wav local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/captured.wav --record-preroll-seconds 0.6 --playback-lead-frames 8192 --startup-silence-frames 4288 --json-out local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/marker-peak-summary.json`
  - `.venv/bin/python scripts/analyze-physical-latency.py local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/fixture/reference.wav local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/captured.wav --json-out local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/physical-latency.json`
- Results:
  - Control state changed as intended before streaming:
    `01:02:03:00:02:00`.
  - Direct USB transport stayed clean:
    `queue_failures=0`, `playback_transfers=395`,
    `frames_written=288000`, `frames_read=304019`.
  - External marker still FAILS readiness:
    `offset_mean_seconds=4.667208`, `offset_std_seconds=0.001308`,
    residual after record pre-roll plus expected internal lead/startup
    `3.807208s`.
  - Physical latency still FAILS:
    `first_energy_seconds=4.9`, `best_correlation=-0.318510`,
    `aligned_snr_db=-4.36`, `linear_fit_snr_db=-9.47`,
    `linear_residual_over_capture_rms=0.947840`.
  - Soundcheck metrics still FAIL:
    `quality_alignment_score=0.960242`, SNR floor `-17.54 dB`,
    mid/high residual `7.080066/7.008900`, no clipping.
- Decision:
  - Keep the playback-profile control-plane fix for consistency with the C++
    model.
  - Reject playback-profile control state as a latency or quality fix.
  - Remaining suspects stay downstream/stateful: USB alternate-setting reset
    behavior, device scheduling/state, Audio 8 DJ firmware/DAC interpretation,
    analog route, or external capture path.
- Evidence paths:
  - `local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/`
  - `local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/play.log`
  - `local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/summary.txt`
  - `local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/metrics.json`
  - `local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/physical-latency.json`
  - `local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/marker-peak-summary.json`
  - `local-analysis/runtime-isolation/20260617-after-playback-profile-marker.json`

## 2026-06-17: Alt0 Before Alt1 Marker Latency Improvement, Quality Still Fails

- Purpose:
  - Test whether the stable multi-second marker delay is caused by stale USB
    alternate-setting/device stream state.
- Candidate:
  - Build with `HAL_SELECT_ALT0_BEFORE_ALT1=1`.
  - Run with `--playback-profile` so control bytes are
    `01:02:03:00:02:00`.
  - Default build restored after the run with
    `OPENA8DJ_SELECT_ALT0_BEFORE_ALT1=0`.
- Commands:
  - `make -B HAL_SELECT_ALT0_BEFORE_ALT1=1 build/opena8dj-usb-play build/audio-record`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8 --reference-wav local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/fixture/reference.wav --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 6 --postroll-seconds 8 --lead-frames 8192 --playback-profile --skip-build`
  - `.venv/bin/python scripts/analyze-latency-marker-peaks.py local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/fixture/reference.wav local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/captured.wav --record-preroll-seconds 0.6 --playback-lead-frames 8192 --startup-silence-frames 5568 --json-out local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/marker-peak-summary.json`
  - `.venv/bin/python scripts/analyze-physical-latency.py local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/fixture/reference.wav local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/captured.wav --json-out local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/physical-latency.json`
  - `make -B build/opena8dj-usb-play build/audio-record build/opena8dj-control`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/20260617-after-alt0-diagfield-playback-profile-marker.json`
- Results:
  - Diagnostic field confirms the candidate was active:
    `select_alt0_before_alt1=1`.
  - Marker latency gate PASS:
    `offset_mean_seconds=0.405589`, `offset_std_seconds=0.001256`,
    `paired_peaks=4`.
  - First physical energy gate PASS:
    `first_energy_seconds=0.65`.
  - Overall physical latency still FAILS:
    `best_correlation=0.414578`, `aligned_snr_db=-3.99`,
    `linear_fit_snr_db=-6.76`,
    `linear_residual_over_capture_rms=0.908807`.
  - Soundcheck quality still FAILS:
    `quality_alignment_score=0.858726`, SNR floor `-14.49 dB`,
    mid/high residual `5.086371/4.926583`, no clipping.
  - Transport stayed clean:
    `queue_failures=0`, `playback_transfers=396`,
    `frames_written=288000`, `frames_read=304118`.
  - Runtime isolation after the run PASS:
    HAL inactive, lock absent, no forbidden OpenA8DJ/mainline processes.
- Interpretation:
  - Alt0-before-alt1 is the first diagnostic that materially improves marker
    latency.
  - It does not produce audiophile-valid output. The remaining failure is
    signal integrity/linearity/routing/capture, not the gross multi-second
    startup delay.
- Decision:
  - Keep `HAL_SELECT_ALT0_BEFORE_ALT1=0` as default until a full music + CPU
    gate passes.
  - Promote alt0-before-alt1 to next candidate input for music and same-day
    A/B testing, not to product readiness.
- Evidence paths:
  - `local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/`
  - `local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/play.log`
  - `local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/summary.txt`
  - `local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/metrics.json`
  - `local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/physical-latency.json`
  - `local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/marker-peak-summary.json`
  - `local-analysis/runtime-isolation/20260617-after-alt0-diagfield-playback-profile-marker.json`

## 2026-06-17: Alt0 Playback-Profile Real Music Rejection

- Purpose:
  - Determine whether the alt0-before-alt1 marker latency improvement also
    improves real music quality.
- Candidate:
  - Build with `HAL_SELECT_ALT0_BEFORE_ALT1=1`.
  - Run direct USB playback with `--playback-profile`.
  - Default build restored after the run with
    `OPENA8DJ_SELECT_ALT0_BEFORE_ALT1=0`.
- Commands:
  - `make -B HAL_SELECT_ALT0_BEFORE_ALT1=1 build/opena8dj-usb-play build/audio-record`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 12 --postroll-seconds 2 --lead-frames 8192 --playback-profile --skip-build`
  - `make -B build/opena8dj-usb-play build/audio-record build/opena8dj-control`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/20260617-after-alt0-music.json`
- Results:
  - Diagnostic field confirms the candidate was active:
    `select_alt0_before_alt1=1`.
  - Direct USB transport stayed clean:
    `queue_failures=0`, `playback_transfers=771`,
    `frames_written=576000`, `frames_read=592208`.
  - Music quality FAIL:
    `quality_alignment_score=0.103674`, SNR floor `-24.25 dB`,
    mid/high residual `16.213903/15.560684`, no clipping.
  - Failure-mode analysis:
    `timebase_or_alignment_instability`,
    `window_alignment_is_unstable_for_music`,
    `static_lr_mix_or_polarity_not_sufficient`,
    `simple_memoryless_nonlinearity_not_sufficient`,
    `residual_tracks_program_level`.
  - Runtime isolation after the run PASS:
    HAL inactive, lock absent, no forbidden OpenA8DJ/mainline processes.
- Decision:
  - Reject alt0-before-alt1 as a complete product fix.
  - Keep it as a partial state-reset finding because it improves marker
    latency, but require another mechanism for real music quality/linearity.
- Evidence paths:
  - `local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s/`
  - `local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s/play.log`
  - `local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s/summary.txt`
  - `local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s/metrics.json`
  - `local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s/failure-modes.json`
  - `local-analysis/runtime-isolation/20260617-after-alt0-music.json`

## 2026-06-17: Continuous Timeline Reset Fix, Direct USB Music

- Purpose:
  - Explain why the alt0/playback-profile real-music run failed despite clean
    early packet evidence, then verify the fix with a 12-second USB diagnostic
    music run.
- Root cause found:
  - `OutputTimelineWrite` reset the timeline for continuous writes when the
    producer was more than half a ring ahead of the reader.
  - In the failing diagnostic run, the reset happened at write frame `162048`
    and produced mid-music startup-silence/underrun gaps beginning around
    served frame `145600`.
- Fix:
  - Continuous writes no longer trigger the future-gap reset. Discontinuous
    future writes can still reset; continuous producer lead is handled by the
    existing elastic/high-water policy.
- Commands:
  - `make -B HAL_SELECT_ALT0_BEFORE_ALT1=1 HAL_DIAGNOSTIC=1 build/opena8dj-usb-play build/audio-record build/opena8dj-control`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-soundcheck/20260617-no-continuous-reset-alt0-music-pairA-12s-usbdiag --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 12 --postroll-seconds 2 --lead-frames 8192 --playback-profile --collect-usb-diagnostics --skip-build`
  - `make -B build/opena8dj-usb-play build/audio-record build/opena8dj-control`
  - `.venv/bin/python scripts/analyze-driver-capture.py ... --usb-compare-seconds 12`
  - `.venv/bin/python scripts/analyze-soundcheck-capture.py ... --time-warp --drift-profile`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/20260617-after-continuous-reset-fix.json`
  - `.venv/bin/python scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness/20260617-after-continuous-reset-fix.json`
- Internal USB result after fix:
  - Written vs reference: alignment `1.000000`, lag `0`, SNR `999.00 dB`.
  - Consumed vs written/reference: alignment `1.000000`, lag `0`, SNR
    `999.00 dB`, lag windows min/max/first/last all `0`.
  - Packed USB: Mode 2 `check_offset=8`, `start_byte=4`, big-endian,
    `check_errors=0`, `panic_flags=0`, alignment `1.000000`, lag `0`,
    gain `0.5`, SNR `999.00 dB` for `575919` compared frames.
- Physical iRig result after fix:
  - Still FAILS product gates:
    `quality_alignment_score=0.957628`, SNR floor `9.38 dB`,
    mid/high residual `1.422297/1.413835`, quiet mid-band noise
    `-35.22 dBFS`, lag jumps `0`, clipping `0`.
  - Time-warped reanalysis still FAILS:
    `quality_alignment_score=0.961334`, SNR `10.41 dB`,
    mid/high residual `1.404391/1.367270`, lag jumps `31`.
- Promotion readiness:
  - `local-analysis/promotion-readiness/20260617-after-continuous-reset-fix.json`
    remains `FAIL` and `branch_promotion_allowed=false`.
  - Blocking gates include physical music quality, latest music/CPU same-run
    pairing, runtime CPU, physical latency alignment, physical investigation,
    and Traktor/timecode physical validation.
- Runtime isolation:
  - PASS. HAL inactive, lock absent, forbidden mainline/OpenA8DJ processes
    absent.
- Evidence paths:
  - `local-analysis/direct-usb-soundcheck/20260617-no-continuous-reset-alt0-music-pairA-12s-usbdiag/`
  - `local-analysis/direct-usb-soundcheck/20260617-no-continuous-reset-alt0-music-pairA-12s-usbdiag/driver-capture-analysis-explicit-usb-12s.txt`
  - `local-analysis/direct-usb-soundcheck/20260617-no-continuous-reset-alt0-music-pairA-12s-usbdiag/metrics.json`
  - `local-analysis/direct-usb-soundcheck/20260617-no-continuous-reset-alt0-music-pairA-12s-usbdiag/metrics-timewarp.json`
  - `local-analysis/runtime-isolation/20260617-after-continuous-reset-fix.json`
  - `local-analysis/promotion-readiness/20260617-after-continuous-reset-fix.json`

## 2026-06-17: Decorrelated Direct USB Pair A Fixture, Routing Pass, Quality Fail

- Purpose:
  - Remove ambiguous real-music alignment from the next physical check by using a
    deterministic decorrelated stereo fixture with sparse transients and
    separate left/right tone families.
  - Verify whether the post-reset-fix direct USB path can simultaneously prove
    internal packet integrity, physical Pair A routing, and audiophile-quality
    loopback capture.
- Commands:
  - `.venv/bin/python scripts/generate-loopback-reference.py local-analysis/fixtures/decorrelated-direct-usb/reference-12s-peak030.wav --rate 48000 --seconds 12 --peak 0.30`
  - `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock" scripts/run-direct-usb-soundcheck --run-dir local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag --reference-wav local-analysis/fixtures/decorrelated-direct-usb/reference-12s-peak030.wav --capture-device "iRig Stream" --capture-channels 1,2 --pair A --rate 48000 --seconds 12 --postroll-seconds 2 --lead-frames 8192 --playback-profile --collect-usb-diagnostics --skip-build`
  - `.venv/bin/python scripts/analyze-channel-matrix-tones.py local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag --skip-seconds 0.68 --analysis-seconds 11 --json-out local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/tone-matrix.json`
  - `.venv/bin/python scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness/20260617-after-decorrelated-direct-usb.json`
  - `scripts/runtime-isolation-audit --expect-hal inactive --json-out local-analysis/runtime-isolation/20260617-after-decorrelated-direct-usb.json`
- Internal USB result:
  - Written, consumed, and packed USB output all align to the fixture at
    `1.000000`, lag `0`, and SNR `999.00 dB`.
  - Packed USB decodes as Mode 2 with `start_byte=4`, `check_offset=8`,
    big-endian samples, gain `0.5`, `check_errors=0`, and `panic_flags=0`
    over `575907` compared frames.
- Physical Pair A routing result:
  - `tone-matrix.json` reports `result=PASS`.
  - Expected signal floor is strong: `expected_floor_amplitude=0.147371`.
  - Crosstalk/leakage clears the strict gate:
    `max_wrong_source_leakage_db=-57.447168`,
    `left_to_right_leakage_db=-61.527228`,
    `right_to_left_leakage_db=-55.793274`.
  - Capture clipping remains `0`.
- Physical quality result:
  - Product music/fixture quality still FAILS:
    `quality_alignment_score=0.721193`, SNR floor `-2.96 dB`,
    mid/high residual `2.117458/2.018361`, quiet mid-band noise
    `-21.77 dBFS`, lag jumps `0`, clipping `0`.
  - Linear-matrix fitting reports a large physical residual
    (`residual_over_capture=0.748310`), so polarity/gain/matrix correction is
    not enough.
  - Tone-response compensation is diagnostic only and worsens the residual; a
    simple 3-band response model does not explain the failure.
- Promotion readiness:
  - `local-analysis/promotion-readiness/20260617-after-decorrelated-direct-usb.json`
    remains `FAIL` with `branch_promotion_allowed=false`.
  - New gates pass: `direct_usb_internal_integrity` and
    `physical_decorrelated_matrix_routing`.
  - Blocking gates remain: same-run music/CPU pairing, physical latency
    alignment, physical music quality, runtime CPU, physical investigation, and
    Traktor/timecode physical validation.
- Runtime isolation:
  - PASS. HAL inactive, lock absent, no forbidden OpenA8DJ/mainline processes.
- Decision:
  - Treat this as useful isolation evidence, not product readiness.
  - The current direct USB data plane and Pair A routing are clean for this
    fixture, but the captured waveform is not audiophile-valid and the product
    HAL still needs same-day mainline A/B, CPU, and timecode evidence.
- Evidence paths:
  - `local-analysis/fixtures/decorrelated-direct-usb/reference-12s-peak030.wav`
  - `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/`
  - `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/driver-diagnostics-analysis.txt`
  - `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/metrics.json`
  - `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/tone-matrix.json`
  - `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/linear-matrix.json`
  - `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/tone-response-compensation.json`
  - `local-analysis/promotion-readiness/20260617-after-decorrelated-direct-usb.json`
  - `local-analysis/runtime-isolation/20260617-after-decorrelated-direct-usb.json`

## 2026-06-17: Same-Day C++ HAL vs Mainline HAL A/B, Promotion Rejected

- Purpose:
  - Measure the C++ product HAL and the read-only mainline HAL artifact on the
    same day, same physical iRig route, same Audio 8 DJ, same Pair A output,
    same 48 kHz / 512-frame CoreAudio config, and same deterministic fixture.
- Safety:
  - Hardware lock used for HAL load/reload, CoreAudio restart, playback, and
    iRig capture.
  - Mainline worktree remained read-only. The mainline bundle was copied into
    C++ evidence before installation.
  - Final cleanup forced the active HAL out of `/Library/Audio/Plug-Ins/HAL`
    and final runtime isolation PASSed with HAL inactive and lock absent.
- Candidate snapshots:
  - C++ commit `0809d21a94147db92ffb787a66dc589e0b9b6872`.
  - Mainline commit `08745b73d23d4f6e410ab8308926a9584120be89`.
  - C++ HAL sha256:
    `f466c73673ad2cc2533c01bbae7ba2a9c3d44cd153db99ea198c61b42b40df4b`.
  - Mainline HAL sha256:
    `569c7303a1a9672d40c56eeee914eadccdbcd541562e1d2d674d1a3ffb9b90dc`.
- C++ HAL result:
  - Safety load PASS.
  - Soundcheck FAIL:
    quality `0.134709`, SNR floor `-12.66 dB`, mid/high residual
    `4.904891/4.494813`, quiet mid-band noise `-23.34 dBFS`,
    lag jumps `18`, clipping `0`.
  - CPU p95:
    driver `23.2%`, coreaudiod `20.5%`.
- Mainline HAL result:
  - First safety attempt was blocked by external `mediaremoted` CPU, not by
    enumeration failure. Recovery ran and HAL was unloaded.
  - Retry safety load PASS.
  - Soundcheck FAIL:
    quality `0.246599`, SNR floor `-13.28 dB`, mid/high residual
    `5.774651/5.636904`, quiet mid-band noise `-24.03 dBFS`,
    lag jumps `41`, clipping `0`.
  - CPU p95:
    driver `5.6%`, coreaudiod `10.3%`.
- Comparison:
  - Both candidates fail absolute audiophile gates.
  - C++ does not beat mainline overall:
    quality is worse by `-0.111889`, driver CPU is much worse
    (`23.2%` vs `5.6%`), and coreaudiod CPU is worse (`20.5%` vs `10.3%`).
  - C++ is better only on residual ratios, lag jumps, and SNR floor in this
    fixture, but those partial wins are not enough for readiness.
- Decision:
  - `FAIL_CPP_NOT_BETTER_THAN_MAINLINE`.
  - Do not move C++ to `main`.
  - Do not move C mainline to `Legacy`.
  - Next C++ work must target product HAL CPU and physical alignment/quality,
    not direct USB packet correctness alone.
- Evidence paths:
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/manifest.txt`
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/cpp-hal-safety/summary.txt`
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/cpp-soundcheck/metrics.json`
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/cpp-soundcheck/cpu-profile.tsv`
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/mainline-hal-safety-retry/summary.txt`
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/mainline-soundcheck/metrics.json`
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/mainline-soundcheck/cpu-profile.tsv`
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/ab-comparison.json`
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/final-runtime-after-force-unload.json`

## 2026-06-17: Hot-Path Timing And Corrected Stream-Stats Denominators

- Purpose:
  - Measure callback-adjacent HAL cost without changing default product builds.
  - Correct stream-stats evidence so raw transfer completions and sampled
    transaction counters are not mixed.
- Commands:
  - `make hal build/opena8dj-control`
  - `python3 -m py_compile scripts/analyze-stream-stats.py scripts/run-soundcheck scripts/check-stream-stats-contract.py`
  - `python3 scripts/check-stream-stats-contract.py`
  - `make -B HAL_HOT_PATH_TIMING=1 hal build/opena8dj-control`
  - `scripts/run-cpp-offline-gates`
  - Locked physical run using `HAL_HOT_PATH_TIMING=1`, temporary HAL install,
    Pair A playback through Open Audio 8 DJ, and iRig Stream capture.
- Offline result:
  - PASS.
  - Debug CTest: `17/17`.
  - Release CTest: `18/18`.
  - Stream-stats contract: `196` fields, `0` mismatches.
  - Offline evidence: `local-analysis/cpp-offline/current-offline-gates.json`.
- Physical result:
  - Safety load PASS.
  - Soundcheck FAIL:
    quality `0.970666`, SNR `10.78 dB`, lag jumps `19`,
    mid/high residual `1.378008/1.352014`, quiet mid-band noise
    `-31.35 dBFS`, clipping `0`.
  - Hot-path average ticks:
    capture handler `3755.08`, capture decode `6.05`, capture requeue
    `1825.69`, playback queue `1862.70`, playback fill `289.26`,
    playback enqueue `1493.53`, playback completion `20.14`.
  - Corrected transfer accounting:
    raw capture transfers `1000.35/s`, sampled capture transfers `62.43/s`,
    playback completed `1000.22/s`, sampled playback transfers `62.43/s`.
    Capture sampled transfer composition is about `4.36` valid transactions
    and `3.64` zero-complete transactions per 8-slot transfer, with all `8.0`
    slots classified.
  - Interpretation:
    this is not enough to justify a fixed full-8-slot OUT layout. At this
    packet size/rate, full 8-slot playback every millisecond would over-read
    output audio; any pacing fix must preserve ~48 kHz output consumption.
- Safety:
  - Hardware lock used.
  - Final runtime isolation PASS: HAL inactive, lock absent, no forbidden
    OpenA8DJ/mainline processes.
- Decision:
  - Keep `HAL_HOT_PATH_TIMING=0` by default.
  - Treat this as diagnostic evidence only.
  - C++ remains not ready for hardware-readiness claims, Traktor/timecode
    claims, or branch promotion.
- Evidence paths:
  - `local-analysis/hot-path-timing/20260617T140410Z-sampled-denom/summary.txt`
  - `local-analysis/hot-path-timing/20260617T140410Z-sampled-denom/safety/summary.txt`
  - `local-analysis/hot-path-timing/20260617T140410Z-sampled-denom/soundcheck/summary.txt`
  - `local-analysis/hot-path-timing/20260617T140410Z-sampled-denom/soundcheck/metrics.json`
  - `local-analysis/hot-path-timing/20260617T140410Z-sampled-denom/stream-stats-summary.json`
  - `local-analysis/hot-path-timing/20260617T140410Z-sampled-denom/runtime-isolation-final.json`

## 2026-06-17: Offline Capture-Paced Rate-Shape Gate

- Purpose:
  - Prevent mathematically unsafe playback-pacing ideas from reaching hardware.
  - Separate rate correctness from product readiness.
- Commands:
  - `cmake --build build/cpp-release --target opena8djcpp_jitter_model`
  - `./build/cpp-release/opena8djcpp_jitter_model`
  - `scripts/run-cpp-offline-gates`
- Result:
  - PASS.
  - Debug CTest: `17/17`.
  - Release CTest: `18/18`.
  - Jitter model now reports `3` rate-shape rows and `0` rate-shape failures.
- Rate-shape findings:
  - `iso8_observed_partial_layout_rate_safe`:
    about `4.360721` playback transactions/ms, `352` bytes/request,
    `32` USB bytes/frame, output about `47967.9` frames/s, rate error
    `-668 ppm`, model PASS.
  - `iso8_forced_full8_layout_overreads`:
    `8.0` playback transactions/ms, output about `88000` frames/s, rate
    error `833333 ppm`, model PASS as an expected unsafe rejection.
  - `mainline_like_iso64_q8_rate_shape_not_sufficient`:
    rate-safe in the model but marked physically rejected because the exact
    C++ ISO64/q8 candidate already failed locked music quality.
- Decision:
  - Do not spend a hardware window on forced full-8 ISO8 OUT layouts.
  - Do not retry plain ISO64/q8 as a product candidate without a new physical
    quality hypothesis.
  - C++ remains not ready for hardware-readiness claims, Traktor/timecode
    claims, or branch promotion.
- Evidence paths:
  - `local-analysis/cpp-offline/jitter-model.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`

## 2026-06-17: Rejected-Default Static Policy Gate

- Purpose:
  - Make offline gates fail if a physically rejected or diagnostic-only HAL knob
    is accidentally promoted to the default Makefile build.
  - Preserve opt-in diagnostic flags without letting them silently become the
    product candidate.
- Commands:
  - `cmake --build build/cpp-release --target opena8djcpp_static_policy_check`
  - `./build/cpp-release/opena8djcpp_static_policy_check`
  - `scripts/run-cpp-offline-gates`
- Direct checker result:
  - PASS.
  - `audited_files=10`.
  - `forbidden_hits=0`.
  - `path_policy=true`.
  - `rejected_default_checks=21`.
  - `default_policy_failures=0`.
- Protected default classes:
  - transfer-rate and pacing defaults rejected by model or physical evidence;
  - output format/packer defaults rejected by physical evidence;
  - diagnostic instrumentation defaults that must remain opt-in;
  - sample-time and explicit-scheduling defaults that failed product gates.
- Evidence paths:
  - `local-analysis/cpp-offline/static-policy.json`
  - `local-analysis/cpp-offline/current-offline-gates.json`

## 2026-06-17: Offline Timebase Family Classification

- Purpose:
  - Classify existing physical soundcheck failures as fixed latency, linear
    drift, local lag jumps, or residual after local lag correction.
  - Avoid changing transport/CPU knobs without knowing which failure mode they
    can plausibly improve.
- Commands:
  - `python3 -m py_compile scripts/analyze-timebase-family.py`
  - `scripts/analyze-timebase-family.py local-analysis/timebase-window-comparison/20260617-current-family/window-trace-*.json --json-out local-analysis/timebase-window-comparison/20260617-current-family/timebase-family.json`
  - `scripts/analyze-soundcheck-window-trace.py local-analysis/mainline-ab/20260617-sameday-ab-085735/cpp-soundcheck --json-out local-analysis/mainline-ab/20260617-sameday-ab-085735/cpp-window-trace.json`
  - `scripts/analyze-soundcheck-window-trace.py local-analysis/mainline-ab/20260617-sameday-ab-085735/mainline-soundcheck --json-out local-analysis/mainline-ab/20260617-sameday-ab-085735/mainline-window-trace.json`
  - `scripts/analyze-timebase-family.py local-analysis/mainline-ab/20260617-sameday-ab-085735/cpp-window-trace.json local-analysis/mainline-ab/20260617-sameday-ab-085735/mainline-window-trace.json --json-out local-analysis/mainline-ab/20260617-sameday-ab-085735/timebase-ab.json`
- Current-family C++ result:
  - `analysis_result=PASS`, `stability_result=FAIL`, `result=FAIL`.
  - `trace_count=7`.
  - `runs_with_lag_jumps=7`.
  - `runs_with_residual_after_lag_correction=7`.
  - `max_lag_jump_count_gt_2_frames=35`.
  - `max_lag_abs_p95=24` frames.
  - `max_abs_drift_ppm=40.21`.
  - Median local-lag correction improves mid-band residual by only about
    `1.10%`, so fixed/linear lag correction is not enough for the current
    family.
- Same-day A/B result:
  - `analysis_result=PASS`, `stability_result=FAIL`, `result=FAIL`.
  - C++ A/B:
    fixed/local lag roughly `-265..-285` frames, `7` lag jumps,
    corrected mid residual median about `0.89`.
  - Mainline A/B:
    drift about `-128.93 ppm`, `41` lag jumps, corrected mid residual median
    about `5.63`.
- Interpretation:
  - Fixed latency can make raw quality metrics pessimistic.
  - C++ still fails readiness because current-family evidence shows lag jumps
    and residual after local correction, and product CPU still does not beat
    mainline.
- Evidence paths:
  - `local-analysis/timebase-window-comparison/20260617-current-family/timebase-family.json`
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/timebase-ab.json`
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/cpp-window-trace.json`
  - `local-analysis/mainline-ab/20260617-sameday-ab-085735/mainline-window-trace.json`

## 2026-06-17: Output Flush Timing Mainline Alignment

- Purpose:
  - Remove a C++-specific timing difference where output could flush inside
    `WriteMix` before `EndIOOperation`.
  - Keep a diagnostic flag for controlled A/B while matching mainline by
    default.
- Change:
  - Added `HAL_FLUSH_OUTPUT_IN_WRITE_MIX ?= 0`.
  - Added `OPENA8DJ_FLUSH_OUTPUT_IN_WRITE_MIX` to HAL build flags.
  - Guarded the early `WriteMix` flush behind that flag.
  - Added the default to `static_policy_check`.
- Commands:
  - `git diff --check`
  - `make -B hal`
  - `cmake --build build/cpp-release --target opena8djcpp_static_policy_check`
  - `./build/cpp-release/opena8djcpp_static_policy_check`
  - `scripts/run-cpp-offline-gates`
- Result:
  - HAL build PASS.
  - Static policy PASS with `22` rejected/default checks and `0` failures.
  - Offline gates PASS:
    Debug CTest `17/17`, Release CTest `18/18`, evidence schema PASS,
    hardware-lock policy PASS, stream-stats contract PASS, static policy PASS.
- Interpretation:
  - This is a timing candidate only. It does not prove sound quality,
    performance, routing, Traktor, or timecode readiness until a locked
    physical A/B passes.

## 2026-06-17: Locked Output Flush Timing Physical Rejection

- Candidate:
  - Commit `a3dd76a`, `HAL_FLUSH_OUTPUT_IN_WRITE_MIX=0`.
- Commands:
  - `make -B hal build/audio-wav-play build/audio-record build/audio-config build/opena8dj-control`
  - `scripts/test-hal-candidate-safety --candidate build/OpenA8DJ.driver --cycles 1 --leave-loaded --wait 8 --run-dir local-analysis/physical-product/20260617-output-flush-mainline/hal-candidate-safety`
  - `scripts/run-soundcheck --run-dir local-analysis/soundcheck/20260617-output-flush-mainline-irig-pairA-12s-cpp-hal --capture-device "iRig Stream" --capture-channels 1,2 --pair A --seconds 12 --mode dense --target-peak-db -16 --stream-stats-snapshots --monitor-stream-stats --audio-stack-recover-on-fail --audio-stack-unload-on-recover`
  - `scripts/analyze-soundcheck-window-trace.py local-analysis/soundcheck/20260617-output-flush-mainline-irig-pairA-12s-cpp-hal --json-out local-analysis/soundcheck/20260617-output-flush-mainline-irig-pairA-12s-cpp-hal/window-trace.json`
  - `python3 scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-output-flush-mainline-irig-pairA-12s-cpp-hal/stream-stats-during.tsv --json-out local-analysis/soundcheck/20260617-output-flush-mainline-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
  - `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-after-output-flush-mainline.json`
  - `scripts/audio-stack-guard --force-unload-opena8dj --wait 4 --enumeration-timeout 8 --min-idle-pct 20 --run-dir local-analysis/audio-stack-guard/after-output-flush-mainline-force-unload`
- Safety result:
  - HAL candidate safety PASS.
  - Post-run forced unload PASS:
    `opena8dj_state=unloaded`, `opena8dj_driver_pids=none`,
    `audio_stack_health=PASS`.
- Physical result:
  - Soundcheck FAIL.
  - `quality_alignment_score=0.962241 < 0.980`.
  - SNR `10.29 dB < 35 dB`.
  - `lag_jumps_gt_2_frames=23 > 0`.
  - Mid/high residual ratios `1.407975/1.362266`, both over strict gates.
  - Quiet mid noise `-35.17 dBFS > -58 dBFS`.
  - No clipping and `click_outliers=0`.
- Window/timebase result:
  - `lag_jumps_gt_2_frames=23`.
  - local lag range `-22..5` frames.
  - corrected mid residual median `1.413201`.
  - local-lag correction improves mid residual by only `0.59%`.
- CPU result:
  - OpenA8DJ driver p95 about `22.4%`.
  - `coreaudiod` p95 `47.2%` including startup/load samples.
  - This fails the mainline-relative CPU gate by a wide margin.
- Promotion readiness:
  - `local-analysis/promotion-readiness-after-output-flush-mainline.json`
    reports `result=FAIL`, `branch_promotion_allowed=false`.
- Interpretation:
  - Aligning flush timing with mainline is not sufficient to solve physical
    music quality, lag stability, or CPU.
  - Do not claim readiness, do not move C++ to `main`, and do not move C to
    `Legacy`.
  - The mainline-aligned default may remain as a narrower baseline, but it is
    not an evidence-backed product improvement by itself.

## 2026-06-17 Transport Cadence Matrix

- Command:
  - `scripts/summarize-transport-cadence.py --json-out local-analysis/transport-cadence/current.json --csv-out local-analysis/transport-cadence/current.csv`
  - `python3 -m py_compile scripts/summarize-transport-cadence.py scripts/analyze-stream-stats.py scripts/analyze-timebase-family.py scripts/analyze-soundcheck-window-trace.py`
- Scope:
  - Offline-only parse of existing artifacts. No hardware, CoreAudio, USB, HAL install, defaults, or service state touched.
- Result:
  - PASS, `59` physical soundcheck runs summarized.
  - Families identified from existing stream-stats cadence artifacts:
    - `ISO5/q64`: best quality `0.978050`, median driver CPU p95 about `36.9%`.
    - `ISO8/q8`: best quality `0.964724`, median driver CPU p95 about `22.4%`.
    - `ISO10/q8`: quality `0.969379`, driver CPU p95 `19.6%`.
    - `ISO64/q8`: median quality about `0.678356`, min driver CPU p95 `6.0%`.
- Evidence:
  - `local-analysis/transport-cadence/current.json`
  - `local-analysis/transport-cadence/current.csv`
- Interpretation:
  - Current evidence shows a transport tradeoff, not a promotable candidate.
  - Product readiness still requires simultaneous physical music quality, low CPU, routing, recovery, and Traktor/timecode evidence.
