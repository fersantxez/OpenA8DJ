# Decision Log

## 2026-06-18: Bind Prepared Runtime Physical Windows to Offline Artifact Hash

Decision:
- Add `--prepared-runtime-candidate` to the physical superiority runner and
  read-only preflight.
- Require prepared-runtime physical windows to verify the candidate executable
  SHA-256 against `hal-prepared-runtime-candidate.json`, verify bundle
  completeness evidence, and verify the current offline dispatch contract.
- Add `opena8djcpp_prepared_runtime_physical_window_contract` and require it
  from the full offline evidence summary and schema.

Reason:
- The next useful physical experiment is submit-rate, CPU/resource, and audio
  quality comparison for the prepared-runtime HAL candidate. That evidence is
  only defensible if the run proves exactly which bundle was installed.
- A same-session physical result without candidate hash binding could be real
  but still unusable for promotion because it would not attribute the measured
  improvement to the prepared-runtime artifact built by the offline gates.

Evidence:
- `opena8djcpp_prepared_runtime_physical_window_contract`: PASS.
- Read-only candidate preflight with `--prepared-runtime-candidate` and
  `--candidate build/OpenA8DJ-prepared-runtime.driver`: PASS for artifact
  binding in diagnostic `--candidate-only --skip-known-good` mode.
- Candidate executable hash matched offline prepared candidate evidence.
- Route preflight still rejects MacBook Air Speakers as built-in/acoustic
  output for promotion evidence.

Alternatives discarded:
- Inferring prepared runtime identity from bundle path alone: rejected because
  paths are mutable.
- Letting candidate-only or skip-known-good diagnostics imply product
  readiness: rejected; same-session route revalidation and mainline/C++ A/B
  remain mandatory.

Next implication:
- The prepared-runtime physical A/B command must use
  `--prepared-runtime-candidate`, a valid non-Audio8 wired known-good output,
  iRig capture, and the global hardware lock.

## 2026-06-18: Require HAL Prepared Runtime Binding Contract Before Claims

Decision:
- Add `opena8djcpp_hal_prepared_runtime_binding_contract` as a required
  offline gate between the build-only prepared HAL profile and any physical
  runtime experiment.
- Require the contract from `prepared_transport_migration_gate`,
  `hal_transport_runtime_gate`, evidence schema, static policy, and the full
  offline runner.
- Keep the claim blockers intact: the binding contract can prove source/runtime
  geometry wiring, but cannot prove lower CPU, better sound, Timecode Vinyl
  correctness, or branch promotion.

Reason:
- The prepared profile is meaningful only if its 64-transaction geometry is
  actually used by transfer pools, enqueue calls, capture-paced playback
  batching, completion-owned lifetimes, timestamps, and observable counters.
- A compile-only target is too weak to justify hardware claims, while a direct
  runtime rewrite is too risky without a precise contract for what must remain
  true.

Evidence:
- `opena8djcpp_hal_prepared_runtime_binding_contract`: PASS.
- Focused CTest passed:
  `opena8djcpp_hal_prepared_runtime_binding_contract`,
  `opena8djcpp_prepared_transport_migration_gate`,
  `opena8djcpp_hal_transport_runtime_gate`, and
  `opena8djcpp_static_policy_check`.
- Key contract values:
  `expected_logical_iso_frames=8`, `expected_slots_per_submit=8`,
  `expected_capture_transactions_per_submit=64`,
  `expected_playback_transactions_per_submit=64`,
  `expected_submit_reduction_ratio=8`,
  `physical_evidence_present=false`, and `product_claim_allowed=false`.

Alternatives discarded:
- Treating `make -B hal-prepared-runtime` as runtime proof: rejected because
  it does not measure accepted USB submits, CPU, jitter, or audio quality.
- Installing/loading the candidate before this contract: rejected because
  transfer lifetime and completion semantics are hot-path risks.

Next implication:
- The next hardware step remains a lock-gated route revalidation and then a
  prepared-runtime physical A/B that records submit counters, CPU/resource
  counters, capture quality, routing isolation, and Timecode Vinyl evidence.

## 2026-06-18: Add Default-Off HAL Prepared Runtime Build Profile

Decision:
- Add a local `hal-prepared-runtime` build target and source-level compile-time
  guards for a prepared USB submit runtime profile.
- Keep `HAL_PREPARED_USB_SUBMIT_RUNTIME ?= 0` as the default and require
  offline source/evidence gates before any physical use.
- Update `hal_transport_runtime_gate` so it accepts a guarded opt-in profile
  while still blocking CPU, sound-quality, Timecode Vinyl, and branch-promotion
  claims until lock-gated same-session physical evidence exists.

Reason:
- The previous offline adapter proved the HAL geometry can map into prepared
  USB request submits, but there was no buildable HAL profile for the next
  controlled runtime experiment.
- The risky part is not making the flag exist; it is preventing it from
  silently becoming the default or being mistaken for measured superiority.

Evidence:
- Focused gates passed:
  `opena8djcpp_hal_prepared_runtime_source_contract`,
  `opena8djcpp_prepared_transport_migration_gate`,
  `opena8djcpp_hal_transport_runtime_gate`, and
  `opena8djcpp_static_policy_check`.
- Local build-only profile compiled with `make -B hal-prepared-runtime`; the
  normal local HAL bundle was then rebuilt with `make -B hal`.
- No install, load, CoreAudio mutation, USB reset, playback, capture, or
  hardware access was performed.

Alternatives discarded:
- Enabling the prepared runtime by default: rejected because there is no
  physical A/B evidence yet.
- Treating local compilation as a CPU-quality win: rejected because the active
  runtime still has no measured prepared-submit reduction.

Next implication:
- Run full offline gates from a clean commit. Only after those pass can a
  lock-gated route revalidation window be requested for the prepared runtime
  candidate.

## 2026-06-18: Add HAL Prepared-Submit Adapter Contract Before Runtime Mutation

Decision:
- Add an offline `opena8djcpp_hal_prepared_submit_adapter_contract` before
  changing the live HAL `enqueueIORequestWithData` path.
- Require this contract inside `opena8djcpp_prepared_transport_migration_gate`.
- Keep CPU/resource and product-quality claims blocked until the adapter is
  bound to real HAL or DriverKit USB and then wins a same-session physical A/B.

Reason:
- Current performance evidence is blocked because HAL still submits USB
  directly from capture/playback paths.
- Existing prepared-submit models prove batching in abstract DriverKit/core
  contracts, but the next risk is preserving HAL geometry, request lifecycle,
  timestamps, and payload bytes when bridging toward the real runtime.
- A small offline adapter contract gives objective pass/fail evidence before
  touching hardware, CoreAudio, USB, or driver install state.

Evidence:
- Focused run:
  `opena8djcpp_hal_prepared_submit_adapter_contract` PASS.
- Key metrics:
  `logical_slots=528`, `usb_submit_calls=66`, `total_bytes=185856`,
  `total_frames=5808`, `max_live_requests=4`,
  `partial_submit_calls=0`, `fallback_allocations=0`,
  `payload_equivalent=true`.
- Migration gate now includes `hal_prepared_submit_adapter_safe=PASS`.

Alternatives discarded:
- Editing the live HAL queue path first: rejected because it risks changing
  buffer lifetime, first-frame scheduling, and completion behavior before the
  bridge is proven offline.
- Treating existing DriverKit submit contracts as enough: rejected because they
  did not explicitly represent the HAL geometry boundary that is blocking the
  real CPU claim.

Next implication:
- The next implementation step is a default-off runtime binding of this
  adapter to real HAL or DriverKit USB submit paths, followed by a lock-gated
  physical A/B only after the route is revalidated.

## 2026-06-17/18: Reject Current C++ HAL Physical Candidate Again

Decision:
- Do not claim physical readiness, audiophile quality, lower CPU, or branch
  promotion for the current C++ HAL candidate.
- Keep the loaded candidate as diagnostic-only evidence and unload it after the
  failed physical window.
- Continue toward a prepared transport / DriverKit timing model rather than
  random HAL queue/coalescing toggles.

Reason:
- The candidate can enumerate `Open Audio 8 DJ` as `8 in / 8 out`, but current
  locked physical soundchecks still fail strict quality by a wide margin.
- The controlled fixture run eliminates automatic music selection as the main
  explanation and still fails with low correlation, bad SNR, and lag jumps.
- The fresh idle iRig capture is clean, so idle capture noise alone does not
  explain the playback failure.
- Process sampling again shows driver CPU dominated by IOUSBHost async enqueue
  paths rather than audio packing/routing.

Evidence:
- HAL safety run:
  `local-analysis/hal-candidate-safety/20260618T003723Z-cpp-candidate-leave-loaded`.
- Real-music soundcheck:
  `local-analysis/soundcheck/20260618T003816Z-cpp-hal-irig-pairA-12s`,
  `quality_alignment_score=0.136314`, SNR `-23.07 dB`,
  `lag_jumps_gt_2_frames=39`.
- Controlled fixture soundcheck:
  `local-analysis/soundcheck/20260618T004016Z-cpp-hal-irig-pairA-fixture-12s-threshold70`,
  `quality_alignment_score=0.603070`, SNR `-5.81 dB`,
  `lag_jumps_gt_2_frames=18`, `opena8dj_driver` CPU around `21-22%`.
- Driver-sampled fixture soundcheck:
  `local-analysis/soundcheck/20260618T004250Z-cpp-hal-irig-pairA-fixture-6s-driver-sample`,
  `dominant_interpretation=usbhost_async_enqueue_from_capture_and_playback_paths`.
- iRig idle capture:
  `local-analysis/irig-capture-isolation/20260618T004203Z-irig-idle-after-cpp-hal-fail`,
  max RMS `-62.350199 dBFS`, max peak `-41.031139 dBFS`,
  `idle_capture_unhealthy=false`.
- Final unload guard:
  `local-analysis/audio-stack-guard/20260618T004407Z-unload-after-failed-cpp-physical`,
  `opena8dj_state=unloaded`, audio stack health PASS.

Alternatives discarded:
- Treat HAL enumeration as readiness: rejected because quality and CPU failed.
- Blame iRig idle noise: rejected by the fresh locked idle capture.
- Keep probing simple playback coalescing/refill variants: rejected because
  prior probes reduced CPU by breaking quality, and the current sample points
  to the broader USB enqueue cadence/timing model.

Next implication:
- The next implementation step should move the runtime path toward the prepared
  transport/DriverKit request model already covered by offline contracts, then
  re-enter HAL/DriverKit hardware testing only when it predicts lower enqueue
  cadence without changing the validated packet/layout timing.

## 2026-06-17: Require Same-Window Known-Good Route Evidence For Promotion

Decision:
- `scripts/evaluate-promotion-readiness.py` now treats
  `known-good-route/metrics.json` as a required artifact in the same
  `local-analysis/physical-superiority-window/<id>` bundle as the C++ music,
  CPU, tone, latency, USB, matrix, and same-session comparison evidence.
- The offline summary now includes
  `same_window_known_good_route_revalidation_missing` as a hard promotion
  blocker.

Reason:
- Current Core Audio enumeration only exposes iRig Stream and built-in
  speakers as non-Audio8 devices. Built-in/acoustic output is diagnostic only
  and cannot validate the wired mixer/REC OUT -> iRig capture route.
- Historical route evidence and skipped known-good runs are useful forensic
  inputs, but they must not be enough to support an audiophile quality or
  branch-promotion claim.

Alternatives discarded:
- Trusting a prior route-only pass: rejected because the physical route can
  drift between sessions.
- Allowing candidate-only windows to satisfy bundle completeness: rejected
  because they cannot prove the capture route or same-session mainline
  comparison.

Evidence:
- `./build/audio-list` showed iRig Stream, MacBook Air Microphone, and
  MacBook Air Speakers only.
- `scripts/evaluate-promotion-readiness.py` now reports
  `same_window_known_good_route_revalidated` as a failing gate until current
  same-window evidence exists.

## 2026-06-17: Validate Standalone Known-Good Route Resolution Under Lock

Decision:
- Add `scripts/validate-known-good-route-request.py`.
- `scripts/run-known-good-route-soundcheck` now runs that validator after
  acquiring the global hardware lock and before playback/capture.
- The validator rejects a requested known-good output if the resolved CoreAudio
  device is OpenA8DJ/Audio 8, built-in/acoustic without diagnostic opt-in, the
  same device as capture, or a virtual/pre-device capture path.

Reason:
- The physical-window preflight already rejects resolved Audio 8 devices, but
  the standalone known-good route runner could still be called directly.
- An ambiguous output substring such as `Open` must not be allowed to resolve
  to `Open Audio 8 DJ` and then masquerade as an independent route proof.

Alternatives discarded:
- Relying only on argument-text rejection: rejected because device resolution
  can turn a harmless-looking substring into Audio 8.
- Running live CoreAudio enumeration before the lock: rejected. The standalone
  runner now performs live validation only after lock acquisition; offline tests
  use a synthetic `audio-list` text fixture.

Evidence:
- `python3 scripts/test-promotion-window-contract.py` passes and verifies that
  a synthetic `Open` request resolving to `Open Audio 8 DJ` fails.
- Manual offline fixture check returned `Wired` route `0` and `Open` route `1`.

## 2026-06-17: Add C++ Physical Capture Forensics Before Further Hardware Claims

Decision:
- Add `opena8djcpp_physical_capture_forensics`, a compiled C++ analyzer over
  archived iRig WAV captures.
- Wire it into CMake, the offline gate runner, the evidence schema, and the
  global gate summary.
- Treat the analyzer PASS as diagnostic health only; it cannot promote the
  C++ driver or claim audiophile quality.

Reason:
- Aggregated `metrics.json` files are not enough to decide whether the current
  physical failure is fixable by gain/routing correction, fixed latency, or a
  real variable route/timebase problem.
- The C++ forensic pass recomputes fixed-lag correlation, per-window lag
  stability, 2x2 stereo matrix explanation, residual energy, high-change
  residual ratio, and classification directly from archived reference/capture
  WAVs.
- Current evidence shows `61` physical WAV candidates, `12` deep analyses,
  and `0` strict quality candidates. The best analyzed capture has quality
  `0.978050` and SNR floor `9.845114 dB`, but it is still classified as
  `variable_timebase_or_route_capture_instability`.

Alternatives discarded:
- Install more tools before analyzing existing WAVs: rejected for now because
  a dependency-free C++ analyzer can extract the next useful signal without
  touching hardware, CoreAudio, USB, or system state.
- Promote based on the best `0.978050` quality score: rejected because SNR,
  lag stability, and matrix residual evidence still fail.

Evidence:
- `local-analysis/cpp-offline/physical-capture-forensics.json`.

Next implication:
- Any further physical test must be same-route, lock-gated, and designed to
  distinguish capture-route/timebase instability from true driver output
  quality. Branch promotion remains blocked.

## 2026-06-17: Hardware Recovered, Product Quality Still Rejected

Decision:
- Accept the lock-gated HAL safety pass as proof that iRig and Audio 8 DJ can
  currently be recovered for controlled tests.
- Reject the current C++ HAL candidate for product quality and performance.
- Reject direct USB as an audiophile-quality escape hatch; it is useful
  diagnostically, but it still fails SNR/residual quality.

Reason:
- The HAL safety gate loaded commit `8cb4669`, enumerated `Open Audio 8 DJ` as
  `8 in / 8 out`, and post-test recovery unloaded it cleanly.
- The HAL soundcheck failed quality and CPU: quality `0.962986`, SNR floor
  `10.317819 dB`, `26` lag jumps, driver CPU p95 `22.6%`, coreaudiod p95
  `32.4%`.
- Direct USB bypassed HAL/CoreAudio playback but still failed physical quality:
  native quality `0.959037`, SNR floor `9.697139 dB`, residual ratios around
  `1.42/1.41`, and classification
  `uncorrelated_residual_or_capture_path_dominant`.

Alternatives discarded:
- Continue optimizing HAL CPU only: rejected as insufficient because direct USB
  still shows strong physical residual outside HAL.
- Promote direct USB timing because summary lag jumps were `0`: rejected
  because the native C++ WAV analyzer saw `20` window lag jumps and the SNR
  remained around `9.7 dB`.

Evidence:
- `local-analysis/hal-candidate-safety/20260617T183412Z-cpp-8cb4669-leave-loaded`.
- `local-analysis/soundcheck/20260617T183449Z-cpp-8cb4669-irig-pairA-12s-after-forensics`.
- `local-analysis/audio-stack-guard/20260617T183615Z-post-soundcheck-unload`.
- `local-analysis/direct-usb-soundcheck/20260617T183629Z-pairA-12s-after-hal-fail`.

Next implication:
- Next physical work should isolate the capture/analog route with a known-good
  non-Audio8 source or a loopback fixture, and separately reduce HAL CPU/lag.
  Do not claim C++ is better than mainline.

## 2026-06-17: Attribute Direct USB Failure After Clean Packed USB Payload

Decision:
- Add `opena8djcpp_direct_usb_path_attribution` as a compiled C++ evidence
  gate.
- Treat Direct USB as a diagnostic splitter, not as a product-quality
  candidate.
- Prioritize isolating the post-USB physical path before further packet-format
  or packer changes.

Reason:
- Latest Direct USB diagnostics prove the data delivered to the device is
  internally clean: written, consumed, and packed USB alignment all `1.000000`;
  USB SNR floor `999 dB`; USB check errors `0`; panic flags `0`.
- The same run's physical iRig capture fails badly: quality `0.959037`, SNR
  floor `9.697139 dB`, mid/high residual `1.420546/1.410032`, and native lag
  jumps `20`.
- Static matrix, polynomial, and LTI explanations are insufficient:
  matrix improvement `0.241057 dB`, polynomial improvement `0.010168 dB`,
  low/mid coherence around `0.234650`, and LTI SNR delta negative.

Alternatives discarded:
- Keep changing the USB packer: rejected because packed USB evidence is clean
  in the current diagnostic run.
- Blame the HAL only: rejected because Direct USB bypasses HAL playback and
  still shows the same low-SNR residual family.

Evidence:
- `local-analysis/cpp-offline/direct-usb-path-attribution.json`.
- `local-analysis/direct-usb-soundcheck/20260617T183629Z-pairA-12s-after-hal-fail`.

Next implication:
- The next useful physical experiment is not another HAL rebuild. It is a
  controlled route/capture isolation test: known-good non-Audio8 source into
  the same mixer/iRig path, or a direct Audio 8 output-to-iRig fixture that
  removes mixer/EQ ambiguity. Only after the post-USB route is understood do
  HAL CPU/lag optimizations become product-decisive.

## 2026-06-17: iRig Idle Capture Is Clean, Signal Route Still Unproven

Decision:
- Add `opena8djcpp_irig_idle_capture_gate` as a compiled evidence gate over
  saved iRig idle captures.
- Treat iRig idle cleanliness as route-isolation evidence only, not as product
  quality or DAC proof.

Reason:
- A lock-gated capture opened only `iRig Stream` for recording. It did not play
  audio, install/load HAL, restart CoreAudio, reset USB, or change default
  devices.
- The latest idle capture is quiet: max RMS `-66.94 dBFS`, max peak
  `-41.65 dBFS`, and max first-difference RMS `-68.87 dBFS`.
- This makes a permanently noisy iRig input less likely, while still leaving
  the under-signal path unresolved: mixer/REC OUT, cabling, Audio 8 analog
  output, or a route-specific interaction.

Alternatives discarded:
- Use idle capture as proof the physical route is healthy: rejected because no
  signal was present and mixer/REC OUT behavior under signal is still unknown.
- Keep optimizing packet format after clean USB diagnostics: rejected because
  the current failing evidence remains post-USB/physical.

Evidence:
- `local-analysis/irig-capture-isolation/20260617T185109Z-irig-idle-12s`.
- `local-analysis/cpp-offline/irig-idle-capture-gate.json`.

Next implication:
- The next minimum physical test is a known-good non-Audio8 source through the
  same mixer/REC OUT -> iRig route using a deterministic reference. If that
  passes, test Audio 8 Pair A directly to iRig without mixer/EQ ambiguity.

## 2026-06-17: Instrument Direct USB Timeline, Keep Reset Reply Wait Default

Decision:
- Add monotonic timeline instrumentation to the direct USB player and iRig
  recorder.
- Add experimental build flags for `AUDIO_PARAMS` reset no-wait and settle
  timing, but keep the default as `HAL_AUDIO_PARAMS_RESET_WAIT_FOR_REPLY=1`.
- Do not promote reset no-wait or reset no-wait + settle as a product
  optimization yet.

Reason:
- Locked direct USB evidence showed first captured energy only about `0.19s`
  after the first `OpenA8DJUSBWriteOutput` call, so the multi-second delay is
  not the analog/DAC path after first write.
- The large delay is before first write, inside `OpenA8DJUSBStart`, where the
  reset/stream `AUDIO_PARAMS` command sequence can wait for replies.
- Avoiding the reset reply wait with no settle failed startup completely.
- Small settle values were not stable enough to promote: `250ms` once started
  quickly but later returned to a multi-second start, and `100ms` produced no
  captured energy.

Alternatives discarded:
- Disable `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM`: still rejected because the
  prior HAL candidate safety run failed before playback.
- Make no-wait reset the default after a short run: rejected because startup was
  not stable and strict physical quality remained FAIL.
- Treat the `0.19s` first-write-to-energy delay as the product blocker:
  rejected because the dominant delay is before first write.

Evidence:
- `local-analysis/direct-usb-timeline-instrumentation/20260617T134242Z-pairA-6s-usbdiag-nsec`:
  `player_after_start_seconds=4.242518`,
  `player_after_first_write_seconds=4.242867`,
  `record_first_energy_record_seconds=4.882604`,
  `first_energy_after_first_write_seconds=0.191420`, strict quality FAIL.
- `local-analysis/direct-usb-reset-no-wait/20260617T134558Z-pairA-6s-usbdiag`:
  no-wait/no-settle `player_rc=6`, no energy above threshold.
- `local-analysis/direct-usb-reset-no-wait/20260617T134734Z-settle500ms-pairA-2s`:
  started with `player_after_start_seconds=2.636657`, but strict quality FAIL.
- `local-analysis/direct-usb-reset-no-wait/20260617T134755Z-settle250ms-pairA-2s`:
  started with `player_after_start_seconds=0.270771`, but strict quality FAIL.
- `local-analysis/direct-usb-reset-no-wait/20260617T134841Z-settle250ms-pairA-6s-usbdiag`:
  longer run returned to `player_after_start_seconds=4.502963`, strict quality
  FAIL.
- Runtime isolation after each physical run: PASS, HAL inactive, lock absent.

Next implication:
- Continue using the timeline instrumentation for diagnostics.
- Do not claim startup optimization readiness until the reset/control sequence
  has stable repeated evidence and does not regress physical quality.

## 2026-06-17: Separate Raw Capture/Playback Completions From Sampled Stream Stats

Decision:
- Add raw capture and playback completion counters and make
  `opena8dj-control` report them as `captureTransfersCompleted` and
  `playbackTransfersCompleted` when present.
- Keep the older sampled stream-stats counters for low-overhead diagnostics,
  but do not compare sampled completions against raw submitted/completed
  counts.

Reason:
- With `OPENA8DJ_HOT_STREAM_STATS_INTERVAL=16`, the existing
  `_streamStats.playbackTransfers` counter was sampled once per 16 playback
  completions. `playbackTransfersSubmitted` was raw.
- That produced misleading physical evidence such as
  `playbackTransfersSubmitted=8131` versus `playbackTransfersCompleted=508`.
- After adding `playbackTransfersCompletedRaw`, the same style of locked
  soundcheck reports `playbackTransfersSubmitted=8123` and
  `playbackTransfersCompleted=8123`.
- The apparent next conclusion, playback running `16x` faster than capture,
  was also invalid because `captureTransfersCompleted` was sampled too.
  Adding `captureTransfersCompletedRaw` makes the same style of retry run
  report `captureTransfersCompleted=8137`,
  `playbackTransfersSubmitted=8129`, and
  `playbackTransfersCompleted=8129`.

Alternatives discarded:
- Disable hot-stream-stats sampling: rejected because it would add mutex work
  back into the completion path for every transfer.
- Divide submitted by the sampling interval: rejected because it hides the real
  raw transfer rate and is wrong when sampling policy changes.
- Rename existing public keys immediately: rejected because scripts already
  consume `captureTransfersCompleted` and `playbackTransfersCompleted`; the
  control tool can preserve the keys while using raw fields when available.

Evidence:
- `local-analysis/physical-stream-stats-contract/20260617T132743Z-288f65a`:
  before fix, `8131` submitted versus `508` reported completed.
- `local-analysis/physical-stream-stats-raw-completions/20260617T133008Z-288f65a`:
  after fix, `8123` submitted and `8123` completed.
- `local-analysis/physical-stream-stats-raw-capture-playback/20260617T133414Z-fe668ef-retry`:
  after adding raw capture too, `8137` capture completions, `8129` playback
  submissions, and `8129` playback completions.
- The fixed capture/playback run still failed quality:
  alignment `0.969899`, SNR `10.93 dB`, `20` lag jumps.
- Runtime isolation after cleanup:
  `local-analysis/runtime-isolation/post-stream-stats-raw-capture-playback-physical.json`,
  PASS, HAL inactive, lock absent.

Next implication:
- The `16x` playback/capture completion-rate hypothesis is rejected as a
  measurement artifact. CPU work must use the new raw counters and cannot lean
  on the older sampled transfer counters for rate comparisons.

## 2026-06-17: Make Stream-Stats Payload Drift A Gate Failure

Decision:
- Make `hal` build `build/opena8dj-control` as part of the same local build
  used before physical HAL tests.
- Make the control tool rebuild when `src/hal/OpenA8DJUSB.m` changes.
- Add an offline stream-stats contract gate that compares the duplicated
  `OpenA8DJStreamStatsPayload` layout between HAL and control source files.

Reason:
- Recent physical evidence showed `playbackTransfersSubmitted` around `16x`
  higher than playback completions. That value cannot be used for readiness
  until the observability contract is trustworthy.
- The HAL and `opena8dj-control` duplicate the stream-stats struct by hand.
  Without a gate, a payload edit in one file can silently produce misleading
  physical-test metrics.
- `run-soundcheck` reads `build/opena8dj-control`; forcing it to rebuild with
  the HAL reduces the chance of stale local tooling during a locked run.

Alternatives discarded:
- Trust the existing binary manually: rejected because physical-test evidence
  must be reproducible and tool-version safe.
- Delay until a full shared-header refactor: rejected because the cheap gate
  blocks drift now without a risky HAL refactor.
- Treat `playbackTransfersSubmitted` as fixed: rejected. The gate proves field
  parity and build freshness only; the next physical run must still verify the
  counter's runtime meaning.

Evidence:
- `python3 scripts/check-stream-stats-contract.py`: PASS, `166` fields in both
  sources, `0` mismatches.
- `make -B hal`: PASS and rebuilt both HAL and `build/opena8dj-control`.
- `scripts/run-cpp-offline-gates`: PASS, with
  `local-analysis/cpp-offline/stream-stats-contract.json` included in
  `current-offline-gates.json`.

## 2026-06-17: Reject ISO64 And Playback Coalescing As C++ Defaults

Decision:
- Keep C++ transfer-rate changes rejected unless a later design proves equal
  or better physical quality.
- Do not adopt ISO64 as the C++ HAL default even though it reduces OpenA8DJ
  driver CPU p95 to mainline-like levels.
- Do not adopt playback coalescing as a CPU optimization.
- Do not promote the unrolled output packer to default yet.
- Keep the transfer-ledger call-site prune as callback hygiene only, not as a
  product performance or audio-quality claim.

Reason:
- The current C++ HAL can trade quality for CPU, but that is not a product
  improvement. Audiophile readiness requires lower resource use with equal or
  better physical sound quality.
- ISO64+unrolled measured OpenA8DJ driver CPU p95 `5.5%`, but physical quality
  collapsed: alignment `0.186393`, SNR `-20.96 dB`, and `60` lag jumps.
- Coalesce2+unrolled lowered CPU p95 to `16.5%`, but physical quality also
  collapsed: alignment `0.258519`, SNR `-18.71 dB`, and `57` lag jumps.
- Unrolled output packing with ISO8 had better alignment than the rejected
  transfer-rate variants but still failed physical quality and produced click
  evidence.
- The default ledger-pruned build also failed the subsequent physical run, so
  clean internal counters and offline byte gates remain insufficient.

Alternatives discarded:
- Accept ISO64 because CPU matches mainline: rejected because physical music
  capture is far below product thresholds.
- Accept coalesce2 as a compromise: rejected because it still fails quality and
  does not meet the CPU target.
- Accept unrolled packing as default after one better alignment run: rejected
  until click behavior and repeatability are understood.
- Use `playbackTransfersSubmitted` as readiness evidence: rejected because the
  counter currently reports about `16x` the completion count in physical runs
  and needs a contract fix.

Evidence:
- `local-analysis/physical-unrolled-pack/20260617T131114Z-640dee9`: FAIL,
  alignment `0.962106`, SNR about `9.24 dB`, `44` lag jumps, clicks `113`,
  driver CPU p95 `22.0%`.
- `local-analysis/physical-iso64-unrolled/20260617T131356Z-640dee9`: FAIL,
  alignment `0.186393`, SNR `-20.96 dB`, `60` lag jumps, driver CPU p95
  `5.5%`.
- `local-analysis/physical-coalesce2-unrolled/20260617T131709Z-640dee9`:
  FAIL, alignment `0.258519`, SNR `-18.71 dB`, `57` lag jumps, driver CPU p95
  `16.5%`.
- `local-analysis/physical-ledger-callsite-prune/20260617T131921Z-640dee9`:
  FAIL, alignment `0.234322`, SNR `-17.53 dB`, `61` lag jumps, driver CPU p95
  `23.4%`.
- Runtime isolation after cleanup: PASS, HAL inactive, lock absent.

## 2026-06-17: Reduce HAL Hot-Path Locks Without Changing Cadence

Decision:
- Fold output timeline start-frame resolution into the existing
  `OutputTimelineWrite` lock.
- Aggregate input stats locally per capture transfer, then merge once under
  `_inputStatsMutex`.
- Do not treat this as a product-performance or audio-quality claim until a
  locked physical A/B run proves lower CPU with equal or better quality.

Reason:
- Coalescing showed that CPU can improve while quality gets worse, so further
  CPU work must preserve USB cadence and payload bytes.
- These changes remove avoidable mutex acquisitions in callback-adjacent paths
  without changing sample conversion, routing, output bytes, transfer count,
  queue depth, sample rate, or device defaults.

Alternatives discarded:
- Disable stats: rejected because stats-off already reduced observability and
  did not produce a valid readiness candidate.
- Coalesce playback completions: rejected by physical quality and the new
  burst cadence gate.
- Transfer-pool cursor: rejected by prior HAL safety failure.

Evidence:
- `make -B hal usb-play`: PASS.
- `make -B hal HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1`: PASS.
- `scripts/run-cpp-offline-gates`: PASS.
- `local-analysis/runtime-isolation/post-hal-hotpath-lock-reduction.json`:
  PASS, HAL inactive, lock absent.
- `local-analysis/promotion-readiness-current.json`: FAIL,
  `branch_promotion_allowed=false`.

Physical outcome:
- Locked HAL safety for commit `056d29b`: PASS.
- Locked Pair A/iRig music soundcheck:
  `local-analysis/soundcheck/20260617-hotpath-lock-056d29b-irig-pairA-16s-cpp-hal`,
  FAIL.
- Key failures: SNR `10.41 dB`, `43` lag jumps, mid/high residual
  `1.430949/1.358723`, driver CPU p95 `37.5%`.
- Decision update: keep the code hygiene change, but reject it as a physical
  readiness/performance candidate. Do not run another standalone physical
  test of this candidate without a new transport/cadence/device-state change.

## 2026-06-17: Reject CPU Wins That Violate Playback Cadence

Decision:
- Add an offline playback burst cadence model to `opena8djcpp_jitter_model`.
- Treat playback coalescing that increases completion spacing beyond `1.25x`
  the capture period as unsafe unless a later physical design proves otherwise.
- Keep `HAL_PLAYBACK_COALESCE_TRANSFERS=2` rejected as a quality candidate,
  despite its lower driver CPU.

Reason:
- The coalesce2 physical soundcheck reduced OpenA8DJ driver p95 CPU to
  `28.5%`, but quality failed badly: alignment `0.898854`, SNR `5.85 dB`,
  `45` lag jumps, mid residual `2.5634`, high residual `1.6666`.
- The modeled cadence shows the mechanism plainly: coalesce2 halves the
  completion count, but doubles playback completion spacing from `64` to `128`
  frames. That is not an audiophile-safe optimization.

Alternatives discarded:
- Accept coalesce2 as a performance improvement: rejected because product
  quality and cadence are worse.
- Keep the insight only in prose: rejected because readiness gates must block
  misleading CPU-only wins automatically.
- Reject all future transaction-count work: rejected because CPU still matters,
  but future work must preserve cadence or prove a safer pacing model.

Evidence:
- `local-analysis/cpp-offline/jitter-model.json`: `burst_rows=3`,
  `burst_failures=0`, coalesce2 and coalesce4 marked unsafe by model and
  expected rejection.
- `local-analysis/cpp-offline/current-offline-gates.json`: offline gates PASS
  and `unsafe_burst_scenarios` records the rejected burst profiles.
- `local-analysis/soundcheck/20260617-coalesce2-only-43773be-irig-pairA-16s-cpp-hal/metrics.json`:
  physical coalesce2 quality failure.

## 2026-06-17: Reject Reset-Off Variant Before Soundcheck

Decision:
- Keep `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=1` as the default.
- Do not run music soundcheck with `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0`
  unless a new hypothesis explains its load-time CoreAudio CPU spike.

Reason:
- The reset-off variant failed the HAL candidate safety gate before playback or
  capture. CoreAudio enumeration passed, but `coreaudiod` reached `115.1%` and
  total watched CPU reached `130.0%`, exceeding the safety threshold.
- The OpenA8DJ driver process itself was nearly idle (`0.1%`), so this looks
  like a CoreAudio load/enumeration interaction, not a playback-quality result.

Alternatives discarded:
- Proceed to soundcheck despite safety failure: rejected because the candidate
  already violated the low-resource/safe-load gate.
- Treat reset-off as neutral because no audio was played: rejected because
  high `coreaudiod` CPU during load is itself a product blocker.

Evidence:
- Build evidence:
  `local-analysis/physical-reset-audio-params-off/20260616-203712/build.log`
  contains `-DOPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM=0`.
- Safety evidence:
  `local-analysis/physical-reset-audio-params-off/20260616-203712/hal-candidate-safety/summary.txt`.
- Recovery evidence:
  `local-analysis/runtime-isolation/post-reset-audio-params-off-safety-fail.json`
  returns `PASS`, with HAL inactive and lock absent.

## 2026-06-17: HAL Flag Changes Must Force Rebuild

Decision:
- Add `build/.hal-cflags.stamp` and make `hal` plus `usb-play` rebuild when
  `HAL_CFLAGS` changes.
- When the stamp detects a flag change, remove only the generated HAL and
  `usb-play` binaries inside the C++ worktree so the next target cannot reuse a
  stale binary.

Reason:
- During reset-off preparation, `make HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0
  usb-play hal` initially reported parts as already up to date. That could have
  led to a physical test using a previous binary.
- Build flags are part of the candidate identity. Physical evidence is invalid
  if the binary was not rebuilt with the intended flags.

Alternatives discarded:
- Rely on manual `rm` before every variant: rejected because it is easy to miss
  under time pressure.
- Use `make -B` for every physical variant: rejected as a workaround rather
  than a reproducible build contract.

Evidence:
- `local-analysis/build-flags/reset0-rebuild-v2.log`: changing default to
  `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=0` rebuilds both affected targets.
- `local-analysis/build-flags/reset0-repeat-v2.log`: repeating the same flags
  does not rebuild.
- `local-analysis/build-flags/default-restore-v2.log`: restoring default
  `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM=1` rebuilds both affected targets.

## 2026-06-17: Promotion Evaluator Must Use Latest Paired Soundcheck Evidence

Decision:
- Make `scripts/evaluate-promotion-readiness.py` select the latest existing
  `local-analysis/soundcheck/*/metrics.json` and `cpu-profile.tsv` by default.
- Add a `latest_music_cpu_pair` gate requiring the music metrics and CPU profile
  to come from the same soundcheck directory.

Reason:
- The evaluator previously used fixed default paths. That still produced a
  `FAIL`, but it could hide the fact that newer physical evidence was worse or
  better than the fixed run.
- Promotion depends on current evidence, not any older convenient passing or
  failing artifact.

Alternatives discarded:
- Keep manual `--music` and `--cpu` paths only: rejected because the default
  promotion command must be safe and hard to misuse.
- Permit unpaired latest music/CPU files: rejected because quality and resource
  claims must refer to the same physical run.

Evidence:
- `scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json`
  returns `FAIL` and selects
  `local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal`
  for both music and CPU evidence.
- Current blockers remain physical music quality, runtime CPU, physical
  investigation readiness, and unvalidated physical Traktor/timecode.

## 2026-06-17: Keep Audio-Params Reset Enabled But Testable

Decision:
- Add `HAL_RESET_AUDIO_PARAMS_BEFORE_STREAM`, defaulting to `1`, and make the
  HAL reset `AUDIO_PARAMS` with rate code `0xff` only when that flag is enabled.

Reason:
- Mainline exposed this as a physical-test knob. C++ was always performing the
  reset, which made one-factor parity tests harder.
- The default remains enabled to preserve current behavior until a locked
  physical test proves that skipping the reset is better.

Alternatives discarded:
- Disable the reset by default: rejected because prior physical history does
  not prove that this improves quality or startup reliability.
- Leave the behavior hard-coded: rejected because it blocks controlled
  mainline-parity experiments.

Evidence:
- `make usb-play hal`
- `scripts/run-cpp-offline-gates`: Debug `16/16` PASS, Release `17/17` PASS.
- Release bench after the change: `pack_mib_s=1657.06`,
  `decode_mib_s=585.852`, `route_frames_s=9.46081e+08`.

## 2026-06-16: Make The Hardware Lock Mandatory In Physical Scripts

Decision:
- Add a shared hardware-lock helper and require it in HAL candidate safety,
  direct Audio 8 DJ gates, and physical soundcheck.
- Add an offline CTest policy gate so removing that lock discipline fails the
  normal offline gate path.

Reason:
- The goal requires more physical testing, but physical tests are only useful
  if they cannot race mainline QA, other agents, CoreAudio reloads, or USB
  recovery work.
- `test-hal-candidate-safety` performs HAL install/reload and `coreaudiod`
  restart, and `run-soundcheck` plays/captures audio; both must be gated before
  any further physical evidence can be trusted.

Alternatives discarded:
- Rely on operator discipline or docs only: rejected because previous hardware
  work already exposed lock/process coordination risk.
- Put lock only in outer manual commands: rejected because direct script
  invocation would still be unsafe.

Evidence:
- `local-analysis/cpp-offline/hardware-lock-policy.json` records `PASS`,
  `4` audited scripts, and `0` missing requirements.
- `local-analysis/cpp-offline/current-offline-gates.json` records
  `hardware_lock_policy=PASS`.

## 2026-06-16: Require C++ Mode 2 Bytes To Match The Python Oracle

Decision:
- Add a cross-oracle gate that compares C++ Mode 2 packed output bytes against the inherited Python oracle for the full 72-row matrix.
- Match C++ Float32-to-S24 quantization to the oracle's Float32 rounding path.

Reason:
- The previous C++ packet matrix proved internal round-trip behavior but could still pass if pack and decode shared the same wrong quantization.
- The Python oracle is independent and was inherited from the proven mainline behavior checks. Byte-for-byte parity is stronger evidence than round-trip alone.

Alternatives discarded:
- Keep only the Python text oracle: rejected because it did not execute C++.
- Keep only C++ round-trip tests: rejected because they do not prove parity with the external oracle.

Evidence:
- `local-analysis/cpp-offline/mode2-cross-oracle-parity.json` records `72` rows, `0` failures, `max_byte_mismatches=0`, `max_length_delta=0`, `total_check_errors=0`, and `total_panic_flags=0`.
- `local-analysis/promotion-readiness-current.json` includes `offline_mode2_cross_oracle_parity=PASS`.

## 2026-06-16: Create Isolated C++/DriverKit Worktree

Decision:
- Use `/Users/fer/dev/audio8djcpp` on branch `driverkit/cpp-redesign`.

Reason:
- The C/Objective-C mainline and Rust experiment must remain read-only references.

Alternatives discarded:
- Editing `/Users/fer/dev/opena8dj`: rejected by isolation rule.
- Editing `/Users/fer/dev/audio8djrust`: rejected by isolation rule.

Evidence:
- `git worktree list --porcelain` shows a separate C++ worktree and branch.

## 2026-06-16: Start With Pure C++ Offline Core

Decision:
- Build a C++20 core first, with DriverKit only as a prepared shell.

Reason:
- Offline gates can validate packet, routing, metrics, and policy behavior without installing drivers or touching hardware.

Alternatives discarded:
- Direct DriverKit implementation first: rejected because signing, entitlement, install, and activation risk would obscure core behavior.
- Blind C port: rejected because the intended strategy is greenfield shell, brownfield behavior.

Evidence:
- `opena8djcpp_core_contract` validates initial channel/routing/surface contracts offline.

## 2026-06-16: No Readiness Claim From Compile Alone

Decision:
- A clean build is evidence only for build health, not product readiness.

Reason:
- Audio quality, jitter, routing, timecode, and physical behavior require measurable gates and later coordinated hardware tests.

Alternatives discarded:
- Declaring physical readiness after scaffold: rejected.

Evidence:
- `docs/SUCCESS_METRICS.md` and `docs/TEST_PLAN.md` define required gates.

## 2026-06-16: Split Functional And Performance Gates By Build Type

Decision:
- Keep the default build as a safe functional contract gate.
- Run performance/resource gates only in `Release` or `RelWithDebInfo`.

Reason:
- Debug/no-optimization throughput is not a valid comparison against mainline or Rust oracle performance.
- Release CTest now includes `opena8djcpp_offline_bench`, which fails if pack/decode are below `100 MiB/s`, routing is below `1,000,000 frames/s`, or Mode 2 check/panic counters are nonzero.

Alternatives discarded:
- Treat Debug benchmark failure as product failure: rejected because it measures compiler mode more than data-plane design.
- Hide performance from CTest entirely: rejected because objective performance evidence must block claims.

Evidence:
- `local-analysis/cpp-offline/offline-bench-release.json` records `pack_mib_s=924.305`, `decode_mib_s=499.804`, `route_frames_s=5.17603e+08`, `check_errors=0`, and `panic_flags=0`.

## 2026-06-16: Keep Start Byte 4 For Physical Output

Decision:
- Keep `OPENA8DJ_OUTPUT_START_BYTE=4` for the current HAL transport candidate.

Reason:
- Long physical tone capture with `ISO5/q64/start_byte=4/big/gain0.50` measured
  a clean 1 kHz tone through iRig: `sideband_ratio=0.000657`, strongest sideband
  `-64.78 dB`, `click_outliers=0`, no clipping.
- `start_byte=2` produced much higher output level but worse tone quality:
  `sideband_ratio=0.080717`, residual ratio `0.985763`, and peak at `1.0`.
- `ISO64/q8` was rejected immediately for physical output because the same tone
  worsened to `sideband_ratio=0.791833` and `click_outliers=372`.

Alternatives discarded:
- `start_byte=2`: rejected because it behaves like malformed audio despite
  loud signal.
- `ISO64/q8`: rejected because it catastrophically worsened sidebands/clicks.

Evidence:
- `local-analysis/hardware-quality/20260616-170024-start-byte-2v4-tone-long/start-byte-4/tone-analysis.txt`
- `local-analysis/hardware-quality/20260616-170024-start-byte-2v4-tone-long/start-byte-2/tone-analysis.txt`
- `local-analysis/hardware-quality/20260616-165746-iso64-q8-irig-tone-pairA/tone-analysis.txt`

## 2026-06-16: Do Not Claim Physical Readiness From Current iRig Results

Decision:
- The current C++/HAL candidate is not ready to claim better-than-mainline
  physical quality or low-resource operation.

Reason:
- The best real-music iRig gate still fails strict thresholds:
  `quality_alignment_score=0.938154`, `analog_snr_db=8.93`,
  `mid_band_residual_ratio=1.379896`, `high_band_residual_ratio=1.347577`,
  `lag_jumps_gt_2_frames=24`.
- CPU evidence is not yet clean: the same run recorded `opena8dj_driver_p95=11.5`
  and `coreaudiod_p95=95.8` in the profiling window, which is not acceptable as
  a low-resource claim without a controlled baseline comparison.
- Removing cadence diagnostics caused a CoreAudio enumeration hang during a tone
  attempt; the system had to be recovered by reinstalling the cadence-diagnostic
  variant.

Alternatives discarded:
- Declaring readiness from clean tone sidebands: rejected because tone quality
  does not cover real-music residuals, lag jumps, CPU, or Traktor/timecode.

Evidence:
- `local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4/summary.txt`
- `local-analysis/hardware-quality/20260616-170759-recover-install-iso5-q64-start4-cadence`

## 2026-06-16: Benchmark Preallocated Mode 2 Decode As The Real-Time Metric

Decision:
- Treat `decode_mode2_usb_bytes_into` as the primary offline decode performance
  metric because it writes into caller-owned storage and matches the intended
  real-time data-plane policy.
- Keep the allocating wrapper benchmark as informational only.

Reason:
- The previous benchmark measured `decode_mode2_usb_bytes`, which resizes a
  `std::vector` and can mix harness allocation cost with decode cost.
- The real-time path must use preallocated storage, zero hot-loop allocations,
  and bounded work.
- The decode hot loop also used per-byte modulo operations for transfer/group
  position. Replacing them with bounded counters preserves behavior while
  reducing arithmetic variance.

Alternatives discarded:
- Continue using the allocating wrapper as the product metric: rejected because
  it does not model the callback-safe path.
- Skip the allocating wrapper entirely: rejected because it remains useful for
  tests and developer tooling.

Evidence:
- `local-analysis/cpp-offline/offline-bench-release.json` records
  `decode_into_mib_s=570.085`, `decode_allocating_mib_s=552.130`,
  `decode_into_output_overflows=0`, `decode_into_check_errors=0`, and
  `decode_into_panic_flags=0`.
- `scripts/run-cpp-offline-gates` passed default CTest `8/8`, Release CTest
  `9/9`, packet matrix `72/72`, Python Mode 2 oracle, timecode matrix, DVS
  signal smoke, realtime audit, jitter model, static policy, and evidence
  schema after the change.

## 2026-06-16: Reject Packer Counter Rewrite After Measured Regression

Decision:
- Keep the original `Mode2OutputPacker::fill_into` modulo/check-byte structure
  for now.

Reason:
- A counter-based rewrite looked cleaner but degraded measured pack throughput
  to about `1149.74 MiB/s` in the same Release benchmark flow, versus the
  reverted final median of `1602.11 MiB/s`.
- Real-time code changes must be accepted by evidence, not by aesthetics.

Alternatives discarded:
- Shipping the counter rewrite: rejected because it made the hot output packer
  materially slower.

Evidence:
- Final post-revert `local-analysis/cpp-offline/offline-bench-release.json`
  records `pack_mib_s=1602.11`, `decode_into_mib_s=570.085`,
  `float_to_s24_frames_s=85096700`, `route_frames_s=950086000`, and
  `route_reversed_frames_s=581896000`.

## 2026-06-16: Add Offline DriverKit Shell Contract Without Dext Claim

Decision:
- Compile and test a DriverKit shell lifecycle contract as ordinary C++ while
  the machine lacks DriverKit SDK/AudioDriverKit framework support.

Reason:
- The previous `driverkit/` code was a placeholder with empty start/stop
  methods. That is not enough for architecture confidence, even though a real
  dext build is still blocked by local toolchain/entitlement constraints.
- The offline shell validates device model ownership and bounded lifecycle
  behavior without installing, activating, or reloading any System Extension.

Alternatives discarded:
- Declare DriverKit readiness from the surface model alone: rejected because it
  did not execute any shell code.
- Attempt real dext install/activation now: rejected because the current
  toolchain lacks DriverKit SDK and hardware/system-extension operations require
  a separate authorized window.

Evidence:
- `local-analysis/cpp-offline/driverkit-shell-contract.json` records PASS with
  `system_extension_activated=false`.
- `local-analysis/promotion-readiness-current.json` now includes
  `offline_driverkit_shell_contract=PASS` but still returns FAIL overall because
  physical music quality, runtime CPU, and physical Traktor/timecode vinyl are
  not proven.

## 2026-06-16: Require DVS Claims To Traverse Mode 2 Input Decode

Decision:
- Offline DVS/timecode evidence must include a path that packs synthetic input
  into Mode 2 USB bytes, decodes those bytes through the selected input profile,
  and then runs the timecode analyzer on the decoded deck pair.

Reason:
- Direct synthetic signal analysis proves analyzer thresholds, but it does not
  prove the packet/input decode path feeding Traktor-facing data.
- Playback mode must keep input decode disabled while still preserving packet
  stats for observability.
- Timecode-vinyl, CD-line, and phono modes must explicitly enable decode and
  preserve A/B/C/D deck isolation.

Alternatives discarded:
- Treat `dvs-signal-smoke` alone as DVS readiness: rejected because it bypasses
  Mode 2 input decode.
- Always decode input in playback profile: rejected because playback mode should
  not spend data-plane work on timecode capture unless the profile enables it.

Evidence:
- `local-analysis/cpp-offline/dvs-packet-input-decode.json` records `24` rows,
  `0` failures, zero leakage, no decode overflows, and playback decode-off PASS.
- `local-analysis/promotion-readiness-current.json` includes
  `offline_dvs_packet_input_decode=PASS` but still returns FAIL overall because
  physical real-music quality, runtime CPU, and physical Traktor/timecode vinyl
  are not proven.

## 2026-06-16: Represent Full Routing As Fixed Route Entries

Decision:
- Extend C++ `RoutingMatrix` from source-channel-only mapping to fixed
  `RouteEntry` rows with source channel and precomputed gain `1`, `0`, or `-1`.

Reason:
- The Rust oracle explicitly covers identity, muted channels, pair remap,
  side swap, and inversion. C++ had only identity/reversed source mapping.
- Full A/B/C/D routing and DVS workflows need mute/invert/cross-deck behavior
  without introducing dynamic storage in the hot path.

Alternatives discarded:
- Keep source-only routing: rejected because it cannot represent mute or
  inversion.
- Add per-frame dynamic routing decisions: rejected because routing policy must
  be prevalidated outside the real-time path.

Evidence:
- `opena8djcpp_core_contract` passes the Rust-oracle compound case: A from D,
  B muted, C side-swapped, and D right inverted.
- `local-analysis/cpp-offline/offline-bench-release.json` records
  `route_advanced_frames_s=2.71652e+08`.
- `scripts/evaluate-promotion-readiness.py` now requires advanced routing
  throughput above the offline floor before promotion can pass.

## 2026-06-16: Distinguish Mode 2 Check Cadence From Full Frame Bytes

Decision:
- Keep two separate constants in the C++ protocol model: Mode 2 check cadence
  is `16` bytes, while one full 8-channel audio frame is `32` bytes.

Reason:
- Mainline behavior and packet checks operate on a 16-byte cadence, but the
  product data model still represents a full 8-channel frame as 32 USB bytes.
- The new protocol contract initially caught this ambiguity. Keeping both
  names prevents later code from treating the check cadence as the whole audio
  frame or vice versa.

Alternatives discarded:
- Collapse both meanings into `kMode2GroupBytes`: rejected because it hides a
  real protocol distinction and would make future DriverKit/USB transport code
  easier to misread.
- Advertise 88.2/96 kHz as supported now: rejected until physical quality
  evidence exists for those rates; the CAIAQ rate codes are modeled but marked
  deferred.

Evidence:
- `local-analysis/cpp-offline/protocol-contract.json` records PASS with
  `check_cadence_bytes=16`, `full_frame_bytes=32`,
  `default_start_byte=4`, VID/PID `0x17cc:0x1978`, endpoints
  `0x01/0x81/0x82/0x06`, and 8 input/output channels.
- `scripts/evaluate-promotion-readiness.py` now requires this protocol
  contract before branch promotion can pass.

## 2026-06-16: Require Full Simulated Output Matrix Before Promotion

Decision:
- Add a deterministic C++ simulated-output matrix gate covering output pairs
  A/B/C/D at 44.1/48 kHz with dense, transient, and wideband material at gains
  `1.0` and `0.5`.

Reason:
- The previous simulated output oracle was one strong software-output fixture,
  but it did not prove all output pairs and required rates through the C++ Mode
  2 pack/decode path.
- A deterministic in-process C++ gate avoids dependence on local music files
  while still checking alignment, quantization residual, SNR, check bytes,
  panic flags, and wrong-pair leakage.

Alternatives discarded:
- Depend on `scripts/run-simulated-output-soundcheck` inside the core offline
  gate: rejected because it may depend on local music discovery/conversion and
  is slower/noisier than a deterministic core contract.
- Treat packet matrix as sufficient output-quality evidence: rejected because
  it checks byte parity but not program-material SNR/residual/leakage metrics.

Evidence:
- `local-analysis/cpp-offline/simulated-output-matrix.json` records PASS with
  `48` rows, `0` failures, minimum SNR `119.407 dB`, max residual ratio
  `1.07069e-06`, and max leakage `-240 dBFS`.
- `scripts/evaluate-promotion-readiness.py` now requires
  `offline_simulated_output_matrix=PASS` before branch promotion can pass.

## 2026-06-16: Keep Physical Readiness Blocked After USB Tone/Music Failure

Decision:
- Do not promote the C++ candidate and do not claim hardware/listening
  readiness. Keep Mode 2 output defaults at big-endian, start byte `4`, check
  offset `8`, but make the direct USB diagnostic tool build with the same
  `HAL_CFLAGS` as the HAL bundle. Promotion evaluation must also consume the
  latest physical investigation summary, so negative current evidence overrides
  older passing runs.

Reason:
- Locked physical evidence showed the iRig route is available and clean when
  idle, but Audio 8 playback through the C++ HAL/direct USB path produces
  severe analog mismatch/noise.
- Start-byte, endian, check-offset, valid-capture-layout, and ISO-frame sweeps
  did not produce a passing physical tone. The best meaningful direct tone
  evidence remains far below product threshold, around `-17 dB` to `-19.5 dB`
  tone SNR depending on variant.
- The prior direct USB tool was not compiled with HAL flags, so its evidence
  could mislead future debugging unless the build was fixed.
- The previous promotion evaluator defaulted to an older physical music run;
  that made the report less conservative than the actual latest hardware state.

Alternatives discarded:
- Promote because offline gates pass: rejected because physical music quality,
  runtime CPU, and Traktor/timecode physical evidence still fail.
- Change default check offset away from `8`: rejected because the sweep did not
  improve quality and mainline notes still identify offset `8` as the least bad
  playback-compatible value.
- Enable valid-capture-layout by default immediately: rejected because the
  same-amplitude comparison improved tone SNR by only about `0.25 dB`, not a
  defensible product improvement.

Evidence:
- `local-analysis/usb-physical-investigation-summary.json`
- `local-analysis/soundcheck/20260616-185543-irig-pairA-24s-cpp-hal`
- `local-analysis/irig-baseline/20260616-191203-no-playback-9s`
- `local-analysis/start-byte-sweep/20260616-190418-tone1k-minus36db`
- `local-analysis/start-byte-sweep/20260616-190657-tone1k-minus36db-native-i24`
- `local-analysis/check-offset-sweep/20260616-191616-validlayout-tone1k-minus18db`
- `local-analysis/direct-usb-soundcheck/20260616-191418-validlayout-tone1k-minus18db`
- `local-analysis/direct-usb-soundcheck/20260616-191931-halflags-tone1k-minus18db`
- `local-analysis/runtime-isolation/current-after-usb-investigation.json`
- `local-analysis/promotion-readiness-current.json`

## 2026-06-16: Align C++ HAL Transport Defaults With Mainline 0.3.135 Baseline

Decision:
- Change C++ HAL build defaults from ISO5 with 64/64 capture/playback queues to
  ISO64 with 8/8 queues, and expose `OPENA8DJ_OUTPUT_PREFETCH_FRAMES` with a
  default build value of 64.

Reason:
- The latest mainline physical/CPU baseline documented for `0.3.135` used
  `HAL_ISO_FRAMES=64`, `HAL_CAPTURE_QUEUE=8`, `HAL_PLAYBACK_QUEUE=8`, and
  `HAL_OUTPUT_PREFETCH_FRAMES=64`.
- The C++ candidate had diverged to ISO5 and 64/64 queues, which changes USB
  cadence, transfer size, in-flight behavior, and callback load. Given the
  latest physical C++ failure plus driver CPU around `33-35%`, the next
  candidate should first match the known mainline transport profile before more
  speculative packet experiments.

Alternatives discarded:
- Keep ISO5 because one direct tone comparison beat ISO8: rejected because that
  result was still a severe physical FAIL and did not compare against the
  actual mainline ISO64 baseline.
- Continue changing packet byte offsets first: rejected because offset/endian
  sweeps did not find a passing physical tone.

Evidence:
- `/Users/fer/dev/opena8dj/Makefile` read-only defaults:
  `HAL_ISO_FRAMES=64`, `HAL_CAPTURE_QUEUE=8`, `HAL_PLAYBACK_QUEUE=8`,
  `HAL_OUTPUT_PREFETCH_FRAMES=64`.
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md` documents the
  `0.3.135` CPU/digital stability baseline using ISO64/8/8.
- `local-analysis/usb-physical-investigation-summary.json` records the C++
  ISO5-era physical failure.

Follow-up:
- The locked ISO64 C++ HAL run failed worse than the prior ISO5 run:
  `quality_alignment_score=0.051643`, `analog_snr_db=-31.90`,
  `lag_jumps_gt_2_frames=60`.
- ISO64/8/8 is rejected as a C++ default until the remaining lifecycle
  differences are ported and retested. The C++ default returns to ISO5/64/64,
  the less-bad measured profile.

Evidence:
- `local-analysis/soundcheck/20260616-iso64-irig-pairA-16s-cpp-hal`
- `local-analysis/runtime-isolation/post-iso64-failed-unload.json`
- `local-analysis/audio-stack-guard/20260616-iso64-force-unload/force-unload.log`

## 2026-06-16: Add HAL Transport Lifecycle Parity Knobs Before More Physical Sweeps

Decision:
- Add C++ HAL build-time support for playback request coalescing, configurable
  playback-before-capture-requeue ordering, and transfer-pool cursor checkout.
- Keep all new lifecycle knobs neutral by default:
  `HAL_PLAYBACK_COALESCE_TRANSFERS=1`,
  `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=0`, and
  `HAL_TRANSFER_POOL_CURSOR=0`.

Reason:
- Read-only mainline comparison found lifecycle-level transport behavior that
  C++ did not yet model, while ISO64/8/8/prefetch64 was already physically
  rejected in the C++ implementation.
- More byte/start/check sweeps are low-value until the C++ transport can test
  the same lifecycle degrees of freedom mainline uses.
- Neutral defaults preserve the current less-bad measured profile while making
  future physical variants explicit and reproducible.

Alternatives discarded:
- Promote because offline gates pass: rejected; physical music quality and CPU
  still fail.
- Keep changing USB packet offsets first: rejected; recent physical sweeps did
  not produce a passing analog result.
- Enable coalescing/cursor by default immediately: rejected; those variants need
  locked physical evidence before becoming candidate defaults.

Evidence:
- Build: `make usb-play hal` passed.
- Offline gates: `scripts/run-cpp-offline-gates` passed Debug `15/15` and
  Release `16/16`.
- Promotion gate: `scripts/evaluate-promotion-readiness.py --json-out
  local-analysis/promotion-readiness-current.json` returned `FAIL` as expected.
- Evidence paths:
  - `local-analysis/cpp-offline/current-offline-gates.json`
  - `local-analysis/promotion-readiness-current.json`

Follow-up:
- Locked physical evidence at commit `e0ad0a0` improved from the ISO64
  regression but still failed product thresholds:
  `quality_alignment_score=0.934891`, `analog_snr_db=8.73`,
  `lag_jumps_gt_2_frames=25`, driver CPU p95 `36.0%`.

Evidence:
- `local-analysis/soundcheck/20260616-lifecycle-irig-pairA-16s-cpp-hal`
- `local-analysis/runtime-isolation/post-lifecycle-failed-unload.json`
- `local-analysis/audio-stack-guard/20260616-lifecycle-force-unload/force-unload.log`
- `local-analysis/promotion-readiness-current.json`

## 2026-06-16: Gate USB Input Decode On Actual Input IO

Decision:
- Port mainline-style input decode activation to the C++ HAL/USB bridge:
  `OpenA8DJUSBSetInputDecodeActive`, activation from HAL `PrepareInputCycle`,
  deactivation on final `StopIO`, and capture decode bypass when no input
  client is active.

Reason:
- The failed lifecycle candidate still showed OpenA8DJ driver CPU p95 around
  `36%` during output-only soundcheck. C++ was decoding input traffic even when
  the active physical test only used Audio 8 DJ output and iRig capture.
- Mainline gates input decode from HAL input cycles, so this is behavior parity
  and a plausible resource reduction without changing packet layout or output
  format.

Alternatives discarded:
- Continue with packet byte sweeps: rejected because the latest failure is now
  dominated by CPU/resource waste plus residual analog quality, and recent
  offset/layout sweeps did not pass.
- Disable input decode globally: rejected because timecode vinyl and Traktor
  require input routing and DVS capture once those tests run.

Evidence:
- Build: `make usb-play hal` passed.
- Offline gates: `scripts/run-cpp-offline-gates` passed Debug `15/15` and
  Release `16/16`.
- Evidence path: `local-analysis/cpp-offline/current-offline-gates.json`

Follow-up:
- Locked physical retest of active input decode gating failed worse than the
  lifecycle candidate: `quality_alignment_score=0.028314`,
  `analog_snr_db=-28.18`, `lag_jumps_gt_2_frames=52`, OpenA8DJ driver CPU p95
  `41.8%`, CoreAudio p95 `21.8%`.
- Decision amended: keep the API/knob for explicit future experiments, but set
  `HAL_INPUT_DECODE_ACTIVE_GATING=0` by default. The current default candidate
  must not include this physically rejected behavior.

Evidence:
- `local-analysis/soundcheck/20260616-inputdecode-irig-pairA-16s-cpp-hal`
- `local-analysis/audio-stack-guard/20260616-inputdecode-force-unload/force-unload.log`
- `local-analysis/runtime-isolation/post-inputdecode-failed-unload.json`
- `local-analysis/promotion-readiness-current.json`

## 2026-06-16: Reject Playback-Before-Capture-Requeue Variant

Decision:
- Keep `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=0` as the default.

Reason:
- The variant compiled and passed offline gates, but locked physical evidence
  was worse than the lifecycle baseline and still far below product thresholds.
- It did not solve runtime CPU: OpenA8DJ driver p95 remained `36.0%`, while
  CoreAudio p95 spiked to `74.0%` in the measured run.

Evidence:
- `local-analysis/hal-candidate-safety/20260616-queue-before-cpp-lockpolicy-leave-loaded`
- `local-analysis/soundcheck/20260616-queue-before-irig-pairA-16s-cpp-hal`
- `local-analysis/audio-stack-guard/20260616-queue-before-force-unload/force-unload.log`
- `local-analysis/runtime-isolation/post-queue-before-failed-unload.json`
- `local-analysis/promotion-readiness-current.json`

## 2026-06-16: Reject Transfer-Pool Cursor Variant For Safety

Decision:
- Keep `HAL_TRANSFER_POOL_CURSOR=0` as the default and do not run physical audio
  with `HAL_TRANSFER_POOL_CURSOR=1` in the current code.

Reason:
- The variant passed offline gates, but failed the HAL candidate safety gate
  before any soundcheck.
- During the safety load/enumeration window, the OpenA8DJ driver process was
  nearly idle (`0.1%`), while `coreaudiod` spiked to `172.2%`. That is a system
  safety failure and not an acceptable basis for an audio-quality run.
- The safety script unloaded the HAL and recovery passed, so this was contained
  without leaving the system in a dirty state.

Alternatives discarded:
- Run soundcheck anyway: rejected because the safety gate exists specifically to
  block high-risk audio-stack states before physical playback/capture.
- Treat the spike as harmless transitory load: rejected until a bounded
  lifecycle explanation and a repeatable safe-load metric prove it.

Evidence:
- `local-analysis/hal-candidate-safety/20260616-pool-cursor-cpp-lockpolicy-leave-loaded`
- `local-analysis/runtime-isolation/pre-pool-cursor-physical.json`

## 2026-06-16: Disable Output Amplitude Stats By Default

Decision:
- Set C++ `HAL_OUTPUT_AMPLITUDE_STATS=0` by default while keeping the knob
  available for explicit diagnostic builds.

Reason:
- Mainline and Rust-derived notes both keep output amplitude stats disabled by
  default.
- C++ had the flag enabled by default, which adds per-output-frame peak and
  clipping work in the USB output fill path.
- The current blocker includes high driver CPU during output-only physical
  soundchecks, so default candidates should not carry nonessential hot-path
  diagnostics.

Alternatives discarded:
- Remove amplitude stats entirely: rejected because the field is still useful
  in targeted diagnostics.
- Keep stats enabled until quality is fixed: rejected because CPU/resource
  superiority is a product gate, and this default diverges from mainline.

Evidence:
- C++ default before change: `Makefile: HAL_OUTPUT_AMPLITUDE_STATS ?= 1`.
- Mainline default: `/Users/fer/dev/opena8dj/Makefile:
  HAL_OUTPUT_AMPLITUDE_STATS ?= 0`.

## 2026-06-16: Reject Mainline Preopen/Stop-ISOC Lifecycle Defaults In C++

Decision:
- Keep the newly ported lifecycle knobs available, but set
  `HAL_BACKGROUND_PREOPEN_ON_INIT=0` and `HAL_STOP_ISOC_ON_STOP=0` by default
  after physical rejection.

Reason:
- The mainline-style lifecycle candidate passed safety and offline gates, but
  locked physical music capture regressed severely:
  `quality_alignment_score=0.159859`, `analog_snr_db=-16.87`, and
  `lag_jumps_gt_2_frames=59`.
- This is worse than the prior less-bad lifecycle baseline and cannot be used
  as a default or promotion candidate.

Alternatives discarded:
- Keep the defaults because they match mainline: rejected because the C++ host
  lifecycle is not yet equivalent enough for those defaults to be safe.
- Run a longer soundcheck: rejected because the short controlled run already
  shows a severe failure and the gates are designed to stop there.

Evidence:
- `local-analysis/hal-candidate-safety/20260616-lifecycle-preopen-cpp-lockpolicy-leave-loaded`
- `local-analysis/soundcheck/20260616-lifecycle-preopen-irig-pairA-16s-cpp-hal`
- `local-analysis/audio-stack-guard/20260616-lifecycle-preopen-force-unload/force-unload.log`
- `local-analysis/runtime-isolation/post-lifecycle-preopen-failed-unload.json`

## 2026-06-16: Reject Fast Prefetch Clear And Atomic Write Stats Defaults

Decision:
- Keep the code paths available, but set `HAL_FAST_OUTPUT_PREFETCH_CLEAR=0`
  and `HAL_OUTPUT_WRITE_STATS=0` by default.

Reason:
- The isolated fast-clear/write-stats candidate passed offline and safety
  gates, but locked physical capture regressed severely:
  `quality_alignment_score=-0.048481`, `analog_snr_db=-32.06`,
  `lag_jumps_gt_2_frames=46`.
- The failure is too large to treat as normal run variance. It must not be a
  candidate until a targeted explanation exists.

Alternatives discarded:
- Keep these defaults because they are CPU optimizations: rejected because
  product quality gates outrank theoretical CPU savings.
- Keep only atomic write stats on: deferred until a separate controlled
  one-factor run is justified; the combined run was not safe enough to promote.

Evidence:
- `local-analysis/hal-candidate-safety/20260616-fastclear-writestats-cpp-lockpolicy-leave-loaded`
- `local-analysis/soundcheck/20260616-fastclear-writestats-irig-pairA-16s-cpp-hal`
- `local-analysis/audio-stack-guard/20260616-fastclear-writestats-force-unload/force-unload.log`
- `local-analysis/runtime-isolation/post-fastclear-writestats-failed-unload.json`

## 2026-06-16: Reject Calibrated Default And Direct USB Plain Variants

Decision:
- Keep C++ in `NOT_READY` status after the calibrated `-16 dB` physical run
  and two direct USB plain-CFLAGS diagnostics.
- Do not promote, do not claim better-than-mainline, and do not move C++ toward
  `main` until physical quality and CPU both beat the C baseline.

Reason:
- Calibrated HAL physical run still failed:
  `quality_alignment_score=0.960076`, `analog_snr_db=2.71`,
  `lag_jumps_gt_2_frames=35`, `mid_band_residual_ratio=1.565287`,
  `high_band_residual_ratio=1.461400`.
- Runtime CPU was worse than mainline `0.3.135`: C++ driver avg/p95
  `30.73%/36.70%`, while the C baseline documents driver p95 around `6.5%`.
- Direct USB plain-CFLAGS did not rescue quality:
  plain CFLAGS produced `quality_alignment_score=0.186400` with clipping, and
  plain CFLAGS plus only `OPENA8DJ_OUTPUT_GAIN=0.50f` produced
  `quality_alignment_score=0.023502`.
- Active-section stream stats in the HAL run did not show simple render
  starvation; the ring stayed near target and active underruns remained zero
  during the fed section.

Alternatives discarded:
- Treat earlier failures as overdrive artifacts: rejected because `-16 dB`
  still failed without clipping in the HAL path.
- Blame only `HAL_CFLAGS` contamination in `usb-play`: rejected because the
  plain-gain05 direct USB diagnostic still failed badly.
- Promote based on offline gates: rejected because offline gates currently do
  not predict physical DAC/capture quality for this transport.

Evidence:
- `local-analysis/soundcheck/20260616-default-minus16-irig-pairA-16s-cpp-hal`
- `local-analysis/direct-usb-soundcheck/20260616-plaincflags-minus16-music`
- `local-analysis/direct-usb-soundcheck/20260616-plaincflags-gain05-minus16-music`
- `local-analysis/runtime-isolation/post-default-minus16-failed-unload.json`

Follow-up:
- Prioritize below-HAL USB transport/cadence and device-state differences.
- Add byte-for-byte packer dumps and/or device-observed transfer diagnostics
  before more physical sweeps.

## 2026-06-16: Treat Mode 2 Packer Layout As Offline-Parity Verified

Decision:
- Do not spend the next physical window on more start-byte/check-offset layout
  sweeps unless new device-observed evidence contradicts the offline parity
  model.

Reason:
- Added an independent `opena8djcpp_mode2_mainline_layout_parity` gate that
  implements the mainline unrolled Mode 2 fill structure separately from the
  C++ `Mode2OutputPacker`.
- The gate passed `132` rows across start bytes `0..5`, short partial transfer
  sizes, and normal transfer sizes including `352`.
- This removes one important false-positive risk in the old offline oracle:
  packer/decode self-consistency is no longer the only byte-layout evidence.

Alternatives discarded:
- Continue byte-offset physical sweeps now: rejected because direct physical
  sweeps already failed and the independent byte layout gate now passes.
- Declare physical readiness from layout parity: rejected because physical iRig
  quality and CPU still fail badly.

Evidence:
- `local-analysis/cpp-offline/mode2-mainline-layout-parity.json`
- `scripts/run-cpp-offline-gates` with Debug `16/16`, Release `17/17`.

Follow-up:
- Focus on transfer cadence, USB transaction state, `AUDIO_PARAMS` reset
  behavior, and device-observed control/stream state.

## 2026-06-17: Add Hot Stream Stats Gate And Restore Atomic Output Write Stats

Decision:
- Port mainline-style `OPENA8DJ_ENABLE_HOT_STREAM_STATS` and
  `OPENA8DJ_HOT_STREAM_STATS_INTERVAL` gates to the C++ HAL, with default
  behavior still equivalent to the current per-completion stats path
  (`enabled=1`, `interval=1`).
- Change C++ `HAL_OUTPUT_WRITE_STATS` default from `0` to `1`, matching
  mainline atomic-only accounting and removing a useless mutex path where
  `addStreamStatAtOffset(outputFramesWritten)` was later overwritten by the
  atomic snapshot value.
- Add `outputLateWriteFrames` and `outputLateWriteBatches` observability to
  the C++ stream stats payload and control tool, matching mainline semantics.

Reason:
- Read-only comparison found C++ was paying a stream-stats mutex on every
  output write when `HAL_OUTPUT_WRITE_STATS=0`, but `streamStatsSnapshot`
  always overwrote `outputFramesWritten` from `_outputFramesWrittenAtomic`.
  That made the fallback both hot-path cost and unobservable.
- Mainline already uses atomic output write stats by default and reports late
  writes. C++ was missing late-write counters, leaving a plausible path where
  physical quality could be bad while output counters looked clean.
- The hot stream-stats gate gives controlled CPU/observability experiments
  without changing the default physical candidate behavior.

Alternatives discarded:
- Disable hot stream stats by default now: rejected until a locked physical run
  proves lower CPU without hiding required failure evidence.
- Treat the old fast-clear/write-stats physical rejection as proof against
  atomic write stats alone: rejected because that run changed two variables.
  `HAL_FAST_OUTPUT_PREFETCH_CLEAR=0` remains the default.

Evidence:
- `make usb-play hal`
- `make HAL_HOT_STREAM_STATS=0 usb-play hal`
- `make HAL_OUTPUT_WRITE_STATS=0 usb-play hal`
- `scripts/run-cpp-offline-gates` with Debug `16/16`, Release `17/17`.
- Logs:
  - `local-analysis/build-flags/hot-stats-off-build.log`
  - `local-analysis/build-flags/output-write-stats-off-build.log`
  - `local-analysis/build-flags/default-after-hot-stats-output-write-build.log`
  - `local-analysis/offline-gates-after-hot-stats-output-write.log`

Follow-up:
- Next locked physical candidate should verify whether atomic write stats plus
  late-write observability improves CPU diagnostics and whether any late writes
  correlate with iRig quality failure.
- If quality still fails, prioritize the other read-only findings: queue depth
  `64/64` versus mainline `8/8`, output prefetch `256` versus mainline `64`,
  and input/control-plane parity.

## 2026-06-17: Reject Hot-Stats/Atomic-Write Candidate As Not Ready

Decision:
- Do not promote commit `5e6fab7` and do not claim physical readiness after the
  locked iRig music run.
- Keep the code/tooling improvements, but continue optimization because the
  candidate still fails music quality and CPU gates.

Reason:
- Safety install passed, but physical soundcheck failed:
  `quality_alignment_score=0.962133`, `snr_db=10.24`,
  `click_outliers=29`, `lag_jumps_gt_2_frames=45`,
  `mid_band_residual_ratio=1.443461`,
  `high_band_residual_ratio=1.362932`, quiet mid-band noise `-35.91 dBFS`.
- CPU remains above mainline: OpenA8DJ driver avg/p95 `31.58%/36.00%`,
  coreaudiod avg/p95 `4.70%/7.00%`.
- Stream stats showed output write accounting is now observable and no active
  underruns/timeline resets/panic flags, but late-write text snapshots were
  all zero. Late writes do not explain this failure.

Alternatives discarded:
- Promote based on slight SNR/CoreAudio CPU improvement over the previous
  calibrated run: rejected because all product gates still fail and driver CPU
  is far above the C baseline.
- Retest the same candidate immediately: rejected until a new variable changes.

Evidence:
- Safety: `local-analysis/physical-hotstats-write-late/20260616-205111/hal-candidate-safety`
- Soundcheck: `local-analysis/soundcheck/20260616-hotstats-write-late-irig-pairA-16s-cpp-hal`
- Stream stats summary: `local-analysis/stream-stats/hotstats-write-late-summary-v2.json`
- Recovery/audit:
  - `local-analysis/audio-stack-guard/20260616-force-unload-hotstats-write-late`
  - `local-analysis/runtime-isolation/post-hotstats-write-late-failed-unload.json`

Follow-up:
- Fix structured TSV capture for late-write counters before the next physical
  run.
- Next one-factor physical hypotheses: reduce queue depth to mainline `8/8`,
  or reduce output prefetch to mainline `64`.

## 2026-06-17: Reject Queue-Depth 8/8 As Standalone Candidate

Decision:
- Do not change the default queue depths from C++ `64/64` to mainline `8/8`
  based on the standalone physical run.
- Keep `queue8` as a diagnostic clue only: it reduced click outliers, but did
  not beat quality or runtime CPU gates.

Reason:
- Physical soundcheck with `HAL_CAPTURE_QUEUE=8 HAL_PLAYBACK_QUEUE=8` still
  failed:
  `quality_alignment_score=0.964133`, `snr_db=10.22`,
  `lag_jumps_gt_2_frames=39`, `mid_band_residual_ratio=1.422599`,
  `high_band_residual_ratio=1.365050`, quiet mid-band noise `-36.08 dBFS`.
- Click outliers improved from `29` to `0`, and coreaudiod p95 improved from
  `7.0%` to `3.1%` versus the immediately preceding hotstats run.
- OpenA8DJ driver CPU worsened: avg/p95/max `32.335%/37.2%/37.9%`, still far
  above the mainline C baseline.
- Stream stats showed no late writes, no active underruns, no timeline resets,
  no panic flags, and capture transaction error ratio stayed around `2.273`.

Alternatives discarded:
- Promote `queue8` because clicks improved: rejected because SNR/residual/lag
  and driver CPU still fail.
- Make `queue8` default before testing prefetch/cadence: rejected because the
  driver CPU regression is material.

Evidence:
- Build/safety: `local-analysis/physical-queue8/20260616-205510`
- Soundcheck: `local-analysis/soundcheck/20260616-queue8-irig-pairA-16s-cpp-hal`
- Stream stats: `local-analysis/stream-stats/queue8-summary.json`
- Recovery/audit:
  - `local-analysis/audio-stack-guard/20260616-force-unload-queue8`
  - `local-analysis/runtime-isolation/post-queue8-failed-unload.json`
- Default rebuild restored `HAL_CAPTURE_QUEUE_DEPTH=64` and
  `HAL_PLAYBACK_QUEUE_TARGET=64`:
  `local-analysis/build-flags/default-restore-after-queue8.log`.

Follow-up:
- Next standalone physical hypothesis: reduce `HAL_OUTPUT_PREFETCH_FRAMES` from
  `256` to mainline `64`, with default queue depth restored to `64/64`.
- If prefetch alone improves quality without driver CPU regression, consider a
  later controlled combination with `queue8`; do not combine before a positive
  one-factor result.

## 2026-06-17: Reject Output Prefetch 64 As Standalone Candidate

Decision:
- Keep `HAL_OUTPUT_PREFETCH_FRAMES=256` as the C++ default.
- Do not combine `prefetch64` with `queue8`; the standalone prefetch result did
  not improve the product gates.

Reason:
- Physical soundcheck with `HAL_OUTPUT_PREFETCH_FRAMES=64` failed:
  `quality_alignment_score=0.956371`, `snr_db=10.40`,
  `click_outliers=4`, `lag_jumps_gt_2_frames=48`,
  `mid_band_residual_ratio=1.431220`,
  `high_band_residual_ratio=1.365281`, quiet mid-band noise `-35.98 dBFS`.
- Compared with the hotstats/atomic-write baseline, prefetch64 worsened
  quality alignment and lag jumps, and driver CPU p95 increased to `39.5%`.
- Stream stats still showed no active underruns, late writes, timeline resets,
  or panic flags. Capture transaction error ratio stayed around `2.273`.

Alternatives discarded:
- Adopt mainline prefetch `64` for parity: rejected because parity did not
  translate into better physical evidence in the C++ HAL.
- Test `queue8 + prefetch64`: rejected because both one-factor runs failed
  driver CPU/quality gates.

Evidence:
- Build/safety: `local-analysis/physical-prefetch64/20260616-205818`
- Soundcheck: `local-analysis/soundcheck/20260616-prefetch64-irig-pairA-16s-cpp-hal`
- Stream stats: `local-analysis/stream-stats/prefetch64-summary.json`
- Recovery/audit:
  - `local-analysis/audio-stack-guard/20260616-force-unload-prefetch64`
  - `local-analysis/runtime-isolation/post-prefetch64-failed-unload.json`
- Default rebuild restored `OPENA8DJ_OUTPUT_PREFETCH_FRAMES=256`:
  `local-analysis/build-flags/default-restore-after-prefetch64.log`.

Follow-up:
- The remaining pattern is stable: no software underruns/late writes, but
  persistent analog residual/lag and high driver CPU. Next work should inspect
  capture transaction error semantics and USB transaction request sizes/status
  rather than more parity knobs.

## 2026-06-17: Treat Zero-Complete Capture ISO Slots As Packetization, Not Transport Failure

Decision:
- Do not use aggregate `captureTransactionErrors/transfer ~= 2.273` as a
  physical-readiness blocker by itself.
- Keep blocking on status failures, short/other-size capture transactions,
  classified ISO-slot mismatches, residual/lag, and CPU.

Reason:
- New capture-detail counters decompose the aggregate "error" bucket. Recent
  detailed runs show:
  - `captureStatusFailures=0`
  - `captureOtherByteCountTransactions=0`
  - `captureShortTransfers=0`
  - `captureTransactionFailures == captureZeroCompleteTransactions`
  - `captureExpectedTransactions + captureZeroCompleteTransactions == 5 *
    captureTransfers`
  - useful capture transactions are exactly `352` bytes.
- This means the stable ratio is mostly empty ISO microframes in a 5-frame
  transfer, not failed USB status.
- The physical product blocker remains real: analog residual/lag and driver
  CPU still fail badly versus mainline.

Alternatives discarded:
- Continue chasing the aggregate counter as a USB transport failure: rejected
  because the decomposed counters show no status, short, or other-size failures
  in recent default/variant runs.
- Ignore capture details entirely: rejected because `lifecycle-preopen` has a
  real measurable mismatch (`classified_transactions != total_iso_slots`) and
  must stay rejected.

Evidence:
- Recent invariant pass:
  `local-analysis/stream-stats/capture-iso-invariants-recent-v3.json`
- Historical iRig sweep:
  `local-analysis/stream-stats/capture-iso-invariants-all-irig-v3.json`
  (`lifecycle-preopen` remains FAIL; older runs without detail are UNKNOWN).
- Tool:
  `scripts/analyze-capture-iso-invariants.py`

## 2026-06-17: Port Mainline-Style Unrolled HAL Output Packing As CPU Candidate

Decision:
- Replace the C++ HAL's default per-byte/modulo `fillPlaybackBytes` loop with a
  mainline-style unrolled 16-byte fast path when `OPENA8DJ_OUTPUT_CHECK_OFFSET`
  is the default `8`.
- Keep the previous generic path for non-default diagnostic check offsets.

Reason:
- Mainline already uses an unrolled group writer for Mode 2 output packing.
- C++ still paid modulo and branch work per byte on every playback transfer.
- The change preserves byte layout for the default check offset and should
  reduce CPU/jitter in the playback hot path. It does not alter USB request
  counts, queue depth, sample time policy, gain, or control-plane state.

Alternatives discarded:
- More queue/prefetch/ISO parity knobs: rejected for now because recent locked
  one-factor runs already failed quality/CPU gates.
- Claim product improvement from offline build/gates: rejected; the candidate
  must still pass locked iRig quality and CPU comparison.

Evidence:
- `make usb-play hal` PASS.
- `make HAL_OUTPUT_CHECK_OFFSET=4 hal` PASS for the generic fallback.
- `make hal` rebuilt the default artifact after fallback verification.
- `scripts/run-cpp-offline-gates` PASS: Debug `16/16`, Release `17/17`.

## 2026-06-17: Reject Unrolled HAL Output Pack As Default

Decision:
- Disable the unrolled HAL output pack path by default.
- Keep it only as an opt-in diagnostic knob via `HAL_UNROLLED_OUTPUT_PACK=1`.

Reason:
- The locked physical run failed far worse than previous candidates:
  `quality_alignment_score=0.131043`, SNR `-18.43 dB`,
  `lag_jumps_gt_2_frames=47`, mid-band residual ratio `8.393129`, high-band
  residual ratio `7.405798`.
- Runtime CPU did not improve enough to justify keeping it: OpenA8DJ driver
  avg/p95/max `30.959%/35.600%/35.900%`.
- Stream stats remained clean for underruns/late writes/timeline resets and
  capture ISO invariants passed, so this result does not support a readiness
  claim or a simple capture-transport explanation.

Alternatives discarded:
- Keep unrolled output pack as default because offline gates passed: rejected
  because physical quality is the governing gate.
- Continue directly to more physical sweeps with this candidate: rejected
  because the soundcheck is a severe regression.

Evidence:
- Safety first run failed on transient high `coreaudiod` CPU, then retry PASS:
  `local-analysis/physical-unrolled-pack/20260616-211513/hal-candidate-safety`
  and
  `local-analysis/physical-unrolled-pack/20260616-211622-retry/hal-candidate-safety`.
- Soundcheck:
  `local-analysis/soundcheck/20260616-unrolled-pack-irig-pairA-16s-cpp-hal`.
- Stream/capture summaries:
  `local-analysis/stream-stats/unrolled-pack-summary.json` and
  `local-analysis/stream-stats/unrolled-pack-capture-iso-invariants.json`.
- Cleanup:
  `local-analysis/audio-stack-guard/20260616-force-unload-unrolled-pack-explicit`
  and
  `local-analysis/runtime-isolation/post-unrolled-pack-failed-unload.json`.

## 2026-06-17: Treat Lag Correction As Insufficient Explanation For Default Residual

Decision:
- Stop treating the default music failure as a simple lag/drift problem.
- Keep lag-jump metrics as blockers, but prioritize signal coloration,
  distortion, wrong mixed signal, and runtime CPU as the next investigation
  targets.

Reason:
- Offline window traces over existing iRig captures show that local lag
  correction barely changes residual for default-like runs:
  - default-after-unrolled: median mid residual `1.448463 -> 1.430920`
    (`1.2%` improvement);
  - hotstats-write-late: `1.452215 -> 1.443643` (`0.6%` improvement);
  - queue8: `1.491993 -> 1.431929` (`4.0%` improvement).
- Corrected median correlation is consistently about `0.969-0.971`, but the
  residual remains far above the product gate.
- Stream stats still show no active underruns, no late writes, no timeline
  resets, and capture ISO invariants pass for the detailed runs.

Alternatives discarded:
- Chase only `lag_jumps_gt_2_frames`: rejected because even after per-window
  lag correction the spectral residual remains too high.
- Treat clean stream counters as product quality: rejected because the analog
  evidence is still failing.

Evidence:
- `local-analysis/soundcheck-window-trace/default-after-unrolled-v2.json`
- `local-analysis/soundcheck-window-trace/hotstats-write-late-v2.json`
- `local-analysis/soundcheck-window-trace/queue8-v2.json`

## 2026-06-17: Require Decorrelated Physical Matrix Before Blaming Crosstalk

Decision:
- Treat music-based 2x2 L/R matrix fits as diagnostic only.
- Add a dedicated decorrelated channel-matrix gate for the next physical
  iRig route check, rather than claiming that the existing music captures prove
  crosstalk or routing leakage.

Reason:
- Existing physical music captures use a stereo source whose L/R channels are
  too correlated for a stable crosstalk estimate: `input_lr_correlation` is
  `0.985848` and the fit condition number is about `140.3`.
- Default-like runs fit an apparent mixed L/R matrix but still leave large
  full-band residual after the linear model:
  - default-after-unrolled residual/predicted `0.303950`;
  - hotstats-write-late residual/predicted `0.306780`;
  - queue8 residual/predicted `0.503410`.
- The rejected unrolled-pack run is essentially unrelated to the reference
  (`global_mono_correlation=-0.182678`, residual/capture `0.999913`), which
  confirms that this tool can classify severe payload regressions but does not
  rescue that candidate.

Alternatives discarded:
- Declare the analog path has crosstalk based on correlated stereo music:
  rejected because the fit is ill-conditioned.
- Continue more music-only soundchecks before a matrix/crosstalk check:
  rejected because the next blocker is whether the physical route or candidate
  emits a wrong mixed signal.

Evidence:
- `scripts/analyze-soundcheck-linear-matrix.py`
- `local-analysis/soundcheck-linear-matrix/recent-failed-physical-music.json`
- `scripts/run-channel-matrix-gate`
- `local-analysis/channel-matrix/offline-prepare-smoke`
- `local-analysis/channel-matrix/20260617T014008Z-pairA-decorrelated-matrix`

## 2026-06-17: Pair A Decorrelated Tone Matrix Passes, Residual Blocker Remains

Decision:
- Treat the Pair A/iRig physical route as not showing gross L/R crosstalk for
  the decorrelated fixture.
- Do not use sample-by-sample linear residual from this fixture as the routing
  verdict; the frequency-domain tone matrix is the appropriate crosstalk gate.
- Keep physical music quality and runtime CPU as release blockers.

Reason:
- The locked physical channel-matrix run captured the generated fixture with
  no clipping and strong expected tones on the expected channels.
- Frequency-domain matrix metrics passed with leakage below the `-45 dB`
  threshold:
  - left-to-right leakage `-59.48 dB`;
  - right-to-left leakage `-49.67 dB`;
  - max wrong-source leakage `-51.27 dB`;
  - expected floor amplitude `0.06577`.
- The sample-domain 2x2 linear fit still rejects the capture because latency,
  phase, and analog filtering leave large residual. That is useful as a
  coloration clue, not as crosstalk proof.

Alternatives discarded:
- Blame Pair A L/R crosstalk for the music failure: rejected by decorrelated
  tone evidence.
- Promote because channel matrix passed: rejected because music quality,
  runtime CPU, and physical Traktor/timecode are still failing/unvalidated.

Evidence:
- Safety load PASS:
  `local-analysis/physical-channel-matrix/20260617-physical-matrix-safety`.
- Physical matrix run:
  `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix`.
- Tone matrix:
  `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/tone-matrix.json`.
- Linear diagnostic:
  `local-analysis/channel-matrix/20260617-irig-pairA-decorrelated-matrix/linear-matrix.json`.
- Cleanup and isolation:
  `local-analysis/audio-stack-guard/20260617-after-channel-matrix-unload` and
  `local-analysis/runtime-isolation/after-channel-matrix-physical-unload.json`.

## 2026-06-17: Add Atomic Stream-Stats Accumulators As CPU Candidate

Decision:
- Add `HAL_STREAM_STATS_ATOMIC_ACCUMULATORS` as an experimental HAL build flag.
- Keep the production/default HAL build on
  `HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=0` after physical validation rejected
  the flag as a default.
- Treat this as a rejected-default runtime CPU experiment; do not claim audio
  readiness or promotion from it.

Reason:
- The previous hot stream stats path updated `_streamStats` under
  `_streamStatsMutex` from the capture and playback isochronous completion
  path.
- The new path uses relaxed atomic counters and min/max/sum/sample timing
  accumulators, then overlays those counters into the existing snapshot outside
  the hot completion updates.
- The change does not modify packet layout, audio payload bytes, routing,
  scheduling policy, sample rate policy, or device topology.

Alternatives discarded:
- Disable hot stats to reduce CPU: rejected because observability is still
  required while quality and transport blockers are unresolved.
- Increase the stats interval as the default optimization: deferred because it
  would reduce evidence density without removing the lock from the sampled hot
  path.
- Touch timeline or USB scheduling at the same time: rejected because the next
  CPU experiment should be isolated enough for a clear physical comparison.

Evidence:
- Both flag paths compile:
  `make HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=0 hal && make hal`.
- Tool/HAL combined builds compile both fallback and default paths:
  `make usb-play hal && make HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=0 usb-play hal && make usb-play hal`.
- Offline gates PASS: Debug `16/16`, Release `17/17`.
- Runtime isolation after offline validation PASS:
  `local-analysis/runtime-isolation/after-atomic-stream-stats-offline.json`.
- Promotion readiness remains FAIL:
  `local-analysis/promotion-readiness-after-atomic-stream-stats.json`.
- Locked physical soundcheck rejected the flag as a default:
  `local-analysis/soundcheck/20260617-atomic-stream-stats-a11012f-irig-pairA-16s-cpp-hal`.
- Physical metrics with the flag enabled:
  - driver CPU median/p95/max `36.6%/37.6%/37.9%`;
  - quality alignment `0.963004`;
  - SNR `10.34 dB`;
  - click outliers `106`;
  - lag jumps `41`;
  - mid/high residual ratios `1.413150/1.367878`.
- Cleanup required a manual minimal unload because `audio-stack-guard`
  health passed and therefore did not enter recovery despite
  `--unload-opena8dj`; final isolation PASS:
  `local-analysis/runtime-isolation/after-atomic-stream-stats-manual-unload.json`.

Follow-up:
- The next CPU work should target real hot-path cost, not stats accounting:
  output/capture queue depth, transfer batching, property polling behavior, or
  a measured scheduling/lifecycle change with no quality regression.

## 2026-06-17: Promote Hot Stream Stats Interval 16 As Partial CPU Improvement

Decision:
- Change the default from `HAL_HOT_STREAM_STATS_INTERVAL=1` to
  `HAL_HOT_STREAM_STATS_INTERVAL=16`.
- Keep `HAL_HOT_STREAM_STATS=1`; do not remove stream observability.
- Treat this as a partial CPU improvement only. It does not clear hardware
  readiness or branch promotion.

Reason:
- This reduces `_streamStatsMutex` accounting frequency in sampled capture and
  playback completion stats without changing audio bytes, routing, USB request
  sizes, transfer coalescing, sample rates, or playback scheduling.
- The locked Pair A/iRig run with interval 16 kept the same broad music-quality
  failure shape as default, but reduced driver CPU p95 from the latest default
  control `38.5%` to `35.7%`.
- It also reduced total watched audio/UI p95 from `57.9%` to `53.5%`, and the
  soundcheck click count was within the script threshold (`5`, threshold `10`).

Alternatives discarded:
- `HAL_FAST_OUTPUT_PREFETCH_CLEAR=1`: rejected by locked physical soundcheck.
  It reduced driver CPU p95 to `36.8%`, but worsened quality alignment to
  `0.954699` and SNR to about `9.53 dB`.
- `HAL_STREAM_STATS_ATOMIC_ACCUMULATORS=1`: already rejected as a default by
  physical evidence.
- Disable hot stats entirely: still rejected because observability is needed
  while physical quality and CPU are unresolved.

Evidence:
- Hot stats interval 16 safety:
  `local-analysis/physical-hotstats-interval16/20260617-a1c8b50/hal-candidate-safety`.
- Hot stats interval 16 soundcheck:
  `local-analysis/soundcheck/20260617-hotstats-interval16-a1c8b50-irig-pairA-16s-cpp-hal`.
- Fast prefetch rejected soundcheck:
  `local-analysis/soundcheck/20260617-fast-prefetch-clear-a1c8b50-irig-pairA-16s-cpp-hal`.
- Final isolation after hotstats16 unload PASS:
  `local-analysis/runtime-isolation/after-hotstats-interval16-manual-unload.json`.
- Tone-response compensation diagnostic:
  `local-analysis/tone-response-compensation/recent-music-runs.json`.

Remaining blockers:
- Physical music quality still fails: quality alignment around `0.964`, SNR
  around `10.28 dB`, lag jumps `41`, and mid/high residual ratios
  `1.429448/1.362535`.
- Driver CPU p95 `35.7%` is improved but still far above the mainline threshold
  `6.5%`.
- Physical Traktor/timecode remains unvalidated.

## 2026-06-17: Tone Response Shape Does Not Explain Music Residual Offline

Decision:
- Do not claim that Pair A music residual is explained by simple physical
  tone-response/EQ coloration.
- Keep a dedicated offline diagnostic script for future response-shape checks:
  `scripts/analyze-tone-response-compensation.py`.

Reason:
- A three-band per-channel response model derived from the passing physical
  decorrelated tone matrix was fit against six existing music captures.
- After applying response shape and re-fitting global gain, SNR moved only
  modestly (`-0.19 dB` to `+1.88 dB`) and the mid-band residual ratio worsened
  by about `4.94` to `5.02` absolute across the runs.
- That means the current tone-response model does not account for the bad
  music residual. The blocker remains either a more complex non-linear/phase
  behavior, format issue not exposed by tone leakage, or external route/capture
  mismatch that needs a better controlled reference.

Evidence:
- `scripts/analyze-tone-response-compensation.py`
- `local-analysis/tone-response-compensation/recent-music-runs.json`

## 2026-06-17: LTI Transfer Analysis Rejects Simple Linear Route Explanation

Decision:
- Add a SciPy/NumPy offline analysis environment and
  `scripts/analyze-lti-transfer-quality.py`.
- Do not pursue simple EQ/linear phase compensation as the next product fix.
- Keep the next quality investigation focused on non-linear/time-varying
  behavior, output format semantics, or a physical reference mismatch that a
  stable linear transfer function cannot model.

Reason:
- The LTI analysis estimates per-channel transfer functions using Welch/CSD,
  reconstructs a predicted capture, and compares scalar-gain residual against
  LTI-predicted residual.
- Across six existing Pair A/iRig music captures, mid/high coherence is very
  low:
  - mid-band mean coherence is about `0.075` to `0.124`;
  - high-band mean coherence is about `0.020` to `0.045`.
- Applying the estimated transfer function does not improve the music gate:
  LTI SNR deltas are negative in every tested run, about `-0.78 dB` to
  `-2.37 dB`, and mid/high residual ratios worsen.
- Therefore the failed music residual is not explained by a stable linear
  channel response between the generated reference and iRig capture.

Evidence:
- Analysis dependencies: `requirements-analysis.txt`.
- Script: `scripts/analyze-lti-transfer-quality.py`.
- Result JSON:
  `local-analysis/lti-transfer-quality/recent-music-runs.json`.

Implication:
- A better physical reference is still needed, but not as a simple EQ
  calibration. The next high-value physical test should isolate whether the
  reference file sent to CoreAudio is actually the signal appearing at the
  Audio 8 DJ analog outs, or whether a format/phase/non-linear effect appears
  before the iRig capture.

## 2026-06-17: Failure-Mode Classifier Rejects Simple Mix/Polarity/Non-Linearity

Decision:
- Add `scripts/analyze-soundcheck-failure-modes.py` as an offline diagnostic
  for existing physical soundcheck WAVs.
- Do not chase simple L/R swap, polarity inversion, static 2x2 matrix, or
  memoryless cubic compensation as the next product fix.

Reason:
- Across recent failing Pair A/iRig music captures, the best identity/swap/
  polarity scalar model still stays near `9-10 dB` SNR.
- A static 2x2 matrix improves SNR by only about `0.13-0.24 dB`.
- A cubic memoryless model improves SNR by only about `0.002-0.004 dB`.
- Capture clipping is absent.
- With local lag search (`--drift-max-lag 128`), default-like runs show
  small net drift but persistent residual, so drift alone does not explain the
  music failure.

Evidence:
- Script: `scripts/analyze-soundcheck-failure-modes.py`.
- Wide-lag diagnostic:
  `local-analysis/soundcheck-failure-modes/recent-music-runs.json`.
- Local-lag diagnostic:
  `local-analysis/soundcheck-failure-modes/recent-music-runs-local128.json`.

Implication:
- The next physical quality test should isolate the real analog reference route
  and runtime discontinuities. A decorrelated or impulse/MLS physical fixture
  with direct reference capture is higher value than more EQ fitting.

## 2026-06-17: Reject Stats-Off As CPU Default

Decision:
- Do not promote `HAL_OUTPUT_WRITE_STATS=0 HAL_HOT_STREAM_STATS=0`.
- Keep `HAL_OUTPUT_WRITE_STATS=1`, `HAL_HOT_STREAM_STATS=1`, and
  `HAL_HOT_STREAM_STATS_INTERVAL=16` as the current default.

Reason:
- Stats-off removes completion/write telemetry, but locked physical evidence
  did not show a CPU win. Driver CPU p95 was `36.8%`, worse than the
  interval-16 run (`35.7%`) and still far above the mainline threshold
  (`6.5%`).
- Music quality did not improve: quality alignment `0.960287`, SNR
  `10.48 dB`, lag jumps `37`, and mid/high residual ratios
  `1.424930/1.362660`.
- Removing observability while the root cause is unresolved is not an
  acceptable default unless it buys a material CPU reduction, which it did not.

Alternatives discarded:
- Promote stats-off because it removes hot-path accounting: rejected by
  measured CPU.
- Keep stats-off for quality: rejected because the failing music signature
  remains.
- Continue with interval 16: accepted as the current partial CPU cleanup
  because it preserves observability and has better measured p95.

Evidence:
- Build under test:
  `make -B hal HAL_OUTPUT_WRITE_STATS=0 HAL_HOT_STREAM_STATS=0`.
- Safety:
  `local-analysis/physical-stats-off/20260617-a1c8b50-stats-off/hal-candidate-safety`.
- Soundcheck:
  `local-analysis/soundcheck/20260617-stats-off-a1c8b50-irig-pairA-16s-cpp-hal`.
- Promotion readiness after run:
  `local-analysis/promotion-readiness-after-stats-off.json`.
- Final isolation:
  `local-analysis/runtime-isolation/final-after-stats-off.json`.

## 2026-06-17: Reject Sparse Output-Cycle Clear

Decision:
- Do not keep or promote the experimental sparse output-cycle clear path.
- Leave the current default output-cycle reset behavior unchanged.

Reason:
- The sparse-clear candidate passed HAL install safety but failed physical
  music quality with the same broad signature as earlier rejected candidates:
  quality alignment `0.963647`, SNR `10.48 dB`, lag jumps `33`, and mid/high
  residual ratios `1.408180/1.364597`.
- Runtime CPU got worse rather than better: OpenA8DJ driver median/p95/max
  `37.15%/38.3%/38.5%`, compared with `34.5%/35.7%/36.3%` for the current
  interval-16 default run. coreaudiod p95 also rose to `14.2%`.
- The experiment adds hot-path branching/complexity without an objective win.

Alternatives discarded:
- Keep the flag disabled for future use: rejected because the first physical
  A/B showed worse CPU and no quality improvement, so carrying dead hot-path
  complexity is not justified.
- Promote as an optimization because it reduces a `memset`: rejected by
  measured end-to-end CPU.

Evidence:
- Build under test:
  `make -B hal HAL_OUTPUT_SPARSE_CYCLE_CLEAR=1`.
- Safety:
  `local-analysis/physical-sparse-cycle-clear/20260617-a1c8b50/hal-candidate-safety`.
- Soundcheck:
  `local-analysis/soundcheck/20260617-sparse-cycle-clear-a1c8b50-irig-pairA-16s-cpp-hal`.
- Promotion readiness after run:
  `local-analysis/promotion-readiness-after-sparse-cycle-clear.json`.
- Final isolation:
  `local-analysis/runtime-isolation/final-after-sparse-cycle-clear.json`.

## 2026-06-17: Add Runtime Discontinuity Correlation Diagnostic

Decision:
- Add `scripts/analyze-runtime-discontinuities.py` as an offline diagnostic
  over existing soundcheck evidence.
- Do not treat CPU spikes, visible stream counter deltas, or observable
  underrun/replay counters as the primary music-quality root cause unless a
  future run shows stronger correlation.

Reason:
- Across four recent Pair A/iRig music runs, windowed lag jumps remain present
  (`p95` about `24.7-30.35` frames), but the diagnostic found no strong
  correlation (`abs(r) >= 0.70`) between residual/lag/SNR windows and CPU or
  stream-stat delta columns after searching temporal offsets from `-5s` to
  `+5s`.
- The same runs still have no capture clipping and still sit around scalar
  SNR `10 dB`, so the blocker is not cleared. The evidence now points more
  strongly at reference-route mismatch, physical path behavior, format/phase
  semantics, or runtime behavior not represented by the existing counters.

Alternatives discarded:
- Keep tuning CPU flags blindly: rejected because several low-risk and
  medium-risk CPU experiments did not move quality and the new diagnostic does
  not show a strong CPU/residual relationship.
- Declare runtime/timeline innocent: rejected. The diagnostic only proves that
  current CPU/stream counters do not explain the music residual; a runtime
  discontinuity not exposed by those counters remains possible.

Evidence:
- Script: `scripts/analyze-runtime-discontinuities.py`.
- Result JSON:
  `local-analysis/runtime-discontinuities/recent-music-runs.json`.

## 2026-06-17: Add Monitor-Free Soundcheck CPU Mode

Decision:
- Add `--no-monitor-stream-stats` to `scripts/run-soundcheck` so CPU A/B runs
  can avoid polling `opena8dj-control stream-stats` during playback.

Reason:
- The normal soundcheck monitor samples stream stats every CPU interval. That
  is valuable for observability, but it can perturb the exact CPU metric being
  measured.
- Readiness runs still need stream evidence. The new mode is diagnostic-only:
  it helps separate driver CPU from monitoring overhead, but cannot prove
  glitch freedom on its own.

Evidence:
- Syntax check:
  `python3 -m py_compile scripts/run-soundcheck`.

## 2026-06-17: Reject Coalesce2 + Pool Cursor Candidate

Decision:
- Do not run physical soundcheck for
  `HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=1`.
- Keep default `HAL_PLAYBACK_COALESCE_TRANSFERS=1` and
  `HAL_TRANSFER_POOL_CURSOR=0`.

Reason:
- The candidate compiled and offline gates passed, but the HAL safety gate
  failed immediately after load. Audio stack health reported coreaudiod
  `86.8%`, mediaremoted `57.5%`, and total watched CPU `145.3%`.
- The same default build passed the same safety gate immediately afterward, so
  the failure is candidate-specific enough to block further physical testing.
- A candidate that cannot pass HAL safety is not a valid path to audiophile
  quality or low CPU, regardless of the expected transaction reduction.

Evidence:
- Candidate build:
  `make -B hal HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=1`.
- Offline gates: Debug `16/16`, Release `17/17`.
- Safety failure:
  `local-analysis/physical-coalesce2-poolcursor/20260617-190a7ed/hal-candidate-safety`.
- Post-failure isolation:
  `local-analysis/runtime-isolation/after-coalesce2-safety-fail-before-unload.json`.
- Default safety confirmation:
  `local-analysis/physical-default-normal-confirm/20260617-190a7ed/hal-candidate-safety`.

## 2026-06-17: Monitor-Free Soundcheck Is Diagnostic But Not Reliable Evidence

Decision:
- Keep `--no-monitor-stream-stats` available as a diagnostic mode, but do not
  use the first monitor-free run as product-quality or CPU-improvement evidence.
- Continue using normal soundcheck with stream-stat evidence for readiness
  decisions.

Reason:
- Default monitor-free soundcheck produced a badly decorrelated capture:
  quality alignment `0.097964`, SNR `-29.18 dB`, estimated channel gains about
  `0.06`, and no stream-stat evidence. That is not comparable to normal
  soundchecks.
- It did not reduce driver CPU: OpenA8DJ driver p95 was `39.0%`, worse than
  the normal-confirm run p95 `36.9%`.
- A normal default soundcheck immediately afterward returned to the known
  failing-but-aligned signature: quality alignment `0.963713`, SNR `10.57 dB`,
  lag jumps `46`, mid/high residual `1.417748/1.364806`.

Evidence:
- Monitor-free run:
  `local-analysis/soundcheck/20260617-default-monitorfree-a1c8b50-irig-pairA-16s-cpp-hal`.
- Normal confirmation:
  `local-analysis/soundcheck/20260617-default-normal-confirm-a1c8b50-irig-pairA-16s-cpp-hal`.
- Final isolation:
  `local-analysis/runtime-isolation/final-after-default-normal-confirm.json`.
- Promotion readiness:
  `local-analysis/promotion-readiness-after-default-normal-confirm.json`.

## 2026-06-17: Reject Coalesce2-Only Candidate

Decision:
- Do not promote `HAL_PLAYBACK_COALESCE_TRANSFERS=2` as a default.
- Keep `HAL_PLAYBACK_COALESCE_TRANSFERS=1`.

Reason:
- The isolated coalesce2 candidate passed HAL safety and did reduce driver CPU
  p95 from the normal default confirmation (`36.9%`) to `28.5%`, which supports
  the hypothesis that transaction frequency is a real CPU cost.
- The quality regression is unacceptable: quality alignment fell to
  `0.898854`, SNR to `5.85 dB`, lag jumps remained `45`, and mid/high residual
  rose to `2.563432/1.666568`.
- The CPU is still far above mainline (`28.5%` vs `6.5%`) even after the
  quality regression, so this is not a useful tradeoff.

Alternatives discarded:
- Keep coalesce2 as a CPU profile: rejected because it damages physical music
  quality and still misses the CPU gate.
- Try coalesce4 immediately: rejected. Coalesce2 already shows the direction
  worsens quality/cadence before reaching target CPU.

Evidence:
- Build:
  `make -B hal HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_TRANSFER_POOL_CURSOR=0`.
- Safety:
  `local-analysis/physical-coalesce2-only/20260617-43773be/hal-candidate-safety`.
- Soundcheck:
  `local-analysis/soundcheck/20260617-coalesce2-only-43773be-irig-pairA-16s-cpp-hal`.
- Final isolation:
  `local-analysis/runtime-isolation/final-after-coalesce2-only.json`.
- Promotion readiness:
  `local-analysis/promotion-readiness-after-coalesce2-only.json`.

## 2026-06-17: Fix Cadence Outlier Threshold For Coalesced Playback

Decision:
- Split cadence outlier expectations between capture and playback transfer
  durations.
- Keep the public `cadenceExpectedTransferTicks` field as the base capture
  transfer period for compatibility.

Reason:
- `ExpectedIsoTransferTicks()` used `kIsoFramesPerTransfer` for every cadence
  outlier. That is correct for capture and default playback, but wrong when
  `HAL_PLAYBACK_COALESCE_TRANSFERS > 1` because playback transfers then span
  more USB microframes.
- The coalesce2-only run showed playback completions at about `580` transfers
  versus default `1825`, with average playback delta around `47060` ticks
  instead of the default `15083` ticks. The old diagnostic threshold did not
  represent playback transfer duration correctly for that experiment.
- This is observability only. It does not change audio bytes, routing, USB
  request counts, transfer scheduling, or HAL behavior.

Evidence:
- Default build:
  `make -B hal`.
- Diagnostic coalesce build:
  `make -B hal HAL_CADENCE_DIAGNOSTIC=1 HAL_PLAYBACK_COALESCE_TRANSFERS=2`.

## 2026-06-17: Instrument Transfer-Pool Fallback Allocations Before More Physical Runs

Decision:
- Add stream-stats counters for capture and playback transfer-pool fallback
  allocations.
- Surface those counters through `opena8dj-control stream-stats`,
  `scripts/run-soundcheck` TSV output, and `scripts/analyze-stream-stats.py`.
- Treat nonzero fallback allocations during streaming as a diagnostic failure
  and CPU/latency blocker.

Reason:
- The HAL default build uses the transfer pool, but the checkout path still
  falls back to `CreateIsoTransfer(...)` if every pool slot is in use.
- Existing physical runs show high driver CPU and bad music quality without
  active underruns, late writes, or timeline resets. If fallback allocations
  are happening under sustained streaming, that is a concrete real-time path
  violation that current evidence could not see.
- This is a low-risk observability change: it does not change USB request
  layout, payload bytes, queue depth, pool selection order, scheduling, sample
  rate, routing, or CoreAudio defaults.

Alternatives discarded:
- Retest the same `056d29b` candidate: rejected because it already failed
  physical quality and CPU gates.
- Re-enable pool cursor or coalescing immediately: rejected because prior
  physical/safety evidence rejected those variants.
- Guess whether pool fallback happens from CPU alone: rejected because the
  stream-stats payload can report it directly.

Evidence:
- `make -B hal build/opena8dj-control`: PASS.
- `python3 -m py_compile scripts/run-soundcheck scripts/analyze-stream-stats.py`:
  PASS.
- Existing-run compatibility check:
  `scripts/analyze-stream-stats.py local-analysis/soundcheck/20260617-hotpath-lock-056d29b-irig-pairA-16s-cpp-hal --json-out local-analysis/stream-stats/hotpath-lock-056d29b-summary-with-pool-fields.json`.
- Runtime isolation after the build:
  `local-analysis/runtime-isolation/post-transfer-pool-instrumentation-build.json`,
  PASS, HAL inactive, lock absent.

## 2026-06-17: Add Transfer-Pool Offline Model As A Promotion Gate

Decision:
- Add a pure C++ transfer-pool model to the offline gates.
- Require the promotion evaluator to pass `offline_transfer_pool_model` before
  any branch-promotion claim can be made.
- Do not count this as physical audio-quality evidence.

Reason:
- Transfer-pool fallback allocation would violate the realtime no-allocation
  policy and could explain CPU/latency symptoms that are invisible to underrun
  counters.
- The model proves that healthy capture/playback queue configurations do not
  need fallback allocation, and that explicit capture/playback leak scenarios
  are rejected.
- This is safer than spending another hardware run on a candidate whose pool
  behavior is not structurally bounded offline.

Alternatives discarded:
- Leave fallback detection only to physical stream-stats: rejected because the
  offline suite should catch impossible pool configurations before hardware.
- Treat CTest PASS as enough: rejected because CTest alone did not express
  fallback-allocation PASS/FAIL semantics for promotion.

Evidence:
- Standalone model: `opena8djcpp_transfer_pool_model`, PASS, `6` rows,
  `0` failures.
- Offline gate summary:
  `local-analysis/cpp-offline/current-offline-gates.json`.
- Transfer-pool evidence:
  `local-analysis/cpp-offline/transfer-pool-model.json`.
- Promotion readiness:
  `local-analysis/promotion-readiness-current.json`, still FAIL because
  physical music quality, runtime CPU, latest physical investigation, and
  physical Traktor/timecode vinyl gates remain unresolved.

## 2026-06-17: Add Aggregate Transfer Ledger Instrumentation

Decision:
- Add an always-preallocated transfer-ledger ring in the HAL USB engine and
  export aggregate counters through stream stats.
- Capture queue/complete and playback queue/complete events with host time,
  first frame number, transfer bytes, completion bytes, in-flight count, pool
  status, and output-read/drop/replay/silence counters.
- Do not add a large IPC dump command yet; use aggregate stream-stats fields
  for the next physical diagnostic run.

Reason:
- Existing physical failures have clean underrun/late-write counters but bad
  music quality and high CPU. We need transfer-level observability to prove
  whether queue/completion gaps, implicit scheduling, active silence, replay,
  drops, or completion imbalance are hiding under aggregate counters.
- A fixed POD ring with relaxed atomic counters is lower risk than logging or
  file I/O in the callback.
- Appending aggregate fields to the existing stream-stats payload preserves
  backward compatibility with older evidence.

Alternatives discarded:
- Log every transfer: rejected because logging in the audio/USB path is itself
  a glitch and CPU risk.
- Export full raw ledger over IPC immediately: deferred because the next run
  first needs aggregate evidence, and the current 4 KiB IPC payload model is
  not designed for large dumps.
- Reuse `_streamStatsMutex` per transfer: rejected because it would add a lock
  to the hot path and contaminate CPU measurements.

Evidence:
- `make -B hal build/opena8dj-control`: PASS.
- `python3 -m py_compile scripts/run-soundcheck scripts/analyze-stream-stats.py`:
  PASS.
- Existing-run compatibility check:
  `local-analysis/stream-stats/hotpath-lock-056d29b-summary-with-ledger-fields.json`.
- Offline gates:
  `scripts/run-cpp-offline-gates`, Debug `17/17`, Release `18/18`, evidence
  schema PASS.
- Runtime isolation:
  `local-analysis/runtime-isolation/final-after-transfer-ledger-instrumentation.json`,
  PASS, HAL inactive, lock absent.
- Promotion readiness:
  `local-analysis/promotion-readiness-current.json`, FAIL, promotion forbidden.

## 2026-06-17: Reject Native 24-Bit Output And Stop Byte-Format Sweeps

Decision:
- Keep the default physical output byte format as Mode 2 big-endian 24-bit
  with `HAL_OUTPUT_START_BYTE=4` and `HAL_OUTPUT_CHECK_OFFSET=8`.
- Reject `HAL_OUTPUT_NATIVE=1` as unsafe for this hardware path.
- Stop spending physical windows on simple byte-order/start-byte sweeps until
  new evidence contradicts the diagnostic capture.

Reason:
- The diagnostic HAL captured written frames, consumed frames, and packed USB
  bytes for a failing Pair A/iRig music run. The internal path was perfect
  against the reference: written/consumed/packed output comparisons aligned at
  score `1.000000`, packed USB had `0` check errors, `0` panic flags, and
  decoded at gain `0.5` with effectively zero residual.
- The physical analog capture from the same run still failed with SNR about
  `10.51 dB`, `40` lag jumps, and mid/high residual ratios about
  `1.428/1.359`.
- The native/little-endian A/B was catastrophic: quality alignment about
  `0.0036`, SNR `-63.94 dB`, quiet noise about `-8.87 dBFS`, and more than
  `520k` clipped capture frames.

Alternatives discarded:
- Promote native/little-endian output as a possible fix: rejected by physical
  clipping and near-zero alignment.
- Continue random start-byte/check-offset sweeps: rejected because the current
  packed output bytes decode perfectly at the established big/start4/check8
  contract and inactive output pairs remain zero.
- Treat the diagnostic internal PASS as readiness: rejected because the iRig
  physical run still fails product-quality thresholds.

Evidence:
- Diagnostic run:
  `local-analysis/soundcheck/20260617-diag-pack-big-start4-irig-pairA-16s-cpp-hal`.
- Output USB analysis:
  `local-analysis/driver-capture-analysis/diag-pack-big-start4-output-packed-usb-auto.txt`
  and pair A-D variants.
- Input USB analysis:
  `local-analysis/driver-capture-analysis/diag-pack-big-start4-input-packed-usb-pairA.txt`
  through `pairD.txt`.
- Native A/B rejection:
  `local-analysis/soundcheck/20260617-native-i24-start4-irig-pairA-16s-cpp-hal`.
- Final cleanup:
  `local-analysis/runtime-isolation/final-after-disable-native-reject.json`,
  PASS, HAL inactive and lock absent.

Next decision pressure:
- The next candidate must investigate device/USB scheduling or physical route
  behavior after packed output bytes, not superficial HAL byte-format changes.

## 2026-06-17: Reject Payload Mutation, Explicit Scheduling, And Fixed OUT As Readiness Paths

Decision:
- Keep queue-to-completion playback payload mutation out of the active fault
  set unless new evidence appears.
- Reject `HAL_EXPLICIT_SCHED=1 HAL_USB_CLOCK_ANCHOR=1 HAL_USB_STABLE_FRAME=1`
  for the current Audio 8 DJ HAL path.
- Reject `HAL_PLAYBACK_CAPTURE_PACED=0` fixed OUT pacing for the current HAL
  path.
- Do not claim that the current C++ default is better than mainline based on
  tone-only evidence.

Reason:
- Payload guard ran about `1600` checks/s during a failing physical music run
  and reported `0` mismatches. The physical quality failure persisted, so the
  buffer handed to IOUSBHost is stable between queue and completion.
- Explicit scheduling made the path much worse: music alignment about
  `0.0255`, SNR about `-33.8 dB`, click outliers `14`, playback completion
  rate about `23/s`, and timeline resets `35`.
- Fixed OUT pacing also made the path much worse: alignment about `-0.154`,
  SNR about `-28.5 dB`, and lag jumps `40`.
- A fresh default 1 kHz physical tone is not enough for promotion:
  `sideband_ratio=0.006623` beats the historical final `0.3.24` floor but not
  the best mainline sideband ratio `0.004942`; strongest sideband `-42.74 dB`
  and `40` click outliers fail the stricter target.

Alternatives discarded:
- Promote explicit scheduling because it uses nonzero first-frame numbers:
  rejected because the physical output rate collapsed and timeline reset.
- Promote fixed OUT because it decouples playback from capture completion
  shape: rejected because physical music became decorrelated and SNR negative.
- Declare tone PASS as product readiness: rejected because tone is only one
  gate, the tone does not beat best mainline, music still fails, CPU still
  fails, and Traktor/timecode physical evidence is absent.

Evidence:
- Payload guard:
  `local-analysis/soundcheck/20260617-payload-guard-bff59cc-irig-pairA-12s-cpp-hal`
  and `local-analysis/stream-stats/payload-guard-bff59cc-summary.json`.
- Explicit scheduling rejection:
  `local-analysis/soundcheck/20260617-explicit-sched-bff59cc-irig-pairA-12s-cpp-hal`
  and `local-analysis/stream-stats/explicit-sched-bff59cc-summary.json`.
- Fixed OUT rejection:
  `local-analysis/soundcheck/20260617-fixed-out-bff59cc-irig-pairA-12s-cpp-hal`
  and `local-analysis/stream-stats/fixed-out-bff59cc-summary.json`.
- Physical tone:
  `local-analysis/physical-tone/20260617-bff59cc-default/tone-1khz-irig-pairA/tone-analysis.txt`.
- Promotion readiness:
  `local-analysis/promotion-readiness-after-bff59cc-default-tone.json`,
  FAIL.

Next decision pressure:
- The next useful quality work should either improve the aligned default music
  path without harming the already decent tone sidebands, or produce a more
  controlled physical reference that resolves the timebase/alignment
  instability seen in music captures.

## 2026-06-17: Add Bounded Transfer-Ledger Export Before More Physical Knob Sweeps

Decision:
- Add an explicit IPC/CLI export for the latest transaction-level transfer
  ledger entries.
- Hook `scripts/run-soundcheck --stream-stats-snapshots` so future physical
  runs save `transfer-ledger-after.tsv`.
- Add an offline analyzer for the saved ledger so transaction evidence has
  PASS/FAIL semantics instead of being raw text only.
- Do not treat this as a product-quality improvement; it is observability for
  the next physical diagnosis.

Reason:
- Existing aggregate stream stats ruled out several broad categories but could
  not align individual queue/complete events with the physical music timebase
  instability.
- The HAL already wrote a bounded preallocated circular ledger in the transfer
  path. Exporting a small stable window on request gives needed transaction
  evidence without per-buffer logging or file I/O.
- Blind scheduling/format sweeps have produced clear regressions. More physical
  testing should capture the missing transaction evidence first.

Alternatives discarded:
- Add logs from the transfer callbacks: rejected because callback logging is
  real-time unsafe and would perturb CPU/jitter.
- Export the entire 4096-entry ledger through the existing IPC frame: rejected
  because the IPC payload is capped at 4096 bytes. The command now returns a
  bounded latest-entry window.
- Keep relying only on aggregate stream stats: rejected because the current
  failure requires event ordering, first-frame numbers, in-flight state, bytes,
  status, and output-read ranges.

Evidence:
- Build:
  `make -B hal build/opena8dj-control`, PASS.
- Help surface:
  `build/opena8dj-control --help 2>&1 | rg -n "transfer-ledger|stream-stats|input-stats"`,
  PASS.
- Offline gates:
  `scripts/run-cpp-offline-gates`, PASS, debug `17/17`, release `18/18`.
- Analyzer smoke:
  `scripts/analyze-transfer-ledger.py local-analysis/transfer-ledger/synthetic-pass.tsv --json-out local-analysis/transfer-ledger/synthetic-pass-analysis.json`,
  PASS.
- Runtime isolation:
  `local-analysis/runtime-isolation/after-transfer-ledger-run-soundcheck-hook.json`,
  PASS.

Next decision pressure:
- Run one locked physical default music soundcheck with the new ledger export
  and analyze `transfer-ledger-after.tsv` with
  `scripts/analyze-transfer-ledger.py` against `stream-stats-during.tsv`,
  captured WAV lag windows, and CPU profile before changing cadence again.

## 2026-06-17: Expand Transfer Ledger Export And Bound Full Dumps

Decision:
- Increase the preallocated HAL transfer-ledger ring from `4,096` to `131,072`
  entries.
- Extend the IPC request with `startSequence` and make
  `opena8dj-control transfer-ledger --all` export all entries available at the
  initial snapshot in bounded IPC chunks.
- Make full transfer-ledger recording diagnostic-only by default:
  product builds use `HAL_TRANSFER_LEDGER=0`, while physical ledger diagnosis
  must build with `HAL_TRANSFER_LEDGER=1`.
- Update `run-soundcheck --stream-stats-snapshots` to save the full bounded
  ledger, and update `scripts/analyze-transfer-ledger.py` to parse the real CLI
  header format, old fixtures, and full-window evidence.

Reason:
- The first physical ledger export was only the final ~29 entries and had
  `overwritten > 0`, so it could not prove event continuity through the music
  window.
- Parser semantics were too aggressive: it treated the title line as a TSV
  header, treated expected capture zero-complete transactions as product
  failures, and summed cumulative output counters across rows.
- A live `--all` dump can race the HAL writer. The CLI now prints only rows up
  to the initial `latestSequence`, so `count`, `latestSequence`, and row count
  remain reproducible.
- The bounded ledger writes roughly `6,400` rows/second and touches multiple
  atomics per transfer. It is necessary for diagnosis but must not contaminate
  final CPU/performance claims.

Alternatives discarded:
- Keep the 4,096-entry ring: rejected because a 12s music run writes about
  `91k` ledger entries and would overwrite most evidence.
- Increase IPC payload size: rejected because chunked export keeps the control
  plane bounded and avoids large stack/socket payloads.
- Treat output active-underrun snapshots after playback tail as product failure:
  rejected because stream-stat deltas during the run showed zero active
  underruns; these remain warnings unless correlated with active playback.
- Leave the full ledger enabled by default for product candidates: rejected
  because performance must be measured without diagnostic write amplification.

Evidence:
- Build: `make -B hal build/opena8dj-control`, PASS.
- Offline gates: `scripts/run-cpp-offline-gates`, PASS after the export/parser
  changes.
- Fixture parser: `core/tests/fixtures/transfer-ledger-full-window.tsv`,
  analyzer PASS.
- Physical bounded ledger run:
  `local-analysis/soundcheck/20260617-bounded-full-ledger-irig-pairA-12s-cpp-hal`,
  product soundcheck FAIL but transfer-ledger analysis PASS.
- Ledger analysis:
  `local-analysis/transfer-ledger/bounded-full-ledger-soundcheck-analysis.json`,
  `91,647` rows, `overwritten=0`, continuous, no playback failed/short
  transactions, no sequence gaps, no first-frame regressions.
- Stream summary:
  `local-analysis/stream-stats/bounded-full-ledger-soundcheck-summary.json`,
  no output underruns, no active underruns, no timeline resets, no playback
  transfer errors, no pool fallback allocations.
- Cleanup:
  `local-analysis/runtime-isolation/after-bounded-full-ledger-soundcheck-unload.json`,
  PASS, HAL inactive, lock absent.

Next decision pressure:
- The quality failure survives clean transaction evidence. Next optimization
  must target post-packed-byte behavior: USB/device scheduling/state, analog
  route/reference mismatch, or a controlled comparison against the mainline
  physical path, not another blind byte-order/start-byte sweep.

## 2026-06-17: Runtime CPU Hotspot Is IOUSBHost Transfer Enqueue Cadence

Decision:
- Do not spend more physical windows on already rejected transport knobs
  (`coalesce2`, `pool cursor`, input-decode active gating, ISO64/q8) unless the
  underlying implementation changes.
- Keep `HAL_TRANSFER_LEDGER=0` for product CPU measurement.
- Treat the current HAL transport as CPU-blocked: the next candidate must
  reduce IOUSBHost/Objective-C hot-path cost without increasing playback
  completion gaps or damaging physical music quality.
- Enable `HAL_STREAM_USAGE=1` and have `audio-wav-play` set output stream usage
  for its selected A/B/C/D pair. This is architecturally correct and slightly
  reduces deferred output-cycle flushing, but it is not a readiness improvement.

Reason:
- Product ledger-off and stream-usage physical runs still fail strict music
  quality and CPU gates. Stream usage only changed
  `quality_alignment_score` from `0.971414` to `0.971648`; both are below
  `0.98`, both keep SNR near `10.5 dB`, and both retain lag jumps.
- `sudo sample` during playback-only showed the hot CPU path is not DSP,
  routing, sample conversion, or transfer ledger. The active thread is
  `org.opena8dj.driver.usb`; dominant stacks are capture and playback
  `IOUSBHostPipe enqueueIORequest...` calls from isochronous completions.
- The sample also shows smaller but real costs in input decode and output
  packing, but previous input-decode active gating and playback coalescing
  physical runs were rejected. Optimizing those by toggling old knobs would
  repeat known bad evidence.

Alternatives discarded:
- Promote stream usage as an improvement: rejected because the quality/CPU
  deltas are marginal and still fail all product gates.
- Use `sudo sample` failure as a blocker: rejected because noninteractive sudo
  is available and produced usable symbol evidence.
- Re-test `HAL_PLAYBACK_COALESCE_TRANSFERS=2`: rejected because it already
  reduced CPU at the cost of physical music quality and is modeled as unsafe.
- Re-enable input-decode active gating: rejected because the prior physical run
  was a severe quality and CPU regression.

Evidence:
- Product ledger-off soundcheck:
  `local-analysis/soundcheck/20260617-product-ledgeroff-irig-pairA-12s-cpp-hal`.
- Stream-usage soundcheck:
  `local-analysis/soundcheck/20260617-streamusage-irig-pairA-12s-cpp-hal`,
  `quality_alignment_score=0.971648`, `analog_snr_db=10.52`,
  `lag_jumps_gt_2_frames=28`, `opena8dj_driver_p95=37.2%`,
  `coreaudiod_p95=35.0%`.
- Playback-only symbol profile:
  `local-analysis/profiling/20260617-sudo-sample-streamusage-playback-only/opena8dj-driver.sample.txt`.
  Key sample counts in the USB queue:
  capture requeue via `queueCaptureTransfer`/`IOConnectCallAsyncMethod`
  dominates, playback requeue is the second large bucket, while
  `fillPlaybackBytes` and `decodeCaptureBytes` are much smaller.
- Promotion readiness:
  `local-analysis/promotion-readiness-after-streamusage-sample.json`,
  result `FAIL`; branch promotion remains forbidden.
- Cleanup:
  `local-analysis/runtime-isolation/after-streamusage-soundcheck.json` and
  `local-analysis/runtime-isolation/after-sudo-sample-playback-only.json`,
  both PASS with HAL inactive and lock absent.

Next decision pressure:
- The credible path is a transport/hot-path redesign: preprepared transfers,
  less Objective-C allocation/weak-block work per completion, less pool
  scanning, or a DriverKit/USBDriverKit transport that can keep cadence fine
  while reducing enqueue overhead. Any candidate must prove unchanged or better
  physical music quality before using a CPU win.

## 2026-06-17: Adopt Mainline ISO64/q8 Geometry And StopIO Stream Shutdown

Decision:
- Set the C++ HAL defaults to the mainline-proven transport geometry:
  `HAL_ISO_FRAMES=64`, `HAL_CAPTURE_QUEUE=8`, `HAL_PLAYBACK_QUEUE=8`, and
  `HAL_OUTPUT_PREFETCH_FRAMES=64`.
- Set `HAL_STOP_ISOC_ON_STOP=1` so StopIO closes the isochronous stream and
  avoids post-playback active underrun accumulation.
- Keep the newly ported completion-handler reuse, strict pool, legacy output
  slot, fast ISO config, and USB queue QoS knobs compiled behind flags but
  disabled by default. The combined variant did not improve CPU and produced
  post-run underrun evidence, so it is not a product default.
- Do not promote. The new default is a measured improvement over the prior
  C++ runtime, but it still does not objectively beat mainline performance and
  it does not pass physical music quality.

Reason:
- Baseline mainline `0.3.135` from the read-only mainline build artifact
  passed safety after a 45 s stabilization wait. Its physical soundcheck failed
  quality in the current iRig route, but it established the current CPU target:
  `opena8dj_driver_p95=6.0%`, `coreaudiod_p95=8.0%`.
- C++ ISO5/queue64 stream-usage candidate measured
  `opena8dj_driver_p95=37.2%`. Matching mainline ISO64/q8 geometry reduced
  C++ driver p95 to `10.6%`, and adding StopIO shutdown reduced it to `9.8%`.
- StopIO shutdown fixed a concrete runtime bug: the ISO64/q8 candidate without
  StopIO shutdown left `streaming=yes` and accumulated about `86k` active
  underrun frames after playback stopped; the updated candidate ends with
  `streaming=no`, `outputUnderruns=0`, and `outputActiveUnderruns=0`.
- The current physical iRig route is not quality-valid for promotion: both
  mainline and C++ ISO64/q8 fail with `quality_alignment_score` around
  `0.68`, SNR around `-0.85 dB`, and many lag jumps. This blocks sound-quality
  claims and raises priority for capture-route/reference validation.

Alternatives discarded:
- Make completion-handler reuse/strict-pool/legacy-slot/QoS/fast-ISO default:
  rejected because the combined run did not reduce driver CPU and left
  post-run active-underrun evidence.
- Claim C++ is better because its quality score was similar to mainline in the
  degraded route: rejected. A degraded shared capture route can only block
  claims; it cannot prove audiophile quality.
- Promote the CPU win over previous C++ default: rejected because the product
  criterion is beating mainline, not merely improving over an earlier C++
  candidate.

Evidence:
- Mainline baseline safety:
  `local-analysis/mainline-baseline/20260617-mainline-0.3.135-wait45/hal-candidate-safety`.
- Mainline baseline soundcheck:
  `local-analysis/mainline-baseline/20260617-mainline-0.3.135-wait45/soundcheck-irig-pairA-12s`,
  `opena8dj_driver_p95=6.0%`, `quality_alignment_score=0.680798`,
  `analog_snr_db=-0.83`.
- C++ ISO64/q8 pre-StopIO soundcheck:
  `local-analysis/soundcheck/20260617-cpp-iso64q8-streamusage-irig-pairA-12s`,
  `opena8dj_driver_p95=10.6%`, final `streaming=yes` with active underruns.
- C++ ISO64/q8 StopIO soundcheck:
  `local-analysis/soundcheck/20260617-cpp-iso64q8-stopisoc-irig-pairA-12s`,
  `opena8dj_driver_p95=9.8%`, final `streaming=no`,
  `outputActiveUnderruns=0`, `quality_alignment_score=0.686712`,
  `analog_snr_db=-0.84`.
- Promotion readiness:
  `local-analysis/promotion-readiness-after-iso64q8-stopisoc.json`,
  result `FAIL`.

Next decision pressure:
- Close the remaining performance gap to mainline without changing USB/audio
  semantics. The current delta is likely residual C++/Obj-C diagnostic and
  stream-stat overhead or HAL-side work not present in mainline.
- Independently validate the physical capture/reference route before making
  any sound-quality claim; the current shared route made mainline and C++
  fail similarly.

## 2026-06-17: Disable Input Decode By Default And Require Explicit DVS Enable

Decision:
- Port the mainline control-plane semantics for `inputDecodeEnabled` into the
  C++ HAL and `opena8dj-control`.
- Default input decode is off for playback/output-only use. Timecode vinyl,
  CD-line, and phono profiles explicitly enable input decode.
- Keep the mode-2 capture fast-skip active when input checks are disabled and
  input decode is not active.
- Do not claim product readiness. The change is a measured CPU improvement,
  not an audio-quality proof.

Reason:
- `sudo -n sample` on the ISO64/q8 StopIO candidate showed residual active CPU
  in `decodeCaptureBytes` / `appendInputByte` during playback-only. That work
  is unnecessary when no input/DVS client has enabled input capture.
- Mainline already uses a safer policy: input decode is disabled by default,
  but profiles/control can enable it for DVS/timecode input use.
- Physical comparable Pair A/iRig dense run after the port measured
  `opena8dj_driver_p95=6.3%`, down from C++ ISO64/q8 StopIO `9.8%` and within
  the configured driver p95 target, but still above the current mainline
  baseline `6.0%`.
- The same run still fails quality:
  `quality_alignment_score=0.680121`, SNR `-0.83 dB`,
  `lag_jumps_gt_2_frames=42`. CoreAudio p95 also remains worse than mainline
  at `43.2%`, dominated by startup spikes but still counted by the current
  gate.

Alternatives discarded:
- Re-enable input decode unconditionally: rejected for playback because it
  adds measured CPU cost and does no useful playback work.
- Use the older compile-time `HAL_INPUT_DECODE_ACTIVE_GATING=1` approach as
  the product fix: rejected because an earlier physical run regressed badly.
  The accepted approach is explicit control-plane enablement, matching
  mainline and preserving DVS/timecode paths.
- Promote because driver p95 now nearly matches mainline: rejected because
  physical music quality, coreaudiod CPU, and physical Traktor/timecode gates
  still fail or are missing.

Evidence:
- Profile:
  `local-analysis/profiling/20260617-iso64-stopisoc-sample/playback-only/opena8dj-driver.sample.txt`.
- Offline gates:
  `local-analysis/cpp-offline/current-offline-gates.json`, PASS.
- Physical soundcheck:
  `local-analysis/soundcheck/20260617-cpp-inputdecode-off-dense-ch12-irig-pairA-12s`,
  FAIL quality but driver p95 `6.3%`.
- Promotion readiness:
  `local-analysis/promotion-readiness-after-inputdecode-off-ch12.json`, FAIL.

## 2026-06-17: Expose But Reject HAL_INPUT_IO=0 Physical Diagnostic

Decision:
- Expose the existing `OPENA8DJ_ENABLE_INPUT_IO` HAL macro as
  `HAL_INPUT_IO`, default `1`, so future builds are explicit about whether HAL
  input callbacks are compiled.
- Reject `HAL_INPUT_IO=0` as a valid physical diagnostic/product variant for
  the current HAL: it fails device enumeration safety.

Reason:
- Performance analysis suggested `coreaudiod` p95 may include input/full-duplex
  graph work even when USB input decode is disabled. A compile-time no-input
  HAL variant was the smallest way to test that hypothesis.
- The variant compiled, but `test-hal-candidate-safety` failed before
  soundcheck with `required_device_missing`. The driver process existed, but
  CoreAudio enumeration did not expose UID `org.opena8dj.Audio8DJ`.
- Because the device surface disappears, the variant cannot produce meaningful
  product CPU, quality, or timecode evidence.

Alternatives discarded:
- Run soundcheck anyway through a missing/unlisted device: rejected by safety.
- Make no-input HAL a product optimization: rejected because Audio 8 DJ must
  expose and support 8 inputs and timecode vinyl.

Evidence:
- Safety failure:
  `local-analysis/physical-inputio-off/20260617-inputio-off/hal-candidate-safety/summary.txt`.
- Cleanup:
  `local-analysis/runtime-isolation/after-inputio-off-safety-fail-unload.json`,
  PASS with HAL inactive and lock absent.

## 2026-06-17: Reject Current Physical Pair A Matrix As Promotion Evidence

Decision:
- Treat the current physical Pair A/iRig channel-matrix evidence as a blocker,
  not as a routing/functionality proof.
- Do not claim C++ has better routing or sound quality than mainline until a
  comparable physical matrix passes and C++ beats or equals mainline leakage
  with a validated capture route.

Reason:
- The same decorrelated Pair A matrix was run against the current C++ product
  HAL and the read-only mainline `0.3.135` artifact.
- Both fail the strict `-45 dB` wrong-source leakage threshold, which means the
  current physical route is not clean enough for promotion.
- C++ is worse than mainline in the same route: max wrong-source leakage is
  `-35.36 dB` for C++ versus `-42.58 dB` for mainline. That delta is not
  explainable as a C++ improvement and cannot be hand-waved as a shared route
  problem.

Alternatives discarded:
- Promote because both drivers fail similarly: rejected because C++ must beat
  mainline, not merely share a degraded measurement class.
- Ignore the matrix because real-music evidence is already failing: rejected
  because routing/channel leakage is a separate functional requirement for
  A/B/C/D decks and Traktor/timecode use.
- Treat linear-matrix diagnostics as a pass: rejected because they only
  classify the residual shape and explicitly report large unexplained physical
  residual.

Evidence:
- C++:
  `local-analysis/channel-matrix/20260617-inputdecode-default-pairA-chmatrix/tone-matrix.json`,
  `result=FAIL`, `max_wrong_source_leakage_db=-35.36`.
- Mainline:
  `local-analysis/channel-matrix/20260617-mainline-pairA-chmatrix/tone-matrix.json`,
  `result=FAIL`, `max_wrong_source_leakage_db=-42.58`.
- Final isolation:
  `local-analysis/runtime-isolation/after-mainline-chmatrix-unload.json`,
  PASS with HAL inactive and lock absent.

## 2026-06-17: Add Stream-Usage-Off Harness Control Before Next Physical A/B

Decision:
- Add a harness-only `--no-stream-usage` switch to `audio-wav-play`.
- Expose it through `scripts/run-channel-matrix-gate --no-output-stream-usage`
  and `scripts/run-soundcheck --no-output-stream-usage`.
- Keep the default behavior unchanged. This is not a driver change and not a
  product-readiness claim.

Reason:
- The C++ harness now asks CoreAudio to enable only the selected output stream
  through `kAudioDevicePropertyIOProcStreamUsage`.
- The read-only mainline `0.3.135` artifact was built with
  `HAL_STREAM_USAGE=0`, so the same C++ harness likely gets the stream-usage
  property rejected when testing mainline.
- That means recent C++ vs mainline physical comparisons may include a
  harness/HAL interaction difference: C++ gets selected-pair stream usage,
  mainline falls back to normal all-stream output-buffer behavior.
- The next locked physical A/B needs to control this factor before treating the
  C++ worse leakage result as a pure driver-routing defect.

Alternatives discarded:
- Disable stream usage by default: rejected because the current C++ product
  default intentionally exposes the property and prior evidence used that
  path.
- Patch mainline tooling or mainline HAL: forbidden for this C++ worktree and
  unnecessary for a controlled harness variable.
- Repeat the same physical matrix blindly: rejected because it would reproduce
  an uncontrolled comparison.

Evidence:
- `make -B build/audio-wav-play`: PASS.
- `bash -n scripts/run-channel-matrix-gate`: PASS.
- `python3 -m py_compile scripts/run-soundcheck`: PASS.
- Prepare-only matrix plan with stream usage disabled:
  `local-analysis/channel-matrix/offline-no-stream-usage-plan-v2/plan.txt`,
  `output_stream_usage=0`.
- Offline gates:
  `local-analysis/cpp-offline/current-offline-gates.json`, PASS.
- Promotion readiness:
  `local-analysis/promotion-readiness-current.json`, FAIL.
- Runtime isolation:
  `local-analysis/runtime-isolation/post-stream-usage-harness-control.json`,
  PASS with HAL inactive and lock absent.

## 2026-06-17: Reject Stream-Usage And Mainline-Config Probes As Readiness Fixes

Decision:
- Do not claim C++ routing, physical sound quality, resource use, or branch
  promotion readiness from the latest physical probes.
- Do not adopt the mainline-config build as a product fix yet. It recovers
  output level, but it does not pass the physical matrix or real-music quality
  gates.
- Treat `IOProcStreamUsage` as a measurement variable that must stay recorded
  in physical evidence. It is not the sole root cause.

Reason:
- Disabling output stream usage in the playback harness improved C++ Pair A
  max wrong-source leakage from `-35.36 dB` to `-39.72 dB`, proving that the
  previous C++ vs mainline comparison included a harness/HAL interaction.
- The improved result still fails the `-45 dB` threshold and remains worse
  than the read-only mainline `0.3.135` evidence at `-42.58 dB`.
- A C++ build aligned with several mainline defaults
  (`HAL_STREAM_USAGE=0`, `HAL_FAST_OUTPUT_PREFETCH_CLEAR=1`,
  `HAL_BACKGROUND_PREOPEN_ON_INIT=1`, hot stats interval `1`) recovered
  mainline-like output amplitude, but still failed the Pair A matrix at
  `-40.57 dB` max wrong-source leakage.
- The same mainline-config C++ build failed real-music quality:
  `quality_alignment_score=0.678827`, SNR `-0.83 dB`, `42` lag jumps, mid
  residual ratio `2.536563`, high residual ratio `1.779982`, with no clipping.
- Stream stats showed no output underruns, active underruns, elastic drops,
  timeline resets, or late writes. The remaining failure is not explained by
  the exposed underrun/reset counters.

Alternatives discarded:
- Promote the mainline-config C++ build because it restores level: rejected
  because level parity is not quality parity, and both leakage and music gates
  still fail.
- Blame only the iRig route: rejected because mainline is still measurably
  better than C++ on the same Pair A matrix route.
- Ignore the stream-usage result: rejected because it changed leakage by
  roughly `4.36 dB` and must be controlled in future A/B evidence.

Evidence:
- `local-analysis/channel-matrix/20260617-cpp-no-streamusage-pairA-chmatrix/tone-matrix.json`
- `local-analysis/channel-matrix/20260617-cpp-mainline-parity-config-pairA-chmatrix/tone-matrix.json`
- `local-analysis/soundcheck/20260617-cpp-mainline-parity-config-dense-ch12-irig-pairA-12s/metrics.json`
- `local-analysis/runtime-isolation/post-parity-soundcheck-unload-final.json`

## 2026-06-17: Reject Direct USB Tools As Readiness Evidence

Decision:
- Do not use the current direct USB playback tools as evidence that C++ is
  better than mainline.
- Keep them as diagnostics only. They are useful for isolating the failure
  below HAL/CoreAudio, but their current behavior is not product-quality output.

Reason:
- `opena8dj-usb-play-plain-gain05` bypassed HAL/CoreAudio publication and
  completed physical playback/capture, but Pair A matrix still failed:
  max wrong-source leakage `-44.78 dB`, right-to-left `-29.97 dB`.
- That run showed severe L/R asymmetry: left expected max `0.09584`, right
  expected max `0.01005`.
- `opena8dj-usb-play` built with current HAL flags was worse:
  max wrong-source leakage `-13.19 dB` with both expected channels near the
  minimum threshold.
- Therefore the remaining defect is not solely a HAL/CoreAudio callback issue,
  and the direct USB tools themselves need a stricter control/audio-param
  model before they can serve as a clean bypass oracle.

Alternatives discarded:
- Treat the `-44.78 dB` direct result as close enough: rejected because the
  threshold is `-45 dB`, R->L leakage is poor, and right-channel level is not
  comparable.
- Prefer direct USB over HAL for readiness: rejected because direct HAL-flags
  output is dramatically worse and the tool writes the stereo WAV to all four
  pairs rather than modeling selected-pair routing.

Evidence:
- `local-analysis/channel-matrix/20260617-direct-usb-gain05-pairA-chmatrix/tone-matrix.json`
- `local-analysis/channel-matrix/20260617-direct-usb-halflags-pairA-chmatrix/tone-matrix.json`
- `local-analysis/runtime-isolation/post-direct-usb-halflags-matrix.json`

## 2026-06-17: Add Selected-Pair Direct USB Diagnostic, Still Reject Direct Path

Decision:
- Keep the selected-pair direct USB playback support because it makes the
  diagnostic more precise.
- Do not treat the selected-pair direct results as readiness evidence.

Reason:
- The previous direct USB tool wrote the stereo WAV to all four output pairs,
  so it was not a clean Pair A routing bypass.
- The tool now accepts `[A|B|C|D|all]` and optional `lead_frames` while
  preserving default `all` behavior.
- Selected Pair A with no lead still failed: max wrong-source leakage
  `-35.28 dB`, R->L `-18.05 dB`, right expected max only `0.00607`.
- Selected Pair A with `8192` lead frames improved L->R to `-46.82 dB`, but
  still failed because right expected max dropped to `0.00366` and R->L was
  only `-16.05 dB`.

Alternatives discarded:
- Use direct selected Pair A as a product route: rejected because right-channel
  level is not valid and the matrix fails hard.
- Assume prebuffer solves direct USB: rejected because it only improved one
  leakage direction while worsening right-channel expected level.

Evidence:
- `src/tools/opena8dj-usb-play.m`
- `local-analysis/channel-matrix/20260617-direct-usb-gain05-pairA-selected-chmatrix/tone-matrix.json`
- `local-analysis/channel-matrix/20260617-direct-usb-gain05-pairA-selected-lead8192-chmatrix/tone-matrix.json`
- `local-analysis/runtime-isolation/post-direct-usb-selected-lead8192-matrix.json`

## 2026-06-17: Keep ISO8/q8 As Default Candidate, Reject ISO10 Promotion

Decision:
- Keep ISO8/q8 as the current C++ default quality candidate.
- Reject ISO64/q8 and ISO10/q8 as product defaults for now.
- Do not declare hardware readiness and do not promote C++ over mainline.

Reason:
- Direct USB and HAL Pair A matrix evidence show that short ISO cadence fixes
  the gross selected-pair leakage seen with the older direct tool.
- ISO64/q8 reduces CPU but fails physical quality badly and is not acceptable
  for an audiophile path.
- ISO10/q8 lowers driver CPU versus ISO8/q8, but the real-music gate is worse:
  mid/high residual ratios `1.514509/1.396638` and `35` lag jumps, versus
  ISO8/q8 residual ratios `1.432051/1.356290` and `29` lag jumps.
- ISO10/q8 still fails the mainline resource gate:
  driver p95 `19.6%`, coreaudiod p95 `84.3%`.
- Pair A matrix passing is necessary but not sufficient. It does not prove
  full A/B/C/D routing, Traktor/timecode functionality, real music quality, or
  resource superiority.

Alternatives discarded:
- Promote ISO10/q8 because it is cheaper than ISO8/q8: rejected because the
  quality gate worsens and CPU is still above mainline.
- Promote ISO8/q8 because Pair A matrix beats the stored mainline matrix:
  rejected because real music and CPU still fail, and physical A/B/C/D plus
  timecode vinyl are unvalidated.
- Return to ISO64/q8 for CPU: rejected because quality degraded far below the
  acceptable matrix threshold.

Evidence:
- `local-analysis/channel-matrix/20260617-direct-iso10-pairA-selected-lead8192-chmatrix/tone-matrix.json`
- `local-analysis/channel-matrix/20260617-direct-iso16-pairA-selected-lead8192-chmatrix/tone-matrix.json`
- `local-analysis/channel-matrix/20260617-cpp-iso10q8-hal-pairA-chmatrix/tone-matrix.json`
- `local-analysis/soundcheck/20260617-cpp-iso8q8-dense-ch12-irig-pairA-12s/metrics.json`
- `local-analysis/soundcheck/20260617-cpp-iso10q8-dense-ch12-irig-pairA-12s/metrics.json`
- `local-analysis/promotion-readiness-after-iso10q8.json`

## 2026-06-17: Keep Playback-Only Input Usage Fix, Reject It As Product Improvement

Decision:
- Keep `HAL_INPUT_DECODE_ACTIVE_GATING=1` by default.
- Keep playback-only `audio-wav-play` input stream usage all-off when stream
  usage selection is enabled.
- Do not treat this as a product quality or performance improvement.
- Do not promote C++ over mainline.

Reason:
- The change makes the control-plane intent real: output-only probes should
  not activate input decode at USB stream start, and the harness should not
  request input streams for playback-only runs.
- Offline gates pass after the change.
- The locked physical soundcheck still fails:
  `quality_alignment_score=0.959187`, SNR `10.14 dB`, mid/high residual
  ratios `1.467121/1.368783`, and `30` lag jumps.
- Compared with the prior ISO8/q8 run, `coreaudiod` p95 improved, but driver
  p95 worsened to `24.2%` and real-music quality worsened.
- Failure analysis classifies the run as
  `timebase_or_alignment_instability`; static L/R mix, polarity, simple
  memoryless nonlinearity, and fixed LTI/EQ correction do not explain it.

Alternatives discarded:
- Promote because `coreaudiod` p95 improved: rejected because driver CPU and
  music quality worsened, and the mainline CPU gate still fails.
- Revert the harness/control-plane fix solely because the physical product
  gate failed: rejected because the fix improves test semantics. It is kept as
  harness correctness, not as readiness evidence.
- Continue optimizing input decode first: rejected for now because the latest
  evidence points at timebase/cadence rather than accidental input work.

Evidence:
- `local-analysis/runtime-isolation/after-input-decode-gating-build.json`
- `local-analysis/physical-product/20260617-inputdecode-gated-playback-usage/hal-candidate-safety/summary.txt`
- `local-analysis/physical-product/20260617-inputdecode-gated-playback-usage-wait8/hal-candidate-safety/summary.txt`
- `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/metrics.json`
- `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/failure-modes.json`
- `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
- `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
- `local-analysis/promotion-readiness-after-inputdecode-gated-usage.json`
- `local-analysis/runtime-isolation/after-inputdecode-gated-wait8-unload.json`

## 2026-06-17: Separate ISO Packetization From Timebase Defect

Decision:
- Improve `scripts/analyze-capture-iso-invariants.py` so it detects `isoN`
  paths, derives slot count from capture details when the path lacks `isoN`,
  and treats a missing final stop/drain transfer as a warning rather than a
  transport failure.
- Add `make hal-cadence-diagnostic` as an explicit diagnostic build profile.
- Keep product-default HAL diagnostics off after verification.

Reason:
- The latest inputdecode-gated run has no capture status failures, no short
  capture transactions, no other-size capture transactions, and expected
  `352`-byte useful capture transactions.
- Its apparent aggregate capture errors are zero-complete ISO slots, plus at
  most one missing transfer at stop. That is not enough to explain the
  real-music residual and lag jumps.
- The real blocker still points to timebase/alignment instability below the
  current coarse counters.
- A dedicated diagnostic build lets the next physical run collect cadence,
  ledger, payload-guard, and amplitude evidence without pretending those flags
  are product-performance flags.

Alternatives discarded:
- Continue using aggregate `captureTransactionErrors` as the primary failure:
  rejected because decomposed counters show zero-complete packetization rather
  than status/short/size failure.
- Enable cadence/ledger diagnostics by default in the product candidate:
  rejected because they add measurement overhead and would contaminate CPU
  comparisons against mainline.

Evidence:
- `local-analysis/soundcheck/20260617-inputdecode-gated-wait8-streamusage-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
- `local-analysis/soundcheck/20260617-cpp-iso8q8-dense-ch12-irig-pairA-12s/capture-iso-invariants.json`
- `local-analysis/soundcheck/20260617-cpp-iso10q8-dense-ch12-irig-pairA-12s/capture-iso-invariants.json`

## 2026-06-17: Cadence Diagnostic Rejects Payload Corruption, Keeps Timebase As Target

Decision:
- Do not pursue byte-format, payload-corruption, or gross underrun fixes as the
  next primary line.
- Keep the next product work focused on USB completion jitter, queue timing,
  and capture-paced scheduling.
- Keep branch promotion forbidden.

Reason:
- Locked diagnostic HAL run failed physical music quality:
  quality `0.958757`, SNR `10.09 dB`, mid/high residual
  `1.447622/1.366173`, quiet mid noise `-35.03 dBFS`, and `27` lag jumps.
- Transfer ledger is continuous with no sequence gaps, no overwrites, playback
  queue/complete delta `0`, max in-flight `8`, and no playback short/error
  rows.
- Payload guard found `0` mismatches across the run.
- Output counters show no active underruns, timeline resets, late writes,
  elastic drops, or elastic replays.
- Capture ISO invariants pass once stop-transfer gap is accounted for.
- Failure analyzers still classify the run as timebase/alignment instability;
  fixed LTI/EQ correction worsens SNR.
- Runtime discontinuity analysis finds no strong correlation, but completion
  outlier deltas have weak correlation with lag jumps. This makes completion
  jitter a testable hypothesis, not proof.

Alternatives discarded:
- Continue trying byte-order/output-packer changes: rejected because payload
  guard and previous offline pack/oracle gates are clean.
- Treat the diagnostic run as a product performance candidate: rejected because
  cadence/ledger/payload-guard flags add overhead and are for measurement only.
- Promote based on fewer lag jumps than the previous run: rejected because
  quality and CPU still fail hard against thresholds and mainline.

Evidence:
- `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/metrics.json`
- `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
- `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/transfer-ledger-analysis.json`
- `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
- `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/failure-modes.json`
- `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
- `local-analysis/soundcheck/20260617-cadence-diagnostic-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
- `local-analysis/runtime-isolation/after-cadence-diagnostic-unload.json`
- `local-analysis/promotion-readiness-after-cadence-diagnostic.json`

## 2026-06-17: Reject Playback-Before-Capture-Requeue As Product Improvement

Decision:
- Do not enable `HAL_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE=1` as the product
  default.
- Keep branch promotion forbidden.
- Continue treating completion/cadence timing as a hypothesis, but require the
  next timing probe to improve real-music quality, lag jumps, and CPU together.

Reason:
- The locked product probe failed physical music quality:
  quality `0.961360`, SNR floor `10.25 dB`, mid/high residual
  `1.425897/1.365001`, quiet mid noise `-35.03 dBFS`, and `28` lag jumps.
- CPU remains above mainline:
  driver p95 `21.8%`, `coreaudiod` p95 `12.2%`.
- Stream stats showed no gross output underruns, timeline resets, or late
  writes, so the audible defect remains below those coarse counters.
- Failure analyzers still classify the run as timebase/alignment instability.
- Fixed LTI/EQ correction worsened SNR, and static L/R/polarity or simple
  memoryless nonlinearity remain insufficient explanations.

Alternatives discarded:
- Promote because CPU improved versus some failed diagnostic/product probes:
  rejected because it still misses mainline CPU by a wide margin and music
  quality still fails hard.
- Treat the absence of lightweight completion outliers as proof of timing fix:
  rejected because actual audio lag jumps remained at `28`.
- Move on to Traktor/timecode physical gates immediately: rejected because the
  Pair A real-music gate still fails and would waste hardware time without a
  credible quality candidate.

Evidence:
- `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/metrics.json`
- `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
- `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
- `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/failure-modes.json`
- `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/runtime-discontinuities.json`
- `local-analysis/soundcheck/20260617-playback-before-capture-requeue-irig-pairA-12s-cpp-hal/lti-transfer-quality.json`
- `local-analysis/runtime-isolation/after-playback-before-capture-requeue-unload.json`
- `local-analysis/promotion-readiness-after-playback-before-capture-requeue.json`

## 2026-06-17: Block Implicit Capture-Paced Lead Greater Than One

Decision:
- Do not run `HAL_CAPTURE_PACED_OUT_LEAD>1` physically on the current implicit
  scheduling path.
- Extend the offline transfer-pool model so it gates both pool fallback safety
  and capture-to-playback transfer-rate safety.

Reason:
- The existing model only checked pool pressure. That allowed `lead64` to pass
  because the pool had enough slots, even though it queued `64x` as many
  playback transfers as capture periods in the implicit model.
- `lead2` and `lead4` show the same defect at smaller scale: playback queue
  ratios `2` and `4`, not the required approximately `1`.
- Changing the effective OUT transfer cadence is not a safe audiophile-quality
  optimization. It can increase CPU and alter output timing before any physical
  quality benefit is plausible.

Alternatives discarded:
- Physically test `HAL_CAPTURE_PACED_OUT_LEAD=2` immediately: rejected because
  the offline model now shows a transport-rate-safety failure.
- Treat no pool fallback as sufficient: rejected because pool health and audio
  cadence are separate safety dimensions.
- Use `lead>1` without explicit frame scheduling: rejected for now because the
  current implicit IOUSBHost path cannot prove a stable 1:1 capture/playback
  cadence.

Evidence:
- `local-analysis/cpp-offline/transfer-pool-model.json`
- `local-analysis/cpp-offline/current-offline-gates.json`

## 2026-06-17: Reject Reused ISO Completion Handlers As Default

Decision:
- Keep `HAL_REUSE_ISOC_COMPLETIONS=0` as the product default.
- Do not use reused completion handlers as a promotion or readiness argument.

Reason:
- The isolated locked probe with `HAL_REUSE_ISOC_COMPLETIONS=1` still failed
  physical music quality:
  quality `0.961164`, SNR floor `9.98 dB`, mid/high residual
  `1.459843/1.377935`, quiet mid noise `-34.84 dBFS`, and `25` lag jumps.
- Runtime CPU still failed mainline:
  driver p95 `22.1%`, `coreaudiod` p95 `15.0%`.
- Transport counters stayed clean at the gross level: no output active
  underruns, timeline resets, late writes, or transfer-pool fallback
  allocations. That means the flag does not address the dominant quality
  failure.

Alternatives discarded:
- Promote reuse because it removes per-transfer block construction: rejected
  by measured CPU and quality.
- Combine reuse with other previously rejected transport flags immediately:
  rejected because this isolated run did not create a positive baseline.
- Treat fewer lag jumps than the prior probe as sufficient: rejected because
  the run still has `25` lag jumps and fails SNR/residual thresholds.

Evidence:
- `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/metrics.json`
- `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
- `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
- `local-analysis/soundcheck/20260617-reuse-isoc-completions-irig-pairA-12s-cpp-hal/failure-modes.json`
- `local-analysis/runtime-isolation/after-reuse-isoc-completions-unload.json`
- `local-analysis/promotion-readiness-after-reuse-isoc-completions.json`

## 2026-06-17: Reject Fast ISO Transfer Config As Default

Decision:
- Keep `HAL_FAST_ISO_TRANSFER_CONFIG=0` as the product default.
- Do not use descriptor-layout reuse as a CPU or readiness claim.

Reason:
- The isolated locked probe with `HAL_FAST_ISO_TRANSFER_CONFIG=1` still failed
  physical music quality:
  quality `0.959397`, SNR floor `10.19 dB`, mid/high residual
  `1.450623/1.368530`, quiet mid noise `-35.05 dBFS`, and `35` lag jumps.
- Runtime CPU still failed mainline and did not improve versus recent probes:
  driver p95 `23.1%`, `coreaudiod` p95 `25.9%`.
- Capture ISO invariants passed with no warnings, and stream stats showed no
  gross underruns or pool fallback allocations. That is useful negative
  evidence: repeated descriptor configuration is not the primary quality or
  CPU blocker.

Alternatives discarded:
- Promote fast ISO transfer config because it preserves packet layout:
  rejected because preserving layout is necessary but not sufficient.
- Combine it with reused completion handlers immediately: rejected because
  both isolated factors failed to produce a product win.
- Treat no ISO invariant warnings as quality evidence: rejected because music
  quality and CPU gates are the product truth.

Evidence:
- `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/metrics.json`
- `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
- `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
- `local-analysis/soundcheck/20260617-fast-iso-transfer-config-irig-pairA-12s-cpp-hal/failure-modes.json`
- `local-analysis/runtime-isolation/after-fast-iso-transfer-config-unload.json`
- `local-analysis/promotion-readiness-after-fast-iso-transfer-config.json`

## 2026-06-17: Treat Degraded Route Captures As Claim-Blocking, Not Readiness Evidence

Decision:
- Do not use the degraded mainline-vs-C++ captures with quality around `0.68`
  and SNR around `-0.83 dB` to prove either product's sound quality.
- Keep C++ hardware readiness blocked, because the current C++ captures still
  fail quality even outside the severely degraded route-family.
- Prioritize independent capture/reference route validation before any new
  audiophile-quality claim.

Reason:
- Offline comparison found a shared degraded route-family:
  mainline wait45, C++ inputdecode-off, and C++ ISO64/q8 StopIO all show
  quality around `0.68`, SNR around `-0.83 dB`, mid residual around `2.53`,
  high residual around `1.78`, and very low mid coherence around `0.02`.
- That family also adds `window_alignment_is_unstable_for_music` and
  `residual_tracks_program_level`, which indicates the captured/reference
  comparison is not valid enough for audiophile claims.
- More recent C++ product probes are in a different failing family:
  quality around `0.96-0.97`, SNR around `10 dB`, mid/high residual around
  `1.4/1.36`, and persistent lag jumps. This is still a product failure.

Alternatives discarded:
- Claim C++ matches mainline because both fail similarly in the degraded
  route: rejected because a broken measurement route cannot prove quality.
- Ignore route validation because recent C++ runs have higher alignment:
  rejected because all physical quality claims depend on a trustworthy capture
  path.
- Continue one-flag CPU probes as the next priority: rejected because reused
  completion handlers and fast ISO config both failed, and quality remains
  blocked.

Evidence:
- `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/metrics-summary.json`
- `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/failure-modes.json`
- `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/lti-transfer-quality.json`
- `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/runtime-discontinuities.json`

## 2026-06-17: Reject Inline Inactive Input Decode Bypass

Decision:
- Keep the original `handleCaptureTransfer` path that calls
  `decodeCaptureBytes` for capture transactions.
- Do not retain the earlier inline bypass for inactive input decode as a
  product optimization.

Reason:
- The isolated locked probe still failed physical music quality:
  quality `0.961965`, SNR floor `10.16 dB`, mid/high residual
  `1.429792/1.358387`, quiet mid noise `-35.03 dBFS`, and `31` lag jumps.
- Runtime CPU still failed mainline:
  driver p95 `22.1%`, `coreaudiod` p95 `41.3%`.
- Capture ISO invariants and stream stats did not show gross transport faults,
  which means the attempted bypass is not the dominant quality or CPU fix.
- The code change was semantically small, but a product default needs measured
  benefit. This run did not produce one.

Alternatives discarded:
- Keep the bypass as harmless cleanup: rejected because hot-path changes should
  not be retained without a measurable win when product quality is failing.
- Use the driver p95 as partial CPU progress: rejected because `coreaudiod`
  regressed badly and both CPU metrics remain above the mainline target.
- Continue one-flag CPU probing immediately: rejected until the capture route
  and timebase/cadence defect have a better falsifiable model.

Evidence:
- `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/metrics.json`
- `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
- `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
- `local-analysis/soundcheck/20260617-inline-inactive-decode-bypass-irig-pairA-12s-cpp-hal/failure-modes.json`
- `local-analysis/runtime-isolation/after-inline-inactive-decode-bypass-unload.json`
- `local-analysis/promotion-readiness-after-inline-inactive-decode-bypass.json`

## 2026-06-17: Reject Output Sample Time Follower

Decision:
- Keep `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=0` as the product default.
- Do not treat small CoreAudio `sampleTime` continuity following as a fix for
  the current physical music failure.

Reason:
- The isolated locked probe with `HAL_OUTPUT_SAMPLE_TIME_FOLLOWER=1` preserved
  payload and transport cadence but still failed physical music quality:
  quality `0.962572`, SNR floor `9.94 dB`, mid/high residual
  `1.458736/1.377276`, quiet mid noise `-34.98 dBFS`, and `28` lag jumps.
- Runtime CPU still failed mainline and regressed versus the prior probe:
  driver p95 `24.7%`, `coreaudiod` p95 `53.0%`.
- Capture ISO invariants passed and stream stats showed no gross underruns,
  timeline resets, late writes, or pool fallback allocations. The defect is not
  explained by the follower's local sample-time continuity correction.

Alternatives discarded:
- Enable follower because failure-mode drift ppm improved: rejected because
  product metrics did not improve and lag span/residual/SNR still fail.
- Keep follower as a harmless timebase guard: rejected because CPU worsened and
  no quality gate improved enough to justify the runtime change.
- Resume explicit scheduling or fixed OUT pacing: already physically rejected
  by much worse quality and transport behavior.

Evidence:
- `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/metrics.json`
- `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
- `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/capture-iso-invariants.json`
- `local-analysis/soundcheck/20260617-output-sample-time-follower-irig-pairA-12s-cpp-hal/failure-modes.json`
- `local-analysis/runtime-isolation/after-output-sample-time-follower-unload.json`
- `local-analysis/promotion-readiness-after-output-sample-time-follower.json`

## 2026-06-17: Local Lag Correction Does Not Explain Current Music Residual

Decision:
- Do not spend more hardware windows on shallow sample-time or per-window lag
  corrections unless a new model predicts a much larger residual reduction.
- Treat independent route validation or deeper USB/device transport-state work
  as the next required evidence.

Reason:
- Offline comparison across seven current-family C++ captures shows that local
  per-window lag correction improves mid-band residual by only about `0-2%`.
- Corrected mid residual medians remain around `1.41-1.48`, still far above
  product gates.
- Window lag jumps persist in every compared run (`22-35`), but aligning each
  window locally still does not recover SNR or remove the residual.
- This means the failure is not simply a slow drift or local offset problem.

Alternatives discarded:
- Keep trying `sampleTime`/lag follower variants: rejected because the
  best-case residual improvement in existing evidence is too small.
- Claim route is definitely bad: rejected because current C++ also has its own
  consistent failing family; route validation is required, not assumed.
- Claim USB/device state is definitely bad: rejected until new observability
  links transport/device state to the residual.

Evidence:
- `local-analysis/timebase-window-comparison/20260617-current-family/summary.json`
- `local-analysis/timebase-window-comparison/20260617-current-family/failure-modes.json`
- `local-analysis/timebase-window-comparison/20260617-current-family/runtime-discontinuities.json`
- `local-analysis/timebase-window-comparison/20260617-current-family/lti-transfer-quality.json`

## 2026-06-17: Track Practical Mainline Music Floor Separately From Strict Audiophile Gate

Decision:
- Keep the strict promotion gate for audiophile/readiness claims.
- Also track current C++ physical music against the practical mainline floor
  documented from the valid mixer REC OUT/iRig route.
- Do not claim C++ sound-quality superiority until it passes both the practical
  floor and the broader promotion requirements.

Reason:
- Read-only mainline docs state the physical route was valid, but a valid-route
  time-warped Pair A music recheck still measured quality `0.962043`, SNR
  `9.97 dB`, mid residual `1.637216`, high residual `1.412494`, and quiet mid
  `-46.16 dBFS`.
- The best current C++ run in the current-family comparison is close to the
  practical floor but still fails high residual:
  streamusage quality `0.971648`, mid residual `1.399655`, high residual
  `1.358543` versus threshold `1.355`, quiet mid `-35.20 dBFS`, `28` lag jumps,
  and no clipping.
- CPU remains an independent hard failure versus mainline, so a near-pass on
  physical music cannot justify promotion.

Alternatives discarded:
- Treat the strict `35 dB` SNR failure alone as proof that every run is equally
  useless: rejected because the historical valid route also measured about
  `10 dB` SNR on music.
- Lower readiness to the practical floor only: rejected because the user goal
  is better-than-mainline, not merely comparable to a noisy historical route.
- Promote the streamusage run because it is close: rejected because it still
  fails high residual and CPU.

Evidence:
- `local-analysis/mainline-practical-floor/20260617-current-cpp-music-family/summary.json`
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-12.md:7-12`
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-12.md:55-59`
- `/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-12.md:133-139`

## 2026-06-17: Treat USB Enqueue Path As Primary CPU Target

Decision:
- Use `physical-run-compare` as a practical quality+CPU comparator for current
  physical runs.
- Treat USB transfer enqueue/IOKit/MIG overhead as the primary measured CPU
  target, not Mode 2 packet packing.
- Add an explicit `playbackTransfersSubmitted` payload field so future
  submitted/completed transfer deltas are observable without relying on the
  cadence-diagnostic-only `playbackQueueAttempts` field.

Reason:
- The steady driver sample for
  `20260617-current-default-steady-sample-v5` shows the active OpenA8DJ driver
  spending most sampled USB-queue time in capture/playback requeue through
  `IOUSBHostPipe enqueueIORequestWithData` and `IOConnectCallAsyncMethod`.
- The same sample only shows secondary time in `fillPlaybackBytes`; optimizing
  packet packing alone cannot plausibly move driver p95 from about `24%` to
  the practical target `<12%`.
- Stream stats from the run showed completed playback transfers but submitted
  transfers reported as `0`, because the old tool mapped submitted to a field
  that is only incremented with cadence diagnostics enabled.

Alternatives discarded:
- Keep optimizing the packer first: rejected by sample evidence.
- Trust `playbackTransfersSubmitted=0` in previous runs: rejected because the
  counter source was not valid for product builds.
- Claim that v4/v5 are close enough for promotion: rejected because every
  compared run fails practical quality+CPU.

Evidence:
- `local-analysis/physical-run-compare/20260617-profile-family.json`
- `local-analysis/profiling/20260617-current-default-steady-sample-v5/opena8dj-driver-steady.sample.txt`
- `local-analysis/profiling/20260617-current-default-steady-sample-v5/soundcheck/stream-stats-summary.json`

## 2026-06-17: Add Output-Only No-Capture ISO As Opt-In Experiment

Decision:
- Add `HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1` as an opt-in transport experiment.
- Keep the product default at `0`.
- Do not physically test or promote the variant until it passes normal offline
  gates and has a bounded locked test plan.

Reason:
- The v5 steady sample shows CPU dominated by ISO transfer requeue and
  IOUSBHost enqueue overhead. Disabling ISO IN during playback-only operation
  is the most direct way to remove the capture half of that overhead.
- `HAL_INPUT_IO=0` broke device enumeration and is rejected; this experiment is
  different because it leaves HAL input streams represented and only suppresses
  USB capture transfers while input decode is inactive.
- When input decode becomes active, the experiment restarts initial capture
  transfers so timecode/input paths are not permanently disabled.

Alternatives discarded:
- Make this default immediately: rejected because fixed OUT pacing was already
  physically rejected, and this mode may repeat that quality failure.
- Remove HAL input streams for playback-only: rejected because `HAL_INPUT_IO=0`
  failed the HAL safety gate.
- Ignore the CPU sample and keep tuning packet packing: rejected because the
  sample does not support packet packing as the primary CPU blocker.

Evidence:
- Default HAL/control build PASS.
- `HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1` HAL/control build PASS.
- Local build restored to default `HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=0`.

## 2026-06-17: Reject Output-Only No-Capture ISO As Product Optimization

Decision:
- Reject `HAL_OUTPUT_ONLY_NO_CAPTURE_ISOC=1` as a product optimization.
- Keep it default-off and diagnostic-only unless a future design proves
  physical quality and total runtime CPU improvement.
- Keep the HAL packaging fix that installs `Contents/Info.plist` with mode
  `0644`; this is independent of the rejected transport experiment.

Reason:
- The first locked physical run with the flag built correctly but no
  write-side fill trigger submitted playback: `playbackTransfersSubmitted=0`,
  `playbackTransfersCompleted=0`, and `outputFramesRead=0` while CoreAudio
  wrote `574464` frames.
- After adding the write-side fill trigger, playback resumed, but physical
  quality remained catastrophically below threshold:
  `quality_alignment_score=0.183990`, SNR floor `-21.45 dB`, mid/high
  residual `17.171794/11.452494`, and `41` lag jumps.
- Driver CPU p95 improved to `8.0%`, but coreaudiod p95 rose to `28.3%` and
  quality failed, so this is not a valid product improvement.

Alternatives discarded:
- Promote no-capture output-only mode for lower driver CPU: rejected because
  physical music quality fails by a large margin.
- Keep queuing capture ISO during playback-only output: retained as default
  because current evidence shows capture completions are part of the practical
  playback clocking model for this HAL path.

Evidence:
- `local-analysis/soundcheck/20260617-output-only-no-capture-optin-irig-pairA-12s-cpp-hal/`
- `local-analysis/soundcheck/20260617-output-only-no-capture-optin-fillfix-irig-pairA-12s-cpp-hal/`
- `local-analysis/physical-run-compare/20260617-output-only-no-capture-optin-fillfix-reject.json`
- `local-analysis/runtime-isolation/after-output-only-no-capture-optin-fillfix-reject.json`

## 2026-06-17: Reject Ignoring HAL Output Sample Time

Decision:
- Add `HAL_IGNORE_OUTPUT_SAMPLE_TIME=1` as an opt-in diagnostic flag only.
- Keep the product default at `0`.
- Reject the flag as a product quality or CPU improvement based on locked
  physical evidence.

Reason:
- The direct USB tool that passed Pair A matrix writes contiguous output chunks
  with `sampleTimeValid=false`, so this was a small falsifiable HAL experiment:
  force the HAL path to write contiguous output into `OutputTimelineRing` instead
  of using CoreAudio `mOutputTime.mSampleTime`.
- Offline gates still passed, and HAL candidate safety passed, but the locked
  physical soundcheck still failed product quality:
  `quality_alignment_score=0.963508`, SNR floor `10.20 dB`, mid/high residual
  `1.440572/1.369361`, quiet mid noise `-35.12 dBFS`, and `32` lag jumps.
- CPU still failed the mainline target: driver p95 `22.6%`, coreaudiod p95
  `44.7%`.
- Failure analysis still classifies the run as
  `timebase_or_alignment_instability`; simple polarity/matrix/LTI corrections
  remain insufficient.

Alternatives discarded:
- Promote contiguous HAL output writes because direct USB abs-deadline passed a
  tone matrix: rejected because the HAL physical music run did not improve
  quality or CPU.
- Treat the absence of underruns/resets as success: rejected because the audible
  quality metrics and lag-jump gate still fail.

Evidence:
- `local-analysis/physical-product/20260617-ignore-output-sample-time/hal-candidate-safety/`
- `local-analysis/soundcheck/20260617-ignore-output-sample-time-irig-pairA-12s-cpp-hal/`
- `local-analysis/physical-run-compare/20260617-ignore-output-sample-time-reject.json`
- `local-analysis/audio-stack-guard/after-ignore-output-sample-time-unload/`
- `local-analysis/runtime-isolation/after-ignore-output-sample-time-unload.json`
- `local-analysis/promotion-readiness-after-ignore-output-sample-time.json`

## 2026-06-17: Reject ISO8 Queue64 Prefetch256 As Product Candidate

Decision:
- Reject the direct-like margin experiment
  `HAL_ISO_FRAMES=8 HAL_PLAYBACK_ISO_FRAMES=8 HAL_CAPTURE_QUEUE=64
  HAL_PLAYBACK_QUEUE=64 HAL_OUTPUT_PREFETCH_FRAMES=256` as a product
  candidate.
- Keep the default HAL at ISO8/q8/prefetch64 until a later candidate improves
  physical music quality and runtime CPU together.

Reason:
- Carver's direct-USB/HAL differential audit showed the direct USB matrix
  winner had more startup/queue margin than HAL ISO8/q8, so this was the next
  falsifiable physical experiment.
- The locked Pair A matrix passed, proving static A routing and channel
  separation are still healthy:
  max wrong-source leakage `-53.079 dB`, left-to-right leakage `-58.221 dB`,
  right-to-left leakage `-51.442 dB`, no clipping.
- The locked Pair A/iRig music run still failed product quality:
  `quality_alignment_score=0.966043`, SNR floor `10.15 dB`, mid/high residual
  `1.442529/1.373910`, quiet mid noise `-34.87 dBFS`, and `25` lag jumps.
- Compared with ISO8/q8, lag jumps improved, but high-band residual and total
  CPU worsened. `build/physical-run-compare` reported driver p95 `23.7%` and
  coreaudiod p95 `86.6%`.
- Stream stats still showed no output active underruns, timeline resets, late
  writes, elastic drops/replays, or transfer-pool fallback allocations. Clean
  transport counters are therefore still insufficient as an audio-quality
  readiness claim.

Alternatives discarded:
- Promote q64/prefetch256 because the channel matrix passed: rejected because
  the music-quality and CPU gates are the product gates for audiophile use.
- Keep increasing queue/prefetch margin blindly: rejected because this variant
  increased resource cost and did not explain or remove the music residual.

Evidence:
- `local-analysis/physical-product/20260617-iso8q64-prefetch256/hal-candidate-safety/`
- `local-analysis/channel-matrix/20260617-iso8q64-prefetch256-pairA-chmatrix/tone-matrix.json`
- `local-analysis/soundcheck/20260617-iso8q64-prefetch256-irig-pairA-12s-cpp-hal/`
- `local-analysis/physical-run-compare/20260617-iso8q64-prefetch256-reject.json`
- `local-analysis/runtime-isolation/after-iso8q64-prefetch256-unload.json`
- `local-analysis/promotion-readiness-after-iso8q64-prefetch256.json`

## 2026-06-17: Keep Direct USB Music Soundcheck Diagnostic-Only

Decision:
- Add `scripts/run-direct-usb-soundcheck` and `make direct-usb-soundcheck` as
  lock-gated diagnostic tooling only.
- Do not use direct USB tone-matrix success as evidence that HAL music quality
  or product readiness is solved.

Reason:
- A same-route Pair A/iRig direct USB music run with
  `build/opena8dj-usb-play-plain-gain05`, selected Pair A, and lead `8192`
  failed much worse than the current HAL music candidates:
  `quality_alignment_score=0.103211`, worst-channel SNR `-24.31 dB`,
  mid/high residual `17.114359/16.212469`, and no clipping.
- The direct tool did complete playback and capture with Audio 8 DJ streaming:
  `frames_written=576000`, `frames_read=567535` after playback, no queue
  failures, and no late frames. Underruns appeared after the source ended while
  the wrapper was still waiting for capture/drain.
- Failure-mode analysis again classifies the run as
  `timebase_or_alignment_instability` with unstable music-window alignment;
  simple polarity, static matrix, memoryless nonlinear, and LTI explanations
  are insufficient.
- Therefore the direct USB selected-pair/lead tone result is useful for static
  routing experiments, but not an audiophile music oracle.

Alternatives discarded:
- Treat direct USB as a fallback product path: rejected because direct music
  quality is currently worse than HAL and fails the same objective gates.
- Use the direct tone matrix as proof of physical route correctness for music:
  rejected because the music run on the same iRig route fails catastrophically.

Evidence:
- `local-analysis/direct-usb-soundcheck/20260617-direct-usb-plain-gain05-lead8192-pairA-12s/`
- `local-analysis/runtime-isolation/after-direct-usb-soundcheck.json`

## 2026-06-17: Formalize Physical Latency As A Promotion Blocker

Decision:
- Add a formal physical latency/alignment gate to
  `scripts/analyze-physical-latency.py` and
  `scripts/evaluate-promotion-readiness.py`.
- Reject all current physical latency evidence for promotion.

Reason:
- Current physical captures can contain signal, but it appears seconds late and
  does not align cleanly to the reference. Representative direct USB Pair A:
  `first_energy_seconds=5.25`, `best_correlation=-0.623648`,
  `aligned_snr_db=-7.78`, `linear_fit_snr_db=-1.74`, and
  `linear_residual_over_capture_rms=0.773905`.
- These values are incompatible with timecode/DVS and audiophile playback even
  if static tone routing or offline packet gates pass.
- Treating this as a gate prevents branch promotion from depending on memory or
  interpretation of old logs.

Alternatives discarded:
- Continue tracking latency as a note in physical investigation only: rejected
  because promotion readiness needs machine-checkable blockers.
- Accept negative correlation as a simple polarity issue: rejected because the
  absolute correlation and linear-fit SNR are also far below thresholds.

Evidence:
- `local-analysis/direct-usb-latency-separation/20260617-direct-usb-pairA-postroll8-3s/physical-latency.json`
- `local-analysis/promotion-readiness/20260617-after-physical-latency-gate.json`

## 2026-06-17: Keep Explicit Isochronous Scheduling Disabled

Decision:
- Keep `HAL_EXPLICIT_SCHED=0` as the default.
- Add `HAL_EXPLICIT_SCHED_FAIL_FALLBACK=1` as diagnostic-only; default remains
  `0`.
- Do not promote explicit scheduling or fallback scheduling as a product path.

Reason:
- Explicit scheduling without fallback fails with queue saturation:
  `queue_failures=2805`, `qfail_last=0xe00002be`, `qfail_other=2805`,
  `qfail_explicit=2805`, `sched_fallbacks=0`, capture RMS `0.000380`, and
  physical music quality `0.041196`.
- The failures are not `too-old`/`too-new`; they are host/queue saturation
  while explicit scheduling remains active.
- Fallback-on-queue-full reduces the failure storm:
  `sched_fallbacks=1`, `queue_failures=135`, `qfail_explicit=1`, and
  `frames_read=153671`.
- Fallback still fails physical quality:
  quality `0.005597`, SNR floor `-52.51 dB`, capture RMS `0.001699`, and
  physical latency FAIL.

Alternatives discarded:
- Enable fallback by default: rejected because it is a recovery observation,
  not a quality or CPU win.
- Continue explicit scheduling lead sweeps before route/timebase proof:
  rejected because current explicit mode cannot produce useful physical output.

Evidence:
- `local-analysis/direct-usb-soundcheck/20260617-explicit-sched-instrumented-pairA-3s/`
- `local-analysis/direct-usb-soundcheck/20260617-explicit-sched-fallback2-instrumented-pairA-3s/`
- `local-analysis/runtime-isolation/20260617-after-explicit-sched-fallback2-instrumented.json`

## 2026-06-17: Stable Marker Latency Is Not Enough For Readiness

Decision:
- Add deterministic marker tooling for latency probes.
- Do not loosen physical latency or music gates based on marker offset
  stability alone.

Reason:
- Direct USB marker capture shows a stable physical delay:
  offset mean `4.646000s`, std `0.001237s` across four bursts after merging
  split capture peaks.
- The direct USB transport counters were clean (`queue_failures=0`,
  `playback_transfers=396`, `frames_written=288000`), and iRig captured strong
  signal without clipping.
- A lead `0` rerun shifted only the small expected internal margin and still
  showed a stable delayed marker: mean `4.900115s`, std `0.001250s`.
- After subtracting the wrapper record pre-roll plus expected lead/startup
  silence, the residual delay remains `3.780667s..4.129448s`.
- Even after selecting the best aligned window, quality remains bad:
  `aligned_snr_db=-2.33`, `linear_fit_snr_db=-0.64`, and
  `linear_residual_over_capture_rms=0.732560`.

Alternatives discarded:
- Treat this as only a constant-latency compensation problem: rejected because
  SNR and residual gates still fail after alignment.
- Use marker success to claim route validity: rejected because the route has
  signal but not audiophile-valid transfer quality.

Evidence:
- `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/marker-peak-summary.json`
- `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8/physical-latency.json`
- `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-lead0/marker-peak-summary.json`
- `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-lead0/physical-latency.json`

## 2026-06-17: Compare Internal USB Diagnostics Before Blaming Route

Decision:
- Before changing audio format, route, or scheduling again, run the same marker
  with USB diagnostics enabled and compare internal consumed/packed buffers to
  external iRig capture.
- Treat C++ written/consumed/packed byte paths as cleared for the marker
  failure until new evidence contradicts this run.

Reason:
- `lead_frames`, `target_latency`, and `startup_silence` are sub-second terms;
  they cannot explain the residual multi-second delay.
- If `opena8dj-output-consumed-f32.raw` or `opena8dj-output-packed-usb.raw`
  already show the marker delayed by seconds, the bug is in our timeline,
  packing, or queue model.
- If internal buffers align near the reference while iRig stays delayed, the
  fault is downstream of the software timeline: USB/device/firmware/DAC,
  analog route, or capture path.
- The diagnostic run produced that exact split:
  written buffer alignment `1.000000`, consumed buffer alignment `1.000000`,
  packed USB decode alignment `1.000000`, USB check errors `0`, but external
  iRig marker mean `4.930875s`, std `0.001348s`, physical latency FAIL.

Alternatives discarded:
- Continue lead sweeps: rejected because lead `0` and lead `8192` both leave a
  residual delay near four seconds after expected offsets.
- Declare capture route broken immediately: rejected because the route has
  strong, stable signal; internal diagnostics can narrow the boundary first.

Evidence:
- `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/driver-diagnostics-analysis.txt`
- `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/driver-packed-usb-analysis.txt`
- `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/marker-peak-summary.json`
- `local-analysis/direct-usb-latency-marker/20260617-default-marker-pairA-6s-postroll8-usbdiag/physical-latency.json`
- `local-analysis/runtime-isolation/20260617-after-usbdiag-marker.json`

## 2026-06-17: Reject Valid Capture-Out Layout As A Latency Fix

Decision:
- Keep `OPENA8DJ_VALID_CAPTURE_OUT_LAYOUT=0` as the default.
- Treat `HAL_VALID_CAPTURE_OUT_LAYOUT=1` as a rejected diagnostic, not a
  product optimization.

Reason:
- The alternate layout preserved the same failure class:
  external marker mean `4.638750s`, std `0.001297s`, and residual after record
  pre-roll plus expected internal lead/startup `3.773417s`.
- Physical latency still failed:
  `first_energy_seconds=4.85`, `best_correlation=0.565271`,
  `aligned_snr_db=-2.99`, `linear_fit_snr_db=-3.28`, residual/capture
  `0.824708`.
- Physical quality also failed:
  `quality_alignment_score=0.960473`, SNR floor `-31.75 dB`, mid/high
  residual `38.609794/40.459687`, and right click outliers `337`.
- Direct USB transport counters stayed clean, so this flag did not expose a
  useful scheduling or packet-transmission win.

Alternatives discarded:
- Promote the alternate layout because it has stable markers: rejected because
  stable delayed markers remain a blocking latency failure.
- Continue changing channel layout before isolating the downstream boundary:
  rejected because USB diagnostic evidence already cleared written, consumed,
  and packed marker bytes for the default path.

Evidence:
- `local-analysis/direct-usb-latency-marker/20260617-valid-capture-layout-marker-pairA-6s-postroll8/summary.txt`
- `local-analysis/direct-usb-latency-marker/20260617-valid-capture-layout-marker-pairA-6s-postroll8/metrics.json`
- `local-analysis/direct-usb-latency-marker/20260617-valid-capture-layout-marker-pairA-6s-postroll8/physical-latency.json`
- `local-analysis/direct-usb-latency-marker/20260617-valid-capture-layout-marker-pairA-6s-postroll8/marker-peak-summary.json`

## 2026-06-17: Playback Profile Is Correct Control-Plane State, Not A Quality Fix

Decision:
- Make playback profile state consistent across the C++ model and tools:
  input mode `1`, ground lifts off, software lock off, input decode off,
  identity input source map.
- Keep this as a control-plane correctness fix.
- Do not treat it as an audio-quality or latency fix.

Reason:
- Before the change, direct USB diagnostics showed playback markers running
  with device control bytes `00:02:03:01:02:01`; the offline playback profile
  contract expected disabled decode and CAIAQ input mode `1`.
- The forced playback-profile run confirmed the new state was applied before
  streaming: `01:02:03:00:02:00`.
- The physical marker still failed:
  `offset_mean_seconds=4.667208`, std `0.001308`, and residual after record
  pre-roll plus expected internal lead/startup `3.807208s`.
- Physical latency still failed:
  `first_energy_seconds=4.9`, `best_correlation=-0.318510`,
  `aligned_snr_db=-4.36`, `linear_fit_snr_db=-9.47`, residual/capture
  `0.947840`.
- Transport stayed clean (`queue_failures=0`), so this is not a queue failure
  fix either.

Alternatives discarded:
- Revert the control-plane fix because it did not improve physical sound:
  rejected because the control contract is still more coherent and better for
  future DVS/timecode modes.
- Promote playback-profile direct USB as a candidate: rejected because all
  physical latency and music-quality gates still fail.

Evidence:
- `local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/play.log`
- `local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/summary.txt`
- `local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/physical-latency.json`
- `local-analysis/direct-usb-latency-marker/20260617-playback-profile-marker-pairA-6s-postroll8/marker-peak-summary.json`

## 2026-06-17: Alt0 Before Alt1 Improves Marker Latency But Does Not Clear Quality

Decision:
- Keep `OPENA8DJ_SELECT_ALT0_BEFORE_ALT1=0` as default.
- Treat alt0-before-alt1 as the leading USB state-reset candidate for the next
  music and A/B tests.
- Do not claim readiness or promote the candidate yet.

Reason:
- With `HAL_SELECT_ALT0_BEFORE_ALT1=1` and forced playback profile, diagnostics
  confirm the candidate was active: `select_alt0_before_alt1=1`.
- Marker latency improved from the previous `4.6..4.9s` family to
  `offset_mean_seconds=0.405589`, std `0.001256`, and first energy `0.65s`.
- Transport counters stayed clean:
  `queue_failures=0`, `playback_transfers=396`, `frames_written=288000`,
  `frames_read=304118`.
- The signal is still not physically valid:
  `best_correlation=0.414578`, `aligned_snr_db=-3.99`,
  `linear_fit_snr_db=-6.76`, residual/capture `0.908807`.
- Soundcheck quality still failed:
  `quality_alignment_score=0.858726`, SNR floor `-14.49 dB`, mid/high
  residual `5.086371/4.926583`.

Alternatives discarded:
- Promote alt0-before-alt1 immediately: rejected because marker latency is only
  one gate and physical quality remains far below threshold.
- Ignore alt0 because quality still fails: rejected because it is the first
  candidate to materially remove the multi-second marker delay and therefore
  narrows the state-reset boundary.

Evidence:
- `local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/play.log`
- `local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/summary.txt`
- `local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/physical-latency.json`
- `local-analysis/direct-usb-latency-marker/20260617-alt0-diagfield-playback-profile-marker-pairA-6s-postroll8/marker-peak-summary.json`

## 2026-06-17: Reject Alt0 Before Alt1 As A Complete Product Fix

Decision:
- Do not promote alt0-before-alt1 to default/product yet.
- Keep it as a partial state-reset improvement for marker latency.
- Continue quality investigation on real music and same-day A/B evidence.

Reason:
- The real-music run confirmed the candidate was active:
  `select_alt0_before_alt1=1` and playback control `01:02:03:00:02:00`.
- Transport counters stayed clean:
  `queue_failures=0`, `playback_transfers=771`, `frames_written=576000`,
  `frames_read=592208`.
- Real music still failed badly:
  `quality_alignment_score=0.103674`, SNR floor `-24.25 dB`, mid/high
  residual `16.213903/15.560684`, no clipping.
- Failure-mode analysis still points to timebase/alignment instability and
  rejects simple LR mix/polarity or memoryless nonlinearity as sufficient.

Alternatives discarded:
- Treat marker latency PASS as product readiness: rejected because music
  quality is the stronger user-facing gate.
- Revert all alt0 work: rejected because it remains the first evidence-backed
  fix for gross marker delay and should inform the eventual state-reset model.

Evidence:
- `local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s/play.log`
- `local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s/summary.txt`
- `local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s/metrics.json`
- `local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s/failure-modes.json`
- `local-analysis/runtime-isolation/20260617-after-alt0-music.json`

## 2026-06-17: Fix Continuous Output Timeline Future-Gap Reset

Decision:
- Change `OutputTimelineWrite` so a continuous write at
  `ring->maxWrittenFrame + 1` does not trigger the future-gap reset solely
  because the producer is more than half a ring ahead of the reader.
- Keep reset behavior for stale writes and discontinuous future writes.
- Do not promote the candidate yet.

Reason:
- USB diagnostics on the failing alt0/playback-profile music run showed a real
  mid-music timeline reset at write frame `162048`.
- That reset produced startup-silence/underrun gaps beginning around served
  frame `145600`, explaining why short packet probes passed while 12-second
  music failed badly.
- After the fix, written, consumed, and packed USB diagnostics are perfect for
  the 12-second music run: alignment `1.000000`, lag `0`, SNR `999.00 dB`,
  USB `check_errors=0`, USB `panic_flags=0`.
- The physical iRig music gate improved materially but still fails:
  quality `0.957628`, SNR floor `9.38 dB`, mid/high residual
  `1.422297/1.413835`.

Alternatives discarded:
- Increase ring size only: rejected as a blunt mitigation. The reset predicate
  itself was wrong for continuous writes.
- Treat internal USB perfection as product readiness: rejected because physical
  capture, CPU, same-day mainline comparison, and Traktor/timecode gates still
  fail or remain unproven.

Evidence:
- `local-analysis/direct-usb-soundcheck/20260617-alt0-playback-profile-music-pairA-12s-usbdiag2/driver-capture-analysis-explicit-usb-12s.txt`
- `local-analysis/direct-usb-soundcheck/20260617-no-continuous-reset-alt0-music-pairA-12s-usbdiag/driver-capture-analysis-explicit-usb-12s.txt`
- `local-analysis/direct-usb-soundcheck/20260617-no-continuous-reset-alt0-music-pairA-12s-usbdiag/metrics.json`
- `local-analysis/direct-usb-soundcheck/20260617-no-continuous-reset-alt0-music-pairA-12s-usbdiag/metrics-timewarp.json`
- `local-analysis/promotion-readiness/20260617-after-continuous-reset-fix.json`

## 2026-06-17: Accept Direct USB Pair A Routing Evidence, Reject Promotion

Decision:
- Accept the decorrelated direct USB fixture as evidence that the current
  direct USB data plane and physical Pair A routing can be clean for one
  controlled fixture.
- Do not treat that as product readiness, sound-quality superiority, or branch
  promotion evidence.
- Keep C++ off `main` and keep C mainline out of `Legacy` until product HAL
  quality, CPU, and timecode gates beat mainline under comparable conditions.

Reason:
- The decorrelated fixture removes the ambiguous alignment problem seen with
  dense/repetitive music.
- Internal diagnostics are perfect for the run: written, consumed, and packed
  USB output all align at `1.000000`, lag `0`, SNR `999.00 dB`, with
  `check_errors=0` and `panic_flags=0`.
- The physical Pair A tone matrix passes with
  `max_wrong_source_leakage_db=-57.447168`, `left_to_right=-61.527228`, and
  `right_to_left=-55.793274`, proving no basic Pair A deck leakage in this
  direct USB diagnostic route.
- The full captured waveform still fails the audiophile gate:
  `quality_alignment_score=0.721193`, SNR floor `-2.96 dB`, mid/high residual
  `2.117458/2.018361`, and quiet mid-band noise `-21.77 dBFS`.
- Promotion readiness remains `FAIL` and `branch_promotion_allowed=false`.

Alternatives discarded:
- Promote because routing passed: rejected because routing/crosstalk is only one
  functional gate and waveform quality still fails badly.
- Blame the C++ packet/data plane for this specific run: rejected because the
  written, consumed, and packed USB buffers match the reference exactly.
- Claim this beats mainline: rejected because the measurement is direct USB
  diagnostic evidence, not a same-day product HAL A/B against mainline with CPU
  and timecode coverage.

Evidence:
- `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/driver-diagnostics-analysis.txt`
- `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/tone-matrix.json`
- `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/metrics.json`
- `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/linear-matrix.json`
- `local-analysis/direct-usb-soundcheck/20260617-decorrelated-no-continuous-reset-alt0-pairA-12s-usbdiag/tone-response-compensation.json`
- `local-analysis/promotion-readiness/20260617-after-decorrelated-direct-usb.json`
- `local-analysis/runtime-isolation/20260617-after-decorrelated-direct-usb.json`

## 2026-06-17: Reject Branch Promotion After Same-Day Mainline A/B

Decision:
- Do not move C++ to `main`.
- Do not move C mainline to `Legacy`.
- Keep C++ as an experimental product-HAL candidate until it beats mainline in
  both physical quality and CPU on comparable same-day evidence.

Reason:
- A same-day A/B was run using copied candidate bundles, not by writing into the
  mainline worktree.
- Both candidates failed absolute audiophile quality gates.
- C++ product HAL did not beat mainline overall:
  quality `0.134709` for C++ versus `0.246599` for mainline.
- C++ product HAL CPU is much worse:
  driver p95 `23.2%` versus mainline `5.6%`, and coreaudiod p95 `20.5%`
  versus mainline `10.3%`.
- C++ improved some secondary metrics on this fixture:
  lower mid/high residual ratios, fewer lag jumps, and slightly better SNR
  floor. These partial wins are useful but insufficient.

Alternatives discarded:
- Promote because C++ has cleaner direct USB internal evidence: rejected because
  product HAL A/B is the relevant comparison and currently fails.
- Promote because C++ improves residual/lag metrics in the A/B: rejected because
  global alignment and CPU are worse and both candidates fail absolute gates.
- Repeat mainline until it gets a worse run: rejected as biased. The accepted
  measurement is the first safety-passing same-route retry after external CPU
  interference was cleared.

Evidence:
- `local-analysis/mainline-ab/20260617-sameday-ab-085735/ab-comparison.json`
- `local-analysis/mainline-ab/20260617-sameday-ab-085735/cpp-soundcheck/metrics.json`
- `local-analysis/mainline-ab/20260617-sameday-ab-085735/cpp-soundcheck/cpu-profile.tsv`
- `local-analysis/mainline-ab/20260617-sameday-ab-085735/mainline-soundcheck/metrics.json`
- `local-analysis/mainline-ab/20260617-sameday-ab-085735/mainline-soundcheck/cpu-profile.tsv`
- `local-analysis/mainline-ab/20260617-sameday-ab-085735/final-runtime-after-force-unload.json`

## 2026-06-17: Add Opt-In Hot-Path Timing And Fix Stream-Stats Denominators

Decision:
- Add `HAL_HOT_PATH_TIMING=1` as an opt-in diagnostic build flag, default off.
- Export `captureTransfersSampled` and `playbackTransfersSampled` alongside
  raw transfer counts.
- Make `scripts/analyze-stream-stats.py` report which denominator it used for
  capture transaction ratios and keep raw-vs-sampled ratios separate.
- Do not treat hot-path timing builds as product-performance evidence.

Reason:
- CPU profiles showed the OpenA8DJ driver spends most time in USB enqueue /
  completion work, but the prior evidence lacked precise callback section
  timing.
- Raw transfer counters and sampled transaction counters were being mixed in
  summaries. That can hide or exaggerate capture transaction behavior.
- The corrected locked run shows the real capture shape: about `1000.35` raw
  capture transfers/s, `62.43` sampled transfers/s, and within sampled
  transfers about `4.36` valid transactions plus `3.64` zero-complete
  transactions per 8-slot transfer, with all `8.0` slots classified.
- The partial capture-paced OUT layout is not automatically a defect: at
  48 kHz and the current bytes-per-packet calculation, forcing all 8 slots
  every millisecond would read output audio far faster than 48 kHz.
- The same run still failed physical quality:
  `quality_alignment_score=0.970666`, SNR `10.78 dB`, `19` lag jumps,
  mid/high residual `1.378008/1.352014`, quiet mid-band noise `-31.35 dBFS`.

Alternatives discarded:
- Use the earlier raw denominator ratio (`0.227` zero-complete per raw
  transfer) as the primary conclusion: rejected because the transaction
  counters are sampled while raw completions are not.
- Enable timing in default/product builds: rejected because it adds extra
  accounting to callback-adjacent paths.
- Claim a performance improvement from timing evidence: rejected because this
  is instrumentation only and the physical quality gate still fails.

Evidence:
- `local-analysis/hot-path-timing/20260617T140410Z-sampled-denom/soundcheck/summary.txt`
- `local-analysis/hot-path-timing/20260617T140410Z-sampled-denom/soundcheck/metrics.json`
- `local-analysis/hot-path-timing/20260617T140410Z-sampled-denom/stream-stats-summary.json`
- `local-analysis/hot-path-timing/20260617T140410Z-sampled-denom/runtime-isolation-final.json`
- `local-analysis/cpp-offline/current-offline-gates.json`

Next implication:
- The current product HAL is still not ready and still does not beat mainline.
- Next transport experiments must improve physical quality and CPU without
  violating the measured output read rate. Already-rejected paths remain
  blocked: output-only, fixed OUT pacing, coalesced playback, ISO64/q8, and
  `VALID_CAPTURE_OUT_LAYOUT=1`.

## 2026-06-17: Gate Capture-Paced Rate Shape Offline Before More Hardware

Decision:
- Extend the offline jitter model with rate-shape rows for capture-paced
  playback candidates.
- Reject any candidate whose average output frame consumption cannot stay close
  to the requested sample rate before it reaches hardware.
- Treat rate-safe transport shape as necessary but not sufficient for product
  readiness.

Reason:
- Corrected hot-path evidence showed sampled capture transfers carry about
  `4.36` valid transactions and `3.64` zero-complete transactions per sampled
  ISO8 transfer.
- That partial layout is compatible with 48 kHz output consumption:
  about `47967.9` frames/s, `-668 ppm`.
- Forcing a full 8 playback slots per millisecond with the same `352` byte
  requests and `32` USB bytes/frame would consume about `88000` frames/s,
  `833333 ppm` too high. That is unsafe before considering noise, CPU, or
  routing.
- A mainline-shaped ISO64/q8 rate shape can be rate-safe, but the exact C++
  ISO64/q8 candidate was already physically rejected, so rate math cannot
  overrule physical quality evidence.

Alternatives discarded:
- Force full ISO8 OUT layouts to avoid partial slots: rejected because the
  offline model proves it over-reads output audio.
- Retry plain ISO64/q8 because it looks rate-safe: rejected because same-day
  C++ physical evidence already showed unacceptable quality.
- Use compile success or rate safety as readiness: rejected because product
  gates require physical quality, CPU, routing, recovery, and timecode evidence.

Evidence:
- `local-analysis/cpp-offline/jitter-model.json`
- `local-analysis/cpp-offline/current-offline-gates.json`

Next implication:
- The next optimization should either preserve the measured output consumption
  near 48 kHz while reducing callback cost, or introduce a new transport model
  that passes this offline gate before any hardware run.

## 2026-06-17: Enforce Rejected HAL Defaults In Static Policy

Decision:
- Extend `opena8djcpp_static_policy_check` so offline gates fail if known
  rejected or diagnostic-only HAL flags become Makefile defaults.
- Surface the rejected-default check count and failure count in
  `current-offline-gates.json`.

Reason:
- The project has accumulated many physical rejections. Repeating one by
  accidentally changing a Makefile default would waste hardware time and could
  produce misleading "new" evidence.
- The protected defaults include CPU-attractive but quality-rejected paths:
  ISO64-like default promotion, playback coalescing, fixed/output-only pacing,
  unrolled output pack, fast prefetch clear, atomic stream stats, reused ISO
  completions, fast ISO config, explicit scheduling, native output format,
  valid-capture OUT layout, and sample-time follower/ignore variants.
- This is a cheap offline guardrail that does not touch hardware, CoreAudio, USB,
  mainline, or Rust.

Alternatives discarded:
- Keep the rejections only in prose: rejected because prose does not fail a
  build before a risky hardware run.
- Forbid the flags entirely: rejected because several remain useful as opt-in
  diagnostics when a locked, documented experiment needs them.

Evidence:
- `local-analysis/cpp-offline/static-policy.json`
- `local-analysis/cpp-offline/current-offline-gates.json`

Next implication:
- Any future candidate that wants to promote a rejected knob must change both
  the evidence and this policy deliberately, with a new reason and fresh
  physical proof.

## 2026-06-17: Separate Fixed Latency From Timebase Instability Offline

Decision:
- Add `scripts/analyze-timebase-family.py` to aggregate existing per-window
  soundcheck traces into drift, lag-jump, local-lag-correction, and residual
  classifications.
- Use this as an analysis gate before changing more transport or CPU knobs.
- Do not reinterpret this as readiness or branch-promotion evidence.

Reason:
- Current C++ physical failures repeatedly show strong quality loss with clean
  packet/stream counters. The missing distinction was whether the loss is fixed
  latency, linear drift, discontinuous lag, or residual after time correction.
- Current-family C++ traces show all `7` runs have lag jumps and residual after
  lag correction, while maximum linear drift is only about `40 ppm`. The
  classifier reports `analysis_result=PASS` but `stability_result=FAIL`.
- The same-day C++ A/B run had a large fixed lag around `-265..-285` frames;
  local lag correction improves median mid-band residual from about `3.05` to
  `0.89`. That explains why raw alignment alone can look worse than the
  locally corrected waveform.
- The same-day mainline A/B run is still worse by this diagnostic: drift around
  `-129 ppm`, `41` lag jumps, and corrected mid residual about `5.63`. The A/B
  family also reports `stability_result=FAIL`, so it is diagnosis evidence, not
  readiness evidence.

Alternatives discarded:
- Treat raw quality alignment as the only diagnosis: rejected because fixed
  latency can dominate raw correlation.
- Treat local lag correction as readiness: rejected because current-family C++
  still has lag jumps/residual after correction, CPU failure, and no physical
  timecode proof.
- Install numerical packages for this pass: unnecessary; the existing pure
  Python window-trace data is enough for the classification.

Evidence:
- `local-analysis/timebase-window-comparison/20260617-current-family/timebase-family.json`
- `local-analysis/mainline-ab/20260617-sameday-ab-085735/timebase-ab.json`
- `local-analysis/mainline-ab/20260617-sameday-ab-085735/cpp-window-trace.json`
- `local-analysis/mainline-ab/20260617-sameday-ab-085735/mainline-window-trace.json`

Next implication:
- The next product change should target stable latency/timebase behavior, not
  just faster USB enqueue code. A candidate must reduce lag jumps and residual
  after local correction while also lowering CPU.

## 2026-06-17: Align Output Flush Timing With Mainline

Decision:
- Add `HAL_FLUSH_OUTPUT_IN_WRITE_MIX`, default `0`.
- Keep output-cycle flush at `EndIOOperation` by default, matching mainline.
- Keep the previous early `WriteMix` flush behavior as an explicit diagnostic
  flag only.
- Add the default to `static_policy_check` so accidental promotion of the early
  flush fails offline gates.

Reason:
- The timebase audit found that C++ and mainline both consume CoreAudio
  `mOutputTime.mSampleTime`, both default `HAL_USB_ZERO_TIMESTAMP=0`, and both
  use the same `8192/4096/8192` output latency constants.
- A real behavioral difference remained: C++ flushed output inside `WriteMix`
  after expected streams arrived, while mainline waited until `EndIOOperation`.
  That can shift when data enters the USB timeline within a cycle without
  changing packet bytes or routing.
- Aligning with mainline is a narrower, lower-risk timing hypothesis than
  retrying already rejected sample-time follower, ignore-sample-time, explicit
  scheduling, coalescing, or ISO64/q8 paths.

Alternatives discarded:
- Leave early flush as product default: rejected because it is a C++-specific
  timing difference with no evidence that it improves quality.
- Remove the early flush code entirely: rejected because it remains useful for
  a controlled A/B if future evidence points back to it.
- Claim improvement from compilation: rejected; this requires physical
  real-music and CPU evidence.

Evidence:
- `make -B hal`
- `./build/cpp-release/opena8djcpp_static_policy_check`

Next implication:
- Before promotion or readiness claims, run a locked physical same-route A/B
  against the previous C++ baseline and mainline. Required wins: fewer lag
  jumps, lower residual after local correction, strict music thresholds, and
  CPU no worse than mainline.

## 2026-06-17: Reject Output Flush Alignment As Standalone Fix

Decision:
- Do not treat `HAL_FLUSH_OUTPUT_IN_WRITE_MIX=0` as a quality or performance
  fix.
- Keep promotion forbidden.
- Keep the mainline-aligned default as a cleaner timing baseline, but require
  a new hypothesis for the remaining quality and CPU failures.

Reason:
- Locked Pair A/iRig physical evidence still fails strict music quality:
  quality `0.962241`, SNR `10.29 dB`, `23` lag jumps, mid/high residual
  `1.407975/1.362266`, quiet mid `-35.17 dBFS`.
- Window trace still shows discontinuous/local timing behavior:
  local lag range `-22..5` frames and corrected mid residual median `1.413201`.
- CPU remains far above mainline target:
  OpenA8DJ driver p95 about `22.4%`; `coreaudiod` p95 includes high startup
  samples and the run still fails the CPU gate.
- Post-run cleanup succeeded with the HAL unloaded and audio stack healthy.

Alternatives discarded:
- Claim partial win from fewer lag jumps: rejected because strict quality, SNR,
  residual, and CPU still fail.
- Promote to a hardware-ready candidate: rejected because promotion readiness
  remains `FAIL` and physical timecode/Traktor is unproven.
- Immediately move C++ to `main`: rejected; C++ has not objectively beaten
  mainline.

Evidence:
- `local-analysis/physical-product/20260617-output-flush-mainline/hal-candidate-safety`
- `local-analysis/soundcheck/20260617-output-flush-mainline-irig-pairA-12s-cpp-hal`
- `local-analysis/soundcheck/20260617-output-flush-mainline-irig-pairA-12s-cpp-hal/window-trace.json`
- `local-analysis/soundcheck/20260617-output-flush-mainline-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
- `local-analysis/promotion-readiness-after-output-flush-mainline.json`
- `local-analysis/audio-stack-guard/after-output-flush-mainline-force-unload`

Next implication:
- The next candidate must address the persistent post-timeline physical
  residual/lag issue and the high per-transfer CPU cost together. More
  sample-time-only or flush-order-only work is not enough.
## 2026-06-17: Add Transport Cadence Evidence Matrix

Decision:
- Add `scripts/summarize-transport-cadence.py` as an offline evidence tool.
- Treat transport family (`inferred_iso_frames` plus playback queue target) as a required physical-run comparison dimension.
- Do not compare C++ candidates against mainline or against each other without naming the effective ISO/queue family when evidence artifacts expose it.

Reason:
- Existing soundcheck rows mix materially different transport cadences. Raw quality metrics alone hid the fact that the current search space has a clear tradeoff: lower ISO cadence tends to improve physical music alignment but burns CPU, while large ISO cadence reduces CPU and can destroy physical music quality.
- Current artifact summary from `local-analysis/transport-cadence/current.json`:
  - `ISO5/q64`: best quality `0.978050`, median driver CPU p95 about `36.9%`, best historical quality family but not performance viable.
  - `ISO8/q8`: best quality `0.964724`, median driver CPU p95 about `22.4%`, still fails quality and CPU.
  - `ISO10/q8`: quality `0.969379`, driver CPU p95 `19.6%`, still fails quality and CPU.
  - `ISO64/q8`: median quality about `0.678356`, minimum driver CPU p95 `6.0%`, CPU-near-mainline but physically rejected for music quality.
- This makes the next useful physical probes intermediate cadence tests only if they are judged on both quality and CPU. A CPU-only win or quality-only win is not promotable.

Alternatives discarded:
- Optimize from `quality_alignment_score` alone: rejected because it would favor `ISO5/q64`, which fails CPU badly.
- Optimize from CPU alone: rejected because it would favor `ISO64/q8`, which fails physical music quality badly.
- Continue using latest-run-only comparisons: rejected because recent runs can be different transport families and therefore not comparable without metadata.

Evidence:
- `scripts/summarize-transport-cadence.py --json-out local-analysis/transport-cadence/current.json --csv-out local-analysis/transport-cadence/current.csv`
- `local-analysis/transport-cadence/current.json`
- `local-analysis/transport-cadence/current.csv`

Next implication:
- The next candidate should test a bounded intermediate cadence such as `ISO12/q8` only under lock, same fixture, same iRig route, same metrics, and must beat both prior C++ quality and mainline-relative CPU gates before any readiness claim.

## 2026-06-17: Reject ISO12/q8 Intermediate Cadence

Decision:
- Reject `HAL_ISO_FRAMES=12 HAL_PLAYBACK_ISO_FRAMES=12 HAL_CAPTURE_QUEUE=8
  HAL_PLAYBACK_QUEUE=8` as a product candidate.
- Do not continue the simple "increase ISO until CPU passes" ladder without a
  new quality mechanism; ISO12 lowered driver CPU versus ISO8/ISO10 but made
  residual and lag stability worse.

Reason:
- Locked Pair A/iRig soundcheck failed strict physical music gates:
  quality `0.963395`, SNR floor `9.68 dB`, `32` lag jumps, mid/high residual
  `1.653871/1.494546`, quiet mid noise `-34.53 dBFS`, clipping `0`.
- Window trace confirmed the quality problem survives local lag correction:
  corrected mid residual median `1.660459`, local lag range `-34..5` frames,
  local lag p95 `26` frames.
- Runtime improved but still failed the mainline-relative CPU gate:
  OpenA8DJ driver p95 about `16.55%`, above the `<= 6.5%` target.
- Stream stats showed transport cadence behaved as the candidate requested:
  capture/playback about `666.81` transfers/s, classified `12`
  transactions per sampled capture transfer, output read rate about
  `48003.69` frames/s, no output underruns/resets/panic flags.

Alternatives discarded:
- Promote ISO12 because CPU improved: rejected because physical music quality
  and residual are worse than ISO8/ISO10 and far from strict gates.
- Keep sweeping upward blindly: rejected because ISO64/q8 already shows the
  high-ISO endpoint can hit CPU-near-mainline while destroying physical music
  quality.

Evidence:
- `local-analysis/physical-product/20260617-iso12q8/hal-candidate-safety`
- `local-analysis/soundcheck/20260617-iso12q8-irig-pairA-12s-cpp-hal`
- `local-analysis/soundcheck/20260617-iso12q8-irig-pairA-12s-cpp-hal/window-trace.json`
- `local-analysis/soundcheck/20260617-iso12q8-irig-pairA-12s-cpp-hal/stream-stats-summary.json`
- `local-analysis/promotion-readiness-after-iso12q8.json`
- `local-analysis/audio-stack-guard/after-iso12q8-force-unload`

Next implication:
- The blocker is not solved by transport cadence alone. The next useful work
  should explain the persistent analog residual/lag mechanism, or split the
  route/capture baseline from driver behavior before more physical probes.

## 2026-06-17: Add Physical Product Evidence Summary Gate

Decision:
- Add `scripts/summarize-physical-product-evidence.py` as an offline product
  evidence summary tool.
- Keep same-session C++ vs mainline comparisons separate from best-global C++
  physical runs.
- Treat fixture degradation as a blocker for audiophile claims even when one
  candidate partially beats another on residual or lag.

Reason:
- Existing evidence contains two different questions:
  - best C++ global physical behavior so far, currently ISO10/q8 with quality
    `0.969379` and driver CPU p95 `19.6%`;
  - same-session C++ vs mainline comparison, where both candidates were in a
    degraded fixture and C++ still failed quality and CPU versus mainline.
- Mixing those questions can produce false claims. A global C++ run must not
  be compared as if it were the same physical session as a mainline run.
- Russell's residual analysis and the offline failure-mode run showed that
  static L/R matrix or simple nonlinear gain corrections do not explain the
  residual; the decision logic needs to preserve route/cadence/timebase
  context.

Alternatives discarded:
- Compare latest C++ against latest mainline regardless of session: rejected
  because fixture state can dominate physical metrics.
- Use quality alone: rejected because the best quality family still fails CPU.
- Use CPU alone: rejected because CPU-near families have been physically
  rejected for music quality.

Evidence:
- `local-analysis/analog-residual/20260617-key-runs-failure-modes.json`
- `local-analysis/physical-product/20260617-product-evidence-summary.json`

Next implication:
- Promotion remains forbidden. The next useful candidate must either validate
  the fixture with a healthy same-session reference or reduce HAL CPU while
  preserving the best current physical quality family.

## 2026-06-17: Reject Raw Reused Isoc Completion Handlers As Default

Decision:
- Add `HAL_RAW_ISOC_COMPLETIONS` as an explicit experimental build knob, but
  keep the product default at `0`.
- Keep `HAL_REUSE_ISOC_COMPLETIONS=0` as the product default.

Reason:
- The hot driver sample showed real cost in per-transfer Objective-C block and
  weak-reference machinery around `IOUSBHost` isochronous completions.
- The raw/reused completion experiment compiled and reduced driver CPU
  slightly versus the current default family, but it did not come close to the
  mainline CPU gate and did not fix physical music quality.
- The raw completion path is a higher-lifetime-risk implementation style, so
  it needs objective product wins before promotion.

Alternatives discarded:
- Promote raw completions for the small CPU reduction: rejected because quality
  still fails, CPU still fails, and the lifetime model is riskier than weak
  completions.

Evidence:
- Build:
  `make -B hal HAL_REUSE_ISOC_COMPLETIONS=1 HAL_RAW_ISOC_COMPLETIONS=1`.
- Physical soundcheck:
  `local-analysis/soundcheck/20260617-raw-reuse-completions-irig-pairA-20s`.
- Metrics:
  quality alignment `0.973571`, SNR `10.53 dB`, lag jumps `57`,
  mid/high residual `1.401298/1.352559`, OpenA8DJ driver steady CPU about
  `21-22%`.
- Cleanup:
  `local-analysis/audio-stack-guard/force-unload-after-raw-reuse-check`
  reported HAL unloaded, no OpenA8DJ driver pids, and audio stack PASS.

## 2026-06-17: Reject Fractional Time-Warp As Current C++ Quality Explanation

Decision:
- Add `scripts/analyze-fractional-time-warp.py` as an offline diagnostic gate.
- Reject fractional delay/time-warp correction as the dominant explanation for
  the current best C++ physical residual.

Reason:
- Arendt's read-only review identified a plausible remaining hypothesis:
  physical captures might be failing because the analyzer only corrects integer
  lag while the route has fractional delay, phase rotation, or windowed
  time-warp.
- The new analyzer applies a smoothed fractional delay curve to existing WAVs
  and remeasures scalar and stereo-matrix SNR. On the relevant current C++
  candidates, the improvement is only about `0.35-0.93 dB`, below the `3 dB`
  threshold for even a partial explanation.
- The existing LTI transfer analyzer also fails to improve current C++ captures
  and reports very low mid/high coherence, so a static phase/EQ correction is
  not sufficient either.

Alternatives discarded:
- Continue tuning capture/reference alignment as the main route to readiness:
  rejected because the best C++ captures do not materially improve after
  fractional time-warp.
- Treat the degraded same-session A/B warp improvement as a product signal:
  rejected because the delay scores are very low and both candidates in that
  session are already marked fixture-degraded.

Evidence:
- `local-analysis/offline-diagnostics/20260617-fractional-time-warp-multi.json`
- `local-analysis/offline-diagnostics/20260617-lti-transfer-multi.json`
- `local-analysis/offline-diagnostics/20260617-failure-modes-multi.json`

Next implication:
- The next candidate must attack HAL USB enqueue CPU and physical driver
  behavior directly, or establish a healthier same-session physical reference.
  Do not spend more hardware windows on alignment-only fixes unless new
  evidence changes this classification.

## 2026-06-17: Add Transport Budget Gate

Decision:
- Add `tools/transport_budget_model.cpp` and include it in CTest and
  `scripts/run-cpp-offline-gates`.
- Treat the gate as a negative frontier diagnostic: PASS means the known
  observed families are classified correctly and no observed family is a
  product candidate.

Reason:
- Existing physical evidence shows a hard tradeoff:
  - lower ISO cadence can approach the quality threshold but burns driver CPU;
  - high ISO cadence approaches mainline CPU but destroys physical quality.
- The project needs a compiled gate that prevents CPU-only or quality-only
  families from being mistaken for readiness.
- The model uses the current objective thresholds: quality alignment `>=0.98`,
  driver CPU p95 `<=6.5%`, and `0` lag jumps.

Alternatives discarded:
- Keep this as prose in docs only: rejected because future candidates need a
  repeatable CTest artifact.
- Use transport cadence alone as a candidate selector: rejected because cadence
  predicts enqueue pressure but not quality; the gate must include physical
  quality and lag evidence.

Evidence:
- `local-analysis/cpp-offline/transport-budget-model.json`
- Current output:
  `product_candidate_exists=false`, `quality_passing_families=0`,
  `driver_cpu_passing_families=1`, best quality `0.978050`, lowest median
  driver CPU p95 `6.3%`, and lowest estimated enqueue calls/s `31.25`.

Next implication:
- A future physical candidate must either move the frontier with a new
  mechanism, or it should be rejected offline before lock/hardware use. The
  current observed family set still does not prove better-than-mainline
  quality or performance.

## 2026-06-17: Add Prepared DriverKit Transport Contract

Decision:
- Add `tools/driverkit_prepared_transport_contract.cpp` and wire it into CTest
  plus `scripts/run-cpp-offline-gates`.
- Treat the next CPU candidate as a prepared transport/backend problem, not as
  another HAL cadence knob.

Reason:
- Profiling already identified transfer enqueue/requeue work as the measured
  driver CPU hotspot, while the transport budget gate shows simple cadence
  changes trade CPU against physical quality instead of satisfying both.
- A DriverKit/USB transport direction is only credible if the HAL steady-state
  hot path stops issuing direct USB requeue work, while packet cadence, order,
  timestamps, routing, and timecode input semantics remain intact.
- The contract provides a compiled offline gate before any DriverKit SDK,
  system extension, USB, CoreAudio, or hardware action.

Contract:
- Safe scenarios must have `hal_steady_requeues=0`.
- All steady-state slots must be prepared; fallback allocations are rejected.
- Completion gaps greater than `1.25x`, timestamp reorder/regression, routing
  identity failure, or timecode profile failure reject the model.
- The model covers both deeper prepared queues and mainline-like queue depth.

Alternatives discarded:
- Keep optimizing the existing HAL enqueue path: rejected as the primary route
  because previous knobs, reuse, raw completions, and cadence variants did not
  satisfy quality plus CPU.
- Build or install a real dext immediately: rejected because the local toolchain
  lacks DriverKit SDK support and hardware/system-extension work requires a
  locked, explicit window.

Evidence:
- `local-analysis/cpp-offline/driverkit-prepared-transport-contract.json`

Next implication:
- The next implementation target is an actual C++ transport abstraction that
  can satisfy this contract with the real packet/ring types, then a locked
  physical A/B only after the offline suite remains green.

## 2026-06-17: Promote Prepared Transport Contract Into Core

Decision:
- Add `PreparedTransportBackend` in `core/include/opena8djcpp/prepared_transport.hpp`
  and `core/src/prepared_transport.cpp`.
- Update `opena8djcpp_driverkit_prepared_transport_contract` to use that core
  type instead of a local one-off simulation.

Reason:
- The previous contract encoded the right architecture, but it was still
  tool-local. That made it too easy for future implementation code to diverge
  from the gate.
- The new core type owns prepared-slot counters, capture/playback SPSC rings,
  timestamp ordering, channel identity validation, HAL-facing read/write calls,
  and safety snapshots.
- This is the first reusable C++ bridge toward a future DriverKit/USB backend
  while preserving the offline-only safety boundary.

Alternatives discarded:
- Keep the contract as a standalone tool: rejected because the product path
  needs reusable core code, not just a JSON-producing model.
- Jump directly into macOS DriverKit SDK code: rejected because the local
  DriverKit SDK is unavailable and system-extension work still requires an
  explicit locked window.

Evidence:
- `opena8djcpp_core_tests` now validates `PreparedTransportBackend`.
- `opena8djcpp_driverkit_prepared_transport_contract` now reports schema
  `opena8djcpp.driverkit-prepared-transport-contract.v2`, backed by the core
  backend.

Next implication:
- The next implementation step is to connect packet packing/decode and routing
  batches to `PreparedTransportBackend`, then replace the model-only backend
  with a real DriverKit/USB adapter when the toolchain and hardware window are
  ready.

## 2026-06-17: Add Prepared Transport Packet/Ring Contract

Decision:
- Extend `PreparedTransportBackend` with batch HAL read/write and backend
  completion APIs.
- Add `tools/prepared_transport_packet_contract.cpp` and wire it into CTest
  plus `scripts/run-cpp-offline-gates`.

Reason:
- The prepared backend had reusable rings and counters, but the product path
  also needs proof that the backend can carry real Audio 8 DJ Mode2 packet
  frames without leaving the offline safety boundary.
- The new gate uses `Mode2OutputPacker`, `decode_mode2_usb_bytes_into`, and the
  core backend together. It validates default `start_byte=4`, transfer size
  `352`, capture decode into the backend ring, playback extraction from the
  backend ring, and repacking/decoding of playback bytes.

Alternatives discarded:
- Keep packet validation separate from transport validation: rejected because a
  future DriverKit adapter must satisfy both contracts together.
- Use physical hardware to validate this step: rejected because the same packet
  and ring invariants are provable offline first.

Evidence:
- `local-analysis/cpp-offline/prepared-transport-packet-contract.json`
- Current result:
  `capture_decoded_frames=131`, `playback_decoded_frames=131`,
  `capture_check_errors=0`, `playback_check_errors=0`,
  `capture_prefix_mismatches=0`, `playback_prefix_mismatches=0`,
  `hal_steady_requeues=0`, `fallback_allocations=0`, and `product_safe=true`.

Next implication:
- The backend now has offline proof for Mode2 packet/ring movement. The next
  missing layer is routing/timecode-profile batch policy over this backend, then
  a real DriverKit/USB adapter when environment and lock window allow.

## 2026-06-17: Add Prepared Transport Routing/Timecode Contract

Decision:
- Add S24 batch routing in the pure C++ core.
- Add `tools/prepared_transport_routing_timecode_contract.cpp` and wire it into
  CTest plus `scripts/run-cpp-offline-gates`.

Reason:
- Packet/ring movement alone does not prove the product path preserves deck
  identity or DVS/timecode behavior.
- The new gate validates playback routing through `PreparedTransportBackend`
  and validates timecode-vinyl, timecode-cd-line, and phono profiles across
  decks A/B/C/D after Mode2 decode and backend capture-ring traversal.
- This keeps the next DriverKit/USB candidate bound to the behavior that
  matters for Traktor/timecode without touching hardware.

Alternatives discarded:
- Rely on the existing standalone DVS packet gate only: rejected because it
  bypasses the prepared backend/ring path.
- Defer deck/profile validation to physical Traktor: rejected because the
  mapping and profile invariants are testable offline first.

Evidence:
- `local-analysis/cpp-offline/prepared-transport-routing-timecode-contract.json`
- Current result:
  playback routing PASS with `0` mismatches; `12` profile/deck rows PASS;
  `hal_steady_requeues=0`; `fallback_allocations=0`.

Next implication:
- The C++ core now has offline proof for packet, ring, routing, and timecode
  profile flow over the prepared backend. Remaining blockers are a real
  DriverKit/USB adapter, physical quality/CPU evidence, recovery validation,
  and same-session mainline comparison.

## 2026-06-17: Add Prepared Transport Recovery Contract

Decision:
- Add `tools/prepared_transport_recovery_contract.cpp` and wire it into CTest,
  `scripts/run-cpp-offline-gates`, and static policy.
- Make `PreparedTransportBackend::safety()` return all-false when the backend
  is not started.

Reason:
- Packet/routing/timecode gates prove steady-state behavior, but not recovery.
  A product driver must survive stop/start and invalid configuration attempts
  without carrying stale frames, timestamp history, or counters into a new
  stream session.
- A never-started instance previously had all counters at zero, which could
  look product-safe if a caller only checked `safety().product_safe`. That is a
  false readiness signal, so the safety contract now requires an active
  session.

Alternatives discarded:
- Treat recovery as physical-only: rejected because stale rings, invalid
  config handling, stopped operations, and timestamp-history reset are
  deterministic offline invariants.
- Leave `safety()` counter-only: rejected because zero counters before start are
  not product evidence.

Evidence:
- `local-analysis/cpp-offline/prepared-transport-recovery-contract.json`
- Required result:
  invalid config failures `0`, false unstarted safe failures `0`, stopped
  operation failures `0`, stale frame mismatches `0`, counter reset failures
  `0`, timestamp reset failures `0`, HAL steady requeues `0`, fallback
  allocations `0`, and final clean-session `product_safe=true`.

Next implication:
- The prepared backend now has offline proof for steady-state packet/routing/
  timecode behavior and restart hygiene. Remaining blockers are a real
  DriverKit/USB adapter and physical same-session quality/CPU proof against
  mainline.

## 2026-06-17: Add Offline DriverKit Runtime Bridge Contract

Decision:
- Extend `AudioDriverSkeleton` with stream configuration, stream start/stop,
  HAL-style playback/capture methods, backend completion, and transport
  counters/safety access.
- Add `tools/driverkit_runtime_contract.cpp` and wire it into CTest,
  `scripts/run-cpp-offline-gates`, and static policy.

Reason:
- The DriverKit shell previously only proved lifecycle state transitions. The
  next executable boundary must show how AudioDriverKit-facing stream callbacks
  will talk to `PreparedTransportBackend` without installing or activating a
  system extension.
- The runtime contract verifies valid/invalid sample-rate and transport config
  handling, stream start sequencing, frame movement through playback/capture
  rings, and shutdown behavior.

Alternatives discarded:
- Jump directly to a dext target: rejected because the HAL/backend runtime
  contract is testable offline first, without touching System Extensions,
  CoreAudio defaults, or USB devices.
- Keep the shell state-only until hardware: rejected because it would leave the
  DriverKit boundary unmeasured.

Evidence:
- `local-analysis/cpp-offline/driverkit-runtime-contract.json`
- Required result:
  lifecycle/config/frame/shutdown failures `0`, HAL steady requeues `0`,
  fallback allocations `0`, ring overruns/underruns `0`, timestamp regressions
  `0`, channel identity failures `0`, and `running_product_safe=true`.

Next implication:
- The C++ line now has an offline DriverKit-style runtime bridge over the
  prepared backend. Remaining blockers are real AudioDriverKit class binding,
  USBDriverKit adapter implementation, signing/entitlements, and physical
  same-session quality/CPU proof.

## 2026-06-17: Add Non-Installing DriverKit Extension Scaffold

Decision:
- Add `driverkit/extension/` with non-installing templates for:
  `Info.plist`, DriverKit entitlements, `OpenA8DJAudioDriver.iig`,
  `OpenA8DJAudioDevice.iig`, and future DriverKit SDK binding sources.
- Add `tools/driverkit_extension_scaffold_contract.cpp` and wire it into CTest,
  `scripts/run-cpp-offline-gates`, evidence schema, and static policy.

Reason:
- The runtime bridge proves the offline C++ boundary, but the project also
  needs a concrete dext shape for the future AudioDriverKit/USBDriverKit build.
- The scaffold captures the Apple-required direction: `IOKitPersonalities`,
  `IOUserAudioDriver`, `IOUserAudioDevice`, HAL user-client properties,
  DriverKit audio entitlement, USB transport entitlement, and Audio 8 DJ USB
  match IDs.
- Keeping it out of the default build prevents accidental system-extension
  activation or false readiness on a machine that currently lacks the DriverKit
  SDK.

Alternatives discarded:
- Generate a hand-edited Xcode project now: rejected because CMake remains the
  source of truth and the local DriverKit SDK is absent.
- Compile or install the dext scaffold now: rejected because that would require
  missing SDK/entitlements and an explicit hardware/system-extension window.

Evidence:
- `local-analysis/cpp-offline/driverkit-extension-scaffold-contract.json`
- Required result:
  files present, Info.plist pass, entitlements pass, IIG pass, runtime binding
  pass, safety pass, default build excludes extension, and installed/activated
  flags false.

Next implication:
- The C++ line now has an offline-tested dext scaffold. Remaining blockers are
  full Xcode/DriverKit SDK, real AudioDriverKit class compilation,
  USBDriverKit endpoint adapter implementation, signing/entitlements, and
  physical same-session quality/CPU proof against mainline.

## 2026-06-17: Add C++ Loopback Quality Analyzer Gate

Decision:
- Add `tools/loopback_quality_analysis.cpp` as a dependency-free C++ analyzer
  for reference/capture loopback quality.
- Wire it into CMake, CTest, `scripts/run-cpp-offline-gates`, and the evidence
  schema.

Reason:
- Audiophile claims need objective signal metrics, not only transport counters
  or clean compilation.
- The Python analyzers remain useful for exploratory diagnostics, but a core
  C++ analyzer gives the candidate a portable, cheap, reproducible gate that can
  run in the same compiled test surface as packet/routing/timecode contracts.
- The analyzer currently measures alignment, fitted gain, correlation, SNR,
  residual RMS/peak, peak level, and robust click outliers. Its selftest proves
  it accepts a clean delayed loopback and rejects a degraded one.

Alternatives discarded:
- Install more Python/scientific dependencies now: rejected because this gate
  does not need them and extra dependencies would reduce reproducibility.
- Treat previous physical captures as enough: rejected because historical
  evidence is not a same-session proof that this C++ candidate beats mainline.

Evidence:
- `local-analysis/cpp-offline/loopback-quality-analysis.json`
- Required result:
  clean synthetic loopback passes; degraded synthetic loopback fails for
  objective reasons such as SNR/correlation/clicks; aggregate result PASS.

Next implication:
- Future locked physical gates can use the compiled analyzer on real iRig or
  other external-loopback captures. This still does not prove readiness until
  the same route shows C++ > mainline on quality, CPU, jitter, routing, and
  timecode.

## 2026-06-17: Add C++ Channel Leakage Tone Contract

Decision:
- Add `tools/channel_leakage_tone_contract.cpp` and wire it into CMake, CTest,
  `scripts/run-cpp-offline-gates`, and evidence schema.

Reason:
- The product gate requires no leakage between decks A/B/C/D. Existing physical
  tone-matrix analysis is useful, but the C++ line also needs a compiled
  offline contract proving the analyzer and Mode 2 pack/decode path can detect
  deck leakage.
- The contract generates per-channel integer-cycle tones for 44.1 kHz and
  48 kHz, exercises each active pair A/B/C/D through real Mode 2 pack/decode,
  measures wrong-source and inactive-deck tone energy, and confirms an injected
  inactive-deck leak is rejected.

Alternatives discarded:
- Reuse only `simulated_output_matrix`: rejected because that gate measures
  broad synthetic audio quality but does not explicitly prove a tone-domain
  leakage detector with negative injected-leak cases.
- Port the full physical capture analyzer in the same patch: deferred because
  that is larger and should read existing capture directories without touching
  hardware.

Evidence:
- `local-analysis/cpp-offline/channel-leakage-tone-contract.json`
- Required result:
  all clean A/B/C/D rows pass at 44.1 kHz and 48 kHz; all injected-leak rows
  are rejected; aggregate result PASS.

Next implication:
- The C++ offline suite now has a stricter digital no-leakage contract. It is
  still not physical routing proof; the next analyzer should consume stored
  `fixture/reference.wav` and `captured.wav` runs and report the physical
  leakage fields used by the current Python tone-matrix gate.

## 2026-06-17: Add C++ Capture Matrix Quality Analyzer

Decision:
- Add `tools/capture_matrix_quality_analysis.cpp` and wire it into CMake,
  CTest, `scripts/run-cpp-offline-gates`, and evidence schema.

Reason:
- Objective sound-quality work needs a compiled analyzer that can read stored
  physical capture directories without opening audio devices or touching USB.
- The tool reports alignment, per-channel fitted gain/correlation/SNR,
  residual RMS/peak, click outliers, clipped frames, and decorrelated-tone
  leakage fields:
  `left_to_right_leakage_db`, `right_to_left_leakage_db`, and
  `max_wrong_source_leakage_db`.
- The selftest accepts a clean synthetic capture and rejects a degraded capture
  with leakage, clipping, click, poor SNR, and poor correlation.

Alternatives discarded:
- Keep this logic only in Python: rejected because the C++ candidate needs a
  compiled, reproducible evidence path.
- Treat tone leakage as sufficient product quality: rejected because existing
  captures can pass leakage while still failing SNR/correlation/residual
  quality.

Evidence:
- `local-analysis/cpp-offline/capture-matrix-quality-analysis.json`
- `local-analysis/cpp-offline/capture-matrix-quality-real-existing-routing.json`

Next implication:
- The next physical window should use this analyzer on same-session C++ and
  mainline captures, but readiness still requires the full quality/CPU/jitter
  comparison, not just leakage PASS.

## 2026-06-17: Require Paired Product Evidence In Promotion Evaluator

Decision:
- Change the default promotion evaluator selection so physical music metrics
  and CPU/resource metrics come from the same `local-analysis/soundcheck/*`
  run directory.
- Add the evaluator result to the offline evidence bundle without treating
  `NOT_READY` as an offline build failure.

Reason:
- Branch promotion is a joint product claim. A direct-USB capture can be useful
  for diagnostics, routing, or USB integrity, but it is not a complete product
  run if there is no same-run CPU profile.
- Mixing the newest music file with the newest CPU file can create false
  conclusions. The correct default is an internally paired physical product
  run, with explicit CLI overrides still available for controlled comparisons.

Alternatives discarded:
- Keep selecting newest music and newest CPU independently: rejected because it
  can mix transport families or test modes.
- Fail the offline gate because the promotion evaluator returns `FAIL`:
  rejected because `FAIL` is currently the correct product verdict. The offline
  gate should prove the evaluator ran and blocked promotion, not pretend the
  candidate is ready.

Evidence:
- `local-analysis/cpp-offline/promotion-readiness-offline-check.json`
- `local-analysis/cpp-offline/current-offline-gates.json`

Next implication:
- Future physical windows must produce same-run `metrics.json` and
  `cpu-profile.tsv` evidence. Promotion remains forbidden until this evaluator
  returns `PASS` and `branch_promotion_allowed=true`.

## 2026-06-17: Add Prepared Slot Scheduler Contract

Decision:
- Add `PreparedSlotScheduler` to the C++ core and gate it with
  `opena8djcpp_prepared_slot_scheduler_contract`.

Reason:
- Existing physical evidence shows the simple ISO/cadence frontier is
  exhausted: high cadence keeps quality closer but costs too much CPU, while
  lower enqueue cadence can hit CPU but destroys quality.
- The next viable architecture must reduce HAL hot-path USB work without
  changing audio cadence. That requires a prepared-slot backend contract with
  explicit lead, backend requeue budget, no fallback allocation, and zero HAL
  steady-state requeues.
- Encoding that contract in C++ makes future DriverKit/USB adapter work
  measurable before any hardware window.

Alternatives discarded:
- Add another HAL timing flag first: rejected because recent physical flags
  have mostly moved along the same failed quality/CPU frontier.
- Keep this only in `transfer_pool_model`: rejected because the product core
  needs reusable state/counters/safety semantics, not just a standalone model.

Evidence:
- `local-analysis/cpp-offline/prepared-slot-scheduler-contract.json`
- `local-analysis/cpp-offline/current-offline-gates.json`

Next implication:
- The future runtime adapter must satisfy the scheduler contract and then prove
  physical quality and CPU against mainline. This contract does not by itself
  authorize driver installation, hardware readiness, or branch promotion.

## 2026-06-17: Add Native Product Superiority Comparator

Decision:
- Extend `tools/physical_run_compare.cpp` from a summary helper into a native
  product comparator with explicit `candidate_vs_mainline_reference` and
  `candidate_vs_baseline_run` modes.
- Add its output to the standard offline evidence bundle as
  `local-analysis/cpp-offline/physical-run-product-superiority.json`.

Reason:
- The project objective is not compilation or clean counters; it is objectively
  better sound quality, functionality, timecode behavior, and lower resource
  use than mainline.
- A native C++ comparator prevents cherry-picking: quality metrics and CPU must
  be evaluated together, and missing or worse metrics fail closed.
- The latest available same-run product evidence still fails both audiophile
  quality and CPU/resource thresholds, so the evidence bundle must say that
  plainly.
- The selected run has raw WAV evidence available. The comparator now records
  that native WAV reanalysis is `AVAILABLE_NOT_YET_USED`, so this is not
  mistaken for a full independent C++ audio-quality analyzer.

Alternatives discarded:
- Leave the decision only in Python promotion scripts: rejected because the C++
  candidate should own a compiled, reproducible product-evidence check.
- Treat a CPU-only win as progress toward promotion: rejected because previous
  ISO-family runs can reduce CPU while failing quality and lag behavior.
- Make the offline gate fail when product superiority fails: rejected because
  the offline bundle should pass when it honestly records `FAIL` product
  readiness; promotion remains blocked by the comparator and evaluator.

Evidence:
- `local-analysis/cpp-offline/physical-run-product-superiority.json`
- `local-analysis/cpp-offline/current-offline-gates.json`

Next implication:
- Future physical windows must produce same-run product evidence that turns
  this comparator to `PASS`, and promotion still requires the separate
  promotion evaluator to return `branch_promotion_allowed=true`.

## 2026-06-17: Add Native Soundcheck WAV Quality Reanalysis

Decision:
- Add `tools/soundcheck_wav_quality.cpp` as the first compiled C++ analyzer for
  stored soundcheck WAV pairs.
- Wire it into CMake, CTest, `scripts/run-cpp-offline-gates`, and the evidence
  schema.

Reason:
- The promotion path needs independent C++ evidence over actual WAV captures,
  not only Python-generated `metrics.json`.
- The latest selected run already has `fixture/reference.wav` and
  `captured.wav`, so there is no reason to defer native reanalysis.
- This first slice proves alignment and broad metric parity before deeper
  algorithm unification.

Alternatives discarded:
- Jump directly to full Python replacement: rejected because the existing
  analyzer includes time-warp, CPU coupling, quiet-window, and failure-mode
  logic; porting that safely should be incremental.
- Use the native comparator alone: rejected because it reads already-computed
  metrics and cannot catch analyzer drift or bad alignment by itself.

Evidence:
- `local-analysis/cpp-offline/soundcheck-wav-quality.json`

Next implication:
- Tighten the parity tolerances and port remaining Python logic: time-warp,
  CPU coupling, exact quiet/noise windows, and click-count semantics.

## 2026-06-17: Use Native WAV Reanalysis In Product Comparator

Decision:
- Make `tools/physical_run_compare.cpp` consume matching
  `local-analysis/cpp-offline/soundcheck-wav-quality.json` evidence.
- Add native music gates alongside recorded `metrics.json` gates when the WAV
  reanalysis matches the selected candidate run.
- Add stable CPU reporting after 5s without relaxing the strict total-run CPU
  gates.

Reason:
- Promotion cannot depend on Python-only quality metrics once native WAV
  evidence exists.
- The selected ISO12/q8 run remains bad under native analysis:
  quality `0.953641`, SNR floor `8.797298 dB`, mid/high residual
  `1.685303/1.580494`, quiet residual `-34.694516 dBFS`, and lag jumps `32`.
- CPU needed phase separation: `coreaudiod` has early transient spikes, but
  OpenA8DJ driver CPU is sustained at `16.6%` even after 5s.
- The selected run has process-level CPU evidence only:
  `callback_attribution_status=external_process_cpu_only_hot_path_timing_absent`.
  This blocks precise callback root-cause claims until hot-path timing is
  captured.

Alternatives discarded:
- Replace recorded metrics with native metrics immediately: rejected because
  the native analyzer is still a first-slice port with broad parity tolerance.
- Relax CPU gates using post-5s `coreaudiod`: rejected because strict product
  evidence must still account for total-run behavior and because driver CPU is
  the larger sustained problem.

Evidence:
- `local-analysis/cpp-offline/soundcheck-wav-quality.json`
- `local-analysis/cpp-offline/physical-run-product-superiority.json`
- `local-analysis/cpp-offline/current-offline-gates.json`

Next implication:
- Do not promote or request readiness from ISO12/q8. The next implementation
  target is reducing sustained driver CPU and isolating the
  residual/capture-path failure with decorrelated physical evidence.
- Future CPU candidates need hot-path timing or equivalent callback attribution
  before claiming why CPU improved.

## 2026-06-17: Add Native Residual Attribution

Decision:
- Add `residual_attribution` to `tools/soundcheck_wav_quality.cpp`.
- Classify the selected stored failure as
  `uncorrelated_residual_or_capture_path_dominant`.

Reason:
- External offline analysis and native reanalysis agree that simple timing,
  simple L/R matrix/routing, clipping, and click outliers do not explain the
  failed physical WAV.
- Native measurements show timing explain `0.728741 dB` and routing matrix
  explain `0.150521 dB`, both far below the `3 dB` material-explanation bar.
- Source L/R correlation is `0.986751`, so stereo music is a poor fixture for
  proving routing matrix behavior; decorrelated physical tones remain required.

Alternatives discarded:
- Keep a single SNR gate as the only audio-quality explanation: rejected
  because it hides whether the failure is timing, routing, distortion, or
  capture-path residual.
- Declare timing dominant from `32` lag jumps alone: rejected because the
  measured timing correction explains less than `1 dB`.

Evidence:
- `local-analysis/cpp-offline/soundcheck-wav-quality.json`

Next implication:
- The next physical window should use a decorrelated fixture and capture-path
  controls before any Traktor/timecode or branch-promotion claim.

## 2026-06-17: Add Native Hot-Path Timing Attribution Analyzer

Decision:
- Add `tools/hot_path_timing_analysis.cpp`.
- Wire it into the offline CMake/CTest/evidence path.

Reason:
- Sustained driver CPU is the current resource blocker, but the selected
  product run has only process-level CPU.
- Existing stored hot-path diagnostics contain nonzero per-segment timing; they
  should be summarized by a native C++ tool rather than left as buried JSON.
- The selected stored timing evidence points at fixed transport work:
  capture requeue, playback queue, and playback enqueue are much larger than
  playback fill.

Alternatives discarded:
- Change audio math first: rejected because stored timing evidence says
  playback fill is not dominant.
- Treat nested timings as additive CPU: rejected because nested/sampled timing
  can exceed the capture handler when summed.

Evidence:
- `local-analysis/cpp-offline/hot-path-timing-analysis.json`

Next implication:
- The next optimization should target fixed transport queue/requeue/enqueue
  overhead or callback cadence, then prove same-session quality did not regress.

## 2026-06-17: Add Native Quality Root-Cause Analysis Gate

Decision:
- Add `tools/quality_root_cause_analysis.cpp`.
- Wire it into CMake/CTest and the offline evidence bundle.

Reason:
- Current physical failures cannot be explained by a single metric.
- Existing evidence says the packed USB payload can be bit-clean while analog
  capture quality still fails hard.
- Mainline and C++ both show a degraded route signature in one stored
  same-fixture comparison, so promotion must be blocked until the physical
  route is validated in-session.

Alternatives discarded:
- Optimize HAL micro-flags again: rejected because reused completions, raw
  completions, fast ISO config, coalescing, and lead changes already have
  negative physical or cadence evidence.
- Treat payload cleanliness as readiness: rejected because analog SNR,
  residual, lag, and CPU gates still fail.

Evidence:
- `local-analysis/cpp-offline/quality-root-cause-analysis.json`

Next implication:
- The next hardware window must first prove route health, then compare C++
  against mainline in the same session before any branch-promotion claim.

## 2026-06-17: Add Native Timecode Readiness Gate

Decision:
- Add `tools/timecode_readiness_gate.cpp`.
- Wire it into CMake/CTest and offline evidence.

Reason:
- The project has multiple offline timecode/DVS tests, but physical Traktor
  lock evidence is still absent.
- A single aggregate gate makes it harder to overclaim “timecode vinyl ready”
  from synthetic PASS results.

Alternatives discarded:
- Treat offline DVS PASS as physical readiness: rejected because Traktor scope
  and lock behavior must be validated under hardware lock.

Evidence:
- `local-analysis/cpp-offline/timecode-readiness-gate.json`

Next implication:
- Physical Traktor/timecode validation remains a required later hardware-window
  gate after music quality, CPU, route health, and safety preflight are sane.

## 2026-06-17: Add Prepared Transport Migration Gate

Decision:
- Add `tools/prepared_transport_migration_gate.cpp`.
- Keep it as an offline migration gate, not a readiness gate.
- Report `product_ready=false`, `branch_promotion_supported=false`, and
  `physical_ab_required_before_claim=true` even when the gate passes.

Reason:
- Stored hot-path timing points at fixed queue/requeue/enqueue cost, with a
  fixed-transport-to-playback-fill ratio of `17.914629`.
- The prepared transport contracts already model zero HAL steady-state
  requeues, no fallback allocations, safe routing/timecode, and clean recovery.
- These facts support building the next controlled transport candidate, but
  they do not prove sound quality, timecode vinyl lock, or CPU superiority
  against mainline.

Alternatives discarded:
- Treat prepared transport contract PASS as product readiness: rejected because
  the selected physical product comparison still fails.
- Continue isolated HAL flag experiments: rejected because several previous
  CPU/cadence knobs were physically rejected or failed quality.

Evidence:
- `local-analysis/cpp-offline/prepared-transport-migration-gate.json`
- `local-analysis/cpp-offline/hot-path-timing-analysis.json`
- `local-analysis/cpp-offline/physical-run-product-superiority.json`

Next implication:
- The next implementation direction is runtime binding for prepared transport,
  followed by a lock-gated same-session A/B hardware window against mainline.
  No branch promotion or quality claim is allowed before that evidence.

## 2026-06-17: Add Prepared Transport Pressure Gate

Decision:
- Add `tools/prepared_transport_pressure_gate.cpp`.
- Require it from `tools/prepared_transport_migration_gate.cpp`.

Reason:
- A migration gate based only on short contracts could miss long-run ring,
  timestamp, fallback, or deck-specific failures.
- The pressure gate covers 44.1/48 kHz and all A/B/C/D decks without touching
  hardware.

Evidence:
- `local-analysis/cpp-offline/prepared-transport-pressure-gate.json`

Next implication:
- Prepared transport has stronger offline support as the next performance
  candidate, but physical readiness and branch promotion remain blocked.

## 2026-06-17: Strengthen DriverKit Runtime Contract

Decision:
- Upgrade `tools/driverkit_runtime_contract.cpp` to schema v2.
- Clear stream configuration when `AudioDriverSkeleton::stop_driver()` runs.
- Require runtime-shell pressure evidence from the prepared migration gate.

Reason:
- The DriverKit shell must not pass only a tiny happy-path batch.
- Stop/restart behavior should not carry stale stream configuration.
- The migration direction should require pressure through the shell boundary,
  not only through the core backend.

Evidence:
- `local-analysis/cpp-offline/driverkit-runtime-contract.json`
- `local-analysis/cpp-offline/prepared-transport-migration-gate.json`

Next implication:
- The next implementation step can focus on dext/USB binding design or a
  lock-gated physical window only after deciding the exact runtime candidate.
  No hardware readiness claim is added by this contract.

## 2026-06-17: Add Capture Route Health Gate

Decision:
- Add `tools/capture_route_health_gate.cpp` and wire it into CMake, CTest,
  offline gates, and evidence schema.
- Treat unhealthy shared capture route evidence as a blocker for product
  promotion even when the diagnostic itself executes successfully.

Reason:
- Current physical evidence mixes candidate failure with signs that the shared
  capture route is not healthy enough for a trustworthy A/B claim.
- The project needs an explicit gate that says whether a measurement can be
  used for promotion, not only whether a candidate beats thresholds.

Evidence:
- `local-analysis/cpp-offline/capture-route-health-gate.json`

Next implication:
- Before another quality-superiority claim, run a lock-gated capture route
  revalidation with the Audio 8 DJ -> capture chain and store the evidence.
  If route health still fails, do not compare driver candidates from that run.

## 2026-06-17: Batch SPSC Publication for Prepared Hot Path

Decision:
- Change `SpscFrameRing::push_many` and `pop_many` to publish read/write
  indices once per batch.
- Add `opena8djcpp_driverkit_prepared_hotpath_contract`.
- Require that contract from `prepared_transport_migration_gate`.

Reason:
- Existing timing evidence points at fixed queue/requeue/enqueue work as the
  CPU suspect, not sample packing/decoding.
- Per-frame index publication is unnecessary for iso-period batches and
  inflates callback-side work in the model.

Evidence:
- `local-analysis/cpp-offline/driverkit-prepared-hotpath-contract.json`
- `local-analysis/cpp-offline/prepared-transport-migration-gate.json`

Next implication:
- Offline evidence now supports the prepared hot-path implementation direction.
  It does not prove lower physical CPU until a lock-gated A/B run compares the
  runtime candidate against mainline on the same route.

## 2026-06-17: Add Physical Evidence Frontier Scan

Decision:
- Add `tools/physical_evidence_frontier.cpp` and wire it into CMake, CTest,
  offline gates, and evidence schema.
- Use the scanner to aggregate all existing soundcheck runs rather than relying
  only on hand-selected physical-run summaries.

Reason:
- The project needs a broad view of the stored physical evidence before
  deciding whether a candidate is plausibly better.
- A single selected run can hide whether the problem is candidate-specific,
  route-wide, or shared across families.

Evidence:
- `local-analysis/cpp-offline/physical-evidence-frontier.json`

Next implication:
- No existing physical run supports promotion. The next physical window must
  first prove capture-route health, then run same-session mainline/C++ A/B.

## 2026-06-17: Add Known-Good Non-Audio8 Route Soundcheck Harness

Decision:
- Add a lock-gated physical harness for a known-good non-Audio8 playback
  source through the existing capture route.
- Extend `audio-wav-play` with explicit device name/UID selection so the
  harness can target a chosen output device without changing default devices.
- Refuse Audio 8 DJ/OpenA8DJ as the known-good source.

Reason:
- Existing evidence separates clean internal/USB payload from failed iRig
  physical capture, but it cannot yet prove whether the shared mixer/REC OUT
  -> iRig route is healthy enough for a fair driver comparison.
- The next measurement should validate the route before another Audio 8 driver
  candidate is judged.

Evidence:
- Focused build/usage/policy checks only so far; no physical route capture yet.

Next implication:
- Run this only inside an authorized hardware window, with the lock held, using
  a real non-Audio8 output routed into the same capture path. Product readiness
  and branch promotion remain blocked until the route test and same-session
  mainline/C++ A/B pass.

## 2026-06-17: Add Physical Superiority Window Runner

Decision:
- Add `scripts/run-physical-superiority-window` as the single scripted entry
  point for the next lock-gated physical evidence window.
- Make it dry-run by default and require `--execute` before playback, capture,
  HAL install/reload, CoreAudio restart, or recovery cleanup can occur.
- Require the known-good route check before candidate soundcheck unless
  explicitly marked diagnostic-only with `--skip-known-good`.

Reason:
- The project needs comparable physical evidence, not another isolated
  candidate tweak.
- A single runner reduces operator error by preserving order, evidence
  directories, lock ownership, native analyzer output, and cleanup.
- It keeps the distinction clear: HAL candidate physical testing is currently
  possible under lock; DriverKit/dext activation is still not part of this
  runner.

Evidence:
- `scripts/run-physical-superiority-window --help`
- Dry-run plan creation under
  `local-analysis/physical-superiority-window/dry-run-check/`
- Audio 8 rejection check for the known-good source.
- `opena8djcpp_hardware_lock_policy_check` now audits `8` sensitive scripts.

Next implication:
- The next physical window can use this runner, but no readiness or branch
  promotion claim is allowed until its route, soundcheck, CPU, native WAV, and
  promotion evaluator evidence all pass against mainline thresholds.

## 2026-06-17: Require Same-Session Mainline/C++ Physical A/B

Decision:
- Tighten `scripts/run-physical-superiority-window` so full execution requires
  both an explicit read-only mainline HAL candidate and an explicit C++ HAL
  candidate.
- Run mainline first, unload it, run C++, analyze both captures with the native
  C++ WAV analyzer, and compare both runs with
  `opena8djcpp_physical_run_compare`.
- Keep `--candidate-only` as a diagnostic mode that always writes a blocked
  same-session comparison result and cannot support readiness or branch
  promotion.

Reason:
- Quality-audiophile claims must be based on a same-session measurement through
  the same physical capture route. An isolated C++ capture cannot prove lower
  noise, lower CPU, lower jitter, or better routing than mainline.
- The previous dry-run runner shape could guide a candidate-only run but did
  not force the evidence needed for a superiority claim.

Evidence:
- `bash -n scripts/run-physical-superiority-window`
- `scripts/run-physical-superiority-window --help`
- Dry-run plan under
  `local-analysis/physical-superiority-window/dry-run-same-session-check/`
- Early refusal when `--execute` omits `--mainline-candidate`.
- Early refusal when `--candidate-only --execute` points at a missing C++
  bundle.
- `opena8djcpp_hardware_lock_policy_check` PASS with `8` audited scripts.

Next implication:
- The next physical window must supply `--mainline-candidate` and `--candidate`
  unless the operator explicitly wants a diagnostic-only blocked run.
- Promotion remains blocked unless both same-session comparison and product
  readiness evaluation pass.

## 2026-06-17: Close Physical Readiness Loopholes

Decision:
- Make `opena8djcpp_physical_run_compare` report
  `branch_promotion_supported=true` only when an explicit baseline run was
  supplied. Fixed historical thresholds remain diagnostic.
- Make `scripts/evaluate-promotion-readiness.py` require a
  `--same-session-compare` JSON whose parent window matches the C++ soundcheck
  window.
- Make `--skip-known-good` in `scripts/run-physical-superiority-window`
  produce `known_good_rc=1` and block a successful runner exit.
- Extend `opena8djcpp_hardware_lock_policy_check` to audit the blocked
  candidate-only and skipped-route markers.
- Treat promotion `NOT_READY` as physical timecode blocked in
  `opena8djcpp_timecode_readiness_gate`.

Reason:
- A standalone comparator, stale promotion evidence, or skipped route check
  could otherwise create a plausible but invalid path to a readiness claim.
- A promotion evaluator that returns early as `NOT_READY` must not be
  interpreted as physical Traktor/timecode evidence.
- The promotion path must require route health, same-session mainline/C++ A/B,
  product quality/CPU gates, and timecode gates simultaneously.

Evidence:
- `opena8djcpp_physical_run_compare` default mode now reports
  `branch_promotion_supported=false` and
  `same_session_baseline_required_for_promotion=true`.
- `scripts/evaluate-promotion-readiness.py --json-out
  local-analysis/promotion-readiness-after-same-session-hardening.json`
  returns `NOT_READY`, `branch_promotion_allowed=false`, and fails
  `evidence:same_session_compare` when no same-window comparison exists.
- `scripts/run-physical-superiority-window` dry-run remains plan-only and
  performs no hardware/audio work.
- `scripts/run-cpp-offline-gates` PASS:
  - Debug CTest `41/41` PASS.
  - Release CTest `42/42` PASS.
  - Hardware lock policy PASS with `8` audited scripts.
- Focused `opena8djcpp_timecode_readiness_gate` after the contract fix reports
  `physical_traktor_timecode_blocked=true` and `product_timecode_ready=false`.
- Focused `opena8djcpp_prepared_transport_migration_gate` returns `PASS`.

Next implication:
- No script-level path can support branch promotion without a same-window
  mainline/C++ physical compare and a successful known-good route check.

## 2026-06-17: Add Read-Only Physical Window Preflight

Decision:
- Add `scripts/physical-window-preflight`.
- Run that preflight from `scripts/run-physical-superiority-window --execute`
  before acquiring the hardware lock or installing/reloading any HAL candidate.
- Check candidates, iRig/CoreAudio capture visibility, Audio 8 DJ USB
  visibility, reference/music files, explicit non-Audio8 known-good output,
  and hardware-lock availability.
- Keep `Open Audio 8 DJ` CoreAudio presence optional before candidate loading,
  but require it to be `8 in / 8 out` if it is already loaded.

Reason:
- A physical superiority window should not begin if the capture route, USB
  device, HAL bundles, or explicit known-good output are missing.
- Failing before the lock avoids partial driver install/reload attempts when
  the evidence window is invalid.
- Audio 8 can be visible on USB while absent from CoreAudio before HAL loading;
  that state is acceptable for the preflight.

Evidence:
- `scripts/physical-window-preflight` on the current system with iRig, Audio 8
  USB, C++ candidate, mainline candidate, and `MacBook Air Speakers` as a
  visible non-Audio8 output returns `PASS`.
- A runner route-only execution with a missing known-good output exits `2`
  before lock acquisition and writes `PHYSICAL_SUPERIORITY_WINDOW:
  PREFLIGHT_FAIL`.

Next implication:
- The next physical A/B command must first pass this read-only preflight, then
  prove the known-good route by capture. A visible output device alone is not a
  claim that the physical cabling is correct.

## 2026-06-17: Block Physical A/B When Known-Good Route Has No Signal

Decision:
- Treat the route-only attempt using `iRig Stream` output -> `iRig Stream`
  capture as a failed known-good route, not as a driver-quality signal.
- Do not proceed to promotion-quality mainline/C++ A/B on top of that route.

Reason:
- The selected source did not arrive at the iRig capture path with usable
  signal. The capture was near idle/noise level and essentially uncorrelated
  with the deterministic reference.
- A/B testing mainline and C++ through an unvalidated or no-signal capture path
  would produce false confidence and could not support an audiophile-quality
  claim.

Evidence:
- Route-only physical preflight passed.
- Playback and recording completed under the hardware lock.
- Known-good route result was FAIL.
- Captured RMS was about `-68.1 dBFS` while the reference RMS was about
  `-21.2/-22.1 dBFS`.
- Alignment score was about `0.004`; SNR was about `-36.4 dB`.
- Evidence directory:
  `local-analysis/physical-superiority-window/20260617T194016Z-route-only-irig-output-to-irig-capture`.

Next implication:
- The next admissible physical step is to provide or select a real non-Audio8
  source physically routed into the same mixer/REC OUT -> iRig capture path,
  then rerun the known-good route gate.
- Driver install/reload and mainline/C++ A/B should remain blocked for
  product claims until that route gate passes.

## 2026-06-17: Treat Skipped-Route Mainline Failure As Diagnostic Only

Decision:
- Keep the `--skip-known-good` mainline physical result as diagnostic evidence,
  not promotion evidence.
- Stop the same-session window after mainline fails instead of loading C++ into
  an already-invalid comparison.
- Require a runner cleanup fix: when `--leave-loaded` is absent, the final
  unload path must force-unload the active HAL even if stack health is PASS.

Reason:
- The window explicitly skipped the known-good route, so it cannot prove the
  capture route is valid.
- Mainline failed the physical quality gate with strong driver CPU correlation;
  loading C++ afterward would not create a valid superiority comparison.
- The final guard used `--recover --unload-opena8dj`, but recovery did not run
  because the stack health check passed, leaving mainline HAL loaded until a
  separate force-unload.

Evidence:
- Diagnostic window:
  `local-analysis/physical-superiority-window/20260617T194403Z-diagnostic-ab-skip-known-good`.
- Mainline metrics:
  - `quality_alignment_score=0.125194`;
  - `analog_snr_db=-12.77`;
  - `lag_jumps_gt_2_frames=40`;
  - `mid_band_cpu_corr=0.969575 source=opena8dj_driver`;
  - `capture_clipped_frames=0`.
- Force-unload window:
  `local-analysis/physical-superiority-window/20260617T194618Z-force-unload-after-diagnostic-ab`.
- Force-unload moved the active HAL to
  `/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.guard-unloaded-20260617-154618`
  and ended with `opena8dj_state=unloaded`,
  `opena8dj_driver_pids=none`, `audio_stack_guard=PASS`.

Next implication:
- Cleanup now uses `--force-unload-opena8dj` when the active HAL should not be
  left loaded.
- This diagnostic strengthens the need for same-session, route-validated
  physical evidence. It does not make C++ better than mainline by itself.

## 2026-06-17: Gate HAL Bundle Completeness

Decision:
- Add an offline HAL bundle completeness check and include it in
  `scripts/run-cpp-offline-gates`.
- Require physical preflight and HAL candidate safety to verify
  `Contents/MacOS/OpenA8DJHAL`, not just the `.driver` directory.
- Split Makefile flag stamps so USB diagnostic player rebuilds do not delete
  the HAL executable.

Reason:
- A directory-only `build/OpenA8DJ.driver` passed earlier checks while missing
  its executable, causing a C++ candidate safety failure at hardware time.
- Direct USB diagnostic builds use different HAL flags for shared USB code;
  they must not invalidate the installable HAL bundle.

Evidence:
- `scripts/check-hal-bundle-complete` failed before `make hal` because
  `hal_executable_exists=FAIL`.
- After `make hal`, the checker passed with executable size `151464` bytes.
- Rebuilding `build/opena8dj-usb-play` with `HAL_DIAGNOSTIC=1` preserved the
  HAL executable hash
  `844810597df79df54cc08040fc4867cfe7f2efdac7877317085307cd761f0071`.

Next implication:
- Future offline gates must fail before hardware if the C++ HAL bundle is not
  structurally installable.

## 2026-06-17: Direct USB Clean Internally, External Capture Still Fails

Decision:
- Treat direct USB diagnostics as route/analog attribution evidence, not HAL
  readiness.
- Do not spend the next iteration on packet packing unless new evidence
  contradicts the clean internal USB payload.

Reason:
- Direct USB playback bypasses HAL install/load and writes the Audio 8 DJ
  payload directly.
- The internal written, consumed, and packed USB artifacts aligned perfectly
  with the reference, while the iRig capture still failed quality thresholds.

Evidence:
- Direct USB default run:
  `local-analysis/direct-usb-soundcheck/20260617T195548Z-audio8-to-irig-route-bundle-complete-context`.
- Internal USB diagnostics:
  - written/consumed alignment `1.000000`;
  - packed USB alignment `1.000000`;
  - USB check errors `0`;
  - USB panic flags `0`.
- External capture:
  - `quality_alignment_score=0.723444`;
  - `snr_db_min=-1.154967`;
  - `lag_jumps_gt_2_frames=0`.
- Playback-profile variant remained FAIL with
  `quality_alignment_score=0.738457` and `snr_db_min=-0.637949`.

Next implication:
- Current physical blocker is after clean internal USB payload: Audio 8 analog
  output, mixer/REC OUT, iRig capture, or timing attribution.
- C++ HAL optimization must not claim superiority until this route is
  controlled and a same-session mainline/C++ comparison passes.

## 2026-06-17: Expand C++ Physical Capture Forensics To Direct USB And Physical Windows

Decision:
- Extend `opena8djcpp_physical_capture_forensics` beyond HAL soundcheck runs so
  it also classifies archived direct USB soundchecks and physical-superiority
  window captures.
- Surface the best direct USB and best physical-window forensic rows in
  `scripts/run-cpp-offline-gates`.

Reason:
- The current quality debate depends on whether the external iRig capture path
  is valid enough to judge the C++ HAL.
- Direct USB runs bypass HAL and prove the generated USB payload can be clean
  internally, but the captured analog path still fails.
- This needs to be visible in the reproducible offline gate summary rather than
  remaining a manual interpretation of scattered evidence files.

Evidence:
- Expanded C++ forensics over archived WAVs currently sees `81` candidate runs,
  including `16` direct USB runs and `3` physical-window runs.
- It analyzes `21` representative runs and finds `0` strict quality candidates.
- Best direct USB archived run is
  `local-analysis/direct-usb-soundcheck/20260617T183629Z-pairA-12s-after-hal-fail`
  with `quality_alignment_score=0.959037`, `snr_floor_db=9.697139`, and
  classification `variable_timebase_or_route_capture_instability`.
- Best physical-window run is the mainline diagnostic
  `local-analysis/physical-superiority-window/20260617T194403Z-diagnostic-ab-skip-known-good/mainline-soundcheck`
  with `quality_alignment_score=0.125194` and `snr_floor_db=-12.774687`.

Next implication:
- Do not install analysis tooling by default. The active blocker is not
  analysis precision but physical route validity.
- The next physical step must be lock-gated known-good non-Audio8 route
  validation into the same iRig chain, then same-session mainline/C++
  comparison.

## 2026-06-17: Make Timecode Readiness Coverage Machine-Readable

Decision:
- Add schema and row count to `opena8djcpp_timecode_matrix`.
- Extend `opena8djcpp_timecode_readiness_gate` with explicit offline coverage
  and machine-readable physical requirements still blocking product timecode
  readiness.
- Surface those fields in `scripts/run-cpp-offline-gates`.

Reason:
- The project can currently prove synthetic DVS/timecode behavior offline, but
  it must not blur that with real Traktor/timecode-vinyl readiness.
- Promotion requires functionality evidence as well as quality and CPU
  evidence. Timecode readiness should therefore report both what is already
  covered and what remains absent.

Evidence:
- Focused run:
  - `opena8djcpp_timecode_matrix`: `row_count=8`, `failures=0`, `PASS`.
  - `opena8djcpp_timecode_readiness_gate`: `offline_timecode_pass=true`,
    `product_timecode_ready=false`, `physical_traktor_timecode_blocked=true`.
- Offline coverage includes profiles `timecode-vinyl`, `timecode-cd-line`,
  `phono`, `disabled`, decks `A/B/C/D`, sample rates `44100/48000`, Mode-2
  packet decode, and prepared transport path.
- Remaining physical requirements are `real_traktor_scope_lock`,
  `physical_timecode_vinyl_decks`, `same_session_mainline_cpp_dvs_comparison`,
  and `validated_capture_route`.

Next implication:
- Timecode functionality is better specified offline, but still cannot be
  claimed complete until a lock-gated physical Traktor/timecode run exists on
  a validated capture route.

## 2026-06-17: Keep Route Validity As The Primary Physical Blocker

Decision:
- Do not treat the post-`4118eab` Direct USB physical failure as a C++ packet
  packer failure.
- Keep branch promotion and audiophile readiness blocked on capture-route
  validation and same-session mainline/C++ comparison.

Reason:
- The latest lock-gated Direct USB run failed external iRig quality, but the
  same run's written, consumed, and packed USB diagnostics were bit-aligned to
  the reference with no check errors or panic flags.
- The refreshed direct USB attribution gate classifies the latest failure as
  `post_usb_device_analog_or_capture_route_dominant`.

Evidence:
- Run:
  `local-analysis/direct-usb-soundcheck/20260617T201948Z-post4118eab-irig-route`.
- External capture:
  - `quality_alignment_score=0.699393`;
  - `snr_db_min=-1.877419`;
  - `mid_band_residual_ratio=1.908435`;
  - `high_band_residual_ratio=1.782892`.
- Internal USB:
  - written alignment `1.000000`;
  - consumed alignment `1.000000`;
  - packed USB alignment `1.000000`;
  - USB check errors `0`;
  - USB panic flags `0`.
- Gate refresh:
  - `opena8djcpp_direct_usb_path_attribution`: latest attribution
    `post_usb_device_analog_or_capture_route_dominant`.
  - `opena8djcpp_capture_route_health_gate`:
    `measurement_valid_for_promotion=false`.

Next implication:
- The next improvement must either validate/fix the external capture route or
  produce a same-session physical comparison on a known-valid route.
- No claim of better sound quality, full functionality, timecode vinyl
  readiness, CPU superiority, or branch promotion is allowed from this result.

## 2026-06-17: Promote Direct USB Attribution Into Capture Route Health

Decision:
- Make `opena8djcpp_capture_route_health_gate` consume
  `direct-usb-path-attribution.json`.
- Add `direct_usb_capture_failed_after_clean_payload` as a promotion blocker.
- Emit the required physical experiments from the gate itself.

Reason:
- Direct USB internal cleanliness is strong evidence, but it was previously
  easy to miss unless the operator read a separate attribution JSON.
- Promotion should be blocked by a single route-health surface when a physical
  capture fails after the written/consumed/packed USB payload has already been
  proven clean.

Evidence:
- Focused gate run:
  - `direct_usb_internal_clean=true`;
  - `direct_usb_capture_failed=true`;
  - `direct_usb_capture_failed_after_clean_payload=true`;
  - `direct_usb_attribution=post_usb_device_analog_or_capture_route_dominant`;
  - `measurement_valid_for_promotion=false`.

Next implication:
- Branch promotion cannot proceed until the capture route health gate no longer
  reports `direct_usb_capture_failed_after_clean_payload` and a same-session
  mainline/C++ physical comparison passes on a validated route.

## 2026-06-17: Make Route-Health Evidence Promotion-Critical

Decision:
- Generate Direct USB attribution, soundcheck WAV quality, and physical product
  comparison evidence before running `opena8djcpp_capture_route_health_gate`.
- Make `scripts/evaluate-promotion-readiness.py` consume
  `capture-route-health-gate.json` and `direct-usb-path-attribution.json`.
- Keep offline analyzer `status=PASS` separate from product readiness by
  adding top-level summary fields for diagnostic status, product readiness,
  branch-promotion permission, physical measurement validity, and physical
  blockers.
- Parse Direct USB metrics from the `latest_run` object specifically.

Reason:
- A gate that consumes stale evidence can block or permit promotion for the
  wrong run.
- A promotion evaluator that does not consume route-health evidence can
  accidentally rely on unrelated missing gates for safety.
- Analyzer health and product readiness are different states; the summary must
  expose both without requiring a reader to inspect nested JSON.

Evidence:
- Independent QA/Metrics review by Laplace found the stale-evidence ordering
  bug, missing promotion-evaluator inputs, ambiguous top-level PASS semantics,
  and fragile Direct USB key lookup.

Next implication:
- Promotion now remains blocked even if unrelated physical gates are later
  satisfied, unless route-health explicitly reports
  `measurement_valid_for_promotion=true` and the Direct USB failure-after-clean
  blocker is absent.

## 2026-06-17: Add Historical iRig Route Reference Gate

Decision:
- Add `opena8djcpp_historical_route_reference_gate` as an offline C++ analyzer.
- The gate reads the read-only mainline route proof at
  `/Users/fer/dev/opena8dj/local-analysis/irig-stream-capture/vlc-long-route-proof-20260612-163849/`
  and current C++ route-health artifacts.
- It records historical route sanity separately from current promotion
  validity.

Reason:
- The project has strong evidence that the mixer/REC OUT -> iRig route worked
  in the past, but historical proof cannot substitute for current same-session
  promotion evidence.
- The analyzer prevents two opposite false claims:
  - "the route was never proven";
  - "the route once worked, therefore current C++ quality is ready".

Evidence:
- Historical reference:
  - `rate=48000`;
  - `duration_sec=49.994667`;
  - `active_seconds_count=46`;
  - `clipped_frames=0`;
  - RMS `-23.161776/-24.327619 dBFS`;
  - peaks `-12.005225/-11.446036 dBFS`.
- Current route state:
  - `current_measurement_valid_for_promotion=false`;
  - `current_direct_usb_internal_clean=true`;
  - `current_direct_usb_capture_failed_after_clean_payload=true`;
  - `current_irig_idle_gate_pass=true`.

Next implication:
- The route has a valid historical reference, but current promotion remains
  blocked until a lock-gated known-good non-Audio8 route capture passes and a
  same-session mainline/C++ comparison passes on that validated route.

## 2026-06-17: Promote HAL Candidate Safety Into Readiness Evidence

Decision:
- Run the C++ HAL candidate through `scripts/test-hal-candidate-safety` under
  the global hardware lock.
- Add `opena8djcpp_hal_candidate_safety_gate` to consume that stored evidence.
- Make `scripts/evaluate-promotion-readiness.py` require HAL safety evidence
  as a promotion precondition.

Reason:
- Audio 8 DJ was visible over USB but not CoreAudio when no HAL was loaded.
- Before any physical quality window can be meaningful, the candidate must
  prove that it can install/reload, publish the expected 8x8 CoreAudio device,
  preserve iRig visibility, and unload cleanly.
- This must be explicit evidence, not an operator memory.

Evidence:
- Lock-gated HAL safety run:
  `local-analysis/hal-candidate-safety/20260617T205049Z-cpp-candidate-safety`.
- Result:
  - `hal_candidate_safety=PASS`;
  - `required_device=PASS` for `org.opena8dj.Audio8DJ`;
  - CoreAudio listed `Open Audio 8 DJ` as `in=8 out=8 rate=48000`;
  - iRig Stream remained visible as `in=2 out=2 rate=48000`;
  - post-unload CoreAudio returned to iRig plus built-in devices;
  - lock was released after the run.
- Analyzer output:
  `local-analysis/cpp-offline/hal-candidate-safety-gate.json`.

Next implication:
- HAL/CoreAudio enumeration is no longer the immediate blocker for a controlled
  physical window.
- Product promotion remains blocked by route validation, Direct USB capture
  failure after clean payload, same-session mainline/C++ A/B, CPU, and physical
  Traktor/timecode evidence.

## 2026-06-17: Harden Physical Runner And Reject Simple CPU Knobs

Decision:
- Change `scripts/run-physical-superiority-window` so a failed mainline
  soundcheck does not abort before C++ evidence is collected.
- Treat HAL safety failure as a hard audio blocker for that candidate, but
  continue to cleanup and final summary.
- Reject `HAL_REUSE_ISOC_COMPLETIONS=1/HAL_RAW_ISOC_COMPLETIONS=1`, simple
  playback coalescing, and disabling hot stream stats as product fixes.

Reason:
- The previous runner lost same-session evidence whenever the baseline failed.
  That made the comparison inconclusive.
- A candidate that fails HAL safety must not play audio, but the overall window
  still needs evidence and cleanup.
- CPU and quality evidence shows:
  - raw/reuse did not improve quality or CPU enough and was worse than mainline
    in the current same-session run;
  - coalescing reduces playback submissions and CPU, but breaks physical
    quality badly;
  - disabling hot stream stats does not materially reduce CPU.

Evidence:
- Raw/reuse same-session window:
  `local-analysis/physical-superiority-window/20260617T212050Z-mainline-vs-cpp-raw-reuse-irig`.
- Default same-session window:
  `local-analysis/physical-superiority-window/20260617T213050Z-mainline-vs-cpp-default-irig`.
- Coalescing probes:
  `local-analysis/physical-superiority-window/20260617T214550Z-cpp-coalesce8-irig`,
  `local-analysis/physical-superiority-window/20260617T215000Z-cpp-coalesce2-irig`.
- No-hot-stats probe:
  `local-analysis/physical-superiority-window/20260617T215750Z-cpp-no-hotstats-retry-irig`.

Next implication:
- The next performance fix must redesign capture/playback pacing or transport
  scheduling so CPU drops without changing the audio timing model.
- No branch promotion is allowed; C++ remains below mainline on CPU and below
  product gates on physical quality.

## 2026-06-17: Reject Simple Playback-Completion-Paced Switch

Decision:
- Do not switch the product default to
  `HAL_PLAYBACK_CAPTURE_PACED=0 HAL_PLAYBACK_COALESCE_TRANSFERS=2
  HAL_PLAYBACK_QUEUE=4`.

Reason:
- The probe reduced playback submissions and CPU, but physical quality
  collapsed. That proves the CPU lever exists but the naive timing model is
  wrong.

Evidence:
- `local-analysis/physical-superiority-window/20260617T221500Z-cpp-playback-paced0-coalesce2-q4-irig`.
- Result:
  - quality `0.035313`;
  - SNR floor `-19.04 dB`;
  - lag jumps `41`;
  - driver CPU p95 after 5s about `16.6%`;
  - playback submissions `3354`.

Next implication:
- Implement a deliberate pacing model with target lead in microframes and
  validated packet layout. Do not continue random coalescing/queue tuning as
  product work.

## 2026-06-17: Treat IOUSBHost Enqueue Cadence As The Primary CPU Blocker

Decision:
- Do not pursue further default-candidate changes based only on local pack,
  stats, completion-handler, or transfer-pool micro-optimizations.
- Keep C++ HAL default unpromoted and use the prepared transport / DriverKit
  direction as the next architecture path for CPU reduction.

Reason:
- A lock-gated default C++ HAL soundcheck with `sudo -n sample` captured the
  active driver stack during playback.
- The sample shows the dominant active path on `org.opena8dj.driver.usb` is
  IOUSBHost async enqueue from capture and playback completion paths, not audio
  packing/routing.
- The same run failed audio quality badly, so the profile is diagnostic
  attribution only, not readiness evidence.

Evidence:
- `local-analysis/profiling/20260617T215041Z-default-sudo-sample/soundcheck`.
- `driver-sample/analysis.json`:
  - `dominant_interpretation=usbhost_async_enqueue_from_capture_and_playback_paths`;
  - `usbhost_enqueue=1608`;
  - `iokit_async=795`;
  - `queue_playback=594`;
  - `queue_capture=1286`;
  - `fill_playback=78`.
- Soundcheck:
  - `quality_alignment_score=0.260184`;
  - SNR floor about `-11.89 dB`;
  - `lag_jumps_gt_2_frames=38`;
  - `opena8dj_driver` CPU p95 `23.5%`.

Alternatives discarded:
- Claim CPU root cause from hot-path timing counters alone: rejected because
  process sampling gives stronger attribution to IOUSBHost/Mach async enqueue.
- Keep tuning coalesce/ISO flags directly: rejected because those probes reduce
  submission count by breaking physical quality.
- Promote C++ because offline gates pass: rejected; physical quality and CPU
  superiority remain objectively false.

Next implication:
- The next implementation must reduce IOUSBHost/Mach enqueue cadence or move
  USB scheduling into a prepared DriverKit-style transport without breaking the
  timing/layout assumptions needed for audio quality, routing, and timecode.

## 2026-06-17: Add Opt-In Capture-Paced Playback Refill Experiment

Decision:
- Add, but do not enable by default,
  `HAL_CAPTURE_PACED_PLAYBACK_REFILL`.
- The experiment keeps capture ISO active for input/timecode while refilling the
  playback queue through the existing independent playback queue path.

Reason:
- `HAL_PLAYBACK_CAPTURE_PACED=0` reduced CPU but broke quality, so fully
  detaching playback from capture is rejected.
- The default capture-paced path preserves timing but drives roughly one OUT
  submission per IN completion, producing high IOUSBHost/Mach enqueue CPU.
- This experiment tests the narrower hypothesis: keep capture alive and use
  capture completions as refill opportunities, but allow playback transfers to
  be coalesced and kept in flight.

Evidence before hardware:
- Default HAL build: PASS.
- Experimental build:
  `make -B hal HAL_CAPTURE_PACED_PLAYBACK_REFILL=1
  HAL_PLAYBACK_COALESCE_TRANSFERS=2 HAL_PLAYBACK_QUEUE=4`: PASS.
- Full offline gates after the change:
  - Debug CTest `43/43` passed.
  - Release CTest `44/44` passed.
  - Evidence schema `required_files=44`, `missing_files=0`,
    `summary_pass=true`, `manifest_pass=true`.
  - `branch_promotion_allowed=false`,
    `physical_measurement_valid_for_promotion=false`.

Alternatives discarded:
- A pure prepared-transport bridge with no runtime queue-pressure change:
  rejected as decorative accounting.
- Promoting this experiment because offline gates pass: rejected; hardware
  sound quality, CPU, timecode, and same-session mainline comparison are still
  unproven.

Next implication:
- The experiment may enter a lock-gated HAL candidate safety window, followed
  by short physical soundcheck only if safety passes. It must be rejected if it
  repeats the playback-completion-paced quality collapse or fails to reduce real
  driver CPU/enqueue pressure.

## 2026-06-17: Reject Capture-Paced Playback Refill As Product Candidate

Decision:
- Do not promote or continue tuning
  `HAL_CAPTURE_PACED_PLAYBACK_REFILL=1 HAL_PLAYBACK_COALESCE_TRANSFERS=2
  HAL_PLAYBACK_QUEUE=4` as a product candidate.

Reason:
- The experiment reduced playback submission pressure but collapsed physical
  quality, matching the failure mode of the earlier playback-completion-paced
  probe.
- Capture remained active, so the failure is not explained by disabling
  input/timecode. The independent coalesced OUT refill timing is still wrong
  for this hardware route.

Evidence:
- `local-analysis/physical-superiority-window/20260617T220551Z-cpp-capture-refill-irig`.
- Build hash: `4150dac3b7a1ab74518dc0166cac4928bf2d62e32291f376c6f8d94f32d834d7`.
- HAL candidate safety: PASS.
- Soundcheck:
  - `quality_alignment_score=0.157019`;
  - SNR `-21.74 dB`;
  - `lag_jumps_gt_2_frames=35`;
  - no clipping.
- CPU:
  - `opena8dj_driver` median `16.1%`, p95 `18.2%`, max `18.3%`;
  - `coreaudiod` median `2.2%`, p95 `28.9%`, max `42.1%`;
  - total watched audio/UI median `18.9%`, p95 `47.4%`.
- Stream stats:
  - playback submissions `3351`;
  - capture completions `12139`;
  - zero transfer-pool fallback allocations;
  - zero output underruns / active underruns / elastic drops.
- Driver sample:
  - `queue_playback=206`, down from `594` in the default profile;
  - `usbhost_enqueue=1259`, down from `1608`;
  - capture requeue remains dominant (`queue_capture=1117`).
- Final guard:
  - `opena8dj_state=unloaded`;
  - `opena8dj_driver_pids=none`;
  - `audio_stack_health=PASS`.

Next implication:
- Do not spend more iterations on independent coalesced playback refill inside
  the HAL. The next viable direction must preserve the hardware's physical
  timing/layout more closely, or move to a lower-overhead prepared
  DriverKit/USBDriverKit transport that reduces enqueue cadence without
  changing the audible timing contract.

## 2026-06-17: Treat DriverKit Timing And Memory Contracts As Offline Gates

Decision:
- The C++ DriverKit shell must fail offline if it does not model stream memory,
  zero timestamps, and stopped-only configuration changes.

Reason:
- Apple AudioDriverKit architecture makes `IOUserAudioDriver`,
  `IOUserAudioDevice`, `IOUserAudioStream`, mapped memory, start/stop IO, zero
  timestamps, and configuration-change sequencing part of the actual driver
  contract, not optional glue.
- The current CPU blocker is real USB enqueue cadence, so a DriverKit shell that
  only renames HAL concepts would be misleading.
- Encoding these requirements in pure C++ gates lets the project advance
  safely while the local machine lacks a DriverKit SDK and while no system
  extension installation is authorized.

Evidence:
- `opena8djcpp_driverkit_runtime_contract` now reports:
  - `io_memory_descriptors=5`;
  - `io_memory_total_bytes=4096`;
  - `zero_timestamp_updates=2`;
  - `zero_timestamp_regressions=0`;
  - negative timestamp test: `timestamp_reject_regressions=2`;
  - `configuration_changes=1`;
  - `rejected_configuration_changes=1`;
  - 44.1 kHz and 48 kHz pressure rows PASS.
- `opena8djcpp_driverkit_extension_scaffold_contract` now requires scaffold
  references to `IOMemoryDescriptor`, `UpdateCurrentZeroTimestamp`,
  `GetCurrentZeroTimestamp`, `RequestDeviceConfigurationChange`, and
  `PerformDeviceConfigurationChange`.
- Full offline gates after the change:
  - Debug CTest `43/43` passed.
  - Release CTest `44/44` passed.
  - Evidence schema `required_files=44`, `missing_files=0`,
    `summary_pass=true`, `manifest_pass=true`.

Alternatives discarded:
- Leave DriverKit timing/memory semantics only in documentation: rejected
  because it would not prevent a future runtime bridge from regressing.
- Install or activate a dext now: rejected because the local SDK/signing state
  is incomplete and no physical install window was requested for this change.

Next implication:
- Future DriverKit work must wire the real dext to these semantics instead of
  weakening the contract. Product superiority remains blocked by physical
  quality, CPU, route validation, and timecode evidence.

## 2026-06-17: Add Saved-Tone Audiophile Sideband Gate

Decision:
- Add `opena8djcpp_audiophile_tone_gate` as an offline evidence artifact and
  CTest.

Reason:
- Music soundcheck alignment/SNR is necessary but not sufficient for
  audiophile-quality claims. A tone-specific gate can catch sideband modulation,
  harmonic distortion, clipping, and residual energy in saved 1 kHz captures.
- The gate must remain honest: PASS means the saved candidate tone clears the
  local threshold and is not worse than the selected historical mainline tone on
  THD/sidebands. It does not validate the current route or promote the product.

Evidence:
- `local-analysis/cpp-offline/audiophile-tone-gate.json`.
- Candidate saved tone:
  - `candidate_threshold_pass=true`;
  - sideband ratio `0.010323`;
  - THD ratio `0.000451`;
  - clipped frames `0`.
- Selected baseline saved tone:
  - `baseline_threshold_pass=false`;
  - sideband ratio `0.013758`;
  - THD ratio `0.000874`.
- Full offline gates after artifact integration:
  - Debug CTest `44/44` passed.
  - Release CTest `45/45` passed.
  - Evidence schema `required_files=45`, `missing_files=0`.

Alternatives discarded:
- Treat tone PASS as product readiness: rejected; current route and music gates
  still fail promotion validity.
- Use a weaker intermediate mainline tone as baseline: rejected after it failed
  the sideband threshold more clearly than the 0.3.24 final tone.

Next implication:
- Future physical windows must collect both real-music and tone evidence. Tone
  sideband wins only matter if the same candidate also beats mainline on route
  health, music quality, CPU, routing, and Traktor/timecode.

## 2026-06-17: Next Low-CPU Direction Is Logical ISO8 Prepared USB Batching

Decision:
- The next implementation direction is a real prepared USB slot scheduler:
  preserve logical ISO8 audio slots while reducing real USB submit/enqueue work
  below that layer.

Reason:
- Direct HAL coalescing, independent playback refill, and playback-completion
  pacing reduced some CPU pressure but broke physical quality.
- Mainline's low CPU is associated with larger effective USB cadence, but
  naive ISO64-style behavior is not safe for this hardware route.
- The safer target is to keep packet/routing/timecode/timestamp semantics at
  ISO8 while batching prepared USB submits underneath that logical cadence.

Required counters for the next offline/runtime model:
- `logical_audio_periods`.
- `usb_submit_calls`.
- `backend_slot_completions`.
- `slot_order_errors`.
- `timestamp_regressions`.
- `hal_steady_requeues`.
- fallback allocations and ring overrun/underrun counters.

Rejected paths:
- Explicit scheduling sweeps.
- Raw/reuse pool cursor retests.
- Independent playback coalescing/refill inside the HAL.
- Decorative prepared-transport bridge that does not reduce real
  `enqueueIORequestWithData` cadence.

Next implication:
- The next patch should extend prepared slot scheduler contracts so
  `logical_audio_gap_ratio` remains safe while `usb_submit_calls` is strictly
  lower than logical audio periods. Only then should a lock-gated physical
  candidate be considered.

## 2026-06-17: Gate Low-CPU Path On Logical ISO8 USB Submit Reduction

Decision:
- Extend `PreparedSlotScheduler` with explicit logical-slot and USB-submit
  accounting.
- Require the migration gate to prove a safe scenario with `8x` modeled USB
  submit reduction while keeping logical audio gap ratio at `1.0`.

Reason:
- The previous failed probes reduced queue pressure by changing audible timing.
  The next viable design must reduce submit/enqueue pressure below the logical
  audio layer, while preserving logical ISO8 cadence, ordering, routing,
  timestamps, and timecode.
- This prevents a future runtime bridge from claiming progress if it merely
  moves accounting around while still submitting one USB request per logical
  slot.

Evidence:
- `opena8djcpp_prepared_slot_scheduler_contract`: PASS.
- Safe batching row:
  - scenario `prepared_iso8_usb_batch8_stable`;
  - `logical_audio_periods=256`;
  - `backend_slot_completions=512`;
  - `usb_submit_calls=66`;
  - `usb_submit_reduction_ratio=8`;
  - zero HAL steady requeues, fallback allocations, logical gap violations, and
    slot order errors.
- `opena8djcpp_prepared_transport_migration_gate`: PASS, including
  `logical_iso8_usb_submit_batching_supported=PASS`.
- Full offline gates:
  - Debug CTest `44/44` passed.
  - Release CTest `45/45` passed.
  - Evidence schema `required_files=45`, `missing_files=0`.

Alternatives discarded:
- Treat old prepared-slot PASS as enough: rejected because it did not prove
  reduced real USB submit cadence.
- Re-open independent HAL playback coalescing: rejected because the physical
  soundcheck failure is already decisive.

Next implication:
- The next implementation may target a runtime adapter only if it can expose
  the same counters: logical periods, USB submit calls, backend slot
  completions, order errors, timestamp regressions, HAL requeues, and fallback
  allocations.

## 2026-06-17: Require Runtime-Facing Batching Counters Before Hardware Path

Decision:
- Add a pure C++ fake runtime adapter around `PreparedSlotScheduler`.
- Require `prepared-transport-migration-gate` to read
  `runtime-adapter-contract.json` and pass
  `runtime_adapter_batched_submit_counters_exposed`.

Reason:
- The scheduler contract proves the model, but the next DriverKit/USB binding
  needs runtime-facing counters, not private scheduler-only accounting.
- This prevents a future runtime bridge from hiding one-submit-per-logical-slot
  behavior behind a green scheduler-only test.

Evidence:
- `opena8djcpp_runtime_adapter_contract`: PASS.
- Stable row:
  - `logical_audio_periods=256`;
  - `backend_slot_completions=512`;
  - `usb_submit_calls=66`;
  - `usb_submit_reduction_ratio=8`.
- Negative rows reject unbatched submits, logical gaps, slot-order errors, HAL
  requeues, and fallback allocations.
- Full offline gates:
  - Debug CTest `45/45` passed.
  - Release CTest `46/46` passed.
  - Evidence schema `required_files=46`, `missing_files=0`.

Next implication:
- The next non-offline implementation must bind these counters to the real
  DriverKit/USB path. Product superiority still requires lock-gated physical
  same-session evidence against mainline.

## 2026-06-17: Require Ordered USB Submit Descriptor Plan

Decision:
- Add `PreparedUsbSubmitPlanner` as a pure C++ core model for batched USB
  submit descriptors.
- Require `prepared-transport-migration-gate` to pass
  `usb_submit_descriptor_plan_safe` before a low-CPU hardware candidate is
  allowed.

Reason:
- Runtime counters alone can still be decorative if they do not prove how
  logical capture/playback slots map into physical USB submit descriptors.
- The next DriverKit/USB binding needs a contract for slot sequence, direction,
  timestamps, byte counts, partial submit rejection, and descriptor overflow
  rejection.

Evidence:
- `opena8djcpp_usb_submit_plan_contract`: PASS.
- Stable row:
  - `stable_logical_slots=528`;
  - `stable_usb_submit_calls=66`;
  - `stable_usb_submit_reduction_ratio=8`;
  - `stable_total_bytes=185856`;
  - capture/playback submits split as `33/33`.
- Negative rows reject unbatched submits, slot-order errors, timestamp
  regressions, and partial flushes.
- Full offline gates:
  - Debug CTest `46/46` passed.
  - Release CTest `47/47` passed.
  - Evidence schema `required_files=47`, `missing_files=0`.

Next implication:
- The real runtime adapter must allocate and recycle actual DriverKit/USB
  descriptors according to this plan; until then, CPU superiority remains an
  offline hypothesis, not a product claim.

## 2026-06-17: Require USB Submit Payload Contract

Decision:
- Add `opena8djcpp_usb_submit_payload_contract`.
- Require `prepared-transport-migration-gate` to pass
  `usb_submit_payload_plan_safe` before the prepared transport path can be
  treated as a hardware-candidate direction.

Reason:
- Descriptor counts alone were not enough. The previous model could count
  logical ISO8 slots correctly while under-describing the Mode2 payload frame
  capacity required by a 352-byte transfer.
- The contract now keeps two facts separate:
  - logical timestamps advance by ISO8 slots;
  - USB payload descriptors carry Mode2 payload frames derived from bytes.

Evidence:
- `opena8djcpp_usb_submit_payload_contract`: PASS.
- Stable payload contract:
  - `descriptors=66`;
  - `capture_descriptors=33`;
  - `playback_descriptors=33`;
  - `total_bytes=185856`;
  - `total_frames=5808`;
  - `check_errors=0`;
  - `panic_flags=0`;
  - `output_overflows=0`;
  - `prefix_mismatches=0`;
  - descriptor, direction, and timestamp mismatches all `0`.
- Full offline gates:
  - Debug CTest `47/47` passed.
  - Release CTest `48/48` passed.
  - Evidence schema `required_files=48`, `missing_files=0`.

Alternatives discarded:
- Keep `frame_count` as logical `slot_count * frames_per_slot`: rejected
  because it caused payload decode overflow in the descriptor payload gate.
- Treat the payload contract as optional: rejected because CPU reduction is
  only meaningful if batched descriptors still carry the full Mode2 payload.

Next implication:
- A real DriverKit/USB backend must submit payloads that satisfy both the
  logical ISO8 cadence contract and the Mode2 payload contract. Product
  superiority still requires lock-gated physical same-session A/B evidence
  against mainline.

## 2026-06-17: Bind DriverKit Shell To Prepared USB Submit Plan

Decision:
- Add prepared USB submit binding state to `AudioDriverSkeleton`.
- Add `opena8djcpp_driverkit_usb_submit_binding_contract`.
- Require `prepared-transport-migration-gate` to pass
  `driverkit_usb_submit_binding_safe`.

Reason:
- Core-only USB submit tests can still be bypassed by the DriverKit shell.
- The future `IOUserAudioDriver` / USB backend needs the same logical ISO8,
  descriptor, payload, timestamp, and lifecycle contract at the runtime
  boundary before any hardware window is justified.

Evidence:
- `opena8djcpp_driverkit_usb_submit_binding_contract`: PASS.
- Stable DriverKit binding:
  - `periods=256`;
  - `transport_frame_mismatches=0`;
  - `transport_safe=true`;
  - `usb_submit_safe=true`;
  - `logical_slots=528`;
  - `usb_submit_calls=66`;
  - `descriptors=66`;
  - `capture_descriptors=33`;
  - `playback_descriptors=33`;
  - `total_bytes=185856`;
  - `total_frames=5808`;
  - `check_errors=0`;
  - `panic_flags=0`;
  - `output_overflows=0`;
  - payload/direction/timestamp mismatches all `0`;
  - `stopped=true`.
- Full offline gates:
  - Debug CTest `48/48` passed.
  - Release CTest `49/49` passed.
  - Evidence schema `required_files=49`, `missing_files=0`.

Alternatives discarded:
- Keep the DriverKit shell as a frame-only transport bridge: rejected because it
  would not prove the low-CPU USB submit plan survives the runtime boundary.
- Treat the core USB submit contract as sufficient: rejected because product
  risk is at the binding between runtime callbacks, prepared slots, and actual
  USB submission.

Next implication:
- The next implementation step can move from offline DriverKit shell binding
  toward a real DriverKit/USBDriverKit submit adapter. Product superiority and
  branch promotion remain blocked until lock-gated same-session physical A/B
  proves quality, routing, timecode vinyl, CPU, and recovery against mainline.

## 2026-06-17: Require DriverKit USB Request Lifecycle Pool

Decision:
- Add `PreparedUsbRequestPool` as a pure C++ model of preallocated USB request
  submit, completion, recycle, and stale-handle rejection.
- Add `opena8djcpp_driverkit_usb_request_lifecycle_contract`.
- Require `prepared-transport-migration-gate` to pass
  `driverkit_usb_request_lifecycle_safe`.

Reason:
- Submit descriptor correctness is not enough for physical-readiness planning.
  A real DriverKit/USBDriverKit path can still glitch if requests are allocated
  dynamically, leaked, completed out of lifecycle, or recycled unsafely.
- The future hardware candidate needs bounded request pressure before any
  locked physical window is worth the risk.

Evidence:
- `opena8djcpp_driverkit_usb_request_lifecycle_contract`: PASS.
- Stable lifecycle row:
  - `source_descriptors=66`;
  - `stable_submit_calls=66`;
  - `stable_completion_calls=66`;
  - `stable_recycle_calls=66`;
  - `stable_max_live_requests=4`;
  - `stable_submitted_bytes=185856`;
  - `stable_completed_bytes=185856`;
  - `stable_submitted_frames=5808`;
  - `stable_completed_frames=5808`;
  - fallback allocations, invalid completions, stale completions, and live
    requests all `0`.
- Negative rows reject request-pool pressure and stale completions.
- Full offline gates:
  - Debug CTest `49/49` passed.
  - Release CTest `50/50` passed.
  - Evidence schema `required_files=50`, `missing_files=0`.

Alternatives discarded:
- Rely on descriptor counts only: rejected because descriptors do not prove
  request object lifetime, recycling, or bounded in-flight pressure.
- Allow fallback allocation on pressure: rejected because the real-time path
  must not allocate or repair missing request objects during audio flow.

Next implication:
- A real DriverKit/USBDriverKit submit adapter must map these modeled request
  slots to real async USB request objects and preserve the same counters.
  Product superiority still requires lock-gated physical same-session quality,
  routing, timecode vinyl, CPU, and recovery evidence against mainline.

## 2026-06-17: Require USB Request Shutdown/Cancel/Restart Contract

Decision:
- The DriverKit shell now owns the prepared USB request pool during stream
  runtime.
- Stream stop distinguishes normal completions from shutdown cancellations.
- Added `opena8djcpp_driverkit_usb_request_shutdown_contract`.
- `prepared-transport-migration-gate` now requires
  `driverkit_usb_request_shutdown_safe=PASS`.

Reason:
- A physical USB adapter can fail during stop/restart even if steady-state
  submit/complete/recycle accounting is correct.
- Treating live stop requests as normal completions would hide lifecycle bugs
  and overstate hardware readiness.

Evidence:
- Focal gate PASS:
  - `inflight_requests_at_stop=3`;
  - `cancelled_requests=3`;
  - `live_requests_after_stop=0`;
  - `submit_calls=4`;
  - `completion_calls=1`;
  - `recycle_calls=4`;
  - `submitted_frames=352`;
  - `completed_frames=88`;
  - `cancelled_frames=264`;
  - `restart_after_cancel_safe=true`;
  - `late_completion_rejected=true`.
- Migration gate PASS now includes
  `driverkit_usb_request_shutdown_safe=PASS`.

Alternatives discarded:
- Drain live requests as if they completed normally during stop: rejected
  because it hides cancellation behavior and creates misleading accounting.
- Leave shutdown behavior to real hardware testing: rejected because this
  invariant is cheap and safer to prove offline first.

Next implication:
- The next USB/DriverKit adapter must map cancellation and late completion
  behavior to real async request objects. This does not prove physical audio
  quality, CPU superiority, or timecode readiness.

## 2026-06-17: Separate Route-Revalidation Readiness From Product Readiness

Decision:
- Added `opena8djcpp_physical_window_readiness_gate`.
- A PASS allows only planning a lock-gated known-good non-Audio8 route
  revalidation window.
- Product physical A/B and branch promotion remain forced false until the route
  is current-valid and same-session mainline-vs-C++ physical comparison passes.

Reason:
- Existing evidence proves the digital/internal path can be clean while the
  physical capture route remains invalid for promotion.
- Without a current valid route, any sound-quality or CPU-superiority claim can
  be contaminated by iRig/capture-route instability.

Evidence:
- `opena8djcpp_physical_window_readiness_gate`: PASS.
- The gate now consumes source artifacts directly for product-blocked,
  route-invalid, direct-USB-blocking, historical-route-stale, HAL-safety,
  migration, and lock-policy decisions. It does not use
  `current-offline-gates.json` as a decision source.
- `ready_for_route_revalidation_window=true`.
- `ready_for_product_physical_ab=false`.
- `ready_for_branch_promotion=false`.
- Full offline gates: Debug CTest `51/51`, Release CTest `52/52`, evidence
  schema `required_files=52`, `missing_files=0`.

Alternatives discarded:
- Let HAL safety PASS authorize a product A/B window: rejected because HAL
  enumeration safety is not sound-quality evidence.
- Reuse historical iRig route evidence for promotion: rejected because it is
  not current same-session evidence after the later route failures.
- Treat direct USB internal-clean evidence as product readiness: rejected
  because the same direct USB run still showed failed physical capture.

Next implication:
- The next physical action, if authorized, must be route revalidation only:
  known-good non-Audio8 source into the same iRig capture route under the
  hardware lock. Mainline-vs-C++ A/B, timecode vinyl, and branch promotion
  remain blocked until that route passes.

## 2026-06-17: Reject Built-In Speakers as Wired Route Evidence

Decision:
- `scripts/physical-window-preflight`,
  `scripts/run-known-good-route-soundcheck`, and
  `scripts/run-physical-superiority-window` now reject built-in speaker /
  acoustic output sources by default for known-good route validation.
- Added `--allow-built-in-output-acoustic-diagnostic` as an explicit diagnostic
  escape hatch only.
- `opena8djcpp_hardware_lock_policy_check` now requires this protection.

Reason:
- The currently visible non-Audio8 output is `MacBook Air Speakers`, which is
  not a wired source into the mixer/REC OUT -> iRig route.
- Accepting built-in speakers would let room leakage or silence masquerade as
  capture-route evidence and could poison later sound-quality and CPU claims.

Evidence:
- Current CoreAudio enumeration: `iRig Stream`, `MacBook Air Microphone`, and
  `MacBook Air Speakers`.
- `scripts/physical-window-preflight --route-only ... --known-good-output-device "MacBook Air Speakers"` exits `1` with failing gate
  `known_good_output_not_builtin_acoustic`.
- `scripts/run-physical-superiority-window --execute --route-only ... --known-good-output-device "MacBook Air Speakers"` exits `2` at preflight before lock acquisition.
- `scripts/run-known-good-route-soundcheck --output-device "MacBook Air Speakers" ...` exits `2` before playback/capture.

Alternatives discarded:
- Run an acoustic speaker-to-iRig test as route proof: rejected because it does
  not test the wired shared capture route.
- Reuse the same iRig device as output and capture: already proven diagnostic
  only and not usable for promotion.

Next implication:
- The next real physical route revalidation still requires a separate
  non-Audio8 output physically wired into the same capture chain, or a current
  valid Audio 8/mainline route after HAL loading followed by same-session A/B.

## 2026-06-17: Structured Reads for Physical-Window Readiness Evidence

Decision:
- Added `tools/evidence_json.hpp` and `opena8djcpp_evidence_json_contract`.
- Refactored `opena8djcpp_physical_window_readiness_gate` to validate critical
  source artifacts through structured JSON field reads:
  `result`, `safety`, `branch_promotion_allowed`,
  `measurement_valid_for_promotion`, `digital_payload_clean`,
  `shared_route_unhealthy`, direct-USB `latest_run`, promotion blockers,
  required physical experiments, migration gates, and hardware-lock paths.

Reason:
- Promotion readiness is a high-risk decision surface. Broad string matching
  can pass if a stale, nested, or unrelated field happens to contain the right
  text.
- The gate should fail unless the expected fields and arrays are present in the
  expected artifacts.

Evidence:
- `opena8djcpp_evidence_json_contract`: PASS.
- `opena8djcpp_physical_window_readiness_gate`: PASS.
- Full offline gates after the change: Debug CTest `52/52`, Release CTest
  `53/53`, evidence schema `required_files=52`, `missing_files=0`.

Alternatives discarded:
- Add a third-party JSON dependency: rejected for this narrow offline tooling
  path; the needed shape is small and deterministic.
- Keep broad text matching: rejected because it weakens blocker integrity for
  sound-quality, CPU, timecode, and branch-promotion claims.

Next implication:
- Future readiness gates should prefer structured reads for critical fields.
  Text matching is acceptable only for narrow diagnostics that cannot authorize
  hardware windows, product readiness, or branch promotion.

## 2026-06-17: Structured Reads for Offline Summary Schema

Decision:
- Refactored `opena8djcpp_evidence_schema_check` to use the shared evidence
  JSON reader for critical fields in `current-offline-gates.json` and
  `docs/CANDIDATE_MANIFEST.json`.

Reason:
- The summary schema check is the final offline evidence integrity gate. It
  should prove the actual fields that matter for safety and readiness, not just
  that matching text exists somewhere in the file.

Evidence:
- Focused `opena8djcpp_evidence_schema_check`: PASS.
- Full offline gates after the change: Debug CTest `52/52`, Release CTest
  `53/53`, evidence schema `required_files=52`, `missing_files=0`,
  `summary_pass=true`, `manifest_pass=true`.

Alternatives discarded:
- Leave summary validation as broad string search: rejected because the summary
  carries hard blockers and no-touch safety flags.
- Make schema validation exhaustive: deferred because the targeted critical
  fields cover the readiness risk without turning this small C++ tool into a
  full schema engine.

Next implication:
- Any future field that gates hardware windows, product readiness, branch
  promotion, or no-touch safety should be added to structured schema checks.

## 2026-06-17: Evidence Parser Contract Must Leave an Artifact

Decision:
- `scripts/run-cpp-offline-gates` now writes
  `local-analysis/cpp-offline/evidence-json-contract.json`.
- `opena8djcpp_evidence_schema_check` requires that artifact.

Reason:
- The evidence parser protects readiness gates. Its own contract should be
  visible in the evidence bundle instead of existing only inside CTest output.

Evidence:
- Focused schema check: PASS with `required_files=53`, `missing_files=0`.
- Full offline gates: Debug CTest `52/52`, Release CTest `53/53`, evidence
  schema `required_files=53`, `missing_files=0`.

Alternatives discarded:
- Keep the parser contract only as a CTest entry: rejected because the project
  relies on explicit evidence files for later physical-window and promotion
  decisions.

Next implication:
- New critical offline contracts should leave machine-readable evidence files
  when they materially affect readiness decisions.

## 2026-06-17: Promotion Evidence Must Be One Physical Bundle

Decision:
- Added `single_physical_promotion_evidence_bundle` to the promotion evaluator.
- Added `single_physical_promotion_evidence_bundle_missing` to offline hard
  blockers.

Reason:
- Independent physical artifacts can come from different routes, driver builds,
  candidate parameters, or capture conditions. Combining the best artifact from
  each run can create a false superiority claim.
- Promotion requires a single lock-gated physical window with one route,
  candidate, baseline, and run context.

Evidence:
- Focused evaluator run:
  `local-analysis/promotion-readiness-after-bundle-integrity.json`, expected
  FAIL with `product_window=null` and
  `single_physical_promotion_evidence_bundle=FAIL`.
- Full offline gates after integration: Debug CTest `52/52`, Release CTest
  `53/53`, evidence schema `required_files=53`, `missing_files=0`.

Alternatives discarded:
- Keep only music/CPU same-folder pairing: rejected because latency, marker,
  USB integrity, matrix, tone, and same-session comparison could still be
  selected from unrelated windows.
- Rely on docs to warn about mixed evidence: rejected because promotion gating
  must encode this as machine-checkable policy.

Next implication:
- The next valid promotion window must write all promotion-critical artifacts
  under one `local-analysis/physical-superiority-window/<id>` directory.

## 2026-06-17: Diagnostic PASS Must Not Mean Product Readiness

Decision:
- Added `opena8djcpp_diagnostic_pass_semantics_gate`.
- Added its machine-readable artifact to the full offline evidence bundle.
- Updated the evidence schema check to validate nested structured summary
  fields for prepared transport, DriverKit USB request counters, and
  physical-window readiness.

Reason:
- Several gates correctly return PASS for analyzer health or offline coverage,
  while product readiness is still false. That is acceptable only if the
  evidence itself makes the distinction impossible to miss.

Evidence:
- Full offline gates after integration: Debug CTest `53/53`, Release CTest
  `54/54`, evidence schema `required_files=54`, `missing_files=0`,
  `summary_pass=true`, `manifest_pass=true`.
- `local-analysis/cpp-offline/diagnostic-pass-semantics-gate.json` reports
  `blocked_claim=DIAGNOSTIC_PASS_DOES_NOT_MEAN_PRODUCT_READINESS_OR_BRANCH_PROMOTION`.

Alternatives discarded:
- Rely on prose-only warnings: rejected because readiness claims must be
  machine-checkable.
- Treat diagnostic PASS as failure: rejected because analyzer health is still
  useful evidence when its limited meaning is explicit.

Next implication:
- New analyzer gates that can PASS before product readiness must either be
  covered by this semantics gate or expose equivalent blocked-claim fields.

## 2026-06-17: Tone Evidence Alone Cannot Authorize Audiophile Quality

Decision:
- Added `opena8djcpp_product_quality_claim_gate`.
- Added `local-analysis/cpp-offline/product-quality-claim-gate.json` to the
  full offline evidence bundle and schema requirements.

Reason:
- A narrow saved-tone gate can pass while real-music capture, route validity,
  CPU, and same-session promotion evidence remain blocked. The project needs a
  machine-readable quality-claim guard that ties those facts together.

Evidence:
- Full offline gates after integration: Debug CTest `54/54`, Release CTest
  `55/55`, evidence schema `required_files=55`, `missing_files=0`,
  `summary_pass=true`, `manifest_pass=true`.
- Product quality claim gate reports `quality_claim_allowed=false` with
  blockers for analyzer-only soundcheck, missing real-music superiority,
  invalid route, non-current tone measurement, and branch promotion not allowed.

Alternatives discarded:
- Strengthen the tone gate only: rejected because tone is necessary but not
  sufficient for real music or Traktor/timecode claims.
- Rely on promotion evaluator alone: rejected because quality claim semantics
  should be explicit in the offline evidence summary.

Next implication:
- Future physical windows must make `quality_claim_allowed=true` before any
  statement that C++ has better audiophile quality than mainline.

## 2026-06-17: Evidence Must Match Current Candidate Commit

Decision:
- Added `opena8djcpp_evidence_provenance_freshness_gate`.
- Added a promotion evaluator gate named
  `candidate_evidence_matches_claimed_commit`.
- The full offline runner now records worktree dirtiness, runs the provenance
  gate after writing `current-offline-gates.json`, and embeds the result back
  into the summary.
- The runner then reruns promotion evaluation and updates
  `promotion_readiness_evaluator` so the final summary does not preserve a
  stale pre-provenance promotion result.

Reason:
- A subagent audit found that schema PASS could remain attached to a stale
  `base_commit`. That would allow a false claim that the current C++ candidate
  has evidence generated for a previous commit.

Evidence:
- Focused build of `opena8djcpp_evidence_provenance_freshness_gate`: PASS.
- Full offline gates after integration: Debug CTest `54/54`, Release CTest
  `55/55`, evidence schema `required_files=56`, `missing_files=0`,
  `summary_pass=true`, `manifest_pass=true`.
- Provenance gate reports `summary_matches_head=true`,
  `working_tree_clean_for_claim=true`, and `claimable_current_candidate=true`.

Alternatives discarded:
- Trust commit hashes in prose: rejected because readiness claims must be
  machine-checkable.
- Require every historical artifact to carry a candidate commit immediately:
  deferred because many old physical artifacts predate this contract; the
  current offline summary and promotion evaluator are the first hard boundary.

Next implication:
- Promotion is impossible unless the offline evidence bundle is fresh for HEAD.

## 2026-06-18: Block HAL Runtime Superiority Claims Until Real USB Submit Reduction Exists

Decision:
- Added `opena8djcpp_hal_transport_runtime_gate`.
- Added `local-analysis/cpp-offline/hal-transport-runtime-gate.json` to the
  full offline evidence bundle, summary, CTest, and schema requirements.
- Treat the current HAL candidate as a safety/loadable diagnostic candidate,
  not a CPU-superiority candidate, while `OpenA8DJUSB.m` still performs direct
  `enqueueIORequestWithData` work from capture/playback paths.

Reason:
- Existing prepared transport contracts model an 8x USB submit reduction, but
  the loadable HAL runtime still has the direct IOUSBHost enqueue hot path.
- Physical sampling attributes current driver CPU to IOUSBHost/Mach enqueue
  work. A model that is not wired to real enqueue cadence cannot prove lower
  CPU or audiophile quality.
- Prior coalescing/refill probes reduced CPU but damaged physical quality, so
  a simple HAL cadence knob is not acceptable as product work.

Evidence:
- Focused gate run:
  `opena8djcpp_hal_transport_runtime_gate` reports
  `runtime_reduction_missing=true`, `hal_has_direct_usb_enqueue=true`,
  `hal_has_no_runtime_prepared_submit=true`,
  `offline_usb_submit_reduction_ratio=8`, and
  `product_claim_blocked=true`.
- The first full offline run after adding the gate intentionally failed
  provenance because the worktree was dirty; that confirms current-candidate
  claims remain blocked until this change is committed and rerun cleanly.
- Local toolchain check: `xcrun --sdk driverkit --show-sdk-path` fails because
  this machine currently has Command Line Tools only, not a DriverKit SDK.

Alternatives discarded:
- Claim the prepared transport model as runtime performance evidence:
  rejected because it does not reduce actual HAL IOUSBHost enqueue calls.
- Keep tuning HAL coalesce/refill knobs: rejected because prior physical runs
  already showed CPU reduction with unacceptable quality collapse.
- Install a macOS full installer as a substitute for DriverKit: rejected; the
  missing dependency is Xcode/DriverKit SDK, not a macOS reinstall.

Next implication:
- The next real implementation must either integrate a runtime path that
  actually reduces USB enqueue/submit cadence, or move to a full
  DriverKit/USBDriverKit build environment with the proper SDK.

## 2026-06-18: DriverKit SDK Availability Is a Promotion Blocker

Decision:
- Added `opena8djcpp_driverkit_sdk_preflight_gate`.
- Added `local-analysis/cpp-offline/driverkit-sdk-preflight-gate.json` to the
  full offline evidence bundle, summary, CTest, and schema requirements.
- Added `real_driverkit_sdk_and_selected_xcode_missing` to promotion hard
  blockers while this host lacks full Xcode and DriverKit SDK.

Reason:
- The C++ line has a DriverKit scaffold and offline DriverKit-facing contracts,
  but this host currently cannot compile a real DriverKit dext because the
  selected developer directory is Command Line Tools and `xcrun --sdk driverkit`
  fails.
- A real DriverKit candidate must be measurable as a build artifact, not only a
  source skeleton.

Evidence:
- Focused gate run reports `xcrun_driverkit_sdk_available=false`,
  `xcode_select_path=/Library/Developer/CommandLineTools`,
  `selected_full_xcode=false`, `xcode_app_present=false`,
  `xcodes_cli_present=true`, `xcodes_cli_usable=true`,
  `xcodes_installed_count=0`, `aria2_present=false`, and
  `real_driverkit_claim_blocked=true`.
- `xcodes list` shows `26.5 (17F42) [Apple Silicon]` as available for this
  host, but it is not installed.

Alternatives discarded:
- Treat the DriverKit scaffold as a real dext candidate: rejected because it
  does not build/link against AudioDriverKit/DriverKit on this machine.
- Start a multi-gigabyte Xcode install without first recording the blocker:
  rejected because the product evidence must explain why DriverKit readiness is
  currently blocked and what exact condition clears it.

Next implication:
- Before any real DriverKit/dext build claim, install and select full Xcode with
  DriverKit SDK, rerun this preflight gate, then build the DriverKit target into
  the build directory only. Installation/activation still requires a separate
  lock-gated window.

## 2026-06-18: Do Not Attempt Full Xcode Install Until Disk Gate Passes

Decision:
- Installed `aria2` so `xcodes` can use the faster download path.
- Extended `opena8djcpp_driverkit_sdk_preflight_gate` to measure
  `/Applications` free space and a conservative `80 GiB` minimum before a full
  Xcode install attempt.
- Treat `noninteractive_xcode_install_prerequisites_met=false` as a toolchain
  blocker for installing Xcode on this host.

Reason:
- Installing full Xcode is a large system-level developer-tool change. Starting
  it with insufficient disk risks partial installs, failed expansion, or cleanup
  work that does not advance driver quality.
- The current machine has enough tooling to start a download, but not enough
  measured free space to do it safely.

Evidence:
- Focused preflight after installing `aria2` reports `aria2_present=true`,
  `fast_download_helper_present=true`, `applications_free_gib=12.641`,
  `xcode_install_minimum_free_gib=80.000`,
  `xcode_install_disk_space_ok=false`, and
  `noninteractive_xcode_install_prerequisites_met=false`.

Alternatives discarded:
- Start `xcodes install 26.5` immediately: rejected because the free-space
  gate fails.
- Keep the blocker as prose only: rejected because future readiness claims must
  be based on executable evidence.

Next implication:
- Free space or provide a build host with full Xcode/DriverKit SDK before the
  real DriverKit build target can replace the offline scaffold as a runnable
  dext candidate.

## 2026-06-18: Split Logical ISO8 Cadence From Opt-In Batched Capture Transfers

Decision:
- Added `OPENA8DJ_CAPTURE_ISO_FRAMES_PER_TRANSFER` and `HAL_CAPTURE_ISO_FRAMES`
  as an opt-in capture physical-transfer size.
- Kept the default capture size equal to `HAL_ISO_FRAMES`, preserving the
  existing ISO8 logical cadence by default.
- Sized capture transfer pools, capture queue requests, capture cadence timing,
  and capture-paced playback request buffering from
  `kCaptureIsoFramesPerTransfer`.
- Kept playback chunking through `kPlaybackIsoFramesPerTransfer`, so a larger
  capture completion can feed multiple logical playback batches instead of
  truncating after the first ISO8 group.
- Added `opena8djcpp_hal_logical_capture_batching_contract` to make this source
  property executable evidence.

Reason:
- Prior global `HAL_ISO_FRAMES=64` style experiments changed both capture and
  playback geometry at once, which made the result too risky for audiophile
  claims.
- The safer performance path is to keep the proven logical audio cadence while
  allowing capture USB submissions to be physically batched in an experimental
  build such as `HAL_ISO_FRAMES=8 HAL_CAPTURE_ISO_FRAMES=64
  HAL_CAPTURE_QUEUE=8 HAL_PLAYBACK_ISO_FRAMES=8`.
- This can reduce capture enqueue cadence without changing the default product
  behavior or claiming superiority before same-window physical metrics exist.

Evidence:
- Focused build: default HAL and opt-in capture ISO64 HAL both compile.
- Focused gate: `opena8djcpp_hal_logical_capture_batching_contract` PASS.
- Runtime superiority remains blocked by `opena8djcpp_hal_transport_runtime_gate`
  because the loadable HAL still performs direct IOUSBHost enqueue work and no
  same-session physical A/B CPU/audio evidence exists.

Alternatives discarded:
- Make capture ISO64 the default: rejected because it has no current physical
  sound-quality, timecode, routing, or CPU superiority evidence.
- Repeat global ISO64 for all paths: rejected because it changes too much at
  once and previously failed to establish a safe promotion path.
- Treat the split as proof of better performance: rejected because compile and
  source contracts are not physical runtime metrics.

Next implication:
- The next lock-gated HAL diagnostic window can test the opt-in capture-batched
  candidate against mainline in the same session, but promotion remains blocked
  until route revalidation, music/tone capture, CPU sampling, routing matrix,
  and Traktor/timecode vinyl evidence all pass from one physical bundle.

## 2026-06-18: Separate USB Submit Attempts From Accepted Submits

Decision:
- `captureTransfersSubmitted` and `playbackTransfersSubmitted` now mean accepted
  IOUSBHost isochronous submit work.
- Added `captureSubmitAttempts` and `playbackSubmitAttempts` as appended
  stream-stats fields.
- Extended soundcheck TSV, stream-stats analysis, runtime gates, and physical
  comparison contracts so attempts, accepted submits, and submit failures remain
  observable.

Reason:
- Capture previously incremented `captureTransfersSubmitted` even when
  `enqueueIORequestWithData` failed, while playback incremented submitted only
  on success.
- Same-session CPU/resource comparison now depends on submit cadence. Ambiguous
  counters would let failed queue attempts distort the measurement.

Evidence:
- Focused stream-stats contract: PASS with `field_count=205`,
  `control_field_count=205`, `mismatch_count=0`, and
  `last_field=playbackSubmitAttempts`.
- Focused HAL transport runtime gate: PASS with success-only submit counters
  and runtime submit observability.
- Focused physical submit comparison contract: PASS.

Alternatives discarded:
- Keep only the success-only semantic change: rejected because attempts are
  useful diagnostics when a physical run has queue rejection.
- Count attempts as submitted: rejected because accepted USB work is the metric
  needed for performance comparison.

Next implication:
- Future physical A/B runs can report both accepted submit cadence and failed
  enqueue attempts before any CPU/resource superiority claim.

## 2026-06-18: Gate DriverKit Runtime Binding Stubs

Decision:
- Added `opena8djcpp_driverkit_runtime_binding_gap_gate`.
- The gate treats current `IOUserAudioDevice` pass-through/stub methods as an
  explicit product blocker while still allowing offline CI to pass when the
  blocker is truthfully reported.

Reason:
- The prepared C++ backend has stream models, preallocated request structures,
  and USB submit planning, but the DriverKit extension source still leaves
  `StartIO`, `StopIO`, configuration-change sequencing, stream memory, and zero
  timestamp integration unimplemented.
- A clean scaffold build must not become a false signal for audio quality,
  timecode, CPU, or hardware readiness.

Evidence:
- The new gate requires `prepared_backend_available=true`,
  `runtime_binding_blocked=true`, and
  `product_driverkit_runtime_ready=false`.
- It also requires the blocked claim:
  `NO_DRIVERKIT_RUNTIME_OR_HARDWARE_READINESS_WHILE_IOUSERAUDIODEVICE_PATHS_ARE_STUBS`.

Alternatives discarded:
- Treat the existing DriverKit scaffold contract as enough: rejected because it
  verifies shape, not runtime binding.
- Fail CTest while stubs exist: rejected because the useful offline state is a
  passing diagnostic that records exactly why readiness remains blocked.

Next implication:
- The next DriverKit implementation step is to bind `IOUserAudioDevice` stream
  memory, monotonic zero timestamps, configuration-change policy, and the
  prepared USB request adapter to the skeleton before any dext runtime claim.

## 2026-06-18: Add Pure C++ Audio Device Runtime Binding

Decision:
- Added `AudioDeviceRuntimeBinding` and
  `opena8djcpp_driverkit_device_binding_contract`.
- The binding models the future `IOUserAudioDevice` lifecycle against
  `AudioDriverSkeleton` without compiling or installing a dext.

Reason:
- The skeleton already had stream memory, timestamp, configuration, prepared
  transport, and USB request-pool behavior, but there was no focused object that
  represented the device-facing DriverKit sequence.
- This separates a testable binding policy from Apple SDK mechanics, reducing
  the risk of replacing the current extension stubs later.

Evidence:
- Focused binding contract PASS:
  - `initial_io_memory_descriptors=5`;
  - `initial_io_memory_total_bytes=4096`;
  - `changed_io_memory_total_bytes=8192`;
  - `stream_memory_publications=2`;
  - `zero_timestamp_publications=2`;
  - one configuration change accepted while stopped;
  - one configuration change rejected while IO was running;
  - `product_driverkit_runtime_ready=false`.

Alternatives discarded:
- Wire the `.iig` extension source directly first: rejected for this step
  because this machine still lacks a usable DriverKit SDK and the binding policy
  needed executable coverage before source-level SDK work.
- Treat the binding model as dext readiness: rejected because the real extension
  source still has stubs and there is no signed/runnable DriverKit artifact.

Next implication:
- Replace the extension `StartIO`/`StopIO`/configuration stubs with calls into
  this binding, then build on a DriverKit SDK host and keep hardware validation
  lock-gated.

## 2026-06-18: Bind DriverKit Extension Source to Runtime Model

Decision:
- Replaced the source-level `IOUserAudioDevice` pass-through/stub hooks with
  calls into a small `extension_bridge` backed by `AudioDeviceRuntimeBinding`.
- Upgraded `opena8djcpp_driverkit_runtime_binding_gap_gate` to schema v2 so it
  requires source binding completeness instead of requiring stubs to exist.

Reason:
- The previous gate was useful while the scaffold was a documented blocker, but
  it had become the next implementation obstacle: PASS meant "the stubs are
  truthfully detected." The safer next state is PASS meaning "the extension
  source is wired to the tested C++ binding while real dext readiness remains
  blocked."

Evidence:
- Focused DriverKit CTest subset PASS:
  - `opena8djcpp_driverkit_runtime_contract`;
  - `opena8djcpp_driverkit_extension_scaffold_contract`;
  - `opena8djcpp_driverkit_runtime_binding_gap_gate`;
  - `opena8djcpp_driverkit_device_binding_contract`.
- The runtime-binding gate now requires:
  - `device_start_io_passthrough=false`;
  - `device_stop_io_passthrough=false`;
  - `device_configuration_change_unsupported=false`;
  - `source_binding_present=true`;
  - `source_binding_complete=true`;
  - `real_driverkit_sdk_runtime_blocked=true`;
  - `product_driverkit_runtime_ready=false`.

Alternatives discarded:
- Claim DriverKit runtime readiness after source wiring: rejected because the
  extension still has not been built with the DriverKit SDK, stream-memory
  publication is only modeled by source contracts, and timestamp handling still
  uses a placeholder monotonic model.
- Install or activate a dext now: rejected because the work remains offline and
  there is no signed DriverKit artifact with lock-gated validation evidence.

Next implication:
- Build the extension on a full Xcode/DriverKit SDK host, replace placeholder
  timestamps with the real AudioDriverKit timing source, then validate physical
  quality/performance only under the hardware lock.

## 2026-06-18: Add Audiophile Precision Claim Gate

Decision:
- Added `opena8djcpp_audiophile_precision_claim_gate`.
- The gate reads existing offline evidence from the latest same-window
  `physical-superiority-window` bundle and latest `route-validation-offline`
  runtime-discontinuity bundle.

Reason:
- Quality claims need objective barriers beyond packet correctness, tone gates,
  or single-run soundcheck metrics. LTI coherence, residual ratios, time-warp
  stability, runtime correlation, and repeated same-window samples separate a
  real driver improvement from a broken capture route or unstable timebase.

Evidence:
- Focused gate PASS with claim blocked:
  - candidate `min_mid_coherence=0.102145` versus threshold `0.80`;
  - candidate `min_high_coherence=0.0398416` versus threshold `0.65`;
  - candidate `min_lti_snr_delta_db=-1.12498` versus threshold `6.0`;
  - candidate `max_lti_mid_ratio=2.4614` and `max_lti_high_ratio=5.37161`
    versus threshold `1.15`;
  - candidate `delay_p95_frames=66` and `delay_range_frames=132`;
  - max runtime residual correlation `0.463763` versus threshold `0.16`;
  - same-window runs `1` versus required `3`.

Alternatives discarded:
- Install more analysis libraries first: rejected because existing `numpy` and
  `scipy` are sufficient for this gate, and the missing piece was formal
  PASS/FAIL semantics in the C++ evidence chain.
- Treat diagnostic LTI/time-warp scripts as sufficient: rejected because loose
  analyzer output does not block product claims unless it is integrated into
  CTest, the offline summary, and evidence schema.

Next implication:
- Future physical windows must produce repeated same-window mainline/C++ runs
  that pass LTI, time-warp, runtime-correlation, and statistical sample gates
  before any audiophile superiority claim.

## 2026-06-17: Add DVS/Timecode Stress-Margin Gate

Decision:
- Added `opena8djcpp_dvs_timecode_stress_margin`.
- Made `opena8djcpp_timecode_readiness_gate` require this new stress evidence
  in addition to the existing timecode matrix, signal analysis, DVS packet
  decode, and prepared-transport routing/timecode contract.

Reason:
- Basic DVS signal smoke and packet decode prove that profiles route and decode
  under clean synthetic conditions. They do not prove margin against plausible
  stressors such as frequency drift, input noise, deck crosstalk, stereo
  imbalance, and small dropouts.
- The correct offline leakage metric is correlated tonal leakage at the
  timecode frequency. Total inactive-channel energy would incorrectly fail due
  to deliberately injected synthetic noise.

Evidence:
- Focused DVS/timecode CTest subset PASS: `4/4`.
- `opena8djcpp_dvs_timecode_stress_margin` focused PASS:
  - rows `48`;
  - failures `0`;
  - false accepts `0`;
  - deck swaps `0`;
  - minimum absolute correlation `0.999407`;
  - maximum frequency error `27.5531 ppm`;
  - maximum jitter p95 `0.0070985` frames;
  - maximum balance error `0.451314 dB`;
  - maximum inactive tonal leakage `-88.3757 dBFS`;
  - minimum active/inactive tonal gap `82.7135 dB`.
- `opena8djcpp_timecode_readiness_gate` now reports
  `dvs_timecode_stress_margin_pass=true` while keeping
  `product_timecode_ready=false`.

Alternatives discarded:
- Treat synthetic packet decode as enough DVS evidence: rejected because it did
  not exercise stress margin or deck false-accept behavior.
- Fail on total inactive-channel RMS: rejected because injected broadband noise
  is not the same thing as correlated timecode leakage between decks.

Next implication:
- This raises offline confidence in the DVS/timecode data path. It does not
  prove Traktor/vinyl readiness; physical scope lock, deck validation, capture
  route validation, and same-session mainline/C++ comparison remain required.
- Banach audit accepted the thresholds as defensible only for a synthetic
  routing/decode margin guard, not as evidence of real timecode-vinyl lock.
- The expanded matrix uses a `30 ppm` synthetic frequency-error threshold
  because the current zero-crossing estimator can add several ppm of error
  under simultaneous drift, noise, crosstalk, and dropout.

## 2026-06-18: Add Local SciPy/SoundFile Audiophile WAV Analyzer

Decision:
- Added a pinned local analysis environment (`requirements-analysis.txt` and
  `scripts/setup-analysis-env`) inside the C++ worktree.
- Added `scripts/analyze-audiophile-wav.py` for offline reference/capture WAV
  analysis.
- Integrated its synthetic broadband self-test into
  `scripts/run-cpp-offline-gates`, `current-offline-gates.json`, and evidence
  schema.

Reason:
- Existing C++ physical analysis is useful, but the fastest route to more
  precise audiophile evidence is a dedicated numerical analyzer using proven
  signal-processing libraries.
- The analyzer must distinguish measurable sound-quality evidence from invalid
  capture windows: exact reference matching, active-band coherence, residual
  SNR, delay-window drift, transfer ripple, clipping, and stereo leakage only
  when the reference is decorrelated enough.

Evidence:
- Local environment installed in `.venv-analysis` with `numpy 2.0.2`,
  `scipy 1.13.1`, and `soundfile 0.13.1`.
- Self-test PASS:
  - alignment score `0.999868`;
  - left/right SNR about `67.97/67.95 dB`;
  - left/right active mid-band coherence about `0.9999999`;
  - delay p95 `0` frames;
  - stereo leakage evaluable;
  - worst off-diagonal leakage `-116.678 dB`.
- Reanalysis of existing 2026-06-17 mainline/C++ physical captures still FAILS
  against each run's saved reference WAV: SNR and coherence are below threshold,
  delay p95 is above threshold, and the selected music reference is too
  correlated for a valid stereo leakage claim.

Alternatives discarded:
- Rely only on hand-written C++ WAV metrics: rejected for this layer because
  SciPy/SoundFile gives better validated primitives for correlation, coherence,
  CSD/Welch analysis, and varied WAV IO.
- Make the new analyzer a product-readiness gate immediately: rejected because
  the current physical captures are not valid promotion evidence and the tool's
  self-test only validates the analyzer, not the driver.

Next implication:
- Future physical superiority windows must save exact reference WAVs and use a
  decorrelated stereo fixture when validating no-leakage claims.
- No branch promotion, mainline replacement, or audiophile superiority claim is
  allowed until current same-session mainline/C++ evidence passes this analyzer
  plus the existing route, CPU, DVS/timecode, and readiness gates.

## 2026-06-18: Add Compiled C++ Audiophile WAV Analyzer

Decision:
- Added `tools/audiophile_wav_analysis.cpp` as a compiled, offline WAV analyzer
  with the same claim-blocking semantics as the Python/SciPy analyzer.
- Integrated it into CMake, CTest, `scripts/run-cpp-offline-gates`,
  `current-offline-gates.json`, and `opena8djcpp_evidence_schema_check`.
- Kept the Python/SciPy analyzer as a richer oracle instead of replacing it.

Reason:
- Audiophile readiness cannot depend on one implementation of the measurement
  path. A compiled C++ analyzer gives us a reproducible, dependency-light check
  for alignment, SNR, active-band coherence, delay-window stability, clipping,
  DC, and stereo matrix leakage.
- Python/SciPy remains valuable for Welch/coherence/CSD-style analysis; C++ is
  now the independent gate that prevents a single runtime/library stack from
  being the only judge.

Evidence:
- `opena8djcpp_audiophile_wav_analysis_self_test` PASS.
- C++ self-test metrics in `local-analysis/cpp-offline/current-offline-gates.json`:
  alignment score `0.999999995907`, left/right SNR `81.0840107264` /
  `81.4529542213 dB`, active mid coherence `0.999999994231` /
  `0.999999994944`, delay p95 `0`, evaluable stereo matrix, and worst
  off-diagonal leakage `-89.700983159 dB`.
- Full offline gates before commit passed functional/schema coverage: Debug
  CTest `64/64`, Release CTest `65/65`, evidence schema `68` required files,
  missing `0`. The provenance gate correctly remained FAIL because the worktree
  had uncommitted analyzer changes.

Alternatives discarded:
- Replace Python with C++ completely: rejected because SciPy still provides a
  stronger high-precision oracle for deeper spectral analysis.
- Treat analyzer self-test as product proof: rejected. Both analyzers explicitly
  emit `product_claim_allowed=false`; they validate measurement math only.

Next implication:
- A physical promotion window must pass both the compiled C++ analyzer and the
  Python/SciPy analyzer on current same-session evidence before any sound
  quality claim can move forward.

## 2026-06-18: Wire Audiophile Analyzers Into Physical Window Evidence

Decision:
- Updated `scripts/run-physical-superiority-window` so each Audio 8 soundcheck
  leg writes both:
  - `audiophile-wav-analysis-cpp.json`;
  - `audiophile-wav-analysis.json`.
- The script now builds `opena8djcpp_audiophile_wav_analysis` with the other
  physical-window tools and records per-leg analyzer return codes in
  `window-soundcheck-status.txt`.

Reason:
- A future physical window is the only evidence that can move the project
  toward an audiophile-quality claim. The high-precision analyzers must be part
  of that window by default, not a manual afterthought.
- This keeps the current safety model intact: the analyzers only read the WAVs
  saved by the lock-gated soundcheck and write JSON beside them.

Evidence:
- Source change only so far; no physical window was executed in this step.
- `bash -n scripts/run-physical-superiority-window` is required after this
  change before the next commit.

Next implication:
- Same-session mainline/C++ physical A/B evidence will now carry both analyzer
  outputs per leg. Product superiority remains blocked unless those analyzer
  results, route validity, CPU/submit cadence, and Traktor/timecode gates pass.

## 2026-06-18: Require Audiophile Analyzer PASS In Physical Comparator

Decision:
- Updated `opena8djcpp_physical_run_compare` so promotion-capable physical A/B
  comparison requires both analyzer outputs for candidate and baseline legs:
  - compiled C++ `audiophile-wav-analysis-cpp.json`;
  - Python/SciPy `audiophile-wav-analysis.json`.
- Updated `opena8djcpp_product_quality_claim_gate` and the evidence schema so
  the global quality-claim guard reports
  `same_session_audiophile_wav_analyzers_pass=false` until that evidence exists
  and passes.

Reason:
- The previous step made future physical windows generate the analyzer files,
  but the product comparator still did not require them structurally. That left
  room for a same-session comparison to be interpreted without the stricter
  audiophile evidence.
- A product-quality claim must fail closed if either analyzer is missing or
  fails on either side of the mainline/C++ comparison.

Evidence:
- Focused comparator run on the existing 2026-06-17 mainline/C++ physical
  bundle remains `FAIL` and now includes explicit failing gates:
  `candidate_audiophile_cpp_wav_analysis_pass`,
  `candidate_audiophile_python_wav_analysis_pass`,
  `baseline_audiophile_cpp_wav_analysis_pass`, and
  `baseline_audiophile_python_wav_analysis_pass`.
- `opena8djcpp_product_quality_claim_gate` returns PASS as a guard with
  `quality_claim_allowed=false`,
  `same_session_audiophile_wav_analyzers_pass=false`, and blocker
  `same_session_audiophile_wav_analyzers_missing_or_failing`.

Alternatives discarded:
- Leave analyzer execution as a side artifact only: rejected because artifacts
  that do not participate in the comparator can be overlooked during promotion.

Next implication:
- Even if future route/CPU/tone metrics improve, branch promotion remains
  blocked unless the same-session physical comparator sees both analyzer JSONs
  passing for both mainline and C++ legs.

## 2026-06-18: Add Compiled Audiophile Analysis Stack Contract

Decision:
- Added `opena8djcpp_audiophile_analysis_stack_contract`.
- The new gate makes dual-path WAV analysis a build-time contract:
  - native C++ analyzer is present and exposes the audiophile metrics;
  - Python/SciPy remains pinned as an independent oracle;
  - physical windows run both analyzers per leg;
  - same-session comparison requires both analyzers for mainline and C++;
  - product quality claims fail closed if either analyzer is missing or failing.

Reason:
- The project goal is objective audiophile quality, not a clean compile or a
  single metric. Keeping C++ as the native analyzer lowers runtime dependency
  risk, while retaining Python/SciPy as an oracle catches implementation drift.
- This also answers the current analysis direction: install or use external
  tools only when they improve evidence. Here, the better move is a compiled
  contract that makes the existing high-precision stack non-optional.

Alternatives discarded:
- Remove Python/SciPy now: rejected because it is still a useful independent
  spectral oracle for physical captures.
- Rely on documentation saying both analyzers should run: rejected because
  promotion safety needs executable evidence.

Next implication:
- Offline PASS now includes proof that analyzer evidence cannot be silently
  skipped. Physical quality, performance, routing, and timecode claims remain
  blocked until current lock-gated same-session evidence exists and passes.

## 2026-06-18: Add Native C++ LTI Transfer-Quality Analyzer Self-Test

Decision:
- Added `opena8djcpp_lti_transfer_quality`, a native C++ diagnostic analyzer
  for LTI transfer quality.
- Integrated its self-test into CMake/CTest, `scripts/run-cpp-offline-gates`,
  and the evidence schema as
  `local-analysis/cpp-offline/lti-transfer-quality-cpp-self-test.json`.

Reason:
- LTI transfer quality is the highest-impact remaining Python/SciPy dependency
  for audiophile superiority evidence. It distinguishes route coloration that a
  linear transfer can explain from residual distortion or time instability.
- The first step is not to remove Python. The safe step is a C++ analyzer with
  schema-compatible fields and a deterministic self-test, followed by a parity
  gate against existing Python evidence.

Alternatives discarded:
- Replace the Python LTI analyzer immediately: rejected because numeric parity
  against SciPy/Welch/CSD evidence is not proven yet.
- Leave LTI entirely in Python: rejected because C++ is the candidate line and
  final evidence should not depend on a fragile analysis environment where a
  compiled deterministic path is feasible.

Next implication:
- Add a parity gate that runs the C++ LTI analyzer on saved physical runs and
  compares core fields against `scripts/analyze-lti-transfer-quality.py` before
  using the C++ output for claim-critical gates.

## 2026-06-18: Add LTI C++/Python Parity Guard

Decision:
- Added `opena8djcpp_lti_transfer_quality_parity_gate`.
- The gate reruns the C++ LTI analyzer on the saved same-session physical
  mainline/C++ run from `20260617T212050Z-mainline-vs-cpp-raw-reuse-irig` and
  compares it against the existing Python/SciPy LTI JSON.
- In its initial form, it passed as a guard while reporting
  `lti_parity_pass=false` and `cpp_lti_claim_allowed=false`.

Reason:
- The C++ LTI analyzer self-test proves the tool can measure a deterministic
  generated fixture, but it does not prove numerical equivalence with the
  existing SciPy oracle on real saved captures.
- The focused parity run shows useful partial agreement: alignment lag and
  scalar SNR are close. It also shows claim-critical divergence: LTI SNR delta
  and LTI residual ratios differ by much more than the allowed tolerance.

Evidence:
- Candidate saved physical leg:
  - alignment lag delta: `1` frame;
  - scalar SNR delta: about `0.0766 dB`;
  - LTI SNR delta: about `13.38 dB`;
  - LTI mid-ratio delta: about `1.279`.
- Baseline saved physical leg:
  - alignment lag delta: `5` frames;
  - scalar SNR delta: about `0.0343 dB`;
  - LTI SNR delta: about `10.185 dB`;
  - LTI mid-ratio delta: about `2.726`.

Alternatives discarded:
- Claim the C++ LTI analyzer is equivalent because its self-test passes:
  rejected. The saved physical evidence contradicts equivalence.
- Remove the C++ LTI analyzer: rejected. It already provides a useful native
  measurement path; it just needs parity work before claim use.

Next implication:
- Improve the C++ LTI reconstruction/PSD path or add a closer Welch/CSD model
  until the parity gate can flip `lti_parity_pass=true`.

## 2026-06-18: Make C++ LTI Analysis Numerically Comparable To Python/SciPy

Decision:
- Replaced the sparse 14-frequency DFT estimate in
  `opena8djcpp_lti_transfer_quality` with a dense Welch/CSD-style segmented FFT
  estimate.
- Matched the Python/SciPy cross-spectral orientation used by
  `signal.csd(got, ref)`.
- Kept the analysis offline-only and dependency-free; no hardware, USB,
  CoreAudio, driver install/reload, playback, or recording is involved.

Reason:
- The previous C++ analyzer could pass a generated fixture but diverged from
  Python/SciPy on saved physical evidence because it estimated LTI behavior at a
  sparse frequency grid and used the opposite CSD orientation.
- The requested objective requires metrics that can be trusted, not just
  compiled tools. Passing parity against the existing oracle removes one source
  of metric ambiguity.

Evidence:
- Candidate saved physical leg:
  - alignment lag delta: `1` frame;
  - scalar SNR delta: about `0.0766 dB`;
  - LTI SNR delta: about `0.1754 dB`;
  - LTI mid-ratio delta: about `0.0542`;
  - LTI high-ratio delta: about `0.0049`.
- Baseline saved physical leg:
  - alignment lag delta: `5` frames;
  - scalar SNR delta: about `0.0343 dB`;
  - LTI SNR delta: about `0.0182 dB`;
  - LTI mid-ratio delta: about `0.1127`;
  - LTI high-ratio delta: about `0.0838`.

Alternatives discarded:
- Lower parity thresholds: rejected. It would hide real analyzer disagreement.
- Full-length FFT/IFFT reconstruction in the always-on gate: rejected for now
  because an earlier attempt was too slow for mandatory offline gates. Segment
  FFT Welch/CSD gives a fast, close-enough comparison for the current claim
  guard.

Next implication:
- The next analysis migration target is fractional time-warp parity, followed
  by runtime discontinuity correlation parity.
- This does not authorize product readiness or mainline replacement.

## 2026-06-18: Add Native C++ Fractional Time-Warp Parity Guard

Decision:
- Added `opena8djcpp_fractional_time_warp` as an offline-only native C++
  analyzer for the fractional delay/time-warp diagnostic previously implemented
  in `scripts/analyze-fractional-time-warp.py`.
- Added `opena8djcpp_fractional_time_warp_parity_gate` to compare C++ output
  against the saved Python/SciPy `fractional-time-warp.json` oracle evidence
  for the same-session candidate/mainline run
  `20260617T212050Z-mainline-vs-cpp-raw-reuse-irig`.
- Integrated the tools into CMake/CTest and the offline evidence runner.
- Kept product claims blocked: parity means only that the C++ analyzer can be
  trusted for the saved evidence covered by the guard.

Reason:
- Audiophile claims depend on timebase evidence. A Python-only diagnostic is
  useful as an oracle, but the C++ line needs native, reproducible analyzers
  for long-term gates.
- The implementation ports the same core behavior: per-window fractional peak
  estimation, median-smoothed delay curve, scalar and 2x2 matrix fit before and
  after warp, and `3 dB` / `6 dB` classification thresholds.
- Matching the saved Python/SciPy output is the objective test that the native
  analyzer is not silently changing the meaning of the metric.

Evidence:
- Focused Debug CTest:
  - `opena8djcpp_fractional_time_warp_self_test`: PASS.
  - `opena8djcpp_fractional_time_warp_parity_gate`: PASS.
- Focused Release CTest:
  - `opena8djcpp_fractional_time_warp_self_test`: PASS.
  - `opena8djcpp_fractional_time_warp_parity_gate`: PASS.
- Parity guard:
  - `timewarp_parity_pass=true`.
  - `cpp_timewarp_claim_allowed=true`.
  - `blockers=[]`.
  - Candidate scalar improvement delta about `0.0000041 dB`.
  - Candidate matrix improvement delta about `0.0000039 dB`.
  - Baseline scalar improvement delta about `0.00000005 dB`.
  - Baseline matrix improvement delta about `0.0000004 dB`.
  - Candidate and baseline classifications both match the Python oracle:
    `fractional_time_warp_rejected`.

Alternatives discarded:
- Treat the existing Python diagnostic as sufficient forever: rejected. The C++
  line needs native gates and fewer runtime dependencies for reproducible
  quality evidence.
- Loosen the claim gate because time-warp is now native: rejected. Native
  parity does not make the physical capture route valid or prove the driver is
  better than mainline.

Next implication:
- The next analysis migration target is runtime discontinuity correlation
  parity.
- Runtime CPU/resource superiority still requires prepared-submit runtime
  evidence and lock-gated physical comparison against mainline.

## 2026-06-18: Add Native C++ Runtime Discontinuity Parity Guard

Decision:
- Added `opena8djcpp_runtime_discontinuity_analysis` as an offline-only native
  C++ analyzer for the saved WAV/TSV runtime-correlation diagnostic previously
  implemented in `scripts/analyze-runtime-discontinuities.py`.
- Added `opena8djcpp_runtime_discontinuity_parity_gate` to compare C++ output
  against the saved Python/SciPy oracle bundle
  `local-analysis/route-validation-offline/20260617-mainline-cpp-route-signature/runtime-discontinuities.json`.
- Integrated both tools into CMake/CTest and the offline evidence runner.

Reason:
- The audiophile precision gate blocks product claims when runtime residual
  correlation is above threshold or missing. That metric must be reproducible
  by the C++ line before Python can become only an oracle rather than the sole
  implementation.
- The C++ analyzer ports the same windowed model: aligned WAV windows, integer
  lag, scalar residual/SNR, TSV CPU/stream telemetry, offset search, and
  strongest correlation ranking.

Evidence:
- Focused Debug and Release CTest subsets pass for:
  - `opena8djcpp_runtime_discontinuity_analysis_self_test`.
  - `opena8djcpp_runtime_discontinuity_parity_gate`.
- Parity guard:
  - `runtime_discontinuity_parity_pass=true`.
  - `cpp_runtime_discontinuity_claim_allowed=true`.
  - `python_run_count=6`.
  - `cpp_run_count=6`.
  - max top-correlation delta about `0.00000047`.
  - max residual median delta about `0.00000004`.
  - max lag-jump p95 delta about `0`.
  - max SNR median delta about `0.000031 dB`.

Alternatives discarded:
- Leave runtime discontinuity as Python-only: rejected for the same reason as
  LTI and time-warp. Claim-critical analysis needs native reproducibility.
- Treat parity as performance proof: rejected. It only proves metric parity on
  saved evidence; it does not reduce real USB submit work or CPU.

Next implication:
- The native analyzer stack now covers LTI, fractional time-warp, and runtime
  discontinuity parity.
- The next engineering blocker is runtime: integrate prepared submit into the
  actual HAL/DriverKit path and prove lower CPU/resource use without worse
  physical audio quality.

## 2026-06-18: Build Separate HAL Prepared-Runtime Candidate Artifact

Decision:
- Added `scripts/build-hal-prepared-runtime-candidate` and the
  `make hal-prepared-runtime-candidate` target.
- The script builds the opt-in prepared runtime HAL geometry, copies it to
  `build/OpenA8DJ-prepared-runtime.driver`, rebuilds the normal HAL bundle, and
  writes JSON evidence. It refuses candidate and JSON output paths outside the
  C++ worktree.
- The offline gate runner now validates both the default HAL bundle and the
  separated prepared-runtime candidate bundle.

Reason:
- Lower CPU/resource use needs a prepared-runtime artifact that is explicit,
  reproducible, and impossible to confuse with the default HAL bundle.
- Keeping the artifact separate lets us prepare a physical comparison window
  without silently replacing the default local build.

Evidence:
- Focused `make hal-prepared-runtime-candidate`: PASS.
- Focused bundle check for `build/OpenA8DJ-prepared-runtime.driver`: PASS.
- Full offline runner before commit: PASS for Debug CTest `74/74`, Release
  CTest `75/75`, evidence schema `required_files=80`, `missing_files=0`.
  Provenance correctly remained FAIL before commit because the worktree was
  dirty.

Alternatives discarded:
- Treat `make hal-prepared-runtime` alone as the physical-test candidate:
  rejected because it leaves `build/OpenA8DJ.driver` in opt-in geometry and can
  blur evidence.
- Claim CPU superiority from the separate bundle: rejected. A build artifact
  proves only reproducibility and bundle completeness; real CPU/resource
  superiority requires same-session physical comparison against mainline.

Next implication:
- After commit, rerun full offline gates so provenance is clean.
- Next physical action is route-only revalidation under the global lock, not a
  product A/B.

## 2026-06-18: Bind HAL Prepared Runtime Through Explicit Submit Dispatch

Decision:
- Added default-off submit dispatch wrappers in the HAL runtime:
  `submitCaptureTransfer`, `enqueuePreparedCaptureSubmitWithTransfer`,
  `submitPlaybackTransfer`, and `enqueuePreparedPlaybackSubmitWithTransfer`.
- The normal HAL path remains the fallback. The prepared helpers are only
  selected when `OPENA8DJ_HAL_PREPARED_USB_SUBMIT_RUNTIME` is enabled and
  `kPreparedRuntimeGeometrySupported` is true.
- Strengthened the binding, migration, runtime, summary, and schema gates to
  require `prepared_runtime_dispatch_path_present=true`.

Reason:
- The separated candidate artifact was reproducible, but the HAL queue methods
  still mixed queue policy and physical IOUSBHost submit. A dedicated dispatch
  point makes the opt-in runtime path auditable and gives physical evidence a
  clear place to attribute submit cadence.

Evidence:
- `make -B hal`: PASS.
- `make hal-prepared-runtime-candidate`: PASS.
- Focused release gates PASS:
  - `opena8djcpp_hal_prepared_runtime_binding_contract`.
  - `opena8djcpp_prepared_transport_migration_gate`.
  - `opena8djcpp_hal_transport_runtime_gate` as a blocking guard.

Alternatives discarded:
- Convert `OpenA8DJUSB.m` to Objective-C++ and bind the C++
  `PreparedUsbRequestPool` directly in this pass: rejected for now because it
  expands build/signing/runtime risk before the current geometry-based prepared
  candidate has physical submit-counter evidence.
- Treat this dispatch as CPU superiority: rejected. Lower CPU and no audio
  regression still require lock-gated same-session physical metrics.

Next implication:
- Run full offline gates after commit so provenance is fresh.
- Next hardware action remains route-only revalidation first. Only after that
  can the prepared-runtime candidate be compared against mainline with submit
  counters, CPU p95, jitter, and dual audiophile WAV analyzers.

## 2026-06-18: Add Read-Only Physical Route Inventory Gate

Decision:
- Added `scripts/physical-route-inventory` as a read-only environment
  classifier for physical evidence windows.
- The offline gate runner now writes
  `local-analysis/cpp-offline/physical-route-inventory.json` and the evidence
  schema requires it in `current-offline-gates.json`.
- The inventory distinguishes iRig capture visibility, Audio 8 DJ USB
  visibility, Audio 8 DJ CoreAudio visibility, active HAL installation, valid
  non-Audio8 known-good output availability, and same-device iRig diagnostic
  availability.

Reason:
- Sound-quality and CPU/resource claims are only meaningful if the physical
  route can support the claim. The current machine can show iRig and Audio 8 DJ
  on USB while still lacking an active Audio 8 DJ CoreAudio device and lacking
  a non-Audio8 wired known-good output for promotion evidence.
- Capturing this as JSON prevents a diagnostic route from being mistaken for a
  promotable mainline-vs-C++ comparison.

Evidence:
- Focused inventory: PASS.
- Current decision:
  `LOCK_GATED_SAME_DEVICE_IRIG_DIAGNOSTIC_ONLY`.
- Current blockers:
  `audio8dj_not_visible_as_coreaudio_device`,
  `active_opena8dj_hal_not_installed`, and
  `non_audio8_non_builtin_known_good_output_not_visible`.
- No playback, capture, driver install/reload, USB reset, CoreAudio restart,
  default-device change, sample-rate change, or buffer-size change was
  performed.

Alternatives discarded:
- Run a physical A/B immediately: rejected because the visible route cannot
  produce promotable evidence yet.
- Treat iRig same-device loopback as route proof: rejected. It can diagnose
  whether iRig itself is alive, but it does not prove the external capture route
  needed to judge Audio 8 DJ output quality.

Next implication:
- Use the inventory before any lock-gated hardware window.
- If only the same-device iRig diagnostic is available, it may be run as a
  diagnostic under lock, but the result must remain blocked for product,
  quality, CPU/resource, timecode, and branch-promotion claims.

## 2026-06-18: Record Latest iRig Same-Device Diagnostic In Inventory

Decision:
- Extended `scripts/physical-route-inventory` to read the latest
  `local-analysis/known-good-route/*` diagnostic evidence.
- The offline summary now records the latest known-good-route result, whether
  it was valid for promotion, the same-device diagnostic result, alignment, and
  SNR floor.
- The schema gate now requires these fields to exist.

Reason:
- The lock-gated iRig same-device diagnostic produced real physical evidence,
  but local WAV/JSON evidence is ignored by git. The current offline summary
  must still expose the outcome so later runs cannot accidentally treat iRig
  visibility as route validity.

Evidence:
- Latest diagnostic:
  `local-analysis/known-good-route/20260618T061314Z-irig-same-device-diagnostic`.
- Request validation: PASS, but `valid_for_promotion=false`.
- Route summary: FAIL.
- Alignment score: `0.009115610530373885`.
- SNR floor: `-43.31856306497336 dB`.
- Click outliers: `22`.
- No driver install/reload, USB reset, CoreAudio restart, default-device
  change, sample-rate change, or buffer-size change was performed.

Alternatives discarded:
- Leave the diagnostic only in ignored `local-analysis`: rejected because the
  next gate run would not expose the physical route failure.
- Treat native analyzer `PASS` as route success: rejected because that result
  is analyzer-only and explicitly not product readiness; the Python route
  analyzer and summary correctly failed the route.

Next implication:
- Product and branch-promotion claims remain blocked.
- Next useful physical route action is to restore a real non-Audio8 wired
  source into the iRig capture chain or run a HAL-candidate visibility/safety
  window under lock; same-device iRig output/capture has now failed as route
  evidence.

## 2026-06-18: Preserve Route Failure Details In Offline Summary

Decision:
- The offline gate summary must expose the latest route diagnostic's click
  outliers, Python analyzer return code, native analyzer return code, and
  native readiness claim.
- The schema gate now requires those fields.

Reason:
- A route failure is not actionable enough when summarized only as alignment
  and SNR. For audiophile-quality work, the summary must distinguish a failed
  physical route analyzer from an analyzer-only native PASS, and must preserve
  click evidence.

Evidence:
- The latest iRig same-device diagnostic had `analysis_rc=1`, `native_rc=0`,
  `click_outliers=22`, and native readiness
  `ANALYZER_ONLY_NOT_PRODUCT_READINESS`.

Next implication:
- Future offline evidence cannot silently hide why a physical capture route
  failed. Product claims remain blocked until a promotable route passes in the
  same session as the mainline/C++ comparison.

## 2026-06-18: Treat Latest HAL Safety Failure As Consumable Blocker

Decision:
- Change `opena8djcpp_hal_candidate_safety_gate` so it can consume a failed
  latest HAL safety window and still let the offline evidence summary finish.
- Add `safety_window_status` and guard/recovery fields to the summary.
- Add the dynamic hard blocker
  `latest_hal_candidate_safety_window_not_passing` whenever the latest safety
  window does not pass.

Reason:
- The latest lock-gated HAL safety recheck produced useful physical evidence:
  Audio 8 DJ enumerated as 8x8 and iRig remained visible, but the safety guard
  failed due high watched audio-stack CPU. Aborting the entire offline summary
  would hide the current blocker; treating the window as product-safe would be
  false.

Evidence:
- Latest run:
  `local-analysis/hal-candidate-safety/20260618T062630Z-default-current-safety`.
- During guard:
  - `Open Audio 8 DJ`, `uid=org.opena8dj.Audio8DJ`, `in=8 out=8`.
  - `iRig Stream`, `in=2 out=2`.
  - `audio_stack_health=FAIL`.
  - `coreaudiod=56.3%`.
  - `opena8dj_driver=0.3%`.
  - max watched label `mediaremoted=57.3%`.
- Recovery:
  - HAL unloaded.
  - OpenA8DJ driver pid absent.
  - CoreAudio enumeration still passed.

Alternatives discarded:
- Keep the gate process failing: rejected because it prevents the offline
  summary from preserving the latest physical blocker.
- Convert the safety window to PASS: rejected because current hardware safety
  did not pass.

Next implication:
- Offline gates may still pass as evidence-processing gates, but product
  readiness and branch promotion remain blocked by the latest HAL safety
  failure plus the invalid capture route.

## 2026-06-18: Promote Prepared USB Runtime Submitter Into Core

Decision:
- Add `PreparedUsbRuntimeSubmitter` as a reusable pure-C++ core component.
- Keep it offline-only for now, with a contract that proves the default
  prepared geometry submits `528` logical slots as `66` USB submit calls,
  `33` capture descriptors and `33` playback descriptors.
- Keep product/performance claims blocked until this core submitter is bound to
  real HAL or DriverKit USB and measured in a same-session physical A/B.

Reason:
- The existing prepared USB batching behavior was proven in tool contracts, but
  not exposed as a reusable core runtime API. That made the next optimization
  step too easy to duplicate in HAL/DriverKit code.
- A core submitter gives HAL and DriverKit one shared, preallocated model for
  planner-to-request-pool submission, with bounded live requests and explicit
  fallback accounting.

Evidence:
- `tools/prepared_usb_runtime_submit_contract.cpp`
- `local-analysis/cpp-offline/prepared-usb-runtime-submit-contract.json`
- Contract invariants:
  - `logical_slots=528`
  - `usb_submit_calls=66`
  - `partial_submit_calls=0`
  - `total_bytes=185856`
  - `total_frames=5808`
  - `request_submit_calls=66`
  - `request_completion_calls=66`
  - `fallback_allocations=0`
  - `submit_failures=0`
  - `runtime_safe=true`
  - `payload_equivalent=true`

Alternatives discarded:
- Keep the submitter only inside tool contracts: rejected because the HAL and
  DriverKit paths need a shared runtime abstraction before physical CPU
  reduction can be tested rigorously.
- Bind directly into HAL in the same step: rejected because the core contract
  should be stable and evidence-backed before touching the physical runtime
  path again.

Next implication:
- The next implementation step is to bind this core submitter into the HAL
  prepared-runtime opt-in path, then run only a lock-gated route/safety window
  before any performance or quality claim.

## 2026-06-18: Bind Prepared HAL Runtime Through C ABI Bridge

Decision:
- Add `PreparedUsbAsyncRuntime` as the core lifecycle model for real async USB
  submissions.
- Add `OpenA8DJPreparedRuntimeBridge.h/.mm` as an opt-in C ABI / Obj-C++ bridge
  for the HAL prepared-runtime profile.
- Keep `HAL_PREPARED_USB_SUBMIT_RUNTIME ?= 0` as the default. Only
  `make hal-prepared-runtime` links the bridge and C++ core sources.

Reason:
- `OpenA8DJUSB.m` is Objective-C, not Objective-C++, so directly including C++
  core headers would break the normal HAL build.
- The physical USB lifecycle is async. Handles must remain live until
  IOUSBHost completion or explicit cancel; an offline submitter that completes
  internally is not sufficient for a real HAL path.
- Separating the C ABI bridge preserves the current main HAL behavior while
  allowing the prepared profile to register bounded descriptors before each
  real `enqueueIORequestWithData`.

Evidence:
- `local-analysis/cpp-offline/prepared-usb-async-runtime-contract.json`
- `local-analysis/cpp-offline/hal-prepared-runtime-binding-contract.json`
- `local-analysis/cpp-offline/prepared-transport-migration-gate.json`
- `make -B hal-prepared-runtime` builds a local candidate only; no install,
  load, CoreAudio, USB, hardware, default-device, sample-rate, or buffer change.

Alternatives discarded:
- Compile the entire HAL in C++: rejected because it turns the C HAL source into
  C++ and creates unnecessary language-risk in the default path.
- Recycle request handles at submit time: rejected because real IOUSBHost
  completion owns transfer lifetime.

Next implication:
- Offline migration evidence now supports preparing a controlled hardware
  candidate. It still does not prove lower CPU, better sound quality, timecode
  vinyl behavior, or branch promotion.

## 2026-06-18: Use Directional Byte Geometry for Prepared HAL Runtime

Decision:
- Replace the single prepared-runtime `bytesPerSlot`/`bytes_per_slot` value with
  separate capture and playback byte geometry.
- Configure capture as `kIsoBytesPerFrame * kIsoFramesPerTransfer`.
- Configure playback as `bytesPerPacket * kIsoFramesPerTransfer`.

Reason:
- The first prepared HAL candidate loaded and enumerated, but the physical
  diagnostic showed zero real USB submits:
  `captureTransfersSubmitted=0`, `playbackTransfersSubmitted=0`, and
  `outputFramesRead=0`.
- Capture transfer buffers are physically sized as 512-byte ISO transactions,
  while playback transaction sizes follow the negotiated bytes-per-packet
  value. A single descriptor byte shape cannot represent both safely.

Evidence:
- Physical diagnostic:
  `local-analysis/physical-superiority-window/20260618T071046Z-prepared-candidate-only-skip-route-5d633be`
- Offline contracts after fix:
  - `local-analysis/cpp-offline/prepared-usb-async-runtime-contract.json`
  - `local-analysis/cpp-offline/hal-prepared-runtime-binding-contract.json`
  - `local-analysis/cpp-offline/prepared-transport-migration-gate.json`
- Candidate build:
  - `make -B hal-prepared-runtime-candidate`
  - Prepared candidate hash:
    `a54d1aab197c5809c9734b37609a3b6713bf2bef39c78d9a64d48c08101eaea5`

Alternatives discarded:
- Loosen descriptor byte validation entirely: rejected because it would hide
  real geometry mistakes in the hot path.
- Use capture's 512-byte slot size for both directions: rejected because
  playback would then accept descriptors that do not match submitted payloads.

Next implication:
- Rerun a lock-gated prepared candidate diagnostic after commit/freshness gates.
  The first pass criterion is nonzero real capture/playback USB submits with
  no qfail storm, not audio-quality superiority.

## 2026-06-18: Expose Prepared Runtime Rejection Counters Before More Hardware Claims

Decision:
- Append prepared-runtime lifecycle and rejection counters to the HAL stream
  stats payload.
- Print those counters through `opena8dj-control stream-stats`.
- Capture them in `scripts/run-soundcheck` TSV snapshots and summarize them in
  `scripts/analyze-stream-stats.py`.
- Require the observability in the prepared-runtime and HAL transport gates.

Reason:
- The post-fix physical diagnostic proved capture submits now reach USB, but
  playback still fails before IOUSBHost enqueue:
  `playbackSubmitAttempts=508` and `playbackTransfersSubmitted=0`.
- The previous stats could not distinguish descriptor mismatch, live-request
  exhaustion, fallback allocation, stale completion, or another request-pool
  cause. Continuing hardware tests without that split would waste lock windows.

Evidence:
- Physical diagnostic:
  `local-analysis/physical-superiority-window/20260618T072122Z-prepared-candidate-only-skip-route-7be2be6`
- Offline gates after instrumentation:
  - Debug CTest: `77/77 PASS`
  - Release CTest: `78/78 PASS`
  - `stream-stats-contract`: `PASS`, last field
    `preparedRuntimeCancelledBytes`
  - Product claims remain blocked by `product-quality-claim-gate` and
    `hal-transport-runtime-gate`.

Alternatives discarded:
- Infer the cause from `playbackQueueFailures`: rejected because `qfail` only
  means the queue call returned false, not why.
- Add logging in the hot path: rejected because per-buffer logs are forbidden in
  the real-time path.

Next implication:
- Commit and rerun freshness gates, then run one short lock-gated prepared
  candidate diagnostic. Its first purpose is attribution of the playback
  rejection cause, not audio-quality or branch-promotion evidence.
## 2026-06-18 - HAL Default Output Surface Moves To One 8-Channel Stream

- Decision: default HAL builds now expose one 8-channel output stream instead of four 2-channel output streams and disable output stream usage (`HAL_OUTPUT_STREAMS ?= 1`, `HAL_STREAM_USAGE ?= 0`).
- Reason: lock-gated same-machine physical evidence after killing the Pioneer firmware updater USB blocker showed the four-stream HAL path corrupts output timing/layout materially. The default no-trace HAL scored `quality_alignment_score=0.326384` with `lag_jumps_gt_2_frames=29`, while the one-stream candidate scored `quality_alignment_score=0.975550` with the same Audio 8 DJ -> iRig route and no capture clipping.
- Alternatives rejected for the current default:
  - `HAL_IGNORE_OUTPUT_SAMPLE_TIME=1`: physical run scored `quality_alignment_score=-0.065227`, worse than default.
  - `HAL_FLUSH_OUTPUT_IN_WRITE_MIX=1`: physical run scored `quality_alignment_score=-0.040334`, worse than default.
  - prepared runtime as default: rejected for product quality until physical quality passes; prepared 8-slot and 2-slot runs failed badly even when submit counters worked.
- Evidence:
  - `/Users/fer/dev/audio8djcpp/local-analysis/physical-superiority-window/20260618T081331Z-default-hal-notrace-after-fwupdate-kill`
  - `/Users/fer/dev/audio8djcpp/local-analysis/physical-superiority-window/20260618T082010Z-ignore-sampletime-after-fwupdate-kill`
  - `/Users/fer/dev/audio8djcpp/local-analysis/physical-superiority-window/20260618T082130Z-flush-write-mix-after-fwupdate-kill`
  - `/Users/fer/dev/audio8djcpp/local-analysis/physical-superiority-window/20260618T082250Z-outputstreams1-after-fwupdate-kill`
- Readiness impact: this is a strong quality improvement but not product readiness. The one-stream candidate still fails strict gates: `analog_snr_db=10.70`, `lag_jumps_gt_2_frames=25`, `click_outliers=15`, and audiophile analyzers fail.

## 2026-06-18 - Post-Reboot Autologin/Codex Recovery Still Broken

- Decision: keep reboot recovery as an explicit operational blocker, not part of the audio-quality candidate.
- Reason: the system did not recover Codex automatically after reboot earlier in this work session even though the user needed no-human-login continuation. This must be fixed before planned reboot-dependent hardware windows.
- Readiness impact: no driver readiness or branch-promotion claim may depend on unattended reboot recovery until there is fresh evidence that login/Codex resume works.

## 2026-06-18 - WAV Analyzer Disagreements Fail Closed

- Decision: native soundcheck WAV analysis now returns `FAIL` when native-vs-recorded metric parity disagrees, and CTest uses explicit `--health-check` mode so executable availability is not confused with evidence quality.
- Reason: the old analyzer could emit a diagnostic `PASS` while hiding disagreement between the native C++ analyzer and recorded metrics. That is unacceptable for audiophile readiness because it can turn ambiguous captures into false confidence.
- Alternatives rejected:
  - Keep broad `WARN` parity semantics: rejected because downstream gates could treat the artifact as softer than it is.
  - Keep CTest judging archived WAV evidence by default: rejected because stale physical evidence can fail for correct reasons and should not make binary health checks flaky.
- Evidence: `./scripts/run-cpp-offline-gates` passed after the gate update, while product quality and branch promotion remained blocked.
- Readiness impact: analyzer health can pass independently, but WAV evidence must pass dual analyzer/parity requirements before any quality claim.
