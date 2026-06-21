# Project Documentation

These pages explain how OpenA8DJ works, how it is built, how quality is checked,
and how someone can continue the project.

If you are new to the repository, read the first section before jumping into
build commands. It explains the hardware model, the macOS driver architecture,
and the tradeoffs behind the current 0.5.x line.

## Product And Architecture

- [Architecture](architecture.md): the current macOS C++ driver line and the
  boundaries between HAL, core audio logic, tools, and future DriverKit work.
- [Hardware model](hardware.md): Audio 8 DJ channels, deck pairs, input modes,
  MIDI, and the assumptions the driver makes.
- [Cable and routing options](cabling.md): common DJ, DVS, recording, and
  testing setups.
- [Use cases](use-cases.md): practical workflows the project aims to support.
- [Platform support](platform-support.md): what is public, experimental, or
  historical across macOS, Windows, Linux, Rust, and DriverKit work.
- [Roadmap](roadmap.md): the next engineering goals and remaining validation
  gaps.

## Contributing

- [Contributing guide](contributing.md): what kinds of contributions help, how
  to keep the project legally clean, and which quality gates apply.
- [Build from source](build.md): local build commands and artifact layout.
- [Control tools](tools/control-tools.md): Control Center and command-line tool
  behavior for support and diagnostics.

## Validation And Quality

OpenA8DJ treats sound quality as part of correctness. These pages explain how
builds are checked before they are offered to users:

- [Public validation summary](public-validation-summary.md)
- [Testing and validation](validation/testing-and-validation.md)
- [Measurement methodology](validation/measurement-methodology.md)
- [Test plan](validation/test-plan.md)
- [Success metrics](validation/success-metrics.md)
- [Testing notes](validation/testing.md)

## Release And Reference

- [Release process](release.md)
- [Legal and publication policy](../reference/legal.md)
- [0.5.0 release notes](../reference/release-notes-0.5.0.md)
- [0.4.0 historical release notes](../reference/release-notes-0.4.0.md)

## Design Notes

- [Real-time design](design/realtime-design.md)
- [DriverKit notes](design/driverkit.md)

## Legacy And Research Branches

The public release line is macOS C++ on `main`. Historical and experimental
work is documented separately in
[Legacy and research branches](legacy-and-research-branches.md).

## Maintainer State

Raw transfer notes, detailed logs, rejected experiments, local paths, and
notarization state live in [project state](../../docs-state/README.md). They are
kept for continuity, not as the normal reading path.
