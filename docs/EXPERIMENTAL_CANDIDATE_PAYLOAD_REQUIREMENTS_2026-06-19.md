# Experimental Candidate Payload Requirements - 2026-06-19

## Purpose

Windows and Linux candidates must never be "just the driver binary".

Every experimental candidate must be a complete, inspectable package with the
driver, companion tools, configuration surface, installation/rollback path,
verification tooling, metadata, and limitations.

This protects the macOS/C++ mainline, makes tester feedback useful, and avoids
shipping a mysterious kernel binary with no support surface.

## Required Candidate Contents

Every platform candidate must include:

1. Driver payload
   - Windows: INF, SYS, CAT.
   - Linux: module source/DKMS package or built module only for an approved
     internal artifact.

2. User-mode tools
   - status command;
   - diagnostics command;
   - profile/control command;
   - routing inspection command where available.

3. Configuration/profile model
   - playback profile;
   - DVS vinyl profile;
   - DVS CD/line profile;
   - phono/recording profile;
   - A/B/C/D routing labels;
   - import/export path when implemented.

4. Installer and uninstaller
   - driver installer;
   - tools installer;
   - uninstall/rollback path;
   - reboot-required handling where applicable.

5. Verification tooling
   - install verification;
   - driver presence;
   - hardware detection;
   - surface/topology/diagnostics output;
   - generated JSON or text report.

6. Documentation
   - `README-FIRST` or equivalent in the downloadable package;
   - tester install guide;
   - uninstall guide;
   - limitations;
   - exact readiness label.

7. Metadata
   - platform;
   - architecture;
   - source branch;
   - source commit;
   - build host;
   - build timestamp;
   - signing state;
   - SHA256 hashes;
   - validation status;
   - known blockers.

8. Debug/support material
   - symbols policy;
   - logs/report path;
   - exact command for collecting diagnostics;
   - internal QA symbols when safe.

9. Provenance and licensing
   - no Native Instruments binaries;
   - no Native Instruments firmware payloads;
   - no Native Instruments branding assets;
   - third-party licenses listed when used.

## Windows Candidate Payload

Driver package:

- `OpenA8DJUsb.inf`
- `OpenA8DJUsb.sys`
- `OpenA8DJUsb.cat`
- optional internal PDBs
- signing manifest

Tools package:

- `opena8djctl.exe`
- future `OpenA8DJ Control Center.exe`
- diagnostic collector when implemented
- Start Menu/PATH integration when implemented

Package surface:

- separate driver installer;
- separate tools installer;
- optional bundle installer;
- `README-FIRST.txt`;
- `verify.cmd`;
- `uninstall.cmd`;
- `installer-manifest.json`;
- SHA256 hashes.

## Linux Candidate Payload

Driver package:

- `opena8dj-dkms` while out-of-tree; or
- upstream/in-tree integration notes when no DKMS package is needed;
- module configuration;
- Secure Boot/module signing notes where applicable.

Tools package:

- `opena8dj-linuxctl` or equivalent;
- diagnostics/report command;
- profile/routing command.

Configuration packages where needed:

- `opena8dj-udev`;
- `opena8dj-alsa-ucm`;
- profile/config schema.

Package surface:

- `.deb` first for Debian/Ubuntu;
- `.rpm` second for Fedora/RHEL derivatives;
- uninstall/rollback instructions;
- verification report;
- `README-FIRST` equivalent;
- no automatic hardware bind/load during early experimental install unless the
  operator explicitly chooses that step.

## Merge Gate

A Windows or Linux candidate cannot be proposed for mainline merge until its
payload contract exists and is organized under platform-owned paths.

The minimum acceptable state is:

- driver payload represented;
- tools payload represented;
- install/uninstall represented;
- verification represented;
- documentation represented;
- hashes/metadata represented;
- limitations represented.

Anything less is a prototype, not a candidate.
