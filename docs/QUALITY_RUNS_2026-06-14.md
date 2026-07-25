# Quality runs - 2026-06-14

Physical route:

- Output: Open Audio 8 DJ -> external mixer -> mixer REC OUT.
- Capture: iRig Stream, channels 1,2, 48 kHz.
- Music fixture: `local-analysis/baseline-fixtures/nueva-mexico-baseline-268s-60s-48k-s16.wav`.
- Calibrated test level: `--target-peak-db -16`.

## Baseline recovery

Installed safe recovery:

- Version: `0.3.64`
- Installed hash:
  `f7c57a5041a5adb19bdc2691eb6e4feeab659a9642d9bf40e826e3d9e3fe7df1`
- Core Audio state after restore: `audio_stack_health=PASS`, `Open Audio 8 DJ`
  visible as 8 input / 8 output, `iRig Stream` visible as 2 input / 2 output.

Short calibrated baseline:

- Run: `local-analysis/recovery-usb-irig-smoke-minus16-20260613-235335/A`
- Result: `FAIL`, but useful baseline.
- Metrics: `quality_alignment_score=0.953355`,
  `mid_band_residual_ratio=1.402706`, p95 `1.496382`,
  quiet mid noise `-34.79 dBFS`, clicks `0`,
  driver CPU `26.38/31.50% avg/p95`, coreaudiod p95 `26.08%`.
- Decision: good click behavior, too much driver CPU.

Long calibrated baseline:

- Run: `local-analysis/physical-baseline-0.3.64-repeat20-minus16-20260614-001016/A`
- Manual physical gate was required because the 20 s wrapper left the physical
  gate files empty.
- Metrics: `quality_alignment_score=0.949590`,
  `mid_band_residual_ratio=1.431510`, p95 `1.472938`,
  quiet mid noise `-35.94 dBFS`, clicks `0`,
  driver CPU `22.12/26.15% avg/p95`, coreaudiod p95 `3.40%`.
- Decision: this remains the safest loaded baseline. It still fails release
  criteria because driver CPU is too high.

## Rejected experiments

### 0.3.71 - ISO5 OUT-first + fast quantizer

- Change: preserved ISO5 and normal OUT cadence, queued capture-paced OUT before
  requeueing IN, and used a cheaper float-to-24-bit conversion.
- Run: `local-analysis/physical-smoke-0.3.71-outfirst-fastpack-minus16-20260613-235628/A`
- Metrics: `quality_alignment_score=0.976561`,
  `mid_band_residual_ratio=1.403168`, p95 `1.529214`,
  quiet mid noise `-34.69 dBFS`, clicks `0`,
  driver CPU `24.93/31.91% avg/p95`, coreaudiod p95 `27.98%`.
- Rejected: CPU still failed and p95/max residual got worse than baseline.

### 0.3.72 - ISO5 OUT-first only

- Change: OUT-first without fast quantizer.
- Run: `local-analysis/physical-smoke-0.3.72-outfirst-only-minus16-20260613-235845/A`
- Metrics: `quality_alignment_score=0.972808`,
  `mid_band_residual_ratio=1.390322`, p95 `1.459021`,
  quiet mid noise `-34.82 dBFS`, clicks `17`,
  driver CPU `28.40/34.90% avg/p95`, coreaudiod p95 `21.58%`.
- Rejected: worse CPU and audible-risk click count.

### 0.3.73 - valid capture OUT layout filter

- Change: derive OUT request layout only from capture packets with expected
  byte count.
- Run: `local-analysis/physical-smoke-0.3.73-valid-capture-out-layout-minus16-20260614-000010/A`
- Metrics: `quality_alignment_score=0.970007`,
  `mid_band_residual_ratio=1.422364`, p95 `1.510572`,
  max residual `1.808862`, clicks `10`,
  driver CPU `22.56/30.18% avg/p95`, coreaudiod p95 `34.22%`.
- Rejected: lower average CPU did not justify clicks and residual spikes.

### 0.3.74 - ISO6

- Change: ISO grouping 6, normal OUT cadence, no QoS change.
- Run: `local-analysis/physical-smoke-0.3.74-iso6-minus16-20260614-000213/A`
- Metrics: `quality_alignment_score=0.978135`,
  `mid_band_residual_ratio=1.409973`, p95 `1.517746`,
  max residual `1.648283`, clicks `11`,
  driver CPU `17.68/23.31% avg/p95`, coreaudiod p95 `78.47%`.
- Rejected: driver CPU finally passed, but clicks and coreaudiod spikes made it
  unsafe.

### 0.3.75 - ISO6 + USB queue QoS user-interactive

- Change: ISO6 plus high-priority USB dispatch queue.
- Short run: `local-analysis/physical-smoke-0.3.75-iso6-usb-qos2-minus16-20260614-000428/A`
- Short metrics: `quality_alignment_score=0.975909`,
  `mid_band_residual_ratio=1.393043`, p95 `1.488663`,
  clicks `0`, driver CPU `16.74/23.65% avg/p95`.
- Long run: `local-analysis/physical-repeat-0.3.75-iso6-usb-qos2-warm-minus16-20260614-000544/A`
- Long metrics: `quality_alignment_score=0.913582`,
  `mid_band_residual_ratio=1.438215`, p95 `1.532476`,
  clicks `116`, driver CPU `19.63/23.32% avg/p95`.
- Rejected: promising short CPU result, but unstable on a longer music segment.

### 0.3.76 - ISO6 + QoS + background warm-open

- Change: same as 0.3.75 plus background warm-open.
- Safety run: `local-analysis/safety-0.3.76-iso6-qos2-warmopen-20260614-000831`
- Rejected before audio: `coreaudiod` rose above 100% and `audio-list` hung
  during the safety cycle. The guard recovered and the driver was rolled back.
- Decision: do not combine this warm-open implementation with the current
  transport path.

### 0.3.77 - ISO5 + USB queue QoS user-interactive

- Change: preserve ISO5 and normal OUT cadence, only raise USB queue QoS.
- Run: `local-analysis/physical-smoke-0.3.77-iso5-usb-qos2-repeat20-minus16-20260614-001243/A`
- Metrics: `quality_alignment_score=0.913167`,
  `mid_band_residual_ratio=1.418119`, p95 `1.471818`,
  clicks `4`, driver CPU `22.84/25.96% avg/p95`,
  coreaudiod p95 `6.08%`.
- Rejected: worse than 0.3.64 long baseline, which had zero clicks.

## Current conclusion

- ISO5 normal OUT cadence is still the safest physical-sound path.
- ISO6 can make driver CPU pass, but current ISO6 variants are not stable enough:
  they introduce clicks on longer music or create Core Audio heat.
- Raising USB dispatch queue QoS does not reduce IOUSBHost enqueue cost and can
  worsen click behavior.
- Background warm-open remains useful as a concept for client-start latency, but
  the current implementation is unsafe in these candidate combinations.
- The next implementation direction should preserve ISO5 cadence and focus on
  old-driver-style fixed output slot lifetime / IOUSBHost request lifecycle, with
  safety gates before physical audio.

## 2026-06-14 00:18-00:40 follow-up

### Harness correction - iRig physical gate

- Problem reproduced: `run-soundcheck` could spend a long time in the generic
  analyzer before the iRig physical gate, and an interrupted long run could
  leave empty physical gate files without a final summary.
- Change: for iRig captures, `run-soundcheck` now goes directly to
  `physical-music-quality-gate`; physical JSON/text are written through temp
  files and only promoted after valid JSON parsing.
- Change: stream-stats polling during playback is now opt-in via
  `--monitor-stream-stats`. The old default polled `opena8dj-control
  stream-stats` every CPU sample and measurably perturbed CPU/jitter.

### Updated 0.3.64 baseline without stream-stats polling

- Installed baseline restored and verified:
  version `0.3.64`, hash
  `f7c57a5041a5adb19bdc2691eb6e4feeab659a9642d9bf40e826e3d9e3fe7df1`.
- Run:
  `local-analysis/physical-baseline-0.3.64-no-stream-poll-long-minus16-20260614-003200/A`
- Metrics: `quality_alignment_score=0.929898`,
  `mid_band_residual_ratio=1.417685`, high-band `1.346457`,
  quiet mid noise `-36.60 dBFS`, clicks `1`,
  driver CPU `25.03/32.80% avg/p95`, coreaudiod p95 `10.46%`.
- This is now the fair local baseline for long iRig music runs that do not
  perturb playback with stream-stats IPC.

### 0.3.78 - strict pool, capture and playback, hot stats on

- Change: circular transfer-pool cursor and strict no-fallback behavior for both
  capture and playback. This was broader than the old-driver hypothesis.
- Short run:
  `local-analysis/physical-smoke-0.3.78-strict-pool-short-minus16-20260614-002700/A`
- Metrics: clicks `0`, `mid_band_residual_ratio=1.396783`, driver CPU
  `27.94/33.60% avg/p95`.
- Rejected: CPU worse than 0.3.64 and the change touched capture as well as OUT.

### 0.3.79 - strict pool, capture and playback, hot stats off

- Change: same broad strict pool experiment, but with hot stream stats disabled.
- Short no-poll run:
  `local-analysis/physical-smoke-0.3.79-no-stream-poll-short-minus16-20260614-003000/A`
- Short metrics: clicks `0`, `mid_band_residual_ratio=1.360495`, driver CPU
  `20.23/31.99% avg/p95`.
- Long no-poll run:
  `local-analysis/physical-smoke-0.3.79-no-stream-poll-long-minus16-20260614-003100/A`
- Long metrics: `quality_alignment_score=0.917168`,
  `mid_band_residual_ratio=1.424333`, clicks `8`, driver CPU
  `26.18/34.70% avg/p95`.
- Rejected: more clicks and worse CPU than fair 0.3.64 baseline.

### 0.3.80 - OUT-only strict fixed slots, amplitude stats still on

- Change: corrected the experiment to OUT-only fixed slots; capture uses the
  previous pool behavior. Hot stream stats disabled.
- Short no-poll run:
  `local-analysis/physical-smoke-0.3.80-out-slots-nohot-short-minus16-20260614-003500/A`
- Short metrics: clicks `0`, `mid_band_residual_ratio=1.365516`, driver CPU
  `20.18/32.39% avg/p95`.
- Long no-poll run:
  `local-analysis/physical-smoke-0.3.80-out-slots-nohot-long-minus16-20260614-003600/A`
- Long metrics: `quality_alignment_score=0.906613`,
  `mid_band_residual_ratio=1.409425`, clicks `0`, driver CPU
  `26.18/34.39% avg/p95`.
- Steady CPU after the first 2 seconds was `28.54% avg / 34.30% p95`, worse
  than the fair 0.3.64 baseline (`27.29% avg / 32.80% p95`).
- Rejected: audio click count improved, but sustained driver CPU regressed.

### 0.3.81 - OUT-only strict fixed slots, amplitude and hot stats off

- Change: same OUT-only fixed-slot experiment with `OUTPUT_AMPLITUDE_STATS=0`
  and `HOT_STREAM_STATS=0`, matching the 0.3.64 hot-stat flags.
- Rejected before audio: safety check failed immediately after load with
  `coreaudiod=131.1%` and later `coreaudiod=100.7%`; `audio-list` became slow
  and the stack remained hot.
- Rolled back to exact 0.3.64 hash. At the end of this pass, sudo recovery was
  blocked because the stored password was rejected, so further physical tests
  are blocked until `coreaudiod` is recovered by sudo or reboot.

## Revised conclusion after fixed-slot pass

- The stream-stats monitor was contaminating quality runs; future acceptance
  runs should avoid playback-time stream-stats polling unless the test is
  explicitly about stream counters.
- Fixed output slots are not yet a win in this implementation. OUT-only slots
  reduced clicks in one long run, but regressed sustained CPU; the stats-matched
  version destabilized `coreaudiod`.
- Do not install or request human listening for 0.3.78-0.3.81.
- Current safe installed target remains exact 0.3.64 after rollback, but the
  macOS audio stack must be cooled/restarted before any further candidate work.

## 2026-06-14 02:10-02:36 follow-up

### Installed safe base before experiments

- Restored and verified `0.3.89`, hash
  `3a96177cc99592b227b05791a4894d5eb1e88b6182e131d5b055daa9900906cc`.
- This remains the best source-built clean-sound base from the previous pass,
  but it is not a release/high-fi candidate because long music gates still fail
  CPU and intermittent click thresholds.

### 0.3.91 - disable outputFramesWritten hot stat

- Change: `HAL_OUTPUT_WRITE_STATS=0`, no USB cadence or Core Audio timeline
  change.
- Tone run:
  `local-analysis/listen-gate/0.3.91-no-output-write-stats-tone-20260614-021114`
- Metrics: `sideband_ratio=0.007659`, strongest sideband `1120 Hz` at
  `-44.88 dB`, clicks `285`.
- Rejected: worse click behavior than 0.3.89 tone baseline. Rolled back to
  `0.3.89`.

### 0.3.92 - enable Core Audio IOProc stream usage

- Change: `HAL_STREAM_USAGE=1`, intended to let output-only clients avoid
  unnecessary input work while preserving all streams.
- Safety: pass; Audio 8 DJ still advertised 8 in / 8 out.
- Tone run:
  `local-analysis/listen-gate/0.3.92-stream-usage-tone-20260614-021433`
- Metrics: `sideband_ratio=0.010157`, strongest sideband `940 Hz` at
  `-39.93 dB`, clicks `86`.
- Rejected before music. Rolled back to `0.3.89`.

### 0.3.93 - HAL built with -O3

- Change: build-system-only experiment via `HAL_OPTFLAGS=-O3`.
- Rejected before audio: safety failed with `coreaudiod=103.9%`, then
  `126.9%` after first rollback attempt.
- Recovery: `local-analysis/recovery-after-0.3.93-o3-20260614-021733`
  restored a cold audio stack and `0.3.89`.

### 0.3.94 - fast output prefetch clear

- Change: `HAL_FAST_OUTPUT_PREFETCH_CLEAR=1`.
- Tone run:
  `local-analysis/listen-gate/0.3.94-fast-prefetch-clear-tone-20260614-021900`
- Metrics: `sideband_ratio=0.010657`, strongest sideband `1060 Hz` at
  `-38.63 dB`, clicks `81`.
- Rejected before music. Rolled back to `0.3.89`.

### 0.3.95 - flush touched output cycle

- Change: added `OPENA8DJ_FLUSH_TOUCHED_OUTPUT`, built with
  `HAL_FLUSH_TOUCHED_OUTPUT=1`; default remains off.
- Rationale: test whether waiting for all four output streams delays stereo
  clients by one cycle.
- Tone run:
  `local-analysis/listen-gate/0.3.95-flush-touched-tone-20260614-022301`
- Metrics: `sideband_ratio=0.013849`, strongest sideband `1060 Hz` at
  `-37.45 dB`, clicks `121`.
- Rejected before music. The hardware path does not tolerate this flush change.
  Rolled back to `0.3.89`.

### 0.3.96 - capture queue depth 128

- Change: `HAL_CAPTURE_QUEUE=128`, preserving ISO5 transfer size and output
  cadence.
- Tone run:
  `local-analysis/listen-gate/0.3.96-capture128-tone-20260614-022455`
- Metrics: `sideband_ratio=0.013597`, strongest sideband `1060 Hz` at
  `-37.79 dB`, clicks `98`.
- Rejected before music. Rolled back to `0.3.89`.

### Diagnostic - playback profile without rebuild

- Change: applied `opena8dj-control profile playback` during a `0.3.89` music
  gate so input decode stayed off while USB IN still paced output.
- Run:
  `local-analysis/diagnostic-0.3.89-profile-playback-minus16-20260614-022032/A`
- Metrics: `quality_alignment_score=0.938989`, mid residual `1.430993`,
  high residual `1.352032`, quiet mid noise `-36.59 dBFS`, clicks `12`,
  driver CPU `24.98/32.28% avg/p95`.
- Conclusion: input decode is not the main CPU source; CPU remains dominated by
  USB enqueue/completion cadence.

### Bench calibration failure

- After the rejected candidates, repeated `0.3.89` tone no longer matched the
  earlier clean 0.3.89 tone:
  `local-analysis/listen-gate/0.3.89-base-after-rejects-tone-20260614-022614`
  measured `sideband_ratio=0.009400`, clicks `73`.
- After audio-stack recovery, `0.3.89` still measured worse:
  `local-analysis/listen-gate/0.3.89-base-after-recovery-tone-20260614-022730`
  measured `sideband_ratio=0.012195`, clicks `108`.
- Exact `0.3.64`, previously a clean comparison, also measured bad on the same
  current physical chain:
  `local-analysis/listen-gate/0.3.64-exact-current-chain-tone-20260614-022854`
  measured `sideband_ratio=0.012901`, clicks `133`.
- iRig idle capture was clean:
  `local-analysis/irig-idle-noise-20260614-023033`, RMS `-74.82 dBFS`,
  mid FFT noise `-118.95 dBFS`, high FFT noise `-122.91 dBFS`.
- Audio 8 DJ USB reset succeeded, but post-reset `0.3.89` tone still did not
  recover the earlier clean baseline:
  `local-analysis/listen-gate/0.3.89-after-audio8-usb-reset-tone-20260614-023214`
  measured `sideband_ratio=0.008254`, clicks `200`.
- iRig USB reset succeeded, but iRig stopped appearing as a Core Audio device.
  USB/ioreg still shows `iRig Stream` and AppleUSBAudio/usbaudiod ownership, but
  `audio-list` and `system_profiler SPAudioDataType` list only MacBook devices
  and Open Audio 8 DJ.

### Current state and decision

- Installed driver restored to `0.3.89`, hash
  `3a96177cc99592b227b05791a4894d5eb1e88b6182e131d5b055daa9900906cc`.
- Audio stack health is cold/pass, and Open Audio 8 DJ enumerates.
- Physical iRig gates are blocked until iRig reappears as a Core Audio device.
- Do not install another candidate or request human listening until the iRig
  capture device is restored and the `0.3.89`/`0.3.64` tone baseline returns to
  the previously clean range.

## 2026-06-14 02:40-02:48 iRig capture recovery incident

- Installed driver before recovery remained `0.3.89`, hash
  `3a96177cc99592b227b05791a4894d5eb1e88b6182e131d5b055daa9900906cc`.
- Core Audio listed `iRig Stream` intermittently, but every physical capture
  attempt failed with `AudioDeviceStart failed: 2003329396`.
- System log showed the decisive failure was Apple USB audio, not OpenA8DJ
  playback:
  - `Unable to select alternate setting`
  - endpoint 0 control timeout `0xe00002d6`
  - repeated descriptor/control request timeouts from `iRig Stream@00110000`.
- A forced isolation test moved `OpenA8DJ.driver` out of the active HAL folder
  and restarted Core Audio. With OpenA8DJ unloaded, `iRig Stream` still did not
  publish as a usable Core Audio capture device.
- A logical iRig reset while OpenA8DJ was unloaded failed with
  `reset failed: Unable to reset device`; iRig did not reappear during an
  80-second Core Audio polling window.
- OpenA8DJ was restored to active HAL before any reboot/recovery:
  - Active bundle executable: `Contents/MacOS/OpenA8DJHAL`
  - Active hash:
    `3a96177cc99592b227b05791a4894d5eb1e88b6182e131d5b055daa9900906cc`
  - `audio-stack-health=PASS`, driver/coreaudiod CPU idle.
- Decision: no further candidate builds should be installed until a reboot or
  physical iRig power-cycle restores reliable iRig capture. First post-reboot
  gate must be `audio-list` plus a 2 second iRig `audio-record`; only then run
  `candidate-listen-gate` for the restored `0.3.89` baseline.

## 2026-06-14 09:50-10:25 post-reboot internal gates

### Gate hardening - playback CPU and real-output sanity

- Added `scripts/playback-cpu-gate` to reject a build before human listening
  when:
  - `AudioDeviceStart` / first callback are slow,
  - driver or `coreaudiod` CPU p95 is high,
  - Core Audio callbacks happen but the USB output path does not read frames,
  - output timeline resets or active underruns appear.
- This specifically catches the false positive seen in the first OUT coalescing
  experiment, where CPU looked good because no valid playback transfers were
  completing.

### 0.3.102 - ISO5 normal cadence, capture pool cursor, reused completions

- Change: `HAL_STOP_ISOC_ON_STOP=1`, `HAL_STOP_GRACE_USEC=30000000`,
  `HAL_TRANSFER_POOL_CURSOR=1`, `HAL_REUSE_ISOC_COMPLETIONS=1`.
- Installed hash:
  `6beae64b304f06fc5fc2c507b2dfb51bfc58231df535670c24afe86c135a5d76`.
