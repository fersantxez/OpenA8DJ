# Timecode latency offline evidence

Date: 2026-07-25
Worktree: `/Users/fer/dev/opena8dj-latency-lab`
Branch: `codex/timecode-latency`
Base revision: `27a8410`
Current evidence revision: `1532971`

## Scope

This evidence records the non-interactive implementation and offline validation of
the timecode/vinyl latency instrumentation. It is diagnostic evidence only. It
does not prove physical DVS latency, sound quality, Traktor behavior, or product
readiness.

The stable output profile was intentionally left unchanged and remains the only
profile eligible for use:

- `HAL_OUTPUT_TARGET_LATENCY_FRAMES=3072`
- `HAL_INPUT_MAX_LATENCY_FRAMES=0`
- `HAL_OUTPUT_PREFETCH_FRAMES=128`

The lock-gated safety tests did load and unload the real Audio 8 DJ HAL and
restart CoreAudio as part of candidate recovery. They did not reset USB, change
default devices, or use a physical turntable. The explicit CoreAudio recovery
and final stable restore are recorded below.

## Implemented gates

- `scripts/timecode-latency-offline-gate` runs a deterministic profile matrix,
  validates output/host geometry, compares modeled latency against the stable
  baseline, and writes JSON evidence.
- `scripts/timecode-latency-e2e-fixture` propagates deterministic input markers
  through capture completion, host scheduling, output prefetch, and start/
  restart/output queues. It reports input-to-output p50/p95/p99, steady-state
  jitter, monotonic output ordering, and separate start/restart response
  distributions. It is explicitly synthetic and never touches audio hardware.
- `scripts/test-timecode-latency-e2e-fixture.py` covers the baseline,
  `prefetch64`, `restart1024`, and `output3008` response changes while
  asserting that physical evidence and product claims remain false.
- `scripts/test-timecode-latency-offline-gate.py` covers the baseline, the
  `output2048`, `output3008`, `host256`, and `persistent-transport32` candidates, unsafe geometry
  rejection, and monotonic modeled reductions.
- The offline matrix and deterministic fixture include a separately named
  `persistent-transport32` profile. It represents four 8-frame prepared slots
  per submit, but remains model-only until the exact artifact is proven on the
  real HAL path.
- `make hal-prepared-medium-candidate` generates the medium prepared-runtime
  bundle with four slots per submit, eight logical ISO frames, and eight-entry
  capture/playback queues. It restores the stable default HAL after building
  the candidate and does not install or load it.
- `scripts/run-cpp-offline-gates` accepts the optional
  `OPENA8DJ_SOURCE_REFERENCE_WAV` environment variable and forwards it to the
  physical-window planner. This makes a future lock-gated source-reference
  comparison reproducible without treating a plan as a completed measurement.
- `scripts/run-cpp-offline-gates` includes the latency gate and its test while
  preserving diagnostic failures for route contamination and iRig idle capture.
- `tools/evidence_provenance_freshness_gate.cpp` now resolves the active
  worktree instead of falling back to the legacy checkout, and treats the
  optional candidate manifest as optional. The current offline evidence is
  therefore attributable to `324cddd` with a clean worktree and the provenance
  gate passes.
- The CPU-pool candidate contract now correctly reports that a build-only helper
  cannot claim physical evidence or product readiness.

## Offline results

The latency gate result was `PASS`, with `model_only=true`,
`physical_evidence_present=false`, `product_claim_allowed=false`, and no hard
failures. At 48 kHz, the modeled pipeline p95 values were:

| Profile | Modeled p95 | Modeled reduction | Work proxy change |
| --- | ---: | ---: | ---: |
| baseline | 78.667 ms | 0.000% | 0.000% |
| prefetch64 | 77.333 ms | 1.695% | 0.000% |
| capture32 | 78.000 ms | 0.847% | +88.889% |
| persistent-transport32 | 78.000 ms | 0.847% | +88.889% |
| capture16 | 77.667 ms | 1.271% | +266.667% |
| output2560 | 68.000 ms | 13.559% | 0.000% |
| output2816 | 73.333 ms | 6.780% | 0.000% |
| output3008 | 72.000 ms | 8.475% | 0.000% |
| output2304 | 62.667 ms | 20.339% | 0.000% |
| output2048 | 57.333 ms | 27.119% | 0.000% |
| host256 | 52.000 ms | 33.898% | +11.111% |

