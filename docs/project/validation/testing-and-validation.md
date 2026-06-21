# Testing And Validation

OpenA8DJ treats audio quality as a real release requirement. A build is not
considered ready just because it compiles or because counters look clean.

## Release Validation Layers

1. Source and package build.
2. Offline C++ tests.
3. HAL smoke tests.
4. Package signing and notarization checks for public assets.
5. Audio MIDI Setup visibility.
6. Channel routing checks.
7. MIDI endpoint checks.
8. Physical sound validation with real music and external capture.
9. Human listening sign-off on the exact installed artifact.

## Sound Quality Rule

Do not offer a normal listening build unless the exact loaded or packaged
artifact has passed the strongest available sound-quality validation for that
context. If full physical capture is not available, say that plainly.

## Physical Capture Method

The preferred validation path uses real music through the Audio 8 DJ and records
the analog output in real time with an external capture path. For the 0.5.0
freeze, the accepted sound profile used that live-recording method.

The captured WAV is compared against the source using alignment, residual,
clipping, click, and timing checks. Human listening remains the final arbiter.

## Evidence

Readable summaries live in:

- [Public validation summary](../public-validation-summary.md)
- [Measurement methodology](measurement-methodology.md)

Detailed logs, rejected experiments, local paths, and build names live in
maintainer state:

- [Detailed evidence](../../../docs-state/evidence/test-evidence.md)
