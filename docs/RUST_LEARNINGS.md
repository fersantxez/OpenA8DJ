# Rust Learnings For The C++ Tree

Extraction date: 2026-06-16.

Scope: this document was written from read-only inspection of
`/Users/fer/dev/audio8djrust`. No hardware, Core Audio, USB, audio playback,
audio recording, installs, reloads, resets, tests, or formatters were executed.

## Source Map

Primary Rust references:

- `/Users/fer/dev/audio8djrust/docs/RUST_PM_SUCCESS_METRICS.md`
- `/Users/fer/dev/audio8djrust/docs/RUST_IMPLEMENTATION_STATUS.md`
- `/Users/fer/dev/audio8djrust/docs/RUST_NO_IRIG_AUDIO_QUALITY.md`
- `/Users/fer/dev/audio8djrust/docs/RUST_DRIVER_ARCHITECTURE.md`
- `/Users/fer/dev/audio8djrust/docs/RUST_PRODUCT_PLAN.md`
- `/Users/fer/dev/audio8djrust/docs/MAINLINE_FINDINGS_2026_06_14.md`
- `/Users/fer/dev/audio8djrust/scripts/rust-no-irig-software-gate`
- `/Users/fer/dev/audio8djrust/scripts/run-simulated-output-soundcheck`
- `/Users/fer/dev/audio8djrust/crates/open-a8dj-core/src/{sample,mode2,input,routing,timecode,metrics,topology}.rs`
- `/Users/fer/dev/audio8djrust/crates/open-a8dj-tools/src/bin/opena8dj-rust-{pack-sim,pack-bench,timecode-analyze,dvs-matrix-smoke}.rs`
- `/Users/fer/dev/audio8djrust/crates/open-a8dj-ffi/include/open_a8dj_rust.h`

## Product Contract To Preserve

The Rust PM contract says the Rust redesign must meet or exceed mainline on
physical audio quality, full Audio 8 DJ I/O and routing, Traktor/timecode vinyl
and CD-line behavior, low CPU/resource use, stability, and deterministic tests
(`/docs/RUST_PM_SUCCESS_METRICS.md:15-29`). Clean counters are explicitly
necessary but not sufficient; physical output/capture, DVS behavior, and human
listening remain the product authority (`RUST_PM_SUCCESS_METRICS.md:28-29`).

The current mainline internal reference is `0.3.135`. Its internal targets are:
output-pair smoke PASS, timecode smoke PASS, playback CPU/UI stress PASS, strict
no-iRig click-risk gate PASS across three real-music runs, device start about
`0.094s`, first callback about `0.101s`, driver p95 about `6.5%`, `coreaudiod`
p95 about `1.7%`, and zero timeline resets, underruns, elastic drops/replays,
late writes, queue outliers, and playback completion outliers
(`RUST_PM_SUCCESS_METRICS.md:31-57`). It is still not an audiophile baseline
while physical capture is blocked (`RUST_PM_SUCCESS_METRICS.md:53-57`).

Useful mainline settings to carry into C++ policy comparisons:

```text
HAL_ISO_FRAMES=64
HAL_CAPTURE_QUEUE=8
HAL_PLAYBACK_QUEUE=8
HAL_OUTPUT_PREFETCH_FRAMES=64
HAL_BACKGROUND_PREOPEN_ON_INIT=1
HAL_STOP_ISOC_ON_STOP=1
HAL_STOP_GRACE_USEC=10000000
HAL_FAST_OUTPUT_PREFETCH_CLEAR=1
HAL_OUTPUT_AMPLITUDE_STATS=0
atomic outputFramesWritten counter
```

Source: `RUST_PM_SUCCESS_METRICS.md:59-72`.

Status strings should stay precise, not collapsed into generic failure:
`PASS`, `FAIL`, `NOT_READY`, `BLOCKED_LOCK_BUSY`,
`BLOCKED_PHYSICAL_CAPTURE`, `BLOCKED_DIRTY_ROUTE`,
`BLOCKED_USB_ENUMERATION`, `BLOCKED_IRIG_UNSTABLE`,
`BLOCKED_UNVALIDATED_DVS`, `BLOCKED_STALE_HASH`, `SKIPPED_BUSY`
(`RUST_PM_SUCCESS_METRICS.md:171-192`; encoded in
`crates/open-a8dj-core/src/metrics.rs:1-31`).

