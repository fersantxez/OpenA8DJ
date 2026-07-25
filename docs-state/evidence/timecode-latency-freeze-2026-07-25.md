# Timecode latency freeze: experimental output3072

Date: 2026-07-25
Branch: `codex/timecode-latency`
Decision: freeze `output3072` as the usable experimental version now.

## Why this version

The `output2816` profile was the attractive latency compromise in the earlier
frontier, but its fresh exact-artifact safety repeat failed during the first
post-unload recovery cycle. It is therefore rejected for activation today.
The `output3072` profile keeps the responsive 3072-frame geometry and has the
strongest repeatable recovery evidence available in this checkout. This makes
it the correct temporary choice when sound safety and immediate usability have
priority over another small latency reduction.

## Frozen artifact

- Bundle: `build/OpenA8DJ-frozen-stable3072.driver`
- Executable SHA-256:
  `c6e4d491e35e73d90109cab33c71a616173d002fbc6fa2519c241512eb85c951`
- Manifest: `build/hal-candidates/frozen-stable3072.json`
- Build target: `hal-timecode-frozen-good-output3072-candidate`
- Local bundle signature: PASS (ad hoc/local validation only)
- Official Developer ID signature/notarization: NOT CLAIMED

The normal `build/OpenA8DJ.driver` bundle was rebuilt as the `output3072`
rollback after the named artifact was copied. The frozen bundle was then used
by the exact-artifact safety run, so the currently loaded HAL matches the
frozen executable hash above.

## Configuration and measured gates

- Reference sample rate: 48 kHz
- Host buffer: 512 frames
- Capture ISO batch: 64 frames
- Output start latency: 3072 frames
- Output restart latency: 1536 frames
- Output target: 3072 frames
- Elastic high water: 9216 frames
- Offline modeled pipeline: 78.667 ms p95 for the baseline geometry
- Offline C++/fixture gates: PASS
- Exact-artifact HAL safety: PASS, one cycle, 20-second stabilization window
- Required device: Open Audio 8 DJ present, 8 inputs / 8 outputs at 48 kHz
- Final audio stack health: PASS
- Final watched CPU sample: 0.2% maximum watched process, 0.0% driver/coreaudiod
- Hardware lock after validation: free

Evidence directories:

- `local-analysis/hal-candidate-safety/frozen-stable3072-final`
- `local-analysis/hal-candidate-safety/restore-stable-output3072-after-freeze-failure-wait20`
- `local-analysis/timecode-latency/offline`

## Limits

This is a frozen experimental usability build, not a public-ready release.
The offline latency result is a model and fixture result, not a physical
turntable measurement. The physical Audio 8 to iRig comparison remains below
the sound-quality gate, a wired known-good route is still unavailable, and the
Traktor/timecode-vinyl window is not certified. No claim of audiophile quality,
official DriverKit/deXt readiness, or production promotion is allowed from
this freeze.

Rollback target: rebuild or restore `hal-timecode-frozen-good-output3072-candidate`
into `build/OpenA8DJ.driver` and rerun the guarded safety check before any
future candidate activation.
