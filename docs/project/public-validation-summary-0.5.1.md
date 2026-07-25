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
c2d7bfef23c1127ce8cba98b84b64d06cd5e306faaeed4c3adf37ab87c431778

OpenA8DJ-0.5.1.dmg SHA-256:
663edff5d4f9fe1945f1838fc72cd1e1ffa0f2697297db926aac813e3f7f6998

OpenA8DJ-0.5.1.pkg SHA-256:
3cd3dbd2da3ddf5b255cd5f70768ca4024e3f2b277887ce572578c833723283c
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

The DMG was rebuilt after the PKG ticket was stapled, then re-signed and
notarized. The embedded PKG therefore validates its own stapled ticket, passes
Gatekeeper assessment, and contains OpenA8DJ Control Center. The public DMG is
self-contained and does not depend on the 0.5.0 tools package.

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
