# Notarization State

This file records maintainer-facing signing and notarization state. Do not put
Apple IDs, app-specific passwords, API keys, private keys, or keychain exports
in this repository.

As of 2026-07-25, the release Mac has valid Developer ID Application and
Developer ID Installer identities, plus a local `notarytool` keychain profile
named `OpenA8DJNotary`.

## Current 0.5.1 Responsive Release

The responsive `output3072` freeze was rebuilt after valid Developer ID
identities were restored. The final build includes the current packaged
documentation and was signed with:

```text
Developer ID Application: Fernando Sanchez (D3KWK7MN3Y)
Developer ID Installer: Fernando Sanchez (D3KWK7MN3Y)
```

Apple accepted all four release-verification containers on 2026-07-25:

```text
OpenA8DJ-0.5.1.pkg
  submission: 054caf6a-6eac-47e6-b493-49c6a9578a58
  status: Accepted

OpenA8DJ-0.5.1.dmg
  submission: c934841f-ab88-4c79-9a36-29fc952a8189
  status: Accepted

opena8dj-tools-0.5.1.pkg
  submission: 1cc4fbb5-7830-4db6-b6fc-f040c3f3886e
  status: Accepted

opena8dj-tools-0.5.1.dmg
  submission: 60b58902-379e-4c41-99d8-0be9087c4ddb
  status: Accepted
```

The four containers were stapled and validated. `make verify-signed-release`
then passed on the exact final stapled files, including Developer ID
authorities, trusted timestamps, Gatekeeper assessment, stapled tickets, code
signatures, package signatures, and checksums.

Final stapled SHA-256 hashes:

```text
663edff5d4f9fe1945f1838fc72cd1e1ffa0f2697297db926aac813e3f7f6998  OpenA8DJ-0.5.1.dmg
3cd3dbd2da3ddf5b255cd5f70768ca4024e3f2b277887ce572578c833723283c  OpenA8DJ-0.5.1.pkg
9e4874cc4ba555b6be4e6a694f7d43ace4c795626e7ddaaf5c8925017d5b9f14  opena8dj-tools-0.5.1.dmg
6e6135e41b072bd29878c35dc70aaf241255287ad0bcfd73c7a298dcfd7f8be7  opena8dj-tools-0.5.1.pkg
```

The final DMG is self-contained. Its embedded PKG contains OpenA8DJ Control
Center and independently passes `xcrun stapler validate` and Gatekeeper
assessment.

The GitHub release remains DMG-only for users: publish
`OpenA8DJ-0.5.1.dmg` and `OpenA8DJ-0.5.1-checksums.txt`. The standalone PKG
and tools containers are internal verification artifacts and must not be
uploaded as separate release assets.

The public GitHub-downloaded installation result is recorded in
`docs/project/public-validation-summary-0.5.1.md` after the final release
replacement and clean end-user installation.

## Current 0.5.0 Publication Attempt

The current publication artifacts were rebuilt from source commit
`86bd0274f57c9eed231e9533c0c5a8e8f4cd30b5` after the documentation
reorganization moved maintainer continuity notes to top-level `docs-state` and
updated packaged release notes. They were signed with the Developer ID
Application and Developer ID Installer certificates, locally verified, and
submitted to Apple on 2026-06-21T15:19Z.

Later documentation-only edits outside `docs/reference`,
`resources/dmg`, and `resources/control-surfaces-dmg` do not change the bytes
inside the release packages or DMGs. Do not restart notarization solely for a
non-packaged documentation update.

Pre-staple SHA-256 hashes:

```text
1f35cae55709bfcd964863fde46af7c35e841db27803582fdf3851ea975671b4  OpenA8DJ-0.5.0.dmg
2bbec9fca54679dff0a8096307c74d72b106d0b8249a654ed6d8a5a4712c9847  OpenA8DJ-0.5.0.pkg
63d2bbce75402a3627b651b208909da2e318f047443559b6d842159432802bb3  opena8dj-tools-0.5.0.dmg
42e04f560201d4aeed61a68841540149dd458e793782e5f271cc7d1e23535faf  opena8dj-tools-0.5.0.pkg
```

Latest Apple notarization status checked on 2026-06-22T06:23Z:

