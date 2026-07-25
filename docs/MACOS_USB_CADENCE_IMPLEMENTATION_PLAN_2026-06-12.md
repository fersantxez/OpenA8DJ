# macOS USB cadence implementation plan - 2026-06-12

This is the implementation design after consolidating the old-driver clues,
the failed source-built candidates, the physical iRig QA results, and the
macOS-first boundary.

## Objective

Produce an OpenA8DJ candidate that is safe enough for human listening by
reducing the audible background noise, CPU/window-correlated artifacts, and
1 kHz physical sidebands heard through the Audio 8 DJ analog output.

The decisive proof is macOS stability plus physical iRig quality:

- Core Audio remains stable.
- `audio-list` does not hang.
- idle `coreaudiod` and `OpenA8DJ.driver` CPU return to normal.
- physical 1 kHz iRig captures have no abnormal sidebands.
- physical music iRig captures improve residual/SNR/window-stress metrics.
- only then ask for human listening.

## Non-goals and hard rules

- Do not implement a Linux/ALSA translation. Linux `snd-usb-caiaq` is only a
  comparative source of hypotheses about hardware sensitivity to USB cadence.
- Do not force HAL `GetZeroTimeStamp` to USB frame, transaction timestamp, or
  unstable re-anchor timing.
- Do not expose device-cadence corrections through the public Core Audio
  timeline. Any cadence following must stay inside the USB transport/output
  buffer path.
- Do not repeat the `0.2.60` prequeue/saturation direction without new cadence
  evidence.
- Do not repeat the direct `0.2.76` USB-zero-timestamp experiment.
- Do not ask for human listening because internal counters are clean.
- Do not install a behavior-changing candidate if the current tree cannot pass
  safety reloads and post-unload recovery.

## Current baseline facts

- Best source-built result so far: `0.2.71` build `73`.
- `0.2.71` passed safety and had the best source-built tone result
  (`sideband_ratio=0.019363`, strongest 1060 Hz sideband `-34.17 dB`), but was
  rejected by listening because background noise and window-switch artifacts
  remained audible.
- Negative `0.2.71` window-stress capture:
  `local-analysis/soundcheck/candidate-0271-user-window-stress-20260612-190258`.
  Key metrics: `quality_alignment_score=0.849187`, `analog_snr_db=6.22`,
  `mid_band_1000_5000_residual_ratio=1.528360`,
  `mid_band_window_residual_ratio_p95=1.681541`,
  `quiet_mid_band_noise_dbfs=-32.40`, `click_outliers=31`,
  `lag_jumps_gt_2_frames=128`.
- Rejected after that:
  `0.2.72` transfer-pool safety failure, `0.2.73` sample-time follower tone
  failure, `0.2.74` ISO16 tone failure, `0.2.75` gain/coreaudiod failure,
  `0.2.76` USB zero timestamp/silent tone, `0.2.77` four streams tone failure,
  `0.2.78` source recovery safety failure.
- The current source has `OPENA8DJ_PLAYBACK_CAPTURE_PACED=1`, so the next work
  is not merely "turn capture-paced on". The question is whether the macOS
  IOUSBHost completion timing, packet layout, active OUT lifecycle, and output
  read rate are actually clean enough under CPU/window stress.

## Design

### 1. Keep HAL timing stable

`OpenA8DJ_GetZeroTimeStamp` remains a stable Core Audio timeline based on the
HAL clock mutex state and `mach_absolute_time()`. The disabled
`OPENA8DJ_ENABLE_USB_ZERO_TIMESTAMP` path must stay off for candidate builds.

Allowed HAL changes in the next pass:

- expose additional read-only diagnostics if needed;
- preserve advertised device shape and output-only default behavior;
- keep input streams hidden unless a later, explicit input-stress phase needs
  them.

Disallowed HAL changes:

- USB-clock `GetZeroTimeStamp`;
- changing the zero timestamp period as an audio-quality experiment;
- changing Core Audio stream topology to chase USB cadence;
- property-probe backoff or IOProc stream usage experiments unless a separate
  safety plan requires them.

### 2. Exploit existing stats before adding more

Before a new driver build, run one safe no-behavior-change observability pass if
a known-safe baseline is already installed and the audio stack is stable. The
current driver already exposes useful stream stats through the IPC/control
path:

- `captureCompletionDelta*`;
- `playbackCompletionDelta*`;
- `captureToPlaybackQueueDelta*`;
- `playbackTransfersInFlight`;
- `captureZeroCompleteTransactions`;
- `captureOtherByteCountTransactions`;
- `playbackShortTransfers`;
- `playbackQueueFailures`;
- `outputFramesRead` / `outputFramesWritten`;
- `outputActiveUnderruns`;
- `outputElasticDrops` / `outputElasticReplays`;
- `outputTimelineResets`;
- clock anchor validity and clock counters.

