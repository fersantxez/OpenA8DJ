# OpenA8DJ-rust Implementation Status

Last updated: 2026-06-15

This branch now has an executable Rust driver core and a physically exercised
HAL-hosted candidate. It remains separate from the mainline C/Objective-C
worktree. All hardware/audio runs below used the global hardware gate and the
bounded `OpenA8DJ-rust.driver` install wrapper.

## Built Artifacts

- `target/release/opena8dj-rust-pack-sim`
  - offline mode-2 output packing simulator;
  - validates Python/mainline-compatible start byte `4`;
  - emits `open-a8dj-rust.pack-sim.v1` JSON summaries.
- `target/release/libopen_a8dj_ffi.a`
  - C ABI static library for the Rust core;
  - exposes config, counters, stateless stream-frame signed-24 encoding, and
    callback-driven mode-2 playback byte filling.
- `build/OpenA8DJ-rust.driver`
  - local HAL bundle candidate only;
  - bundle id `org.opena8dj.driver.hal-rust`;
  - device UID `org.opena8dj.Audio8DJ-rust`;
  - executable `OpenA8DJHALRust`;
  - links the Rust staticlib and uses Rust for full mode-2 playback byte fill
    under `OPENA8DJ_USE_RUST_CORE=1`;
  - uses the current mainline internal candidate transport profile:
    ISO64, capture queue `8`, playback queue `8`, output prefetch `64`,
    output amplitude stats off, output write stats on an atomic counter,
    stop ISO on StopIO, and background pre-open / keep-open USB behavior;
  - presents as `Open Audio 8 DJ-rust` in HAL smoke tests;
  - keeps Obj-C for output timeline, stats, diagnostic semantics, USB
    scheduling, and capture decode while Rust owns playback packet bytes;
  - does not have an unguarded install target.
- `scripts/rust-hal-hardware-window`
  - builds the Rust HAL candidate before acquiring the lock;
  - acquires `$AUDIO_GATE_LOCK_ROOT` through `scripts/audio-hardware-gate`;
  - temporarily installs `OpenA8DJ-rust.driver`;
  - moves an active `OpenA8DJ.driver` out of HAL during the bounded window;
  - restores the previous mainline HAL bundle and restarts Core Audio on exit
    unless `--keep-installed` is explicitly supplied.
- `scripts/rust-no-irig-software-gate`
  - runs the reproducible no-iRig software quality gate;
  - does not play audio, record audio, install or reload HAL drivers, change
    default devices, or acquire the hardware lock;
  - writes a self-contained evidence directory under
    `local-analysis-rust/software-runs/`;
  - covers Rust build hygiene, HAL bundle smoke/parity, packet parity,
    Rust pack-sim matrix behavior, and simulated output quality for all four
    stereo output pairs.

## Verified Offline

- `cargo fmt --all -- --check`
- `cargo test --workspace --locked`
- `cargo clippy --workspace --locked -- -D warnings`
- `cargo build --workspace --release --locked`
- `make smoke-hal-rust parity-smoke-hal-rust rust-packet-parity` after the
  ISO64/q8 + pre-open candidate update;
- `python3 scripts/validate-mode2-output-packing.py --start-byte 4 --frames 64 --byte-order big`
- `opena8dj-rust-pack-sim --frames 64 --start-byte 4 --byte-order big --json-summary`
- byte-for-byte comparison between Rust mode-2 output and the Python reference
  for `frames=64,start_byte=4,transfer_bytes=352,byte_order=big`;
- C header/staticlib smoke against `open_a8dj_rust.h`;
- callback-provider parity test proving Rust callback filling matches the
  slice-based mode-2 packer;
- `make rust-packet-parity`
  - compares legacy C mode-2 packing against Rust callback-driven filling
    without opening USB;
  - covers start bytes `0..5`, transfer sizes `352`, `48`, and `80`, gains
    `1.0` and `0.5`, and big/native signed-24 byte order;
- `bash -n scripts/rust-hal-hardware-window`;
- `bash -n scripts/rust-no-irig-software-gate`;
- `scripts/rust-hal-hardware-window --help`;
- `make hal-rust`;
- `make smoke-hal-rust`;
- `make parity-smoke-hal-rust`.
- `make rust-no-irig-software-gate`.

## Current Evidence

The release simulator reports:

