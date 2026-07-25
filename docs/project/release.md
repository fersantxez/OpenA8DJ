# Release Process

This process applies to the modern macOS C++ mainline. The previous
C/Objective-C implementation is preserved separately on the `legacy` branch and
is not the user-facing release line.

## Build

```sh
make clean
make all
make dist
```

Generated artifacts:

```text
build/OpenA8DJ-<version>.pkg
build/OpenA8DJ-<version>.dmg
build/OpenA8DJ-<version>-checksums.txt
```

## Local test install

```sh
sudo installer -pkg build/OpenA8DJ-<version>.pkg -target /
```

Then validate:

```sh
./build/audio-inspect
./build/audio-io-test 2 44100
./build/audio-io-test 2 48000
./build/audio-pair-tone A 3 440 0.06
./build/audio-pair-tone B 3 660 0.06
./build/midi-list
/usr/local/bin/opena8dj-control
```

Core Audio should return to idle after tests:

```sh
ps -o %cpu,%mem,pid,comm -p $(pgrep coreaudiod | head -1)
```

For Traktor Scratch/timecode validation, use
[Traktor and Timecode Vinyl](../user/traktor-timecode.md). Mark a release as
fully DVS-ready only after the physical input pairs, vinyl profile, CD/line
profile, and `input-mode` values have been verified.

Treat 88.2/96 kHz as extended validation. Do not advertise those rates as
production-ready until they pass the same listening, routing, and loopback tests
as 44.1/48 kHz.

## Legal/provenance gate

Before attaching any artifact to a public release, complete the
[legal and publication policy](../reference/legal.md):

- confirm the release is MIT-licensed and includes `LICENSE`, `NOTICE.md`, and
  `BRAND_POLICY.md`;
- confirm no Native Instruments binaries, firmware, installers, logos, or other
  proprietary payloads are present in Git history or release artifacts;
- confirm no copied third-party implementation code under incompatible license
  terms is present;
- confirm product names are used only for compatibility and do not imply
  affiliation, sponsorship, endorsement, certification, or official status;
- confirm release text says official public binary downloads come from GitHub
  Releases and that mirrors/repackaged installers are not official;
- confirm unvalidated features are described as unvalidated;
- confirm binary artifacts are signed/notarized if advertised as public-ready.

If any item is uncertain, publish source only and hold the binary artifacts.

## Signing for local testing

Local builds use an ad-hoc HAL bundle signature by default:

```sh
make dmg
```

You can also pass a local signing identity with `SIGN_IDENTITY`.

## Signing for public distribution

Public release needs an active Apple Developer Program membership. Logging in
with an Apple ID is not enough; Apple must complete enrollment and make the
Developer ID certificate types available for the team.

Required Apple assets:

- Developer ID Application identity for the HAL bundle
- Developer ID Installer identity for the PKG
- Apple notarization credentials stored locally with `notarytool`
- stapled notarization tickets for both PKG and DMG

Create the notarization profile once on the release Mac. Prefer an App Store
Connect API key for automation; an app-specific password also works for a
single maintainer account:

```sh
xcrun notarytool store-credentials OpenA8DJNotary \
  --apple-id "<apple-id>" \
  --team-id "<TEAMID>" \
  --password "<app-specific-password>"
```

Do not commit Apple IDs, app-specific passwords, API keys, `.p8` files, or
keychain exports.

Example:

```sh
make clean
make release-signed \
  SIGN_IDENTITY="Developer ID Application: Example Team (TEAMID)" \
  PKG_SIGN_IDENTITY="Developer ID Installer: Example Team (TEAMID)" \
  DMG_SIGN_IDENTITY="Developer ID Application: Example Team (TEAMID)"
make notarize NOTARY_PROFILE=OpenA8DJNotary
make verify-signed-release
```

`make release-signed` blocks if the Developer ID identities are not supplied.
`make notarize` submits the driver PKG/DMG and the tools PKG/DMG to Apple, waits
for the result, staples tickets, and regenerates checksums. `make
verify-signed-release` is the release gate: it checks Developer ID Application
signatures on the HAL bundle, command-line tools, and Control Center app;
Developer ID Installer signatures on both PKGs; Gatekeeper assessment; stapled
tickets; and checksums.

Do not upload replacement public assets until Apple returns `Accepted`,
stapling succeeds, and `make verify-signed-release` passes. Current
maintainer-facing signing state belongs in
[notarization state](../../docs-state/notarization-state.md).

## GitHub release

Publish only the end-user DMG and its checksum file:

```text
build/OpenA8DJ-<version>.dmg
build/OpenA8DJ-<version>-checksums.txt
```

The PKG remains inside the driver DMG because macOS Installer consumes it. Do
not upload that PKG as a separate GitHub asset. Optional tools can have their
own versioned release when they change.

For 0.5.0 and later, the public release should make the easy path clear:
download the DMG from GitHub Releases, open it, run the bundled PKG installer,
and reconnect the Audio 8 DJ if needed.

The DMG includes `LICENSE`, `NOTICE.md`, `LEGAL.md`, and `BRAND_POLICY.md`.
The PKG installs the same files under `/Library/Documentation/OpenA8DJ`.

For public previews, the release title and notes must say whether the macOS
package is not notarized or not Developer ID signed. A polished end-user release
still requires the macOS signing/notarization gate; a preview may be published
only if it passes the legal/provenance gate and clearly documents the macOS
approval step.

For private validation snapshots, create a draft release and attach the DMG,
PKG, and checksum file. Publish the release only after the legal/provenance gate
and the appropriate preview or signing/notarization gate are complete.
