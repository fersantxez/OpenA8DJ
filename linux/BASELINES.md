# Linux Baselines

Current readiness:

```text
diagnostic only, sound quality not validated
```

This document records the exact kernel references used for Linux planning. These
are audit baselines, not support claims.

## Baseline Snapshot

Recorded on 2026-06-22 from kernel.org:

| Purpose | Repository | Ref | Commit |
| --- | --- | --- | --- |
| Upstream/mainline audit | `https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git` | `HEAD` | `1dc18801be29bc54709aa355b8acd80e183b03cd` |
| Stable/LTS bridge audit | `https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git` | `linux-6.12.y` | `0b8f247169e487eff2d4c2dd531bc43f7efda2cb` |
| Older stable compatibility check | `https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git` | `linux-6.6.y` | `d1cfde2d5d15be14123bdd1689162bd27f995a90` |

## Policy

- Mainline is used for upstreamable design and patch review.
- `linux-6.12.y` is the first stable/LTS audit lane.
- `linux-6.6.y` is a compatibility check lane only.
- Packaging support is not claimed until the target distro kernel, headers,
  compiler, Secure Boot behavior, and module build path are tested.
- If these refs move, this document must be updated with a new date and commit
  set before code work continues.

## Distro Packaging Targets

Packaging targets are not selected yet. The first package pass should target a
specific Linux host or container matrix rather than generic distro names.

Required metadata for every target:

- distro name and version.
- kernel version and package name.
- kernel headers package.
- compiler version.
- DKMS/akmods/kmod policy.
- Secure Boot default behavior.
- module signing path.
- PipeWire/JACK/ALSA versions used for validation.

## Baseline Decision Gate

Before kernel code is added, the Linux agent must complete:

1. Source audit against mainline `1dc18801be29bc54709aa355b8acd80e183b03cd`.
2. Source audit against stable `0b8f247169e487eff2d4c2dd531bc43f7efda2cb`.
3. Delta note for CAIAQ differences between those refs.
4. Driver channel decision:
   - upstream-style CAIAQ patch;
   - out-of-tree DKMS bridge;
   - focused `snd-opena8dj` fallback.

