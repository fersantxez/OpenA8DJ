# Linux Implementation Plan

Current readiness:

```text
diagnostic only, sound quality not validated
```

This is the execution plan for turning the Linux track from design material into
a serious experimental implementation. It intentionally separates work that can
be done offline from work that requires real Audio 8 DJ hardware and the shared
hardware lock.

## Non-Negotiable Rules

- Linux work stays in `/Users/fer/dev/opena8dj-linux-agent`.
- Linux branch is `linux/full-driver-agent`.
- `/Users/fer/dev/opena8dj` is reference-only for Linux work.
- No install script may load, bind, unbind, reset USB, restart audio services,
  or run playback/capture tests as a hidden side effect.
- No artifact may be presented as a normal-use candidate until the exact
  loaded or packaged artifact has passed physical sound-quality validation.
- Until then the label remains:

```text
diagnostic only, sound quality not validated
```

## Architecture Direction

Primary path:

```text
upstream-style snd-usb-caiaq extension + Linux tools/UCM/packaging
```

Fallback path:

```text
focused snd-opena8dj kernel module
```

The fallback is justified only if `snd-usb-caiaq` cannot expose honest timing,
stable A/B/C/D routing, MIDI, controls, diagnostics, and physical sound quality
without invasive changes that would harm other CAIAQ devices.

The product target remains 8 physical inputs and 8 physical outputs with stable
A/B/C/D routing. The first kernel implementation must not assume this means one
native ALSA 8-channel PCM, because current CAIAQ source creates stereo
substreams. A single 8x8 presentation can be implemented in user space if
client compatibility proves it is needed.

## Milestone 0: Planning And Safe Scaffolding

Status: in progress.

Deliverables:

- `docs-state/linux/linux-architect-state.md`
- `linux/IMPLEMENTATION_PLAN.md`
- `linux/BASELINES.md`
- `linux/LEGAL_AND_PROVENANCE.md`
- `linux/ENUMERATION_PLAN.md`
- `linux/SND_USB_CAIAQ_AUDIT.md` updated with source-level findings.
- read-only `opena8dj-linuxctl` prototype.

Acceptance:

- The plan identifies implementation order, validation gates, and hard stops.
- No hardware-affecting operation is added.
- The read-only tool runs safely on non-Linux hosts and reports that hardware
  validation is not present.

## Milestone 1: Kernel Baseline And CAIAQ Audit

Status: planned.

Deliverables:

- Snapshot exact kernel refs for:
  - mainline/upstream patch review;
  - longterm/stable packaging bridge.
- Re-run the CAIAQ source audit against those exact refs.
- Produce a patch-design note for diagnostics and Audio 8 DJ-specific quirks.
- Decide the implementation channel:
  - upstream patch series;
  - out-of-tree DKMS bridge;
  - focused `snd-opena8dj` fallback.

Acceptance:

- No code is written against an unnamed kernel.
- GPL provenance and SPDX placement are documented before kernel-derived code
  enters this repo.
- The audit names every required kernel behavior that must be measured on real
  hardware.

## Milestone 2: Read-Only Linux Diagnostics

Status: first prototype implemented.

Deliverables:

- `opena8dj-linuxctl status`
- `opena8dj-linuxctl diagnostics --json`
- `opena8dj-linuxctl self-test`

The tool must collect only read-only data:

- OS and kernel identity.
- `/proc/asound/cards`
- `/proc/asound/pcm`
- `lsusb -d 17cc:1978`, when available.
- `aplay -l`, `arecord -l`, `aconnect -l`, when available.
- `amixer -c <card> controls`, only for matching Audio 8 DJ cards.

Acceptance:

- Tool can run without root.
- Tool does not call `modprobe`, `rmmod`, `systemctl`, `udevadm trigger`, USB
  reset helpers, playback, capture, or mixer-write commands.
- JSON output includes the readiness label and tests-not-run list.

## Milestone 3: Packaging Skeleton

Status: planned after Milestone 1 decision.

Deliverables:

- Debian source package metadata:
  - `opena8dj-tools`
  - `opena8dj-dkms`, only if out-of-tree is selected
  - `opena8dj-alsa-ucm`
  - `opena8dj-udev`
  - optional `opena8dj` meta-package
- RPM lane:
  - `akmods` for Fedora-style systems if out-of-tree;
  - DKMS where distro policy expects it;
  - kmod only if kernel ABI policy justifies it.
- Candidate metadata JSON.
- Dry-run install/uninstall checks.

Acceptance:

- Package install never changes live audio state.
- Secure Boot behavior is stated before package publication.
- Tools can install separately from the kernel module.

