# OpenA8DJ Project Agents

This directory contains visible project agents that can be launched in separate
worktrees or terminals. These are not hidden subagents. Each agent has explicit
ownership, boundaries, and delivery contracts.

## Active Agents

- `linux-driver-agent/`: owns the Linux ALSA/CAIAQ implementation track,
  Linux configuration tooling, Linux validation ladder, and Linux packaging
  plan.

## Coordination Rules

- macOS remains the priority implementation and must not be destabilized.
- Windows work stays isolated in its Windows branch/worktree.
- Linux work must stay in its Linux branch/worktree.
- Any hardware-affecting action must respect the shared hardware lock described
  in `docs/SHARED_HARDWARE_COORDINATION.md`.
- Build-only, static analysis, and documentation work do not need the hardware
  lock.
- No agent may revert unrelated changes made by another agent.