## Current Rust Evidence And What It Does Not Prove

Current Rust candidate recorded in the PM doc: commit `3429796`
(`RUST_PM_SUCCESS_METRICS.md:78-86`). It passed three locked real-music playback
runs through `Open Audio 8 DJ-rust` with CPU stress and `profile playback`
forcing `input-decode: off` (`RUST_PM_SUCCESS_METRICS.md:88-99`).

The accepted no-iRig software gate is:

```text
/Users/fer/dev/audio8djrust/local-analysis-rust/software-runs/rust-no-irig-software-gate-20260615T205923Z
```

It passed formatting, tests, clippy, Rust HAL smoke/parity, Rust/C packet parity,
tool build, packer throughput, synthetic timecode/input analysis, offline DVS
matrix smoke at `44.1 kHz` and `48 kHz`, a 72-row pack-sim matrix, and simulated
output soundcheck for pairs A/B/C/D (`RUST_PM_SUCCESS_METRICS.md:101-145`).

The product reading is important for C++: Rust has a credible no-iRig internal
playback/stress candidate and a reproducible software-only quality gate, but it
is still `BLOCKED_PHYSICAL_CAPTURE` for audiophile release because iRig physical
capture and human listening have not run (`RUST_PM_SUCCESS_METRICS.md:147-159`;
`RUST_IMPLEMENTATION_STATUS.md:246-257`).

## No-iRig Gate Shape

The official no-iRig command is `make rust-no-irig-software-gate` or
`scripts/rust-no-irig-software-gate` (`RUST_NO_IRIG_AUDIO_QUALITY.md:9-21`).
The gate explicitly does not play audio, record audio, install/unload/reload HAL
drivers, change default audio devices, change sample rate/buffer size, open
Traktor/VLC/Spotify/audio UI, reset Core Audio/USB, or acquire the shared
hardware lock (`RUST_NO_IRIG_AUDIO_QUALITY.md:23-33`;
`scripts/rust-no-irig-software-gate:29-30`).

Default software gate parameters:

- `RUST_NO_IRIG_SECONDS=6`, `RUST_NO_IRIG_MODE=dense`,
  `RUST_NO_IRIG_GAIN=0.5`, `SOUNDCHECK_RATE=48000`,
  `RUST_NO_IRIG_PAIRS="A B C D"`
  (`scripts/rust-no-irig-software-gate:8-14`;
  `Makefile:120-123`).
- Pack bench: `20000` transfers, `1000` warmup transfers, min `100 MiB/s` and
  min `1,000,000 frames/s`
  (`scripts/rust-no-irig-software-gate:17-20`;
  `opena8dj-rust-pack-bench.rs:26-39`).
- Synthetic timecode carrier: `1000 Hz`, minimum absolute correlation `0.95`
  (`scripts/rust-no-irig-software-gate:21-22`).
- DVS smoke rates: `44100 48000`
  (`scripts/rust-no-irig-software-gate:23`).

Gate steps worth mirroring in C++ tooling:

1. Build hygiene: fmt/test/clippy equivalent plus HAL smoke/parity and packet
   parity (`scripts/rust-no-irig-software-gate:163-171`).
2. Offline pack bench with start byte `4`, transfer bytes `352`, gain `0.5`,
   big-endian byte order, throughput floors above
   (`scripts/rust-no-irig-software-gate:181-205`).
3. Synthetic timecode analysis for duration/rate/carrier/correlation
   (`scripts/rust-no-irig-software-gate:207-226`).
4. DVS matrix smoke at each configured rate
   (`scripts/rust-no-irig-software-gate:228-254`).
5. Pack-sim matrix: start bytes `0..5`, transfer sizes `48/80/352`, byte orders
   `big/native`, gains `1.0/0.5`, requiring 72 rows and zero failures,
   mismatches, check errors, and panic flags
   (`scripts/rust-no-irig-software-gate:256-330`).
