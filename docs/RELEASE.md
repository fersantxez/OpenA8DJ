# Release Process

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

Public release needs:

- Developer ID Application identity for the HAL bundle
- Developer ID Installer identity for the PKG
- Apple notarization
- stapling of notarization tickets

Example:

```sh
make dmg \
  SIGN_IDENTITY="Developer ID Application: Example Team (TEAMID)" \
  PKG_SIGN_IDENTITY="Developer ID Installer: Example Team (TEAMID)" \
  DMG_SIGN_IDENTITY="Developer ID Application: Example Team (TEAMID)"
```

Notarization is intentionally not automated in this repo because it requires
team-specific credentials.

## GitHub release

Attach these files to each macOS release:

```text
build/OpenA8DJ-<version>.dmg
build/OpenA8DJ-<version>.pkg
build/OpenA8DJ-<version>-checksums.txt
```

The README links to the latest release page and the current versioned assets so
testers can download the installer without building from source.

The DMG includes `LICENSE`, `NOTICE.md`, `LEGAL.md`, and `BRAND_POLICY.md`.
The PKG installs the same files under `/Library/Documentation/OpenA8DJ`.

For private validation snapshots, create a draft release and attach the DMG,
PKG, and checksum file. Publish the release only after the legal/provenance gate
and macOS signing/notarization gate are complete.

## Windows release gate

Do not publish a Windows MSI from this macOS HAL tree. Windows release artifacts
are only valid after the project contains a real Windows driver package with an
INF, driver binary, catalog, and signing story. See `docs/WINDOWS.md`.
