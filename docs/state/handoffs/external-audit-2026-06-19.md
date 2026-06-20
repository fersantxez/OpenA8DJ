# Handoff: External Audit Follow-Up

Date: 2026-06-19

Purpose: preserve the current repository/release state and hand off the
external audit findings to the next implementation-planning agent.

## 2026-06-20 Status Update

This document is historical audit handoff evidence. The original findings below
were used to clean up the public 0.5.0 release surface.

Current follow-up status:

- `main` has since moved beyond the commit recorded in this handoff.
- OpenA8DJ 0.5.0 is the canonical macOS C++ stable baseline.
- Apple Developer Program membership is active.
- Local Developer ID Application and Developer ID Installer certificates are
  installed.
- The `OpenA8DJNotary` keychain profile is stored and validated.
- Replacement 0.5.0 assets have been rebuilt with Developer ID signatures.
- Apple notarization submissions are pending service completion before public
  replacement assets can be uploaded.

Do not treat the older unsigned-release wording below as the current desired
public documentation state. It is retained to show what the external audit
asked the project to correct.

## Current Public State

- Repository: `https://github.com/fersantxez/OpenA8DJ`
- Canonical branch: `main`
- Current `main` commit: `2b939d6cb383216f3176bbfb72a0504cfaf3d5fc`
- Current release tag: `v0.5.0`
- Release URL: `https://github.com/fersantxez/OpenA8DJ/releases/tag/v0.5.0`
- `v0.5.0` points to `main`.
- `origin/HEAD` points to `origin/main`.

Published release assets:

```text
OpenA8DJ-0.5.0.dmg
OpenA8DJ-0.5.0.pkg
OpenA8DJ-0.5.0-checksums.txt
opena8dj-tools-0.5.0.dmg
opena8dj-tools-0.5.0.pkg
```

Remote branches after cleanup:

```text
main
legacy
windows/rebuild-surface
linux/full-driver-agent
rust/modular-core-spike
```

Known branch roles:

- `main`: canonical macOS C++ driver line.
- `legacy`: preserved C/Objective-C historical branch.
- `windows/rebuild-surface`: experimental Windows work, not validated.
- `linux/full-driver-agent`: experimental Linux placeholder/branch, not validated.
- `rust/modular-core-spike`: Rust lab/oracle, not macOS runtime.

## What Was Already Done

The repository was cleaned so the public narrative no longer presents C++ as a
secondary redesign line. The current README presents OpenA8DJ as an independent
open-source preservation project and presents macOS C++ 0.5.x as the canonical
line.

The `driverkit/cpp-redesign` remote branch was removed after `main` became the
canonical line.

The stale `codex/fix-48khz-playback` remote branch was removed because it was
already contained in `main` and added public branch noise.

The local HAL installed on this laptop was removed after publication so a user
can install from GitHub for testing. The hardware lock was acquired and released
for that uninstall. Final observed state after uninstall:

```text
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver absent
hardware lock free
```

## Verification Already Run

From `main` at `2b939d6`:

```sh
make all
make control-center
make dist
cmake -S . -B build/cmake-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake-release --parallel
ctest --test-dir build/cmake-release --output-on-failure
make smoke-hal parity-smoke-hal
```

Results:

- Build/package: PASS.
- CMake build: PASS.
- CTest: PASS, 88/88.
- HAL smoke/parity: PASS.
- Generated release assets: PASS.
- GitHub release download + checksum verification: PASS.

Downloaded GitHub release checks:

```text
OpenA8DJ-0.5.0.dmg: OK
OpenA8DJ-0.5.0.pkg: OK
opena8dj-tools-0.5.0.dmg: OK
opena8dj-tools-0.5.0.pkg: OK
```

Current 0.5.0 release checksums from `main` evidence:

```text
afca883993a85bdf65468b23030897f5d05be4115ae687c2abbb7641dffa3c49  OpenA8DJ-0.5.0.dmg
7b0b77bf623a2204d471a86805c36bbc7335abfce197012024d36210bfad0770  OpenA8DJ-0.5.0.pkg
1c03ee1f6effeef913e172f8fcf07500a08d539784f8d7acaccba36081a03cb9  opena8dj-tools-0.5.0.dmg
fbb066b8a991c2f298d2a3c009e755cc75fc5b52e2a5adfee32b317bbd490564  opena8dj-tools-0.5.0.pkg
```

## External Audit Verdict

External audit verdict: **Approved with reservations**.

The audit confirmed:

- `origin/main` presents OpenA8DJ 0.5.x as the canonical macOS C++ line.
- Legacy C, Rust, Windows, and Linux are separated at the narrative level.
- Release `v0.5.0` exists, points to `main`, contains expected assets, and
  checksums verify.

The audit found the repo is not yet fully public-ready because:

- the signing/notarization language is still too ambiguous;
- public release notes expose internal/candidate wording;
- `docs/STABLE_0.5.0_REFERENCE.md` still says the release is not public;
- public validation evidence is too internal for an external reader;
- a user needs slightly better post-install guidance for Audio MIDI Setup.

## Audit Findings To Address

### P1: Signing/notarization wording is ambiguous

