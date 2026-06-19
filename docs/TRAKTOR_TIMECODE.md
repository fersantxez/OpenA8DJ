# Traktor Timecode Validation

This document tracks using OpenA8DJ with Traktor timecode vinyl or CD control.

## Current readiness

OpenA8DJ 0.4.0 is the promoted modern macOS C++ mainline. It exposes the Core
Audio surface Traktor needs for output routing and timecode assignment:

- 4 stereo output streams: Output A, B, C, and D
- one 8-channel input stream with named stereo channel pairs:
  Input A, B, C, and D
- 44.1 and 48 kHz playback validated in local listening tests on the tested
  physical route
- CAIAQ control access for Audio 8 DJ input mode and ground-lift flags
- CoreMIDI endpoints published by the installed LaunchAgent

The last public preview with this Traktor-facing channel surface before the
modern C++ promotion was 0.2.6, where initial Timecode Vinyl operator
validation had passed. The 0.4.0 release preserves the 8-input/8-output channel
surface, uses a safer macOS input-stream shape, and combines it with the newer
capture-paced output transport. The remaining release gate is the complete
physical matrix: every input pair, vinyl mode, CD/line mode, channel order, and
low-latency behavior.

## Pre-test checklist

1. Confirm the driver is loaded:

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

The expected control state is:

```text
input-mode: 0 (timecode-vinyl)
software-lock: on
```

4. Set Traktor to use `Open Audio 8 DJ`.

5. Validate at 44.1 kHz and 48 kHz.

6. Start with the currently tested A/B output routing:

```text
Deck A -> Output A L/R
Deck B -> Output B L/R
```

7. Assign timecode inputs in Traktor:

```text
Deck A timecode input -> Input A L/R
Deck B timecode input -> Input B L/R
```

## Hardware-control items to verify

`opena8dj-control` exposes Audio 8 DJ control values:

```text
input-mode: 0|1|2|timecode-vinyl|timecode-cd-line|phono
gnd-vinyl: on|off
gnd-cd-line: on|off
gnd-phono: on|off
software-lock: on|off
```

The input mode mapping used by OpenA8DJ is:

```text
0 -> timecode-vinyl
1 -> timecode-cd-line
2 -> phono
```

This follows public Audio 8 DJ control facts: the Audio 8 DJ exposes three
input modes, while Audio 4 DJ hides mode 0 and uses modes 1 and 2 for line and
phono.

## Acceptance Criteria

- Traktor receives signal on the expected input pair only.
- Traktor's timecode scope is stable for Deck A and Deck B.
- Absolute/relative mode behaves normally.
- Playback speed is correct at 44.1 and 48 kHz.
- Audio output remains isolated on A/B while timecode inputs are active.
- No white noise, heavy aliasing, channel swapping, or input dropouts.
- Ground-lift and input-mode changes have the expected hardware effect.

## Known risks

- Timecode depends on input quality, not just output routing.
- The current control tool exposes raw values rather than friendly mode names.
- 88.2 and 96 kHz are exposed and accepted by the USB protocol, but Traktor
  timecode should be validated first at 44.1 and 48 kHz.
- Public distribution still requires Developer ID signing and notarization.
