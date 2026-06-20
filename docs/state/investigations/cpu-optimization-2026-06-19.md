# CPU Optimization Investigation - 2026-06-19

This note records the first CPU optimization investigation pass for the
canonical macOS C++ line. It is intentionally conservative: no CPU improvement
is claimed until an exact artifact beats mainline in a lock-gated, same-session
physical A/B run.

## Scope

- Worktree: `/private/tmp/opena8dj-main-merge`
- Branch: `codex/cpu-optimization-investigation`
- Base: `main`
- No driver install.
- No package execution.
- No USB, Audio 8 DJ hardware, Traktor, Audio MIDI Setup, CoreAudio restart, or
  default audio-device changes.

## Executive Finding

CPU pool is now the frozen 0.5.0 stable build profile.

The original investigation started conservatively and did not claim product
readiness from offline evidence alone. After that, the exact CPU pool artifact
was installed under an explicitly authorized hardware window, validated through
iRig Stream with real music, left loaded, and then accepted by human listening.

The frozen 0.5.0 stable profile enables:

- `HAL_TRANSFER_POOL_CURSOR=1`
- `HAL_FAST_ISO_TRANSFER_CONFIG=1`

The completion-handler experiments remain default-off and are not part of the
stable freeze:

- `HAL_REUSE_ISOC_COMPLETIONS=0`
- `HAL_RAW_ISOC_COMPLETIONS=0`

## Implementation Update

An opt-in CPU pool candidate has now been implemented as a build-only artifact:

```sh
make hal-cpu-pool-candidate
```

That target builds `build/OpenA8DJ-cpu-pool.driver`, writes
`build/hal-candidates/cpu-pool-candidate.json`, runs HAL smoke/parity checks on
the temporary candidate bundle, restores the default `build/OpenA8DJ.driver`,
and runs HAL smoke/parity again on the restored default bundle.

The candidate enables:

- `HAL_TRANSFER_POOL_CURSOR=1`
- `HAL_FAST_ISO_TRANSFER_CONFIG=1`

The fast ISO transfer configuration path now stores a layout signature on each
pooled transfer. On stable layouts, it can skip the old per-transaction layout
verification pass and reset only the mutable transaction completion fields.

The implementation also fixes the opt-in Makefile wiring for completion-handler
experiments by passing `OPENA8DJ_REUSE_ISOC_COMPLETION_HANDLERS` and
`OPENA8DJ_RAW_ISOC_COMPLETION_HANDLERS` to the source. Those flags remain
default-off and are not part of the recommended CPU pool candidate.

Implementation verification:

- `make hal-cpu-pool-candidate`: PASS.
- Candidate HAL smoke/parity: PASS.
- Restored default HAL smoke/parity: PASS.
- Candidate HAL hash:
  `79390010acbd96b799d3f69d9f1ae92ccaec68e37439ae4a54a2ab91ea091098`.
- Restored default HAL hash:
  `e71c4f6f18c4cd8f611c4290a2367ad25f7f3cba7c57e46948db6ac453e3beae`.
- `ctest --test-dir build/cmake-release --output-on-failure`: 89/89 passed.
- `scripts/audio-stack-health`: PASS after the build-only work.
- Hardware lock: absent/free after the build-only work.

## iRig Physical Soundcheck Update

Additional iRig sound checks were run after the build-only CPU candidate work.
These checks exercised the currently installed and loaded HAL, not the CPU pool
candidate, because the CPU pool candidate was not installed or loaded.

Installed HAL under test:

- Path: `/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver`
- Version: `0.5.0`
- Build: `150`
- Executable SHA256:
  `8b9c837d30dcf8b214edf1cd9944b0f34b97ff6fee1589823c8fce64d4ffb230`

iRig evidence:

- `local-analysis/soundcheck/20260619T223643-irig-cpu-pool-audio-check`
  - Pair B, iRig Stream channels 1,2, random selected source.
  - Result: FAIL.
  - `quality_alignment_score=0.636251`, `analog_snr_db=-4.05`,
    `capture_clipped_frames=0`.
- `local-analysis/channel-matrix/20260619T223820-irig-pairB-matrix`
  - Pair B decorrelated tone matrix through iRig Stream channels 1,2.
  - Result: tone matrix PASS.
  - `left_to_right_leakage_db=-74.37`, `right_to_left_leakage_db=-62.52`,
    `capture_clipped_frames=0`.
  - Interpretation: the physical B-to-iRig route is live and channel-separated,
    but the linear residual diagnostic still warns about unmodelled physical
    residual.
