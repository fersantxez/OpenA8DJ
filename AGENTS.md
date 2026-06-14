# Rust Parallel Experiment Rules

This worktree is the isolated Rust/modular-core experiment for OpenA8DJ.

## Absolute boundary

- The main implementation worktree at `/Users/fer/dev/opena8dj` is read-only for this experiment.
- Agents working here may inspect `/Users/fer/dev/opena8dj` with read-only commands such as `rg`, `sed`, `git show`, `git diff`, and `git log`.
- Agents working here must not edit, format, generate files into, install from, clean, reset, or otherwise mutate `/Users/fer/dev/opena8dj`.
- Do not run commands from this worktree that install or replace the active OpenA8DJ HAL driver unless the user explicitly asks for a Rust experiment install.

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

> You are working in `/Users/fer/dev/audio8djrust` on branch `rust/modular-core-spike`. `/Users/fer/dev/opena8dj` is strictly read-only reference material. It is forbidden to modify, format, install from, clean, reset, or generate files into the main worktree.
