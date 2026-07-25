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

## GitHub-Downloaded Installation

The published prerelease is
[OpenA8DJ 0.5.1 Responsive Freeze (Experimental)](https://github.com/fersantxez/OpenA8DJ/releases/tag/v0.5.1).
It contains exactly these two assets:

```text
OpenA8DJ-0.5.1.dmg
OpenA8DJ-0.5.1-checksums.txt
```

The assets were downloaded again from GitHub into `~/Downloads`. The downloaded
DMG passed its published SHA-256 check and `hdiutil verify`. Its internal PKG
reported version 0.5.1, and the embedded HAL executable matched the frozen
SHA-256 above.

The GitHub-downloaded DMG was then mounted and its internal PKG installed with
the documented `sudo installer` path. This fallback was used because the
preview is not Developer ID signed or notarized; normal Gatekeeper acceptance
is not claimed. After 20 seconds:

- the installed receipt reported version 0.5.1;
- the installed HAL matched the frozen executable SHA-256;
- `Open Audio 8 DJ` enumerated as 8 in / 8 out at 48 kHz;
- audio-stack health passed;
- the driver sampled at 0.9% CPU and CoreAudio at 0.2% CPU;
- no USB reset, sample-rate change, default-device change, playback, recording,
  or Traktor automation was performed;
- the hardware lock was released.

A subsequent stabilized health sample passed at 0.0% driver CPU and 0.0%
CoreAudio CPU. The existing Control Center and `opena8dj-control` helper were
also present after the driver upgrade.

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