All tested profiles passed the offline geometry rule `output >= host * 4`.
These numbers are modeled pipeline values, not round-trip measurements.

The full C++ matrix passed:

- 88/88 offline tests passed.
- 89/89 release tests passed.
- The repeated offline benchmark completed five runs with zero check errors,
  zero panic flags, and zero decode-output overflows on every run.
- Representative five-run benchmark ranges were 1,537.87 to 1,625.16 MiB/s
  for packing, 563.718 to 578.056 MiB/s for decode-into, and 519.011 to
  831.103 million frames/s for forward routing.

The final offline rerun after the candidate iterations also passed the latency
gate and the C++ release suite: `89/89` tests passed. The five-run benchmark
reported zero check errors, zero decode-output overflows, and zero panic flags;
the observed ranges were 1,615.78-1,634.21 MiB/s for packing,
561.095-578.756 MiB/s for decode-into, and 835.852-1,025.670 million frames/s
for forward routing.

The deterministic input-to-output fixture also passed for the baseline and all
candidate profiles in its offline matrix. The baseline result was:

| Profile | p50 | p95 | p99 | steady jitter p99-p50 | restart p50 |
| --- | ---: | ---: | ---: | ---: | ---: |
| baseline | 71.760 ms | 76.805 ms | 77.099 ms | 5.068 ms | 43.125 ms |
| prefetch64 | 70.427 ms | 75.472 ms | 75.765 ms | 5.068 ms | 41.792 ms |
| restart1024 | 71.760 ms | 76.805 ms | 77.099 ms | 5.068 ms | 32.458 ms |
| output3008 | 70.427 ms | 75.472 ms | 75.765 ms | 5.068 ms | 42.458 ms |
| persistent-transport32 | 70.427 ms | 75.472 ms | 75.765 ms | 5.068 ms | 42.458 ms |

The fixture gates passed marker propagation, monotonic output ordering, p95 <=
100 ms, p99 <= 110 ms, and steady-state jitter p99-p50 <= 12 ms. The
`prefetch64` model reduces the synthetic p95 by 1.333 ms (1.736% relative to
the fixture baseline); `restart1024` reduces the first restart response while
leaving steady-state p95 unchanged. These are timeline-fixture results only:
they are not input-to-output measurements from a DVS signal, a turntable, or
Traktor, and they do not authorize promotion.

The `persistent-transport32` fixture also passed all of those gates. Its
modeled steady-state p95 is `0.667 ms` lower than baseline, with the same
restart response as baseline. The corresponding offline work proxy rises by
`88.889%` because the model counts 32-frame capture completions rather than
the current 64-frame baseline. That tradeoff must be measured on the exact
candidate before promotion; it is not evidence that the real driver has
achieved the modeled result.

The next transport iteration was built and checked offline as
`local-analysis/candidates/prepared-medium.driver`. It uses four prepared slots
per USB submit (32 frames per request), eight capture and eight playback live
requests, and a 16-slot preallocated request pool. Its HAL executable SHA-256
was `5a78c1e94e34b7941ab22e0ef4b53361cfa72f0aca335c6e81d3cfc90f8c0996`; the
restored default output3072 HAL remained
`79390010acbd96b799d3f69d9f1ae92ccaec68e37439ae4a54a2ab91ea091098`.

The persistent transport contract passed for the medium candidate over 128
completion periods per direction: 256 steady submits, 16 maximum live
requests, 256-frame maximum lead in each direction, zero slot mismatches,
zero fallback allocations, continuous sequence/timestamp accounting, and
complete drain. The expected offline submit reduction is 4x versus one-slot
submits. This is a model and bundle contract only: it has no HAL binding,
physical USB evidence, sound-quality evidence, CPU superiority evidence, or
Traktor timecode evidence, so it remains default-off and is not promoted.

