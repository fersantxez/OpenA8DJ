# Mainline Findings Integrated On 2026-06-14

This file records new C/Objective-C mainline findings that affect the
OpenA8DJ-rust design. The mainline worktree remains read-only reference
material.

## 0.3.133 Is Internally Healthy But Not A Listening Candidate

Source:
`/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md`.

Finding:

- Sequential locked internal gates passed after shared audio-gate hardening.
- Output-pair smoke passed for A/B/C/D.
- Timecode smoke passed.
- Playback CPU/UI stress passed with driver p95 around `6.8%`,
  `coreaudiod` p95 around `1.8%`, no timeline resets, and no active underruns.
- Integrated candidate preflight still ended
  `BLOCKED_PHYSICAL_CAPTURE` because iRig was missing from USB/Core Audio.

Rust implication:

- Treat `0.3.133` as a useful internal-performance reference, not as an
  audiophile baseline.
- Rust may model the ISO64/q8 policy as a candidate runtime policy, but it must
  not promote that policy without valid physical capture and listening.
- Internal pass states must remain separate from physical quality states.

## Shared Gate Lock Prevents Contaminated Measurements

Finding:

- Output-pair, timecode, playback CPU, candidate preflight, and autonomous QA
  now use the same user-global lock:
  `$HOME/.opena8dj/hardware-gate.lock`.
- Busy lock tests return exit code `75` and record owner pid/gate/run directory
  instead of running a contaminated measurement.
- Autonomous QA skips cycles as `SKIPPED_BUSY` when another agent owns the lock.

Rust implication:

- Rust hardware/audio/CPU-sensitive tooling must keep using the same lock.
- Rust readiness should preserve `BLOCKED_LOCK_BUSY` and `SKIPPED_BUSY` as
  explicit statuses rather than converting them to generic failures.
- Autonomous Rust supervisors, if added later, must be low priority and skip
  when the lock is busy.

## Capture Diagnosis Now Carries USB-Port Detail And Recovery Action

Finding:

- Mainline capture diagnosis now emits:
  - `usb_enumeration_failures`;
  - `failed_usb_ports`;
  - `next_recovery_action`;
  - exact iRig USB VID/PID readiness for `0x1963:0x0059`;
  - Core Audio iRig input readiness.
- The current missing-iRig state reports a failed `AppleUSB20HubPort` and the
  recovery hint:
  `power_cycle_iRig_or_move_iRig_to_direct_mac_port_then_rerun_capture_device_diagnose`.

Rust implication:

- Rust capture-readiness schemas must preserve USB-port failure details and
  next recovery action fields.
- Missing iRig is not only "no capture"; it may be a USB-enumeration recovery
  state that should block physical gates without hiding the cause.

## Stable iRig Readiness Is Required Before Physical Gates

Finding:

- Candidate watch requires stable iRig readiness for consecutive polls before
  running candidate preflight.
- Mainline default is `stable_polls=3`.
- If iRig appears transiently but does not remain stable, the blocked reason is
  `irig_not_stable_yet`.

Rust implication:

- Rust physical gates must not start from a single iRig appearance.
- Readiness should include `ready_streak`, `stable_polls`, and
  `BLOCKED_IRIG_UNSTABLE` or equivalent state.

## Spectral Coloration Is Now A First-Class Physical Music Gate

Finding:

- Mainline music analysis now emits:
  - `low_band_capture_to_ref_gain_db`;
  - `mid_band_capture_to_ref_gain_db`;
  - `high_band_capture_to_ref_gain_db`;
  - `mid_vs_low_coloration_delta_db`;
  - `high_vs_low_coloration_delta_db`;
  - `high_vs_mid_coloration_delta_db`;
  - `metallic_coloration_score_db`.
- Absolute gate:
  - `mid_vs_low` within `+/-5 dB`;
  - `high_vs_low` within `+/-6 dB`;
  - `metallic_coloration_score_db <= 6 dB`.
- Relative gate:
  - if a baseline has the same keys, candidate coloration must stay within
    baseline + `0.75 dB`.
- Offline calibration confirmed the metric catches the historical
  metallic/noisy listening failure.

Rust implication:

- Rust metrics and physical-gate schemas must include spectral-coloration
  fields.
- A Rust candidate cannot claim "audiophile" based only on residual/clicks; it
  must also pass coloration.

## Autonomous Supervisor Can Contaminate CPU Gates

Finding:

- Mainline autonomous recovery polling every 10 seconds could push
  `coreaudiod` p95 over the `8%` threshold during playback CPU gates.
- Slower defaults were adopted: poll every `30s`, recover for `20s`, recover
  every `10` cycles, while keeping approximate recovery cadence around five
  minutes.
- After slow polling, playback CPU gate passed again with `coreaudiod` p95
  around `3.2%` and stress p95 around `1.6%`.

Rust implication:

- Supervisors are not free. Even read-only Core Audio/USB polling can pollute
  CPU measurements.
- Any Rust supervisor must be slow, lock-aware, and disabled or skipped during
  higher-priority gates unless it owns the lock for a bounded window.

## Immediate Rust Integration Tasks

1. Add these findings to the product and audiophile design docs.
2. Add capture-readiness, stable-poll, USB-port, recovery-action, supervisor,
   and spectral-coloration fields to the future Rust metrics schema.
3. Keep Rust's first implementation milestone hardware-free: topology,
   routing, sample, mode2, and metrics types.
