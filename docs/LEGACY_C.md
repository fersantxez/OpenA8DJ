# Legacy C Branch

The previous C/Objective-C OpenA8DJ implementation is preserved on the
`legacy` branch.

That branch is not deleted and should not be treated as abandoned data. It is
the historical reference for:

- the earlier C HAL implementation;
- Linux/CAIAQ-derived USB behavior and packet handling learnings;
- physical-test results from the 0.3.x line;
- recovery scripts and operator knowledge;
- baseline comparison when validating future C++ changes.

The current user-facing driver line is `main`, starting with OpenA8DJ 0.4.0.
That line is the modern macOS C++ architecture and the release line users
should download from GitHub Releases.

Do not port legacy code blindly into `main`. Preserve useful behavior only when
evidence shows it improves sound quality, routing, stability, timecode behavior,
or resource use.
