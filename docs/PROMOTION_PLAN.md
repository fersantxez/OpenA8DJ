# OpenA8DJ C++ Promotion Plan

Status update: this document is historical. The user authorized promotion after
the 0.4.0 human validation milestone, the C++ line was promoted to local
`main`, and the previous C/Objective-C line was preserved on `legacy`. Keep this
document as the audit model for future large branch moves.

Original status: design only. Do not execute branch moves from this document
until every gate below is PASS and the user explicitly authorizes the promotion
window.

Mandatory warning for every operator or subagent:

> PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorización de ventana.

## Objective

When, and only when, the C++/DriverKit line objectively beats the current C/Obj-C mainline, preserve the current C line as `Legacy` and promote the C++ implementation to `main` without losing history, tags, evidence, rollback capability, or auditability.

This plan does not declare readiness. A clean build, passing offline tests, or one good hardware run is insufficient.

## No ejecutar hasta PASS

Promotion is blocked until all items in this section are PASS and recorded in `docs/TEST_EVIDENCE.md` with command, timestamp, commit hash, result, and evidence path.

1. Isolation PASS
   - No unreviewed edits in `/Users/fer/dev/opena8dj`.
   - No edits in `/Users/fer/dev/audio8djrust`.
   - C++ work exists only in `/Users/fer/dev/audio8djcpp`.
   - `git worktree list` shows distinct paths.

2. Reproducible build PASS
   - Fresh clone/worktree build from committed C++ branch.
   - No local-only generated dependency required for build.
   - Build artifacts reproducible from documented commands in `docs/BUILD.md`.

3. Offline gates PASS
   - Packet pack/unpack tests PASS.
   - Input decode tests PASS.
   - A/B/C/D routing matrix PASS.
   - 8 inputs and 8 outputs represented correctly.
   - 44.1 kHz and 48 kHz policies validated.
   - Timecode vinyl synthetic analyzer PASS.
   - No malloc/lock/logging in the declared real-time hot path, verified by review and test instrumentation where possible.

4. Hardware quality gates PASS under lock
   - Audio 8 DJ playback/capture path validated.
   - iRig capture path visible before test and captured in evidence.
   - 0 underruns and 0 overruns in long run.
   - 0 clipping unless the test intentionally drives clipping.
   - No deck leakage across A/B/C/D.
   - Timecode vinyl profile validates with physical or accepted synthetic-equivalent evidence.
   - Recovery path leaves CoreAudio, USB, Audio 8 DJ, and iRig visible after test.

5. Comparative quality PASS
   - Same signal, same sample rate, same buffer policy, same capture chain compared against C mainline.
   - C++ measured quality is equal or better for noise floor, channel isolation, glitches/clicks, timecode behavior, and long-run stability.
   - Any subjective listening claim has matching objective evidence.

6. Comparative performance PASS
   - CPU <= C mainline under the same scenario.
   - Jitter <= C mainline under the same scenario.
   - Memory footprint <= C mainline or justified by measurable quality/stability gain.
   - No real-time allocation or blocking regression.

7. Rust oracle comparison PASS where applicable
   - C++ meets or exceeds the useful Rust gates for routing, DVS/timecode, pack/decode throughput, and offline quality checks.
   - Any deviation is documented with rationale and evidence.

8. DriverKit readiness PASS
   - Entitlements, signing, install/uninstall, activation/deactivation, and rollback are documented.
   - No automatic install or default-device change is hidden in build/test scripts.
   - Physical test window plan includes lock owner, expected duration, actions, rollback, and evidence directory.

9. Evidence integrity PASS
   - Evidence directory archived with SHA-256 manifest.
   - Candidate commit hash recorded.
   - C baseline commit hash recorded.
   - Tool versions and macOS version recorded.
   - `docs/SUCCESS_METRICS.md`, `docs/TEST_PLAN.md`, and `docs/TEST_EVIDENCE.md` agree.

10. Human approval PASS
   - User explicitly authorizes promotion branch operations after reviewing the evidence bundle.
   - No promotion command runs during hardware testing, reboot recovery, or any unstable USB/CoreAudio state.

## Blocking Conditions

Do not promote if any condition is true:

- Any required gate is FAIL, SKIP, stale, or undocumented.
- Music quality gate has unexplained lag jumps, clicks, dropouts, leakage, low SNR, or alignment failures.
- Timecode vinyl is not validated.
- iRig or capture hardware is intermittently missing.
- CoreAudio/USB recovery requires manual intervention not captured in a safe plan.
- C++ branch contains uncommitted or untracked source changes.
- Mainline branch tip changed after baseline measurement.
- `Legacy` branch already exists and its target commit is not verified.
- Required tags already exist and point to unexpected commits.
- Remote push would require force.
- Any operator proposes rewriting history instead of preserving it.

## Promotion Model

Preferred model: preserve history, no force push.

