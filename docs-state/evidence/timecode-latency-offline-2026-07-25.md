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

## Evidence files

- `local-analysis/timecode-latency/offline/latency-offline-gate.json`
- `local-analysis/timecode-latency/offline/result.txt`
- `local-analysis/cpp-offline/current-offline-gates.json`
- `/tmp/opena8dj-cpp-offline-gates.log`