6. Simulated output for each pair A/B/C/D, writing per-pair summaries
   (`scripts/rust-no-irig-software-gate:332-368`).

The result always carries the warning that it is software-only and excludes
analog DAC, physical outputs, iRig capture, and human listening
(`scripts/rust-no-irig-software-gate:376-389`).

## Threshold Crib Sheet

Internal runtime minimums:

- start latency `<= 0.25s`, stretch `<= 0.10s`;
- first callback `<= 0.30s`, stretch `<= 0.12s`;
- driver average CPU `<= 10%`, stretch `<= 5.5%`;
- driver p95 CPU `<= 12%`, stretch `<= 6.5%`;
- stress driver p95 CPU `<= 12%`, stretch `<= 6.5%`;
- `coreaudiod` p95 `<= 8%`, stretch `<= 1.5%`;
- stress `coreaudiod` p95 `<= 8%`, stretch `<= 1.5%`;
- WindowServer p95 during UI stress `<= 45%`, stretch `<= 20%`;
- `outputFramesWritten > 0`;
- `outputFramesRead >= 0.90 * written`, stretch `>= 0.995 * written`;
- timeline resets, active underruns, panic flags, queue failures, and
  non-diagnostic input check errors must be `0`.

Sources: `RUST_PM_SUCCESS_METRICS.md:194-218`;
`crates/open-a8dj-core/src/metrics.rs:48-61,101-137`.

Physical tone thresholds:

- capture peak `0.020..0.920`, stretch `0.100..0.800`;
- sideband ratio `<= 0.008`, stretch `<= 0.004`;
- segment sideband p95 `<= 0.006`, stretch `<= 0.003` in the PM doc;
- segment sideband max `<= 0.008`, stretch `<= 0.004`;
- strongest 940/1060-ish sideband `<= -43 dB`, stretch `<= -50 dB`;
- click outliers and segment click rate `0`.

Sources: `RUST_PM_SUCCESS_METRICS.md:220-235`;
`crates/open-a8dj-core/src/metrics.rs:63-71,138-162`.

Physical real-music thresholds:

- `measurement_status=VALID`, `candidate_quality_status=PASS`,
  `verdict=PASS`;
- alignment `>= 0.970`, stretch `>= 0.985`;
- capture RMS `-28..-10 dBFS`;
- clipped frames `0`;
- 1-5 kHz residual `<= 1.38`;
- 1-5 kHz window p95 `<= 1.40`;
- 1-5 kHz window max `<= 1.46`;
- 1-5 kHz p95/median `<= 1.03`;
- 1-5 kHz max/median `<= 1.06`;
- 5-12 kHz residual `<= 1.32`;
- quiet 1-5 kHz noise `<= -32.5 dBFS`;
- click outliers `0`;
- lag jumps over 2 frames `<= 3`;
- CPU/noise correlation `<= 0.08`, stretch `<= 0.04`;
- driver CPU average `<= 8%`, p95 `<= 12%`, `coreaudiod` p95 `<= 8%`.

Sources: `RUST_PM_SUCCESS_METRICS.md:237-264`;
`crates/open-a8dj-core/src/metrics.rs:73-83,163-192`.

Spectral coloration fields are first-class release blockers:

- `mid_vs_low_coloration_delta_db` within `+/-5 dB`, stretch `+/-2 dB`;
- `high_vs_low_coloration_delta_db` within `+/-6 dB`, stretch `+/-2.5 dB`;
- `metallic_coloration_score_db <= 6 dB`, stretch `<= 2.5 dB`;
- baseline-relative coloration must be `<= baseline + 0.75 dB`.

Sources: `RUST_PM_SUCCESS_METRICS.md:266-287`;
`MAINLINE_FINDINGS_2026_06_14.md:86-113`.

Capture readiness before long physical gates:

