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
- The promotion evaluator now selects the latest paired soundcheck metrics and
  CPU profile by default. The current selected pair is
  `local-analysis/soundcheck/20260617-cpp-mainline-parity-config-dense-ch12-irig-pairA-12s`,
  and the `latest_music_cpu_pair` evidence gate passes only because both
  artifacts come from that same physical run. The product gates still fail.

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

## 2026-06-17 Current Blocking Update

Current commit: `13ac259`.

- Offline gates remain PASS, but product promotion remains forbidden.
- Input decode is now control-plane gated and off by default for playback. This
  improved C++ driver p95 to `6.3%`, close to but still above same-window
  mainline `6.0%`.
- The latest comparable Pair A/iRig music capture still fails quality:
  C++ `quality_alignment_score=0.680121`, SNR `-0.83 dB`, `42` lag jumps;
  mainline `quality_alignment_score=0.680798`, SNR `-0.83 dB`, `39` lag jumps.
- `coreaudiod` p95 remains a blocker in the current gate:
  C++ `43.2%` versus same-window mainline `8.0%`, dominated by startup spikes
  but still counted as resource evidence.
- The latest Pair A channel matrix rejects the current C++ HAL as worse than
  mainline in the same route:
  C++ max wrong-source leakage `-35.36 dB`; mainline `-42.58 dB`; threshold
  `-45 dB`.
- Physical Traktor/timecode vinyl remains unvalidated.

Conclusion: C++ is not ready for hardware-readiness claims, branch promotion,
or replacement of C mainline. The next acceptable work must produce evidence
that improves physical leakage/quality and preserves or improves CPU versus
mainline.

## 2026-06-17 Stream-Usage And Mainline-Config Status

- Default C++ Pair A matrix with harness stream usage disabled:
  `max_wrong_source_leakage_db=-39.72`; still FAIL and still worse than
  mainline `-42.58`.
- Mainline-config C++ recovered physical output level but still failed Pair A
  matrix: `max_wrong_source_leakage_db=-40.57`.
- Mainline-config C++ real-music gate failed:
  `quality_alignment_score=0.678827`, SNR `-0.83 dB`, `42` lag jumps,
  mid residual ratio `2.536563`, high residual ratio `1.779982`.
- Runtime CPU gate remains FAIL in
  `local-analysis/promotion-readiness-current.json`:
  `opena8dj_driver_p95=6.2%` is under the driver cap used by the evaluator,
  but `coreaudiod_p95=4.1%` is above the selected baseline cap `1.7%`.
- Final isolation:
  `local-analysis/runtime-isolation/post-parity-soundcheck-unload-final.json`
  PASS, HAL inactive, lock absent.

Decision: branch promotion remains forbidden. Do not move C mainline to
Legacy, do not move C++ to `main`, and do not claim audiophile readiness.

## 2026-06-17 ISO8/ISO10 Product-Gate Status

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-iso10q8.json`.

- Branch promotion remains forbidden:
  `branch_promotion_allowed=false`.
- Offline gates remain PASS.
- Pair A physical matrix now passes for the short-cadence HAL candidates:
  ISO8/q8 around `-52 dB` max wrong-source leakage, ISO10/q8
  `-52.30 dB`.
- This is not readiness. The real-music gate still fails:
  - ISO8/q8: alignment `0.964724`, SNR `10.00 dB`,
    mid/high residual `1.432051/1.356290`, `29` lag jumps.
  - ISO10/q8: alignment `0.969379`, SNR `10.18 dB`,
    mid/high residual `1.514509/1.396638`, `35` lag jumps.
- Runtime CPU still fails mainline:
  ISO8/q8 driver p95 `23.1%`, ISO10/q8 driver p95 `19.6%`, versus mainline
  target around `<= 6.5%`.
- ISO8/q8 remains the default candidate because ISO10/q8 is cheaper but worse
  on music residual and lag jumps.
- Required before branch promotion:
  real-music PASS, CPU at or below mainline under comparable conditions, full
  A/B/C/D physical routing, Traktor/timecode physical validation, and final
  runtime isolation/rollback evidence.

## 2026-06-17 Input-Decode-Gated Probe Status

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-inputdecode-gated-usage.json`.

