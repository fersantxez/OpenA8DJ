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
`docs/TRAKTOR_TIMECODE.md`. Mark a release as fully DVS-ready only after the
physical input pairs, vinyl profile, CD/line profile, and `input-mode` values
have been verified.

Treat 88.2/96 kHz as extended validation. Do not advertise those rates as
production-ready until they pass the same listening, routing, and loopback tests
as 44.1/48 kHz.

## Legal/provenance gate

Before attaching any artifact to a public release, complete the publication
policy in `docs/LEGAL.md`:

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
`make notarize` submits the PKG and DMG to Apple, waits for the result, staples
both tickets, and regenerates checksums. `make verify-signed-release` is the
release gate: it checks Developer ID Application signatures on the HAL bundle
and packaged tools, Developer ID Installer signature on the PKG, Gatekeeper
assessment, stapled tickets, and checksums.

Current local blocker observed during the Apple enrollment attempt:

```text
0 valid codesigning identities found
No Keychain password item found for profile: OpenA8DJNotary
```

The browser session is authenticated as an Apple ID and the Developer Program
enrollment form has been submitted. Apple currently shows:

```text
Thank you for your submission.
We'll review the details you provided and contact you soon.
```

After Apple accepts enrollment and any payment/verification is complete,
create/download the Developer ID Application and Developer ID Installer
certificates, then store notarization credentials and rerun the official
release commands above.

Without this step, macOS Gatekeeper may block the PKG with a message saying
Apple could not verify it is free of malware. That is expected for ad-hoc signed
preview builds and must be documented in the README, install guide, DMG README,
and release notes. Do not describe an ad-hoc signed preview as a polished
one-click installer.

## GitHub release

Attach these files to each macOS release:

```text
build/OpenA8DJ-<version>.dmg
build/OpenA8DJ-<version>.pkg
build/opena8dj-tools-<version>.dmg
build/opena8dj-tools-<version>.pkg
build/OpenA8DJ-<version>-checksums.txt
```

The README links to the latest release page and the current versioned assets so
testers can download the installer without building from source.

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