## Promotion status

No candidate was promoted. The complete runner returned a nonzero status because
the safety gates correctly retain these blockers:

- exact-artifact physical HAL safety smoke is missing;
- the Audio 8 DJ to iRig source-reference matrix has not passed in the current
  session;
- same-session product sound and CPU comparisons are not proven;
- the Traktor timecode/vinyl physical gate is missing;
- the isolated worktree is not a claimable current-release provenance surface.

The runner also preserved the existing diagnostic failures for route
contamination and iRig idle capture. Those failures are not converted into a
product claim and must be resolved with a fresh lock-gated physical session.

The next valid promotion step is a fresh physical comparison using the exact
candidate artifact, with the existing stable 3072 profile as control. Until that
exists, `output3008`, `output2304`, `output2048`, and `host256` remain laboratory
candidates only, and the stable profile remains the only defensible default.

## Physical candidate validation

The later lock-gated tests used the real Audio 8 DJ USB device, iRig Stream at
48 kHz for the capture path, pair B, and then restored iRig Stream to its prior
44.1 kHz setting. The hardware lock was free after every run. These tests did
touch CoreAudio and the external capture path. The safety and cleanup paths
restarted or recovered CoreAudio where required; they did not reset USB, change
default devices, or install a candidate permanently.

Candidate artifacts and safety results:

| Candidate | HAL executable SHA-256 | Safety result | Physical result | Decision |
| --- | --- | --- | --- | --- |
| output3072 control | `79390010acbd96b799d3f69d9f1ae92ccaec68e37439ae4a54a2ab91ea091098` | prior stable safety evidence | prior physical route/sound evidence | retain as stable |
| output2560 | `e18647f3902b3c9a66977c59305251e8b95421650a5c3a66e7ff34c2bd41a353` | FAIL, CoreAudio health 108.1% CPU during load | not eligible | reject |
| output2816 | `1ceae118c56cf31af1b34d12bf22e2fc8341e73c8180e94a82ad17871ee49052` | PASS, two cycles, clean recovery | FAIL: alignment 0.958982, SNR 4.94 dB, lag jumps 22 | reject |
| output2048 | `402dafe1f24ccb4ecaa7cac09b91a810dc87b7d16aae3e566ed1aed7c4649617` | FAIL on two-cycle repeat; post-unload CoreAudio reached 170.0% CPU | not eligible | reject |
| output2304 | `b9a5b9831d8c414a47c1d40f71ec8e2e813cc1ee85a4ce03099445a38b5d23fc` | PASS, two cycles, clean recovery | FAIL: first energy 0.70 s, correlation 0.284, aligned SNR -1.58 dB, lag jumps 22 | reject |
| prefetch64 | `de9e1cfc67e6b7833e19e1cae6843fa972f2fe3430a89f9fc41cc1335d2ebeba` | FAIL on first cycle; CoreAudio 125.6% CPU and total watched 134.5% | not eligible; physical sound test intentionally skipped | reject |
| restart1024 | `dce993ccf0763fc8915b84875f2e6618875340691cffa46b970e24eeb825d2b6` | PASS, two cycles, clean recovery | FAIL: alignment 0.958576, SNR 4.95 dB, lag jumps 22; CPU peaks 63.1% CoreAudio and 153.9% total watched | reject |
| start2816 | `9acd9b948fe5b5df4865f6e2498a66f5a5f2d953a3093a34315dc0b63bf71086` | FAIL on post-unload guard; CoreAudio 105.3% CPU and total watched 114.5% | not eligible; physical sound test intentionally skipped | reject |
| responsive512 | `d13747097e1bd3e7baf779eb43d7da6d526e1c9985b628b9c22fe9e77c5ac0ef` | PASS, two cycles, clean recovery | FAIL: alignment 0.959289, SNR 4.96 dB, lag jumps 23; no clipping | reject |
| output3008 | `60585c8e66c23363cb04784cdea471a996783c5365d42980d148f364c41f1101` | FAIL on post-unload guard; CoreAudio 138.2% CPU and total watched 145.1% | not eligible; physical sound test intentionally skipped | reject |

