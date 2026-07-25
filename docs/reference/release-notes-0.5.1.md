# OpenA8DJ 0.5.1 Release Notes

OpenA8DJ 0.5.1 freezes the responsive `output3072` profile selected during the
July 2026 DVS latency work. It is the signed and notarized public release of the
version accepted in Traktor before the next optimization cycle.

## Responsive Freeze

- Output start latency: 3072 frames
- Output target latency: 3072 frames
- Output restart latency: 1536 frames
- Elastic high water: 9216 frames
- Host validation buffer: 512 frames
- Primary validation rate: 48 kHz
- Modeled offline pipeline latency: 78.667 ms p95

The more aggressive `output2816` candidate was not promoted because its fresh
exact-artifact recovery test failed. The 3072-frame profile passed three
guarded load/recovery cycles and was selected as the safer responsive point.

## Validation

- Default offline C++ suite: 88/88 passed
- Release offline C++ suite: 89/89 passed
- Release benchmark: zero check errors, overflows, or panic flags
- Exact-artifact HAL safety: three cycles passed
- Core Audio visibility: 8 inputs and 8 outputs at 48 kHz
- Maximum watched CPU during the accepted soak: 0.1%
- Final idle sample: driver 0.0%, CoreAudio 0.0%
- Operator acceptance: responsive Traktor behavior accepted on 2026-07-25

The latency number is an offline fixture/model measurement. This release does
not claim a completed automated physical needle-to-audio latency measurement
or a full Timecode Vinyl certification matrix.

## Distribution Status

The GitHub release contains one driver DMG and one checksum file. The PKG is
inside the DMG and is not uploaded as a separate asset.

The single DMG installs the driver, MIDI bridge, command-line helper, and
OpenA8DJ Control Center. No separate tools DMG is required.

The final DMG hash is published in `OpenA8DJ-0.5.1-checksums.txt`.

The driver, command-line helpers, embedded PKG, and public DMG are Developer ID
signed. Apple notarization and stapled tickets allow Gatekeeper to validate the
GitHub-downloaded release during normal installation.

## Continue From Here

Future latency work starts from this exact profile and must pass the
[timecode latency checkpoints](../project/timecode-latency-checkpoints.md)
before replacing it.
