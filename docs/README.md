# OpenA8DJ Documentation

Start here if you want to use OpenA8DJ, understand how it works, or continue
the project.

OpenA8DJ is an independent open-source preservation project for the Native
Instruments Audio 8 DJ. The current public driver line is the macOS C++ line on
`main`.

This documentation is organized for human readers first. The main `docs`
folders explain installation, architecture, validation, release policy, and
contribution workflows. Raw handoffs and machine-readable continuity notes are
kept outside this tree in [`docs-state`](../docs-state/README.md).

## If You Just Want To Use It

Use these pages in order:

1. [Quick start](user/quick-start.md)
2. [Install on macOS](user/install.md)
3. [Traktor and Timecode Vinyl](user/traktor-timecode.md)
4. [Control Center](user/control-center.md)
5. [Troubleshooting](user/troubleshooting.md)
6. [Uninstall](user/uninstall.md)

## If You Want To Understand The Project

Read these pages when you want to know what OpenA8DJ is doing and why:

- [Project documentation index](project/README.md)
- [Architecture](project/architecture.md)
- [Hardware model](project/hardware.md)
- [Cable and routing options](project/cabling.md)
- [Use cases](project/use-cases.md)
- [Platform support](project/platform-support.md)
- [Roadmap](project/roadmap.md)

## If You Want To Contribute

Start with the high-level contribution guide, then follow the build and
validation documents for the kind of change you want to make:

- [Contributing guide](project/contributing.md)
- [Build from source](project/build.md)
- [Testing and validation](project/validation/testing-and-validation.md)
- [Measurement methodology](project/validation/measurement-methodology.md)
- [Test plan](project/validation/test-plan.md)
- [Success metrics](project/validation/success-metrics.md)
- [Release process](project/release.md)

## If You Want Release Or Legal Details

- [Public validation summary](project/public-validation-summary.md)
- [Legal and publication policy](reference/legal.md)
- [OpenA8DJ 0.5.0 release notes](reference/release-notes-0.5.0.md)
- [OpenA8DJ 0.4.0 historical release notes](reference/release-notes-0.4.0.md)

## How The Folders Are Organized

- `user/`: non-technical install, daily use, Traktor, Control Center, and
  troubleshooting guides.
- `project/`: human-readable project documentation for architecture,
  hardware behavior, cabling, use cases, validation, building, releasing, and
  contributing.
- `project/validation/`: quality gates, sound-check methodology, and the test
  plan used to decide whether a build is ready for people.
- `reference/`: stable reference material such as release notes and legal
  policy.

## Project State

Handoffs, raw test evidence, internal investigations, date-stamped runbooks,
and notarization state live in [docs-state](../docs-state/README.md). Those files
are preserved because they are useful for maintenance and future agents, but
they may contain old wording, rejected experiments, local paths, and internal
process details.