The output2304 physical run also recorded zero capture status failures, zero
capture transaction errors, zero playback transfer errors, zero output
underruns, zero active underruns, zero late-write frames, and zero panic flags.
Those transport counters are useful diagnostics but do not override the failed
quality gate. Its physical-latency analyzer reported a 0.70 s first-energy
observation, best absolute correlation 0.284, aligned SNR -1.58 dB, linear-fit
SNR -10.53 dB, and linear residual 0.958 of capture RMS. The result is
diagnostic only and cannot support a product-quality or responsive-vinyl claim.

A same-procedure isolated control run makes the performance rejection stronger:
the output2304 capture had native quality alignment `0.959203` versus `0.959141`
for output3072, but its sampled CPU profile reached `83.6%` CoreAudio and
`188.2%` total audio UI/services, versus `12.2%` and `44.8%` for the 3072
control. The full same-session A/B attempt was also invalid because the 3072
control itself entered a `175.3%` CoreAudio runaway during the load window.
That session is recorded as invalid comparison evidence, not as a promotion
pass.

The output2816 run was materially calmer than output2304: sampled CoreAudio
peaked at `25.5%`, total audio UI/services at `61.9%`, and the driver at `7.3%`.
Those are useful performance observations, but the sound-quality gate still
failed and the candidate therefore remains laboratory-only.

The prefetch64 run kept the output target at 3072 frames and reduced only
`HAL_OUTPUT_PREFETCH_FRAMES` from 128 to 64. Its offline model predicted a
small 1.695% latency reduction with no modeled work increase, but the exact
candidate failed the first live safety cycle: CoreAudio reached `125.6%` CPU
and the total watched audio workload reached `134.5%`, above the configured
120% safety ceiling. Core Audio enumeration still passed and the recovery
window returned to `audio_stack_health=PASS`; because the safety gate failed,
no physical sound or latency test was run for this candidate. It is rejected.

The restart1024 candidate kept the 3072-frame start and steady-state target,
and reduced only the discontinuity restart preroll from 1536 to 1024 frames.
It passed two safety cycles and recovered cleanly, but the isolated physical
soundcheck still failed with quality alignment `0.958576`, analog SNR `4.95`
dB, and 22 lag jumps over two frames. The run had no clipping and no transfer
errors, but the quality gate is decisive. CPU sampled during the soundcheck
peaked at `63.1%` for CoreAudio and `153.9%` for the total watched audio/UI
workload. No promotion is allowed.

The start2816 candidate kept the 3072-frame steady-state target and restart
preroll, changing only the first-start preroll from 3072 to 2816 frames. Its
loaded-cycle health initially passed, but the post-unload safety guard failed:
CoreAudio reached `105.3%` CPU and the watched total reached `114.5%`, with the
candidate unloaded. Recovery passed, so no physical sound test was attempted.
The subsequent stable restore encountered a separate CoreAudio recovery spike
(`108.9%`, total `128.0%`); the explicit `audio-stack-health --reset` recovery
then passed, and the stable output3072 bundle was loaded and verified again.

The responsive512 candidate retained the output target and prefetch settings
and reduced only the input maximum-latency allowance to 512 frames. It passed
two safety cycles and clean recovery. The isolated audio route still failed
the strict quality gate with alignment `0.959289`, analog SNR `4.96` dB, and
23 lag jumps, although no clipping was observed. This fixture does not carry
Traktor timecode, so it cannot prove DVS input-latency improvement; the
candidate is rejected pending a real timecode stimulus and a valid product
comparison.

