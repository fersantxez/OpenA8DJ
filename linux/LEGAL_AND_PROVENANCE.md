# Linux Legal And Provenance Policy

Current readiness:

```text
diagnostic only, sound quality not validated
```

This repository is MIT-licensed in its project-level files, while Linux kernel
modules and kernel-derived code are GPL-family work. The Linux track must keep
that boundary explicit.

## Kernel Code

- Upstream `snd-usb-caiaq` is GPL kernel code.
- Any copied, modified, or derived kernel module source must carry appropriate
  SPDX identifiers and provenance.
- Do not place GPL-derived kernel source in a path or package that implies it is
  MIT-covered OpenA8DJ user-space code.
- Prefer patch series against an exact kernel baseline for early review.
- If an out-of-tree DKMS bridge is created, its source package must include
  Linux-compatible GPL licensing metadata.

## User-Space Tools

OpenA8DJ Linux user-space tools may remain under the repository's project
license if they do not copy GPL kernel implementation code.

Allowed:

- Reading `/proc/asound`, `/sys`, and command output.
- Calling ALSA tools for status.
- Friendly profile/config wrappers.
- Diagnostics export.

Not allowed without license review:

- Copying CAIAQ kernel code into user-space tools.
- Reusing GPL-only implementation snippets in MIT-labelled files.

## Native Instruments Boundary

Do not redistribute:

- Native Instruments proprietary drivers.
- Native Instruments firmware blobs.
- Native Instruments installers.
- Native Instruments application binaries.
- Native Instruments logos or copyrighted assets.

Allowed factual references:

- Native Instruments Audio 8 DJ compatibility.
- USB vendor/product ID `17cc:1978`.
- Publicly observable USB descriptors.
- Publicly documented hardware behavior.

## Package Metadata Requirements

Every Linux candidate must include:

- license files and SPDX metadata.
- source provenance for kernel code, tools, packaging, and docs.
- exact git commit.
- exact kernel baseline.
- module/source hashes.
- validation label.
- statement that no Native Instruments proprietary payload is included.

## Review Gate

Before any `.deb`, `.rpm`, DKMS, akmods, kmod, or source bundle is published:

1. Verify all SPDX identifiers.
2. Verify package license fields.
3. Verify no proprietary NI payload is present.
4. Verify GPL-derived files are not described as MIT-only.
5. Verify readiness label is present in package description, README, and
   candidate metadata.

