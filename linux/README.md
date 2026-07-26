# OpenA8DJ Linux Track

This directory owns the Linux driver track for OpenA8DJ / Native Instruments
Audio 8 DJ. The current state is a design and audit surface only:

```text
diagnostic only, sound quality not validated
```

No Linux kernel module is ready for installation, no hardware validation has
been run from this worktree, and no audiophile-quality claim is valid until the
physical validation ladder in `QUALITY_AND_PERFORMANCE_GATES.md` is complete.

## Scope

The target Linux implementation is an ALSA-native driver path for Audio 8 DJ:

- PCM playback: 8 channels, exposed as A/B/C/D stereo pairs.
- PCM capture: 8 channels, exposed as A/B/C/D stereo pairs.
- First validated rates: 44.1 kHz and 48 kHz.
- Deferred rates: 88.2 kHz and 96 kHz, only after physical validation.
- Full-duplex operation with honest `hw_params`, `prepare`, `trigger`,
  `pointer`, and delay behavior.
- ALSA controls for Audio 8 DJ input mode, ground lift flags, software lock,
  profiles, and diagnostics.
- ALSA rawmidi input/output visible to `aconnect`, PipeWire, JACK, and DAWs.
- PREEMPT_RT-friendly hot paths with preallocated URBs, fixed rings, bounded
  completion handlers, and no hidden XRUNs.

## Ownership Boundaries

Primary Linux ownership:

- `linux/`
- `agents/linux-driver-agent/`
- `docs/Linux*`
- `docs/LINUX_*`
- `scripts/linux-*`
- `tools/linux/`

Do not edit macOS, Windows, HAL, installer, or control-surface paths from this
track unless the integration need is explicit and documented.

## Hardware Lock

Build-only, documentation, and static analysis work do not need the shared
hardware lock.

Any action that touches Audio 8 DJ hardware, USB live state, iRig capture,
playback/capture, Traktor, live driver installation/reload, or CPU/audio
measurements must use:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

If the lock is occupied, do not force it.

## Current Package

The initial Linux track surface includes:

- Linux architecture notes.
- macOS/Windows hardware lesson integration for Linux.
- Linux configuration and profile model.
- Linux quality/performance gates.
- A first `snd-usb-caiaq` audit.
- A placeholder `linux/driver/` scaffold that refuses real module builds.
- Linux packaging strategy for Debian/Ubuntu first and RPM second.
- Linux candidate payload contract for complete experimental packages.
- Linux handoff for future merge/recovery in `HANDOFF_2026-06-19.md`.

## Packaging

Linux packaging is tracked in `PACKAGING.md`.

Every Linux candidate must also satisfy `CANDIDATE_PAYLOAD.md`. A candidate is
not complete if it only ships a driver binary or `.ko`; it needs the surrounding
tools, configuration/profile schema, rollback path, diagnostics, metadata,
provenance, and validation labels.

Packaging priorities:

- Debian/Ubuntu and derivatives first.
- RPM-based distributions second, with `akmods`, DKMS, or kmod strategy chosen
  after the driver path is settled.

The packaging scaffold lives under:

- `linux/packaging/debian/`
- `linux/packaging/rpm/`

The first experimental package set is generated under:

```text
dist/linux/experimental/opena8dj-linux-experimental-0.1.0~experimental20260622/
```

It includes:

- `opena8dj-linux-experimental_0.1.0~experimental20260622_all.deb`
- `opena8dj-linux-experimental-0.1.0-0.experimental20260622.noarch.rpm`
- `opena8dj-linux-experimental-0.1.0~experimental20260622.tar.gz`
- `opena8dj-linux-candidate.json`
- `README-FIRST.md`
- `SHA256SUMS`

Those packages install tools, docs, profile metadata, and a conservative udev
tag. They rely on the distro's in-kernel `snd-usb-caiaq` driver and do not
install a replacement kernel module. No packaging path is allowed to auto-load,
bind, reset, or test live Audio 8 DJ hardware.

The current experimental package set is generated under:

```text
dist/linux/experimental/opena8dj-linux-experimental-0.1.1~experimental20260726/
```

It keeps the same diagnostic-only scope and adds:

- `opena8dj-linuxctl hardware-model`
- `hardware-model.json` in verify reports
- macOS/Windows-aligned profile aliases and control postures
- packaged `linux/MACOS_WINDOWS_HARDWARE_LESSONS.md`
- expanded profile schema with USB endpoints, A/B/C/D topology, hot-path rules,
  and validation policy

The next engineering step is to complete a source-level audit against the exact
kernel baseline and decide whether the safest path is an upstream-style
`snd-usb-caiaq` extension or a new `snd-opena8dj` module. Real packaging
metadata should follow that decision.
