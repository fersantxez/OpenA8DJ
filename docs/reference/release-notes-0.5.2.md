# OpenA8DJ 0.5.2 Lifecycle Preview

This public pre-release packages lifecycle-stability work for limited testing.
OpenA8DJ 0.5.1 remains the stable release. This build uses ad-hoc code signing
and is not Apple-notarized, so macOS will require an explicit approval after
the download checksum is verified.

## Changes

- Add bounded Core Audio IPC reply waits and lifecycle audit coverage.
- Add hardware-operation locking, timeout-aware health probes, and a
  journaled filesystem transaction core with fixture-based rollback tests.
- Add offline load/unload, device-enumeration, and repository-path tests.
- Preserve the existing 0.5.1 audio profile; this preview is focused on
  lifecycle reliability.

## Validation

- Debug offline suite before the release metadata bump: 99/99 passed.
- Release offline suite after the release metadata bump: 99/99 passed. The
  separate manifest-identity test still points at the earlier captured run,
  so it reports stale hashes for the new version metadata and was excluded
  from this 99-test run.
- Candidate safety gate on macOS: one load/restart/enumeration cycle passed;
  Core Audio reported the Audio 8 DJ as 8 inputs and 8 outputs at 48 kHz.
- Candidate binary hash matched the locally installed executable.

The candidate has not completed an extended Traktor soak or every planned
physical lifecycle case. In particular, this preview is not a certification of
all R02–R12 work. The prior 200-cycle physical run was against the 0.5.1
baseline, not this candidate.

## macOS Approval

The HAL and tools use ad-hoc code signatures. The PKG and DMG are unsigned, and
no artifact is Developer ID signed or Apple-notarized. Developer ID signing
stalled locally, and Apple notarization returned an agreement-required error.
After verifying the published SHA-256 checksum, use Finder's Control-click >
Open flow or System Settings > Privacy & Security > Open Anyway if macOS blocks
the installer. Do not disable Gatekeeper. Users who want the stable notarized
download should use the 0.5.1 release.

OpenA8DJ is an independent compatibility project. It is not affiliated with,
endorsed by, sponsored by, or certified by Native Instruments.