The output3008 candidate reduced the output start and steady-state target from
3072 to 3008 frames and restart preroll from 1536 to 1504 frames. Its offline
fixture improved p95 from `76.805 ms` to `75.472 ms`, but the live safety gate
failed after unload: CoreAudio reached `138.2%` CPU and the watched total reached
`145.1%`, above the `120%` ceiling. CoreAudio enumeration still passed and the
recovery returned to `audio_stack_health=PASS`; no physical sound or latency
test was run. The candidate is rejected despite the modeled improvement.

### Source-reference Audio 8 to iRig A/B window

The source-reference plan and preflight passed after the reference WAV was
explicitly supplied. The executed same-session window then failed both absolute
sound-quality gates, so it does not support promotion or a product latency
claim. The candidate side used the current stable `output3072` bundle; it was
not a new optimization candidate.

| Run | Quality alignment | SNR floor | Mid residual | High residual | Quiet mid noise | Lag jumps >2 frames | Driver CPU p95 | CoreAudio CPU p95 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| mainline reference | 0.514801 | -5.51 dB | 2.985026 | 3.830859 | -48.66 dBFS | 38 | 5.2% | 9.3% |
| stable output3072 candidate | 0.958731 | -11.59 dB | 3.974424 | 3.899467 | -45.47 dBFS | 24 | 6.3% | 5.1% |

The candidate improved relative alignment and lag-jump count versus the
mainline run, but failed its relative SNR, residual, quiet-noise, and driver-CPU
gates. More importantly, neither run met the absolute product thresholds of
`quality_alignment >= 0.98`, `SNR >= 35 dB`, zero lag jumps, and the configured
residual/noise ceilings. The physical window therefore returned `FAIL`, with no
promotion. The run also lacked the required audiophile WAV analyses, and no
Traktor timecode stimulus was used.

Evidence for this window is under
`local-analysis/physical-evidence-window/20260725T193436Z/source-reference-ab`,
including `physical-window-preflight.json`, both `metrics.json` files,
`same-session-physical-compare.json`, `promotion-readiness.json`, and the two
CPU profiles. The window acquired the shared hardware lock, loaded/unloaded the
Audio 8 HAL, and performed CoreAudio recovery during cleanup. It did not reset
USB, alter default devices, or use a physical turntable. The known-good
`local-analysis/candidates/output3072/OpenA8DJ.driver` bundle was reloaded
afterward and passed one safety cycle; Audio 8 DJ returned as 8x8 at 48 kHz,
`audio_stack_health=PASS`, and the lock was `LOCK_FREE`.

## Final post-test state

After the candidate runs, the stable output3072 bundle was loaded again and
passed one safety cycle. Core Audio enumerated Open Audio 8 DJ as 8 inputs and
8 outputs at 48 kHz; iRig Stream was restored to 44.1 kHz; final audio-stack
health was `PASS`; and the hardware lock was `LOCK_FREE`. A final repeat after
the prefetch64 rejection used
`local-analysis/hal-candidate-safety/final-stable-output3072-after-prefetch64`
and reached the same healthy state. No latency candidate was left installed or
promoted.

After the output3008 failure, the first attempted restore used the newly built
output3072-named artifact and was correctly rejected by the safety guard during
load (`coreaudiod=142.5%`, enumeration failure). The recovery path unloaded the
bundle and returned to a healthy stack. A second restore explicitly loaded the
known-good `local-analysis/candidates/output3072/OpenA8DJ.driver` bundle; it
passed the safety gate with `audio_stack_health=PASS`, CoreAudio at `0.0%`, and
the required Audio 8 DJ device present. The post-restore check confirmed
Open Audio 8 DJ at 8x8/48 kHz, iRig Stream at 44.1 kHz, and a free hardware
lock.

After the source-reference A/B window, the same known-good output3072 bundle
was restored with
`local-analysis/hal-candidate-safety/restore-stable-output3072-after-source-reference`.
The restore passed one safety cycle. The final non-destructive checks showed
Open Audio 8 DJ at 8x8/48 kHz, iRig Stream at 44.1 kHz,
`audio_stack_health=PASS`, and `LOCK_FREE`.

