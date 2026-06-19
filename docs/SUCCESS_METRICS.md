# Success Metrics

OpenA8DJ `main` is the modern macOS driver line. These metrics define what the
current macOS C++ release must prove before stronger quality, performance, or
production-readiness claims are made.

Compilation alone is not a product metric.

## Required Product Shape

- macOS exposes `Open Audio 8 DJ` as 8 inputs and 8 outputs.
- Inputs are usable as stereo pairs A/B/C/D for Traktor/DVS assignment.
- Outputs are usable as stereo pairs A/B/C/D for deck routing.
- CoreMIDI endpoints appear as `Open Audio 8 DJ MIDI In` and
  `Open Audio 8 DJ MIDI Out`.
- The hardware profile command supports `timecode-vinyl`.
- 44.1 kHz and 48 kHz are the minimum validated production rates.
- 88.2 kHz and 96 kHz are extended validation targets until explicitly marked
  as passed in release notes.

## Offline Gates

Offline gates must pass before packaging or human testing:

- C++ core contract tests.
- Packet layout and sample conversion tests.
- A/B/C/D identity routing tests.
- 8 input / 8 output topology tests.
- Timecode profile and deck-isolation tests.
- Real-time policy audit: no allocation, blocking lock, file I/O, logging, UI,
  or synchronous IPC in the audio hot path.
- Build/package safety checks for the exact commit being released.

## Physical Sound Quality Gates

A candidate can be offered for human listening only after the exact loaded
artifact has been validated with real sound-quality evidence, unless it is
explicitly labeled diagnostic-only.

Required physical evidence:

- Original WAV/tone reference saved with the run.
- Audio 8 DJ output captured through the approved external capture route.
- Analyzer result comparing captured audio against the original reference.
- No clipping.
- No click bursts.
- No obvious white-noise, metallic, pitch, channel-order, or bass-loss failure.
- No channel leakage between A/B/C/D above the documented threshold for the
  run.
- Idle noise captured and reported when the test is intended to validate
  background noise.

## Performance Gates

Performance evidence must be captured under the same route and driver build as
the sound-quality evidence:

- Active underruns: `0`.
- Active overruns: `0`.
- Playback failures: `0`.
- Driver CPU p95 recorded.
- `coreaudiod` CPU p95 recorded.
- No sustained audio-stack CPU runaway.
- Start latency and first-callback latency recorded for release candidates.
- Jitter or scheduling discontinuity counters recorded for transport changes.

The current goal is stable, clean audio first. CPU reductions are accepted only
when they do not regress sound quality, routing, Timecode Vinyl, or recovery.

## Timecode Vinyl Gates

Before declaring full DVS readiness:

- `timecode-vinyl` profile command succeeds.
- Traktor sees all input pairs A/B/C/D.
- Deck A/B timecode signal is present on the expected physical inputs.
- Channel order is correct.
- Scratch response is responsive and stable.
- No leakage from unrelated input pairs.
- Output routing remains correct while timecode input is active.

Operator validation is required for final DVS claims because Traktor behavior
and vinyl signal quality cannot be fully proven by offline fixtures alone.

## Release Gates

Public release artifacts must include:

- DMG.
- PKG.
- Checksums.
- Release notes.
- Install and uninstall instructions.
- Known limits.
- Legal/provenance statement.

Production-quality release also requires:

- Developer ID signing.
- Apple notarization.
- Full output-pair validation.
- Full DVS/timecode matrix validation.
- Long-run stability evidence.
- Recovery evidence after install, uninstall, reconnect, and audio-stack idle.

## Legacy Branch Boundary

The previous C/Objective-C implementation and its historical evidence live on
the `legacy` branch. `main` may mention `legacy` only to explain branch roles or
baseline provenance. New user-facing work, documentation, packaging, and
quality claims belong to the modern macOS `main` line.
