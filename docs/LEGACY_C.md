# Legacy C Branch

The previous C/Objective-C OpenA8DJ implementation is preserved on the
`legacy` branch.

That branch is a historical reference, not the current user-facing driver. It
keeps useful project memory:

- earlier C HAL implementation work;
- Linux/CAIAQ-derived USB behavior and packet-handling learnings;
- physical-test results from the 0.3.x line;
- recovery scripts and operator knowledge;
- comparison material for validating future changes.

The current user-facing driver is the macOS C++ line on `main`, beginning with
the 0.5.x baseline.

Do not port legacy code blindly into `main`. Preserve behavior only when
evidence shows it improves sound quality, routing, stability, timecode behavior,
or resource use.