The result is consistent with the offline frontier: lower output targets can
reduce modeled delay without preserving the real capture quality and service
stability required for DVS. No candidate is promoted. The installed/default
profile remains output3072 until a future exact-artifact physical window passes
both safety and sound-quality gates.

After the fixture and documentation changes, the final non-interactive rerun
reported latency gate `PASS` with six e2e fixture profiles and zero hard
failures; the C++ release suite remained `89/89` passed. A non-destructive
post-run health check still showed Open Audio 8 DJ as 8 inputs/8 outputs at
48 kHz, iRig Stream at 44.1 kHz, `audio_stack_health=PASS`, and
`hardware_lock=LOCK_FREE`. This check did not reset CoreAudio or change any
device configuration.

The broader offline runner still returns a diagnostic nonzero status because its
readiness packet intentionally preserves three missing historical physical
artifacts and the physical promotion gates remain closed. The final objective
auditor now reports only the external blockers: full DriverKit/deXt runtime
proof, a fresh same-session Audio8-to-iRig A/B comparison, runtime CPU/resource
superiority, and the real Traktor/timecode-vinyl window. No stale-HEAD blocker
remains.

## Evidence files

- `local-analysis/timecode-latency/offline/latency-offline-gate.json`
- `local-analysis/timecode-latency/offline/e2e-baseline.json`
- `local-analysis/timecode-latency/offline/e2e-prefetch64.json`
- `local-analysis/timecode-latency/offline/e2e-restart1024.json`
- `local-analysis/timecode-latency/offline/e2e-output2816.json`
- `local-analysis/timecode-latency/offline/e2e-output2560.json`
- `local-analysis/timecode-latency/offline/e2e-output2304.json`
- `local-analysis/timecode-latency/offline/e2e-output3008.json`
- `local-analysis/timecode-latency/offline/result.txt`
- `local-analysis/cpp-offline/current-offline-gates.json`
- `local-analysis/candidates/prepared-medium.json`
- `local-analysis/candidates/prepared-medium-bundle.json`
- `local-analysis/candidates/prepared-medium.driver`
- `/tmp/opena8dj-persistent-medium.json`
- `/tmp/opena8dj-cpp-offline-gates.log`
- `local-analysis/hal-candidate-safety/candidate2304-cycles2`
- `local-analysis/physical-superiority-window/20260725-output2304-physical-pairB-48000`
- `local-analysis/physical-superiority-window/20260725-output3072-control-physical-pairB-48000`
- `local-analysis/physical-superiority-window/20260725-output2304-vs-3072-same-session-pairB-48000`
- `local-analysis/hal-candidate-safety/candidate2816-cycles2`
- `local-analysis/physical-superiority-window/20260725-output2816-physical-pairB-48000`
- `local-analysis/hal-candidate-safety/prefetch64-cycles2`
- `local-analysis/hal-candidate-safety/final-stable-output3072-after-prefetch64`
- `local-analysis/hal-candidate-safety/restart1024-cycles2`
- `local-analysis/physical-superiority-window/20260725-restart1024-physical-pairB-48000`
- `local-analysis/hal-candidate-safety/final-stable-output3072-after-restart1024`
- `local-analysis/hal-candidate-safety/start2816-cycles2`
- `local-analysis/hal-candidate-safety/final-stable-output3072-after-start2816`
- `local-analysis/hal-candidate-safety/final-stable-output3072-after-start2816-reset`
- `local-analysis/hal-candidate-safety/output3008-cycles2`
- `local-analysis/hal-candidate-safety/restore-stable-output3072-after-output3008`
- `local-analysis/physical-evidence-window/20260725T193436Z/source-reference-ab`
- `local-analysis/hal-candidate-safety/restore-stable-output3072-after-source-reference`
- `local-analysis/hal-candidate-safety/responsive512-cycles2`
- `local-analysis/physical-superiority-window/20260725-responsive512-physical-pairB-48000`
- `local-analysis/hal-candidate-safety/final-stable-output3072-after-responsive512`
