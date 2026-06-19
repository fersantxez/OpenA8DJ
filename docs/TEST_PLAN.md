# Test Plan

This plan validates the modern macOS OpenA8DJ line. It separates safe offline
checks from hardware/audio checks that require explicit operator approval and
the global hardware lock.

## Safe Offline Checks

Run these before packaging or offering a build:

```sh
cmake -S . -B build/cpp-offline
cmake --build build/cpp-offline
ctest --test-dir build/cpp-offline --output-on-failure
make clean
make all
make dist
```

Expected coverage:

- C++ core contracts.
- Packet packing and input decode.
- 8-in/8-out device model.
- A/B/C/D routing identity and leakage checks.
- Timecode profile policy and synthetic DVS signal handling.
- DriverKit/AudioDriverKit shell contracts.
- Real-time policy checks.
- DMG/PKG/checksum generation.

Offline checks must not install drivers, change audio defaults, reset USB,
restart Core Audio, play sound, record sound, or touch physical hardware.

## Package Checks

For every release candidate:

- Build `OpenA8DJ-<version>.dmg`.
- Build `OpenA8DJ-<version>.pkg`.
- Build `OpenA8DJ-<version>-checksums.txt`.
- Confirm the DMG opens and contains the PKG plus license/legal docs.
- Confirm the PKG contains only expected install paths.
- Confirm the uninstall script is included.
- Confirm release notes match the artifact version.

## Hardware Lock Policy

Hardware/audio checks require:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

Acquire the lock before any test that installs, unloads, reloads, plays audio,
records audio, touches USB, changes Core Audio state, changes default devices,
opens Traktor, or resets services.

If the lock is occupied, do not wait while holding partial state. Report the
desired window, duration, actions, and evidence directory.

## Physical Sound-Quality Check

Before handing a build to a human listener:

1. Install or load the exact candidate artifact under lock.
2. Verify `Open Audio 8 DJ` enumerates as 8 inputs / 8 outputs.
3. Verify MIDI endpoints are present.
4. Play a saved WAV or generated tone through Audio 8 DJ.
5. Capture the Audio 8 DJ output through the approved external capture route.
6. Compare captured audio against the original WAV/tone reference.
7. Record CPU/resource counters from the same run.
8. Save all evidence under `local-analysis/`.
9. Mark the build as candidate-only if the evidence is clean; otherwise keep it
   diagnostic-only.

Do not hand over a normal candidate if the exact loaded build has not passed a
real sound-quality check.

## Traktor / Timecode Check

Use this after basic sound quality is clean:

1. Set the profile:

   ```sh
   /usr/local/bin/opena8dj-control profile timecode-vinyl
   ```

2. Open Traktor.
3. Assign input pairs A/B/C/D.
4. Confirm vinyl signal appears on the expected deck.
5. Confirm scratch response.
6. Confirm output routing while timecode is active.
7. Record which physical inputs, vinyl mode, buffer size, and sample rate were
   used.

Full DVS readiness requires the complete input matrix, not just one successful
deck.

## Evidence Format

Each physical run should record:

- date and local time;
- git commit;
- artifact hash;
- installed driver hash;
- sample rate and buffer size;
- input/output pair;
- source WAV/tone path;
- capture WAV path;
- analyzer result;
- CPU/resource summary;
- underrun/overrun counts;
- human listening notes if applicable;
- PASS/FAIL/DIAGNOSTIC_ONLY status.

## Branch Roles

- `main`: current macOS C++ driver, docs, packaging, and releases.
- `legacy`: previous C/Objective-C implementation and historical reference
  material.

Tests in `main` should validate the modern macOS line directly. Historical
comparisons are useful as engineering context, but they must not make the
public `main` branch look like the old implementation.