```json
{"schema":"open-a8dj-rust.pack-sim.v1","status":"PASS","frames":64,"source_start_frame":1,"packed_bytes":2112,"transfer_bytes":352,"transfers":6,"start_byte":4,"byte_order":"big","decoded_frames":65,"compared_frames":63,"checks":528,"check_errors":0,"panic_flags":0,"sample_bytes":1584,"mismatches":0}
```

The HAL Rust smoke reports the expected local bundle shape:

```text
HAL smoke OK: deviceID=2 name=Open Audio 8 DJ-rust sampleRate=48000 streams=5 buffer=512 bufferBytes=16384 bufferRange=512-4096
HAL parity OK
```

The C/Rust packet harness reports:

```text
PASS rust_packet_parity cases=72
```

The latest no-iRig software quality gate reports:

```text
rust_no_irig_software_gate=PASS
run_dir=/Users/fer/dev/audio8djrust/local-analysis-rust/software-runs/rust-no-irig-software-gate-20260615T035139Z
```

It passed `cargo fmt`, `cargo test`, `cargo clippy`, Rust HAL smoke/parity,
Rust/C packet parity, tool build, a `72`-row Rust pack-sim matrix, and simulated
output soundchecks for pairs `A/B/C/D`.

The Rust pack-sim matrix reported:

```text
matrix_rows=72
matrix_failures=0
start_bytes=0,1,2,3,4,5
transfer_bytes=48,80,352
byte_orders=big,native
max_check_errors=0
max_panic_flags=0
max_mismatches=0
```

Each simulated output pair reported:

```text
alignment_score=1.000000
simulated_snr_db=75.22
mid_band_1000_5000_residual_ratio=0.000669
mid_band_1000_5000_residual_dbfs=-108.83
mid_band_cpu_corr=0.000000
```

The current physical/internal candidate is commit `3429796`:

```text
local-analysis-rust/physical-runs/rust-candidate-3429796-stress-drivercpu-3x-20260615T032525Z
```

It passed three real-music playback runs through the temporarily installed
`Open Audio 8 DJ-rust` HAL device with CPU stress enabled:

| Run | start | first callback | driver p95 | stress driver p95 | coreaudiod p95 | stress coreaudiod p95 |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | `0.026026s` | `0.033130s` | `6.9%` | `6.2%` | `1.5%` | `1.4%` |
| 2 | `0.025478s` | `0.032509s` | `6.7%` | `6.1%` | `1.5%` | `1.3%` |
| 3 | `0.026860s` | `0.032618s` | `6.7%` | `5.8%` | `1.5%` | `1.3%` |

All three runs reported:

```text
outputTimelineResets=0
outputActiveUnderruns=0
outputPanicFlags=0
outputElasticDrops=0
outputElasticReplays=0
control_rc=0
```

The key mainline learning integrated in this candidate is playback-only input
decode gating. `scripts/rust-playback-cpu-smoke` now forces
`opena8dj-control profile playback`, which leaves `input-decode: off` for
playback-only gates while preserving explicit `timecode-*` profiles for input
and DVS work.

The previous no-stress three-run playback check also passed at commit
`a60eced`:

```text
local-analysis-rust/physical-runs/rust-candidate-a60eced-no-irig-3x-inputdecode-gated-20260615T031542Z
```

That run showed `coreaudiod` p95 `1.8%`, `1.8%`, and `1.9%`, with zero
timeline resets, active underruns, panic flags, elastic drops, or elastic
replays.

## Not Yet Claimed

- No iRig capture, Traktor, Spotify, VLC, default-device change, USB reset, or
  physical tone/music quality gate has been run from this Rust worktree.
- The HAL candidate has produced physical playback through Audio 8 DJ during
  guarded runs, but it has not yet been captured through iRig.
- It is an internal no-iRig playback/stress candidate that matches or beats the
  current mainline `0.3.135` internal playback reference on start latency,
  first callback latency, `coreaudiod` p95, and stress `coreaudiod` p95; driver
  p95 is in the same target band.
- It is not yet an audiophile-validated or DVS-validated replacement for the
  mainline driver.

## Next Cut

1. Keep `make rust-no-irig-software-gate` green after each Rust candidate
   change.
2. Wait for iRig USB/Core Audio capture readiness.
3. Under the global lock, run Rust tone and real-music physical capture through
   iRig and compare against a valid mainline baseline.
4. Run DVS/timecode input gates with `opena8dj-control profile timecode-vinyl`
   or `profile timecode-cd-line`, then verify decode is active only for input
   clients.
5. Add UI stress parity if mainline raises the no-iRig bar beyond the current
   CPU-stress filter.
