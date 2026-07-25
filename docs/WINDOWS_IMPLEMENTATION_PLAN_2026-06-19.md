# Windows Implementation Plan - 2026-06-19

Branch: `windows/rebuild-surface`

This branch is for Windows-only work. The absolute priority is that the macOS
driver line remains unaffected. Do not edit macOS HAL behavior, macOS build
flags, macOS tools, packaging, soundcheck gates, or installed-driver workflows
from this branch.

Allowed surfaces for this branch:

- `windows/`
- Windows-specific documentation under `docs/`
- CI/build files only when they are Windows-only

## Invariant

Windows work must never make a macOS build, install, soundcheck, or candidate
handoff riskier. If a change needs shared code, stop and design an adapter
boundary first.

## Shared Hardware Lock

The Audio 8 DJ, iRig Stream capture route, Core Audio stack, USB bus, and CPU
budget are shared with the main macOS implementation and the macOS Control
Center work. Windows work must be generous and respectful with this hardware.

Do not touch the physical Audio 8 DJ unless the shared lock is free and the
Windows task has acquired it. Release the lock as soon as the hardware action is
finished, including after failures.

Required lock root:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

Before any hardware-affecting action:

```sh
./scripts/shared-hardware-lock-status
```

For commands that touch hardware, use:

```sh
./scripts/shared-hardware-lock-run \
  --gate windows-<short-test-name> \
  --run-dir local-analysis/windows/<short-test-name>-$(date +%Y%m%d-%H%M%S) \
  -- <command>
```

Actions that require the lock:

- installing, uninstalling, loading, or replacing any driver that can claim
  Audio 8 DJ;
- running Windows hardware tests against the physical Audio 8 DJ;
- running `opena8djctl` against a live device;
- claiming or resetting USB;
- changing sample rate, buffer size, input mode, ground-lift, software lock, or
  any other hardware state;
- playback, capture, Traktor, VLC, Spotify, DAW, MIDI loopback, or physical
  quality gates;
- restarting Core Audio, USB audio services, USB power services, or anything
  that can affect the macOS side's current measurement.

Actions that do not need the lock:

- editing code;
- reading documentation;
- building Windows artifacts without installing/loading them;
- static analysis;
- endpoint skeleton tests that do not touch Audio 8 DJ, iRig, Core Audio, USB,
  or CPU/audio-sensitive measurement windows.

If the lock is busy, wait or skip. Do not clear the lock unless
`shared-hardware-lock-status` proves the owner is stale and the owner pid is
dead.

## Phase 1: rebuild the Windows surface

Status: started in this branch.

Goals:

- Make the Windows API versioned.
- Stop pretending that streaming exists before the isochronous engine exists.
- Expose explicit component states: USB, controls, audio endpoints, isochronous
  engine, MIDI, ASIO.
- Expose planned topology for A/B/C/D input/output pairs.
- Expose diagnostics counters that future streaming work can fill.

Implemented first pass:

- `OPENA8DJ_DRIVER_API_VERSION` moved to `2`.
- New IOCTLs:
  - `IOCTL_OPENA8DJ_GET_SURFACE`
  - `IOCTL_OPENA8DJ_GET_TOPOLOGY`
  - `IOCTL_OPENA8DJ_GET_DIAGNOSTICS`
- `opena8djctl surface`, `topology`, and `diagnostics`.
- `opena8djctl start` now reaches a driver path that rejects start until a real
  isochronous engine exists.

Exit criteria:

- Builds with WDK on Windows.
- `opena8djctl status/surface/topology/diagnostics` work on a test machine.
- `start` is rejected honestly.
- No macOS files changed by the Windows surface work.

## Phase 2: ACX endpoint skeleton

Goal: publish Windows audio endpoints without touching the USB hardware stream.

Tasks:

- Audit Microsoft ACX samples.
- Add a separate ACX prototype project or subproject.
- Publish a render/capture endpoint with deterministic silence/tone.
- Validate WASAPI shared/exclusive enumeration.
- Capture ETW/WPA traces.

Exit criteria:

- Windows sees real audio endpoints.
- Endpoint skeleton can stream deterministic audio without Audio 8 DJ attached.
- No OpenA8DJ USB streaming yet.

## Phase 3: CAIAQ isochronous engine

Goal: implement real USB audio transport behind the Windows surface.

Tasks:

- Preallocate IN/OUT request pools.
- Keep capture-paced OUT as the first hardware model.
- Track packet status, queue depth, late completions, packet errors, underruns,
  and overruns.
- Start with 48 kHz / 512 frames / Output A/B.

Exit criteria:

- Physical 1 kHz output captured externally.
- No fake streaming state.
- Diagnostics counters move from stub to real values.

## Phase 4: full topology and MIDI

Tasks:

- Decide between one 8-channel endpoint and four stereo endpoint pairs.
- Expose all A/B/C/D outputs and inputs.
- Add MIDI in/out.
- Validate control profile changes.

Exit criteria:

- Traktor routing can address the expected decks.
- MIDI loopback works.
- Full-duplex audio does not degrade output.

## Phase 5: ASIO and release package

Tasks:

- Design ASIO as a facade over the same streaming engine.
- Keep Steinberg SDK licensing separate.
- Add signed driver package flow.
- Build MSI/bootstrapper only after driver package validation.

Exit criteria:

- ASIO improves pro workflows without becoming the only working path.
- Signed package installs, uninstalls, hotplugs, sleeps/wakes, and reboots
  cleanly.
