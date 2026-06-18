# Human Test Candidate - 2026-06-18

Target: first complete human-test candidate by 15:00 America/New_York.

This document is a runbook for a controlled human test. It is not a claim that
the C++ line beats mainline C.

## Decision

- Use the HAL bundle / PKG path for the first human-test candidate.
- Do not attempt a real DriverKit/dext candidate today unless the host gains
  full Xcode, DriverKit SDK, `iig`, sufficient disk space, and usable
  entitlements/provisioning.
- Keep the default HAL candidate conservative:
  - ISO8 capture;
  - ISO8 playback;
  - playback coalesce `1`;
  - rejected capture batching inactive;
  - rejected input decode batching inactive.

## Current DriverKit Blockers

- Active developer tools: Command Line Tools.
- DriverKit SDK: not found by `xcrun --sdk driverkit`.
- `iig`: absent.
- Free disk space: about `7 GiB`, below the project gate for full Xcode.
- DriverKit entitlements/provisioning: not available for a real dext install.

## Build-Only Steps

These steps do not install, load, play audio, record audio, reset USB, change
defaults, or restart CoreAudio.

```sh
./scripts/run-cpp-offline-gates
make dist
shasum -a 256 build/OpenA8DJ-0.3.25.pkg build/OpenA8DJ-0.3.25.dmg
```

Required build artifacts:

- `build/OpenA8DJ.driver`
- `build/OpenA8DJ-0.3.25.pkg`
- `build/OpenA8DJ-0.3.25.dmg`
- `build/OpenA8DJ-0.3.25-checksums.txt`

The candidate identity for the human-test window must include:

- git commit hash;
- HAL executable SHA-256;
- PKG SHA-256;
- DMG SHA-256;
- `local-analysis/cpp-offline/current-offline-gates.json`.

## Lock-Gated Install And Physical Smoke

Do not run these steps unless the global audio/hardware lock is acquired:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

Required preflight:

- iRig Stream visible on USB/CoreAudio capture.
- Audio 8 DJ visible on USB.
- no existing OpenA8DJ process/driver left active from prior windows.
- current offline gates pass on the candidate commit.

Install/safety path:

```sh
sudo installer -pkg build/OpenA8DJ-0.3.25.pkg -target /
scripts/test-hal-candidate-safety \
  --candidate build/OpenA8DJ.driver \
  --cycles 1 \
  --leave-loaded \
  --wait 10 \
  --enumeration-timeout 12 \
  --min-idle-pct 10 \
  --run-dir local-analysis/human-test-candidate/20260618T150000Z/safety
```

Physical smoke path:

```sh
scripts/run-soundcheck \
  --skip-build \
  --music-file "/Users/fer/Music/DJ/20250915_santxez_bangers/Guy J - Fixation (Original Mix) [Sanchez].mp3" \
  --pair A \
  --rate 48000 \
  --buffer 512 \
  --seconds 12 \
  --mode dense \
  --target-peak-db -24 \
  --capture-device "iRig Stream" \
  --capture-channels 1,2 \
  --stream-stats-snapshots \
  --run-dir local-analysis/human-test-candidate/20260618T150000Z/soundcheck-irig-pairA-48k
```

Human listening can start only after:

- safety load enumerates `Open Audio 8 DJ` as 8 inputs / 8 outputs;
- no immediate pops/click storms are seen in the smoke run;
- CPU is not obviously runaway;
- iRig remains visible;
- rollback command is ready.

## Rollback

Preferred:

```sh
sudo /usr/local/bin/opena8dj-uninstall
```

Safety cleanup:

```sh
scripts/audio-stack-guard \
  --force-unload-opena8dj \
  --recover \
  --unload-opena8dj \
  --wait 8 \
  --enumeration-timeout 12 \
  --min-idle-pct 10 \
  --run-dir local-analysis/human-test-candidate/20260618T150000Z/final-clean-unload-guard
```

## Stop Conditions

Stop the physical window immediately if any of these occur:

- iRig disappears;
- Audio 8 DJ disappears;
- OpenA8DJ fails to unload;
- CoreAudio enumeration fails after install/load;
- severe pops/clicks/noise;
- channel swap or deck leakage is detected;
- `opena8dj_driver` or `coreaudiod` CPU becomes runaway;
- any script reports recovery debt.

## Claims Allowed

Allowed after this window:

- "First HAL human-test candidate was built and physically smoke-tested."
- "Offline DVS/timecode/routing gates passed."
- "Measured quality and CPU values were collected."

Not allowed without further evidence:

- "Better than mainline."
- "Audiophile quality."
- "Timecode Vinyl physically ready."
- "Ready for branch promotion."
- "DriverKit/dext ready."
