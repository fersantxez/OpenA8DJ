# Legacy And Research Branches

The current user-facing OpenA8DJ driver is the macOS line on `main`.

Some older and experimental branches are kept in the repository because they
preserve useful project knowledge. They are not the recommended install path
for normal users.

## Legacy Branch

Our initial approach was a C / Objective-C macOS driver, informed by the open
Linux CAIAQ / `snd-usb-caiaq` work and by early Audio 8 DJ hardware
experiments. That work helped document USB behavior, packet layout, routing,
device startup, and physical-test observations.

The project later moved to the current macOS C++ line on `main`, with a cleaner
Core Audio HAL package, C++ contracts, validation tools, and a clearer path
toward future DriverKit / AudioDriverKit work.

The legacy code remains available on the `legacy` branch for future reference,
comparison, and recovery. It should not be treated as the current driver, and it
should not be copied back into `main` unless a specific behavior is proven to
improve sound quality, routing, stability, timecode behavior, or resource use.

## Rust Lab

The Rust branch is a research lab and oracle. It may help with analyzers, tests,
models, or future design ideas, but it is not the macOS runtime shipped to
users.

## Windows And Linux

Windows and Linux branches are experimental platform research. They are public
for continuity and collaboration, but they are not validated installers and
should not be assumed to work without platform-specific testing.

The macOS GitHub Release remains the supported public download for normal
OpenA8DJ users.
