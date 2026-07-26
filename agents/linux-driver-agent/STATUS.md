# Linux Driver Agent Status

Agent: OpenA8DJ Linux Driver Agent

Worktree:

```text
/Users/fer/dev/opena8dj-linux-agent
```

Branch:

```text
linux/full-driver-agent
```

Current state:

```text
diagnostic only, sound quality not validated
```

## 2026-06-19

The agent is running in the required Linux worktree and branch. This first
package creates the initial Linux track surface:

- `linux/README.md`
- `linux/ARCHITECTURE.md`
- `linux/CONFIGURATION_MODEL.md`
- `linux/QUALITY_AND_PERFORMANCE_GATES.md`
- `linux/SND_USB_CAIAQ_AUDIT.md`
- `linux/driver/README.md`
- `linux/driver/Makefile`

No hardware tests were run. The shared hardware lock was not acquired because
this package is documentation, source audit, and a safe placeholder scaffold
only.

## 2026-06-19 Packaging Update

The Linux track now documents Debian/Ubuntu packaging as the primary
distribution lane and RPM packaging as a secondary lane:

- `linux/PACKAGING.md`
- `linux/packaging/debian/README.md`
- `linux/packaging/rpm/README.md`

The packaging strategy covers `opena8dj-dkms`, `opena8dj-tools`,
`opena8dj-alsa-ucm`, `opena8dj-udev`, and a future `opena8dj` meta-package. It
keeps DKMS as a temporary bridge and upstream/in-tree kernel integration as the
long-term target.

No `.deb` or `.rpm` package is produced yet. No hardware tests were run, no
module was installed or loaded, and the shared hardware lock was not acquired.
The quality state remains:

```text
diagnostic only, sound quality not validated
```

## 2026-06-19 Candidate Payload Update

The Linux track now requires every experimental candidate to include the full
payload around the driver, not just a `.ko` or module source:

- `linux/CANDIDATE_PAYLOAD.md`

The contract covers driver/source channel, tools, config/profile schema, udev,
ALSA UCM, docs and `README-FIRST`, uninstall/rollback, diagnostics, hashes,
build metadata, debug-info policy, license/provenance, Native Instruments
no-payload policy, validation labels, and limitations.

`linux/PACKAGING.md` now requires Debian/Ubuntu and RPM candidates to satisfy
that payload contract before they can be considered complete.

No hardware tests were run, no lock was acquired, and no package/module was
installed.

## Boundaries

This agent owns Linux work. It must not mutate macOS, Windows, HAL, installer,
or control-surface paths unless explicitly authorized for integration docs.

## Next Step

Select the exact Linux kernel baseline, complete a source-level CAIAQ audit
against that tree, then decide whether to extend `snd-usb-caiaq` or create a
focused `snd-opena8dj` module. After that decision, create real Debian metadata
first and RPM metadata second.

## Session Handoff

The recovery handoff for this session is:

```text
linux/HANDOFF_2026-06-19.md
```

Read it before merging or continuing this branch.

## 2026-06-22 Architecture And Scaffold Update

The Linux architect pass added a concrete execution plan and safe read-only
implementation scaffolding:

- `docs-state/linux/linux-architect-state.md`
- `linux/IMPLEMENTATION_PLAN.md`
- `linux/BASELINES.md`
- `linux/LEGAL_AND_PROVENANCE.md`
- `linux/ENUMERATION_PLAN.md`
- `linux/Makefile`
- `linux/tools/opena8dj-linuxctl`
- `linux/tools/README.md`

The provisional implementation path is upstream-style `snd-usb-caiaq`
extension. A focused `snd-opena8dj` module remains a fallback only if CAIAQ
cannot provide honest timing, stable A/B/C/D routing, MIDI, controls,
diagnostics, and physical sound quality without invasive changes.

The key architecture correction is that CAIAQ's native PCM model is stereo-pair
substreams, not a single guaranteed ALSA 8x8 PCM. Linux parity is therefore
defined first as 8 physical inputs and 8 physical outputs with stable A/B/C/D
routing. A single 8-channel user-facing presentation can be added through UCM,
PipeWire/JACK policy, or helper profiles once the native surface is validated.

No hardware tests were run. No lock was acquired. No module was installed,
loaded, bound, or built. The readiness label remains:

```text
diagnostic only, sound quality not validated
```

## 2026-06-22 Experimental Package Update

The Linux track now builds installable experimental packages:

```text
dist/linux/experimental/opena8dj-linux-experimental-0.1.0~experimental20260622/
```

Artifacts:

- `opena8dj-linux-experimental_0.1.0~experimental20260622_all.deb`
- `opena8dj-linux-experimental-0.1.0-0.experimental20260622.noarch.rpm`
- `opena8dj-linux-experimental-0.1.0~experimental20260622.tar.gz`
- `opena8dj-linux-candidate.json`
- `README-FIRST.md`
- `SHA256SUMS`

These packages install `opena8dj-linuxctl`, profile schema, docs, and a udev
tag for `17cc:1978`. They rely on the distro's in-kernel `snd-usb-caiaq` driver
and do not install a replacement kernel module.

Local verification performed on macOS:

- CLI syntax check passed.
- `opena8dj-linuxctl self-test` passed.
- `opena8dj-linuxctl verify` wrote read-only reports.
- `.deb` is Debian binary package format 2.0 with control/data tarballs.
- `.rpm` is RPM v3.0 noarch with gzip cpio payload.
- checksums and candidate metadata were generated.

Linux install verification still must run on the target Linux computer. No
hardware validation has been run, and the label remains:

```text
diagnostic only, sound quality not validated
```

## 2026-07-26 macOS/Windows Hardware Lesson Integration

The Linux track now imports the advanced macOS hardware handling and the Windows
transition contract into Linux-specific tooling and documents.

Added:

- `linux/MACOS_WINDOWS_HARDWARE_LESSONS.md`
- `opena8dj-linuxctl hardware-model`
- `hardware-model.json` in `opena8dj-linuxctl verify` reports
- profile schema v2 with USB endpoints, A/B/C/D topology, source oracles,
  hot-path rules, and validation policy
- package version `0.1.1~experimental20260726`

Profile behavior is now aligned with macOS/Windows:

- DVS vinyl sets vinyl mode, vinyl ground lift on, other ground lifts off, and
  software lock on.
- DVS CD/line sets CD/line mode, CD/line ground lift on, other ground lifts off,
  and software lock on.
- Phono/vinyl recording sets phono mode, phono ground lift on, other ground
  lifts off, and software lock on.
- Playback, C/D recording, effects-loop, microphone, DAW, and MIDI profiles do
  not hide A/B mode changes.

No hardware tests were run. No lock was acquired. The label remains:

```text
diagnostic only, sound quality not validated
```

## 2026-07-26 Multi-Distro Experimental Packaging

The experimental package builder now produces:

- `.deb` for Debian, Ubuntu, Linux Mint, and Pop!_OS.
- `.rpm` for Fedora, RHEL, Rocky, AlmaLinux, and openSUSE.
- `.pkg.tar.zst` for Arch, Manjaro, and EndeavourOS.
- `.tar.gz` for manual inspection on other Linux packaging models.

The package version is `0.1.2~experimental20260726`. Native installer coverage
does not constitute distribution support or an audio-quality pass: all package
forms remain diagnostic only, sound quality not validated.
