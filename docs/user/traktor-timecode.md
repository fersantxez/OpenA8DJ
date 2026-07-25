# Traktor Timecode Vinyl

OpenA8DJ 0.5.1 is the frozen responsive macOS preview for Traktor and Timecode
Vinyl testing on the Audio 8 DJ. It uses the accepted `output3072` profile.

The driver exposes the Core Audio surface Traktor expects:

- 4 stereo output streams: Output A, B, C, and D
- one 8-channel input stream with named stereo channel pairs:
  Input A, B, C, and D
- CoreMIDI endpoints for MIDI I/O
- Audio 8 DJ hardware control through OpenA8DJ Control Center for DVS Vinyl,
  CD/line, phono, ground-lift flags, software lock, and input decode

## Pre-Test Checklist

1. Confirm the driver is installed and visible in macOS:

Open Audio MIDI Setup and check that `Open Audio 8 DJ` appears with 8 inputs
and 8 outputs.

2. Open `OpenA8DJ Control Center.app`.

3. Select `DVS Vinyl` and click `Apply` if you want to re-apply the default
vinyl state. The driver already starts with vinyl input active by default.

Expected state:

```text
input-mode: 0 (timecode-vinyl)
software-lock: on
input-decode: on
```

The default `DVS Vinyl` state uses the validated low-noise vinyl settings.

4. Set Traktor to use `Open Audio 8 DJ`.

5. Validate first at 44.1 kHz and 48 kHz.

6. Start with the common A/B routing:

```text
Deck A -> Output A L/R
Deck B -> Output B L/R
```

7. Assign timecode inputs in Traktor:

```text
Deck A timecode input -> Input A L/R
Deck B timecode input -> Input B L/R
```

## Hardware-Control Items

OpenA8DJ Control Center exposes Audio 8 DJ control values:

```text
input mode
ground lift
software lock
input decode
```

Input mode mapping:

```text
0 -> timecode-vinyl
1 -> timecode-cd-line
2 -> phono
```

## Acceptance Criteria

- Traktor receives signal on the expected input pair only.
- Traktor's timecode scope is stable for Deck A and Deck B.
- Absolute and relative modes behave normally.
- Playback speed is correct at 44.1 and 48 kHz.
- Deck output remains isolated on A/B/C/D.
- No white noise, heavy aliasing, channel swapping, or input dropouts.
- Lifting the needle does not create runaway playback.
- Ground-lift and input-mode changes have the expected hardware effect.

## Known Risks

- Timecode depends on the physical input path, cartridge, turntable, cable, and
  Traktor setup, not just the driver.
- 88.2 and 96 kHz require separate production-quality validation.
- Use only GitHub Release downloads and verify checksums if anything looks
  unusual.