- Cold gate:
  `local-analysis/playback-cpu-gate-0.3.102-cold-20260614-101215`
  failed with `device_start_seconds=4.345642`, driver CPU
  `24.05/28.90% avg/p95`.
- Warm gate:
  `local-analysis/playback-cpu-gate-0.3.102-warm-20260614-101322`
  started quickly (`0.102858s`) but still failed driver CPU
  `25.30/28.50% avg/p95`.
- Rejected: object/pool churn is not the limiting cost; IOUSBHost cadence still
  dominates. Post-run health also showed `coreaudiod` hot until cooldown.

### 0.3.103 - OUT-only coalescing, first implementation

- Change: `HAL_PLAYBACK_COALESCE_TRANSFERS=2`, keeping capture ISO5.
- Installed hash:
  `4547bc939250879dca4a756572a068431d75a296ebd0056b673794eabc65b4e4`.
- Cold gate:
  `local-analysis/candidate-0.3.103-out-coalesce2-20260614-101537/playback-cold`
  reduced driver CPU to `13.31/16.30% avg/p95` but failed cold start
  (`4.353557s`).
- Warm gate:
  `local-analysis/candidate-0.3.103-out-coalesce2-20260614-101537/playback-warm`
  passed start/CPU (`0.125375s`, `14.10/16.10% avg/p95`) but stream stats
  revealed the fatal false positive: `playbackTransfersCompleted=0`,
  `outputFramesRead=0`, `outputTimelineResets=225`.
- Rejected: CPU dropped because the OUT path was not producing valid physical
  output. This is now blocked by `scripts/playback-cpu-gate`.

### 0.3.104 - OUT-only coalescing with variable capture request counts

- Change: fixed the coalescing experiment so it can accumulate variable valid
  capture request counts instead of requiring exactly five transactions every
  capture completion.
- Installed hash:
  `978c6ec0ea582be4ada6e9681f491407cc4fc8ca316cddd1ac35786dcb88d415`.
- Rejected before acceptance: immediately after load, health showed
  `coreaudiod=87.6%` and `mediaremoted=61.7%`.
- Aborted cold gate still showed real output (`outputFramesRead=664214`) but
  failed `coreaudiod` p95 `86.60%` and cold start `4.447380s`.
- Decision: variable coalescing fixed the “no output” bug, but this build is
  not stable enough for further audio work.

### 0.3.105 - rollback-style ISO5 normal cadence with 30s grace

- Change: experimental paths off again:
  `HAL_PLAYBACK_COALESCE_TRANSFERS=1`, `HAL_TRANSFER_POOL_CURSOR=0`,
  `HAL_REUSE_ISOC_COMPLETIONS=0`, with stop-isoc plus 30s close grace retained.
- The first post-install `audio-list` hung and `coreaudiod` reached about 100%;
  a cascade audio-stack recovery restored enumeration.
- Final local gate:
  `local-analysis/playback-cpu-gate-0.3.105-iso5-normal-final-20260614-102317`
  proved real output path is alive (`outputFramesWritten=2885632`,
  `outputFramesRead=2885994`, no timeline resets/active underruns) but failed:
  `device_start_seconds=4.328031`, driver CPU `24.16/29.10% avg/p95`.
- Decision: usable only as a loaded internal work baseline, not a candidate and
  not a release. It still fails latency and CPU gates, and iRig physical gates
  remain blocked because `iRig Stream` is absent from USB/Core Audio.

### 0.3.106 - OUT-only coalescing retry after clean load wait

- Change: same variable-count OUT coalescing as 0.3.104, but tested with a
  stricter protocol: install, wait for audio-stack health PASS, then enumerate
  with timeout before playback gates.
- Installed hash:
  `93b13f8151ab92c15c501a40b3dc3bd129f2c94fba07f600baf1bb1824dd2c73`.
- Load health passed after the first wait sample and `audio-list` returned
  normally.
- Cold gate:
  `local-analysis/candidate-0.3.106-out-coalesce2-retry-20260614-102945/playback-cold`
  showed real output with no resets (`outputFramesRead=2886082`) but failed:
  `device_start_seconds=4.347651`, driver CPU `18.30/24.80% avg/p95`.
- Warm gate:
  `local-analysis/candidate-0.3.106-out-coalesce2-retry-20260614-102945/playback-warm`
  started quickly (`0.089864s`) and preserved real output
  (`outputFramesRead=2886203`, no resets/active underruns), but failed driver
  CPU `24.04/25.00% avg/p95`.
- Stream stats showed OUT transfer reduction (`playbackTransfersCompleted=30599`
  vs about `96273` capture completions), but CPU did not improve reliably.
- Rejected: OUT coalescing is not a safe CPU win in the AudioServerPlugIn /
  IOUSBHost path, and it remains physically unproven while iRig is absent.

### 0.3.107 - parked ISO5 normal build

- Change: returned to ISO5 normal capture-paced OUT with experimental coalescing,
  pool cursoring, and completion reuse disabled:
  `HAL_PLAYBACK_COALESCE_TRANSFERS=1`, `HAL_TRANSFER_POOL_CURSOR=0`,
  `HAL_REUSE_ISOC_COMPLETIONS=0`, while keeping stop-isoc plus 30s close grace.
- Installed hash:
  `38e149d54db78e6c16bd236fcb4ae6bc4af35e608dff219a1688f315bca7d1df`.
- Load health passed on the first wait sample and `audio-list` returned normally
  with `Open Audio 8 DJ` as 8 in / 8 out at 48 kHz.
- Purpose: safe parked internal baseline so the rejected OUT-coalescing
  experiment is not left active.
- Not a candidate: by construction it shares the ISO5 CPU/start-latency failures
  measured on 0.3.105 and still lacks iRig physical proof.

## Current technical conclusion

- The most trustworthy sound path is still ISO5 normal capture-paced OUT, but
  it fails CPU and cold client-start gates.
- OUT coalescing can lower CPU only if it preserves valid output; the corrected
  variable-count version produced output but destabilized Core Audio.
- Reusing completion handlers and pool cursoring did not materially reduce CPU.
- No build from this pass is ready for human listening, email notification, or
  release.
- Next safe work must either:
  - reduce IOUSBHost transaction overhead without changing physical OUT cadence,
  - or redesign OUT coalescing with a Core Audio stability gate before any iRig
    run.
- Final approval still requires `iRig Stream` to reappear and pass tone/music
  physical gates with sideband, click burst, residual-window, CPU-correlation,
  and CPU p95 thresholds.

## 2026-06-14 10:39-11:03 iRig recovered, strict physical retests

### Gate hardening

- `scripts/playback-cpu-gate` now measures before/after stream-stat deltas
  instead of trusting accumulated counters. It can also run a controlled CPU
  stress phase during playback.
- `scripts/physical-music-quality-gate` is now intentionally stricter for
  candidate work: zero click outliers, lower CPU limits, stronger alignment,
  bounded lag drift, bounded window click count, and CPU/noise correlation
  `<= 0.08`.
- `scripts/candidate-listen-gate` now requires a music file and a physical
  baseline JSON, then runs tone, physical music, and physical music under stress.
  It no longer treats tone-only or legacy `quality-gate` output as sufficient
  for a human-test candidate.

### iRig recovery

- After physical unplug/replug, `iRig Stream` returned in both USB and Core
  Audio.
- Probe:
  `local-analysis/irig-recovery-probe-20260614-104942`.
- Result: `audio-record` succeeded for 2 seconds at 48 kHz on iRig channels
  1,2 with no clipping. This re-enabled physical gates.

### 0.3.108 - ISO5 legacy OUT slots

- Change: old-driver-inspired strict/circular OUT slot identity, ISO5, no OUT
  coalescing, queue playback before capture requeue.
- Installed hash:
  `ea29e46310e7e452ca23e73a832a45b6f17ef495ce186c8f9349f7bcc01e67fa`.
- Physical run:
  `local-analysis/physical-smoke-0.3.108-iso5-legacy-20260614-105434`.
- Metrics: `quality_alignment_score=0.968490`,
  `mid_band_residual_ratio=1.436545`, high-band `1.346829`,
  clicks `0`, lag jumps `57`, driver CPU `20.49/26.07% avg/p95`.
- Rejected: physically much better than ISO8, but still not high fidelity and
  CPU regressed. Legacy OUT slots do not solve the CPU problem.

### 0.3.109 - ISO8 legacy OUT slots

- Change: same legacy OUT slot experiment, but ISO8 to mimic the old kext's
  8 microframe IRPs more closely.
- Installed hash:
  `11f809bf063f65be20366edb8abf5a77e809c467df82f20ba13f748325642e3a`.
- Playback CPU gate:
  `local-analysis/playback-cpu-gate-0.3.109-iso8-legacy-stress-20260614-104354`.
- CPU improved materially: `20.60/23.20% avg/p95`, stress
  `15.88/21.00%`, with real output and no resets.
- Physical run:
  `local-analysis/physical-smoke-0.3.109-iso8-legacy-20260614-105304`.
- Metrics: `quality_alignment_score=0.241502`,
  `mid_band_residual_ratio=7.084648`, high-band `6.149359`, lag jumps `79`.
- Rejected: ISO8 reduces CPU but destroys physical waveform quality.

### 0.3.110 - ISO8 legacy OUT slots + flush touched output

- Change: ISO8 legacy slots plus `HAL_FLUSH_TOUCHED_OUTPUT=1`.
- Installed hash:
  `76127e3fdb4f2b4521f47b8c17ab08526ae23223ea2251194dd62542c9b28cbe`.
- Playback CPU gate:
  `local-analysis/playback-cpu-gate-0.3.110-iso8-legacy-flushtouched-cold-stress-20260614-104723`.
- CPU improved: `17.09/20.90% avg/p95`, stress `11.04/19.40%`, with
  real output and no resets. It still failed cold start (`4.564s`).
- Physical run:
  `local-analysis/physical-smoke-0.3.110-iso8-legacy-flushtouched-20260614-105111`.
- Metrics: `quality_alignment_score=0.243767`,
  `mid_band_residual_ratio=7.052842`, high-band `6.092765`, lag jumps `76`.
- Rejected: same physical waveform failure as 0.3.109. `flush touched` was not
  the root cause; ISO8 is the destructive change in this branch.

### 0.3.111 - ISO5 normal current physical baseline

- Change: returned to ISO5 normal capture-paced OUT, no legacy slots, no
  coalescing, hot stats on.
- Installed hash:
  `a0f00f6d5f2b8aaf99bef68459ccae5962f4fc2f365ed38c8dab184e5d90c3bc`.
- Physical run:
  `local-analysis/physical-baseline-0.3.111-iso5-normal-20260614-105615`.
- Metrics: `quality_alignment_score=0.970356`,
  `mid_band_residual_ratio=1.431906`, high-band `1.348028`,
  clicks `2`, lag jumps `49`, driver CPU `19.97/25.57% avg/p95`,
  coreaudiod p95 `16.28%`.
- Decision: safest currently loaded baseline after the ISO/no-hotstats
  experiments, but not a candidate. It fails click, CPU, residual-window, and
  CPU-correlation gates.

### 0.3.112 - ISO5 normal with hot stats off

- Change: `HAL_HOT_STREAM_STATS=0`, `HAL_OUTPUT_AMPLITUDE_STATS=0`, keeping
  output write stats on.
- Installed hash:
  `cae578291317a1a8524ab9fd006e2a03c12ed62ae7d44647a0ae00cff5016d07`.
- Rejected before audio:
  `local-analysis/guard-0.3.112-nohotstats-20260614-105810` failed with
  `coreaudiod=72.1%` and `mediaremoted=61.8%`.
- Rollback initially left `coreaudiod` hot; recovery unloaded OpenA8DJ and
  restored a cold stack:
  `local-analysis/recovery-after-nohotstats-incident-20260614-105917`.
- Decision: do not use this no-hotstats configuration as a candidate path.

### 0.3.113 - ISO10 normal current physical retest

- Change: ISO10 normal capture-paced OUT, no legacy slots, hot stats on.
- Installed hash:
  `d12c4421cd074c10fdaa5696565fc73a2dda87b11a4e4f65fbb544255d1530dd`.
- Physical run:
  `local-analysis/physical-smoke-0.3.113-iso10-current-20260614-110112`.
- Metrics: driver CPU improved to `12.33/15.56% avg/p95`, but physical quality
  regressed: `quality_alignment_score=0.961280`,
  `mid_band_residual_ratio=1.679983`, high-band `1.404874`, lag jumps `59`.
- Rejected: ISO10 is a CPU win but not a high-fidelity win on the current iRig
  route.

### Current loaded state after this pass

- Loaded version: `0.3.111`.
- Loaded hash:
  `a0f00f6d5f2b8aaf99bef68459ccae5962f4fc2f365ed38c8dab184e5d90c3bc`.
- `iRig Stream` and `Open Audio 8 DJ` both enumerate in Core Audio.
- No candidate is ready for human listening or email.

### 0.3.114 - ISO5 normal + native I24 byte order

- Change: `HAL_OUTPUT_NATIVE=1` with otherwise normal ISO5 settings.
- Installed hash:
  `17901c1dc5c6bedeed71db59f05d398b2463e37b67c356bd7d01a5d25934262c`.
- Physical run:
  `local-analysis/physical-smoke-0.3.114-native-i24-20260614-110406`.
- Metrics: catastrophic physical failure: `quality_alignment_score=0.005709`,
  `mid_band_residual_ratio=11119.809000`, high-band `10797.196825`,
  `capture_clipped_frames=685430`, capture RMS `-2.83 dBFS`.
- Rejected: native I24 byte order is not compatible with this physical output
  path. Rolled back immediately.

### Final loaded state after native I24 rejection

- Loaded version: `0.3.111`.
- Loaded hash:
  `a0f00f6d5f2b8aaf99bef68459ccae5962f4fc2f365ed38c8dab184e5d90c3bc`.
- Guard:
  `local-analysis/guard-final-safe-0.3.111-after-native-reject-20260614-110533`,
  `audio_stack_guard=PASS`, `coreaudiod=0.0%`, `opena8dj_driver=0.0%`.
- `iRig Stream` and `Open Audio 8 DJ` both enumerate in Core Audio.
- No candidate is ready for human listening or email.

### iRig replug recovery and dirty-route gate

- After an iRig unplug/replug, a first physical run on loaded `0.3.111` produced
  a catastrophic mismatch despite sane capture level:
  `local-analysis/physical-after-irig-replug-0.3.111-20260614-1109`.
- Metrics: `quality_alignment_score=0.246418`,
  `mid_band_residual_ratio=6.880214`, high-band `5.953911`,
  driver CPU `20.14/25.77% avg/p95`.
- After a soft Core Audio reset, the same loaded build returned to the known
  baseline range:
  `local-analysis/physical-after-irig-replug-reset-0.3.111-20260614-1112`.
- Metrics: `quality_alignment_score=0.965586`,
  `mid_band_residual_ratio=1.440186`, high-band `1.345375`,
  clicks `0`, lag jumps `55`, driver CPU `20.38/26.00% avg/p95`.
- Decision: the iRig route can record correctly after replug, but Core Audio /
  route state can become dirty. `scripts/physical-music-quality-gate` now marks
  sane-level catastrophic mismatches as
  `physical_route_integrity=DIRTY_OR_WRONG_ROUTE`, so those runs are rejected as
  invalid route state before comparing driver quality.

### 0.3.115 - ISO5 + reused completions + fast ISO config

- Change: `HAL_REUSE_ISOC_COMPLETIONS=1` and
  `HAL_FAST_ISO_TRANSFER_CONFIG=1` with otherwise normal ISO5 settings.
- Installed hash:
  `826ac344859caca4fdce6933101accf6ac8a3f26d9cdd2aee5da61e3b8015fe2`.
- CPU gate:
  `local-analysis/playback-cpu-gate-0.3.115-reuse-fastiso-stress-20260614-1115`.
- Metrics: start `4.416s`, driver CPU `107.68/121.50% avg/p95`,
  `outputFramesWritten=2885632`, `outputFramesRead=0`,
  `outputTimelineResets=225`.
- Rejected: catastrophic. The combined change leaves output unread and burns
  CPU. The likely culprit is fast ISO config reusing same-layout transfers before
  request counts/offsets are safely initialized.

### 0.3.116 - ISO5 + output amplitude stats off

- Change: `HAL_OUTPUT_AMPLITUDE_STATS=0` only, preserving ISO5, packing, hot
  stream stats, and queue behavior.
- CPU gate:
  `local-analysis/playback-cpu-gate-0.3.116-noamp-stress-20260614-1117`.
- Metrics: start `4.414s`, driver CPU `22.57/25.80% avg/p95`,
  stress driver CPU `6.39/17.40% avg/p95`, no timeline resets.
- First physical run was route-dirty:
  `local-analysis/physical-smoke-0.3.116-noamp-20260614-1118`,
  `quality_alignment_score=0.233296`,
  `physical_route_integrity=DIRTY_OR_WRONG_ROUTE` on recheck.
- After reset, the route returned to baseline-like alignment but quality did
  not improve:
  `local-analysis/physical-smoke-0.3.116-noamp-after-reset-20260614-1119`.
- Metrics after reset: `quality_alignment_score=0.967614`,
  `mid_band_residual_ratio=1.447441`, high-band `1.354581`,
  clicks `10`, driver CPU `19.43/25.69% avg/p95`,
  coreaudiod p95 `18.71%`.
- Rejected: no physical improvement and more clicks than baseline.

### 0.3.117 - ISO5 + reused completions only

- Change: `HAL_REUSE_ISOC_COMPLETIONS=1` without fast ISO config.
- Rejected before audio: loading/recovery repeatedly produced hot Core Audio /
  AirPlay state, including `coreaudiod=129.0%` and `AirPlayXPCHelper=53.0%`
  after a reset during load validation.
- Recovery / rollback run:
  `local-analysis/guard-recover-0.3.117-hot-20260614-1124` and
  `local-analysis/guard-recover-rollback-0.3.111-after-0.3.117-20260614-1124`.
- Decision: do not use the current completion-reuse implementation as a
  candidate path. It may still be useful only after a code-level redesign of
  handler lifetime, but the flag path is unsafe.

### Baseline CPU profile

- Profile run:
  `local-analysis/profile-baseline-0.3.111-sudo-20260614-1123`.
- `sample` showed the dominant hot path in the driver helper is USB isochronous
  requeue through IOUSBHost, not the float-to-24-bit packing loop:
  `handleCaptureTransfer -> queueCaptureTransfer -> IOUSBHostPipe enqueue...`
  and
  `handleCaptureTransfer -> queuePlaybackWithRequests -> IOUSBHostPipe enqueue...`.
- Small local costs were also visible in `fillPlaybackBytes`,
  `loadNextOutputFrameWithStats`, and transfer-pool Objective-C access, but
  IOUSBHost enqueue/mach messaging dominated the samples.
- Implication: simple packing, gain, or amplitude-stat tweaks are unlikely to
  deliver audiophile quality. Any useful next implementation must preserve ISO5
  physical cadence while reducing or stabilizing user-space IOUSBHost requeue
  pressure, or move work out of the realtime completion path without changing
  packet timing.

### 0.3.118 - ISO5 + USB queue QOS user-interactive

- Change: `HAL_USB_QUEUE_QOS=2`, otherwise normal ISO5.
- CPU gate:
  `local-analysis/playback-cpu-gate-0.3.118-qos2-stress-20260614-1126`.
- Metrics: start `4.377s`, driver CPU `22.61/26.10% avg/p95`,
  stress driver CPU `5.67/17.10% avg/p95`, no timeline resets.
- Physical run:
  `local-analysis/physical-smoke-0.3.118-qos2-20260614-1127`.
- Metrics: `quality_alignment_score=0.966028`,
  `mid_band_residual_ratio=1.450045`, high-band `1.350793`,
  clicks `20`, lag jumps `74`, driver CPU `24.52/27.42% avg/p95`.
- Rejected: QOS2 does not improve CPU and worsens physical clicks/residual.

### Current loaded state after QOS2 rejection

- Loaded version: `0.3.111`.
- Loaded hash:
  `a0f00f6d5f2b8aaf99bef68459ccae5962f4fc2f365ed38c8dab184e5d90c3bc`.
- Guard:
  `local-analysis/guard-rollback-0.3.111-after-qos2-20260614-1128`,
  `audio_stack_guard=PASS`, `coreaudiod=0.0%`, `opena8dj_driver=0.0%`.
- `iRig Stream` and `Open Audio 8 DJ` both enumerate in Core Audio.
- No candidate is ready for human listening or email.

### 0.3.119 - cached packet size and input decode atomics

- Change: cached `CalculateBytesPerPacket` results at stream start and loaded
  input transform atomics once per decode transfer.
- Intended risk profile: no change to ISO5 cadence, output packing, public Core
  Audio timeline, or channel layout.