- `local-analysis/soundcheck/20260619T223900-irig-cable-guy-pairB-current-hal`
  - Pair B, iRig Stream channels 1,2, known comparable source
    `Cable Guy - Dj Deep (Original Mix).mp3`.
  - Result: PASS against calibrated iRig thresholds.
  - `quality_alignment_score=0.949537`, `analog_snr_db=8.61`,
    `mid_band_1000_5000_residual_ratio=1.502375`,
    `high_band_5000_12000_residual_ratio=1.404521`,
    `quiet_mid_band_noise_dbfs=-39.89`, `mid_band_cpu_corr=0.594586`,
    `click_outliers=194`, `lag_jumps_gt_2_frames=20`,
    `capture_clipped_frames=0`.

Post-run state:

- `scripts/audio-stack-health`: PASS.
- Hardware lock: absent/free.
- No driver install, package execution, USB reset, CoreAudio restart, Traktor
  launch, or default-device change was performed.

This does not validate the CPU pool candidate's sound quality. To validate that
candidate, the exact `build/OpenA8DJ-cpu-pool.driver` artifact must be installed
or loaded under an explicitly authorized hardware A/B window, then compared
against the installed baseline using the same iRig route.

## CPU Pool Candidate Install + iRig Validation

The CPU pool candidate was later installed under an explicitly authorized
hardware window and tested through the same iRig pair B route.

Evidence directory:

`local-analysis/physical-cpu-candidate-ab/20260620T120032-cpu-pool-install-irig`

Install details:

- Baseline installed HAL executable SHA256 before replacement:
  `8b9c837d30dcf8b214edf1cd9944b0f34b97ff6fee1589823c8fce64d4ffb230`.
- Build-only CPU candidate executable SHA256 before signing:
  `79390010acbd96b799d3f69d9f1ae92ccaec68e37439ae4a54a2ab91ea091098`.
- Staged signed candidate executable SHA256:
  `c6e4d491e35e73d90109cab33c71a616173d002fbc6fa2519c241512eb85c951`.
- Installed candidate executable SHA256:
  `c6e4d491e35e73d90109cab33c71a616173d002fbc6fa2519c241512eb85c951`.
- CoreAudio was restarted after HAL replacement.
- The candidate was left installed after the passing soundcheck.

iRig candidate soundcheck:

- Run directory:
  `local-analysis/physical-cpu-candidate-ab/20260620T120032-cpu-pool-install-irig/soundcheck-candidate`
- Pair B, iRig Stream channels 1,2, known comparable source
  `Cable Guy - Dj Deep (Original Mix).mp3`.
- Result: PASS against calibrated iRig thresholds.
- `quality_alignment_score=0.949423`, `analog_snr_db=8.78`,
  `mid_band_1000_5000_residual_ratio=1.522984`,
  `high_band_5000_12000_residual_ratio=1.408289`,
  `quiet_mid_band_noise_dbfs=-39.86`, `mid_band_cpu_corr=0.387509`,
  `click_outliers=184`, `lag_jumps_gt_2_frames=20`,
  `capture_clipped_frames=0`.

CPU profile comparison against the immediately prior installed-HAL iRig run:

- Prior installed HAL, same source/route:
  - `opena8dj_driver` average CPU: `6.522%`
  - `opena8dj_driver` max CPU: `7.500%`
  - `coreaudiod` average CPU: `2.996%`
  - `coreaudiod` max CPU: `9.300%`
- CPU pool candidate:
  - `opena8dj_driver` average CPU: `5.209%`
  - `opena8dj_driver` max CPU: `6.200%`
  - `coreaudiod` average CPU: `2.587%`
  - `coreaudiod` max CPU: `8.800%`

Post-run state:

- `scripts/audio-stack-health`: PASS.
- Hardware lock: absent/free.
- Active installed HAL hash: `c6e4d491e35e73d90109cab33c71a616173d002fbc6fa2519c241512eb85c951`.
- No package execution, USB reset, system reboot, Traktor launch, or
  default-device change was performed.

This validated the installed CPU pool build for a human listening window on
this machine and route.

## 0.5.0 Stable Freeze

On 2026-06-20, the loaded CPU pool build was accepted by human listening and
frozen as OpenA8DJ 0.5.0 stable.

Stable freeze reference:

```text
release=OpenA8DJ 0.5.0
stable_profile=cpu-pool
installed_hal_sha256=c6e4d491e35e73d90109cab33c71a616173d002fbc6fa2519c241512eb85c951
unsigned_build_hal_sha256=79390010acbd96b799d3f69d9f1ae92ccaec68e37439ae4a54a2ab91ea091098
validated_route=Open Audio 8 DJ pair B -> iRig Stream channels 1,2
validated_source=Cable Guy - Dj Deep (Original Mix).mp3
human_listening_result=PASS
```

