# Linux Packaging Strategy

This document defines the packaging track for Debian, Ubuntu, and derivatives
first, with an RPM lane for Fedora, Red Hat, and compatible derivatives.

Current state:

```text
diagnostic only, sound quality not validated
```

The first package set produced from this tree is experimental and diagnostic
only. It is stored in:

```text
dist/linux/experimental/opena8dj-linux-experimental-0.1.0~experimental20260622/
```

No package should load, bind, reload, or test live Audio 8 DJ hardware without
explicit operator steps and the shared hardware lock during validation.

The current package set produced after integrating macOS/Windows hardware
lessons is stored in:

```text
dist/linux/experimental/opena8dj-linux-experimental-0.1.1~experimental20260726/
```

Every `.deb`, `.rpm`, DKMS, akmods, or built-module candidate must satisfy the
candidate payload contract in `CANDIDATE_PAYLOAD.md`. The package artifact is
not acceptable if it ships only a `.ko` or driver source without tools,
configuration/profile schema, rollback, diagnostics, metadata, provenance, and
readiness labels.

## Priorities

Primary lane:

- Debian.
- Ubuntu.
- Debian/Ubuntu derivatives that preserve DKMS, udev, ALSA, and PipeWire
  packaging conventions.

Secondary lane:

- Fedora.
- Red Hat Enterprise Linux derivatives.
- RPM-based systems using `akmods`, DKMS, or kernel module packaging.

## Package Split

Recommended package names:

| Package | Purpose | Initial readiness |
| --- | --- | --- |
| `opena8dj-dkms` | Temporary out-of-tree kernel module source and DKMS integration. | Not implemented. |
| `opena8dj-tools` | User-space status/control/diagnostic tools. | Not implemented. |
| `opena8dj-alsa-ucm` | ALSA UCM profiles and channel naming policy if needed. | Not implemented. |
| `opena8dj-udev` | udev rules for permissions, stable tags, and user-space access. | Not implemented. |
| `opena8dj` | Optional meta-package depending on the supported pieces. | Not implemented. |

The first real packaging pass should keep driver and tools separable. This lets
users install tools for inspection without automatically installing or loading a
kernel module.

## Experimental Package Set - 2026-06-22

Built artifacts:

- `opena8dj-linux-experimental_0.1.0~experimental20260622_all.deb`
- `opena8dj-linux-experimental-0.1.0-0.experimental20260622.noarch.rpm`
- `opena8dj-linux-experimental-0.1.0~experimental20260622.tar.gz`
- `opena8dj-linux-candidate.json`
- `README-FIRST.md`
- `SHA256SUMS`

Package contents:

- `/usr/bin/opena8dj-linuxctl`
- `/usr/lib/udev/rules.d/70-opena8dj-audio8dj.rules`
- `/usr/share/opena8dj-linux-experimental/profile-schema.json`
- documentation under `/usr/share/doc/opena8dj-linux-experimental/`

Driver channel:

```text
in-kernel snd-usb-caiaq
```

The packages do not ship a kernel module. They are intended for a Linux machine
whose distro kernel already has `CONFIG_SND_USB_CAIAQ` enabled. They install
diagnostic and profile tooling only.

The `.deb` was generated as Debian binary package format 2.0 with
`control.tar.gz` and `data.tar.gz`. The `.rpm` was generated as an RPM v3.0
binary package with gzip-compressed cpio payload. This host does not have
`dpkg-deb`, `rpm`, or `rpmbuild`, so final install verification still must be
run on the Linux test machine.

## Experimental Package Set - 2026-07-26

Built artifacts:

- `opena8dj-linux-experimental_0.1.1~experimental20260726_all.deb`
- `opena8dj-linux-experimental-0.1.1-0.experimental20260726.noarch.rpm`
- `opena8dj-linux-experimental-0.1.1~experimental20260726.tar.gz`
- `opena8dj-linux-candidate.json`
- `README-FIRST.md`
- `SHA256SUMS`

Additions over the 2026-06-22 package:

- `opena8dj-linuxctl hardware-model`
- `hardware-model.json` in verify reports
- macOS/Windows-derived source oracles in the diagnostic JSON
- DVS/phono/line profile control postures aligned with macOS/Windows
- profile aliases compatible with macOS-style operator names
- packaged `MACOS_WINDOWS_HARDWARE_LESSONS.md`

The package remains diagnostic-only. It still does not ship a kernel module,
load `snd-usb-caiaq`, bind/unbind USB, restart audio services, play audio,
record audio, or validate sound quality.

