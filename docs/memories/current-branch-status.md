# Current Branch Status

Date: 2026-06-19

## Canonical State

`main` is the OpenA8DJ macOS driver line. It is the branch users should see
first when they want to download, build, install, or understand the current
driver.

The current product architecture is the macOS C++ stack:

- Core Audio HAL plug-in for installation today
- IOUSBHost transport for the Audio 8 DJ
- CoreMIDI bridge
- C++ core contracts and offline gates
- DriverKit/AudioDriverKit scaffolding as the forward System Extension path
- support tools and Control Center for profile/configuration workflows

## Preserved Branches

`legacy` preserves the previous C/Objective-C implementation as historical
reference material.

Rust work is kept separately as an experimental lab/oracle. It may inform tests
or future implementation choices, but it is not part of the macOS `main` runtime.

Windows and Linux work should stay isolated in platform-specific branches and
paths until those platforms have their own tested installers and validation
evidence.

## What Belongs In Main

- macOS C++ driver source
- HAL installer path and packaging
- DriverKit/AudioDriverKit forward scaffolding
- current quality, routing, timecode, and performance gates
- macOS support/configuration tools
- docs for current macOS installation and experimental platform status

## What Does Not Belong In Main

- full legacy C implementation history
- Rust runtime experiments
- Windows or Linux driver code that can interfere with macOS build/release
- hardware tests that assume exclusive access without the project hardware lock

## Important Clarification

The current macOS branch still contains `.c`, `.m`, and `.mm` files. That is
expected for Core Audio HAL, CoreMIDI, IOUSBHost, and bridge code. These files
are part of the modern macOS product line. They do not mean the old C branch is
still the active driver architecture.