- `physical_capture_status=READY`;
- iRig VID/PID `0x1963:0x0059`;
- `found_irig_usb_by_id=1`;
- `found_irig_core_audio=1`;
- `ready_streak >= stable_polls`;
- `stable_polls >= 3`;
- USB enumeration failures must be explicit, with failed USB ports and next
  recovery action preserved.

Sources: `RUST_PM_SUCCESS_METRICS.md:289-318`;
`MAINLINE_FINDINGS_2026_06_14.md:49-84`.

## Mode-2 Packing And Simulation

Core constants:

- streams: `4`;
- channels per stream: `2`;
- bytes per sample: `3`;
- USB bytes per sample lane group: `4`;
- frame bytes per stream: `6`;
- group bytes: `16`;
- check offset: `8`;
- default start byte: `4`;
- default transfer bytes: `352`.

Sources: `crates/open-a8dj-core/src/mode2.rs:4-12`;
`crates/open-a8dj-core/src/mode2.rs:464-472`.

Mode-2 check byte rule:

```text
((stream << 1) as u8) | (((!group) & 1) as u8)
```

Source: `crates/open-a8dj-core/src/mode2.rs:242-245`.

The packer fills check bytes at offset 8 of each 16-byte group, loads one
8-channel frame into four 6-byte stream lanes, advances `output_byte_in_frame`,
and wraps at 6 bytes (`mode2.rs:55-87,129-164`). The decoder validates start
byte and transfer byte layout, counts check errors and panic flags, reconstructs
S24 frames, and tracks `checks`, `check_errors`, `panic_flags`, and
`sample_bytes` (`mode2.rs:314-401`).

Important tests to port conceptually:

- default start byte round-trips synthetic frames with zero check errors and
  panic flags (`mode2.rs:484-511`);
- every start byte `0..5` round-trips with the expected source offset
  (`mode2.rs:513-545`);
- `pack_until_comparable` reaches the Python-validator boundary at `2112`
  packed bytes for 64 frames (`mode2.rs:547-565`);
- callback-provider packing equals slice packing and leaves
  `output_byte_in_frame=4` (`mode2.rs:567-592`).

`opena8dj-rust-pack-sim` defaults to 64 frames, start byte 4, transfer bytes
352, gain 1.0, big-endian output (`opena8dj-rust-pack-sim.rs:14-39`). It fails
unless decoded frames are complete enough, check errors are 0, panic flags are
0, and mismatches are 0 (`opena8dj-rust-pack-sim.rs:125-186`).

`opena8dj-rust-pack-bench` defaults to 20,000 transfers, 1,000 warmup transfers,
transfer bytes 352, start byte 4, gain 0.5, big-endian, min `100 MiB/s`, and min
`1,000,000 frames/s` (`opena8dj-rust-pack-bench.rs:13-39`). It measures
transfers/sec, frames/sec, MiB/sec, ns/transfer, and a checksum
(`opena8dj-rust-pack-bench.rs:159-190`), and PASS requires both throughput
floors (`opena8dj-rust-pack-bench.rs:254-264`).

## Sample Conversion

The Rust conversion policy intentionally matches current HAL/Python behavior for
finite samples: apply gain, clamp to `[-1.0, 1.0]`, quantize through signed Q31,
then keep the upper signed 24 bits. Non-finite samples become flagged silence
instead of crossing the realtime boundary (`crates/open-a8dj-core/src/sample.rs:36-72`).

Useful constants and tests:

- `S24_MIN=-0x800000`, `S24_MAX=0x7fffff`, `S24_SCALE=8388608.0`
  (`sample.rs:1-4`);
- big-endian vectors: `0.0 -> 00 00 00`, `1.0 -> 7f ff ff`,
  `-1.0 -> 80 00 00` (`sample.rs:123-143`);
- native/little vectors: `S24_MAX -> ff ff 7f`, `S24_MIN -> 00 00 80`
  (`sample.rs:145-155`);
- out-of-range samples saturate and report clipping (`sample.rs:157-165`);
- NaN and infinities become zero with `non_finite=true` (`sample.rs:167-175`).

## Topology And Routing

Topology is exactly 4 stereo pairs and 8 channels:

