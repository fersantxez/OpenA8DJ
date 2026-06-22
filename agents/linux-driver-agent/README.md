# Linux Driver Agent

This is the visible project agent for the OpenA8DJ Linux implementation.

It is meant to be launched as a separate Codex/session/worktree, not as an
invisible subagent of another assistant.

Start here:

1. Read `PROMPT.md`.
2. Read `IMPLEMENTATION_CONTRACT.md`.
3. Create or enter the Linux worktree.
4. Implement the Linux driver track without touching macOS or Windows
   implementation files unless an explicit adapter document is needed.

Bootstrap helper:

```sh
./scripts/bootstrap-linux-driver-agent
```

The helper prints the intended branch/worktree setup. It does not touch
hardware.