## Candidate Payload Contract

Packaging deliverables must implement `CANDIDATE_PAYLOAD.md`.

Minimum candidate payload:

- driver/module source or built module for the selected channel.
- `opena8dj-tools` or an explicit internal-only exception.
- config/profile schema.
- udev rules if needed.
- ALSA UCM profiles if needed.
- docs and `README-FIRST`.
- uninstall/rollback path.
- diagnostics and verify report tooling.
- hashes and build metadata.
- symbols/debug info policy.
- license/provenance and Native Instruments no-payload policy.
- validation labels and limitations.

This applies equally to Debian/Ubuntu `.deb` candidates and RPM/akmods/DKMS
candidates. A package that only installs a kernel object is incomplete.

## DKMS vs In-Tree Module

### Temporary DKMS Lane

DKMS is the likely first distribution mechanism while the Linux driver is still
outside the kernel tree.

Benefits:

- Works on Debian/Ubuntu systems with matching kernel headers.
- Lets testers rebuild for local kernels.
- Keeps packaging iteration independent from kernel release cadence.

Costs and risks:

- Build failures are common when kernel APIs drift.
- Secure Boot module signing must be handled explicitly.
- DKMS install hooks can create unsafe expectations if they auto-load hardware.
- DKMS success does not prove audio quality.

Temporary package target:

```text
opena8dj-dkms
```

It should install source under a versioned DKMS path and register with DKMS, but
initial packages must not auto-bind Audio 8 DJ hardware as a hidden side effect.

### Upstream or In-Tree Lane

The long-term target is either:

- an upstreamable Audio 8 DJ improvement to `snd-usb-caiaq`, or
- a focused in-tree `snd-opena8dj` module if CAIAQ cannot satisfy the product
  requirements cleanly.

Benefits:

- Better kernel integration.
- Lower install friction.
- Normal distro kernel QA.
- Less DKMS breakage.

Costs and risks:

- Longer review cycle.
- Must preserve behavior for other CAIAQ devices if extending CAIAQ.
- Release timing depends on kernel and distro schedules.

The package strategy should treat DKMS as a bridge, not the final quality bar.

## Debian/Ubuntu Layout

Planned source package areas:

```text
linux/packaging/debian/
  README.md
  control              # future
  rules                # future
  changelog            # future
  copyright            # future
  opena8dj-dkms.dkms   # future
  opena8dj-tools.install
  opena8dj-alsa-ucm.install
  opena8dj-udev.install
  postinst             # future, conservative
  prerm/postrm         # future, conservative
```

This package surface is not present yet beyond README scaffolding.

## Install Behavior

Preferred install behavior for early packages:

- Install files.
- Register DKMS source if the user installed `opena8dj-dkms`.
- Build only when headers are available.
- Do not force module load against live hardware.
- Do not unbind existing drivers automatically.
- Do not change default audio devices.
- Do not run playback/capture tests.
- Print a clear diagnostic readiness label.

Potential future explicit operator commands:

```sh
sudo modprobe opena8dj
opena8dj-linuxctl status
opena8dj-linuxctl diagnostics
```

If extending `snd-usb-caiaq`, the module name and modprobe flow may remain
CAIAQ-specific. The package must not hide that from operators.

## `postinst`

Future `postinst` scripts should be conservative:

- Register DKMS.
- Run `depmod` only after a successful module build.
- Install udev rules and trigger a rules reload if needed.
- Install ALSA UCM files if packaged.
- Print next-step instructions.

Future `postinst` scripts must not:

- Load the module automatically for attached Audio 8 DJ hardware.
- Bind or unbind USB drivers.
- Restart PipeWire/JACK.
- Run audio tests.
- Claim audiophile readiness.

## Uninstall

Future uninstall behavior should:

- Remove user-space tools installed by the package.
- Deregister DKMS versions owned by the package.
- Remove packaged udev and ALSA/UCM files.
- Avoid touching unrelated ALSA, PipeWire, JACK, or user configuration.
- Avoid resetting USB devices.
- Avoid unloading modules that may be in active use unless the user explicitly
  requested it.

## Modprobe

Future module configuration may need:

```text
/etc/modprobe.d/opena8dj.conf
```

Possible uses:

- select debug verbosity.
- select conservative defaults.
- avoid accidental binding until explicit validation.

Module options must be documented and stable. Debug options must not create hot
path logging in steady-state audio.

## udev

A future `opena8dj-udev` package may install rules for:

