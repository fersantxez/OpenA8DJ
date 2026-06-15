# Rust No-iRig Audio Quality Strategy

This document defines what OpenA8DJ-rust may measure while iRig capture is
unavailable.

No-iRig evidence is useful for detecting driver-side regressions before the
Audio 8 DJ DAC. It is not a substitute for post-DAC capture or human listening.

## Implemented Gate: Software-Only Output Quality

Canonical command:

```sh
make rust-no-irig-software-gate
```

Direct script:

```sh
scripts/rust-no-irig-software-gate
```

This gate is the official no-iRig quality check. It does not:

- play audio;
- record audio;
- install, unload, or reload HAL drivers;
- change default audio devices;
- change sample rate or buffer size;
- open Traktor, VLC, Spotify, or any audio UI;
- reset Core Audio, USB audio, or USB devices;
- acquire the shared hardware lock.

It measures:

- Rust format, tests, and clippy status;
- Rust HAL bundle smoke/parity;
- C/Rust packet parity;
- Rust packer throughput, with a conservative default floor of `100 MiB/s` and
  `1,000,000 frames/s` for the offline callback-driven output packer;
- Rust timecode/input signal analysis on a synthetic stereo carrier, covering
  carrier frequency, edge jitter, channel balance, clipping, and stereo
  correlation before physical DVS testing is available;
- Rust DVS matrix smoke at `44.1 kHz` and `48 kHz`, simulating Deck A on
  Input A and Deck B on Input B, then verifying mode-2 input decode, no packet
  check errors, no panic flags, no C/D leakage, and DVS profile policy
  (`input-mode=0`, `input-decode=on`);
- Rust pack-sim matrix behavior across start bytes, transfer sizes, signed-24
  byte orders, and gains;
- software simulated output quality for stereo pairs `A/B/C/D` using real
  music fixture material;
- alignment, SNR, 1-5 kHz residual, and CPU/noise correlation in the simulated
  output path.

Latest accepted evidence:

```text
/Users/fer/dev/audio8djrust/local-analysis-rust/software-runs/rust-no-irig-software-gate-20260615T205923Z
```

Status vocabulary:

```text
PASS
FAIL
```

Product interpretation:

- `PASS` means the Rust candidate is clean through offline driver-side output
  generation and simulated USB packet reconstruction.
- `PASS` does not prove analog DAC quality, output jack behavior, physical
  cable routing, iRig capture, or human listening quality.
- Any Rust candidate that changes output packing, gain, routing, sample format,
  HAL stream shape, or playback profile must keep this gate green before it can
  request physical testing.
- A throughput `PASS` only proves the offline packer is not catastrophically
  slow. Real CPU acceptance still comes from locked playback/load gates.
- A synthetic timecode `PASS` proves the Rust analysis path and thresholds work
  on known-good stereo carrier material. It does not prove Traktor vinyl/CD-line
  behavior until a real Audio 8 DJ input capture or Traktor scope validation
  passes under the hardware lock.
- A DVS matrix smoke `PASS` proves offline routing/decode policy for synthetic
  Input A/B timecode at the target rates. It does not prove absolute/relative
  mode, real control vinyl, Traktor scope, phono/CD-line analog behavior, or
  low-latency physical capture.

## Future Option: Driver-Internal Diagnostic Capture

Priority: medium.

Purpose:

- capture what Core Audio gives the HAL;
- capture what the driver sends toward the USB output path;
- compare driver-internal audio against a reference without relying on iRig.

Useful for:

- clipping detection;
- gain mistakes;
- byte-order mistakes;
- discontinuities before USB transfer;
- stream routing mistakes;
- drift inside the driver pipeline.

Limitation:

- still pre-DAC;
- cannot validate analog output noise, DAC behavior, cable route, or speaker
  listening quality.

Acceptance before implementation:

- must write evidence under `local-analysis-rust/software-runs/`;
- must not install/reload HAL unless explicitly run inside a hardware window;
- if it uses a live HAL instance or Core Audio device, it must acquire the
  shared hardware lock first.

## Future Option: Audio 8 DJ Self-Loopback

Priority: high once a safe physical route exists.

Purpose:

- route an Audio 8 DJ output pair into an Audio 8 DJ input pair;
- measure post-DAC and pre-ADC quality without iRig.

Useful for:

- analog output smoke beyond software-only tests;
- channel routing validation;
- gross click/noise detection;
- timecode-input path practice once input decode is stable.

Limitation:

- shares the same device ADC and driver input path, so it is not an independent
  external capture reference;
- requires physical cabling and touches hardware/audio;
- may disturb mainline experiments if run outside a coordinated window.

Acceptance before implementation:

- must acquire `$AUDIO_GATE_LOCK_ROOT`;
- must release the lock before waiting for user input;
- must clearly report output pair, input pair, cable route, sample rate,
  duration, and evidence directory;
- must skip rather than run if the lock is busy.

## Future Option: MacBook Microphone Smoke

Priority: low.

Purpose:

- provide a coarse "is sound obviously broken?" check when no capture interface
  is available.

Useful for:

- silence detection;
- catastrophic clicks;
- extremely distorted playback;
- quick human-audible smoke only.

Limitation:

- room, speaker, microphone, AGC, and macOS input processing dominate the
  measurement;
- not acceptable for audiophile quality metrics;
- not acceptable for candidate promotion.

Acceptance before implementation:

- must acquire `$AUDIO_GATE_LOCK_ROOT` because it plays and records audio;
- must label results as low-confidence smoke only;
- must never replace iRig or another independent physical capture route.
