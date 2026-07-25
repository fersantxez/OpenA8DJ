# Timecode latency offline evidence

Date: 2026-07-25
Worktree: `/Users/fer/dev/opena8dj-latency-lab`
Branch: `codex/timecode-latency`
Base revision: `27a8410`

## Scope

This evidence records the non-interactive implementation and offline validation of
the timecode/vinyl latency instrumentation. It is diagnostic evidence only. It
does not prove physical DVS latency, sound quality, Traktor behavior, or product
readiness.

The stable output profile was intentionally left unchanged:

- `HAL_OUTPUT_TARGET_LATENCY_FRAMES=3072`
- `HAL_INPUT_MAX_LATENCY_FRAMES=0`
- `HAL_OUTPUT_PREFETCH_FRAMES=128`

No USB device, Audio MIDI Setup device, CoreAudio service, or physical turntable
was touched during this run.

## Implemented gates

- `scripts/timecode-latency-offline-gate` runs a deterministic profile matrix,
  validates output/host geometry, compares modeled latency against the stable
  baseline, and writes JSON evidence.
- `scripts/test-timecode-latency-offline-gate.py` covers the baseline, the
  `output2048` and `host256` candidates, unsafe geometry rejection, and monotonic
  modeled reductions.
- `scripts/run-cpp-offline-gates` includes the latency gate and its test while
  preserving diagnostic failures for route contamination and iRig idle capture.
- The CPU-pool candidate contract now correctly reports that a build-only helper
  cannot claim physical evidence or product readiness.

## Offline results

The latency gate result was `PASS`, with `model_only=true`,
`physical_evidence_present=false`, `product_claim_allowed=false`, and no hard
failures. At 48 kHz, the modeled pipeline p95 values were:

| Profile | Modeled p95 | Modeled reduction | Work proxy change |
| --- | ---: | ---: | ---: |
| baseline | 78.667 ms | 0.000% | 0.000% |
| capture32 | 78.000 ms | 0.847% | +88.889% |
| capture16 | 77.667 ms | 1.271% | +266.667% |
| output2560 | 68.000 ms | 13.559% | 0.000% |
| output2816 | 73.333 ms | 6.780% | 0.000% |
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
exists, `output2304`, `output2048`, and `host256` remain laboratory candidates
only, and the stable profile remains the only defensible default.

## Physical candidate validation

The later lock-gated tests used the real Audio 8 DJ USB device, iRig Stream at
48 kHz for the capture path, pair B, and then restored iRig Stream to its prior
44.1 kHz setting. The hardware lock was free after every run. These tests did
touch CoreAudio and the external capture path; they did not reset USB, restart
CoreAudio, change default devices, or install a candidate permanently.

Candidate artifacts and safety results:

| Candidate | HAL executable SHA-256 | Safety result | Physical result | Decision |
| --- | --- | --- | --- | --- |
| output3072 control | `79390010acbd96b799d3f69d9f1ae92ccaec68e37439ae4a54a2ab91ea091098` | prior stable safety evidence | prior physical route/sound evidence | retain as stable |
| output2560 | `e18647f3902b3c9a66977c59305251e8b95421650a5c3a66e7ff34c2bd41a353` | FAIL, CoreAudio health 108.1% CPU during load | not eligible | reject |
| output2816 | `1ceae118c56cf31af1b34d12bf22e2fc8341e73c8180e94a82ad17871ee49052` | PASS, two cycles, clean recovery | FAIL: alignment 0.958982, SNR 4.94 dB, lag jumps 22 | reject |
| output2048 | `402dafe1f24ccb4ecaa7cac09b91a810dc87b7d16aae3e566ed1aed7c4649617` | FAIL on two-cycle repeat; post-unload CoreAudio reached 170.0% CPU | not eligible | reject |
| output2304 | `b9a5b9831d8c414a47c1d40f71ec8e2e813cc1ee85a4ce03099445a38b5d23fc` | PASS, two cycles, clean recovery | FAIL: first energy 0.70 s, correlation 0.284, aligned SNR -1.58 dB, lag jumps 22 | reject |

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

The result is consistent with the offline frontier: lower output targets can
reduce modeled delay without preserving the real capture quality and service
stability required for DVS. No candidate is promoted. The installed/default
profile remains output3072 until a future exact-artifact physical window passes
both safety and sound-quality gates.

## Evidence files

- `local-analysis/timecode-latency/offline/latency-offline-gate.json`
- `local-analysis/timecode-latency/offline/result.txt`
- `local-analysis/cpp-offline/current-offline-gates.json`
- `/tmp/opena8dj-cpp-offline-gates.log`
- `local-analysis/hal-candidate-safety/candidate2304-cycles2`
- `local-analysis/physical-superiority-window/20260725-output2304-physical-pairB-48000`
- `local-analysis/physical-superiority-window/20260725-output3072-control-physical-pairB-48000`
- `local-analysis/physical-superiority-window/20260725-output2304-vs-3072-same-session-pairB-48000`
- `local-analysis/hal-candidate-safety/candidate2816-cycles2`
- `local-analysis/physical-superiority-window/20260725-output2816-physical-pairB-48000`
