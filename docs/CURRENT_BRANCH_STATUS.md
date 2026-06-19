# Current Branch Status

Date: 2026-06-19

## Canonical State

`main` is the current OpenA8DJ macOS driver line. It is the branch users should
see first when they want to download, build, or understand the current driver.
It contains the macOS HAL installer path, the C++ core contracts, and the
DriverKit/AudioDriverKit forward shell.

`legacy` is the preserved previous C/Objective-C implementation. It contains
the historical branch that was based on and inspired by Linux CAIAQ /
`snd-usb-caiaq` reverse-engineering work and the 0.3.x physical-test history.

## Verified Remote Refs

| Ref | Current role |
|---|---|
| `origin/main` | Current public default branch and macOS C++ line. |
| `origin/driverkit/cpp-redesign` | Same modern C++/DriverKit development line. |
| `origin/legacy` | Preserved previous C/Objective-C line. |
| `v0.4.0` | Public release tag for OpenA8DJ 0.4.0. |

## What Belongs In Main

- Modern macOS installation and packaging docs.
- Core Audio HAL implementation for the current installable preview.
- C++ core/audio/transport contracts.
- DriverKit/AudioDriverKit scaffolding and readiness notes.
- Current quality, routing, timecode, and performance gates.
- Historical evidence only when clearly marked as dated audit material.

## What Belongs In Legacy

- The previous C/Objective-C product branch.
- Old Linux/CAIAQ-derived runtime architecture.
- 0.3.x branch behavior and emergency reference state.

## Important Clarification

The current `main` branch still contains some `.c`, `.m`, and `.mm` files. That
is expected for the macOS HAL plug-in, CoreAudio/CoreMIDI tooling, and bridge
code. These files are part of the modern macOS C++/DriverKit product line. They
do not mean the old C mainline is still active.
