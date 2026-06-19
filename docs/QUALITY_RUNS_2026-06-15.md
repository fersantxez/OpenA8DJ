# Quality Runs - 2026-06-15

## No-iRig optimization pass

Context:

- iRig Stream is unavailable at USB/Core Audio level, so this pass used only
  software/digital gates and real playback through Open Audio 8 DJ.
- Physical iRig capture remains mandatory before release or human listening.
- Starting safe baseline: `0.3.135`, installed HAL hash
  `0949969d396223257c8207b4454798b6e8a09593b1a6479e87ae2e6bbe24bb6a`.
- Baseline backup saved at
  `local-analysis/hal-backups/pre-no-irig-opt-20260615-001813/OpenA8DJ.driver`.

### 0.3.135 restored baseline

- Gate:
  `local-analysis/digital-audio-quality-gate/0.3.135-no-irig-baseline-20260615-001316-20260615-001316`.
- Result: `digital_audio_quality_gate=PASS`.
- Simulated output A/B/C/D:
  - alignment `1.000000`;
  - simulated SNR `75.22 dB`;
  - mid-band residual ratio `0.000669`;
  - mid-band residual `-108.83 dBFS`;
  - window click outliers `0`.
- Two repeated no-iRig stress runs:
  - Run 1: driver p95 `6.70%`, coreaudiod p95 `1.60%`,
    stress driver p95 `5.80%`, stress coreaudiod p95 `1.50%`,
    elastic drops `0`, late write frames `0`, playback completion outliers `0`.
  - Run 2: driver p95 `6.60%`, coreaudiod p95 `1.70%`,
    stress driver p95 `5.80%`, stress coreaudiod p95 `1.50%`,
    elastic drops `0`, late write frames `0`, playback completion outliers `0`.
- Decision: keep as known-good no-iRig baseline.

### 0.3.136 - ISO64 with hot stream stats interval 8

- Change: `HAL_HOT_STREAM_STATS_INTERVAL=8`, otherwise same ISO64/q8 baseline.
- Safety load:
  `local-analysis/hal-safety-0.3.136-hotstats8-iso64-noirig-20260615-001831`,
  PASS and left loaded for measurement.
- Gate:
  `local-analysis/digital-audio-quality-gate/0.3.136-hotstats8-iso64-noirig-20260615-001903-20260615-001903`.
- Result: `digital_audio_quality_gate=PASS`.
- Simulated output A/B/C/D remained identical to baseline.
- Two repeated no-iRig stress runs:
  - Run 1: driver p95 `6.70%`, coreaudiod p95 `1.60%`,
    stress driver p95 `5.80%`, stress coreaudiod p95 `1.50%`,
    elastic drops `0`, late write frames `0`, playback completion outliers `0`.
  - Run 2: driver p95 `6.60%`, coreaudiod p95 `1.70%`,
    stress driver p95 `5.70%`, stress coreaudiod p95 `1.50%`,
    elastic drops `0`, late write frames `0`, playback completion outliers `0`.
- Decision: rejected as an optimization. It is safe, but not materially better
  than 0.3.135.

### 0.3.137 - ISO64 with hot stream stats interval 32

- Change: `HAL_HOT_STREAM_STATS_INTERVAL=32`, otherwise same ISO64/q8 baseline.
- Safety load:
  `local-analysis/hal-safety-0.3.137-hotstats32-iso64-noirig-20260615-002307`,
  PASS and left loaded for measurement.
- Gate:
  `local-analysis/digital-audio-quality-gate/0.3.137-hotstats32-iso64-noirig-20260615-002341-20260615-002341`.
- Result: `digital_audio_quality_gate=PASS`.
- Simulated output A/B/C/D remained identical to baseline.
- Two repeated no-iRig stress runs:
  - Run 1: driver p95 `6.60%`, coreaudiod p95 `1.60%`,
    stress driver p95 `6.00%`, stress coreaudiod p95 `1.50%`,
    elastic drops `0`, late write frames `0`, playback completion outliers `0`.
  - Run 2: driver p95 `6.60%`, coreaudiod p95 `1.70%`,
    stress driver p95 `5.70%`, stress coreaudiod p95 `1.50%`,
    elastic drops `0`, late write frames `0`, playback completion outliers `0`.
- Decision: rejected as an optimization. It passes, but run 1 stress driver p95
  regressed from `5.80%` baseline to `6.00%`.

### 0.3.138 - atomic hot stream counts experiment

- Change: attempted to move hot USB completion stream counters from
  `_streamStatsMutex` to atomics, preserving the public `stream-stats` fields.
- Compile: first compile exposed timing-variable warnings with cadence
  diagnostics disabled; warnings were fixed before load.
- Safety load:
  `local-analysis/hal-safety-0.3.138-atomic-hot-stream-counts-noirig-20260615-002944`.
- Result: rejected by safety gate before audio quality testing.
- Failure:
  - `audio_stack_health=FAIL`;
  - `coreaudiod=74.5%`;
  - `mediaremoted=50.4%`;
  - total watched CPU `125.1%`.
- Recovery: safety gate unloaded OpenA8DJ and restored a healthy audio stack.
- Follow-up: 0.3.135 backup was reloaded with
  `local-analysis/restore-0.3.135-after-rejected-0.3.138-20260615-003112`,
  PASS and left loaded.
- Code decision: reverted the 0.3.138 source changes. Do not promote this path
  without a deeper redesign and a new safety hypothesis.

### Restored 0.3.135 verification

- Gate:
  `local-analysis/digital-audio-quality-gate/0.3.135-restored-after-noirig-iteration-20260615-003328-20260615-003328`.
- Result: `digital_audio_quality_gate=PASS`.
- Simulated output A/B/C/D:
  - alignment `1.000000`;
  - simulated SNR `75.22 dB`;
  - mid-band residual ratio `0.000669`;
  - mid-band residual `-108.83 dBFS`;
  - window click outliers `0`.
- One no-iRig stress run:
  - driver p95 `6.80%`;
  - coreaudiod p95 `1.80%`;
  - stress driver p95 `5.80%`;
  - stress coreaudiod p95 `1.50%`;
  - elastic drops `0`;
  - late write frames `0`;
  - playback completion outliers `0`.
- Final no-iRig state: 0.3.135 is loaded and remains the best measured
  software-only candidate from this pass.