- CPU gate:
  `local-analysis/playback-cpu-gate-0.3.119-cache-input-stress-20260614-1133`.
- Metrics: start `4.343s`, driver CPU `30.05/33.70% avg/p95`,
  stress driver CPU `9.55/27.40% avg/p95`, no timeline resets.
- Rejected: CPU regressed versus baseline. The code change was removed before
  rebuilding the safe baseline.

### Current loaded state after 0.3.119 rejection

- Loaded version: `0.3.111`.
- Loaded hash:
  `a0f00f6d5f2b8aaf99bef68459ccae5962f4fc2f365ed38c8dab184e5d90c3bc`.
- Guard:
  `local-analysis/guard-rollback-0.3.111-after-0.3.119-20260614-1135`,
  `audio_stack_guard=PASS`, `coreaudiod=0.0%`, `opena8dj_driver=0.0%`.
- `iRig Stream` and `Open Audio 8 DJ` both enumerate in Core Audio.
- No candidate is ready for human listening or email.

### 0.3.120 - ISO6 intermediate cadence

- Change: `HAL_ISO_FRAMES=6`, otherwise normal baseline settings.
- Rationale: ISO8/ISO10 reduced completion frequency but degraded physical
  waveform. ISO6 tested whether a smaller reduction could help CPU without
  destroying cadence.
- CPU gate:
  `local-analysis/playback-cpu-gate-0.3.120-iso6-stress-20260614-1137`.
- Metrics: start `4.333s`, driver CPU `25.56/28.50% avg/p95`,
  stress driver CPU `8.23/23.30% avg/p95`,
  playback completions delta `80169`, no timeline resets.
- Physical run:
  `local-analysis/physical-smoke-0.3.120-iso6-20260614-1138`.
- Metrics: `quality_alignment_score=0.241965`,
  `physical_route_integrity=DIRTY_OR_WRONG_ROUTE`,
  driver CPU `28.07/30.34% avg/p95`.
- Rejected: CPU did not improve versus baseline and the physical run was
  route-dirty. Since the CPU gate was already worse, ISO6 was not worth a
  reset/retry as a candidate.

### Current loaded state after ISO6 rejection

- Loaded version: `0.3.111`.
- Loaded hash:
  `a0f00f6d5f2b8aaf99bef68459ccae5962f4fc2f365ed38c8dab184e5d90c3bc`.
- Guard:
  `local-analysis/guard-rollback-0.3.111-after-iso6-20260614-1139`,
  `audio_stack_guard=PASS`, `coreaudiod=0.0%`, `opena8dj_driver=0.0%`.
- `iRig Stream` and `Open Audio 8 DJ` both enumerate in Core Audio.
- No candidate is ready for human listening or email.

### iRig replug / USB reset incident - physical capture blocked

- User confirmed iRig was unplugged/replugged and would remain connected.
- Initial short `audio-record` against `iRig Stream` worked, proving Core Audio capture was alive immediately after replug.
- Two physical music runs on loaded safe `0.3.111` then failed as `DIRTY_OR_WRONG_ROUTE` despite sane capture level:
  - `local-analysis/physical-after-irig-replug-fixed-0.3.111-20260614-114623`
  - `local-analysis/physical-after-irig-replug-reset2-0.3.111-20260614-114742`
- Capture level was around `-19.4 dBFS RMS` and peak around `-5 dBFS`, but quality alignment was only about `0.24-0.25`, mid residual about `6.8-7.0`, and high residual about `5.9-6.1`; the gate correctly rejected the route before comparing driver quality.
- Low-rate envelope analysis showed the captures still followed the music, while waveform correlation collapsed versus the known-valid physical capture. This indicates route/phase/deformation state, not a silent iRig.
- During diagnostics, invoking `build/usb-reset-device` with no args reset the iRig Stream, because the tool defaults to iRig `0x1963:0x0059`. After that, iRig remained visible in IOUSB/ioreg but no longer appeared as a Core Audio input device, even after restarting `coreaudiod` and `usbaudiod`.
- Temporarily removing OpenA8DJ from `/Library/Audio/Plug-Ins/HAL` did not make iRig reappear, so this is not caused by the OpenA8DJ HAL bundle alone.
- System was recovered to safe installed OpenA8DJ `0.3.111` hash `a0f00f6d5f2b8aaf99bef68459ccae5962f4fc2f365ed38c8dab184e5d90c3bc`; Core Audio/driver CPU returned to normal.
- Decision: physical iRig gates are blocked until iRig Stream reappears in Core Audio. Do not run `build/usb-reset-device` without explicit vendor/product args again. Do not request human listening or send readiness email.

### 0.3.122 - fixed fast ISO transfer config

- Change: fixed `OPENA8DJ_FAST_ISO_TRANSFER_CONFIG` so it no longer treats a newly created transfer as reusable before `requestCount` and `offset` have been initialized. Also added `build/usb-reset-device` to the Makefile and removed the dangerous default iRig reset target.
- Rationale: prior `0.3.115` with fast ISO config was catastrophic (`outputFramesRead=0`, timeline resets, >100% driver CPU). The code inspection showed `CreateIsoTransferWithCapacity` initialized lengths/counts such that the fast path could skip first-time transaction setup.
- Safety load: `local-analysis/hal-safety-0.3.122-fastiso-fixed-20260614-120155`, PASS; candidate loaded as `0.3.122`, hash `3b95f0dfdecb5c2273de5628ff38b14a6ecd6649cf213aa77421b8ed722e9862`.
- CPU/playback gate: `local-analysis/playback-cpu-gate-0.3.122-fastiso-fixed-stress-20260614-120224`.
- Metrics: start `4.333588s`, first callback `4.333626s`, driver CPU `22.19/27.30% avg/p95`, stress driver CPU `17.99/21.20% avg/p95`, `outputFramesWritten=2885632`, `outputFramesRead=2885950`, timeline resets `0`, active underruns `0`.
- Decision: rejected. The bugfix prevents the old catastrophic unread-output failure but does not improve CPU/start latency enough and is worse than the safe `0.3.111` baseline on CPU. Rolled back to `0.3.111` via `local-analysis/rollback-0.3.111-after-0.3.122-20260614-120341`.

### Exact rollback after 0.3.122 rejection

- The first rollback rebuilt `0.3.111` from the current source tree, which produced hash `a969d57c9ac111fee5910936b2002333d3dafe1da0891b06ecd5635e16c66ae5` because the source tree now contains the inactive fast-ISO bugfix.
- To preserve the known physical baseline exactly, restored archived bundle `/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.pre-safety-cycle-1-20260614-120201`, hash `a0f00f6d5f2b8aaf99bef68459ccae5962f4fc2f365ed38c8dab184e5d90c3bc`.
- Restore run: `local-analysis/restore-exact-0.3.111-a0f00-after-0.3.122-20260614-120446`.
- Loaded state after restore: OpenA8DJ enumerates as `org.opena8dj.Audio8DJ`, version `0.3.111`, hash `a0f00f6d5f2b8aaf99bef68459ccae5962f4fc2f365ed38c8dab184e5d90c3bc`; audio stack health PASS.
- `iRig Stream` was not present in Core Audio at this point, so physical capture gates remain blocked.

### 0.3.123 - skip AUDIO_PARAMS reset before stream

- Change: added guarded flag `OPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM`; tested with reset disabled while preserving ISO5/capture-paced settings.
- Rationale: isolate whether the persistent `AudioDeviceStart` delay around `4.3s` comes from the reset-style `AUDIO_PARAMS 0xff` command before the real stream params.
- Safety load: `local-analysis/hal-safety-0.3.123-skip-reset-audio-params-20260614-120746`, PASS; installed hash `ac17a4d3370bbc59f02e7b81db20d3486404c1fba55005d9695681206b6e95af`.
- Short start timing: `local-analysis/start-latency/0.3.123-skip-reset-audio-params-*`, `device-start=4.364830s`, `first-callback=4.364845s`.
- Decision: rejected before full CPU/physical gates. Skipping the reset does not improve client start latency. Restored exact baseline `0.3.111` / `a0f00f6d5f2b8aaf99bef68459ccae5962f4fc2f365ed38c8dab184e5d90c3bc` via `local-analysis/restore-exact-0.3.111-a0f00-after-0.3.123-*`.

### 0.3.124 - isolated background warm-open

- Change: enabled `OPENA8DJ_BACKGROUND_WARM_OPEN` on the ISO5 baseline.
- Safety load: `local-analysis/hal-safety-0.3.124-iso5-warmopen-isolated-20260614-120907`, PASS; installed hash `aa3d2dad7e10a0c2ad7861b0ca1660f2930838fc36653ec50d3b32ad9423b9c7`.
- Short timing was inconsistent: one start at `0.110277s`, a second at `2.242955s`.
- CPU/playback gate: `local-analysis/playback-cpu-gate-0.3.124-iso5-warmopen-isolated-stress-20260614-121047`.
- Metrics: start `0.101952s`, first callback `0.109516s`, but driver CPU `30.98/32.90% avg/p95`, stress `26.79/28.50%`, and stream stats reported `outputFramesWritten=0`, `outputFramesRead=0`, `playbackTransfersCompleted=0`.
- Decision: rejected. Warm-open is not reliable as implemented and can make the gate blind to active playback. Restored exact `0.3.111`.

### 0.3.125 / 0.3.126 - hot stream stats sampling

- Change: added `OPENA8DJ_HOT_STREAM_STATS_INTERVAL` / `HAL_HOT_STREAM_STATS_INTERVAL` to reduce mutex work in ISO completion telemetry without changing audio cadence or HAL public timing.
- `0.3.125`, interval `8`: first safety run failed because coreaudiod/mediaremoted were still hot immediately after load; retest with wait `12s` passed at `local-analysis/hal-safety-0.3.125-hotstats-interval8-wait12-20260614-121722`.
- `0.3.125` CPU gate: `local-analysis/playback-cpu-gate-0.3.125-hotstats-interval8-stress-20260614-121806`.
  Metrics: start `4.341252s`, driver CPU `21.68/25.70% avg/p95`, stress `18.30/20.00%`, frames written/read valid, no resets/underruns.
- `0.3.126`, interval `32`: safety PASS at `local-analysis/hal-safety-0.3.126-hotstats-interval32-wait12-20260614-121929`.
- `0.3.126` CPU gate: `local-analysis/playback-cpu-gate-0.3.126-hotstats-interval32-stress-20260614-122004`.
  Metrics: start `4.328270s`, driver CPU `21.56/25.30% avg/p95`, stress `18.45/22.30%`, frames written/read valid, no resets/underruns.
- Decision: rejected as a standalone direction. Sampling telemetry reduces some overhead versus worse variants but does not solve CPU or start latency on ISO5.

### 0.3.127 - hot stats interval 32 plus warm-open

- Change: combined `HAL_HOT_STREAM_STATS_INTERVAL=32` with `HAL_BACKGROUND_WARM_OPEN=1`.
- Safety: `local-analysis/hal-safety-0.3.127-hotstats32-warmopen-wait12-20260614-122122`, PASS.
- CPU/playback gate: `local-analysis/playback-cpu-gate-0.3.127-hotstats32-warmopen-stress-20260614-122157`.
- Metrics: start `4.355199s`, driver CPU `21.41/25.50% avg/p95`, stress `18.60/22.30%`, but stream stats again reported `outputFramesWritten=0`, `outputFramesRead=0`, `playbackTransfersCompleted=0` while the player received callbacks.
- Decision: rejected. Background warm-open in its current form does not fix the first start and breaks the reliability of telemetry/gates.

### 0.3.128 - ISO64 / queue 8 macOS translation of old low-CPU idea

- Change: translated the old `0.2.47`/`0.2.48` low-CPU cadence idea to the current macOS-first branch: `HAL_ISO_FRAMES=64`, `HAL_CAPTURE_QUEUE=8`, `HAL_PLAYBACK_QUEUE=8`, `HAL_OUTPUT_PREFETCH_FRAMES=64`, current topology/packing, no Linux model change.
- Safety: `local-analysis/hal-safety-0.3.128-iso64-q8-wait12-20260614-122415`, PASS.
- Cold CPU gate: `local-analysis/playback-cpu-gate-0.3.128-iso64-q8-stress-20260614-122450`.
  Metrics: start `4.342252s`, first callback `4.345344s`, driver CPU `4.43/5.20% avg/p95`, stress `3.83/4.60%`, `outputFramesWritten=2885632`, `outputFramesRead=2886060`, resets `0`, underruns `0`.
- Warm short starts after one real playback were `0.104363s` and `0.122228s`.
- Warm CPU gate: `local-analysis/playback-cpu-gate-0.3.128-iso64-q8-warm-stress-20260614-122615`, PASS.
  Metrics: start `0.118515s`, first callback `0.124344s`, driver CPU `4.74/5.20% avg/p95`, stress `3.37/3.80%`, `outputFramesWritten=2868736`, `outputFramesRead=2868778`, resets `0`, underruns `0`.
- Decision: internally promising for CPU and warm client startup, but not a release/listening candidate without iRig physical quality. Prior ISO64-family builds had bad physical quality, so iRig must decide.

### 0.3.129 - ISO64 / queue 8 with ISO kept alive after StopIO

- Change: same ISO64/q8 cadence, but `HAL_STOP_ISOC_ON_STOP=0` to keep ISO alive during the stop grace.
- Safety: `local-analysis/hal-safety-0.3.129-iso64-q8-keepiso-wait12-20260614-122819`, PASS.
- Short starts: cold `4.376958s`, warm `0.124182s`; driver about `5%` while grace was active.
- Warm CPU gate: `local-analysis/playback-cpu-gate-0.3.129-iso64-q8-keepiso-warm-stress-20260614-122909`.
- Metrics: start `0.128824s`, driver CPU `4.75/5.20% avg/p95`, but `outputTimelineResets=1` and `outputActiveUnderruns=10384`.
- Decision: rejected. Keeping ISO alive after StopIO creates exactly the underrun/reset pattern that can become audible clicks.

### 0.3.130 - ISO64 / queue 8 with long USB-open grace, ISO stopped

- Change: same ISO64/q8 cadence, `HAL_STOP_ISOC_ON_STOP=1`, but extended `HAL_STOP_GRACE_USEC=120000000` so USB stays open after StopIO while ISO is stopped.
- Safety: `local-analysis/hal-safety-0.3.130-iso64-q8-grace120s-wait12-20260614-123138`, PASS.
- Cold short start: `4.635370s`; after 45s idle, stack health was PASS with driver/coreaudiod `0.0%`, and the next short start was `0.221131s`.
- Warm CPU gate after idle: `local-analysis/playback-cpu-gate-0.3.130-iso64-q8-grace120s-warm-after45-stress-20260614-123333`, PASS.
- Metrics: start `0.233457s`, first callback `0.238004s`, driver CPU `4.70/5.30% avg/p95`, stress `3.34/3.80%`, coreaudiod p95 `2.30%`, `outputFramesWritten=2868224`, `outputFramesRead=2868008`, resets `0`, underruns `0`.
- Decision: best internal candidate so far for CPU and warm client startup. Not ready for human listening or email until iRig returns and physical tone/music gates prove ISO64/q8 does not reintroduce the historical metallic/noisy physical quality.

### Current loaded state after 0.3.130 testing

- Loaded state was restored to exact safe baseline after rejected experiments unless explicitly testing a loaded candidate. After the 0.3.130 pass, physical gates are still blocked because iRig Stream does not enumerate in USB/Core Audio.
- Known exact baseline remains `/Library/Audio/Plug-Ins/HAL.disabled/OpenA8DJ.driver.pre-safety-cycle-1-20260614-120201`, version `0.3.111`, hash `a0f00f6d5f2b8aaf99bef68459ccae5962f4fc2f365ed38c8dab184e5d90c3bc`.
- Do not send readiness email. Do not request listening until iRig is visible and `0.3.130` passes physical tone and real-music gates against the known baseline.

### 0.3.131 - ISO64 / queue 8 with pre-open on HAL initialize

- Change: added `OPENA8DJ_BACKGROUND_PREOPEN_ON_INIT` / `HAL_BACKGROUND_PREOPEN_ON_INIT`, which opens the USB device in a background thread during HAL initialization without starting the ISO stream. This keeps GetZeroTimeStamp and Core Audio timing unchanged while moving USB-open latency out of the first client `StartIO`.
- Build settings: ISO64/q8, `HAL_STOP_ISOC_ON_STOP=1`, `HAL_STOP_GRACE_USEC=120000000`, `HAL_BACKGROUND_PREOPEN_ON_INIT=1`.
- Safety: `local-analysis/hal-safety-0.3.131-iso64-q8-preopen-grace120s-wait20-20260614-124103`, PASS.
- Pre-play idle health after preopen: PASS, driver `0.0%`, coreaudiod `0.3%`.
- First short start after load: `device-start=0.091372s`, `first-callback=0.091389s`.
- CPU/playback gate: `local-analysis/playback-cpu-gate-0.3.131-iso64-q8-preopen-grace120s-stress-manual-20260614-124218`, PASS.
- Metrics: start `0.090494s`, first callback `0.098038s`, driver CPU `6.31/6.90% avg/p95`, stress `5.16/5.70%`, `outputFramesWritten=2867712`, `outputFramesRead=2868019`, resets `0`, underruns `0`.
- Long-idle check: after the 120s grace expired, idle CPU stayed PASS, but the next short start regressed to `4.345706s`. Preopen-on-initialize fixed first load only, not starts after the delayed close.
- Decision: useful but incomplete. Needs preopen after delayed USB close.

### 0.3.132 - ISO64 / queue 8 with pre-open after delayed close

- Change: when `HAL_BACKGROUND_PREOPEN_ON_INIT=1`, the delayed close path now closes USB after grace and immediately reopens it in the background without starting ISO. This keeps idle CPU low but keeps later `StartIO` calls warm.
- Build settings: ISO64/q8, `HAL_STOP_ISOC_ON_STOP=1`, short validation grace `HAL_STOP_GRACE_USEC=10000000`, `HAL_BACKGROUND_PREOPEN_ON_INIT=1`.
- Safety: `local-analysis/hal-safety-0.3.132-iso64-q8-preopen-afterclose-grace10s-wait20-20260614-124620`, PASS.
- Pre-play idle health: PASS, driver `0.0%`, coreaudiod `0.2%`.
- First short start after load: `device-start=0.089313s`, `first-callback=0.096843s`.
- After the 10s grace expired and the delayed close/preopen ran, idle health was PASS with driver/coreaudiod `0.0%`; the next short start remained fast at `device-start=0.093096s`, `first-callback=0.100626s`.
- CPU/playback gate: `local-analysis/playback-cpu-gate-0.3.132-iso64-q8-preopen-afterclose-grace10s-stress-20260614-124746`, PASS.
- Metrics: start `0.091025s`, first callback `0.097075s`, driver CPU `6.33/6.80% avg/p95`, stress `5.41/5.80%`, coreaudiod p95 `1.80%`, `outputFramesWritten=2885632`, `outputFramesRead=2885686`, resets `0`, underruns `0`.
- Final loaded hash: `3b0ce02dd43e5fe51623586d63505745ea583b6bf08b9f3904654cd950ff2e3d`; audio stack health PASS, driver/coreaudiod idle `0.0%`.
- Decision: best internal build so far for start latency and CPU. It is not a human-listening or release candidate until iRig physical gates pass, because earlier ISO64-family experiments had poor physical waveform quality.

### iRig status after 0.3.132

- `iRig Stream` is still absent from both Core Audio and the IOUSB tree.
- Current Core Audio devices: MacBook Air Microphone, MacBook Air Speakers, Open Audio 8 DJ.
- IOUSB shows Audio 8 DJ but no IK Multimedia/iRig device.
- Physical tone/music gates remain blocked. Do not send readiness email.

### 0.3.133 - ISO64 / queue 8 pre-open, USB kept open while idle

- Change: removed the delayed close/reopen race found in `0.3.132`. With
  `HAL_BACKGROUND_PREOPEN_ON_INIT=1`, StopIO now stops ISO and cleans transient
  state, but keeps the USB device open while idle instead of closing and
  reopening it after grace. Prior evidence showed an open-but-not-streaming USB
  engine has `0.0%` idle CPU, while the close/reopen path could make a client
  wait about `2.1s` if it arrived during the reopen window.
- Build settings: ISO64/q8, `HAL_STOP_ISOC_ON_STOP=1`, validation grace
  `HAL_STOP_GRACE_USEC=10000000`, `HAL_BACKGROUND_PREOPEN_ON_INIT=1`.
- Safety: `local-analysis/hal-safety-0.3.133-iso64-q8-preopen-keepopen-grace10s-wait20-20260614-125324`, PASS.
- Repeated start/stop gate:
  `local-analysis/startstop-0.3.133-preopen-keepopen-grace10s-20260614-125415`.