Affected areas:

- GitHub release `v0.5.0`
- `README.md`
- `docs/INSTALL.md`
- `docs/BUILD.md`
- DMG README/release notes as needed

Observed reality:

- The release packages have no usable Developer ID signature.
- `pkgutil --check-signature` reports no signature for the PKGs.
- `spctl` rejects PKG/DMG with no usable signature.

Required public wording:

```text
OpenA8DJ 0.5.0 release assets are unsigned, not Developer ID signed, and not
Apple-notarized. macOS Gatekeeper will likely block them unless manually
approved after checksum verification.
```

Planning decision needed:

- Mark `v0.5.0` as prerelease/preview, or retitle it clearly as an unsigned
  preview.
- Align `docs/BUILD.md` with the real preview policy, not only the future signed
  release policy.

### P1: Public release notes expose internal candidate wording

Affected areas:

- GitHub release `v0.5.0`
- `docs/RELEASE_NOTES_0.5.0.md`
- possibly `docs/STABLE_0.5.0_REFERENCE.md`

Problem:

- Public notes still expose internal names such as
  `hal-timecode-frozen-good-output3072-candidate`.
- Public notes mention `local-analysis/...` paths and local HAL hashes in the
  public stable reference.

Expected direction:

- Public-facing text should say `0.5.0 stable build`, `0.5.0 macOS baseline`,
  or `0.5.0 release reference`.
- Move internal target names, local paths, and internal HAL hashes into
  `docs/TEST_EVIDENCE.md` or a clearly marked technical appendix.

### P1: Stable reference doc contradicts public release state

Affected area:

- `docs/STABLE_0.5.0_REFERENCE.md`

Problem:

- It still says 0.5.0 is not yet a public GitHub release.

Required fix:

- Update it to say this is the public GitHub release reference for OpenA8DJ
  0.5.0, or move/retitle it as historical release-prep evidence.

### P2: README omits direct tools PKG

Affected area:

- `README.md`

Required fix:

Add:

```text
opena8dj-tools-0.5.0.pkg: optional tools direct installer package
```

### P2: Public validation summary is missing

Affected areas:

- `README.md`
- `docs/TEST_EVIDENCE.md`
- optional new doc such as `docs/PUBLIC_VALIDATION_SUMMARY.md`

Problem:

- Evidence exists but is too internal for an external user.

Required public summary should include:

- what was tested;
- hardware/software used;
- validated sample rates;
- workflows that passed;
- workflows not yet validated;
- release asset hashes;
- exact signing/notarization state.

### P2: Experimental branch names can confuse users

Affected public branches:

- `windows/rebuild-surface`
- `linux/full-driver-agent`
- `rust/modular-core-spike`

Problem:

- `linux/full-driver-agent` especially sounds more complete than it is.

Possible responses:

- Keep branches but strengthen README wording: only `main` and GitHub Releases
  are for users.
- Retitle/rename experimental branches later if desired.
- Do not merge experimental branches into `main`.

### P2: Audio MIDI Setup guidance is too thin

Affected areas:

- `README.md`
- `docs/INSTALL.md`
- `docs/TRAKTOR_TIMECODE.md`

Required mini-section:

```text
After install, open Audio MIDI Setup and confirm Open Audio 8 DJ appears with 8
inputs and 8 outputs. If it does not appear, reconnect the Audio 8 DJ once, then
reopen the audio app.
```

### P3: Historical 0.4.0 docs should be more clearly isolated

Affected areas:

- `docs/RELEASE_NOTES_0.4.0.md`
- `docs/TEST_EVIDENCE.md`

Required fix:

- Add clearer historical headings.
- Avoid linking 0.4.0 from current docs except as changelog/history.

## Non-Negotiable Safety Rules For The Next Agent

- Do not install packages unless explicitly asked.
- Do not restart CoreAudio unless explicitly asked.
- Do not touch USB, Traktor, default devices, sample rate, or hardware without
  the hardware lock.
- Do not hand a new build to the user for sound testing unless the exact
  artifact has passed the project sound-quality validation rule, or it is
  explicitly labeled diagnostic-only.
- For this audit follow-up, the expected first deliverable is a plan, not code.
- If implementation is later requested, keep edits documentation/release-metadata
  scoped unless there is a concrete reason to alter driver code.

## Recommended First Plan For Successor

1. Re-verify current `main`, release assets, signing state, and branch list.
2. Draft a small public-doc cleanup plan grouped by P1/P2/P3.
3. Decide whether to mark `v0.5.0` as prerelease/unsigned preview.
4. Plan exact wording updates for README, INSTALL, RELEASE_NOTES, BUILD, DMG
   README, and STABLE reference.
5. Plan `docs/PUBLIC_VALIDATION_SUMMARY.md`.
6. Plan branch visibility/naming policy for Windows/Linux/Rust experimentals.
7. Define verification commands after implementation:
   - stale wording grep;
   - release asset presence;
   - checksum verification;
   - package signature status;
   - docs link/path sanity;
   - no hardware touched.

## Suggested Prompt Location

The user requested a prompt for the successor agent. Use this handoff as the
source of truth for the next prompt.