- Pair A: channels 0/1;
- Pair B: channels 2/3;
- Pair C: channels 4/5;
- Pair D: channels 6/7.

Sources: `crates/open-a8dj-core/src/topology.rs:3-49`;
`topology.rs:124-130`.

The routing matrix is explicit: identity, muted, per-channel source, invert,
pair remap, pair mute, pair side swap, and frame application
(`crates/open-a8dj-core/src/routing.rs:25-107`). DVS default starts as identity
(`routing.rs:44-46,140-143`). The useful compound routing test maps A from D,
mutes B, swaps C, and inverts D-right, producing:

```text
[7.0, 8.0, 0.0, 0.0, 6.0, 5.0, 7.0, -8.0]
```

Source: `routing.rs:125-138`.

## Input Decode And DVS Profiles

Input modes are:

- `0`: timecode vinyl;
- `1`: timecode CD/line;
- `2`: phono.

Source: `crates/open-a8dj-core/src/input.rs:8-20`.

Profiles:

- `playback()` uses `TimecodeCdLine`, `software_lock=true`,
  `input_decode_enabled=false`, identity routing (`input.rs:33-44`);
- `timecode_vinyl()` uses mode 0, vinyl ground lift, software lock,
  `input_decode_enabled=true`, identity routing (`input.rs:46-56`);
- `timecode_cd_line()` uses mode 1, CD/line ground lift, software lock,
  `input_decode_enabled=true`, identity routing (`input.rs:58-68`);
- `phono()` uses mode 2, phono ground lift, software lock,
  `input_decode_enabled=true`, identity routing (`input.rs:70-80`).

The decode model first decodes mode-2 USB bytes, preserves raw decoded frame
count, checks, check errors, panic flags, and sample bytes, then only publishes
float frames if `input_decode_enabled` is true. If decode is off, it returns no
frames but still preserves stats (`input.rs:137-173`).

Tests worth carrying:

- DVS profiles match the mainline control policy (`input.rs:180-203`);
- active pairs are preserved with zero check errors and zero panic flags
  (`input.rs:205-226`);
- decode-off preserves stats without publishing frames (`input.rs:228-251`).

This is the key playback CPU lesson: playback-only gates should force the
playback profile so input decode is off, while timecode profiles explicitly
turn it on (`RUST_IMPLEMENTATION_STATUS.md:229-233`).

## Timecode Analyzer

Default `TimecodeAnalysisConfig`:

- sample rate `48000`;
- expected carrier `1000 Hz`;
- minimum RMS `0.05`;
- max balance `1.0 dB`;
- max frequency error `50 ppm`;
- max edge jitter p95 `2.0 frames`;
- min absolute correlation `0.95`;
- max clipped samples `0`.

Sources: `crates/open-a8dj-core/src/timecode.rs:3-28`;
`crates/open-a8dj-core/src/metrics.rs:85-91,193-207`.

The analyzer computes RMS, peak, balance dB, rising zero crossings, carrier
frequency, max absolute frequency error, p95 period jitter, absolute Pearson
correlation, clipped sample count, and PASS/FAIL from those thresholds
(`timecode.rs:90-172`). Tests verify balanced synthetic timecode passes,
wrong expected carrier fails frequency, channel imbalance fails balance, and
clipping fails clip gate (`timecode.rs:267-317`).

The CLI accepts raw f32 input or generates synthetic stereo carrier material.
Default CLI signal is 6 seconds, 0.7 amplitude, right gain 1.0, no phase offset
(`opena8dj-rust-timecode-analyze.rs:33-59,186-198`). JSON output schema:
`open-a8dj-rust.timecode-analyze.v1`
(`opena8dj-rust-timecode-analyze.rs:248-275`).

## DVS Matrix Smoke

`opena8dj-rust-dvs-matrix-smoke` default signal:

- sample rate `48000`;
- duration `6s`;
- Deck A carrier `1000 Hz` on Input A;
- Deck B carrier `1200 Hz` on Input B;
- amplitude `0.7`;
- max C/D leakage RMS `0.0001`;
- timecode thresholds inherited from `TimecodeAnalysisConfig`.

