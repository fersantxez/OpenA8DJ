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
