# Rust Parallel Experiment Rules

This worktree is the isolated Rust/modular-core experiment for OpenA8DJ.

## Absolute boundary

- The main implementation worktree at `/Users/fer/dev/opena8dj` is read-only for this experiment.
- Agents working here may inspect `/Users/fer/dev/opena8dj` with read-only commands such as `rg`, `sed`, `git show`, `git diff`, and `git log`.
- Agents working here must not edit, format, generate files into, install from, clean, reset, or otherwise mutate `/Users/fer/dev/opena8dj`.
- Do not run commands from this worktree that install or replace the active OpenA8DJ HAL driver unless the user explicitly asks for a Rust experiment install.

## Shared hardware/audio gate

This MacBook has one shared Audio 8 DJ, iRig Stream, Core Audio stack, USB bus,
and CPU. The Rust worktree being isolated does not make the hardware isolated.

Before any test that touches hardware, audio routing, physical capture, driver
installation/reload, Core Audio/USB services, or CPU-sensitive playback gates,
agents must use the global lock:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
scripts/audio-hardware-gate --purpose "short reason" --estimated-duration "5m" \
  --evidence-dir "local-analysis-rust/<run-id>" -- <command>
```

The lock owner file is `$AUDIO_GATE_LOCK_ROOT/owner`. A live owner is exclusive.
Only clean the lock when the recorded pid is dead. If the lock is occupied by a
live owner, do not run the test; wait or coordinate a hardware window. Inspect
the current owner with `scripts/shared-hardware-lock-status`.

The lock is mandatory before:

- reproducing audio through Audio 8 DJ;
- recording with iRig;
- opening or automating Traktor, VLC, or Spotify for tests;
- changing default devices, sample rate, or buffer;
- installing, reloading, or unloading HAL drivers;
- restarting `coreaudiod`, `audiohald`, `usbaudiod`, or USB services;
- resetting any USB device;
- running physical soundcheck, playback CPU gate, or candidate quality gate.

Hardware-window requests must state the desired time, estimated duration,
whether the run plays audio, records iRig, installs/reloads the driver,
restarts Core Audio/USB, or changes defaults, and where evidence will be
written.

## Purpose

The Rust branch may learn from the C/Objective-C implementation, QA artifacts, and documented experiments, but it must never delay, block, or contaminate mainline driver investigation.

Mainline remains the source of truth for active audio debugging, hardware gates, and release candidates.

## Expected workflow

- Work only under `/Users/fer/dev/audio8djrust`.
- Keep the branch isolated as `rust/modular-core-spike`.
- Treat `local-analysis` data from the main worktree as evidence, not as a shared write location.
- Put Rust experiment outputs under this worktree's own ignored local output directories.
- Prefer small, testable Rust modules behind a C ABI over broad rewrites.
- Do not propose merging Rust into mainline until it has passed parity tests and the existing QA gates.

## Agent handoff sentence

Every Rust-side agent must be told:

> You are working in `/Users/fer/dev/audio8djrust` on branch `rust/modular-core-spike`. `/Users/fer/dev/opena8dj` is strictly read-only reference material. It is forbidden to modify, format, install from, clean, reset, or generate files into the main worktree. Before any hardware, audio, driver, Core Audio/USB, Traktor/VLC/Spotify, default-device, sample-rate, buffer, iRig, physical soundcheck, playback CPU gate, or candidate quality gate action, acquire `AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"` and treat a live owner as exclusive.
