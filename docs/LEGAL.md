# Legal and Publication Policy

This document is a project policy, not legal advice. It exists to keep
OpenA8DJ publishable as an independent MIT-licensed compatibility project.

## Project Scope

OpenA8DJ is an independent driver stack for owners of the Native Instruments
Audio 8 DJ USB audio interface. It is not affiliated with, sponsored by,
endorsed by, or certified by Native Instruments.

Product and company names are used only to identify compatibility. Do not use
Native Instruments names, product names, logos, trade dress, screenshots, or
marketing assets as OpenA8DJ branding.

## Permitted Inputs

OpenA8DJ code may be based on:

- live testing against lawfully owned hardware;
- USB descriptors, packet captures, and behavior observed from that hardware;
- public macOS, Core Audio, CoreMIDI, DriverKit, USB, and packaging APIs;
- public device manuals and publicly available compatibility facts;
- original implementation work written for this repository.

## Prohibited Inputs

Do not commit or use as implementation source:

- Native Instruments driver binaries, installers, firmware blobs, apps,
  preference panes, images, icons, logos, screenshots, serial numbers, or other
  proprietary payloads;
- copied Native Instruments code or other proprietary implementation material;
- confidential information, SDK material, partner-portal material, or anything
  obtained under NDA;
- copied third-party implementation code under incompatible license terms;
- generated code that was prompted to reproduce proprietary or GPL code.

## MIT License Boundary

This repository is MIT-licensed as an original implementation. Contributors
must not copy third-party source code, comments, tables, structures, or control
flow unless that material is clearly compatible with the MIT License and is
attributed as required.

Compatibility facts such as vendor/product IDs, channel counts, supported sample
rates, endpoint descriptors, and command names may be documented when they are
public facts or are confirmed by live hardware testing.

## Firmware Policy

OpenA8DJ must not redistribute firmware unless a clearly redistributable source
and license are documented first. If a device requires firmware upload in a
future version, that work needs a separate legal review before any public
release.

## Trademark Policy

Use `Native Instruments`, `Audio 8 DJ`, and `Traktor` only in compatibility
statements, test notes, and factual documentation. Do not imply official status,
certification, sponsorship, or endorsement.

Preferred wording:

```text
OpenA8DJ is an independent compatibility driver for the Native Instruments
Audio 8 DJ USB audio interface.
```

Avoid wording such as:

```text
Official Audio 8 DJ driver
Native Instruments driver
Certified Traktor driver
```

## Public Release Gate

Before publishing a public release, verify that:

- `LICENSE` contains the MIT License;
- `NOTICE.md` is included in the repository and release artifacts;
- no Native Instruments binaries, firmware, installers, logos, or assets are in
  Git history or release artifacts;
- no copied third-party implementation code under incompatible license terms is
  present;
- release text states that the project is independent and not endorsed by
  Native Instruments;
- unvalidated features are described as unvalidated, not certified;
- macOS packages are signed/notarized when advertised as public-ready;
- Windows packages are not published unless a real Windows driver package,
  catalog, INF, and signing flow exist.

If any item is uncertain, publish source only and hold binary artifacts until
the uncertainty is resolved.
