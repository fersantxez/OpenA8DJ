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
- Installs/reloads driver: HAL candidate only, through
  `scripts/test-hal-candidate-safety` and only when
  `scripts/run-physical-superiority-window --execute` is explicitly used.
  DriverKit/dext activation remains out of scope.
- Restarts CoreAudio/USB services: CoreAudio restart is expected only as part
  of HAL candidate safety/recovery; USB reset remains forbidden.
- Changes default devices: no for first physical window.
- Evidence directory: `/Users/fer/dev/audio8djcpp/local-analysis/physical-superiority-window/<timestamp>/`

## Entry Criteria

- `scripts/run-cpp-offline-gates` passes.
- Candidate source diff is frozen or committed.
- Runtime quiescence has been checked before installing/restoring anything:
  `scripts/runtime-isolation-audit --expect-hal inactive`.
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

Status: not executed.

Purpose:

- Revalidate the physical capture route before judging the Audio 8 DJ driver.
- Then collect candidate music quality, CPU, stream, and native WAV evidence in
  one evidence directory.

Preconditions:

- Mainline OpenA8DJ LaunchAgents are disabled and no mainline QA process is
  running.
- A HAL candidate bundle exists at `build/OpenA8DJ.driver` or another explicit
  path.
- `scripts/runtime-isolation-audit --expect-hal inactive` passes before HAL
  candidate safety, then `Open Audio 8 DJ` is verified after the candidate is
  loaded by the safety gate.
- `iRig Stream` visible in CoreAudio.
- `Open Audio 8 DJ` visible in CoreAudio with `8 in / 8 out`.
- Global hardware lock acquired by this C++ worktree.
- No mainline autonomous QA process owns the lock.
- A real non-Audio8 known-good output is physically routed into the same iRig
  capture chain.

Command shape:

```sh
scripts/run-physical-superiority-window \
  --execute \
  --mainline-candidate /absolute/path/to/mainline/OpenA8DJ.driver \
  --candidate build/OpenA8DJ.driver \
  --known-good-output-device "<non-Audio8 output>" \
  --capture-device "iRig Stream" \
  --capture-channels 1,2 \
  --reference-wav /absolute/path/to/reference.wav \
  --music-file /absolute/path/to/music.wav \
  --pair A \
  --seconds 12 \
  --run-dir local-analysis/physical-superiority-window/<timestamp>
```

Additional offline analysis after capture:

```sh
scripts/analyze-soundcheck-window-trace.py \
  local-analysis/physical-superiority-window/<timestamp>/cpp-soundcheck \
  --json-out local-analysis/physical-superiority-window/<timestamp>/cpp-soundcheck/window-trace.json
```

Decision criteria:

- `physical-window-preflight.json` must pass before the lock is acquired.
  This proves only that devices/files are currently visible and the lock is
  free; it does not prove the route.
- The run is blocked unless the same window captures both the read-only
  mainline HAL candidate and the C++ HAL candidate through the same iRig route.
- C++ must pass `same-session-physical-compare.json` against mainline before
  any superiority or branch-promotion claim is allowed.
- The known-good non-Audio8 route check is mandatory for a successful physical
  superiority window. `--skip-known-good` is only a diagnostic escape hatch and
  keeps the runner blocked.
- The promotion evaluator must consume the same window's
  `same-session-physical-compare.json`; fixed historical references and stale
  physical windows are not promotion evidence.
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