Second same-artifact iRig pass before user listening:

- Run directory:
  `local-analysis/physical-cpu-candidate-ab/20260620T120432-cpu-pool-repeat-irig/soundcheck-candidate-repeat`
- Result: PASS against calibrated iRig thresholds.
- `quality_alignment_score=0.948151`, `analog_snr_db=8.72`,
  `mid_band_1000_5000_residual_ratio=1.512976`,
  `high_band_5000_12000_residual_ratio=1.405052`,
  `quiet_mid_band_noise_dbfs=-39.90`, `mid_band_cpu_corr=0.253938`,
  `click_outliers=178`, `lag_jumps_gt_2_frames=22`,
  `capture_clipped_frames=0`.
- CPU profile:
  - `opena8dj_driver` average CPU: `5.470%`
  - `opena8dj_driver` max CPU: `6.300%`
  - `coreaudiod` average CPU: `2.674%`
  - `coreaudiod` max CPU: `8.700%`

Post-run state:

- `scripts/audio-stack-health`: PASS.
- Hardware lock: absent/free.
- `Open Audio 8 DJ` visible as 8 inputs / 8 outputs.

The default Makefile HAL profile now builds this stable CPU pool profile.

## Evidence Run

Targeted offline CMake checks were run for the CPU/transport surface:

```sh
ctest --test-dir build/cmake-release -R 'opena8djcpp_(transport_budget_model|hot_path_timing_analysis|playback_scheduler_contract|playback_scheduler_runtime_contract|usb_submit_plan_contract|usb_submit_payload_contract|prepared_usb_runtime_submit_contract|prepared_usb_async_runtime_contract|hal_prepared_submit_adapter_contract|hal_prepared_runtime_source_contract|hal_prepared_runtime_binding_contract|prepared_transport_migration_gate|hal_transport_runtime_gate)' --output-on-failure
```

Result: 13/13 passed.

Key tool results:

- `opena8djcpp_transport_budget_model`: PASS as an offline diagnostic, with
  `product_candidate_exists=false`. It models an 8x submit reduction for some
  designs, but blocks product claims because same-session physical CPU A/B
  evidence is missing.
- `opena8djcpp_hot_path_timing_analysis`: PASS as diagnostic-only evidence.
  The selected stored run reports `dominant_subsegment="capture_requeue"` and
  `attribution="fixed_queue_requeue_enqueue_dominant"`.
- `opena8djcpp_hal_transport_runtime_gate`: PASS because the guard blocks HAL
  superiority claims and keeps rejected physical variants out of the stable
  default load. It explicitly reports
  `playback_scheduler_physically_rejected=true`,
  `prepared_lite_physically_rejected=true`, and
  `next_cpu_direction="DESIGN_NEW_TRANSPORT_REDUCING_IOUSBHOST_ENQUEUE_OR_DRIVERKIT_USB_RUNTIME"`.

## Hot-Path Attribution

The hot-path analyzer selected this stored evidence file:

`/Users/fer/dev/audio8djcpp/local-analysis/physical-evidence-window/20260618T211000Z-hotpath-diagnostic-candidate-only/cpp-soundcheck/stream-stats-summary.json`

Average timing values from that stored run:

- `capture_handler`: 3032.716386 ticks
- `capture_decode`: 5.878472 ticks
- `capture_requeue`: 2032.634774 ticks
- `capture_enqueue`: 1932.840831 ticks
- `playback_queue`: 1705.731010 ticks
- `playback_fill`: 375.834843 ticks
- `playback_enqueue`: 1268.105923 ticks
- `fixed_queue_to_playback_fill_ratio`: 18.463729

The nested timing policy says these values must not be summed as total CPU, but
they are useful for attribution. The evidence points at queue/requeue/enqueue
work as the CPU area worth investigating first.

## Source Observations

The current HAL hot path still contains several CPU-risk surfaces:

- `src/hal/OpenA8DJUSB.m`: `checkoutTransferFromPool` uses a shared transfer
  pool mutex and scans the Objective-C pool. `OPENA8DJ_TRANSFER_POOL_CURSOR`
  can reduce repeated start-at-zero scanning.
- `src/hal/OpenA8DJUSB.m`: capture and playback queue paths rebuild request
  arrays and pass through IOUSBHost enqueue calls for every submitted transfer.
- `src/hal/OpenA8DJUSB.m`: playback fill is visible in timing evidence, but it
  is not the dominant cost compared with queue/requeue/enqueue.
