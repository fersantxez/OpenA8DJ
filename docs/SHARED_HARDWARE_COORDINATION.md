# Shared Hardware Coordination Protocol

This laptop has one physical Audio 8 DJ, one iRig Stream capture path, one
Core Audio stack, and one CPU budget. Every worktree and every agent must treat
those as shared, exclusive test resources.

## Shared Lock

All hardware-affecting tests must use the same lock:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

This path is outside any worktree, so both the C/Obj-C driver tree and the Rust
redesign tree see the same owner.

To inspect the current owner:

```sh
./scripts/shared-hardware-lock-status
```

If the other worktree does not have this helper yet, inspect:

```sh
cat "$HOME/.opena8dj/hardware-gate.lock/owner"
```

## What Requires The Lock

Acquire the shared lock before any action that can change audio state, consume
the Audio 8 DJ/iRig path, or pollute CPU/audio measurements:

- playing audio through Audio 8 DJ
- recording from iRig Stream
- running Traktor, VLC, Spotify, or any automated playback/capture gate
- running physical soundcheck or candidate-listen-gate
- running playback CPU/UI stress gates
- changing Core Audio default devices or sample rate/buffer
- restarting `coreaudiod`, `audiohald`, `usbaudiod`, or USB power services
- resetting any USB device
- installing, unloading, or replacing HAL drivers
- running long audio-stack diagnostics during someone else's measurement

Read-only code editing, builds that do not install, unit tests without Core
Audio/USB, static analysis, and documentation do not need the lock.

## Priority Rules

1. Physical iRig capture gates have highest priority.
2. Human listening setup has priority over autonomous polling.
3. Install/reload/reboot/recovery operations must announce themselves by lock
   owner name and must be short.
4. Autonomous supervisors are lowest priority. If the lock is busy, they must
   skip the cycle and try later.
5. No agent may kill another agent's process or remove the shared lock unless
   the owner pid is dead and the status is clearly stale.

## Required Owner Metadata

The lock owner file should include at least:

```text
pid=<process id>
gate=<short test name>
run_dir=<absolute or repo-relative evidence dir>
cwd=<worktree path>
started_at=<UTC timestamp>
```

## Courtesy Before Long Runs

Before a run longer than two minutes:

1. Check `shared-hardware-lock-status`.
2. Check CPU health.
3. If free, acquire the lock.
4. Write a run manifest with expected duration and the test purpose.
5. Release the lock immediately after the test, even on failure.

## Current OpenA8DJ Behavior

This worktree defaults to the global lock and its autonomous QA supervisor now
uses it. The supervisor will skip cycles while another agent owns the hardware.

Current helper commands:

```sh
./scripts/shared-hardware-lock-status
./scripts/autonomous-audio-qa-status
./scripts/candidate-status
```

Current safety behavior:

- Compound gates export inherited lock state to child gates, so a full quality
  window acquires the physical resource once and nested gates do not self-block.
- `make install-hal` uses the candidate safety loader instead of raw HAL copy
  and Core Audio restart commands.
- State-changing recovery/install/isolation scripts acquire the shared lock
  before touching Core Audio, USB services, or `/Library/Audio/Plug-Ins/HAL`.
- The autonomous QA supervisor defers gates and recovery if VLC, Spotify,
  Traktor, or `audio-wav-play` are active, and limits automatic recovery
  attempts before backing off to observation.

## Prompt For The Other Project Manager

Use this prompt with the agent manager/product manager coordinating the Rust
worktree:

```text
You are coordinating the Rust modular OpenA8DJ redesign in another worktree on
the same MacBook. The C/Obj-C OpenA8DJ agent is also running on this same laptop
and both projects share the same physical Audio 8 DJ, iRig Stream, Core Audio
stack, USB bus, and CPU budget.

Mandatory coordination rule:

Set and use this global lock for every hardware-affecting or CPU/audio-sensitive
test:

export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"

Before any playback, iRig recording, Audio 8 DJ claim, Traktor/VLC/Spotify test,
Core Audio default-device change, HAL install/reload, coreaudiod/audiohald/
usbaudiod restart, USB reset, physical soundcheck, playback CPU gate, or
candidate quality gate, acquire that lock. If the lock is busy, do not run the
test. Wait, reschedule, or ask for coordination.

The lock owner file is:

$HOME/.opena8dj/hardware-gate.lock/owner

It contains pid, gate, run_dir, cwd, and started_at. Treat a live owner as
exclusive. Only clear a stale lock if the owner pid is dead.

Priority:

1. Physical iRig capture gates and human listening setup have top priority.
2. Install/reload/reboot/recovery operations must be short and explicit.
3. Autonomous polling/supervisors are lowest priority and must skip cycles if
   the lock is busy.

For your Rust worktree, implement or wrap your test commands so they acquire
this same lock before touching Audio 8 DJ, iRig, Core Audio, USB, VLC, Spotify,
Traktor, or CPU/UI stress. Your build-only and offline unit tests do not need
the lock.

When you need a test window, report:

- desired start time
- expected duration
- whether it plays audio, records iRig, installs/reloads a driver, restarts
  Core Audio/USB services, or changes default devices
- evidence directory where results will be written

Do not run overlapping hardware/audio tests with the C/Obj-C agent. The shared
hardware decides, not the worktree.
```
