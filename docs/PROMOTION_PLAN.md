# C++ Mainline Promotion

Status: complete for OpenA8DJ 0.4.0.

The C++/DriverKit redesign line has been promoted to the repository `main`
branch. The previous C/Objective-C implementation has been preserved on the
`legacy` branch.

## Completed Branch Layout

| Ref | Meaning |
|---|---|
| `main` | Current macOS C++/DriverKit product line. |
| `driverkit/cpp-redesign` | Parallel development branch for the same modern line. |
| `legacy` | Previous C/Objective-C branch, retained as historical baseline. |
| `v0.4.0` | Public preview release with DMG/PKG/checksum assets. |

## Current Policy

- Do not treat the old C line as `main`.
- Do not delete `legacy`; it is the baseline and recovery reference.
- Do not port legacy C behavior into `main` unless evidence shows it improves
  sound quality, routing, stability, Timecode Vinyl behavior, or resource use.
- Keep public installation guidance pointed at GitHub Releases and the macOS
  DMG/PKG flow.
- Keep future DriverKit/AudioDriverKit work in the modern macOS line.

## Future Large Branch Moves

Any future branch replacement, rollback, or destructive cleanup still requires:

- a clean git audit;
- written evidence path;
- exact source and destination refs;
- rollback plan;
- explicit user authorization.

Historical pre-promotion evidence remains in append-only logs such as
`docs/TEST_EVIDENCE.md`, `docs/DECISION_LOG.md`, and `docs/AGENT_HANDOFFS.md`.
Those dated entries are retained for auditability and do not override the
current branch map above.

