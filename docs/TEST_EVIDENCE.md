# Test Evidence

This file records reproducible release and validation evidence for the modern
macOS mainline.

## 2026-06-19 09:54 EDT - Signing and notarization release gate

- Commit before changes: `5f6e457`
- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Hardware touched: no
- Audio/CoreAudio touched: no
- Driver installed/reloaded: no

Commands run:

```sh
bash -n scripts/notarize-release scripts/verify-signed-release
make -n release-signed \
  SIGN_IDENTITY="Developer ID Application: Example Team (TEAMID)" \
  PKG_SIGN_IDENTITY="Developer ID Installer: Example Team (TEAMID)" \
  DMG_SIGN_IDENTITY="Developer ID Application: Example Team (TEAMID)"
make dist
make verify-signed-release
```

Results:

- Script syntax passed.
- Dry-run release path signs the HAL bundle and packaged tools with Developer
  ID Application, signs the PKG with Developer ID Installer, signs the DMG,
  and regenerates checksums.
- `make dist` still succeeds with the local ad-hoc preview signature.
- `make verify-signed-release` intentionally fails on the ad-hoc preview
  artifact, proving the official release gate blocks unsigned/non-Developer ID
  artifacts.

Observed verifier failure:

```text
HAL bundle is not signed with a Developer ID Application certificate.
Signature=adhoc
TeamIdentifier=not set
```

Generated ad-hoc preview artifact hashes from this run:

```text
7580d6efea5693498d572ae448702e364a0e1de43701972c75dcc92d165909c6  build/OpenA8DJ-0.4.0.dmg
4e037f187177a73a2504782bbca4427e89754dfb1ad44a3aaf51edcba01ce78e  build/OpenA8DJ-0.4.0.pkg
db08d6137a49b577003b9d48fa57cf6cb9df109a8ef984e88ce997a2187a4ce7  build/OpenA8DJ-0.4.0-checksums.txt
```

Current external blocker:

```text
0 valid codesigning identities found
No Keychain password item found for profile: OpenA8DJNotary
```

The Apple Developer browser session is authenticated and the Developer Program
enrollment form has been submitted. Apple currently shows:

```text
Thank you for your submission.
We'll review the details you provided and contact you soon.
```

The account owner must wait for Apple acceptance and complete any remaining
payment or verification before Developer ID certificates and notarization
credentials can exist on this machine.

## 2026-06-19 10:28 EDT - Unity gain and Timecode Vinyl input restore

- Worktree: `/Users/fer/dev/audio8djcpp`
- Branch: `driverkit/cpp-redesign`
- Hardware touched: yes, with hardware/audio lock
- Driver installed/reloaded: yes, local HAL install only
- GitHub release assets replaced: no

Reason:

- Human Traktor testing reported bass saturation and general output saturation.
- Traktor Timecode Vinyl scope showed no signal.
- Local control state before the fix showed `input-mode: timecode-vinyl` but
  `input-decode: off`.

Changes under test:

```text
HAL_OUTPUT_GAIN default: 1.50f -> 1.00f
gInputDecodeEnabledPreference default: false -> true
```

Commands/evidence:

```sh
make -B hal install-hal
/usr/local/bin/opena8dj-control profile timecode-vinyl
./build/audio-input-meter 5
./scripts/run-soundcheck --skip-build --music-file ".../Cable Guy - Dj Deep (Original Mix).mp3" --pair A --rate 48000 --buffer 512 --seconds 12 --mode dense --capture-device "iRig Stream" --capture-channels 1,2
./scripts/run-channel-matrix-gate --run-physical --pair A --rate 48000 --seconds 4 --peak 0.25 --capture-device "iRig Stream" --capture-channels 1,2
```

Evidence directory:

```text
local-analysis/unity-gain-fix-20260619-102845
```

Results:

- Installed HAL hash after unity gain + DVS input fix:
  `3b55a7d11b9efa401208e9addaa7e137ba0ddb329d91d561b22b3e00d883605d`
- `opena8dj-control profile timecode-vinyl` reports `input-decode: on`.
- `audio-input-meter` no longer reports all-zero inputs; A/B/C/D show input
  energy after decode is enabled.
- Unity-gain Cable Guy soundcheck no longer clips the iRig capture:
  `capture_clipped_frames=0`.
- The same music soundcheck still fails quality thresholds, so this is not a
  release-quality claim.
- Unity-gain pair-A tone matrix passes physical channel-separation gate:
  `capture_clipped_frames=0`, `left_to_right_leakage_db=-61.99`,
  `right_to_left_leakage_db=-61.14`.

Conclusion:

- Saturation risk from the previous 1.5x default output gain is reduced.
- Timecode Vinyl silence caused by `input-decode: off` is corrected in the
  loaded local HAL by explicitly enabling the profile and by making decode
  default-on for future loads.
- Do not replace GitHub release assets until the real-music iRig quality gate
  passes.