```text
OpenA8DJ-0.5.0.pkg
  submission: cdd4e4c2-192b-40c8-89ed-5e79d54f62f7
  status: Accepted

OpenA8DJ-0.5.0.dmg
  submission: 9ea2fe39-b4df-4ddb-b80c-3ef517361133
  status: Accepted

opena8dj-tools-0.5.0.pkg
  submission: 5a8cdde2-7788-4058-a8c8-28c7fd5be95e
  status: Accepted

opena8dj-tools-0.5.0.dmg
  submission: dca61499-b3f5-46fd-93b8-ee63f4868942
  status: Accepted
```

No package was installed, no driver was loaded, no hardware was touched, and no
CoreAudio restart was performed for this signing/notarization rebuild.

All four current publication containers were accepted by Apple, stapled, and
verified locally on 2026-06-22T06:24Z.

Final stapled SHA-256 hashes:

```text
c0cce3bda690581d6ef6ebde96758ee47f05c38e4fd93afc30fed6ff5a79bce2  OpenA8DJ-0.5.0.dmg
9c9f92a2e1e9ae376bd2c4991382d98e866cdcdef5fa552f4bc485c420e7c68d  OpenA8DJ-0.5.0.pkg
175ac55752d8c46af9d19503e3f2f04b99478b9921d1808cf8c74093aa811827  opena8dj-tools-0.5.0.dmg
e1bda7fdba18e48b74516ef0d1fd6cde10a0f21be481d388fdfe780472bae54c  opena8dj-tools-0.5.0.pkg
```

`make verify-signed-release` passed on the final stapled files. The final files
were uploaded to GitHub release `v0.5.0`, then downloaded into `~/Downloads`
and validated through the end-user installation flow on 2026-06-22T06:32Z.

GitHub-downloaded install validation summary:

```text
release: https://github.com/fersantxez/OpenA8DJ/releases/tag/v0.5.0
evidence: local-analysis/github-install-e2e-20260622T063225Z
download_checksums: PASS
DMG stapled-ticket validation: PASS
Gatekeeper assessment for public DMGs: PASS
mounted DMG package signature and Gatekeeper assessment: PASS
normal Installer app opened both packages inside mounted DMGs: yes
sudo installer fallback used after GUI open: yes
driver install from mounted public DMG: PASS
tools install from mounted public DMG: PASS
installed receipts/files/code signatures: PASS
Core Audio visibility: Open Audio 8 DJ, 8 inputs / 8 outputs at 48 kHz
MIDI visibility: Open Audio 8 DJ MIDI In/Out present
Control Center and CLI presence: PASS
audio stack health after install: PASS
CoreAudio restart: uninstall only, via the project uninstaller
USB/hardware touched: no
```

## Stale Submissions

Earlier tools artifacts were accepted by Apple and stapled locally:

```text
opena8dj-tools-0.5.0.pkg: 5e4bb7c8-3de1-4b58-98ab-27ebde2b188c
opena8dj-tools-0.5.0.dmg: 9ab1b493-1a32-4674-aede-863f25a9242c
```

The internal HAL diagnostic submission was accepted, and
`build/OpenA8DJ.driver` validated as Notarized Developer ID.

The d31fa63 release-container submissions are also stale for publication after
the docs-state reorganization, even though Apple had accepted two of them:

```text
OpenA8DJ-0.5.0.pkg: e157be38-1542-40c1-8cb9-310564101890
OpenA8DJ-0.5.0.dmg: dc39b66f-3a77-4033-80de-c447bcfb0f2d
opena8dj-tools-0.5.0.pkg: e698ed76-1aff-49fb-b63a-c26d0661be20
opena8dj-tools-0.5.0.dmg: 66cef6ca-a7ee-4a04-b17a-bd8017acd9d8
```

All release-container submissions created before commit `86bd027` are stale for
publication, even if Apple later reports `Accepted`, because packaged release
notes or DMG text changed after those submissions.

Release completion checklist:

1. The final stapled DMG files were uploaded to GitHub release `v0.5.0`.
2. The GitHub-downloaded assets passed the end-user install validation.
3. The final public validation evidence is recorded in this repository.
