# Linux Candidate Payload Contract

Every experimental Linux candidate must include the complete payload around the
driver. A candidate is not acceptable if it only provides a `.ko`, source
tarball, or binary module.

Current state:

```text
diagnostic only, sound quality not validated
```

The first diagnostic package payload is now generated under:

```text
dist/linux/experimental/opena8dj-linux-experimental-0.1.0~experimental20260622/
```

It satisfies the tools/docs/profile/udev/metadata portion of this contract, but
it does not include a replacement kernel module and it has not passed physical
sound-quality validation. This document remains the contract for future driver,
DKMS, akmods, or source-channel candidates.

The current diagnostic package payload is generated under:

```text
dist/linux/experimental/opena8dj-linux-experimental-0.1.1~experimental20260726/
```

It adds the macOS/Windows hardware lesson bridge, an exported
`hardware-model`, expanded profile schema, and macOS/Windows-aligned profile
control postures. It remains diagnostic-only and still does not include a
replacement kernel module or physical sound-quality validation.

## Minimum Payload

Each candidate must include:

- driver/module source or built module according to the selected channel.
- tools package.
- config/profile schema.
- udev rules if needed.
- ALSA UCM profiles if needed.
- docs and `README-FIRST`.
- cross-platform hardware lesson map when the candidate claims Audio 8 DJ
  control/routing behavior.
- uninstall and rollback path.
- diagnostics and verify report tooling.
- hashes and build metadata.
- symbols/debug-info policy.
- license and provenance statement.
- Native Instruments no-payload policy.
- validation labels and limitations.

## Driver or Module Content

The driver part depends on the channel:

| Channel | Required payload |
| --- | --- |
| DKMS source | Versioned module source, DKMS metadata, build instructions, kernel compatibility notes. |
| akmods source | Versioned source, spec integration, akmods build instructions, Fedora kernel compatibility notes. |
| Built module | Exact `.ko` or distro package output, kernel ABI target, signing status, module hash. |
| Upstream/in-tree patch | Kernel baseline, patch series, config requirements, expected module name. |

All channels must state whether the implementation extends `snd-usb-caiaq` or
uses a focused `snd-opena8dj` module.

No payload may silently load, bind, unbind, or test Audio 8 DJ hardware during
install.

## Tools Package

The candidate must include tools or explicitly document why a specific
experimental candidate is driver-only for internal debugging.

Required tool capabilities for a normal candidate:

- status inspection.
- control discovery.
- profile selection or profile report.
- diagnostics report.
- routing verification command that can run only when the operator explicitly
  starts it.
- version and build metadata output.

Suggested future command surface:

```sh
opena8dj-linuxctl status
opena8dj-linuxctl diagnostics
opena8dj-linuxctl verify --report
opena8dj-linuxctl profile traktor-dvs-vinyl
opena8dj-linuxctl version --build-metadata
```

Hardware-affecting commands must not run during package install.

## Config and Profile Schema

Each candidate must include a readable schema for:

- input mode: `timecode-vinyl`, `timecode-cd-line`, `phono`.
- ground lift vinyl/CD-line/phono.
- software lock.
- profile names.
- routing assumptions.
- diagnostics fields.

The schema can initially be documentation-only. Once tools exist, schema and
tool behavior must match.

Required profiles:

- `playback-4out`
- `traktor-dvs-vinyl`
- `traktor-dvs-cd-line`
- `vinyl-recording`
- `phono-recording`
- `line-recording`
- `dj-set-recording`
- `effects-loop`
- `microphone`
- `daw-multichannel`
- `midi-only`
- `midi-bridge`
- `ground-diagnostics`
- `engineering-diagnostics`
- `unlock`

## udev Rules

If the candidate requires user-space device access, it must include udev rules
or a statement that none are required.

udev rules must:

- identify Native Instruments Audio 8 DJ `17cc:1978` when needed.
- limit permissions to the minimum required for tools.
- avoid launching playback, capture, reset, module reload, or quality tests.
- avoid hidden driver binding policy.

## ALSA UCM Profiles

If needed for PipeWire, JACK, DAW, or desktop integration, the candidate must
include ALSA UCM files or a statement that UCM is intentionally deferred.

UCM content should cover:

- A/B/C/D pair names.
- 8-channel playback map.
- 8-channel capture map.
- DVS profiles.
- phono and line recording profiles.
- MIDI naming if UCM is the right layer for the target distro.

