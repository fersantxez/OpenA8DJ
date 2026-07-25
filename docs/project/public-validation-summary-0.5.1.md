# OpenA8DJ 0.5.1 Validation Summary

OpenA8DJ 0.5.1 freezes the responsive `output3072` profile accepted for current
Traktor use. This page separates what was observed from what remains outside
the preview claim.

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
HAL executable SHA-256:
3984d58112e6dc9e5c8901cb7a9d605ddccfe7f6f7b3b0eb17ce2238add6f04d

OpenA8DJ-0.5.1.dmg SHA-256:
be11f1988b0fd7524ab9fca46d406a4c0eeed08e4b23a06be91c1346f80fb1a0
```

The clean 0.5.1 build passed `hdiutil verify`, checksum verification, HAL smoke
and parity checks, 88/88 default tests, and 89/89 release tests. Expanding the
PKG from the mounted DMG produced the same HAL executable hash as the named
frozen candidate.

The mounted-DMG PKG was installed locally. The receipt reported version 0.5.1,
the installed HAL hash matched the packaged hash, the device enumerated as 8 in
/ 8 out at 48 kHz, and the stabilized driver/CoreAudio CPU sample was 0.0% /
0.0%. The hardware lock was released.

## Distribution

The release is DMG-only from the user's perspective:

```text
OpenA8DJ-0.5.1.dmg
OpenA8DJ-0.5.1-checksums.txt
```

The DMG contains the macOS PKG installer. The PKG is not a separate GitHub
asset.

No valid Developer ID identity was available on the release Mac when 0.5.1 was
built. The preview is locally signed, not Developer ID signed or notarized.
OpenA8DJ 0.5.0 remains the last Apple-notarized baseline.

## Not Claimed

- A complete physical needle-to-audio latency measurement
- Full Traktor Timecode Vinyl certification across both rates and decks
- Audiophile superiority over 0.5.0
- DriverKit/deXt production readiness
- Developer ID or Apple notarization for the 0.5.1 preview

Future candidates must follow the
[timecode latency checkpoints](timecode-latency-checkpoints.md).