Sources: `opena8dj-rust-dvs-matrix-smoke.rs:16-45`.

The smoke builds 8-channel frames with Deck A on channels 0/1 and Deck B on
channels 2/3 (`opena8dj-rust-dvs-matrix-smoke.rs:119-135`), packs through
mode-2 with default start byte/transfer bytes and big-endian order
(`opena8dj-rust-dvs-matrix-smoke.rs:198-220`), decodes with
`HardwareInputProfile::timecode_vinyl()` (`opena8dj-rust-dvs-matrix-smoke.rs:222-236`),
then requires:

- Deck A status PASS;
- Deck B status PASS;
- Input C and D RMS <= `max_leakage_rms`;
- `check_errors=0`;
- `panic_flags=0`;
- decoded frames non-empty.

Source: `opena8dj-rust-dvs-matrix-smoke.rs:238-258`.

The PM DVS claim is stricter than the offline smoke. Full readiness requires
8 inputs/8 outputs, named A/B/C/D streams, vinyl mode 0, CD-line mode 1, identity
remap/swap/polarity, Deck A only on Input A, Deck B only on Input B, no leakage,
no channel swap after StartIO/StopIO/rate change/hotplug/sleep-wake, no dropout
under CPU/UI stress, physical Traktor scope validation, and human confirmation
(`RUST_PM_SUCCESS_METRICS.md:320-339`).

## Simulated Output Soundcheck

The simulated output script is useful to port because it exercises real music
through the pack/decode path without speakers or microphone:

1. Prepare real music fixture at requested rate/seconds/mode/target peak
   (`scripts/run-simulated-output-soundcheck:36-56`).
2. Build an 8-channel frame stream with only the requested A/B/C/D pair active
   (`scripts/run-simulated-output-soundcheck:114-123`).
3. Pack through the mode-2 Python validator, decode USB bytes, extract the
   requested stereo pair, and fail if not enough frames decode
   (`scripts/run-simulated-output-soundcheck:130-148`).
4. Write a fake zero CPU profile to reuse the existing soundcheck analyzer
   (`scripts/run-simulated-output-soundcheck:151-180`).
5. Run `analyze-soundcheck-capture.py` with max lag 256, max mid-band residual
   ratio, CPU correlation, and JSON output (`scripts/run-simulated-output-soundcheck:248-272`).
6. Run USB raw analysis with check offset 8, configured start byte/byte order,
   pair, max seconds 2, and max lag 256 (`scripts/run-simulated-output-soundcheck:274-294`).

Defaults:

- pair A;
- rate 48000;
- seconds 6.0;
- mode dense;
- target peak `-12 dB`;
- gain 0.5;
- byte order big;
- start byte 4;
- transfer bytes 352;
- max mid-band residual ratio 0.04;
- max mid-band CPU correlation 0.60.

Source: `scripts/run-simulated-output-soundcheck:207-223`; Makefile mirrors
`SOUNDCHECK_MAX_MID_BAND_RESIDUAL_RATIO=0.04` and
`SOUNDCHECK_MAX_MID_BAND_CPU_CORR=0.60` (`Makefile:100-115`).

Accepted Rust simulated pair results from the latest gate:

```text
alignment_score=1.000000
simulated_snr_db=75.22
mid_band_1000_5000_residual_ratio=0.000669
mid_band_1000_5000_residual_dbfs=-108.83
mid_band_cpu_corr=0.000000
```

Sources: `RUST_PM_SUCCESS_METRICS.md:133-141`;
`RUST_IMPLEMENTATION_STATUS.md:193-201`.

## Architecture Lessons For C++

The useful split is not "Rust vs C++"; it is deterministic driver core vs
platform adapter. The core owns topology, CAIAQ/control policy, sample
conversion, mode-2 packing/unpacking, input routing, scheduling policy, metrics,
and deterministic simulations. The platform adapter owns Apple APIs
(`RUST_DRIVER_ARCHITECTURE.md:120-146`).

Realtime path bugs to avoid:

