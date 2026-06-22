# RPM Packaging Scaffold

This directory is a placeholder for Fedora, Red Hat, and RPM-derived packaging.

Current state:

```text
diagnostic only, sound quality not validated
```

It does not produce a real `.rpm` yet.

## Planned Lanes

Possible RPM lanes:

- `akmods` for Fedora-style local kernel module rebuilds.
- DKMS where the target distribution expects it.
- kABI-aware `kmod` packages for RHEL-derived systems if policy and kernel
  compatibility allow it.
- Separate `opena8dj-tools`, `opena8dj-alsa-ucm`, and `opena8dj-udev` packages.

## Planned Files

Future RPM packaging may include:

```text
opena8dj.spec
sources/
```

The spec must not automatically bind hardware, run playback/capture, reset USB,
or claim audio quality.

## Safety Rule

RPM install behavior must match the Debian lane: install files conservatively,
avoid hidden hardware side effects, and keep all physical validation behind
explicit operator steps.