- `Legacy` is created at the exact C mainline commit that was used for final baseline comparison.
- C++ is promoted to `main` by a normal merge or fast-forward from the validated C++ candidate branch.
- Tags mark all important anchors before and after promotion.
- Evidence is committed or archived before branch movement.
- Rollback is a branch/tag move back to the pre-promotion `main` anchor, never a destructive reset without explicit user approval.

If the C++ branch is not a descendant of the C baseline, stop and design a repository-level import/replace strategy separately. Do not improvise a history rewrite.

## Required Names

Use stable names unless they already exist:

- Legacy branch: `Legacy`
- C++ candidate branch: `driverkit/cpp-redesign`
- Pre-promotion main backup branch: `backup/main-before-cpp-promotion-YYYYMMDD-HHMMSS`
- Promotion integration branch: `promotion/cpp-to-main-YYYYMMDD-HHMMSS`
- C baseline tag: `baseline/c-mainline-before-cpp-promotion-YYYYMMDD`
- C++ candidate tag: `candidate/cpp-driverkit-ready-YYYYMMDD`
- Post-promotion tag: `release/cpp-main-promotion-YYYYMMDD`
- Evidence archive: `evidence/cpp-promotion-YYYYMMDD-HHMMSS.tar.zst`

If any name exists, stop and choose a suffixed name only after recording the collision and target hashes.

## Dry-Run Procedure

These commands are intentionally read-only or simulation-oriented. They may be used to prepare a promotion report, but they do not move branches.

Run from the C++ worktree:

```sh
cd /Users/fer/dev/audio8djcpp
pwd
git rev-parse --show-toplevel
git branch --show-current
git status --porcelain=v1
git log --oneline --decorate -n 20
git tag --list 'baseline/*' 'candidate/*' 'release/*'
git worktree list
```

Verify the mainline and Rust trees are untouched without editing them:

```sh
git -C /Users/fer/dev/opena8dj status --porcelain=v1
git -C /Users/fer/dev/opena8dj branch --show-current
git -C /Users/fer/dev/opena8dj rev-parse HEAD
git -C /Users/fer/dev/audio8djrust status --porcelain=v1
git -C /Users/fer/dev/audio8djrust branch --show-current
git -C /Users/fer/dev/audio8djrust rev-parse HEAD
```

Inspect remote state without mutating local branches:

```sh
git ls-remote --heads origin main Legacy driverkit/cpp-redesign
git ls-remote --tags origin 'baseline/*' 'candidate/*' 'release/*'
git fetch --dry-run --all --prune --tags
```

Verify ancestry assumptions:

```sh
C_BASELINE="$(git -C /Users/fer/dev/opena8dj rev-parse HEAD)"
CPP_HEAD="$(git -C /Users/fer/dev/audio8djcpp rev-parse HEAD)"
echo "C_BASELINE=$C_BASELINE"
echo "CPP_HEAD=$CPP_HEAD"
git -C /Users/fer/dev/audio8djcpp merge-base --is-ancestor "$C_BASELINE" "$CPP_HEAD"
echo "ancestor_status=$?"
git -C /Users/fer/dev/audio8djcpp merge-base "$C_BASELINE" "$CPP_HEAD"
```

Simulate merge conflicts on a disposable dry-run branch inside the C++ worktree only:

```sh
cd /Users/fer/dev/audio8djcpp
git switch --detach "$C_BASELINE"
git switch -c dry-run/cpp-promotion-YYYYMMDD-HHMMSS
git merge --no-commit --no-ff "$CPP_HEAD"
git diff --stat
git merge --abort
git switch driverkit/cpp-redesign
git branch -D dry-run/cpp-promotion-YYYYMMDD-HHMMSS
```

If this dry-run reports conflicts, stop. Resolve the promotion strategy in a separate reviewed branch. Do not touch `main`.

## Evidence Archive Procedure

Before any real branch movement, prepare an immutable evidence bundle from the C++ worktree:

```sh
cd /Users/fer/dev/audio8djcpp
mkdir -p evidence
command -v zstd
tar --exclude='.git' \
  -cf - \
  docs/SUCCESS_METRICS.md \
  docs/TEST_PLAN.md \
  docs/TEST_EVIDENCE.md \
  docs/ARCHITECT_CONTEXT.md \
  docs/DECISION_LOG.md \
  docs/OFFLINE_READINESS_REPORT.md \
  docs/PHYSICAL_TEST_WINDOW_PLAN.md \
  local-analysis \
  | zstd -T0 -19 -o "evidence/cpp-promotion-YYYYMMDD-HHMMSS.tar.zst"
shasum -a 256 "evidence/cpp-promotion-YYYYMMDD-HHMMSS.tar.zst" \
  > "evidence/cpp-promotion-YYYYMMDD-HHMMSS.sha256"
```

If the evidence directory is too large for git, store it in the agreed artifact location and commit only the manifest, checksums, and summary.

## Real Promotion Procedure