- Native Instruments Audio 8 DJ USB ID `17cc:1978`.
- group access for tools where needed.
- tags consumed by user-space status tools.

udev rules must not trigger playback, capture, driver reload, USB reset, or
quality tests.

## ALSA UCM

An `opena8dj-alsa-ucm` package may be useful if ALSA/PipeWire need a stronger
user-space profile for:

- A/B/C/D pair names.
- 8 playback and 8 capture channel mapping.
- DVS profiles.
- phono and line recording profiles.
- MIDI bridge naming.

UCM must mirror kernel truth. It must not mask driver channel swaps or timing
bugs.

## Tools Package

`opena8dj-tools` should be safe to install without a driver package.

Implemented tool surface:

- `opena8dj-linuxctl status`
- `opena8dj-linuxctl diagnostics`
- `opena8dj-linuxctl controls`
- `opena8dj-linuxctl list-profiles`
- `opena8dj-linuxctl apply-profile traktor-dvs-vinyl --yes`
- `opena8dj-linuxctl set-control input-mode phono --yes`
- `opena8dj-linuxctl verify --report-dir <dir>`

Hardware-affecting commands must clearly say what they will do and must be used
only inside a coordinated hardware test window.

## Security and Operator Safety

No early package should surprise the operator by taking control of live Audio 8
DJ hardware.

Rules:

- Package install is not a hardware test.
- Package install is not a quality gate.
- Package install must not imply audio readiness.
- Live binding, playback, capture, USB reset, module reload, and latency/CPU
  measurement require explicit operator action during validation.
- During project validation, those actions require:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

If the lock is occupied, do not force it.

## Readiness Gates Before Publishing `.deb`

No `.deb` should be published outside the project until all applicable gates
pass for the exact package artifacts:

1. Source audit complete against the selected kernel baseline.
2. Package builds reproducibly on supported Debian/Ubuntu releases.
3. DKMS install/remove works with matching headers.
4. Secure Boot behavior is documented.
5. Candidate payload contract is complete.
6. Install does not auto-bind or run hardware tests.
7. Uninstall removes only package-owned files.
8. Lintian or equivalent packaging checks pass, or exceptions are documented.
9. Enumeration gate passes on real Audio 8 DJ hardware.
10. PCM smoke gate passes at 44.1 and 48 kHz.
11. Routing matrix passes for A/B/C/D.
12. CPU/latency gate passes.
13. Physical capture gate passes for the exact built and loaded artifact.
14. DVS/MIDI gate passes for required profiles.
15. Resilience gate passes.

Before physical capture passes, the package label remains:

```text
diagnostic only, sound quality not validated
```

## RPM Lane

RPM packaging is secondary but should be designed early enough to avoid Debian
assumptions leaking into the driver layout.

Recommended future paths:

```text
linux/packaging/rpm/
  README.md
  opena8dj.spec         # future
  sources/              # future, if needed
```

Possible package names:

- `opena8dj-kmod`
- `opena8dj-akmod`
- `opena8dj-dkms`
- `opena8dj-tools`
- `opena8dj-alsa-ucm`
- `opena8dj-udev`

Fedora-style `akmods` may be preferable to DKMS for Fedora users, while RHEL
derivatives may need a kABI-aware `kmod` lane or DKMS depending on target
policy.

RPM packages must keep the same safety rule: package installation must not
secretly bind hardware or claim audio quality.

RPM candidates must also satisfy `CANDIDATE_PAYLOAD.md`; an RPM that ships only
`opena8dj.ko`, `snd-opena8dj.ko`, or a patched `snd-usb-caiaq.ko` is not a
complete candidate.

## Current Next Step

Run the generated package set on the Linux test machine:

```sh
sudo apt install ./opena8dj-linux-experimental_0.1.0~experimental20260622_all.deb
opena8dj-linuxctl status
opena8dj-linuxctl diagnostics --json --controls
opena8dj-linuxctl verify --controls --report-dir ~/opena8dj-linux-report
```

or:

```sh
sudo dnf install ./opena8dj-linux-experimental-0.1.0-0.experimental20260622.noarch.rpm
opena8dj-linuxctl status
opena8dj-linuxctl diagnostics --json --controls
opena8dj-linuxctl verify --controls --report-dir ~/opena8dj-linux-report
```

If the Linux test host shows that the distro `snd-usb-caiaq` surface is
insufficient, the next implementation step is an upstream-style CAIAQ patch or
the documented `snd-opena8dj` fallback.
