# OpenA8DJ-rust Implementation Status

Last updated: 2026-06-15

This branch now has an executable Rust driver core and a buildable HAL-hosted
candidate. It remains separate from the mainline C/Objective-C worktree and has
not been installed or run against physical audio hardware.

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
- `scripts/rust-hal-hardware-window --help`;
- `make hal-rust`;
- `make smoke-hal-rust`;
- `make parity-smoke-hal-rust`.

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

## Not Yet Claimed

- No hardware playback, iRig capture, Traktor, Spotify, VLC, default-device,
  Core Audio reload, HAL install, USB reset, or physical quality gate has been
  run from this Rust worktree.
- The HAL candidate now uses Rust for playback packet bytes, but it has not yet
  produced physical sound or been captured through iRig.
- Its internal transport profile is designed to match the mainline `0.3.135`
  candidate direction, but Rust has no internal CPU/start-latency evidence until
  the guarded physical-window wrapper installs it and the playback CPU gate runs
  under the global hardware lock.
- It is not yet a physically validated replacement for the mainline driver.

## Next Cut

1. Acquire a short hardware window under
   `$AUDIO_GATE_LOCK_ROOT` for install/reload and physical capture.
2. Run the guarded wrapper, for example
   `scripts/rust-hal-hardware-window --evidence-dir <run-dir> -- <test-command>`,
   so `OpenA8DJ-rust.driver` is installed only inside the bounded window.
3. Compare Rust physical evidence against the current mainline gate before
   making any "as good or better" claim.