- Six starts separated by 12s, so each start happened after the 10s grace:
  `0.096818s`, `0.111628s`, `0.109091s`, `0.115184s`,
  `0.115423s`, `0.113184s`; first callbacks all within about `0.10-0.12s`.
- CPU/playback gate:
  `local-analysis/playback-cpu-gate-0.3.133-iso64-q8-preopen-keepopen-grace10s-stress-20260614-125550`, PASS.
- Metrics: start `0.094077s`, first callback `0.101628s`, driver CPU
  `6.40/6.90% avg/p95`, stress `5.29/6.00%`, coreaudiod p95 `1.70%`,
  `outputFramesWritten=2867712`, `outputFramesRead=2868019`, resets `0`,
  underruns `0`.
- Final loaded state: version `0.3.133`, hash
  `0d3bb34b0769cd947f608e129588183327775c62a58f4c52e47e38ede053041a`;
  `audio_stack_health=PASS`, driver/coreaudiod idle `0.0%`.
- `iRig Stream` remains absent from Core Audio and IOUSB. Audio 8 DJ is present.
- Decision: best internal build so far. It passes current internal start/CPU/output
  gates and fixes the post-grace race, but it is still not ready for human
  listening or email because physical iRig tone/music gates are missing.

### 0.3.133 repeated internal gate after playback-cpu-gate fix

- Change: fixed `scripts/playback-cpu-gate` stream-stat interpretation. The HAL
  resets stream counters at `StartIO`, so the post-run stream snapshot is now
  treated as the run measurement instead of a monotonic delta against a previous
  snapshot. This removes the false failures observed when repeated identical
  60s runs produced the same frame count.
- Syntax check: `python3 -m py_compile scripts/playback-cpu-gate`, PASS.
- Run: `local-analysis/long-internal-0.3.133-postgatefix-20260614-130536`.
- Result: three consecutive 60s real-music playback gates with CPU stress all
  passed.
- Gate 1: start `0.093362s`, first callback `0.100931s`,
  driver CPU avg/p95 `6.40/6.90%`, coreaudiod avg/p95 `1.58/1.80%`,
  `playbackTransfersCompleted=7516`, `outputFramesWritten=2886144`,
  `outputFramesRead=2886071`, resets `0`, active underruns `0`, failures `[]`.
- Gate 2: start `0.090256s`, first callback `0.096300s`,
  driver CPU avg/p95 `6.39/7.20%`, coreaudiod avg/p95 `1.58/1.90%`,
  `playbackTransfersCompleted=7516`, `outputFramesWritten=2886144`,
  `outputFramesRead=2886071`, resets `0`, active underruns `0`, failures `[]`.
- Gate 3: start `0.088055s`, first callback `0.095562s`,
  driver CPU avg/p95 `6.26/6.70%`, coreaudiod avg/p95 `1.57/1.70%`,
  `playbackTransfersCompleted=7515`, `outputFramesWritten=2885632`,
  `outputFramesRead=2885686`, resets `0`, active underruns `0`, failures `[]`.
- Final health: `audio_stack_health=PASS`, driver `0.0%`, coreaudiod `0.0%`,
  total watched CPU `0.1%`.
- Decision: internally stable and still the best current loaded build. This is
  not a human-listening/release candidate until physical iRig gates pass.

### iRig status after user unplug/replug

- User unplugged and replugged the iRig and said it will remain connected.
- Core Audio still reports only MacBook Air Microphone, MacBook Air Speakers,
  and Open Audio 8 DJ.
- IOUSB still reports Audio 8 DJ but no `iRig Stream`, no `IK Multimedia`, and
  no matching `1963:0059` device.
- Candidate physical gate:
  `local-analysis/listen-gate/0.3.133-after-user-replug-20260614-130937`.
- Result: `candidate_listen_gate=FAIL`,
  `reason=capture_device_missing_from_core_audio`, `capture_device=iRig Stream`.
- Follow-up watch:
  `local-analysis/irig-watch-after-user-replug-20260614-131034`.
- Result: `irig_watch=NOT_FOUND attempts=30`; each attempt checked Core Audio
  and the IOUSB tree.
- Interpretation: the current blocker is below the OpenA8DJ driver and below
  Core Audio capture. macOS is not enumerating the iRig at all, so the system
  cannot record from it. Do not treat this as an audio-quality result, and do
  not send readiness email.

### iRig retry after second user replug confirmation

- User again confirmed the iRig had been unplugged/replugged and would remain
  connected.
- Health before and after tests: `audio_stack_health=PASS`; driver and
  coreaudiod returned to `0.0%` idle.
- `irig-recovery-gate` run:
  `local-analysis/irig-recovery-gate-20260614-132420`.
- Result: `irig_recovery_gate=FAIL`, `reason=irig_missing_from_usb_tree`,
  `attempts=15`.
- Direct capture probe:
  `local-analysis/manual-capture-probes-20260614-132528`.
- Result: `build/audio-record` against `iRig Stream` failed with
  `input device not found: iRig Stream`.
- Alternative Audio 8 capture probe:
  `local-analysis/manual-capture-probes-20260614-132528-audio8`.
- Result: Core Audio could open `Open Audio 8 DJ` inputs, but channels 1/2
  recorded absolute silence (`rms=0`, `peak=0`), so this is not a substitute
  for the physical mixer REC OUT -> iRig capture route.
- Internal real-music CPU/UI stress gate:
  `local-analysis/playback-cpu-gate-20260614-132626`.
- Result: `playback_cpu_gate=PASS`, start `0.102451s`, first callback
  `0.109623s`, driver CPU avg/p95 `6.31/6.80%`, coreaudiod p95 `1.80%`,
  WindowServer p95 during UI stress `17.80%`, driver stress avg/p95
  `5.67/6.40%`, `outputFramesWritten=2885632`,
  `outputFramesRead=2885686`, timeline resets `0`, active underruns `0`.
- Current Core Audio devices remain only MacBook Air Microphone, MacBook Air
  Speakers, and Open Audio 8 DJ. Current IOUSB tree shows Audio 8 DJ,
  USB Type-C Digital AV Adapter, and `Shadow`, but no `iRig Stream` / no
  `IK Multimedia`.
- Decision: loaded `0.3.133` remains internally stable and low CPU, but is
  still physically unproven. Do not ask for human listening and do not send
  readiness email until iRig is visible in USB/Core Audio and the physical
  tone/music gates pass.

### Unified candidate preflight gate

- Change: added `scripts/candidate-preflight` and `make candidate-preflight`.
- Purpose: enforce the complete pre-human-test ladder in one command:
  audio-stack health, Core Audio enumeration, real-music playback CPU/UI stress,
  iRig USB/Core Audio recovery, short iRig capture, and full physical
  `candidate-listen-gate` when iRig is ready.
- Safety behavior: if iRig is absent, the script exits with
  `candidate_preflight=BLOCKED_PHYSICAL_CAPTURE` instead of producing a false
  quality pass or a misleading driver failure.
- Documentation updated: `docs/TESTING.md` now makes `make candidate-preflight`
  the final gate before human listening or readiness email.
- Checks: `bash -n scripts/candidate-preflight`, `make -n candidate-preflight`,
  and `git diff --check` for the touched files, PASS.
- Follow-up change: `scripts/irig-recovery-gate` now writes
  `reason=candidate_listen_gate_failed` if iRig recovers but the full physical
  candidate gate fails. This keeps iRig recovery failures separate from
  candidate audio-quality failures.
- Run:
  `local-analysis/candidate-preflight/0.3.133-current-20260614-133017`.
- Internal gate result: `playback_cpu_gate=PASS`, start `0.095279s`, first
  callback `0.102822s`, driver CPU avg/p95 `6.28/7.00%`, coreaudiod p95
  `1.80%`, WindowServer p95 during UI stress `16.80%`, driver stress avg/p95
  `5.49/6.10%`, `outputFramesWritten=2885632`,
  `outputFramesRead=2885686`, timeline resets `0`, active underruns `0`.
- Physical gate result: `candidate_preflight=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Post-run health: PASS; Open Audio 8 DJ still visible as 8 input / 8 output,
  driver and coreaudiod returned to `0.0%` idle.
- Decision: the currently loaded `0.3.133` continues to be the best internal
  build, but it is not promoted. The unified gate now prevents accidental
  listening/email promotion until iRig physical capture is actually available.

### Safe iRig watcher for deferred physical gates

- Change: added `scripts/candidate-watch` and `make candidate-watch`.
- Purpose: wait safely for iRig to appear in both IOUSB and Core Audio, then run
  the full `candidate-preflight` ladder automatically.
- Safety behavior: it only polls `audio-list` and `ioreg`; it does not reset USB
  devices, restart Core Audio, or change driver state.
- Follow-up change: `scripts/candidate-watch` and `scripts/candidate-preflight`
  now always write final `audio-stack-health`, `audio-list`, and USB ioreg
  snapshots on exit. This preserves the post-run state even when a gate aborts
  because iRig is absent.
- Documentation updated: `docs/TESTING.md` now documents
  `make candidate-watch CANDIDATE_WATCH_LABEL=<version>`.
- Checks: `bash -n scripts/candidate-watch scripts/candidate-preflight
  scripts/irig-recovery-gate`, `make -n candidate-watch`, and
  `git diff --check`, PASS.
- Smoke run:
  `local-analysis/candidate-watch/0.3.133-current-20260614-133442`.
- Result: `candidate_watch=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`, `attempts=2`.
- Final-snapshot validation run:
  `local-analysis/candidate-watch/0.3.133-current-20260614-133557`.
- Result: `candidate_watch=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`, `attempts=2`; final health snapshot
  remained PASS and final Core Audio enumeration still showed only MacBook Air
  Microphone, MacBook Air Speakers, and Open Audio 8 DJ.
- Decision: this gives a safe handoff path for the next iRig recovery without
  risking another software USB reset or accidental promotion.

### Read-only candidate status summary

- Change: added `scripts/candidate-status` and `make candidate-status`.
- Purpose: report the current installed candidate state in one read-only
  command: installed version/hash, audio-stack health, Open Audio 8 DJ Core
  Audio visibility, iRig USB/Core Audio visibility, and latest matching
  `candidate-preflight` / `candidate-watch` result.
- Safety behavior: it does not start audio, reset USB, restart Core Audio, or
  change driver state.
- Follow-up change: `candidate-status` now prefers gate results whose
  `candidate=` matches the installed version (for example `0.3.133-current`) or
  `current-loaded`, so auxiliary smoke tests do not overwrite the real
  candidate status.
- Checks: `bash -n scripts/candidate-status scripts/candidate-watch`,
  `make -n candidate-status`, and `git diff --check`, PASS.
- Run:
  `local-analysis/candidate-status/20260614-133911`.
- Result: `candidate_status=NOT_READY`,
  `reason=irig_missing_from_usb_tree`, installed version `0.3.133`, installed
  hash `0d3bb34b0769cd947f608e129588183327775c62a58f4c52e47e38ede053041a`,
  `audio_stack_health=PASS`, `found_audio8_core_audio=1`,
  `found_irig_usb=0`, `found_irig_core_audio=0`.
- Latest matching gates:
  - preflight:
    `local-analysis/candidate-preflight/0.3.133-current-20260614-133017/result.txt`,
    `BLOCKED_PHYSICAL_CAPTURE`, `irig_missing_from_usb_tree`.
  - watch:
    `local-analysis/candidate-watch/0.3.133-current-20260614-133901/result.txt`,
    `BLOCKED_PHYSICAL_CAPTURE`, `irig_missing_from_usb_tree`.
- Decision: this is now the quickest current-state check before deciding
  whether to run/continue physical gates or keep waiting for iRig.

### Timecode surface smoke gate on loaded 0.3.133

- Change: added `scripts/timecode-smoke-gate` and `make timecode-smoke-gate`.
- Purpose: cover the non-physical Traktor/timecode surface while iRig physical
  capture remains unavailable. It applies `timecode-vinyl`, verifies input
  decode is enabled, runs a short duplex Core Audio I/O test, verifies input
  and output frames advanced, then restores playback profile.
- Safety behavior: the gate does not claim physical Traktor scope/vinyl quality;
  it is only a Core Audio/control-surface smoke test.
- Run: `local-analysis/timecode-smoke-gate/20260614-134114`.
- Result: `timecode_smoke_gate=PASS`.
- Metrics: `callbacks=188`, `outputFrames=385024`,
  `outputSamples=770048`, `outputPeak=0.02000000`,
  `inputFrames=96256`, `inputRMS=0.00227966`,
  `inputPeak=0.01627851`.
- Timecode profile check: `input-mode: 0 (timecode-vinyl)`, `gnd-vinyl: on`,
  `software-lock: on`, `input-decode: on`.
- Restore check: playback profile restored with `input-decode: off`.
- Health: immediate post-gate health was PASS with transient coreaudiod/driver
  activity; after five seconds, `audio_stack_health=PASS` and coreaudiod/driver
  returned to `0.0%` idle.
- Decision: the loaded `0.3.133` preserves the automated timecode/Core Audio
  surface smoke. Full Traktor vinyl/scope and physical output quality remain
  blocked on iRig physical capture.

### Candidate gate preflight correction

- Change: `scripts/candidate-listen-gate` now checks iRig USB enumeration before
  checking Core Audio UID. This prevents the misleading
  `capture_device_missing_from_core_audio` diagnosis when the real failure is
  that macOS does not enumerate the iRig on USB at all.
- Syntax check: `bash -n scripts/candidate-listen-gate`, PASS.
- Current gate:
  `local-analysis/listen-gate/0.3.133-irig-usb-preflight-20260614-131239`.
- Result: `candidate_listen_gate=FAIL`, `reason=irig_missing_from_usb_tree`,
  `capture_device=iRig Stream`.
- Health immediately after the failed preflight remained PASS:
  driver `0.0%`, coreaudiod `0.0%`, total watched CPU `0.7%`.
- Decision: this is a harness improvement only; it does not validate physical
  audio quality. Continue to block human listening and readiness email until
  iRig is visible in USB/Core Audio and physical tone/music gates pass.

### Post-reboot iRig readiness hardening

- Change: `scripts/post-reboot-audio-test-startup` now records separate startup
  readiness flags for `found_audio8_core_audio`, `found_irig_core_audio`, and
  `found_irig_usb`.
- Change: it captures an IOUSB snapshot on each device-wait attempt and writes
  `device-readiness.txt`.
- Change: it skips the startup iRig baseline recording when the iRig is not
  ready in both USB and Core Audio, writing
  `irig-startup-baseline-summary.json` with `verdict=SKIPPED` instead of
  attempting a meaningless capture.
- Checks: `bash -n scripts/post-reboot-audio-test-startup`, PASS.
- Decision: this improves recovery evidence after the next reboot or physical
  USB recovery. It does not validate current physical audio quality while the
  iRig is absent.

### iRig recovery monitor

- Change: added `scripts/irig-recovery-gate`.
- Purpose: wait for a real iRig recovery before physical audio QA, then verify:
  Open Audio 8 DJ in Core Audio, iRig Stream in the USB tree, iRig Stream in
  Core Audio with IK Multimedia UID, a short iRig capture, and audio stack
  health. It can optionally run `candidate-listen-gate` after the short capture
  succeeds.
- First validation exposed a monitor bug: the time-based wait loop did not stop
  at the requested `--wait 5`, producing repeated attempts under
  `local-analysis/irig-recovery-gate-missing-20260614-131558`.
- Fix: replaced the time loop with a deterministic `max_attempts =
  ceil(wait_seconds / interval_seconds)` loop.
- Checks: `bash -n scripts/irig-recovery-gate`, PASS; `git diff --check` for
  the script, PASS.
- Current absent-iRig test:
  `local-analysis/irig-recovery-gate-missing-fixed-20260614-131657`.
- Result: `irig_recovery_gate=FAIL`, `reason=irig_missing_from_usb_tree`,
  `attempts=5`.
- Change: added Makefile target `irig-recovery-gate` with configurable
  `IRIG_RECOVERY_WAIT`, `IRIG_RECOVERY_INTERVAL`,
  `IRIG_RECOVERY_RECORD_SECONDS`, `IRIG_RECOVERY_CANDIDATE`,
  `IRIG_RECOVERY_MUSIC`, `IRIG_RECOVERY_BASELINE_JSON`, and
  `IRIG_RECOVERY_RUN_CANDIDATE_GATE`.
- Default candidate label is `current-loaded`, not `$(VERSION)`, because the
  Makefile release version can lag behind the installed experimental build.
- Makefile validation:
  `make -n irig-recovery-gate IRIG_RECOVERY_WAIT=3 IRIG_RECOVERY_INTERVAL=1 IRIG_RECOVERY_RUN_CANDIDATE_GATE=1`,
  PASS. It generated a command with `--candidate "current-loaded"` and
  `--run-candidate-gate`.
- Makefile absent-iRig test:
  `local-analysis/irig-recovery-gate-20260614-131800`.
- Result: `irig_recovery_gate=FAIL`, `reason=irig_missing_from_usb_tree`,
  `attempts=3`, as expected.
- Decision: this is the correct current outcome. It proves the monitor does not
  attempt fake capture or candidate QA while macOS cannot see the iRig on USB.

### Internal UI/WindowServer stress gate

- Change: extended `scripts/playback-cpu-gate` with optional `--ui-stress`.
  The UI stress phase uses silent `screencapture` calls during playback to put
  deterministic pressure on WindowServer without changing focus between apps.
- Change: the gate now reports `ui_stress_files` and `cpu_windowserver_p95`, and
  fails if UI stress was requested but no screen captures were produced.
- Change: added Makefile target `playback-cpu-gate`. By default it plays the
  real music fixture through Open Audio 8 DJ with both CPU stress and UI stress.
- Checks: `python3 -m py_compile scripts/playback-cpu-gate`, PASS;
  `git diff --check -- Makefile scripts/playback-cpu-gate`, PASS;
  `make -n playback-cpu-gate`, PASS.
- Run: `local-analysis/playback-cpu-ui-gate-0.3.133-20260614-132018`.
- Result: PASS.
- Metrics: start `0.100977s`, first callback `0.108176s`,
  driver CPU avg/p95 `6.10/7.00%`, coreaudiod p95 `1.80%`,
  stress driver CPU avg/p95 `4.86/5.90%`, WindowServer p95 `17.20%`,
  UI stress captures `13`, `outputFramesWritten=2885632`,
  `outputFramesRead=2885686`, timeline resets `0`, active underruns `0`,
  failures `[]`.
- Final health after the run: `audio_stack_health=PASS`, driver `0.0%`,
  coreaudiod `0.0%`, total watched CPU `0.0%`.
- Decision: this improves internal coverage for the user-observed
  CPU/window-correlated crackle symptom, but it is not physical audio proof.
  The build still needs iRig tone/music gates before human listening or email.

### Output pair smoke gate

- Change: added `scripts/output-pair-smoke-gate` and
  `make output-pair-smoke-gate`.
- Purpose: reject candidates where any exposed A/B/C/D stereo pair fails the
  basic Core Audio surface contract: quick start, first callback, output frames
  advancing, no timeline reset, no active underrun, no panic flag.
- Scope: internal only. This does not prove analog pair routing or mixer output
  quality; physical iRig capture remains mandatory before human listening.
- Integration: `candidate-preflight` now runs this before the long playback
  CPU/UI gate, and `candidate-status` reports the latest matching result.
- Standalone run:
  `local-analysis/output-pair-smoke-gate/0.3.133-current-20260614-134805`.
- Result: PASS for A/B/C/D.
- Pair metrics:
  - A: start `0.089075s`, first callback `0.095117s`,
    frames written/read `101376/101762`, resets/underruns/panic `0/0/0`.
  - B: start `0.100448s`, first callback `0.108006s`,
    frames written/read `101888/102147`, resets/underruns/panic `0/0/0`.
  - C: start `0.092261s`, first callback `0.098304s`,
    frames written/read `101888/101762`, resets/underruns/panic `0/0/0`.
  - D: start `0.091964s`, first callback `0.099508s`,
    frames written/read `101888/101762`, resets/underruns/panic `0/0/0`.
- Integrated candidate-preflight run:
  `local-analysis/candidate-preflight/0.3.133-current-20260614-134850`.
- Integrated result: `candidate_preflight=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Internal playback in that preflight: PASS. Start `0.092737s`, first callback
  `0.100288s`, driver CPU avg/p95 `6.03/6.60%`, coreaudiod p95 `1.80%`,
  stress driver CPU avg/p95 `5.41/6.20%`, stress coreaudiod p95 `2.10%`,
  WindowServer p95 `17.40%`, frames written/read `2885632/2885686`,
  timeline resets `0`, active underruns `0`.