## Milestone 4: CAIAQ Diagnostics Patch

Status: planned.

Primary implementation idea:

- Add Audio 8 DJ-safe counters around CAIAQ without changing audio behavior.
- Expose counters through an upstream-acceptable read-only interface.
- Preserve hot-path real-time hygiene.

Counters to design:

- input panic count.
- output panic count.
- short isochronous packet count.
- failed isochronous packet count.
- output URB starvation count.
- input URB submit failure count.
- output URB submit failure count.
- stream start/stop/unpause counts.
- rate/depth/bpp last configured.
- period elapsed count per stream.
- last EP1 control error.

Acceptance:

- No steady-state hot-path logging.
- No heap allocation in completion callbacks.
- No changes to other CAIAQ devices unless required and reviewed.
- Diagnostics do not hide XRUNs or panic state.

## Milestone 5: Enumeration And PCM Smoke On Linux Host

Status: blocked until a real Linux host/test window exists.

Hardware lock required for live Audio 8 DJ interaction.

Deliverables:

- `lsusb -v` descriptor evidence for `17cc:1978`.
- `dmesg` probe evidence.
- ALSA cards/devices/subdevices.
- rawmidi visibility.
- controls visibility.
- 44.1/48 kHz playback open/prepare/start/stop.
- 44.1/48 kHz capture open/prepare/start/stop.
- Full-duplex open/start/stop.

Acceptance:

- No claim of sound quality.
- No normal-user candidate.
- If this passes, readiness can move at most to `pcm smoke passed`.

## Milestone 6: A/B/C/D Routing Matrix

Status: blocked until hardware validation.

Deliverables:

- Known-tone playback on every output pair.
- Known-tone capture on every input pair.
- Pair isolation report.
- Rate-change and client-restart repeat.
- PipeWire/JACK view of the same routing.

Acceptance:

- A/B/C/D pair order is stable and documented.
- No channel swap, bleed, or silent pair is accepted.
- If a single 8x8 aggregate is needed, it is implemented as a profile/UCM
  feature only after native pair behavior is proven.

## Milestone 7: Physical Audio Quality Gate

Status: blocked until hardware validation.

Deliverables:

- Exact module/package hash.
- Exact kernel version and config fingerprint.
- Real music playback through Audio 8 DJ.
- External capture through known-good capture path.
- WAV-vs-reference comparison.
- Click/noise/residual/clipping metrics.
- CPU/xrun evidence under realistic load.
- Human listening only after measured capture passes.

Acceptance:

- No glitches, metallic noise, white noise, clicks, channel swaps, speed errors,
  unstable CPU, or hidden XRUNs.
- Only then can the label become:

```text
candidate for human listening
```

## Milestone 8: DVS, MIDI, And Resilience

Status: blocked until hardware validation.

Deliverables:

- Timecode vinyl mode on A/B first, then A/B/C/D matrix.
- Timecode CD/line mode.
- Phono/line recording profiles.
- MIDI in/out loopback and burst tests.
- Hot unplug idle and active.
- Suspend/resume.
- PipeWire restart.
- JACK restart.
- Client crash while streaming.

Acceptance:

- DVS scopes are stable at 44.1 and 48 kHz.
- MIDI traffic does not degrade PCM timing.
- Recovery is safe and explainable.

## Implementation Backlog

| Priority | Work item | Output | Blocker |
| --- | --- | --- | --- |
| P0 | Baseline/provenance docs | `BASELINES.md`, `LEGAL_AND_PROVENANCE.md` | none |
| P0 | Read-only diagnostics tool | `linux/tools/opena8dj-linuxctl` | none |
| P0 | CAIAQ audit update | `SND_USB_CAIAQ_AUDIT.md` | none |
| P1 | Linux enumeration procedure | `ENUMERATION_PLAN.md` | Linux host for execution |
| P1 | Candidate metadata schema | `opena8dj-linux-candidate.json` template | driver channel decision |
| P1 | Debian tools package | package metadata | tool path finalized |
| P2 | DKMS/akmods lane | package metadata | driver patch/module source |
| P2 | CAIAQ diagnostic patch | kernel patch series | selected kernel baseline |
| P3 | UCM/channel map profiles | UCM files | verified ALSA surface |
| P3 | Physical quality automation | Linux soundcheck scripts | hardware and capture path |

## Done In This Pass

- Captured Linux architecture state.
- Selected `snd-usb-caiaq` extension as first implementation path.
- Defined fallback criteria for `snd-opena8dj`.
- Added a detailed implementation plan.
- Added baseline, provenance, and enumeration documents.
- Added a read-only Linux diagnostics tool prototype.

