# Promotion Readiness Status

Date: 2026-06-16
Worktree: `/Users/fer/dev/audio8djcpp`
Branch: `driverkit/cpp-redesign`
Candidate code commit: `2f80d08`

## Current Verdict

`NOT_READY_FOR_PROMOTION`

Do not move C++ to `main`. Do not move the C mainline to `Legacy`.

The only command allowed to unlock branch promotion is:

```bash
scripts/evaluate-promotion-readiness.py --json-out local-analysis/promotion-readiness-current.json
```

Promotion remains forbidden unless that command returns `PASS` and writes
`branch_promotion_allowed=true`.

## Baseline Model

C++ must beat the relevant C baseline in each domain, not a single cherry-picked
mainline run:

| Domain | C baseline |
|---|---|
| CPU, digital stability, low-resource runtime | `0.3.135` |
| 8-in/8-out functionality, A/B/C/D topology, timecode policy | `0.3.25` |
| historical physical tone/music floor | `0.3.24` |

## Current Gate Results

| Gate | Result | Evidence |
|---|---|---|
| offline C++ gates | `PASS` | `local-analysis/cpp-offline/current-offline-gates.json` |
| offline throughput | `PASS` | `pack_mib_s=1546.09`, `decode_into_mib_s=565.894`, `route_frames_s=9.99440e+08`, `route_reversed_frames_s=4.81809e+08`, `route_advanced_frames_s=4.72260e+08`, `float_to_s24_frames_s=8.54504e+07` |
| realtime SPSC ring audit | `PASS` | pushed `2815`, popped `2815`, remaining `0`, allocations `0` |
| offline timecode signal analysis | `PASS` | `8` rows, `0` failures |
| offline protocol contract | `PASS` | VID/PID `0x17cc:0x1978`, endpoints `0x01/0x81/0x82/0x06`, 8-in/8-out, Mode 2 check cadence `16`, full frame `32` |
| offline simulated output matrix | `PASS` | `48` rows, `0` failures, SNR min `119.407 dB`, residual max `1.07069e-06`, leakage max `-240 dBFS` |
| offline Mode 2 cross-oracle byte parity | `PASS` | `72` rows, `0` failures, `max_byte_mismatches=0`, `max_length_delta=0`, `total_check_errors=0`, `total_panic_flags=0` |
| offline DVS packet input decode | `PASS` | `24` rows, `0` failures, playback decode-off `PASS` |
| offline DriverKit shell contract | `PASS` | device model valid, no System Extension activated |
| offline hardware lock policy | `PASS` | `4` audited scripts, `0` missing requirements; HAL candidate safety, direct Audio 8 DJ gate, and physical soundcheck require lock |
| simulated output oracle | `PASS` | `local-analysis/simulated-output/2026-06-16T165629-sim-A-big-start4-gain05/metrics.json` |
| physical tone beats historical C tone floor | `PASS` | `sideband_ratio=0.000657`, `click_outliers=0` |
| physical real-music quality | `FAIL` | `quality_alignment_score=0.938154`, `snr_db_min=8.93`, `lag_jumps_gt_2_frames=24` |
| runtime CPU beats C `0.3.135` | `FAIL` | `opena8dj_driver_p95=11.5`, `coreaudiod_p95=95.8` |
| physical Traktor/timecode vinyl | `FAIL` | no physical DVS lock evidence |

## Latest Offline Diagnosis

`scripts/analyze-soundcheck-window-trace.py` was run on the existing failed
music capture without touching hardware.

Evidence:
`local-analysis/soundcheck/2026-06-16T170237-irig-pairA-16s-maxlag-start4/window-trace.json`

Key result:

| Metric | Value |
|---|---:|
| windows | `38` |
| local lag range | `-27..3 frames` |
| lag jumps over 2 frames | `24` |
| raw mid residual median | `1.398197` |
| lag-corrected mid residual median | `1.368747` |
| median improvement from local lag correction | `2.1%` |

Interpretation:

- Timing/cadence is a real problem.
- Correcting local lag does not materially clean the music residual.
- The next physical test must separate startup/cadence from analog/capture
  residual rather than assuming one root cause.

## Blocking Facts

- Tone quality is promising, but tone alone does not prove real-music quality.
- The current real-music iRig run fails strict product quality thresholds,
  especially SNR and quiet mid-band noise.
- CPU evidence is not acceptable for a low-resource claim.
- No physical Traktor/timecode vinyl lock evidence exists.
- A no-diagnostic HAL variant caused an operational hang; that variant remains
  rejected until reproduced safely with a watchdog.
- Physical-script lock discipline now has offline evidence, but this does not
  replace the need to acquire the lock and record active HAL isolation before
  each physical run.
- Current runtime state is intentionally quiesced: the hardware lock is absent,
  mainline OpenA8DJ LaunchAgents are disabled, no OpenA8DJ process is detected,
  and the active HAL path is absent. This is safe for idle state, but it means
  physical tests require an explicit HAL restore/reinstall under lock before
  any capture or playback.
- Latest promotion evaluation evidence is
  `local-analysis/promotion-readiness-current.json`; it returns `FAIL` with
  `branch_promotion_allowed=false`.

## Required Before Branch Promotion

- `scripts/run-cpp-offline-gates` PASS with current evidence summary.
- `scripts/evaluate-promotion-readiness.py` PASS.
- Real-music physical capture PASS against strict thresholds and mainline
  comparison.
- Controlled CPU profile with C++ at or below C `0.3.135`.
- Physical Traktor/timecode vinyl validation with no deck leakage.
- Runtime isolation audit PASS before any physical window:
  `scripts/runtime-isolation-audit --expect-hal active`.
- User authorization for the final branch move after evidence is archived.
