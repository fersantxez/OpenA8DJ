# OpenA8DJ-rust Implementation Status

Last updated: 2026-06-14

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
  - exposes config, counters, mode-2 packing, and stateless stream-frame
    signed-24 encoding.
- `build/OpenA8DJ-rust.driver`
  - local HAL bundle candidate only;
  - bundle id `org.opena8dj.driver.hal-rust`;
  - executable `OpenA8DJHALRust`;
  - links the Rust staticlib and uses Rust for float-to-signed-24 stream-frame
    encoding under `OPENA8DJ_USE_RUST_CORE=1`;
  - does not have an install target.

## Verified Offline

- `cargo fmt --all -- --check`
- `cargo test --workspace --locked`
- `cargo clippy --workspace --locked -- -D warnings`
- `cargo build --workspace --release --locked`
- `python3 scripts/validate-mode2-output-packing.py --start-byte 4 --frames 64 --byte-order big`
- `opena8dj-rust-pack-sim --frames 64 --start-byte 4 --byte-order big --json-summary`
- byte-for-byte comparison between Rust mode-2 output and the Python reference
  for `frames=64,start_byte=4,transfer_bytes=352,byte_order=big`;
- C header/staticlib smoke against `open_a8dj_rust.h`;
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
HAL smoke OK: deviceID=2 name=Open Audio 8 DJ sampleRate=48000 streams=5 buffer=512 bufferBytes=16384 bufferRange=512-4096
HAL parity OK
```

## Not Yet Claimed

- No hardware playback, iRig capture, Traktor, Spotify, VLC, default-device,
  Core Audio reload, HAL install, USB reset, or physical quality gate has been
  run from this Rust worktree.
- The HAL candidate currently uses Rust for sample conversion and keeps the
  existing Obj-C timeline, USB scheduling, capture decode, and mode-2 byte loop.
- It is not yet a physically validated replacement for the mainline driver.

## Next Cut

1. Move mode-2 byte packing itself behind the Rust core while preserving the
   mainline output timeline statistics and diagnostic capture semantics.
2. Add a dry-run HAL packing parity harness that compares C and Rust packet
   bytes without opening USB.
3. Only after offline parity is strict, request a short hardware window under
   `$AUDIO_GATE_LOCK_ROOT` for install/reload and physical capture.

