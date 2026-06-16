# Physical Test Window Plan

Status: not requested, not executed.

This plan is the earliest safe path to physical validation after offline gates
pass. It does not authorize hardware use by itself.

## Lock

Before any physical or system-sensitive action:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

Acquire the global hardware gate before playback, capture, CoreAudio changes,
USB access, DriverKit install/activation, service restart, or Traktor testing.

If the lock is occupied:

- do not run the test;
- do not wait while holding the lock;
- report the owner from `$AUDIO_GATE_LOCK_ROOT/owner`;
- only clean the lock if the owner pid is dead.

## Proposed Window

- Duration: 45-60 minutes.
- Plays audio: yes, only after explicit authorization.
- Records iRig: yes, if the known analog route is available.
- Installs/reloads driver: blocked until DriverKit SDK/signing is available and user explicitly approves.
- Restarts CoreAudio/USB services: no for first physical window.
- Changes default devices: no for first physical window.
- Evidence directory: `/Users/fer/dev/audio8djcpp/local-analysis/physical-window/<timestamp>/`

## Entry Criteria

- `scripts/run-cpp-offline-gates` passes.
- Candidate source diff is frozen or committed.
- DriverKit toolchain blocker is resolved if the test includes a real dext.
- Rollback target is named.
- User grants an explicit coordinated window.

## First Physical Window Scope

1. Confirm lock ownership and record environment.
2. Record exact candidate hash and build artifacts.
3. If no dext is available, run only external comparison and planning; do not install.
4. If a signed/authorized dext exists in a later environment, activate it only under user-approved window.
5. Validate device visibility without changing defaults.
6. Validate A/B/C/D output isolation.
7. Validate 8-input representation.
8. Validate timecode vinyl profile routing.
9. Capture analog output through the approved iRig path.
10. Save metrics, raw captures, and human listening notes.

## Next Minimal Physical Test

Status: blocked until the global hardware lock is free.

Purpose:

- Separate startup/cadence failure from persistent analog/capture residual.
- Reuse the same physical route and fixture as the current failed evidence.

Preconditions:

- `iRig Stream` visible in CoreAudio.
- `Open Audio 8 DJ` visible in CoreAudio with `8 in / 8 out`.
- Global hardware lock acquired by this C++ worktree.
- No mainline autonomous QA process owns the lock.

Command shape:

```sh
scripts/run-soundcheck \
  --capture-device "iRig Stream" \
  --capture-channels 1,2 \
  --pair A \
  --rate 48000 \
  --buffer 512 \
  --seconds 24 \
  --mode dense \
  --target-peak-db -12 \
  --max-lag 360000 \
  --stream-stats-snapshots \
  --run-dir local-analysis/soundcheck/<timestamp>-irig-pairA-24s-startup-discard
```

Additional offline analysis after capture:

```sh
scripts/analyze-soundcheck-window-trace.py \
  local-analysis/soundcheck/<timestamp>-irig-pairA-24s-startup-discard \
  --json-out local-analysis/soundcheck/<timestamp>-irig-pairA-24s-startup-discard/window-trace.json
```

Decision criteria:

- If stable-window lag jumps disappear and residual improves materially after
  startup discard, the main issue is harness/startup/cadence.
- If lag jumps persist, prioritize transport/timeline scheduling.
- If lag correction improves residual by only a few percent again, prioritize
  analog/capture path, gain staging, broadband coloration, or hidden routing
  contamination.
- Do not promote even on improvement unless CPU and Traktor/timecode gates also
  pass.

## Stop Conditions

- Lock unavailable.
- Unexpected CoreAudio default-device change would be required.
- Install/activation asks for unplanned permissions.
- Any channel swap, white noise, pop/click burst, underrun/overrun spike, or device disappearance.
- Any evidence path would be outside `/Users/fer/dev/audio8djcpp`.

## Readiness Claim Rule

Passing this plan is still not enough to claim "better than mainline" unless the
candidate beats or matches mainline C and Rust oracle metrics, physical capture
quality, Traktor/timecode behavior, and human listening results with preserved
evidence.
