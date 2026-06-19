# Modern macOS Driver Architecture

OpenA8DJ `main` is the modern macOS driver line for Audio 8 DJ.

The current public preview is installable as a Core Audio HAL driver package.
That gives macOS audio applications a normal Core Audio device named
`Open Audio 8 DJ` with 8 inputs and 8 outputs. The package also installs the
MIDI/control bridge and command-line tools needed for Audio 8 DJ profile
control.

## User-Facing Shape

- Download from GitHub Releases.
- Open the DMG.
- Run the bundled PKG installer.
- Reconnect the Audio 8 DJ if macOS does not show it immediately.
- Select `Open Audio 8 DJ` in audio applications such as Traktor.

This is the supported public binary path. GitHub Actions artifacts, local
developer builds, and repackaged mirrors are not official release downloads.

## macOS Architecture

- Core Audio HAL plug-in for the current installable preview.
- IOUSBHost-based CAIAQ USB transport.
- CoreMIDI endpoints for MIDI I/O.
- Audio 8 DJ control bridge for hardware profile state such as
  `timecode-vinyl`.
- Pure C++ core contracts for packet layout, channel topology, routing,
  timecode policy, and performance-sensitive data movement.
- DriverKit/AudioDriverKit shell prepared for the future System Extension path.

The driver is user-space. It does not use the old macOS kernel-extension audio
driver model as the main architecture.

## Runtime Separation

- Data plane: audio frames, USB packet representation, timestamps, routing,
  preallocated buffers, underrun/overrun counters.
- Control plane: sample rate, buffer size, routing configuration, timecode
  profile, device state, recovery.
- Observability plane: counters, snapshots, logs, evidence files, and analysis
  outside the real-time audio path.

The real-time path must avoid allocation, blocking locks, per-buffer logging,
disk I/O, synchronous IPC, UI calls, and heavy diagnostics.

## Legacy Relationship

The older C/Objective-C implementation is preserved on the `legacy` branch. It
remains valuable as a historical baseline, especially for Linux/CAIAQ-derived
USB behavior and earlier physical-test learnings. New macOS driver work should
target `main`.