- Final health after the integrated preflight: PASS, driver `0.0%`,
  coreaudiod `0.2%`, total watched CPU `0.5%`.
- Current candidate status remains NOT READY solely because iRig is absent from
  the USB tree and therefore absent from Core Audio.

### Physical capture diagnosis hardening

- Change: added `scripts/capture-device-diagnose` and
  `make capture-device-diagnose`.
- Purpose: make the current physical capture blocker explicit, separating these
  cases:
  - preferred iRig present in USB and Core Audio,
  - preferred iRig absent from USB,
  - preferred iRig present on USB but absent from Core Audio,
  - another external Core Audio input is present but not automatically accepted.
- Integration: `candidate-status` now runs the diagnosis read-only and reports
  `physical_capture_status`, `physical_capture_reason`, and any external Core
  Audio input candidates.
- Integration: `candidate-preflight` now records the same diagnosis before the
  iRig recovery/capture step, so every blocked candidate attempt has a capture
  evidence bundle.
- Safety behavior: alternate capture inputs are listed, not promoted. The
  candidate physical gate still requires the configured capture device, so this
  cannot create a false physical-quality pass.
- Current run: `local-analysis/capture-device-diagnose/20260614-135333`.
- Result: `physical_capture_status=MISSING`,
  `reason=irig_missing_from_usb_tree`.
- Core Audio inputs: 2 total, 0 external candidates after excluding the MacBook
  microphone and Open Audio 8 DJ.
- USB devices visible: `USB2.0 Hub`, `Audio 8 DJ`,
  `USB Type-C Digital AV Adapter`, `Shadow`.

### Physical measurement status split

- Change: `scripts/physical-music-quality-gate` now writes
  `measurement_status` and `candidate_quality_status` alongside the existing
  `verdict`.
- Purpose: preserve compatibility with existing `PASS`/`FAIL` consumers while
  making failed physical runs unambiguous:
  - `measurement_status=VALID` means the run can judge candidate quality.
  - `BLOCKED_DIRTY_ROUTE` means capture level was sane, but the analog/reference
    match was catastrophically wrong, so the bench route is dirty.
  - `BLOCKED_CAPTURE_LEVEL` means the capture is too quiet, too hot, or clipped.
  - `BLOCKED_CAPTURE_ROUTE_UNVERIFIED` means the run cannot prove the iRig path.
  - `BLOCKED_MISSING_CPU_PROFILE` means CPU/noise coupling was not evaluated.
- Change: `run-soundcheck` summary now prints both statuses.
- Change: `candidate-listen-gate` reports blocked physical measurements as
  measurement failures instead of plain candidate verdict failures.
- Validation run dir:
  `local-analysis/measurement-status-validation-20260614`.
- Rechecked dirty-route run:
  `local-analysis/physical-after-irig-replug-reset2-0.3.111-20260614-114742`.
  Result: `measurement_status=BLOCKED_DIRTY_ROUTE`,
  `candidate_quality_status=NOT_EVALUATED`,
  `quality_alignment_score=0.241892`, capture peak `-5.22 dBFS`.
  Decision: this is now explicitly bench/capture contamination, not candidate
  quality evidence.
- Rechecked valid-signal but bad-candidate run:
  `local-analysis/physical-smoke-0.3.74-iso6-minus16-20260614-000213/A`.
  Result: `measurement_status=VALID`, `candidate_quality_status=FAIL`,
  `quality_alignment_score=0.978135`, clicks `11`, driver CPU p95 `23.31%`,
  coreaudiod p95 `78.47%`.
  Decision: this remains valid evidence against the candidate.

### Physical bench sanity gate

- Change: added `scripts/physical-bench-sanity-gate` and
  `make physical-bench-sanity-gate`.
- Purpose: require the bench/capture path to be sane before judging candidate
  quality. The gate checks stack health after a short cooldown/enumeration
  guard, verified capture-device readiness, short capture success, no clipping,
  no implausibly hot idle capture, and final stack health.
- Integration: `candidate-preflight` now runs the bench sanity gate before the
  iRig recovery/full physical candidate gate. `candidate-status` reports the
  latest bench sanity result.
- Current absent-iRig validation:
  `local-analysis/physical-bench-sanity-gate/20260614-140132`.
- Result: `physical_bench_sanity_gate=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Decision: correct current behavior. The script does not fake capture and does
  not use the MacBook microphone or Audio 8 inputs as a substitute.
- Integrated candidate-preflight validation:
  `local-analysis/candidate-preflight/0.3.133-current-20260614-140221`.
- Integrated result: `candidate_preflight=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Output pair smoke in that preflight: A/B/C/D PASS, no timeline resets, no
  active underruns, no panic flags.
- Internal playback CPU/UI gate in that preflight: PASS. Start `0.090385s`,
  first callback `0.097936s`, driver CPU avg/p95 `6.23/7.00%`, coreaudiod p95
  `1.90%`, stress driver CPU avg/p95 `5.51/6.30%`, WindowServer p95 `19.60%`,
  frames written/read `2886144/2886071`, timeline resets `0`, active underruns
  `0`.
- Final health after the integrated preflight: PASS, driver `0.0%`,
  coreaudiod `0.0%`, total watched CPU `0.0%`.

### Bench sanity cooldown fix

- Observation: after adding timecode to `candidate-preflight`, the first
  integrated run `local-analysis/candidate-preflight/0.3.133-current-20260614-140504`
  passed timecode and playback CPU gates, but `physical-bench-sanity-gate`
  failed on its initial instantaneous health sample while `coreaudiod` was
  temporarily at `26.8%`.
- Change: `physical-bench-sanity-gate` now uses `audio-stack-guard --wait 8
  --enumeration-timeout 20 --min-idle-pct 20` before and after the short
  capture instead of a one-shot `audio-stack-health` sample.
- Purpose: reject real hot/unstable Core Audio states while allowing normal
  cooldown after the internal preflight phases.
- Validation: standalone bench gate
  `local-analysis/physical-bench-sanity-gate/20260614-140705` correctly blocked
  as `BLOCKED_PHYSICAL_CAPTURE`, `irig_missing_from_usb_tree`.

### Candidate preflight timecode integration

- Change: `scripts/candidate-preflight` now runs `scripts/timecode-smoke-gate`
  inside each candidate preflight instead of relying only on the latest
  historical timecode result shown by `candidate-status`.
- Order: A/B/C/D output-pair smoke -> timecode surface smoke -> internal
  real-music playback CPU/UI gate -> physical bench sanity -> iRig/full
  physical gate.
- Reason: timecode changes profile/input decode state. Running it before the
  playback gate verifies the Traktor/timecode surface and then restores the
  playback profile before measuring real-music output.
- Passing integrated run after cooldown fix:
  `local-analysis/candidate-preflight/0.3.133-current-20260614-140722`.
- Integrated result: `candidate_preflight=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Timecode result: PASS. Duplex I/O callbacks `188`, output frames `385024`,
  input frames `96256`, input RMS `0.00228230`, input peak `0.01637721`.
  Playback profile restore verified `input-decode: off`.
- Output pair smoke: A/B/C/D PASS, no timeline resets, no active underruns, no
  panic flags.
- Playback CPU/UI gate: PASS. Start `0.089959s`, first callback `0.097506s`,
  driver CPU avg/p95 `6.25/6.80%`, coreaudiod p95 `1.80%`, stress driver CPU
  avg/p95 `5.62/6.40%`, WindowServer p95 `17.00%`, frames written/read
  `2886144/2886071`, timeline resets `0`, active underruns `0`.
- Bench sanity result: `BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Final health: PASS, driver `0.0%`, coreaudiod `0.0%`, total watched CPU
  `0.1%`.

### iRig absent after user replug and OpenA8DJ isolation diagnosis

- User report: iRig was unplugged/replugged and left connected.
- Direct capture diagnosis:
  `local-analysis/capture-device-diagnose/20260614-141113`.
- Result: `physical_capture_status=MISSING`,
  `reason=irig_missing_from_usb_tree`.
- USB devices visible: `USB2.0 Hub`, `Audio 8 DJ`,
  `USB Type-C Digital AV Adapter`, `Shadow`.
- Core Audio devices visible: MacBook Air microphone, MacBook Air speakers,
  `Open Audio 8 DJ` 8 in / 8 out at 48 kHz. No `iRig Stream`.
- iRig recovery gate:
  `local-analysis/irig-recovery-gate-after-user-replug-20260614-1414`.
- Result: `irig_recovery_gate=FAIL`, attempts `15`,
  `reason=irig_missing_from_usb_tree`.
- Forced Core Audio service restart followed by capture diagnosis:
  `local-analysis/capture-device-diagnose/20260614-141349`.
- Result: still `physical_capture_status=MISSING`,
  `reason=irig_missing_from_usb_tree`.
- Manual OpenA8DJ isolation run:
  `local-analysis/irig-isolation-after-user-replug-20260614-1415`.
- Result while OpenA8DJ was removed from active HAL:
  `physical_capture_status=MISSING`, `reason=irig_missing_from_usb_tree`.
- Result after OpenA8DJ was restored:
  `physical_capture_status=MISSING`, `reason=irig_missing_from_usb_tree`,
  `found_audio8_core_audio=1`.
- Reproducible tool added: `scripts/irig-isolation-diagnose` and
  `make irig-isolation-diagnose`.
- Tool validation run:
  `local-analysis/irig-isolation-diagnose/after-user-replug-20260614-1420`.
- Result: `irig_isolation_diagnose=FAIL`,
  `reason=irig_missing_even_without_opena8dj`, restored Audio 8 confirmed with
  `found_audio8_restored=1`.
- Final health after restoration: PASS, driver `0.0%`, coreaudiod `0.0%`,
  total watched CPU `0.0%`.
- Decision: current physical capture blocker is USB enumeration of iRig Stream
  outside the OpenA8DJ driver. Do not treat this as a candidate audio-quality
  failure, and do not request human listening or send readiness email until iRig
  appears in USB and Core Audio again.

### Unified physical-capture readiness diagnosis

- Change: `scripts/candidate-watch` and `scripts/irig-recovery-gate` now call
  `scripts/capture-device-diagnose` for each readiness poll instead of
  duplicating USB/Core Audio grep logic.
- Purpose: keep the same source of truth for iRig physical readiness across
  status, watch, recovery, bench sanity, and candidate preflight.
- Candidate-watch smoke:
  `local-analysis/candidate-watch/diagnose-unified-smoke-20260614-continue`.
- Result: `candidate_watch=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`, `physical_capture_status=MISSING`,
  `found_audio8_core_audio=1`, `found_irig_usb=0`,
  `found_irig_core_audio=0`.
- iRig-recovery smoke:
  `local-analysis/irig-recovery-gate-diagnose-unified-smoke-20260614-continue`.
- Result: `irig_recovery_gate=FAIL`,
  `reason=irig_missing_from_usb_tree`, `physical_capture_status=MISSING`,
  `found_audio8_core_audio=1`, `found_irig_usb=0`,
  `found_irig_core_audio=0`.
- Change: `scripts/playback-cpu-gate` now writes an explicit `reason` line.
  Passing internal gates now report `reason=playback_cpu_gate_passed`, so
  `candidate-status` no longer shows a missing reason for a successful CPU gate.

### Current 0.3.133 preflight after unified diagnosis

- Run:
  `local-analysis/candidate-preflight/0.3.133-diagnose-unified-reason-20260614-continue`.
- Result: `candidate_preflight=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Output pair smoke: PASS for A/B/C/D.
- Timecode surface smoke: PASS.
- Internal playback CPU/UI gate: PASS,
  `reason=playback_cpu_gate_passed`.
- Internal metrics: start `0.093584s`, first callback `0.101131s`,
  driver CPU avg/p95 `6.30/6.80%`, coreaudiod p95 `1.70%`,
  stress driver CPU avg/p95 `5.66/6.30%`, stress coreaudiod p95 `1.60%`,
  WindowServer p95 `17.80%`, output frames written/read
  `2885632/2885686`, timeline resets `0`, active underruns `0`.
- Physical bench sanity: `BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Candidate status after the run:
  `local-analysis/candidate-status/20260614-142320/status.txt`.
- Candidate status: `candidate_status=NOT_READY`,
  `reason=irig_missing_from_usb_tree`; latest preflight subgates correctly show
  output pair PASS, timecode PASS, playback CPU PASS, and capture/bench blocked.
- Cooldown health after the run: PASS, driver `0.0%`, coreaudiod `0.0%`,
  total watched CPU `0.3%`.

### Candidate preflight stack-guard hardening and email gate

- Change: `scripts/candidate-preflight` now uses `audio-stack-guard` before
  internal playback and after full physical completion instead of one-shot
  `audio-stack-health` checks. The guard includes cooldown, Core Audio
  enumeration timeout, global idle threshold, and stale-driver detection.
- Validation run:
  `local-analysis/candidate-preflight/0.3.133-stackguard-20260614-continue`.
- Result: `candidate_preflight=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Preflight guard before playback: PASS, global idle `86.21%`, watched CPU
  `0.3%`, Core Audio enumeration PASS, OpenA8DJ loaded.
- Output pair smoke: PASS.
- Timecode surface smoke: PASS.
- Internal playback CPU/UI gate: PASS,
  `reason=playback_cpu_gate_passed`.
- Internal metrics: start `0.091345s`, first callback `0.098899s`,
  driver CPU avg/p95 `6.32/6.90%`, coreaudiod p95 `1.80%`,
  stress driver CPU avg/p95 `5.59/6.40%`, stress coreaudiod p95 `1.60%`,
  WindowServer p95 `16.80%`, output frames written/read
  `2885632/2885686`, timeline resets `0`, active underruns `0`.
- Physical bench sanity: `BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Change: added `scripts/candidate-ready-email-gate` and
  `make candidate-ready-email-gate`. It does not send mail. It runs
  `candidate-status` and prepares an email payload only if the installed
  candidate is `READY_FOR_HUMAN_TEST`.
- Current email-gate validation:
  `local-analysis/candidate-ready-email-gate/blocked-irig-missing-20260614-continue`.
- Result: `candidate_ready_email_gate=BLOCKED`,
  `reason=irig_missing_from_usb_tree`, `candidate_status=NOT_READY`.
- Decision: no readiness email may be sent in the current state.

### Watch-to-email wrapper and nested preflight status detection

- Risk found: a successful `candidate-watch` stores its preflight result under
  the watch run directory. `candidate-status` previously preferred standalone
  `local-analysis/candidate-preflight/*/result.txt` results, so a fresh
  watch-embedded PASS could be missed by the email gate.
- Change: `scripts/candidate-status` now considers both standalone preflight
  results and `local-analysis/candidate-watch/*/candidate-preflight/result.txt`,
  ordered by result mtime.
- Synthetic validation:
  `local-analysis/candidate-status/synthetic-nested-preflight-detection-20260614`.
- Result: `candidate-status` selected the synthetic nested preflight PASS and
  reported its subgate statuses, but still returned `candidate_status=NOT_READY`
  because the real current iRig USB/Core Audio state remained missing. This is
  the intended no-false-READY behavior.
- Change: added `scripts/candidate-watch-ready-email-gate` and
  `make candidate-watch-ready-email-gate`. It waits for iRig, runs the full
  watch/preflight ladder, then runs `candidate-ready-email-gate`; it never sends
  mail itself.
- Current blocked validation:
  `local-analysis/candidate-watch-ready-email-gate/blocked-irig-missing-20260614-continue`.
- Result: `candidate_watch_ready_email_gate=BLOCKED`,
  `reason=irig_missing_from_usb_tree`,
  `candidate_watch_status=BLOCKED_PHYSICAL_CAPTURE`.
- Decision: the one-command watch-to-email path is wired, but no readiness email
  can be prepared or sent until iRig is visible and the full physical preflight
  passes.

### iRig USB identity hardening

- Historical evidence: earlier recovery notes showed `build/usb-reset-device`
  defaulting to iRig Stream USB identity `0x1963:0x0059`, and Core Audio logs
  consistently showed UID
  `AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1`.
- Risk: USB/ioreg string matching alone could falsely report iRig missing if
  macOS exposed the device as generic USB audio but with the correct
  vendor/product IDs.
- Change: `scripts/capture-device-diagnose` now parses USB device nodes and
  accepts iRig Stream USB readiness by either:
  - `IK Multimedia` / `iRig Stream` USB strings, or
  - USB vendor/product `0x1963:0x0059`.
- Change: `scripts/candidate-status` now consumes `found_audio8_core_audio`,
  `found_irig_usb`, and `found_irig_core_audio` from
  `capture-device-diagnose`, removing a parallel grep-based readiness path.
- Change: `scripts/candidate-listen-gate` now uses
  `capture-device-diagnose` for iRig preflight instead of direct string greps.
- Validation run:
  `local-analysis/capture-device-diagnose-id-validation-20260614`.
- Synthetic READY case: USB node named `Generic USB Audio` with IDs
  `6499:89` (`0x1963:0x0059`) plus Core Audio iRig UID returned
  `physical_capture_status=READY`, `found_irig_usb_by_name=0`,
  `found_irig_usb_by_id=1`.
- Synthetic USB-only case: same USB IDs without Core Audio iRig UID returned
  `physical_capture_status=MISSING`, `reason=irig_missing_from_core_audio`.
- Current live case: still `physical_capture_status=MISSING`,
  `reason=irig_missing_from_usb_tree`, `found_irig_usb_by_id=0`.
- Candidate listen-gate validation:
  `local-analysis/listen-gate/0.3.133-capture-diagnose-id-preflight-20260614`.
- Result: `candidate_listen_gate=FAIL`,
  `reason=irig_missing_from_usb_tree`, using
  `capture-device-diagnose-preflight/result.txt` as evidence.
- Decision: the physical gate is stricter and less brittle. It will not pass
  without both a real iRig USB identity and Core Audio iRig input, but it will
  no longer falsely miss a correctly identified iRig just because USB strings
  are generic.

### Post-reboot startup detection migrated to common capture diagnosis

- Risk found: `scripts/post-reboot-audio-test-startup` still used direct grep
  checks for `IK Multimedia` / `iRig Stream` during its device wait loop.
- Change: startup device polling now runs `scripts/capture-device-diagnose`
  against each `audio-list` and `usb-ioreg` snapshot, so post-reboot detection
  uses the same USB string-or-ID logic as candidate gates.
- Change: skipped startup baseline capture now records
  `physical_capture_status` and the exact diagnostic reason.
- Change: `NEXT_TESTS.txt` now points first to `capture-device-diagnose` and
  the guarded `candidate-watch-ready-email-gate`.
- Cleanup: `scripts/candidate-status` no longer performs its own preliminary
  grep-based iRig detection before reading `capture-device-diagnose`.
- Validation run:
  `local-analysis/startup/post-reboot-20260614-143609`.
- Result: `audio_devices_ready=0`,
  `physical_capture_status=MISSING`,
  `physical_capture_reason=irig_missing_from_usb_tree`,
  `found_audio8_core_audio=1`, `found_irig_usb=0`,
  `found_irig_core_audio=0`.
- Startup baseline capture: skipped with
  `reason=irig_missing_from_usb_tree`; no audio playback was started.
- Startup guard before apps: PASS, global idle `93.92%`, watched CPU `0.0%`,
  Core Audio enumeration PASS.
- Startup guard after app phase: PASS, global idle `81.95%`, watched CPU
  `0.3%`, Core Audio enumeration PASS.
- Final post-migration guard:
  `local-analysis/audio-stack-guard/post-startup-migration-final-20260614`.
- Result: PASS. A one-shot `audio-stack-health` sample briefly reported
  `perf_power=62.2%`, but driver and coreaudiod were both `0.0%`; immediate
  process inspection and the 10 s guard showed `perf_power=0.1%`,
  driver/coreaudiod `0.0%`, global idle `82.47%`, and Core Audio enumeration
  PASS.

### iRig USB parser hex-ID hardening

- Risk found: `capture-device-diagnose` accepted decimal `idVendor` /
  `idProduct` values, but an ioreg-like snapshot could expose IDs as hex
  strings such as `0x1963` and `0x0059`.
- Change: USB property parsing now accepts decimal, `0x` hex, and bare hex
  property values.
- Validation run:
  `local-analysis/capture-device-diagnose-hex-id-validation-20260614`.
- Synthetic READY case: generic USB audio strings with
  `idVendor=0x1963`, `idProduct=0x0059`, plus Core Audio iRig UID returned
  `physical_capture_status=READY`, `found_irig_usb_by_name=0`,
  `found_irig_usb_by_id=1`.
- Synthetic USB-only case: same hex USB IDs without Core Audio iRig UID returned
  `physical_capture_status=MISSING`, `reason=irig_missing_from_core_audio`.
- Current live check:
  `local-analysis/capture-device-diagnose/hex-parser-live-20260614`.
