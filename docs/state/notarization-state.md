# Notarization State

This file records maintainer-facing signing and notarization state. Do not put
Apple IDs, app-specific passwords, API keys, private keys, or keychain exports
in this repository.

As of 2026-06-20, the release Mac has Developer ID Application and Developer ID
Installer certificates available, plus a local `notarytool` keychain profile
named `OpenA8DJNotary`.

Tools artifacts were accepted by Apple and stapled locally:

```text
opena8dj-tools-0.5.0.pkg: 5e4bb7c8-3de1-4b58-98ab-27ebde2b188c
opena8dj-tools-0.5.0.dmg: 9ab1b493-1a32-4674-aede-863f25a9242c
```

The internal HAL diagnostic submission was accepted, and
`build/OpenA8DJ.driver` validated as Notarized Developer ID.

Current driver container submissions being watched:

```text
OpenA8DJ-0.5.0.pkg: 25fa7d44-8093-4775-bea7-564df91b868d
OpenA8DJ-0.5.0.dmg: 95f508d5-0b12-4b71-9cf0-3a03666dd60f
```

Do not upload replacement public assets until:

1. Apple returns `Accepted` for the exact release containers.
2. Stapling succeeds for the final PKG and DMG files.
3. `make verify-signed-release` passes.
4. The checksum file is regenerated from the final stapled files.
5. The GitHub-downloaded assets pass the end-user install validation.
