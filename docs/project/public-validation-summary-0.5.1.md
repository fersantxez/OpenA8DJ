# OpenA8DJ 0.5.1 Validation Summary

OpenA8DJ 0.5.1 freezes the responsive `output3072` profile accepted for current
Traktor use. This page separates what was observed from what remains outside
the release claim.

## Verified

- The source branch is isolated from the Windows and Rust worktrees.
- The default build geometry is 3072 start/target, 1536 restart, and 9216 high
  water.
- The exact frozen HAL executable passed three guarded safety cycles.
- The default and release offline suites passed 88/88 and 89/89.
- The release benchmark reported no check errors, overflows, or panic flags.
- The installed driver matched the frozen executable byte for byte.
- macOS enumerated `Open Audio 8 DJ` with 8 inputs and 8 outputs at 48 kHz.
- The accepted soak observed at most 0.1% watched CPU; the final driver and
  CoreAudio idle sample were 0.0%.
- The operator accepted this version's responsiveness in Traktor on
  2026-07-25.

## Exact Release Artifact

```text
Signed HAL executable SHA-256:
561aadd32bc24f078ad8a94936a8faaec7f5e90ea19e96f8212a078711a9ed62

OpenA8DJ-0.5.1.dmg SHA-256:
92703ac81fd9c4e9ebffa123b42cc835906c2cec716b0eb916bd165b45de66ac

OpenA8DJ-0.5.1.pkg SHA-256:
1d54486ba6d13e69752bf4994ca0c39bcd1a486da0ffff13bf20e51df194002e
```

The clean 0.5.1 build passed `hdiutil verify`, checksum verification, HAL smoke
and parity checks, 88/88 default tests, and 89/89 release tests. The final
Developer ID signatures change the executable bytes relative to the unsigned
frozen candidate, so the signed HAL hash above is the publication identity.

Apple accepted the final PKG, DMG, tools PKG, and tools DMG. All four were
stapled and validated. `make verify-signed-release` passed on the exact final
files, including code signatures, Developer ID authorities, package
signatures, trusted timestamps, Gatekeeper assessment, stapled tickets, and
checksums.

## GitHub-Downloaded Installation

The public release is
[OpenA8DJ 0.5.1 Responsive Freeze](https://github.com/fersantxez/OpenA8DJ/releases/tag/v0.5.1).
It contains exactly these two assets:

```text
OpenA8DJ-0.5.1.dmg
OpenA8DJ-0.5.1-checksums.txt
```

The final GitHub-downloaded installation results are appended here after the
signed assets replace the earlier prerelease files. Publication is not complete
until the downloaded DMG passes its public checksum, Gatekeeper, stapled-ticket,
normal Installer, installed-file, device-visibility, and CPU-health checks.

## Distribution

The release is DMG-only from the user's perspective:

```text
OpenA8DJ-0.5.1.dmg
OpenA8DJ-0.5.1-checksums.txt
```

The DMG contains the macOS PKG installer. The PKG is not a separate GitHub
asset.

The final public DMG, its embedded installer, driver, MIDI bridge, and command
line helper are Developer ID signed. Apple notarization accepted the final
containers and their tickets are stapled. OpenA8DJ 0.5.1 supersedes 0.5.0 as
the current signed and notarized responsive release once the GitHub-downloaded
installation gate above passes.

## Not Claimed

- A complete physical needle-to-audio latency measurement
- Full Traktor Timecode Vinyl certification across both rates and decks
- Audiophile superiority over 0.5.0
- DriverKit/deXt production readiness

Future candidates must follow the
[timecode latency checkpoints](timecode-latency-checkpoints.md).
