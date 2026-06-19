# Traktor Timecode Vinyl

OpenA8DJ 0.5.0 is the current macOS baseline for Traktor and Timecode Vinyl
testing on the Audio 8 DJ.

The driver exposes the Core Audio surface Traktor expects:

- 4 stereo output streams: Output A, B, C, and D
- one 8-channel input stream with named stereo channel pairs:
  Input A, B, C, and D
- CoreMIDI endpoints for MIDI I/O
- Audio 8 DJ hardware profile control for timecode vinyl, CD/line, phono,
  ground-lift flags, software lock, and input decode

## Pre-Test Checklist

1. Confirm the driver is installed and visible in macOS:

```sh
./build/audio-inspect
```

2. Confirm the Audio 8 DJ control bridge is reachable:

```sh
/usr/local/bin/opena8dj-control
```

3. Put the hardware in the Timecode Vinyl profile:

```sh
/usr/local/bin/opena8dj-control profile timecode-vinyl
```

Expected state:

```text
input-mode: 0 (timecode-vinyl)
software-lock: on
input-decode: on
```

If the physical rig has audible computer/CPU-like background noise in the
headphones while the Traktor scope is otherwise stable, test the reversible
low-noise profile:

```sh
/usr/local/bin/opena8dj-control profile timecode-vinyl-low-noise
```

Expected state for that variant:

```text
input-mode: 0 (timecode-vinyl)
gnd-vinyl: off
software-lock: on
input-decode: on
```

Keep this only if the Traktor scope remains stable and the audible noise drops.
If timecode signal quality gets worse, switch back to `profile timecode-vinyl`.

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

`opena8dj-control` exposes Audio 8 DJ control values:

```text
input-mode: 0|1|2|timecode-vinyl|timecode-cd-line|phono
gnd-vinyl: on|off
gnd-cd-line: on|off
gnd-phono: on|off
software-lock: on|off
input-decode: on|off
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
- Unsigned preview packages may require manual macOS approval until Developer
  ID signing and notarization are complete.
