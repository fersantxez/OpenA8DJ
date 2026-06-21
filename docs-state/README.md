# Project State And Memories

This directory is for maintainers and future LLM agents. It preserves continuity
material that is useful for resuming work, auditing decisions, or reconstructing
why a release was made.

It is intentionally not the public documentation path. If you are trying to use
OpenA8DJ or understand the project for the first time, start here instead:

- [Documentation index](../docs/README.md)
- [Project documentation](../docs/project/README.md)
- [Architecture](../docs/project/architecture.md)
- [Testing and validation](../docs/project/validation/testing-and-validation.md)

## What Belongs Here

- handoffs between agents or maintainers;
- raw test evidence;
- rejected experiments;
- date-stamped runbooks;
- signing and notarization state;
- branch-state notes;
- internal investigation notes;
- local continuity notes that should not shape the first-read user experience.

## How To Read This Directory

Treat these files as evidence, not polished product documentation. They may
contain old paths, internal build names, obsolete release wording, local machine
details, or experiments that were rejected later.

When a memory file contains information that users or contributors need, rewrite
that information into a clean human-readable page under `docs/user`,
`docs/project`, or `docs/reference` instead of linking normal readers here.

## What Does Not Belong Here

- normal install instructions;
- consumer troubleshooting;
- public release notes;
- architecture explanations intended for new contributors;
- contribution guides;
- release claims that have not been reflected in public docs.

Those belong in `docs/user`, `docs/project`, or `docs/reference`.