- Branch promotion remains forbidden:
  `branch_promotion_allowed=false`.
- Offline gates remain PASS.
- The input-decode/stream-usage correction is accepted only as harness and
  control-plane correctness:
  playback-only runs should not activate input decode or request input stream
  usage.
- The physical product gate still fails:
  `quality_alignment_score=0.959187`, SNR `10.14 dB`,
  mid/high residual ratios `1.467121/1.368783`, quiet mid noise
  `-35.11 dBFS`, and `30` lag jumps.
- CPU remains above mainline:
  driver p95 `24.2%`, `coreaudiod` p95 `21.9%`, versus the mainline driver
  target around `<= 6.5%`.
- Failure analysis points at timebase/alignment instability rather than input
  decode, fixed EQ, simple L/R mix, polarity, clipping, or simple nonlinearity.
- Physical Traktor/timecode vinyl remains unvalidated.
- Full A/B/C/D physical routing remains unvalidated.
- Final cleanup evidence:
  `local-analysis/runtime-isolation/after-inputdecode-gated-wait8-unload.json`
  PASS, HAL inactive, lock absent.

Decision: do not move C mainline to Legacy, do not move C++ to `main`, and do
not claim better sound quality, functionality, or performance than mainline.

## 2026-06-17 Cadence Diagnostic Status

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-cadence-diagnostic.json`.

- Branch promotion remains forbidden:
  `branch_promotion_allowed=false`.
- Diagnostic physical capture still fails real-music quality:
  quality `0.958757`, SNR `10.09 dB`, mid/high residual
  `1.447622/1.366173`, quiet mid noise `-35.03 dBFS`, `27` lag jumps.
- Runtime CPU still fails mainline:
  driver p95 `24.1%`, `coreaudiod` p95 `12.3%`.
- Diagnostic evidence rejects several candidate root causes:
  - payload corruption: payload guard mismatches `0`;
  - gross queue discontinuity: ledger continuous, no sequence gaps;
  - playback transfer errors: none;
  - output underruns/timeline resets/late writes: none;
  - fixed LTI/EQ correction: worsens SNR.
- Diagnostic evidence keeps cadence/timebase as the active target:
  capture completion outliers `7`, playback completion outliers `8`, with weak
  but nonzero lag-jump correlation.
- Final cleanup:
  `local-analysis/runtime-isolation/after-cadence-diagnostic-unload.json` PASS,
  HAL inactive, lock absent.

Decision remains unchanged: do not move C mainline to Legacy, do not move C++
to `main`, and do not claim audiophile readiness.

## 2026-06-17 Playback-Before-Capture-Requeue Probe Status

Latest promotion evaluation:
`local-analysis/promotion-readiness-after-playback-before-capture-requeue.json`.

- Branch promotion remains forbidden:
  `branch_promotion_allowed=false`.
- Product timing probe still fails real-music quality:
  quality `0.961360`, SNR floor `10.25 dB`, mid/high residual
  `1.425897/1.365001`, quiet mid noise `-35.03 dBFS`, `28` lag jumps.
- Runtime CPU still fails mainline:
  driver p95 `21.8%`, `coreaudiod` p95 `12.2%`.
- This probe is cleaner than the diagnostic profile from an overhead
  standpoint, but it still does not provide a quality or resource superiority
  claim.
- Transport counters do not show gross output underruns, timeline resets, late
  writes, or lightweight completion delta outliers. The remaining failure is
  still audible/metric timebase instability in the capture comparison.
- Final cleanup:
  `local-analysis/runtime-isolation/after-playback-before-capture-requeue-unload.json`
  PASS, HAL inactive, lock absent.

Decision remains unchanged: do not move C mainline to Legacy, do not move C++
to `main`, and do not claim audiophile readiness.
