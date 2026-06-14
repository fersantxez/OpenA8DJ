# Shared Hardware Gate

OpenA8DJ-rust shares the same physical Audio 8 DJ, iRig Stream, Core Audio
stack, USB bus, and CPU with the active C/Objective-C investigation on this
MacBook. Worktree isolation does not isolate the hardware.

## Mandatory lock

Before any hardware/audio/CPU-sensitive test, set and acquire this lock:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

Use the Rust-side wrapper when possible:

```sh
scripts/audio-hardware-gate --purpose "short reason" --estimated-duration "5m" \
  --evidence-dir "local-analysis-rust/<run-id>" -- <command>
```

Inspect the current owner with:

```sh
scripts/shared-hardware-lock-status
```

The owner file is:

```text
$AUDIO_GATE_LOCK_ROOT/owner
```

A live owner is exclusive. If the lock is occupied by a live owner, do not run
the test. Wait or coordinate a window. Only clean the lock if the recorded pid
is dead.

## Actions requiring the lock

Acquire the lock before:

- reproducing audio through Audio 8 DJ;
- recording with iRig;
- opening or automating Traktor, VLC, or Spotify for tests;
- changing default devices, sample rate, or buffer;
- installing, reloading, or unloading HAL drivers;
- restarting `coreaudiod`, `audiohald`, `usbaudiod`, or USB services;
- resetting any USB device;
- running physical soundcheck, playback CPU gate, or candidate quality gate.

## Priority

Use this priority order when coordinating windows:

1. Physical iRig capture and human listening.
2. Install/reload/recovery, only in short windows.
3. Autonomous supervisors, low priority. If the lock is busy, they skip the
   cycle.

## Window request format

When asking for a hardware window, state:

- desired time;
- estimated duration;
- whether the run plays audio;
- whether the run records iRig;
- whether the run installs/reloads a driver;
- whether the run restarts Core Audio/USB services;
- whether the run changes default devices, sample rate, or buffer;
- evidence directory.

## Evidence

Any Rust hardware run must leave evidence under `local-analysis-rust/...` and
record at least:

- lock owner metadata;
- command;
- start and end time;
- driver version/hash if applicable;
- physical route if applicable;
- metrics and listening result if applicable.