- Result: still `physical_capture_status=MISSING`,
  `reason=irig_missing_from_usb_tree`, `found_irig_usb_by_id=0`.
- Email gate validation after parser change:
  `local-analysis/candidate-ready-email-gate/blocked-irig-missing-after-hex-parser-20260614`.
- Result: `candidate_ready_email_gate=BLOCKED`,
  `reason=irig_missing_from_usb_tree`.

### Candidate identity and no-false-ready hardening

- Risk found by gate audit: `candidate-status` could theoretically promote a
  stale `candidate_preflight=PASS` for the same version string even if the
  installed binary hash had changed.
- Change: `scripts/candidate-preflight` now records the installed
  `CFBundleShortVersionString` and SHA-256 hash of
  `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver/Contents/MacOS/OpenA8DJHAL` in
  every result.
- Change: `scripts/candidate-status` now reports
  `latest_candidate_preflight_installed_hash` and
  `latest_candidate_preflight_hash_status`, and only allows
  `READY_FOR_HUMAN_TEST` from a PASS preflight when that hash matches the
  currently installed driver hash.
- Change: `scripts/candidate-preflight --no-physical-gate` can no longer write
  `candidate_preflight=PASS`; if the physical candidate gate is skipped, the
  result is `DIAGNOSTIC_ONLY`.
- Risk found by fixture testing: accepting iRig by USB name alone is too loose
  for candidate promotion.
- Change: `scripts/capture-device-diagnose` still reports iRig name matches,
  but iRig Stream capture readiness now requires the exact USB ID
  `0x1963:0x0059` plus the Core Audio iRig input.
- Change: added `scripts/capture-device-diagnose-selftest` and
  `make capture-device-diagnose-selftest`.
- Self-test validation:
  `local-analysis/capture-device-diagnose-selftest/20260614-145204`.
- Result: PASS for decimal ID, `0x` hex ID, bare hex-like ID, USB-present but
  Core-Audio-missing, false USB device, and name-only ID-not-verified cases.
- Integrated preflight validation:
  `local-analysis/candidate-preflight/0.3.133-hashgate-20260614-144918`.
- Result: `candidate_preflight=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`,
  `installed_version=0.3.133`,
  `installed_hash=0d3bb34b0769cd947f608e129588183327775c62a58f4c52e47e38ede053041a`.
- Internal playback metrics from that run: start `0.096941 s`, first callback
  `0.104489 s`, driver CPU avg/p95 `6.13% / 7.00%`, coreaudiod p95 `2.00%`,
  stress driver avg/p95 `5.63% / 6.60%`, stress coreaudiod p95 `1.90%`,
  WindowServer p95 `19.70%`, output timeline resets `0`, active underruns `0`.
- Current status validation:
  `local-analysis/candidate-status/strict-irig-id-live-20260614-145204`.
- Result: `candidate_status=NOT_READY`,
  `reason=irig_missing_from_usb_tree`, `found_audio8_core_audio=1`,
  `found_irig_usb=0`, `found_irig_usb_by_id=0`, `found_irig_core_audio=0`.
- Decision: no human listening request and no readiness email. The current
  internal gates remain useful, but high-fidelity candidate proof is blocked
  until a real iRig Stream with USB ID `0x1963:0x0059` is visible in macOS and
  the physical tone/music gates pass against the baseline.

### Physical evidence gate hardening

- Follow-up audit risk: several scripts could still blur the boundary between
  diagnostic readiness and physical candidate approval.
- Change: `scripts/physical-bench-sanity-gate` now verifies the actual
  `audio-record` log after recording. For `iRig Stream`, it requires
  `device="iRig Stream"` and an `IK Multimedia:iRig Stream` Core Audio UID
  before it can pass.
- Change: `scripts/irig-recovery-gate` now requires
  `physical_capture_status=READY` from `capture-device-diagnose`, so partial
  flags cannot bypass the strict USB ID check. If it does not run
  `candidate-listen-gate`, a successful short capture reports
  `irig_recovery_gate=READY_FOR_PHYSICAL_GATE`, not `PASS`.
- Change: `scripts/candidate-preflight` treats `irig_usb_id_not_verified` as a
  physical-capture blocker.
- Change: `scripts/physical-music-quality-gate` marks any
  `--allow-non-irig` or `--allow-missing-cpu-profile` run as
  `measurement_status=DIAGNOSTIC_ONLY`, prevents
  `candidate_quality_status=PASS`, and returns a failing exit code for
  candidate gating.
- Change: `scripts/candidate-ready-email-gate` now performs a second readiness
  check before preparing an email payload: iRig USB ID, Core Audio iRig input,
  preflight status, preflight hash match, output-pair/timecode/playback CPU
  subgates, bench sanity, capture readiness, and the linked preflight result
  file.
- Validation: `bash -n` passed for the shell gate scripts, and
  `python3 -m py_compile scripts/physical-music-quality-gate` passed.
- Capture identity self-test:
  `local-analysis/capture-device-diagnose-selftest/20260614-145552`.
- Result: PASS for all fixture cases, including name-only iRig rejection.
- iRig recovery absent-hardware validation:
  `local-analysis/irig-recovery-gate/strict-ready-state-20260614-145552`.
- Result: `irig_recovery_gate=FAIL`,
  `reason=irig_missing_from_usb_tree`.
- Bench sanity absent-hardware validation:
  `local-analysis/physical-bench-sanity-gate/strict-record-identity-20260614-145608`.