Do not run this section until `No ejecutar hasta PASS` is fully satisfied and the user explicitly authorizes promotion.

1. Freeze repository state.

```sh
cd /Users/fer/dev/audio8djcpp
git fetch --all --prune --tags
git status --porcelain=v1
git -C /Users/fer/dev/opena8dj status --porcelain=v1
git -C /Users/fer/dev/audio8djrust status --porcelain=v1
```

Expected result: C++ branch clean except intentional committed promotion docs; mainline and Rust unchanged.

2. Record immutable anchors.

```sh
C_BASELINE="$(git -C /Users/fer/dev/opena8dj rev-parse HEAD)"
CPP_HEAD="$(git -C /Users/fer/dev/audio8djcpp rev-parse HEAD)"
MAIN_REMOTE="$(git ls-remote --heads origin main | awk '{print $1}')"
echo "C_BASELINE=$C_BASELINE"
echo "CPP_HEAD=$CPP_HEAD"
echo "MAIN_REMOTE=$MAIN_REMOTE"
```

`C_BASELINE` must equal the baseline commit used for final C comparison. If not, stop.

3. Create protection anchors.

```sh
git branch "backup/main-before-cpp-promotion-YYYYMMDD-HHMMSS" "$C_BASELINE"
git branch Legacy "$C_BASELINE"
git tag -a "baseline/c-mainline-before-cpp-promotion-YYYYMMDD" "$C_BASELINE" \
  -m "C mainline baseline before C++ DriverKit promotion"
git tag -a "candidate/cpp-driverkit-ready-YYYYMMDD" "$CPP_HEAD" \
  -m "Validated C++ DriverKit candidate before main promotion"
```

If any branch or tag already exists, stop and inspect it. Do not overwrite.

4. Promote through an integration branch.

```sh
git switch -c "promotion/cpp-to-main-YYYYMMDD-HHMMSS" "$C_BASELINE"
git merge --no-ff "$CPP_HEAD" \
  -m "Promote validated C++ DriverKit implementation to main"
```

If conflicts occur, abort and return to design review:

```sh
git merge --abort
git switch driverkit/cpp-redesign
```

5. Re-run minimum promotion verification from the integration branch.

```sh
./scripts/run-cpp-offline-gates
```

Expected result: PASS with evidence appended. If FAIL, stop and do not update `main`.

6. Move `main` only after the integration branch passes.

```sh
git switch main
git merge --ff-only "promotion/cpp-to-main-YYYYMMDD-HHMMSS"
git tag -a "release/cpp-main-promotion-YYYYMMDD" HEAD \
  -m "C++ DriverKit promoted to main after objective PASS gates"
```

If `--ff-only` fails, stop. Do not force.

7. Push safely.

```sh
git push origin Legacy
git push origin "backup/main-before-cpp-promotion-YYYYMMDD-HHMMSS"
git push origin main
git push origin "baseline/c-mainline-before-cpp-promotion-YYYYMMDD"
git push origin "candidate/cpp-driverkit-ready-YYYYMMDD"
git push origin "release/cpp-main-promotion-YYYYMMDD"
```

Never use `git push --force` for this promotion.

## Rollback Plan

Rollback trigger examples:

- Post-promotion build fails from clean clone.
- Hardware quality regresses versus the archived C baseline.
- Timecode vinyl fails.
- CoreAudio/USB recovery becomes unsafe.
- User listening rejects the candidate and objective evidence does not explain it.

Rollback procedure:

```sh
cd /Users/fer/dev/audio8djcpp
git fetch --all --prune --tags
ROLLBACK_TARGET="$(git rev-parse baseline/c-mainline-before-cpp-promotion-YYYYMMDD)"
git switch -c "rollback/main-to-legacy-YYYYMMDD-HHMMSS" main
git revert --no-commit "$ROLLBACK_TARGET"..HEAD
git commit -m "Rollback C++ promotion to C baseline behavior"
./scripts/run-cpp-offline-gates || true
```

Preferred rollback is a revert commit reviewed and pushed normally. Only use branch reset or force push if the user explicitly authorizes emergency repository surgery and every affected remote branch is backed up by tags.

## Audit Checklist

Before real execution, a human-readable promotion report must list:

- C baseline branch, commit, tag, and evidence path.
- C++ candidate branch, commit, tag, and evidence path.
- Exact comparison scenarios and results.
- Hardware lock window used for final physical gates.
- iRig and Audio 8 DJ device identities observed before and after tests.
- CPU, jitter, underrun, overrun, SNR/noise, routing, leakage, and timecode results.
- Known residual risks.
- Rollback branch and tag names.
- Confirmation that no force push or history rewrite is planned.

## Current Readiness Statement

Not ready for promotion. This document defines the safe future procedure only. The C++ line must still prove better quality, complete functionality including timecode vinyl, and equal-or-better performance/resource use against the C mainline with reproducible evidence before any branch promotion is allowed.
