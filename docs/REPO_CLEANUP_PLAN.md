# Repository Cleanup Plan

Date: 2026-06-19

## Target State

`main` is the canonical OpenA8DJ macOS C++ driver line.

The repository should read like a clean open-source preservation project:

- macOS C++ is the primary product line.
- DriverKit/AudioDriverKit is the forward Apple System Extension path.
- the older C/Objective-C implementation is preserved on `legacy`.
- Rust remains an isolated experimental lab/oracle.
- Windows and Linux are experimental platform areas, clearly marked as
  unvalidated until they have their own install/test evidence.

## Non-Goals

- Do not merge the full legacy C branch into `main`.
- Do not merge Rust runtime experiments into `main`.
- Do not merge Windows or Linux branches wholesale into `main`.
- Do not publish a driver artifact as user-ready unless the exact artifact has
  passed the appropriate build/package gates and sound-quality validation.

## Main Branch Contents

`main` may contain:

- macOS HAL driver source and packaging
- C++ core contracts, offline gates, and DriverKit/AudioDriverKit scaffolding
- macOS support tools and Control Center
- public docs for install, Traktor/timecode, routing, tools, and release
- platform-status docs for Windows/Linux experimental work

`main` should not contain platform code that can interfere with macOS build,
packaging, install, or release.

## Platform Isolation

Windows work belongs in Windows-specific branches and paths. It should be
imported into `main` only when it is path-isolated and cannot affect macOS.

Linux work belongs in Linux-specific branches and paths. Historical CAIAQ/Linux
knowledge can be referenced, but Linux runtime work should not be merged into
the macOS release path.

Rust work belongs in Rust-specific branches. Ideas can be ported manually to the
C++ core only when measured evidence shows they improve quality, latency,
resource use, complexity, or test coverage.

## Release Cleanup Checklist

Before publishing a macOS release:

1. `README.md` presents macOS C++ as the primary driver.
2. `docs/INSTALL.md` matches the downloadable artifacts.
3. `resources/dmg/README.txt` matches the release version.
4. `resources/OpenA8DJ.driver/Contents/Info.plist` matches the release version.
5. `make dist` produces driver and tools artifacts.
6. `build/OpenA8DJ-<version>-checksums.txt` includes every public artifact.
7. Signing/notarization status is stated plainly.
8. GitHub release notes match the actual artifacts.
9. Hardware/sound-quality validation status is recorded honestly.

## Promotion Policy

The 0.5.x macOS C++ line may be promoted to `main` after:

- repository documentation no longer treats C++ as a secondary or transitional line;
- release artifacts are generated from the 0.5 stable configuration;
- optional tools build and package cleanly;
- stale 0.4 current-release references are removed from live docs;
- `legacy`, Rust, Windows, and Linux are clearly isolated.