- `src/hal/OpenA8DJUSB.m`: optional ring/transport experiments exist, but prior
  evidence requires caution around latency, DVS/timecode integrity, and
  CoreAudio CPU behavior.

## Build-Only Candidate Sweep

The following build-only HAL variants were compiled and checked with
`make smoke-hal parity-smoke-hal`. These were not installed and were not
physically tested.

Evidence directory:

`local-analysis/cpu-optimization/20260619-215657-offline-candidates`

Results:

| Variant | Flags | Result | HAL hash |
| --- | --- | --- | --- |
| `baseline` | default | PASS | `368afebbacc09c3f8a39d1a9c609b07525942cbe42336834d2f7a8d2ee3a6c9a` |
| `pool-cursor` | `HAL_TRANSFER_POOL_CURSOR=1` | PASS | `c2eb3f0755faa4c5dfef1431934261e5b8396fc76c67e817e6346198ba9e7ab1` |
| `fast-iso-transfer-config` | `HAL_FAST_ISO_TRANSFER_CONFIG=1` | PASS | `48c8870807e539d617b3ecf5c2bf3323b5b3cf5e9dec5535ae6825d8b1182f4f` |
| `reuse-completion-handlers` | `HAL_REUSE_ISOC_COMPLETIONS=1` | PASS | `368afebbacc09c3f8a39d1a9c609b07525942cbe42336834d2f7a8d2ee3a6c9a` |
| `pool-fast-reuse` | `HAL_TRANSFER_POOL_CURSOR=1 HAL_FAST_ISO_TRANSFER_CONFIG=1 HAL_REUSE_ISOC_COMPLETIONS=1` | PASS | `151733810349e3de215da5fb246ae458ec86e5f8e02b370f42c97cb59a0b1dbf` |
| `pool-fast-reuse-raw` | `HAL_TRANSFER_POOL_CURSOR=1 HAL_FAST_ISO_TRANSFER_CONFIG=1 HAL_REUSE_ISOC_COMPLETIONS=1 HAL_RAW_ISOC_COMPLETIONS=1` | PASS | `151733810349e3de215da5fb246ae458ec86e5f8e02b370f42c97cb59a0b1dbf` |

After the sweep, the default HAL build was restored and `smoke-hal` plus
`parity-smoke-hal` passed again.

The original sweep also showed that the completion-handler flags did not affect
the binary hash. That was traced to Makefile macro names that did not match the
source names; the wiring has been corrected, but the completion-handler variants
remain excluded from the first recommended CPU candidate because they have a
higher lifetime risk and no physical CPU/sound-quality win.

## Candidate Ranking

1. Measurement-first physical window: instrument the exact stable mainline load
   and one opt-in CPU candidate, then run source-reference same-session A/B with
   CPU, submit counters, and sound-quality evidence.
2. Low-risk micro-candidate for that window:
   `HAL_TRANSFER_POOL_CURSOR=1 HAL_FAST_ISO_TRANSFER_CONFIG=1`. This may reduce
   pool scanning and transaction reconfiguration overhead, but it is expected to
   be small and has no proven sound-quality or CPU win yet.
3. Larger structural candidate: replace Objective-C/mutex-heavy transfer pool
   and ring work in the hot path with preallocated C slots and non-destructive
   single-producer/single-consumer structures. This needs offline contracts
   before any hardware run.
4. Long-term serious direction: new transport or DriverKit USB runtime that
   reduces IOUSBHost enqueue work.

## Do Not Repeat Without New Evidence

- Do not promote playback scheduler or prepared-lite variants. The current gate
  reports them as physically rejected.
- Do not retry USB-clock/GetZeroTimeStamp timing experiments as a CPU shortcut.
- Do not test raw completion handlers early. They compiled in the sweep, but
  they are higher-risk and did not produce a distinct combined binary hash.
- Do not use destructive input trimming as a CPU optimization. DVS/timecode
  integrity has priority over lower counters.

## Next Required Verification

Before any CPU candidate can be called better:

- Confirm the hardware lock is available and log hardware use first.
- Build the exact baseline and candidate artifacts and record hashes.
- Install only inside an explicitly authorized hardware A/B window.
- Capture CPU, submit counters, queue failures, and stream stats for both
  baseline and candidate in the same session.
- Run source-reference sound-quality comparison on the exact artifacts.
- Reject the candidate if CPU improves but sound quality, DVS/timecode behavior,
  or CoreAudio stability regresses.

Until that happens, every CPU candidate remains experimental and not ready for
users.