Use this first to prove the run harness can correlate stream stats, CPU, tone
sidebands, and music residuals. If this pass already shows a strong correlation,
the next code change can be narrower.

Skip this pass if the installed driver is unsafe, if Core Audio is unstable, or
if the baseline identity cannot be proven.

### 3. Instrument USB transport before changing behavior

Add a low-overhead macOS cadence observer inside `OpenA8DJUSB.m`. It must use
preallocated state, atomics or the existing stats mutex pattern, and aggregate
metrics. No file I/O, no per-transfer logging, no allocations, and no blocking
calls in the streaming callback path.

The first implementation should be build-time gated, for example:

```text
HAL_CADENCE_DIAGNOSTIC ?= 1
-DOPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC=$(HAL_CADENCE_DIAGNOSTIC)
```

Initial aggregate metrics:

- IN completion delta distribution: existing min/max/sum/samples plus count of
  outliers above the expected transfer period.
- IN transaction timestamp delta distribution from
  `IOUSBHostIsochronousTransaction.timeStamp`, if stable/non-zero.
- IN packet layout: min/max/sum request count, complete count, offset,
  zero-complete count, non-expected byte count, filtered count, and a rolling
  layout signature.
- IN completion to OUT queue delta: existing metric plus outlier count.
- OUT queue attempt count, queue failure count, and queue time distribution.
- OUT packet layout before queue: min/max/sum request count, offset continuity,
  total bytes, transaction count, and layout signature.
- OUT in-flight depth sampled at queue and completion: min/max/sum/samples,
  max streak at high depth, max streak at zero/under-target depth.
- OUT completion delta distribution: existing min/max/sum/samples plus outlier
  count.
- OUT completion packet result: complete vs request count, short transfers,
  failed transaction status, zero complete count.
- output read rate: frames written by HAL, frames read by USB, current
  `OutputTimelineAvailable`, elastic drops/replays, startup silence, active
  underruns, and output read pointer jumps.
- CPU/window correlation hook: not inside the driver; scripts should snapshot
  the cadence counters while `run-soundcheck` samples `coreaudiod`,
  `OpenA8DJ.driver`, `WindowServer`, player, recorder, and UI processes.

The observer should support two surfaces:

- the existing `stream-stats` IPC payload extended with aggregate counters;
- a bounded "last N transfer summaries" dump only if it can be implemented
  without callback allocations. If this is too risky, skip the ring on pass 1.

Do not expose these as new HAL properties unless there is a specific need.
Keeping diagnostics off the Core Audio property surface reduces the risk of
`coreaudiod` and `audio-list` probing more state during enumeration.

### 4. Normalize metrics into a single run artifact

Extend the QA scripts so every run captures:

- pre-playback stream stats;
- during-playback stream stats snapshots at a fixed interval;
- post-playback stream stats;
- tone sideband metrics;
- music residual/SNR metrics;
- CPU profile;
- audio-stack guard output;
- installed/loaded build identity.

The output should land under one run directory with a concise summary:

```text
local-analysis/cadence-candidate/<version>-<label>-<timestamp>/
  identity.txt
  safety/
  tone-1/
  tone-2/
  tone-3/
  music/
  cpu-profile.tsv
  stream-stats-before.txt
  stream-stats-during.tsv
  stream-stats-after.txt
  cadence-summary.txt
  verdict.txt
```

### 5. Decide behavior from evidence

Do not implement a behavior change until the instrumentation answers which of
these signatures is present:

| Signature | Meaning | Next experiment |
| --- | --- | --- |
| 1 kHz sidebands correlate with IN completion jitter or IN->OUT queue outliers | host realtime scheduling/cadence problem | realtime-path cleanup first |
| sidebands correlate with OUT in-flight depth collapse or high-depth bursts | output lifecycle/queue pacing problem | active OUT slot/depth controller |
| sidebands correlate with packet layout changes, zero-complete packets, or short OUT completions | packet layout or bogus transaction handling | macOS capture-layout filter |
| music lag jumps correlate with output read pointer jumps, drops, or replays | output timeline follower problem | conservative transport-only read-rate follower |
| CPU spikes correlate with callback work, diagnostic capture, Objective-C allocations, logging, or locks | realtime hygiene problem | no-behavior realtime cleanup |
| no driver cadence metric correlates but physical capture still fails | measurement/model gap | improve iRig analyzer or revisit HAL topology/clock anchor, not `GetZeroTimeStamp` |