- Result: `physical_bench_sanity_gate=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Diagnostic bypass validation:
  `local-analysis/physical-music-quality-gate/diagnostic-bypass-selftest-20260614-145637`.
- Result: exit code `1`, `measurement_status=DIAGNOSTIC_ONLY`,
  `candidate_quality_status=NOT_EVALUATED`, `verdict=FAIL`.
- Integrated preflight validation:
  `local-analysis/candidate-preflight/0.3.133-strict-physical-evidence-20260614-145703`.
- Result: `candidate_preflight=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`, installed hash
  `0d3bb34b0769cd947f608e129588183327775c62a58f4c52e47e38ede053041a`.
- Internal playback metrics from that run: start `0.090676 s`, first callback
  `0.098225 s`, driver CPU avg/p95 `6.26% / 6.80%`, coreaudiod p95 `1.70%`,
  stress driver avg/p95 `5.49% / 6.40%`, stress coreaudiod p95 `1.60%`,
  WindowServer p95 `17.00%`, output timeline resets `0`, active underruns `0`.
- Email gate validation:
  `local-analysis/candidate-ready-email-gate/after-physical-hardening-blocked-20260614-145906`.
- Result: `candidate_ready_email_gate=BLOCKED`,
  `reason=irig_missing_from_usb_tree`; no email sent.
- Final audio-stack guard:
  `local-analysis/audio-stack-guard/final-physical-gate-hardening-20260614-145847`.
- Result: PASS, OpenA8DJ loaded, global idle `85.57%`, watched CPU `0.0%`,
  driver/coreaudiod `0.0% / 0.0%`, Core Audio enumeration PASS.
- Decision: still no human listening request and no readiness email. The
  current installed build remains internally stable, but high-fidelity proof is
  blocked until the real iRig Stream reappears with USB ID `0x1963:0x0059` and
  passes physical music gates.

### Final listen-gate result-file hardening and iRig absence clarification

- Risk found: `candidate-listen-gate` parsed the physical music JSON verdict,
  but did not explicitly require `measurement_status=VALID` and
  `candidate_quality_status=PASS`.
- Change: both normal and stressed physical music JSON gates now require
  `measurement_status=VALID`, `candidate_quality_status=PASS`, and
  `verdict=PASS`.
- Change: `candidate-listen-gate` now writes `result.txt` with
  `candidate_listen_gate=PASS`, tone/music/stress subgate statuses, and reason.
- Change: `scripts/irig-recovery-gate` revalidates that
  `candidate-listen-gate/result.txt` exists and contains
  `candidate_listen_gate=PASS` when `--run-candidate-gate` is used.
- Change: `scripts/candidate-ready-email-gate` revalidates the nested
  `irig-recovery-gate/candidate-listen-gate/result.txt` before preparing any
  email payload.
- Synthetic JSON validation:
  `local-analysis/candidate-listen-gate-json-selftest/20260614-150119`.
- Result: a JSON with `verdict=PASS` but
  `measurement_status=DIAGNOSTIC_ONLY` and
  `candidate_quality_status=NOT_EVALUATED` produced `music_json_gate=FAIL`.
- Live clarification snapshot after user asked what "iRig absent" means:
  `local-analysis/capture-device-diagnose/user-asked-irig-absent-20260614-150242`.
- Result: `physical_capture_status=MISSING`,
  `reason=irig_missing_from_usb_tree`, `found_audio8_core_audio=1`,
  `found_irig_usb=0`, `found_irig_usb_by_id=0`, `found_irig_core_audio=0`.
  Core Audio listed only MacBook Air Microphone, MacBook Air Speakers, and
  Open Audio 8 DJ. USB listed `USB2.0 Hub`, `Audio 8 DJ`,
  `USB Type-C Digital AV Adapter`, and `Shadow`.
- Integrated preflight validation:
  `local-analysis/candidate-preflight/0.3.133-listen-result-hardening-20260614-150150`.
- Result: `candidate_preflight=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`, installed hash
  `0d3bb34b0769cd947f608e129588183327775c62a58f4c52e47e38ede053041a`.
- Internal playback metrics from that run: start `0.089382 s`, first callback
  `0.096931 s`, driver CPU avg/p95 `5.93% / 6.90%`, coreaudiod p95 `1.90%`,
  stress driver avg/p95 `5.27% / 6.30%`, stress coreaudiod p95 `1.50%`,
  WindowServer p95 `18.70%`, output timeline resets `0`, active underruns `0`.
- Final guard:
  `local-analysis/audio-stack-guard/final-listen-result-hardening-20260614-150333`.
- Result: PASS, OpenA8DJ loaded, global idle `86.32%`, watched CPU `0.0%`,
  driver/coreaudiod `0.0% / 0.0%`, Core Audio enumeration PASS.
- Clarification: "iRig absent" means macOS does not enumerate it as USB or Core
  Audio hardware. It does not contradict the user's physical cable state; it
  means the operating system currently cannot record from iRig Stream.

### Candidate status freshness hardening

- Risk found: some latest-result lookups in `candidate-status` used path sort
  rather than file mtime, which could select stale evidence when directory names
  and creation order diverge.
- Change: `scripts/candidate-status` now selects generic latest result files by
  result mtime for output-pair, timecode, bench-sanity, and watch results. The
  preflight selector already used mtime across standalone and watch-embedded
  preflight results.
- Change: `scripts/candidate-watch` now records `found_irig_usb_by_id` in
  `latest-status.txt` and `device-readiness.txt`, and blocks unless
  `physical_capture_status=READY` from `capture-device-diagnose`.
- Fresh current capture diagnosis:
  `local-analysis/capture-device-diagnose/goal-continue-fresh-20260614-150453`.
- Result: `physical_capture_status=MISSING`,
  `reason=irig_missing_from_usb_tree`, `found_audio8_core_audio=1`,
  `found_irig_usb=0`, `found_irig_usb_by_id=0`, `found_irig_core_audio=0`.
- Freshness self-test:
  `local-analysis/candidate-status/freshness-selftest-20260614-150553`.
- Result: candidate-status chose the newest output-pair result by mtime even
  when its path sorted earlier alphabetically.
- Candidate watch validation:
  `local-analysis/candidate-watch/freshness-strict-id-20260614-150608`.
- Result: `candidate_watch=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`.
- Candidate status validation after watch:
  `local-analysis/candidate-status/after-watch-freshness-20260614-150620`.
- Result: status selected the fresh watch result
  `local-analysis/candidate-watch/freshness-strict-id-20260614-150608/result.txt`
  and remained `candidate_status=NOT_READY`.
- Email gate validation:
  `local-analysis/candidate-ready-email-gate/freshness-blocked-20260614-150629`.
- Result: `candidate_ready_email_gate=BLOCKED`,
  `reason=irig_missing_from_usb_tree`; no email sent.
- Final guard:
  `local-analysis/audio-stack-guard/final-freshness-hardening-20260614-150629`.
- Result: PASS, OpenA8DJ loaded, global idle `67.25%`, watched CPU `0.0%`,
  driver/coreaudiod `0.0% / 0.0%`, Core Audio enumeration PASS.
- Decision: current candidate evidence is fresh and still blocked only by the
  missing physical capture device. No human listening and no readiness email.

### iRig USB recovery diagnosis after physical-plug report

- User report: iRig Stream is physically plugged in and should remain connected.
- Interpretation policy: "iRig absent" means absent to macOS, not necessarily
  physically unplugged.
- Added reusable recovery target: `make irig-usb-recovery-diagnose`.
- The target captures USB/Core Audio snapshots, restarts audio services, tries
  an exact iRig USB reset for `0x1963:0x0059` only if IOKit can match it, then
  watches for recovery and records USB port counters from IOService.
- Manual USB snapshot before scripting: macOS USB devices were only
  `USB2.0 Hub`, `Audio 8 DJ`, `USB Type-C Digital AV Adapter`, and `Shadow`.
  iRig USB ID `0x1963:0x0059` was not present; Core Audio listed no iRig input.
- Exact iRig reset attempt: `build/usb-reset-device 0x1963 0x0059` returned
  `matching failed: 0x00000000`, i.e. there is no matched USB device for macOS
  to reset.
- Scripted recovery run:
  `local-analysis/irig-recovery-investigation/scripted-recovery-portstats-20260614-151502`.
- Result: `irig_recovery=MISSING`,
  `reason=irig_missing_from_usb_tree`, `found_irig_usb_by_id=0`,
  `found_irig_core_audio=0`.
- USB devices in the final watch snapshot:
  `USB2.0 Hub,Audio 8 DJ,USB Type-C Digital AV Adapter,Shadow`.
- Port evidence from
  `local-analysis/irig-recovery-investigation/scripted-recovery-portstats-20260614-151502/watch-2/usb-port-summary.txt`:
  `AppleUSB20HubPort@1114112 status=256 connect=16 enum_fail=16 addr_fail=16 over_current=0`.
- Interpretation: the system has a port-level USB handshake/enumeration failure
  before an iRig USB device exists. This is not recoverable by OpenA8DJ or Core
  Audio code while the iRig ID is absent from IOKit.
- Required recovery action: power-cycle iRig or move it to a direct Mac port /
  known-good data cable or hub, then rerun `make irig-usb-recovery-diagnose`.
  Do not rebuild the driver, request listening, or send readiness email until
  iRig reappears with USB ID `0x1963:0x0059` and Core Audio input.
- Final audio-stack guard:
  `local-analysis/audio-stack-guard/irig-recovery-final-20260614-151529`.
- Result: PASS, OpenA8DJ loaded, global idle `76.54%`, total watched CPU `0.3%`,
  driver/coreaudiod `0.0% / 0.0%`, Core Audio enumeration PASS.

### iRig USB recovery delta hardening

- Change: `scripts/irig-usb-recovery-diagnose` now writes `port_delta` and
  reports `usb_port_counter_changes`.
- Purpose: distinguish stale accumulated USB port failures from a fresh
  physical reconnect/power-cycle attempt that actually reached the USB bus.
- Syntax validation: `bash -n scripts/irig-usb-recovery-diagnose` PASS.
- Short Make validation:
  `local-analysis/irig-recovery-investigation/recovery-20260614-151745`.
- Result: `irig_recovery=MISSING`,
  `reason=irig_missing_from_usb_tree`, `found_irig_usb_by_id=0`,
  `found_irig_core_audio=0`, `usb_enumeration_failures=YES`,
  `usb_port_counter_changes=NO`.
- Port delta:
  `local-analysis/irig-recovery-investigation/recovery-20260614-151745/port-delta.txt`.
  All ports had zero deltas during the watch window; the failing hub port still
  had accumulated `connect=16`, `enum_fail=16`, `addr_fail=16`,
  `over_current=0`.
- Capture detector self-test:
  `local-analysis/capture-device-diagnose-selftest/20260614-151803`.
- Result: PASS for decimal, `0x`, bare hex, Core-Audio-missing, false USB
  device, and name-only iRig fixture cases.
- Candidate status:
  `local-analysis/candidate-status/20260614-151803`.
- Result: `candidate_status=NOT_READY`,
  `reason=irig_missing_from_usb_tree`, installed version `0.3.133`, installed
  hash `0d3bb34b0769cd947f608e129588183327775c62a58f4c52e47e38ede053041a`.
  Audio stack PASS; physical capture still missing. No human listening and no
  readiness email.

### Candidate status now exposes iRig USB recovery evidence

- Change: `scripts/candidate-status` now reports the latest
  `irig-usb-recovery-diagnose` result in the same readiness summary.
- Added fields:
  `latest_irig_usb_recovery_status`,
  `latest_irig_usb_recovery_reason`,
  `latest_irig_usb_recovery_found_irig_usb_by_id`,
  `latest_irig_usb_recovery_found_irig_core_audio`,
  `latest_irig_usb_recovery_usb_enumeration_failures`,
  `latest_irig_usb_recovery_usb_port_counter_changes`,
  `latest_irig_usb_recovery_port_summary`,
  `latest_irig_usb_recovery_port_delta`,
  `latest_irig_usb_recovery_next_action`, and
  `latest_irig_usb_recovery_result`.
- Validation:
  `local-analysis/candidate-status/irig-usb-recovery-status-20260614-152021`.
- Result: `candidate_status=NOT_READY`,
  `reason=irig_missing_from_usb_tree`, installed version `0.3.133`, installed
  hash `0d3bb34b0769cd947f608e129588183327775c62a58f4c52e47e38ede053041a`,
  audio stack PASS.
- Latest iRig USB recovery evidence surfaced by that status:
  `latest_irig_usb_recovery_status=MISSING`,
  `latest_irig_usb_recovery_reason=irig_missing_from_usb_tree`,
  `latest_irig_usb_recovery_usb_enumeration_failures=YES`,
  `latest_irig_usb_recovery_usb_port_counter_changes=NO`,
  `latest_irig_usb_recovery_result=local-analysis/irig-recovery-investigation/recovery-20260614-151745/result.txt`.
- Readiness email gate validations:
  `local-analysis/candidate-ready-email-gate/20260614-151851` and
  `local-analysis/candidate-ready-email-gate/20260614-152119`.
- Result: `candidate_ready_email_gate=BLOCKED`,
  `reason=irig_missing_from_usb_tree`; no email sent.
- Final audio stack guard:
  `local-analysis/audio-stack-guard/final-candidate-status-irig-recovery-20260614-152119`.
- Result: PASS, OpenA8DJ loaded, global idle `65.10%`, total watched CPU `4.2%`,
  driver/coreaudiod `0.1% / 3.4%`, Core Audio enumeration PASS.

### iRig absent means absent from macOS USB, not physically unplugged

- User confirmed the iRig Stream is physically connected and should remain
  connected.
- Current macOS evidence: Core Audio lists only `MacBook Air Microphone`,
  `MacBook Air Speakers`, and `Open Audio 8 DJ`; IOUSB lists `USB2.0 Hub`,
  `Audio 8 DJ`, `USB Type-C Digital AV Adapter`, and `Shadow`, but no
  `iRig Stream`, no `IK Multimedia`, and no expected iRig USB id
  `0x1963:0x0059`.
- Latest recovery run:
  `local-analysis/irig-recovery-investigation/recovery-20260614-152953`.
- Result:
  `irig_recovery=MISSING`,
  `reason=irig_missing_from_usb_tree`,
  `software_recovery_limit=no_irig_usb_object_to_reset`.
- Failed-port evidence:
  `port_01=AppleUSB20HubPort@1114112 status=256 connect=16 enum_fail=16 addr_fail=16 over_current=0`.
  No port counters changed during the watch window, so macOS did not observe a
  fresh successful iRig enumeration attempt.
- A controlled reset attempt against the USB2 hub id `0x05e3:0x0608` failed with
  `open failed: Failed to create IOUSBHostObject`; Audio 8 DJ remained present.
- Interpretation: the iRig is physically present in the setup, but digitally
  absent to macOS. If it is missing from the USB tree, software cannot reset the
  iRig by id because no iRig USB object exists. The recovery path is physical
  iRig power-cycle/direct-port or hub-port recovery, then rerun
  `scripts/irig-usb-recovery-diagnose` and only continue physical gates after
  it reports `READY`.

### Audio gate concurrency hardening

- Problem found: a playback CPU gate was previously run at the same time as
  output-pair and timecode smoke gates. That can make CPU/latency measurements
  invalid and produce false failures or false confidence.
- Change: added shared gate lock `scripts/audio-gate-lock.sh` and wired it into
  `scripts/output-pair-smoke-gate`, `scripts/timecode-smoke-gate`, and
  `scripts/playback-cpu-gate`.
- Behavior: if another audio gate is active, the new gate exits quickly with
  `audio_gate_lock_busy` and writes the owning pid/gate/run directory instead
  of running a contaminated measurement.
- Validation:
  - `bash -n scripts/audio-gate-lock.sh scripts/output-pair-smoke-gate scripts/timecode-smoke-gate` PASS.
  - `python3 -m py_compile scripts/playback-cpu-gate` PASS.
  - Artificial busy-lock tests for output-pair, timecode, and playback CPU all
    returned exit code `75` with `reason=audio_gate_lock_busy`.

### Sequential internal gate rerun after lock hardening

- Purpose: rerun the internal gates without concurrency after adding the shared
  audio-gate lock, to classify the earlier coreaudiod CPU failure.
- Pre-guard:
  `local-analysis/audio-stack-guard/pre-sequential-gates-20260614-153540`.
  Result PASS, driver/coreaudiod `0.0% / 0.0%`, Core Audio enumeration PASS.
- Output-pair smoke:
  `local-analysis/output-pair-smoke-gate/0.3.133-sequential-lock-20260614-153555`.
  Result PASS for A/B/C/D; no output timeline resets, active underruns, or
  panic flags. Device-start range `0.086976s` to `0.094704s`; first-callback
  range `0.094523s` to `0.102251s`.
- Timecode smoke:
  `local-analysis/timecode-smoke-gate/sequential-lock-20260614-153615`.
  Result PASS, profile apply/duplex smoke/restore path passed.
- Playback CPU/UI stress:
  `local-analysis/playback-cpu-gate-0.3.133-sequential-lock-20260614-153638`.
  Result PASS. Start `0.099876s`, first callback `0.102924s`, driver avg/p95
  `6.07% / 6.80%`, coreaudiod p95 `1.80%`, stress driver avg/p95
  `5.49% / 6.40%`, stress coreaudiod p95 `1.60%`, WindowServer p95 `17.80%`,
  UI stress files `13`, transfers `7515`, frames written/read
  `2885632 / 2885686`, timeline resets `0`, active underruns `0`.
- Post-guard:
  `local-analysis/audio-stack-guard/post-sequential-gates-20260614-153755`.
  Result PASS, driver/coreaudiod `0.0% / 0.0%`, Core Audio enumeration PASS,
  no audio-gate lock left behind.
- Decision: the previous playback CPU failure with coreaudiod p95 above the
  threshold was caused by concurrent gate contamination. The installed
  `0.3.133` remains internally healthy, but it is still not a human-listening
  candidate while iRig is absent from macOS USB/Core Audio and physical gates
  cannot run.
- Candidate status after exposing the standalone playback CPU gate:
  `local-analysis/candidate-status/20260614-153911`.
  Result `candidate_status=NOT_READY`, `reason=irig_missing_from_usb_tree`;
  latest playback CPU fields show PASS, driver p95 `6.80%`, coreaudiod p95
  `1.80%`, stress coreaudiod p95 `1.60%`, timeline resets `0`, active
  underruns `0`.
- Readiness email gate:
  `local-analysis/candidate-ready-email-gate/20260614-153935`.
  Result `candidate_ready_email_gate=BLOCKED`,
  `reason=irig_missing_from_usb_tree`; no readiness email sent.

### Integrated candidate preflight with gate locks

- Purpose: run the actual pre-human-test ladder after lock hardening, so the
  latest preflight result reflects the same serial execution discipline as the
  standalone gates.
- Pre-guard:
  `local-analysis/audio-stack-guard/pre-integrated-preflight-20260614-154042`.
  Result PASS, driver/coreaudiod `0.0% / 0.0%`, Core Audio enumeration PASS.
- Candidate preflight:
  `local-analysis/candidate-preflight/0.3.133-integrated-lock-20260614-154112`.
- Result:
  `candidate_preflight=BLOCKED_PHYSICAL_CAPTURE`,
  `reason=irig_missing_from_usb_tree`, installed version `0.3.133`, installed
  hash `0d3bb34b0769cd947f608e129588183327775c62a58f4c52e47e38ede053041a`.
- Subgates:
  output-pair smoke PASS, timecode smoke PASS, playback CPU/UI stress PASS,
  physical bench sanity `BLOCKED_PHYSICAL_CAPTURE`.
- Playback CPU inside integrated preflight:
  start `0.132200s`, first callback `0.139760s`, driver avg/p95
  `5.98% / 6.80%`, coreaudiod p95 `1.80%`, stress driver avg/p95
  `5.25% / 6.40%`, stress coreaudiod p95 `1.60%`, WindowServer p95 `17.60%`,
  UI stress files `13`, transfers `7516`, frames written/read
  `2886144 / 2886071`, timeline resets `0`, active underruns `0`.
- Capture diagnosis inside integrated preflight:
  `physical_capture_status=MISSING`,
  `reason=irig_missing_from_usb_tree`, `found_audio8_core_audio=1`,
  `found_irig_usb=0`, `found_irig_usb_by_id=0`, `found_irig_core_audio=0`,
  external Core Audio inputs `0`.
- Candidate status after integrated preflight:
  `local-analysis/candidate-status/20260614-154309`.
  Result `candidate_status=NOT_READY`, `reason=irig_missing_from_usb_tree`,
  audio stack PASS, latest preflight is the integrated run above.
- Readiness email gate:
  `local-analysis/candidate-ready-email-gate/20260614-154309`.
  Result `candidate_ready_email_gate=BLOCKED`,
  `reason=irig_missing_from_usb_tree`; no readiness email sent.

### Capture diagnosis now surfaces USB port enumeration failures

- Problem: `irig-usb-recovery-diagnose` exposed the failing USB hub port, but
  the common `capture-device-diagnose` used by `candidate-status` and
  `candidate-watch` only reported `irig_missing_from_usb_tree`. That made the
  normal readiness summaries less explanatory than the recovery-specific tool.
- Change: `scripts/capture-device-diagnose` now captures a second read-only
  IOService USB-port snapshot and emits `usb_enumeration_failures` plus
  `failed_usb_ports` when macOS exposes hub/port counters.
- Change: `scripts/candidate-status` and `scripts/candidate-watch` now preserve
  those fields. A parser bug that truncated values containing `=` was fixed so
  `failed_usb_ports` keeps the full port detail.
- Validation:
  - `bash -n scripts/capture-device-diagnose scripts/candidate-status
    scripts/candidate-watch scripts/candidate-watch-ready-email-gate` PASS.
  - `local-analysis/capture-device-diagnose-selftest/usb-port-fields-v2-20260614-154636`
    PASS for all fixture cases.
  - Real hardware diagnosis:
    `local-analysis/capture-device-diagnose/usb-port-fields-real-v2-20260614-154637`.
    Result `physical_capture_status=MISSING`,
    `reason=irig_missing_from_usb_tree`,
    `usb_enumeration_failures=YES`,
    `failed_usb_ports=name=AppleUSB20HubPort,class=AppleUSB20HubPort,location=1114112,status=256,connect=16,enum_fail=16,addr_fail=16,over_current=0`.
  - Candidate status:
    `local-analysis/candidate-status/usb-port-fields-fixed-20260614-154653`.
    Result `candidate_status=NOT_READY`,
    `reason=irig_missing_from_usb_tree`,
    `usb_enumeration_failures=YES`, with full failed-port detail.
  - Candidate watch:
    `local-analysis/candidate-watch/usb-port-fields-fixed-20260614-154801`.
    Result `candidate_watch=BLOCKED_PHYSICAL_CAPTURE`,
    `reason=irig_missing_from_usb_tree`,
    `usb_enumeration_failures=YES`, with full failed-port detail.
  - Watch-to-email:
    `local-analysis/candidate-watch-ready-email-gate/usb-port-fields-fixed-20260614-154819`.
    Result `candidate_watch_ready_email_gate=BLOCKED`,
    `reason=irig_missing_from_usb_tree`; no email payload prepared and no email
    sent.

### Actionable recovery hints in readiness outputs

- Problem: the enriched diagnosis showed the failed USB port, but operators
  still had to infer the next action from the reason.
- Change: `scripts/capture-device-diagnose` now emits
  `next_recovery_action`. `scripts/candidate-status` exposes it as
  `capture_next_recovery_action`, `scripts/candidate-watch` includes it in
  blocked results, and `scripts/candidate-watch-ready-email-gate` copies it to
  the top-level blocked result.
- Validation:
  - `bash -n scripts/capture-device-diagnose scripts/candidate-status
    scripts/candidate-watch scripts/candidate-watch-ready-email-gate` PASS.
  - `local-analysis/capture-device-diagnose-selftest/next-action-20260614-155059`
    PASS.
  - Real hardware diagnosis reported:
    `next_recovery_action=power_cycle_iRig_or_move_iRig_to_direct_mac_port_then_rerun_capture_device_diagnose`.
  - Candidate status:
    `local-analysis/candidate-status/next-action-20260614-155116` showed
    `capture_next_recovery_action=power_cycle_iRig_or_move_iRig_to_direct_mac_port_then_rerun_capture_device_diagnose`.
  - Candidate watch:
    `local-analysis/candidate-watch/next-action-20260614-155116` blocked with
    `usb_enumeration_failures=YES`, full failed-port detail, and the same
    `next_recovery_action`.
  - Watch-to-email:
    `local-analysis/candidate-watch-ready-email-gate/next-action-wrapper-20260614-155154`
    blocked before email and now includes `usb_enumeration_failures=YES`, full
    failed-port detail, and the same `next_recovery_action` in its top-level
    `result.txt`.

### Safe replug watcher LaunchAgent

- User constraint: no one is near the laptop, so avoid rebooting or any USB
  reset that could lose control. Need a way to wait safely for a physical
  hub/iRig replug.
- Change: added `scripts/start-safe-replug-watch`,
  `scripts/safe-replug-watch-status`, and `scripts/stop-safe-replug-watch`,
  plus Make targets `safe-replug-watch-start`, `safe-replug-watch-status`, and
  `safe-replug-watch-stop`.
- Behavior: starts a user LaunchAgent
  `com.fer.opena8dj.safe-replug-watch`. While iRig is absent it only polls
  USB/Core Audio via the guarded watch path. It does not reset USB, restart
  Core Audio, reboot, install, or touch the HAL. If iRig returns, it runs the
  existing `candidate-watch-ready-email-gate`; that still never sends mail by
  itself.
- Reboot/autologin audit before this change:
  FileVault off, `autoLoginUser=fer`, `sudo -n true` PASS, existing
  `com.fer.opena8dj.audio-qa-startup` and `com.fer.opena8dj.codex-resume`
  LaunchAgents loaded with previous exit code `0`. Risk remains that Codex app
  continuity is not guaranteed, so do not reboot unless unavoidable.
- Smoke validation:
  `local-analysis/safe-replug-watch/launchagent-smoke-20260614-155919`.
  LaunchAgent started with PID `79734`, exited with blocked result:
  `candidate_watch_ready_email_gate=BLOCKED`,
  `reason=irig_missing_from_usb_tree`, `usb_enumeration_failures=YES`, full
  failed-port detail, and
  `next_recovery_action=power_cycle_iRig_or_move_iRig_to_direct_mac_port_then_rerun_capture_device_diagnose`.
- Stop/status validation:
  `scripts/stop-safe-replug-watch` booted out the LaunchAgent successfully, and
  `scripts/safe-replug-watch-status` reported the last blocked result. No long
  watcher is currently left running.

### Stable iRig readiness before physical gates

- Time: 2026-06-14 16:04:43 -0400.
- Context: user may arrange a one-time hub power-cycle, and the system must not
  launch long physical gates from a transient or unstable iRig enumeration.
- Current passive diagnosis:
  `local-analysis/capture-device-diagnose/continue-20260614-160221`.
  Result `physical_capture_status=MISSING`,
  `reason=irig_missing_from_usb_tree`, `found_audio8_core_audio=1`,
  `found_irig_usb_by_id=0`, `found_irig_core_audio=0`,
  `usb_enumeration_failures=YES`,
  `failed_usb_ports=name=AppleUSB20HubPort,class=AppleUSB20HubPort,location=1114112,status=256,connect=16,enum_fail=16,addr_fail=16,over_current=0`.
- Current stack health:
  `local-analysis/audio-stack-guard/continue-20260614-160221` PASS.
  Driver/coreaudiod were low (`opena8dj_driver=0.2%`,
  `coreaudiod=3.1%`) and Core Audio enumeration passed.
- Change: `scripts/candidate-watch` now requires stable iRig readiness for
  `--stable-polls` consecutive polls before running `candidate-preflight`.
  Default is 3 polls. If iRig appears but does not remain stable, the blocked
  reason is `irig_not_stable_yet`.
- Change: `scripts/candidate-watch-ready-email-gate`,
  `scripts/start-safe-replug-watch`, and Make variables now pass the same stable
  poll threshold through:
  `CANDIDATE_WATCH_STABLE_POLLS`,
  `CANDIDATE_WATCH_READY_EMAIL_STABLE_POLLS`, and
  `SAFE_REPLUG_WATCH_STABLE_POLLS`.
- Safety: this is instrumentation/control-flow only. It does not reset USB,
  restart Core Audio, reboot, install, or change driver audio behavior.
- Validation:
  - `bash -n scripts/candidate-watch scripts/candidate-watch-ready-email-gate
    scripts/start-safe-replug-watch scripts/safe-replug-watch-status
    scripts/stop-safe-replug-watch` PASS.
  - `make -n candidate-watch ... CANDIDATE_WATCH_STABLE_POLLS=4`,
    `make -n candidate-watch-ready-email-gate ...
    CANDIDATE_WATCH_READY_EMAIL_STABLE_POLLS=4`, and
    `make -n safe-replug-watch-start SAFE_REPLUG_WATCH_STABLE_POLLS=4`
    all propagate `--stable-polls`.
  - Short live watch:
    `local-analysis/candidate-watch/stable-polls-smoke-20260614-160536`
    blocked as expected with `reason=irig_missing_from_usb_tree`,
    `ready_streak=0`, `stable_polls=2`,
    `usb_enumeration_failures=YES`, and full failed-port detail.
  - Post-change stack health:
    `local-analysis/audio-stack-guard/post-stable-polls-edit-20260614-160536`
    PASS, driver `0.0%`, coreaudiod `0.6%`.
- Active watcher:
  `local-analysis/safe-replug-watch/0.3.133-stable-irig-watch-20260614-160558`.
  It is running as `com.fer.opena8dj.safe-replug-watch`, pid `91743`, with
  wait `7200`, interval `5`, and `stable_polls=3`.
  Latest observed status reached attempt 4 with `ready_streak=0`,
  `physical_capture_status=MISSING`, `reason=irig_missing_from_usb_tree`.
  Post-start stack health
  `local-analysis/audio-stack-guard/post-safe-replug-watch-start-20260614-160603`
  PASS, driver `0.0%`, coreaudiod `0.7%`.

### Physical music gate coloration metric

- Problem: earlier false positives could still sound metallic or
  high-pass-filtered even when some residual counters looked plausible.
- Change: `scripts/analyze-soundcheck-capture.py` now emits spectral-color
  metrics comparing physical capture vs digital reference:
  `low_band_capture_to_ref_gain_db`, `mid_band_capture_to_ref_gain_db`,
  `high_band_capture_to_ref_gain_db`, `mid_vs_low_coloration_delta_db`,
  `high_vs_low_coloration_delta_db`, `high_vs_mid_coloration_delta_db`, and
  `metallic_coloration_score_db`.
- Change: `scripts/physical-music-quality-gate` now rejects excessive absolute
  coloration (`mid_vs_low` > +/-5 dB, `high_vs_low` > +/-6 dB,
  `metallic_coloration_score_db` > 6 dB) and, when the baseline contains the
  same keys, rejects regressions above baseline + 0.75 dB.
- Safety: analysis-only change. It does not play audio, capture audio, reset
  USB, restart Core Audio, install, or change driver behavior.
- Validation:
  - `python3 -m py_compile scripts/analyze-soundcheck-capture.py
    scripts/physical-music-quality-gate scripts/analyze-music-capture.py` PASS.
  - Archived physical run:
    `local-analysis/physical-baseline-0.3.111-iso5-normal-20260614-105615`
    analyzed to
    `physical-music-gate-coloration-smoke.json`.
    New metrics included `low_band_capture_to_ref_gain_db=5.73`,
    `mid_band_capture_to_ref_gain_db=5.56`,
    `high_band_capture_to_ref_gain_db=4.39`,
    `mid_vs_low_coloration_delta_db=-0.17`,
    `high_vs_low_coloration_delta_db=-1.34`, and
    `metallic_coloration_score_db=1.34`.
    The run still failed existing strict criteria (residual/click/lag/CPU), but
    it did not fail from the new coloration metric.
  - Post-change stack health:
    `local-analysis/audio-stack-guard/post-coloration-gate-20260614-160848`
    PASS, driver `0.0%`, coreaudiod `0.0%`.

### Candidate status reports active safe watcher

- Problem: `candidate-status` reported the latest closed `candidate-watch`
  result, but not the currently running `safe-replug-watch` LaunchAgent. During
  remote hub/iRig recovery that made it too easy to lose track of the active
  passive watcher.
- Change: `scripts/candidate-status` now reads
  `local-analysis/safe-replug-watch/current.env` and reports
  `safe_replug_watch_status`, `safe_replug_watch_pid`,
  `safe_replug_watch_run_dir`, `safe_replug_watch_result`, and
  `safe_replug_watch_stable_polls`.
- Validation:
  - `bash -n scripts/candidate-status` PASS.
  - `local-analysis/candidate-status/with-safe-watch-20260614-161014`
    reported `candidate_status=NOT_READY`,
    `reason=irig_missing_from_usb_tree`,
    `safe_replug_watch_status=RUNNING`, `safe_replug_watch_pid=91743`,
    and `safe_replug_watch_stable_polls=3`.
  - Post-change stack health:
    `local-analysis/audio-stack-guard/post-candidate-status-watch-fields-20260614-161014`
    PASS, driver `0.2%`, coreaudiod `2.9%`.

### Long passive iRig watcher and offline coloration calibration

- Context: user said the physical hub power-cycle may take a while. Reboot was
  judged riskier than waiting for a targeted hub/iRig power-cycle.
- Action: stopped the 2-hour safe watcher and started a longer 8-hour passive
  watcher:
  `local-analysis/safe-replug-watch/0.3.133-stable-irig-watch-long-20260614-161411`.
  LaunchAgent `com.fer.opena8dj.safe-replug-watch`, pid `11770`,
  `wait_seconds=28800`, `interval_seconds=5`, `stable_polls=3`.
- Watcher validation: by attempt 3 it still reported
  `physical_capture_status=MISSING`, `reason=irig_missing_from_usb_tree`,
  `ready_streak=0`, `usb_enumeration_failures=YES`, and the same failed hub
  port detail. It has not launched physical gates.
- Stack validation:
  `local-analysis/audio-stack-guard/post-long-watch-start-20260614-161418`
  PASS, driver `0.0%`, coreaudiod `0.0%`.
- CPU safety: a partial offline recalculation was stopped after the user asked
  about reboot safety. No `physical-music-quality-gate` / ffmpeg process was
  left running; only the passive watcher remained.
- Partial offline coloration calibration:
  `local-analysis/coloration-gate-calibration-20260614-161221`.
  Useful rows before stopping:
  - `human-metallic-0.3.25`: `verdict=FAIL`,
    `quality_alignment_score=0.022833`,
    `metallic_coloration_score_db=15.788746`,
    `mid_vs_low_coloration_delta_db=14.423804`,
    `high_vs_low_coloration_delta_db=15.788746`,
    `mid_band_residual_ratio=36.036703`,
    `mid_band_cpu_corr_max=0.756332`,
    `cpu_opena8dj_driver_p95=33.915`.
    This confirms the new coloration metric catches the historically rejected
    metallic/noisy listening failure.
  - `coalesce-bad-0.3.63-A`: `verdict=FAIL`,
    `metallic_coloration_score_db=26.518254`,
    `mid_band_residual_ratio=224.629210`.
  - `recovery-0.3.64-A`: `verdict=FAIL` under current strict gates due
    residual/quiet-noise/lag/CPU, but not due metallic coloration
    (`metallic_coloration_score_db=1.333962`).
- Documentation fix: `docs/PHYSICAL_MUSIC_QUALITY_GATE.md` now matches the
  current strict defaults for clicks and CPU (`0` clicks, driver avg/p95
  `8%/12%`, coreaudiod p95 `8%`).

### Fast selftest for coloration gate

- Change: added `scripts/physical-music-quality-gate-selftest` and Make target
  `physical-music-quality-gate-selftest`.
- Purpose: give the spectral-coloration metric a cheap deterministic regression
  test that does not use hardware, Core Audio, ffmpeg, or archived WAVs.
- Validation:
  - `python3 -m py_compile scripts/physical-music-quality-gate-selftest
    scripts/analyze-soundcheck-capture.py scripts/physical-music-quality-gate`
    PASS.
  - `make -n physical-music-quality-gate-selftest` expands to
    `./scripts/physical-music-quality-gate-selftest`.
  - Selftest result PASS:
    `uniform_metallic_coloration_score_db=0.000000`,
    `metallic_coloration_score_db=13.350449`,
    `metallic_mid_vs_low_coloration_delta_db=12.996999`,
    `metallic_high_vs_low_coloration_delta_db=13.350449`,
    `bass_heavy_coloration_score_db=12.005268`.
- Post-selftest stack health:
  `local-analysis/audio-stack-guard/post-coloration-selftest-20260614-161626`
  PASS, driver `0.0%`, coreaudiod `0.1%`.
- Watcher state during this pass:
  long safe watcher remained running; latest checked attempt 25 still had
  `physical_capture_status=MISSING`, `reason=irig_missing_from_usb_tree`, and
  `ready_streak=0`.

### Safe watcher status includes latest poll detail

- Problem: checking the long safe watcher required one command for watcher
  status and another manual read of `latest-status.txt`.
- Change: `scripts/safe-replug-watch-status` now reports runner metadata plus
  latest `candidate-watch` poll fields: attempt, `ready_streak`, stable poll
  threshold, Audio 8 / iRig USB/Core Audio visibility, USB enumeration failures,
  failed USB port detail, next recovery action, and the linked capture diagnosis
  file.
- Validation:
  - `bash -n scripts/safe-replug-watch-status` PASS.
  - Live status reported `safe_replug_watch_status=RUNNING`, pid `11770`,
    `wait_seconds=28800`, `interval_seconds=5`, `stable_polls=3`,
    `latest_attempt=40`, `latest_ready_streak=0`,
    `latest_physical_capture_status=MISSING`,
    `latest_physical_capture_reason=irig_missing_from_usb_tree`,
    `latest_usb_enumeration_failures=YES`, and the same failed hub-port detail.
  - Post-change stack health:
    `local-analysis/audio-stack-guard/post-watch-status-detail-20260614-161745`
    PASS, driver `0.0%`, coreaudiod `0.0%`.

### Autonomous software recovery supervisor

- Context: user clarified that iRig Stream and Audio 8 DJ will remain
  physically connected and that absence of iRig must be treated as a software
  recovery problem, not a request for cable intervention.
- Change: added an unattended supervisor:
  - `scripts/autonomous-audio-qa-supervisor`
  - `scripts/start-autonomous-audio-qa`
  - `scripts/autonomous-audio-qa-status`
  - `scripts/stop-autonomous-audio-qa`
  - Make targets: `autonomous-audio-qa-start`,
    `autonomous-audio-qa-status`, `autonomous-audio-qa-stop`
- Behavior:
  - First missing-iRig cycle attempts bounded software recovery immediately;
    later recovery attempts are spaced by cycle count.
  - Recovery refreshes Core Audio, audio HAL, `usbaudiod`, and `usbpowerd`.
  - It only attempts an exact iRig VID/PID reset (`0x1963:0x0059`) and never
    resets Audio 8 DJ.
  - The permanent supervisor uses `irig-usb-recovery-diagnose --lightweight`,
    keeping `ioreg`/port-counter/Core Audio evidence while skipping repeated
    `system_profiler` snapshots.
  - Once Audio 8 DJ and iRig are stable for 3 consecutive polls, it runs the
    full `candidate-watch-ready-email-gate` ladder and prepares the email
    payload only after all physical, CPU, output-pair, and timecode gates pass.
  - `scripts/post-reboot-audio-test-startup` now starts this supervisor after
    reboot, so Codex resume and QA recovery are both relaunched at login.
- Validation:
  - `bash -n` PASS for the new supervisor scripts plus
    `post-reboot-audio-test-startup` and `irig-usb-recovery-diagnose`.
  - `make -n autonomous-audio-qa-start autonomous-audio-qa-status
    autonomous-audio-qa-stop` expands correctly.
  - Pre-smoke stack health:
    `local-analysis/audio-stack-guard/autonomous-preflight-20260614-162550`
    PASS, driver `0.0%`, coreaudiod `0.0%`, idle CPU `74.91%`.
  - One active smoke run:
    `local-analysis/autonomous-audio-qa/autonomy-smoke-20260614-162612`.
    Result: `autonomous_audio_qa=INCOMPLETE`,
    `physical_capture_status=MISSING`, `reason=irig_missing_from_usb_tree`.
    Recovery evidence:
    `cycle-1/irig-usb-recovery-diagnose/result.txt`,
    `irig_recovery=MISSING`, `found_irig_usb_by_id=0`,
    `found_irig_core_audio=0`,
    `usb_enumeration_failures=YES`,
    `software_recovery_limit=no_irig_usb_object_to_reset`,
    `next_recovery_action=continue_autonomous_software_recovery_or_reboot_if_policy_allows`.
  - Post-smoke stack health:
    `local-analysis/audio-stack-guard/autonomous-post-smoke-20260614-162638`
    PASS, driver `0.0%`, coreaudiod `0.5%`, idle CPU `77.59%`.
  - Lightweight recovery smoke:
    `local-analysis/irig-recovery-investigation/lightweight-smoke-20260614-163034`
    completed in lightweight mode with `irig_recovery=MISSING`,
    `lightweight=1`, `found_irig_usb_by_id=0`,
    `usb_enumeration_failures=YES`, and the same failed hub port. This validates
    the permanent loop path without repeated `system_profiler`.
  - Permanent autonomous LaunchAgent started:
    `local-analysis/autonomous-audio-qa/0.3.133-autonomous-lightweight-20260614-163052`,
    label `com.fer.opena8dj.autonomous-audio-qa`, pid `54136`,
    `wait_seconds=0`, `interval_seconds=10`, `recovery_wait_seconds=45`,
    `recovery_interval_cycles=6`.
  - First permanent cycle:
    `cycle-1/status-after-recovery.txt` reported
    `autonomous_audio_qa=RECOVERY_ATTEMPTED`,
    `before_physical_capture_status=MISSING`,
    `after_physical_capture_status=MISSING`,
    `after_reason=irig_missing_from_usb_tree`. Recovery result had
    `lightweight=1`, `software_recovery_limit=no_irig_usb_object_to_reset`,
    and `next_recovery_action=continue_autonomous_software_recovery_or_reboot_if_policy_allows`.
    The sampled snapshot confirms `system_profiler_skipped=lightweight`.
  - Live supervisor health:
    `local-analysis/audio-stack-guard/autonomous-lightweight-live-20260614-163235`
    PASS, driver `0.4%`, coreaudiod `10.8%` during/after recovery, idle CPU
    `79.76%`, Core Audio enumeration PASS.
  - Autonomy/reboot prerequisites rechecked: `autoLoginUser=fer`,
    passwordless `sudo -n true` PASS, `com.fer.opena8dj.codex-resume` loaded
    with last exit `0`, and `com.fer.opena8dj.audio-qa-startup` loaded with
    last exit `0`.
  - `candidate-status` integration:
    `local-analysis/candidate-status/with-autonomous-qa-20260614-163331`
    reported `candidate_status=NOT_READY`,
    `reason=irig_missing_from_usb_tree`,
    `autonomous_audio_qa_status=RUNNING`, pid `54136`,
    `autonomous_audio_qa_latest_cycle=6`,
    `autonomous_audio_qa_latest_capture_status=MISSING`,
    `latest_output_pair_smoke_status=PASS`,
    `latest_timecode_smoke_status=PASS`, and
    `latest_playback_cpu_status=PASS`.

### No-refactor wait mode while pending USB hardware reboot

- Decision: do not split/refactor the large HAL/USB files during the iRig wait.
  Continue with conservative measurement and operational tuning only.
- Initial long-running supervisor status:
  `local-analysis/candidate-status/no-refactor-watch-20260614-183019`
  reported `candidate_status=NOT_READY`,
  `reason=irig_missing_from_usb_tree`,
  Audio 8 DJ visible, iRig absent from USB, and
  `autonomous_audio_qa_status=RUNNING`.
- Reporting fix:
  `scripts/candidate-status` now maps an autonomous supervisor
  `RECOVERY_ATTEMPTED` latest-status file to
  `autonomous_audio_qa_latest_capture_status` and
  `autonomous_audio_qa_latest_reason` using the post-recovery fields. Validation
  run `local-analysis/candidate-status/no-refactor-watch-fixed-20260614-183045`
  correctly reported `autonomous_audio_qa_latest_capture_status=MISSING` and
  `autonomous_audio_qa_latest_reason=irig_missing_from_usb_tree`.
- Quality-gate selftests while iRig is missing:
  - `capture_device_diagnose_selftest=PASS`,
    `local-analysis/capture-device-diagnose-selftest/20260614-183045`.
  - `physical_music_quality_gate_selftest=PASS` with
    `metallic_coloration_score_db=13.350449`,
    `bass_heavy_coloration_score_db=12.005268`.
- Supervisor interference finding:
  - With the autonomous supervisor polling/recovering too often, playback CPU
    gate failed:
    `local-analysis/playback-cpu-gate-20260614-183045`,
    `coreaudiod CPU p95 15.10% > 8.00%`.
  - After reducing recovery frequency but still polling every 10 seconds,
    playback CPU gate still failed:
    `local-analysis/playback-cpu-gate-20260614-183324`,
    `coreaudiod CPU p95 10.00% > 8.00%`,
    stress p95 `9.40% > 8.00%`.
- Operational optimization:
  - Changed autonomous supervisor defaults to poll every `30s`, recover for
    `20s`, and attempt recovery every `10` cycles. This keeps the approximate
    recovery cadence at five minutes while reducing Core Audio enumeration
    pressure during quality gates.
  - Restarted active LaunchAgent with candidate
    `0.3.133-autonomous-lightweight-slowpoll`,
    pid `34301`,
    run dir
    `local-analysis/autonomous-audio-qa/0.3.133-autonomous-lightweight-slowpoll-20260614-183511`.
  - Cooldown stack guard
    `local-analysis/audio-stack-guard/slowpoll-cooldown-20260614-183556`
    PASS, driver `0.0%`, coreaudiod `0.0%`, idle CPU `94.48%`.
- Playback CPU gate after slow polling:
  `local-analysis/playback-cpu-gate-20260614-183604` PASS:
  driver avg `6.08%`, driver p95 `6.90%`, coreaudiod p95 `3.20%`,
  stress coreaudiod p95 `1.60%`, timeline resets `0`, active underruns `0`.
- Consolidated status:
  `local-analysis/candidate-status/slowpoll-playback-pass-20260614-183710`
  reported `candidate_status=NOT_READY` only because iRig is still missing from
  USB; latest output-pair, timecode, and playback CPU gates are PASS.

### Shared hardware lock for parallel Rust redesign worktree

- Context: a second agent is redesigning/reimplementing OpenA8DJ in Rust in a
  different worktree on the same laptop, using the same Audio 8 DJ, iRig Stream,
  Core Audio stack, USB bus, and CPU budget.
- Change:
  - `scripts/audio-gate-lock.sh` default lock root moved from a worktree-local
    `local-analysis/.audio-gate.lock` to the user-global
    `$HOME/.opena8dj/hardware-gate.lock`.
  - `scripts/playback-cpu-gate` now uses the same user-global default.
  - `scripts/autonomous-audio-qa-supervisor` now acquires the shared lock per
    cycle. If another agent owns it, the supervisor writes `SKIPPED_BUSY` and
    skips the cycle instead of touching Core Audio/USB.
  - `scripts/start-autonomous-audio-qa` sets `AUDIO_GATE_LOCK_ROOT` explicitly
    in the LaunchAgent environment.
  - Added `scripts/shared-hardware-lock-status` and Make target
    `shared-hardware-lock-status`.
  - Added protocol and product-manager prompt:
    `docs/SHARED_HARDWARE_COORDINATION.md`.
- Validation:
  - `bash -n` PASS for lock/supervisor/start/status scripts.
  - `python3 -m py_compile scripts/playback-cpu-gate` PASS.
  - `make -n shared-hardware-lock-status autonomous-audio-qa-start
    playback-cpu-gate` expands correctly.
  - Active supervisor restarted as `0.3.133-shared-lock`, pid `11627`,
    run dir
    `local-analysis/autonomous-audio-qa/0.3.133-shared-lock-20260614-191707`.
  - During cycle 1, `scripts/shared-hardware-lock-status` reported BUSY with
    owner `autonomous-audio-qa:0.3.133-shared-lock`, then FREE after the cycle.
  - External-agent smoke held the global lock with owner `external-agent-smoke`.
    While held, autonomous QA reported `latest_autonomous_audio_qa=SKIPPED_BUSY`,
    `latest_reason=audio_gate_lock_busy`, cycle `4`.
  - After the external lock released, `shared_hardware_lock=FREE` and autonomous
    QA resumed normal polling at cycle `5`.

### 0.3.135 atomic write-stats and stricter no-iRig click-risk gate

- Build loaded:
  - Version: `0.3.135`.
  - Installed HAL hash:
    `0949969d396223257c8207b4454798b6e8a09593b1a6479e87ae2e6bbe24bb6a`.
  - Build family now defaults to the loaded candidate configuration:
    ISO64, capture queue `8`, playback queue `8`, output prefetch `64`,
    background preopen enabled, stop-isoc-on-stop enabled, stop grace `10s`,
    fast prefetch clear enabled, output amplitude stats disabled.
- Driver change:
  - `outputFramesWritten` is now counted with an atomic counter instead of
    taking `_streamStatsMutex` from the Core Audio output write path. This keeps
    the metric but removes one realtime-path mutex from every write callback.
- Safety and coordination changes:
  - `scripts/audio-gate-lock.sh` now exports inherited lock state through a
    shared helper and clears the export on release.
  - Compound gates now export inherited lock state before calling subgates.
  - State-changing scripts now acquire the shared hardware lock before moving
    HAL bundles, installing candidates, isolating iRig, or resetting/recovering
    audio services.
  - `make install-hal` now uses `scripts/test-hal-candidate-safety` instead of
    raw install/restart commands.
  - The autonomous QA supervisor now defers gates/recovery while VLC, Spotify,
    Traktor, or `audio-wav-play` are active, and caps automatic recoveries at
    three per hour before observation/backoff.
  - Post-reboot startup now runs `audio-list` with a timeout.
- Safety load:
  - `local-analysis/hal-safety-0.3.135-atomic-written-20260614-204347`
    PASS, `leave_loaded=1`.
- Internal quality window:
  - `local-analysis/quality-window/0.3.135-atomic-written-internal-20260614-204409`
    PASS.
  - Output pairs: PASS.
  - Timecode surface: PASS.
  - Playback CPU gate:
    `device_start_seconds=0.093835`,
    `first_callback_seconds=0.101382`,
    driver avg `5.86%`, driver p95 `6.50%`,
    coreaudiod p95 `1.70%`,
    stress driver p95 `6.00%`,
    stress coreaudiod p95 `1.50%`,
    WindowServer p95 `16.30%`,
    output frames written/read `2885632/2886071`,
    timeline resets `0`,
    active underruns `0`,
    elastic drops/replays `0/0`,
    late write frames/batches `0/0`,
    playback completion outliers `0`,
    capture-to-playback queue outliers `0`,
    zero-complete playback transactions `0`.
- New strict no-iRig click-risk gate:
  - `local-analysis/no-irig-click-risk/0.3.135-atomic-written-20260614-204553`
    PASS, three consecutive real-music passes.
  - Run 1: driver p95 `6.60%`, coreaudiod p95 `1.70%`,
    stress driver p95 `5.80%`, stress coreaudiod p95 `1.50%`,
    late writes `0`, elastic drops `0`, completion outliers `0`.
  - Run 2: driver p95 `6.50%`, coreaudiod p95 `1.70%`,
    stress driver p95 `5.70%`, stress coreaudiod p95 `1.50%`,
    late writes `0`, elastic drops `0`, completion outliers `0`.
  - Run 3: driver p95 `6.40%`, coreaudiod p95 `1.70%`,
    stress driver p95 `5.70%`, stress coreaudiod p95 `1.50%`,
    late writes `0`, elastic drops `0`, completion outliers `0`.
- Candidate physical gate:
  - `local-analysis/quality-window/0.3.135-atomic-written-candidate-20260614-204913`
    `BLOCKED_PHYSICAL_CAPTURE`.
  - Reason: `irig_missing_from_usb_tree`.
  - Candidate-preflight internal subgates all passed before the physical bench
    block: output pair PASS, timecode PASS, playback CPU PASS.
- Consolidated candidate status:
  - `local-analysis/candidate-status/0.3.135-after-candidate-window-20260614-205058`
    reports `candidate_status=NOT_READY`, `reason=irig_missing_from_usb_tree`,
    Audio 8 DJ present in Core Audio, iRig absent from USB/Core Audio, and
    USB enumeration failures on `AppleUSB20HubPort` at location `1114112`.
- Conclusion:
  - 0.3.135 is a stronger internal candidate than 0.3.134 and is currently
    loaded, but it is not approved for human listening or release readiness
    because the physical iRig gate cannot run while iRig is absent from macOS USB.

### 0.3.135 autonomous supervisor relaunch

- Initial relaunch exposed a supervisor bug: bash `set -u` treated an empty
  recovery-history array as unbound and exited after cycle 1. Fixed by storing
  recovery timestamps in `recovery-history.tsv` instead of a shell array.
- Validation:
  - `scripts/autonomous-audio-qa-supervisor --run-once --passive-recovery`
    completed as expected with `autonomous_audio_qa=INCOMPLETE`,
    `reason=run_once_complete`, and
    `physical_capture_reason=irig_missing_from_usb_tree`.
  - Relaunched persistent supervisor for `0.3.135-atomic-written`, pid `32459`,
    run dir
    `local-analysis/autonomous-audio-qa/0.3.135-atomic-written-20260614-205418`.
  - Cycle 1 completed and released the shared lock:
    `autonomous_audio_qa=RECOVERY_ATTEMPTED`,
    `before_physical_capture_status=MISSING`,
    `after_physical_capture_status=MISSING`,
    `recoveries_last_hour=1`.
  - Recovery result:
    `irig_recovery=MISSING`,
    `found_irig_usb_by_id=0`,
    `found_irig_core_audio=0`,
    `usb_enumeration_failures=YES`,
    `software_recovery_limit=no_irig_usb_object_to_reset`,
    `next_recovery_action=observe_with_backoff_until_manual_usb_power_cycle_or_irig_reenumerates`.
  - Post-cycle lock status: `shared_hardware_lock=FREE`.
  - Post-cycle audio stack health: PASS, watched CPU total `0.0%`, driver
    `0.0%`, coreaudiod `0.0%`.
  - Final candidate-status after adding nested supervisor recovery discovery:
    `local-analysis/candidate-status/0.3.135-final-status-fresh-irig-recovery-20260614-205640`
    reports `autonomous_audio_qa_status=RUNNING`, pid `32459`,
    `latest_irig_usb_recovery_result` from the 0.3.135 supervisor run, and
    `latest_irig_usb_recovery_next_action=observe_with_backoff_until_manual_usb_power_cycle_or_irig_reenumerates`.

### Digital pre-physical gate

- Change: added `scripts/digital-audio-quality-gate` and Make target
  `digital-audio-quality-gate`.
- Purpose: create a mandatory software-only gate before any physical iRig
  capture gate. This is not a replacement for iRig; it blocks candidates that
  already fail digital packing, residual, click-risk, or CPU/jitter checks
  before consuming a physical measurement window.
- Gate contents:
  - `run-simulated-output-soundcheck` for pairs A/B/C/D.
  - `no-irig-click-risk-gate` as a subgate, default one run in
    `candidate-preflight`, configurable for longer standalone runs.
- Integration:
  - `scripts/candidate-preflight` now runs `digital-audio-quality-gate` after
    output-pair, timecode, and playback CPU gates, and before
    `capture-device-diagnose` / `physical-bench-sanity-gate`.
  - `scripts/candidate-status` now reports both latest standalone and nested
    digital gate results.
  - Core Audio checks now require the exact Obj-C driver UID
    `org.opena8dj.Audio8DJ`; a generic `Open Audio 8 DJ` name match is rejected
    so the Rust driver cannot accidentally satisfy an Obj-C candidate gate.
- Policy: if the digital gate fails, physical gates are not attempted. If the
  digital gate passes, physical iRig gates are still mandatory before human
  listening or release readiness.
