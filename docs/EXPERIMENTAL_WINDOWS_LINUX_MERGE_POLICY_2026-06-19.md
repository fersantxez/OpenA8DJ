# Experimental Windows/Linux Merge Policy - 2026-06-19

## Decision

Windows and Linux deliverables are experimental candidates until explicitly
promoted. The macOS/C++ mainline remains the priority path.

Do not merge Windows or Linux candidate work into the macOS/C++ mainline until
the repository layout, documentation, build scripts, packages, and binaries are
organized so they cannot interfere with macOS development, packaging, or user
testing.

## Candidate Labels

Use these labels consistently:

- `windows experimental candidate`
- `linux experimental candidate`
- `diagnostic only, sound quality not validated`
- `install test only`
- `audio test candidate`
- `release candidate`

Do not use:

- `production`
- `public release`
- `audiophile quality`
- `ready for Traktor`

unless the exact built artifact passed the relevant validation gates.

## Isolation Rules

### macOS/C++ Mainline

Protected paths:

- `src/hal/`
- `macos/`
- macOS installer/package resources
- macOS Control Center resources
- existing macOS QA scripts
- main release documentation

Windows/Linux work must not refactor or destabilize those paths as a side
effect of adding experimental support.

### Windows

Windows-owned paths:

- `windows/`
- `docs/WINDOWS*`
- Windows-specific installer/package docs and scripts

Windows artifacts must stay under:

```text
windows/dist/
windows/installer/
local-analysis/windows/
```

No Windows binary or generated installer should be committed unless it is an
intentional release artifact and documented as such.

### Linux

Linux-owned paths:

- `linux/`
- `agents/linux-driver-agent/`
- `docs/LINUX*`
- Linux-specific scripts such as `scripts/linux-*`

Linux artifacts must stay under:

```text
linux/
linux/packaging/
local-analysis/linux/
```

No Linux `.ko`, `.deb`, `.rpm`, DKMS build output, or generated package should
be committed unless it is an intentional release artifact and documented as
such.

## Required Pre-Merge Review

Before merging either experimental candidate toward mainline:

1. Confirm branch/worktree isolation.
2. Confirm generated artifacts are ignored or stored in approved release paths.
3. Confirm macOS build and packaging still pass.
4. Confirm Windows/Linux build scripts do not run from default macOS targets.
5. Confirm no install script touches hardware without the shared lock.
6. Confirm docs clearly mark experimental status.
7. Confirm release notes do not imply production readiness.
8. Confirm no Native Instruments proprietary payloads or branding assets were
   introduced.
9. Confirm package names and installer names are platform-specific.
10. Confirm rollback/uninstall instructions exist for each platform.
11. Confirm the candidate satisfies
    `docs/EXPERIMENTAL_CANDIDATE_PAYLOAD_REQUIREMENTS_2026-06-19.md`.

## Binary Policy

Default:

- do not commit generated binaries;
- do not commit generated driver packages;
- do not commit generated installers.

Exception:

- a signed experimental release artifact can be attached or published through a
  release process, with hashes and validation reports.

Required metadata for any candidate binary:

- platform;
- architecture;
- source branch;
- source commit;
- build host;
- signing state;
- SHA256 hashes;
- validation status;
- exact limitations.

## Payload Policy

Windows and Linux candidates must not be only driver binaries. Each candidate
needs the driver payload, user-mode tools, configuration/profile surface,
installer/uninstaller, verification tooling, metadata, documentation, and
limitations described in
`docs/EXPERIMENTAL_CANDIDATE_PAYLOAD_REQUIREMENTS_2026-06-19.md`.

## Merge Gate For Windows

Windows cannot be merged as more than experimental until:

- driver and tools build on a Windows WDK host;
- driver installer and tools installer are separate;
- test-signed install flow works on a clean Windows test machine;
- uninstall and reinstall pass;
- `verify-driver.ps1` produces a report;
- hardware lock behavior is documented and verified for install/uninstall;
- audio endpoint/streaming status is truthfully reported.

Production/public Windows release additionally requires:

- Microsoft-signed catalog;
- signed installer(s);
- clean install/uninstall/upgrade matrix;
- real audio validation if audio endpoints are claimed.

## Merge Gate For Linux

Linux cannot be merged as more than experimental until:

- Debian/Ubuntu packaging plan is represented in `linux/packaging/debian/`;
- RPM plan is represented in `linux/packaging/rpm/`;
- driver implementation path is decided: `snd-usb-caiaq` extension or
  `snd-opena8dj`;
- build instructions are validated on a Linux host;
- no package install auto-loads/binds hardware unexpectedly;
- ALSA controls, rawmidi, routing, and quality gates are documented.

Production/public Linux release additionally requires:

- package builds on target distro;
- install/uninstall pass;
- module load/unload behavior is safe;
- ALSA/PipeWire/JACK enumeration passes;
- physical audio-quality validation passes for any audio claim.

## Final Merge Rule

Merge only when the platform candidate is organized enough that a macOS-focused
developer can ignore it completely during normal macOS work.

That is the bar.
