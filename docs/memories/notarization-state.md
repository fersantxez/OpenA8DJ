# Notarization State

This file records maintainer-facing signing and notarization state. Do not put
Apple IDs, app-specific passwords, API keys, private keys, or keychain exports
in this repository.

As of 2026-06-20, the release Mac has Developer ID Application and Developer ID
Installer certificates available, plus a local `notarytool` keychain profile
named `OpenA8DJNotary`.

## Current 0.5.0 Publication Attempt

The current publication artifacts were rebuilt from source commit
`d31fa6373c1304d429b85537f8379733c62eadc4` after the public documentation was
generalized to avoid naming the external real-time capture device. They were
signed with the Developer ID Application and Developer ID Installer
certificates, locally verified, and submitted to Apple on
2026-06-21T02:21Z.

Later edits that touch only `docs/memories` evidence files do not change the
bytes inside the release packages or DMGs. Do not restart notarization solely
for a memories-only evidence update.

Pre-staple SHA-256 hashes:

```text
4463317a031eb7245a8f709d055c6d65525a5d7884234d0ad8a25468b2334892  OpenA8DJ-0.5.0.dmg
925d7015cc77a22591aefb2786d7f7c8f8ebc49ff733b508e5bb03ffa46a34b5  OpenA8DJ-0.5.0.pkg
22bde3d405387e377ea0c8cf39a52636b65fc8f6f5faa0a10ce18459699294cb  opena8dj-tools-0.5.0.dmg
377883acc5fb68dfe7852258d5019c26c5b2b9b15565188b8a7d6500d05b7d0f  opena8dj-tools-0.5.0.pkg
```

Latest Apple notarization status checked on 2026-06-21T04:41Z:

```text
OpenA8DJ-0.5.0.pkg
  submission: e157be38-1542-40c1-8cb9-310564101890
  status: In Progress

OpenA8DJ-0.5.0.dmg
  submission: dc39b66f-3a77-4033-80de-c447bcfb0f2d
  status: Accepted

opena8dj-tools-0.5.0.pkg
  submission: e698ed76-1aff-49fb-b63a-c26d0661be20
  status: Accepted

opena8dj-tools-0.5.0.dmg
  submission: 66cef6ca-a7ee-4a04-b17a-bd8017acd9d8
  status: In Progress
```

No package was installed, no driver was loaded, no hardware was touched, and no
CoreAudio restart was performed for this signing/notarization rebuild.

## Stale Submissions

Earlier tools artifacts were accepted by Apple and stapled locally:

```text
opena8dj-tools-0.5.0.pkg: 5e4bb7c8-3de1-4b58-98ab-27ebde2b188c
opena8dj-tools-0.5.0.dmg: 9ab1b493-1a32-4674-aede-863f25a9242c
```

The internal HAL diagnostic submission was accepted, and
`build/OpenA8DJ.driver` validated as Notarized Developer ID.

All release-container submissions created before commit `d31fa63` are stale for
publication, even if Apple later reports `Accepted`, because packaged release
notes or DMG text changed after those submissions.

Do not upload replacement public assets until:

1. Apple returns `Accepted` for the exact release containers.
2. Stapling succeeds for the final PKG and DMG files.
3. `make verify-signed-release` passes.
4. The checksum file is regenerated from the final stapled files.
5. The GitHub-downloaded assets pass the end-user install validation.