UCM must not conceal a kernel channel-order bug.

## Documentation and `README-FIRST`

Every candidate must include a `README-FIRST` document at the package or
artifact root.

It must state:

- exact readiness label.
- supported distro/kernel targets.
- install steps.
- uninstall and rollback steps.
- whether Secure Boot signing is handled.
- how to collect diagnostics.
- what is not validated.
- that no audiophile-quality claim is valid without physical capture evidence.
- whether the payload extends `snd-usb-caiaq` or uses `snd-opena8dj`.

The README must put safety and limitations before optional tuning.

## Uninstall and Rollback

Each candidate must include a rollback plan that removes only candidate-owned
files.

Rollback must cover:

- DKMS/akmods unregister.
- module removal guidance.
- package removal.
- udev rule removal.
- ALSA UCM removal.
- tools removal.
- modprobe config removal.
- restoring the previous distro driver path if the candidate overrides CAIAQ.

Rollback must not reset USB or unload active modules unless the operator
explicitly requests that action during a coordinated validation window.

## Diagnostics and Verify Reports

Candidates must include or plan a report artifact for:

- package version.
- git commit.
- build host or build environment identifier.
- distro and kernel target.
- module name.
- module hash.
- Secure Boot/signing status.
- ALSA card/device enumeration.
- controls enumeration.
- rawmidi enumeration.
- channel map status.
- validation label.
- tests run and tests not run.

Hardware-facing verification commands must be explicit and must not run from
post-install hooks.

## Hashes and Build Metadata

Every candidate must provide hashes for shipped artifacts:

- source archive or patch series.
- built module if shipped.
- tools binaries.
- package files.
- README and manifest.

Recommended metadata file:

```text
opena8dj-linux-candidate.json
```

Minimum fields:

```json
{
  "name": "opena8dj-linux",
  "candidate_id": "unset",
  "git_commit": "unset",
  "branch": "linux/full-driver-agent",
  "driver_channel": "dkms-source|akmods-source|built-module|in-tree-patch",
  "module_name": "unset",
  "kernel_targets": [],
  "packages": [],
  "hashes": {},
  "validation_label": "diagnostic only, sound quality not validated"
}
```

## Symbols and Debug Info

Policy:

- Keep enough symbols/debug info for kernel diagnostics during experimental
  validation.
- Do not strip away information needed to diagnose URB, PCM, MIDI, control, or
  timing failures.
- Split debug info into a separate package where distro policy expects it.
- Document how to map a crash, oops, or trace back to the exact source commit.

Suggested future package names:

- `opena8dj-dkms-dbgsym` or distro-generated equivalent.
- `opena8dj-tools-dbgsym` or distro-generated equivalent.
- RPM debuginfo packages following distro conventions.

## License and Provenance

Every candidate must include:

- OpenA8DJ license files.
- SPDX metadata where practical.
- source provenance for driver code, tools, packaging, and docs.
- a note that Native Instruments proprietary firmware, binaries, installers,
  drivers, assets, or payloads are not redistributed.

Native Instruments no-payload policy:

```text
Do not include Native Instruments proprietary binaries, firmware, installers,
driver payloads, trademarks as branding, or copyrighted assets in OpenA8DJ
packages.
```

Use public USB identifiers and factual compatibility references only.

## Validation Labels and Limitations

Each candidate must carry exactly one readiness label from
`QUALITY_AND_PERFORMANCE_GATES.md`.

Allowed early label:

```text
diagnostic only, sound quality not validated
```

The label must appear in:

- package description or release notes.
- `README-FIRST`.
- candidate metadata JSON.
- diagnostics report.

Limitations must state:

- whether hardware tests were run.
- whether the shared hardware lock was used.
- whether playback/capture was tested.
- whether physical capture evidence exists.
- whether Traktor/DVS was tested.
- whether MIDI was tested.
- whether Secure Boot was tested.

## Safety Rule

Package installation is not validation. A complete candidate payload still must
not surprise the operator by touching live hardware.

During project validation, any hardware-affecting action requires:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

If the lock is occupied, do not force it.

## Publication Rule

No `.deb`, `.rpm`, DKMS source bundle, akmods source bundle, or built module
candidate should be published externally unless it includes this payload
contract or explicitly states which required parts are missing and why the
artifact is internal-only.