## Implementation phases

### Phase 0 - stabilize and freeze baseline

Owner: integration lead.

Work:

1. Confirm OpenA8DJ is either safely unloaded or the known candidate is loaded.
2. Run `audio-stack-guard` with recovery disabled first.
3. Record current device list, default output, loaded HAL version/hash, and CPU.
4. Do not proceed if `audio-list` hangs, `coreaudiod` is hot, or iRig/Audio 8 DJ
   enumeration is unstable.

Exit gate:

- audio stack PASS;
- idle Core Audio CPU normal;
- iRig and Audio 8 DJ visible when physically connected;
- baseline identity written.

### Phase 0.5 - existing-stats correlation pass

Owner: QA agent plus integration lead.

Work:

1. Poll the current `stream-stats` IPC output during a physical iRig tone run.
2. Poll it again during physical music with CPU/window stress.
3. Correlate existing stats with sidebands, mid-band residual windows, CPU, and
   WindowServer/player activity.

Exit:

- if existing stats identify the primary signature, proceed directly to the
  matching Phase 3 experiment;
- otherwise proceed to `0.2.79` instrumentation-only.

### Phase 1 - `0.2.79` instrumentation-only candidate

Owner split:

- Agent USB: `src/hal/OpenA8DJUSB.m`, `src/hal/OpenA8DJUSB.h`.
- Agent tools: `src/tools/opena8dj-control.c` and any stats formatting.
- Agent QA: `scripts/run-soundcheck`, `scripts/analyze-tone-capture.py`, new
  wrapper if useful.
- Integration lead: Makefile flags, versioning, install/reload/rollback.

Allowed changes:

- add cadence diagnostic counters and optional bounded summary ring;
- expose them through the existing control/stats path;
- collect them in physical soundcheck artifacts.

Forbidden changes:

- no scheduling behavior change;
- no output queue-depth change;
- no stream topology change;
- no gain change;
- no `GetZeroTimeStamp` change;
- no transfer-pool retry unless it is purely read-only instrumentation.

Gates:

1. Build and HAL smoke pass.
2. Safety reload cycle pass.
3. Post-unload recovery pass.
4. Physical 1 kHz tone through iRig, 3 runs. Instrumentation may be accepted if
   it does not materially regress versus the current best source-built tone.
5. Physical music through iRig with CPU/window stress. The goal is evidence, not
   release.

Exit:

- if safety fails: reject, rollback, document.
- if safety passes and metrics are usable: proceed to analysis.

### Phase 2 - cadence analysis

Owner: integration lead plus QA agent.

Work:

1. Compare `0.2.79` physical tone sidebands against cadence counters.
2. Compare music residual windows and lag jumps against cadence snapshots.
3. Compare CPU/window spikes against in-driver timing outliers.
4. Produce one written verdict selecting exactly one next behavior experiment.

Exit gate:

- a single primary signature selected;
- one behavior experiment chosen;
- rollback path confirmed before build.

### Phase 3A - realtime cleanup, no behavior change

Use this if CPU/window stress correlates with callback work.

Candidate: `0.2.80`.

Allowed changes:

- remove or compile out expensive callback-path diagnostics for candidate
  builds;
- preallocate any remaining per-stream transfer/event state at stream start;
- avoid Objective-C object churn in callbacks;
- avoid logging except sampled/aggregate counters;
- tighten locks around stats without changing queue cadence.

Gate:

- byte/layout behavior must remain equivalent to `0.2.79`;
- CPU improves under VLC/window-stress;
- physical tone and music do not regress.

### Phase 3B - macOS capture-layout OUT experiment

Use this if evidence points to packet layout, zero-complete packets, OUT
lifecycle, or in-flight depth.

Candidate: `0.2.81`.

Design:

- keep Core Audio input streams hidden if needed;
- use USB IN completions internally as cadence observations;
- derive OUT packet layout from successful, real IOUSBHost IN transaction
  observations;
- track active OUT transfer identity/depth explicitly;
- submit OUT only when a valid observed layout exists;
- when no valid layout exists, use a deterministic fallback that is counted and
  visible, not silent;
- keep all transfer state preallocated and bounded.

Rollback:

- compile-time flag off returns to the `0.2.79` instrumentation behavior;
- safety failure immediately unloads OpenA8DJ and restarts Core Audio/media
  services through `audio-stack-guard --recover --unload-opena8dj`.

Gate:

- safety reload pass;
- tone sideband materially better than `0.2.71` without CPU regression;
- music residual/window-stress materially better than `0.2.71`;
- no `audio-list` hang, no hot `coreaudiod`, no zero-output run.

### Phase 3C - transport-only elastic read-rate follower

Use this only if evidence points to output read pointer drift, drops, replays,
or lag jumps while packet layout and OUT lifecycle are clean.

Candidate: `0.2.82`.

Design:

- keep HAL sample time stable;
- apply bounded, rare, single-frame-level corrections inside
  `OutputTimelineReadFrames` or adjacent USB transport state;
- never insert hard-zero during active audio unless counted as an underrun;
- expose every correction in stats.

Gate:

- lag jumps and click outliers decrease;
- sidebands do not worsen;
- music residual/window-stress improves;
- CPU does not rise.

## Candidate quality thresholds

Use relative thresholds until the physical route is better calibrated, because
Audio 8 DJ and iRig are independent clocks and the mixer path level can drift.

Hard rejection:

- `audio-list` hangs.
- `coreaudiod`, `mediaremoted`, or `OpenA8DJ.driver` stay hot at idle.
- physical output peak is zero during playback.
- tone fundamental is effectively missing.
- sideband ratio is worse than the source-built baseline by more than 25%.
- tone `sideband_ratio > 0.025` unless the run is explicitly diagnostic-only
  and not a candidate for human listening.
- strongest 940/1060-ish sideband worse than about `-32 dB` relative unless
  explicitly diagnostic-only.
- post-unload guard fails.
- stream stats socket/control path is unavailable after run.
- growing active underruns, elastic drops/replays, timeline resets, hidden
  fallbacks, clipping, or output silent sections.

Candidate-worthy:

- safety reloads pass repeatedly.
- `OpenA8DJ.driver` and `coreaudiod` return near idle after playback.
- 1 kHz tone sideband ratio improves materially versus `0.2.71`; ideal is
  `<= 0.0194`, and `<= 0.025` is the maximum acceptable listening-candidate
  range only if music metrics also improve.
- strongest 940/1060-ish sidebands drop materially in relative dB; target
  `<= -34 dB`, minimum listening-candidate range about `<= -32 dB`.
- music run improves the `0.2.71` window-stress capture:
  `mid_band_window_residual_ratio_p95`, quiet mid-band noise, lag jumps, click
  outliers, and CPU correlation.
- music normalized p95 should be better than the P2 +1 dB gate
  (`<= 1.72` approximate), with a preferred target around `<= 1.60`.
- music RMS p95 should not be worse than the `0.2.71` repeat value
  (`<= 0.0328`) unless the iRig level difference is measured and justified.
- CPU/window coupling should not worsen; preferred `mid_band_cpu_corr_max <
  0.30`.
- no active underruns, elastic replays/drops, timeline resets, or fallback
  counters are hidden.

Use absolute SNR, click outliers, and lag jumps as regression signals rather
than sole pass/fail gates because Audio 8 DJ and iRig run on independent USB
clocks.

## Human-listening handoff

Only after a candidate passes the physical gates:

1. Leave the exact tested build installed and loaded.
2. Confirm the loaded version/hash matches the tested artifact.
3. Confirm default output is `Open Audio 8 DJ` only if needed for the user's
   listening test; otherwise leave the system on speakers.
4. Send the requested email to `fernandosanchezmunoz@gmail.com` saying there is
   a driver ready for test.
5. Tell the user exactly which app/track/output/buffer to use and what symptoms
   to report: background noise, window-switch clicks, bass distortion, Traktor
   at 48 kHz / 512, and microphone interaction only after output-only passes.

## Subagent execution plan

Use parallel agents only with disjoint ownership:

- USB instrumentation agent:
  owns `src/hal/OpenA8DJUSB.m` and `src/hal/OpenA8DJUSB.h`; no QA scripts, no
  HAL timing changes.
- Control/stats agent:
  owns `src/tools/opena8dj-control.c` and stats output formatting; no driver
  behavior changes.
- QA agent:
  owns scripts under `scripts/` and docs for run protocol; no driver code.
- HAL contract reviewer:
  read-only unless explicitly assigned; checks `OpenA8DJHAL.c` for accidental
  Core Audio contract changes.
- Integration lead:
  owns Makefile flags, version/build bump, install/reload/rollback, final
  verdict, and deciding whether a candidate is allowed to reach human testing.

Merge rule:

- instrumentation may merge first;
- behavior experiments merge one at a time;
- if two agents touch the same file, integration lead resolves manually;
- no agent reverts unrelated changes.
