# Security And Responsible Testing

OpenA8DJ is an experimental driver stack. It runs inside Core Audio on macOS,
and the Windows workstream includes a kernel-mode WDK driver. Failures can
affect system audio, USB device state, or driver stability until the affected
component is restarted or removed.

## Supported Versions

Only the current `main` branch and explicitly published GitHub releases are
supported for testing.

Draft releases and GitHub Actions artifacts are development snapshots. Treat
them as unsupported unless a maintainer asks you to test a specific build.

## Reporting Security Issues

Please do not publish security-sensitive issues publicly before they are
triaged. Use GitHub private vulnerability reporting if it is available on this
repository. If that is not available, contact the repository owner through
GitHub and include enough detail to reproduce the issue privately.

Security-sensitive issues include:

- installer privilege escalation problems;
- unsafe file permissions in installed artifacts;
- crashes, hangs, or memory-safety failures triggered by untrusted input;
- issues that can affect unrelated Core Audio devices, Windows audio devices,
  or user data;
- driver install, uninstall, update, or signing problems that could weaken the
  operating system security model.

## Distribution Safety

Public macOS distribution should use:

- Developer ID signing for the HAL bundle and installer package;
- Apple notarization and stapling;
- published SHA-256 checksums;
- release notes that clearly identify unsupported or unvalidated features.

Public Windows distribution should use:

- Microsoft driver signing or an equivalent accepted Windows driver signing
  flow;
- release notes that clearly mark experimental features;
- validated install, uninstall, hotplug, audio, MIDI, and Traktor test results.

OpenA8DJ releases must not include Native Instruments driver binaries, firmware
blobs, installers, trademarks as branding assets, or other proprietary vendor
payloads.
