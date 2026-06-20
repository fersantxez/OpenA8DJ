# Testing

This document describes the current macOS validation flow for `main`.

## Offline First

Before packaging or installing:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline
ctest --test-dir build/cpp-offline --output-on-failure
make clean
make all
make dist
```

Offline tests cover packet layout, channel topology, routing, synthetic
timecode behavior, DriverKit scaffolding, real-time policy, and packaging
sanity. They do not prove physical sound quality.

## Installed Device Checks

After an authorized install, `Open Audio 8 DJ` should enumerate as:

```text
8 inputs
8 outputs
Input A/B/C/D stereo pairs
Output A/B/C/D stereo pairs
```

Useful local inspection tools:

```sh
./build/audio-inspect
./build/audio-io-test 2 44100
./build/audio-io-test 2 48000
./build/midi-list
/usr/local/bin/opena8dj-control
```

44.1 kHz and 48 kHz are the minimum validated release rates. Higher rates stay
extended validation targets until release notes say otherwise.

## Sound Quality

Do not offer a normal listening build until the exact loaded artifact has
passed a real sound-quality check.

Minimum physical check:

1. Play a saved WAV or generated tone through Audio 8 DJ.
2. Capture the output through the approved external capture route.
3. Compare capture against the original reference.
4. Record CPU/resource counters from the same run.
5. Save the evidence under `<evidence-dir>/`.

If this fails, the build is diagnostic-only.

## Routing

Check every output pair:

```sh
./build/audio-pair-tone A 3 440 0.06
./build/audio-pair-tone B 3 660 0.06
./build/audio-pair-tone C 3 880 0.06
./build/audio-pair-tone D 3 1100 0.06
```

The test must confirm correct pair assignment and no obvious leakage.

## Traktor Timecode

Set the hardware profile before DVS testing:

```sh
/usr/local/bin/opena8dj-control profile timecode-vinyl
```

Then validate in Traktor:

- input pairs A/B/C/D are visible;
- timecode signal appears on the expected physical input;
- scratch response is responsive;
- deck output routing remains correct;
- no unrelated input pair leaks into the active deck.

Full DVS readiness requires the complete input matrix.

## Hardware Lock

Any install, unload, reload, playback, recording, USB action, Core Audio action,
or Traktor session requires:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

Never run hardware/audio checks without the lock and an explicit validation
window.
