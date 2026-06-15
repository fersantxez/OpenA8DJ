# OpenA8DJ-rust PM Success Metrics

This is the authoritative product-metric contract for OpenA8DJ-rust.

Stable path for mainline agents:

```text
/Users/fer/dev/audio8djrust/docs/RUST_PM_SUCCESS_METRICS.md
```

The mainline C/Objective-C worktree remains read-only for Rust work. Mainline
agents may read this file to understand the Rust target and to report new
mainline evidence that should update the target.

## PM Objective

Build `OpenA8DJ-rust`, a modular Rust driver that meets or exceeds the current
C/Objective-C mainline on:

- physical audio quality;
- complete Audio 8 DJ I/O and routing;
- Traktor/timecode vinyl and CD-line behavior;
- low CPU and low resource use;
- stability under Core Audio, USB, hotplug, sleep/wake, UI stress, and
  multi-client use;
- maintainable architecture and deterministic tests.

Clean internal counters are necessary but not sufficient. Physical output,
physical capture, DVS behavior, and human listening are the product authority.

## Baseline Policy

### Current mainline reference

`0.3.133` is the current internal-performance reference:

- output-pair smoke: PASS;
- timecode smoke: PASS;
- playback CPU/UI stress: PASS;
- driver p95 around `6.8%`;
- `coreaudiod` p95 around `1.8%` in clean locked runs;
- stress `coreaudiod` p95 around `1.6%`;
- timeline resets: `0`;
- active underruns: `0`;
- candidate status: `NOT_READY` because physical capture is blocked by missing
  iRig USB/Core Audio enumeration.

Rust must treat this as an internal baseline only. It is not an audiophile
baseline until valid physical capture and listening pass.

### Rust target relationship to mainline

OpenA8DJ-rust must:

- match or beat mainline internal performance;
- pass stricter physical audio gates;
- preserve or improve DVS/timecode behavior;
- provide clearer metrics and architecture;
- never regress mainline hardware workflow expectations.

## Required Status Vocabulary

Use these statuses consistently in Rust metrics, run artifacts, dashboards, and
handoffs:

```text
PASS
FAIL
NOT_READY
BLOCKED_LOCK_BUSY
BLOCKED_PHYSICAL_CAPTURE
BLOCKED_DIRTY_ROUTE
BLOCKED_USB_ENUMERATION
BLOCKED_IRIG_UNSTABLE
BLOCKED_UNVALIDATED_DVS
BLOCKED_STALE_HASH
SKIPPED_BUSY
```

Do not collapse blocked states into generic failure. The product decision is
different when a candidate fails quality versus when the capture path is absent
or another agent owns the hardware lock.

## Internal Runtime Metrics

Rust must meet these before physical testing:

| Metric | Minimum | Stretch target |
|---|---:|---:|
| device start latency | `<= 0.25s` | `<= 0.10s` |
| first callback latency | `<= 0.30s` | `<= 0.12s` |
| driver CPU average | `<= 10%` | `<= 5.5%` |
| driver CPU p95 | `<= 12%` | `<= 6.5%` |
| stress driver CPU average | `<= 10%` | `<= 5.5%` |
| stress driver CPU p95 | `<= 12%` | `<= 6.5%` |
| `coreaudiod` p95 | `<= 8%` | `<= 1.5%` |
| stress `coreaudiod` p95 | `<= 8%` | `<= 1.5%` |
| WindowServer p95 during UI stress | `<= 45%` | `<= 20%` |
| `outputFramesWritten` | `> 0` | required |
| `outputFramesRead` | `>= 0.90 * written` | `>= 0.995 * written` |
| output timeline resets | `0` | `0` |
| active underruns | `0` | `0` |
| output panic flags | `0` | `0` |
| playback queue failures | `0` | `0` |
| input check errors in non-diagnostic run | `0` | `0` |

Rust cannot claim lower CPU if output read, timeline, residual, sideband, click,
or DVS quality regresses.

## Physical Tone Metrics

Physical tone gates require a valid post-DAC capture route:

| Metric | Minimum | Stretch target |
|---|---:|---:|
| capture peak | `0.020..0.920` | `0.100..0.800` |
| `sideband_ratio` | `<= 0.008` | `<= 0.004` |
| segment sideband p95 | `<= 0.006` | `<= 0.003` |
| segment sideband max | `<= 0.008` | `<= 0.004` |
| strongest 940/1060-ish sideband | `<= -43 dB` | `<= -50 dB` |
| click outliers | `0` | `0` |
| segment click rate | `0` | `0` |

The tone gate must reject candidate builds that only look clean in internal
transport counters.

## Physical Real-Music Metrics

Physical music gates require a valid post-DAC capture route:

| Metric | Minimum | Stretch target |
|---|---:|---:|
| `measurement_status` | `VALID` | `VALID` |
| `candidate_quality_status` | `PASS` | `PASS` |
| `verdict` | `PASS` | `PASS` |
| alignment | `>= 0.970` | `>= 0.985` |
| capture RMS | `-28..-10 dBFS` | route dependent |
| clipped frames | `0` | `0` |
| 1-5 kHz residual | `<= 1.38` | `<= valid baseline * 0.98` |
| 1-5 kHz window p95 | `<= 1.40` | `<= valid baseline * 0.98` |
| 1-5 kHz window max | `<= 1.46` | `<= valid baseline` |
| 1-5 kHz p95/median | `<= 1.03` | `<= 1.01` |
| 1-5 kHz max/median | `<= 1.06` | `<= 1.03` |
| 5-12 kHz residual | `<= 1.32` | `<= valid baseline * 0.98` |
| quiet 1-5 kHz noise | `<= -32.5 dBFS` | `<= -36 dBFS` |
| click outliers | `0` | `0` |
| lag jumps over 2 frames | `<= 3` | `0` |
| CPU/noise correlation | `<= 0.08` | `<= 0.04` |
| driver CPU average | `<= 8%` | `<= 5.5%` |
| driver CPU p95 | `<= 12%` | `<= 6.5%` |
| `coreaudiod` p95 | `<= 8%` | `<= 1.5%` |

When a valid physical baseline exists, Rust must not regress against it even if
absolute metrics pass.

## Spectral Coloration Metrics

Rust must include and pass the physical spectral-coloration fields:

| Metric | Minimum | Stretch target |
|---|---:|---:|
| `mid_vs_low_coloration_delta_db` | within `+/-5 dB` | within `+/-2 dB` |
| `high_vs_low_coloration_delta_db` | within `+/-6 dB` | within `+/-2.5 dB` |
| `metallic_coloration_score_db` | `<= 6 dB` | `<= 2.5 dB` |
| baseline-relative coloration | `<= baseline + 0.75 dB` | `<= baseline` |

Required fields:

```text
low_band_capture_to_ref_gain_db
mid_band_capture_to_ref_gain_db
high_band_capture_to_ref_gain_db
mid_vs_low_coloration_delta_db
high_vs_low_coloration_delta_db
high_vs_mid_coloration_delta_db
metallic_coloration_score_db
```

## Capture Readiness Metrics

Before any long physical gate:

| Metric | Required |
|---|---|
| `physical_capture_status` | `READY` |
| `found_irig_usb_by_id` | `1` |
| iRig VID/PID | `0x1963:0x0059` |
| `found_irig_core_audio` | `1` |
| `ready_streak` | `>= stable_polls` |
| `stable_polls` | `>= 3` |
| `usb_enumeration_failures` | `NO` |
| `failed_usb_ports` | empty or `none` |

Blocked capture readiness must preserve:

```text
physical_capture_status
physical_capture_reason
found_audio8_core_audio
found_irig_usb_by_id
found_irig_core_audio
external_core_audio_input_count
usb_enumeration_failures
failed_usb_ports
next_recovery_action
ready_streak
stable_polls
```

## DVS / Timecode Metrics

Rust cannot claim DVS readiness unless all are true at 44.1 and 48 kHz:

- public Core Audio surface exposes 8 inputs and 8 outputs;
- output streams are Output A/B/C/D;
- input stream exposes Input A/B/C/D pair names;
- `timecode-vinyl` profile applies mode `0`, ground-lift policy, and software
  lock;
- `timecode-cd-line` profile applies mode `1`;
- DVS profiles reset input remap, left/right swap, and polarity inversion to
  identity;
- Deck A timecode input is Input A only;
- Deck B timecode input is Input B only;
- Output A/B remain isolated while Input A/B timecode is active;
- no input pair leakage above gate threshold;
- no channel swap after StartIO/StopIO, rate change, hotplug, or sleep/wake;
- no timecode dropouts during CPU/UI stress;
- Traktor scope validation passes physically;
- human operator confirms absolute/relative mode behavior.

## Supervisor And Lock Metrics

Any Rust supervisor must be lower priority than physical capture, human
listening, install/reload/recovery windows, and active quality gates.

Required behavior:

- use `$HOME/.opena8dj/hardware-gate.lock`;
- skip as `SKIPPED_BUSY` when another owner holds the lock;
- record owner pid/gate/run directory on busy lock;
- default to slow polling;
- avoid raising `coreaudiod` during playback CPU or physical quality gates;
- never launch physical gates until capture readiness is stable.

Required fields:

```text
shared_hardware_lock
lock_owner_pid
lock_owner_gate
lock_owner_run_dir
supervisor_status
supervisor_latest_cycle
supervisor_latest_capture_status
supervisor_latest_reason
supervisor_poll_interval_seconds
supervisor_recovery_interval_cycles
```

## Release Gates

### `READY_FOR_HUMAN_TEST`

Allowed only when:

- latest Rust preflight PASS hash matches installed Rust hash;
- internal runtime metrics pass;
- output A/B/C/D smoke passes;
- input A/B/C/D smoke passes;
- timecode smoke passes;
- capture readiness is stable;
- physical tone gate passes;
- physical music gate passes;
- spectral coloration passes;
- CPU/noise correlation passes;
- DVS scope validation passes where the claim includes DVS;
- rollback path is recorded.

### `READY_FOR_RELEASE`

Additionally requires:

- full A/B/C/D physical output matrix;
- full A/B/C/D physical input matrix;
- vinyl and CD-line timecode matrix;
- MIDI loopback;
- hotplug;
- sleep/wake;
- sample-rate changes;
- 8-hour playback;
- installer/uninstaller;
- signing/notarization plan;
- legal/provenance review.

## Metrics Schema Location

Future Rust JSON artifacts should use this top-level shape:

```json
{
  "schema": "open-a8dj-rust.pm-metrics.v1",
  "candidate": "0.x-rust-label",
  "status": "PASS|FAIL|NOT_READY|BLOCKED_*|SKIPPED_BUSY",
  "identity": {},
  "policy": {},
  "internal_runtime": {},
  "physical_tone": {},
  "physical_music": {},
  "spectral_coloration": {},
  "capture_readiness": {},
  "dvs": {},
  "supervisor": {},
  "lock": {},
  "mainline_comparison": {},
  "evidence": []
}
```

## Mainline-Agent Handshake

When a mainline agent finds new evidence relevant to Rust, it should report:

```text
rust_pm_metrics_contract=/Users/fer/dev/audio8djrust/docs/RUST_PM_SUCCESS_METRICS.md
mainline_source_path=<path>
mainline_artifact_path=<path>
candidate=<mainline version/hash>
finding=<short fact>
metric_delta=<numbers>
rust_implication=<what the Rust PM should update>
```

Mainline agents must not write into the Rust worktree unless explicitly asked by
the user. They should hand findings back to the Rust PM.

If a mainline agent needs a machine-readable handoff location without writing
into tracked Rust files, use:

```text
/Users/fer/dev/opena8dj/local-analysis/rust-pm-mainline-evidence/latest.json
```

with schema:

```text
open-a8dj-rust.mainline-evidence.v1
```

That path is an untracked analysis artifact location on the mainline side. The
authoritative Rust target remains this document.