- heap allocation;
- mutex blocking;
- logging;
- file I/O;
- Objective-C message send;
- boxed dynamic dispatch;
- unbounded loops tied to external state;
- panics;
- hidden behavior behind compile-time flag combinations.

Allowed realtime patterns:

- bounded stack buffers;
- preallocated rings;
- atomics;
- predictable sample conversion;
- bounded aggregate counters;
- explicit state transitions.

Source: `RUST_DRIVER_ARCHITECTURE.md:241-288`.

Output timeline policy to preserve:

- client sample time is a hint, not an authority that can force jumps;
- small sample-time jitter is smoothed;
- large gaps reset with measured counters;
- pre-roll/start latency is policy;
- elastic corrections are explicit and counted;
- replay/fade is bounded recovery, not invisible steady state.

Source: `RUST_DRIVER_ARCHITECTURE.md:290-309`.

USB queue policy should be runtime-serializable in run artifacts: capture
transfer frames, capture queue depth, playback transfer frames, playback target
depth, playback max depth, capture-paced lead, explicit scheduling toggle,
open-but-not-streaming idle policy, and stop-ISO-on-StopIO policy
(`RUST_DRIVER_ARCHITECTURE.md:311-328`).

## Metrics And Evidence Schema Ideas

Every snapshot should separate public device state, stream state, capture and
playback transaction counters, output timeline counters, input decode counters,
scheduling counters, capture readiness, supervisor/watch state, spectral
coloration metrics, quality-gate metadata, and build/config identity
(`RUST_DRIVER_ARCHITECTURE.md:330-388`).

The future Rust JSON top-level PM schema is:

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

Source: `RUST_PM_SUCCESS_METRICS.md:405-427`.

## C ABI / Adapter Learnings

The Rust FFI header is a compact model for an adapter boundary:

- versioned config and counters structs;
- explicit byte-order constants;
- opaque engine handle;
- caller-owned buffers;
- default config/channel/default-start/default-transfer queries;
- stateless stream-frame bytes;
- engine create/destroy/reset;
- fill playback bytes from slices or callback;
- output byte position and counters snapshot.

Source: `crates/open-a8dj-ffi/include/open_a8dj_rust.h:10-99`.

This is directly useful if the C++ tree wants an equivalent "boring C ABI"
inside the platform adapter before deeper DriverKit work.

## C++ Action Items

1. Mirror the status vocabulary and keep `BLOCKED_*` distinct from `FAIL`.
2. Add or verify a pure mode-2 pack/unpack parity matrix over:
   start bytes `0..5`, transfer bytes `48/80/352`, byte order `big/native`,
   gains `1.0/0.5`, with zero check errors, zero panic flags, and zero
   mismatches.
3. Preserve the playback profile rule: playback-only gates should keep input
   decode off while preserving decode stats; timecode profiles explicitly enable
   decode.
4. Add synthetic timecode tests with the Rust thresholds: RMS >= 0.05, balance
   <= 1 dB, frequency error <= 50 ppm, p95 jitter <= 2 frames, abs correlation
   >= 0.95, clipped samples = 0.
5. Add a DVS matrix smoke that simulates Deck A on Input A and Deck B on Input B
   at 44.1 and 48 kHz, requires C/D leakage <= 0.0001 RMS, and checks zero
   packet errors/panic flags.
6. Reuse the simulated-output idea for no-hardware regression checks, but keep
   the result labeled pre-DAC/software-only.
7. Keep physical promotion blocked until capture readiness, tone, real music,
   spectral coloration, DVS scope, and human listening are valid.
8. Make every runtime policy in a candidate run serializable: ISO frames, queue
   depths, prefetch, stop behavior, gain, start byte, byte order, input decode
   profile, sample rate, buffer size, lock owner, and evidence paths.

## Non-Claims

This document does not claim Rust, C++, or mainline release readiness. It only
extracts reusable design and QA learnings from the Rust worktree. The Rust
source itself says no-iRig PASS is not analog DAC quality, physical output
quality, cable route validation, iRig capture, Traktor scope validation, or
human listening (`RUST_NO_IRIG_AUDIO_QUALITY.md:68-87`).
