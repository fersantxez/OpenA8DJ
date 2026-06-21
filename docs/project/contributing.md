# Contributing To OpenA8DJ

OpenA8DJ is an independent preservation project for the Native Instruments
Audio 8 DJ. Contributions are welcome when they help keep the hardware useful
and keep the project legally and technically clean.

## Good First Contributions

- clearer install or troubleshooting instructions;
- reports from real Audio 8 DJ hardware with macOS version, cable routing,
  audio app, sample rate, and exact OpenA8DJ version;
- Traktor and Timecode Vinyl validation notes;
- routing, MIDI, and Control Center bug reports;
- small build or test fixes;
- improvements to measurement and documentation.

## Before Changing Code

Read these pages first:

1. [Architecture](architecture.md)
2. [Hardware model](hardware.md)
3. [Testing and validation](validation/testing-and-validation.md)
4. [Build from source](build.md)
5. [Legal and publication policy](../reference/legal.md)

For legal boundaries, also read the repository-level
[CONTRIBUTING.md](../../CONTRIBUTING.md). In short: do not contribute Native
Instruments binaries, firmware, installers, logos, screenshots, proprietary
code, private SDK material, or copied implementation code under incompatible
terms.

## Quality Expectations

OpenA8DJ treats sound quality as a release requirement. A change that touches
audio timing, buffering, USB behavior, routing, input mode, MIDI, or packaging
needs evidence appropriate to its risk.

Typical checks include:

- source build and offline C++ tests;
- HAL smoke tests where relevant;
- package signing and notarization checks for public release assets;
- Audio MIDI Setup visibility;
- routing and MIDI checks;
- real sound-quality validation for audio-path changes;
- human listening sign-off before a normal listening build is offered.

See [Success metrics](validation/success-metrics.md) and
[Test plan](validation/test-plan.md) for the current gates.

## Documentation Style

Keep public docs readable for people who did not watch the development process.
User-facing pages should explain what to do, what should happen, and what to try
if it does not work.

Internal handoffs, raw evidence, date-stamped state, and local investigation
notes belong in [project state](../../docs-state/README.md). If state becomes
important for users or contributors, rewrite it into a clean page rather than
linking people directly to raw notes.

## Branches

The canonical public macOS release line is `main`.

Historical and experimental lines are useful, but they are not the default
install path. See [Legacy and research branches](legacy-and-research-branches.md)
for the current branch model.

## Pull Requests

Before opening a pull request:

1. Keep the change focused.
2. Include the tests or validation that match the risk.
3. Update user or project docs when behavior changes.
4. Preserve the independent, non-affiliated project language.
5. Say clearly what was tested and what was not tested.
