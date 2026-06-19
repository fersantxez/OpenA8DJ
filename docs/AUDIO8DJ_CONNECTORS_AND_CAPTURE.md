# Audio 8 DJ Connectors And Capture Path

Source checked: `local-analysis/audio8dj-2.8.0/payload-files/Applications/Native Instruments/Audio 8 DJ Driver/Documentation/Audio 8 DJ Manual Spanish.pdf`, pages 18, 19, 32, 33, and 34.

This project and the current hardware path are for Native Instruments Audio 8 DJ. If someone says "Audio 2 DJ" in notes, double-check the physical unit: the Audio 8 DJ manual is the one that matches the XLR multicore/turntable setup used here.

## Physical Connector Map

Rear panel:

- `CH A IN 1|2 - OUT 1|2`: Deck A multicore connection. The manual describes this as the male XLR part of the multicore cable. This path is used for turntable/timecode and mixer routing.
- `CH B IN 3|4 - OUT 3|4`: Deck B multicore connection, same role for the second deck.
- `CH C OUT 5|6`: output pair for an auxiliary mixer input or send-effects setup.
- `CH D OUT 7|8`: output pair for an auxiliary mixer input or send-effects setup.
- `GROUND`: optional turntable ground point if grounding through the mixer is noisy.
- `USB`: USB 2.0 connection to the computer.
- `MIDI IN/OUT`: MIDI only, not audio.

Front panel:

- `MIC`: XLR microphone input.
- `CH C IN 5|6`: line-level input, documented for mixer effects output.
- `CH D IN 7|8`: line-level input, documented for a second mixer output and recording.
- `MIC/LINE`: selects the Channel C input source.
- `INPUT MODE`: selects the input mode for Channels A and B: control vinyl, control CD/line, or phono.
- `HEADPHONES`: monitoring output.

Important level rule from the manual: turntables must use Channels A or B because only those channels can use phono-style amplification. Channels C and D are line-level inputs.

## Current macOS Driver Reality

The manual says the original driver can expose Audio 8 DJ as a recording device on macOS. The current OpenA8DJ HAL driver does not yet do that. On this MacBook the live Core Audio enumeration shows:

```text
MacBook Air Microphone: in=1 out=0
MacBook Air Speakers:   in=0 out=2
Open Audio 8 DJ:        in=0 out=8
```

So the Audio 8 DJ physical inputs exist, but macOS cannot currently record from them through this driver. Do not assume `IN 5|6`, `IN 7|8`, `MIC`, `IN 1|2`, or `IN 3|4` are usable capture devices until input support is implemented or direct USB input capture is validated separately.

## Safe Capture Plan For Driver Quality Tests

Do not disturb the existing turntable/multicore wiring on Channels A and B.

The safest deterministic post-card capture path is:

1. Play audio through OpenA8DJ outputs as usual.
2. Take a spare mixer output, ideally `REC OUT`, `BOOTH OUT`, or a second master output.
3. Feed that mixer output into a separate USB audio interface with real line inputs.
4. Select that separate interface as the soundcheck capture device.

This captures the signal after the Audio 8 DJ DAC and after the real analog path, without changing the deck wiring.

## Confirmed External Capture Device

The iRig Stream has been verified on this Mac as a usable Core Audio capture device:

```text
iRig Stream
uid=AppleUSBAudioEngine:IK Multimedia:iRig Stream:152349:2,1
in=2 out=2
rate=48000
```

It records through the macOS class-compliant Apple USB Audio path, so no extra vendor driver is required for the current capture workflow. A 3-second stereo WAV capture succeeded at 48 kHz from channels 1 and 2.

Use it in soundcheck as:

```text
--capture-device "iRig Stream" --capture-channels 1,2
```

## What Not To Cable

- Do not cable a mixer line output into the MacBook headphone jack. This MacBook exposes only the built-in microphone as an input, not a proper line input.
- Do not cable a mixer line output into a phono input. It will be the wrong level and equalization.
- Do not use software loopback devices as release-quality proof. They can prove the player stream, but not what the Audio 8 DJ outputs physically produced.
- Do not move the turntable/multicore cables just to make a test pass.

## Fallback If No Separate Interface Exists

The built-in MacBook microphone can record the room acoustically, but that is only a rough sanity check. It is not a numeric pass/fail test for crackle, mid-band noise, or CPU-coupled distortion.

If no external recorder or USB interface is available, the next engineering path is to implement or validate an Audio 8 DJ input capture path. That is a driver/direct-USB task, not a cable-only task.
