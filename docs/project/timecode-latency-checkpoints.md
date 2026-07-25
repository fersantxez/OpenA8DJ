# Timecode Latency Checkpoints

OpenA8DJ 0.5.1 is the frozen `output3072` DVS baseline. Future work must happen
on an isolated macOS branch and must not silently change the 0.5.1 release
artifact or the installed rollback copy.

## Frozen Baseline

```text
output start: 3072 frames
output target: 3072 frames
output restart: 1536 frames
elastic high water: 9216 frames
capture ISO batch: 64 frames
host validation buffer: 512 frames
primary validation rate: 48000 Hz
offline modeled latency p95: 78.667 ms
```

The reproducible source target is
`make freeze-timecode-stable3072-candidate`. The resulting named bundle and
manifest are:

```text
build/OpenA8DJ-frozen-stable3072.driver
build/hal-candidates/frozen-stable3072.json
```

## Checkpoint 1: Offline Correctness

A candidate must pass:

```sh
make timecode-latency-offline-gate
scripts/run-cpp-offline-gates
```

Required outcomes:

- all default and release tests pass;
- zero decode errors, output overflows, and panic flags;
- evidence provenance matches the current clean commit;
- modeled p95 latency is lower than or equal to the candidate's declared
  target and is compared with 78.667 ms.

## Checkpoint 2: Exact-Artifact Safety

The exact named candidate must pass at least three guarded cycles with a
20-second stabilization window. It must enumerate `Open Audio 8 DJ` as 8 in /
8 out at 48 kHz, recover after unload, and leave the hardware lock free.

Reject a candidate if any cycle hangs, loses the device, fails recovery, or
creates sustained CoreAudio/driver CPU above the established baseline.

## Checkpoint 3: Same-Session Sound And CPU

Use one lock-gated session to compare the frozen 0.5.1 baseline with the new
candidate through the same Audio 8 DJ output and external capture route.

Required outcomes:

- no new clicks, dropouts, channel swaps, clipping, or white noise;
- no regression in alignment, SNR, lag stability, or route isolation;
- driver and CoreAudio CPU are no worse than the baseline unless a documented
  small CPU increase buys a measured and accepted latency improvement;
- the exact captured artifacts and process samples are retained.

## Checkpoint 4: Traktor And Vinyl

Automated fixtures are necessary but cannot certify physical vinyl feel. Before
promotion, test both decks in Traktor at 44.1 and 48 kHz and record:

- scope stability and calibration;
- absolute and relative mode behavior;
- cue/start/reverse response;
- needle lift and re-drop behavior;
- dropout count;
- CPU before, during, and after the test;
- operator acceptance against 0.5.1.

## Checkpoint 5: Release

Only a candidate that passes Checkpoints 1-4 can replace 0.5.1. A polished
public release additionally requires Developer ID signing, Apple acceptance,
stapling, checksum regeneration, installation from the GitHub-downloaded DMG,
and final device/CPU verification.

If any gate fails, reinstall the 0.5.1 DMG and confirm its installed HAL
executable matches the frozen release hash.
