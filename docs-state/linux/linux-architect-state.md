# OpenA8DJ Linux Architect State

Date: 2026-06-22

Worktree: `/Users/fer/dev/opena8dj-linux-agent`

Branch: `linux/full-driver-agent`

Readiness label:

```text
diagnostic only, sound quality not validated
```

## Executive State

The Linux worktree is on the required branch and contains a coherent first
design surface under `linux/` plus `agents/linux-driver-agent/`. Those files are
currently untracked in Git. No Linux kernel module exists, no Debian/RPM package
exists, no module has been installed or loaded, and no Audio 8 DJ hardware
validation has been run from this worktree.

The existing Linux notes are useful but still aspirational. The most important
correction is the PCM surface assumption: upstream `snd-usb-caiaq` does not
start from one native 8-channel playback PCM plus one native 8-channel capture
PCM. The current Linux source creates multiple stereo substreams from the
device-reported analog/digital stream counts. For Audio 8 DJ this likely means
four stereo playback substreams and four stereo capture substreams. The product
goal is still 8 inputs and 8 outputs with stable A/B/C/D routing, but the first
Linux architecture must validate the CAIAQ-native stereo-pair surface before
forcing a single 8x8 kernel-facing PCM shape.

The provisional architecture is:

1. Treat upstream `snd-usb-caiaq` as the first implementation path.
2. Add Audio 8 DJ-specific correctness, diagnostics, naming/profile support,
   and validation tooling around CAIAQ without regressing other CAIAQ devices.
3. Use user-space profiles, ALSA UCM, PipeWire/JACK policy, or explicit helper
   tooling for aggregate 8x8 presentation where that is the right layer.
4. Create a focused `snd-opena8dj` module only if CAIAQ cannot expose honest
   timing, stable routing, full-duplex quality, or required diagnostics without
   structural damage.

No Linux candidate may be handed to a normal user until the exact artifact has
passed the physical quality gates. Until then every Linux artifact must carry:

```text
diagnostic only, sound quality not validated
```

## Audit Inputs

Local files audited in this worktree:

- `git status --short --branch`: branch is `linux/full-driver-agent`; `agents/`
  and `linux/` are untracked.
- `linux/README.md`
- `linux/ARCHITECTURE.md`
- `linux/PACKAGING.md`
- `linux/QUALITY_AND_PERFORMANCE_GATES.md`
- `linux/SND_USB_CAIAQ_AUDIT.md`
- `linux/CONFIGURATION_MODEL.md`
- `linux/CANDIDATE_PAYLOAD.md`
- `linux/HANDOFF_2026-06-19.md`
- `linux/driver/README.md`
- `linux/driver/Makefile`
- `linux/packaging/debian/README.md`
- `linux/packaging/rpm/README.md`
- `agents/linux-driver-agent/STATUS.md`
- macOS reference files in the same worktree: `README.md`, `Makefile`,
  `docs/ARCHITECTURE.md`, `docs/TRAKTOR_TIMECODE.md`,
  `docs/ROADMAP_TO_PRO.md`, `docs/QUALITY_PASS_PROTOCOL.md`,
  `docs/SHARED_HARDWARE_COORDINATION.md`.

Primary upstream sources reviewed on 2026-06-22:

- Linux `sound/usb/caiaq/audio.c`:
  https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/sound/usb/caiaq/audio.c
- Linux `sound/usb/caiaq/device.c`:
  https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/sound/usb/caiaq/device.c
- Linux `sound/usb/caiaq/device.h`:
  https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/sound/usb/caiaq/device.h
- Linux `sound/usb/caiaq/control.c`:
  https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/sound/usb/caiaq/control.c
- Linux `sound/usb/caiaq/midi.c`:
  https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/sound/usb/caiaq/midi.c
- Linux `sound/usb/Kconfig`:
  https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/sound/usb/Kconfig
- Kernel ALSA configuration documentation:
  https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/Documentation/sound/alsa-configuration.rst

Secondary historical source reviewed:

- Linux DJ Audio 8 DJ notes, including old CAIAQ channel mapping and white-noise
  commit references:
  https://www.pogo.org.uk/~mark/linuxdj/

## Reference Version Note

The delegation asks for a matrix against "macOS 0.5.x". This checkout does not
show a macOS 0.5.x reference. The local `Makefile` reports `VERSION := 0.3.135`,
while the public-facing `README.md` and Traktor docs still describe the
`0.3.25` public preview. Therefore this document uses the observed macOS
capabilities in this worktree as the oracle and treats "0.5.x" as a required
reference-refresh task before final parity claims.

This is not a blocker for Linux architecture. It is a blocker for claiming
complete parity with the latest official macOS line.

## Current Linux Worktree State

| Area | Observed state | Assessment |
| --- | --- | --- |
| Branch | `linux/full-driver-agent` | Correct branch. |
| Worktree | `/Users/fer/dev/opena8dj-linux-agent` | Correct Linux-only territory. |
| Git state | `agents/` and `linux/` untracked | Draft material exists but is not committed. |
| Driver code | `linux/driver/Makefile` intentionally refuses module/install/load/unload | Correct safety posture. |
| Linux docs | Initial architecture, packaging, quality gates, CAIAQ audit, config model, payload contract | Useful, but not yet implementation evidence. |
| Packaging | Debian/RPM README scaffolds only | No package path exists yet. |
| Tools | No `opena8dj-linuxctl` implementation | Tooling is design-only. |
| Hardware validation | None from this worktree | No quality or enumeration claim allowed. |
| Hardware lock | Not acquired | Correct, because only read/design work was done. |

## macOS Reference To Linux Target Matrix

| macOS reference capability | Linux target | Current Linux state | Proposed architecture | Risks | Required tests |
| --- | --- | --- | --- | --- | --- |
| Device identity: `Open Audio 8 DJ`, USB `17cc:1978` | ALSA card for Native Instruments Audio 8 DJ, with OpenA8DJ-visible naming where possible | CAIAQ already matches `USB_PID_AUDIO8DJ` in upstream source; no local Linux probe run | Extend CAIAQ or wrap naming with UCM/tools; avoid false official branding | Distro card ID may remain `Audio8DJ`/product string rather than `OpenA8DJ`; naming changes may be unwelcome upstream | `lsusb -v`, `dmesg`, `/proc/asound/cards`, `aplay -l`, `arecord -l` on real Linux host |
| 8 outputs, A/B/C/D deck pairs | 8 playback channels exposed as stable A/B/C/D pairs | Upstream CAIAQ likely exposes four stereo playback substreams, not one 8-channel PCM | Accept CAIAQ-native four stereo PCMs first; add UCM/asound/PipeWire aggregate if clients need one 8-channel device | Some apps want one multichannel PCM; others prefer stereo pairs; wrong aggregation can hide channel swaps | Pair-tone sweep A/B/C/D through external capture; PipeWire/JACK/Traktor/Mixxx device-shape tests |
| 8 inputs, A/B/C/D timecode pairs | 8 capture channels exposed as stable A/B/C/D pairs | Upstream CAIAQ likely exposes four stereo capture substreams; no Linux hardware verification yet | Validate CAIAQ pair order; add profile/tool mapping; consider kernel channel-map support only if safe | Timecode is sensitive to channel swap, phase, mode, and gain; historical notes mention old mapping problems | Known-tone capture on every pair; timecode vinyl/CD-line scope at 44.1 and 48 kHz |
| 44.1 and 48 kHz validated playback | 44.1 and 48 kHz first-class rates | CAIAQ advertises 44.1/48/96 and adds 88.2 for Audio 8 DJ, but Linux track will only claim 44.1/48 after tests | Gate exposed/tested profiles around 44.1/48 first; do not hide kernel rates unless necessary, but do not claim them | CAIAQ may expose 88.2/96 before OpenA8DJ quality proof; user-space may pick unvalidated rates | `speaker-test`/WAV playback, external capture, pitch/speed comparison, rate-switch stress |
| 88.2/96 kHz deferred or production-gated | Evaluate after 44.1/48 pass | CAIAQ source includes 96 and Audio 8 DJ-specific 88.2 | Document as experimental until physical capture passes | Apps may select unsupported-quality rates; CPU/USB packet layout risk increases | Same physical gate as 44.1/48 plus long-run xrun/CPU checks |
| Core Audio input/output buffers are Float32; USB transport is 24-bit CAIAQ | ALSA user-space format should be honest, with conversion left to ALSA/PipeWire plugins | Upstream CAIAQ exposes `SNDRV_PCM_FMTBIT_S24_3BE`; USB packet code uses 3 data bytes plus alignment/check bytes | Preserve honest S24_3BE kernel format; rely on `plughw`, PipeWire, JACK, or app conversion for Float32/S32 use | Apps opening `hw:` directly may fail if they require common formats; conversion could add latency | `aplay --dump-hw-params`; PipeWire/JACK format negotiation; latency under converted and direct paths |
| Capture-paced output transport improved macOS crackle behavior | Full-duplex timing must be honest and low-jitter | Upstream CAIAQ mirrors playback packet layout from completed capture URBs | Prefer CAIAQ cadence model first; add counters around IN/OUT packet anomalies and URB starvation | Playback progress depends on capture completions; output starvation and one-warning panic behavior can mask quality failures | Trace URB cadence; xrun counters; output-under-load capture; external music capture |
| CoreMIDI endpoints | ALSA rawmidi input/output visible to `aconnect`, PipeWire, JACK, and DAWs | CAIAQ rawmidi creates duplex MIDI devices from firmware-reported port counts | Keep CAIAQ rawmidi path; add Linux tool checks and naming docs | EP1 command traffic may interfere with control/MIDI bursts; MIDI may be ignored by desktop profiles | `aconnect -l`, MIDI loopback, long MIDI burst while PCM full-duplex is active |
| `opena8dj-control` exposes input mode, ground lifts, software lock | ALSA controls plus Linux CLI/profile layer | CAIAQ has A8DJ controls: current input mode, GND lift vinyl, GND lift CD/Line, GND lift phono, software lock | Use ALSA controls as kernel truth; build `opena8dj-linuxctl` for friendly names, profiles, and diagnostics | CAIAQ controls are HWDEP-facing and raw; profile persistence can create hidden state | `amixer controls/contents`, mode-change hardware verification, profile apply/report tests |
| Timecode Vinyl profile exists for Traktor validation | DVS vinyl/CD-line/phono profiles | Linux profile model exists only as docs | Implement profiles in user-space CLI/UCM; kernel only exposes deterministic controls | Traktor under Wine, Mixxx, JACK, and PipeWire may need different device shapes | Timecode scope on Deck A/B first, then full A/B/C/D matrix; mode/gnd tests |
| DMG/PKG installer, control tool, LaunchAgent, uninstall path | Debian/Ubuntu first, RPM second, with driver/tools/UCM/udev split and rollback | Packaging docs only; no metadata | `opena8dj-tools` safe alone; temporary `opena8dj-dkms`; `opena8dj-alsa-ucm`; `opena8dj-udev`; optional meta-package | DKMS breakage, Secure Boot signing, accidental driver bind/load, packaging unsupported kernel APIs | Build packages in clean containers; install/uninstall dry runs; no auto-load; Secure Boot signing check |
| Physical iRig and listening gates decide readiness | Linux physical capture gate decides readiness | No Linux physical validation | Reuse macOS philosophy: exact module hash, kernel version, external capture, metrics, then listening | Counters can be clean while analog output is bad | Real music playback, independent capture, WAV-vs-reference, click/noise metrics, human listening only after clean measurements |

## CAIAQ Findings

Upstream `snd-usb-caiaq` already provides several strong anchors:

- It matches Audio 8 DJ as `17cc:1978`.
- `CONFIG_SND_USB_CAIAQ` selects ALSA PCM, rawmidi, and hwdep support.
- It uses EP1 command traffic for device info, audio params, MIDI, read/write
  IO, and auto messages.
- It allocates fixed isochronous URB pools for capture and playback.
- It exposes ALSA PCM callbacks for open, close, prepare, trigger, pointer,
  and period elapsed accounting.
- It derives stream counts from the firmware device spec.
- It exposes rawmidi when the firmware reports MIDI ports.
- It has Audio 8 DJ-specific controls for input mode, three ground-lift flags,
  and software lock.

The key technical mismatch is the PCM shape:

- `CHANNELS_PER_STREAM` is 2.
- `snd_usb_caiaq_audio_init()` computes `n_audio_in`, `n_audio_out`, and
  `n_streams` as stereo stream counts.
- `snd_pcm_new()` is called with `n_audio_out` playback substreams and
  `n_audio_in` capture substreams.
- The generic PCM hardware template has `channels_min = 2` and
  `channels_max = 2`.

Therefore, Linux parity must be defined as "8 physical inputs and 8 physical
outputs with correct A/B/C/D routing", not prematurely as "one native ALSA
8-channel playback PCM and one native ALSA 8-channel capture PCM." A single
8-channel presentation can still be a target, but it should be proven as a
client-compatibility requirement, not assumed as the first kernel design.

## Historical Bug Signals

The historical signals are useful because they point to the same bug surface as
OpenA8DJ macOS work: channel mapping, start/unpause noise, and period/timing
honesty.

Primary commits found from kernel.org:

- `a9b487fa1e00b42f9667abfeca4a5295a71333db`: "ALSA: snd-usb-caiaq: fix
  reported elapsed periods"; commit text says it fixed initial `aplay`
  underruns and problems with latency-picky applications such as PulseAudio.
- `ac9dd9d384b018f1e1c5a9a2686ab5605ce55818`: "ALSA: snd-usb-caiaq: Lock on
  stream start/unpause"; commit text says it fixed a bug that could result in
  white noise after stream start or unpause.

Secondary Linux DJ notes also warn that very old kernels had incorrect Audio 8
DJ channel mapping affecting timecode control, and that the device presented
four stereo pairs by default.

Implication: do not treat enumeration or zero XRUNs as sufficient. The Linux
gate must verify pair order, start/unpause behavior, latency-sensitive clients,
and physical sound quality.

## Architecture Decision Study

### Option A: Upstream-style `snd-usb-caiaq` extension

Verdict: preferred first path.

Why:

- Audio 8 DJ is already supported and matched in the kernel.
- PCM, MIDI, controls, hotplug, disconnect, and packet cadence already exist.
- The current code already models the CAIAQ packet layout, check bytes, EP1
  commands, and device spec.
- Upstream review is plausible if changes are scoped, conservative, and do not
  degrade other Native Instruments CAIAQ devices.

Required work:

- Select exact kernel baselines: one current distro/LTS baseline for packages
  and one current mainline baseline for upstream.
- Add Audio 8 DJ-specific diagnostic counters without hot-path logging.
- Verify or improve pointer, period, xrun, and delay behavior.
- Make A/B/C/D pair mapping explicit through docs, UCM, or channel maps where
  accepted.
- Confirm whether the four-stereo-pair PCM model is sufficient for PipeWire,
  JACK, Mixxx, Traktor/Wine, and DAWs.
- Add user-space control/profile tooling instead of bloating kernel policy.

Risks:

- CAIAQ is shared by many devices. Audio 8 DJ-specific changes must be isolated.
- A true single 8x8 ALSA PCM may be difficult without larger CAIAQ surgery.
- GPL kernel code cannot be copied into the MIT project as if it were MIT code.
- Upstream review will reject product-specific policy or unstable diagnostics.

### Option B: Focused `snd-opena8dj` kernel module

Verdict: fallback only.

Why it might be needed:

- CAIAQ cannot provide the required user-space surface.
- CAIAQ timing/delay/pointer behavior cannot be made honest without invasive
  changes.
- Audio 8 DJ diagnostics require a cleaner architecture than shared CAIAQ can
  support.
- Upstream maintainers prefer a separate driver because Audio 8 DJ behavior is
  too specialized.

Costs:

- Reimplements device matching, EP1, PCM, MIDI, controls, disconnect,
  suspend/resume, URB management, packaging, signing, and QA.
- Higher bug count and longer route to trust.
- More maintenance burden across kernel API drift.
- Still GPL-compatible if it links as a Linux kernel module.

### Option C: User-space ALSA plugin or libusb driver

Verdict: diagnostic only, not the main driver path.

Useful for:

- Descriptor inspection.
- EP1 control experiments.
- Offline packet decoding.
- Prototype profile/status tools.

Not suitable as the production audio driver:

- Harder to integrate as a normal ALSA/PipeWire/JACK card.
- More fragile low-latency scheduling.
- Competes with kernel driver binding.
- Raises permission, hotplug, and recovery complexity.
- Cannot be treated as a substitute for kernel PCM correctness.

### Option D: DKMS/out-of-tree module

Verdict: acceptable bridge, not the quality bar.

Useful for:

- Early testers on Debian/Ubuntu.
- Iterating before upstream acceptance.
- Building against local kernels with headers.

Hard requirements:

- No post-install auto-load or auto-bind.
- Explicit Secure Boot signing story.
- Exact kernel/header/compiler metadata.
- Module hash in every diagnostic report.
- Clear GPL/provenance metadata if distributing kernel-derived code.

### Option E: Hybrid kernel plus tools/UCM/profile packages

Verdict: expected final Linux packaging shape.

Kernel responsibilities:

- Device binding.
- USB isochronous transport.
- ALSA PCM/rawmidi/control truth.
- Timing, pointer, xrun, disconnect, suspend/resume correctness.

User-space responsibilities:

- Friendly names and profiles.
- Diagnostics export.
- Aggregate or policy-specific routing.
- PipeWire/JACK/DAW integration.
- Safe validation commands.

## Implementation Plan

### Phase 0: Baseline and licensing discipline

Deliverables:

- Select two baselines:
  - distro support baseline for packages;
  - mainline baseline for upstream patch review.
- Record exact kernel commit, version, config, and distro targets.
- Decide repository layout for GPL kernel code or patch series.
- Add a Linux legal/provenance note that kernel-derived code is GPL and not
  covered by the repository's MIT license claim.

Acceptance:

- No GPL kernel source is copied into MIT-labelled areas without SPDX and
  license separation.
- No packaging claims support for kernels that were not built/tested.

### Phase 1: Source-level CAIAQ audit

Deliverables:

- Complete line-by-line audit of `audio.c`, `device.c`, `device.h`,
  `control.c`, `midi.c`, and Kconfig for the selected baseline.
- Document the exact Audio 8 DJ device spec observed from Linux hardware:
  analog in/out, MIDI in/out, data alignment, firmware version, endpoint sizes.
- Record PCM shape as actually enumerated by ALSA.

Acceptance:

- The driver choice remains CAIAQ only if PCM shape, controls, MIDI, and timing
  are credible for OpenA8DJ goals.
- If not, write the concrete `snd-opena8dj` split rationale before writing code.

### Phase 2: Non-invasive diagnostics

Deliverables:

- Add counters for:
  - input panic;
  - output panic;
  - short/failed isochronous packets;
  - output URB starvation;
  - inbound and outbound submit failures;
  - period elapsed events;
  - stream start/stop/unpause counts;
  - rate/depth/bpp configuration;
  - last control command error.
- Expose diagnostics through an upstream-acceptable mechanism such as ALSA proc
  info, debugfs for development-only builds, or read-only controls if accepted.
- Add tracepoints only if maintainable and acceptable to ALSA maintainers.

Acceptance:

- No steady-state hot-path logging.
- No heap allocation in the isochronous completion path.
- Counters are useful but not used as a substitute for physical audio tests.

### Phase 3: ALSA surface and A/B/C/D mapping

Deliverables:

- Verify whether Audio 8 DJ appears as four playback and four capture stereo
  substreams, one PCM with subdevices, or another distro-specific shape.
- Create a stable A/B/C/D naming map:
  - kernel names where safe;
  - ALSA UCM or profile names where policy belongs in user space;
  - `opena8dj-linuxctl status` output for human diagnostics.
- Decide if a single 8-channel aggregate is necessary for target clients.

Acceptance:

- A tone sent to A/B/C/D appears only on the corresponding physical output pair.
- A known input tone on A/B/C/D appears only on the corresponding capture pair.
- Pair order survives rate changes, client restarts, PipeWire/JACK restarts,
  and device replug.

### Phase 4: MIDI and control plane

Deliverables:

- Validate rawmidi input/output with `aconnect -l` and loopback.
- Map ALSA controls to friendly names:
  - `input-mode`
  - `gnd-vinyl`
  - `gnd-cd-line`
  - `gnd-phono`
  - `software-lock`
- Implement `opena8dj-linuxctl` once the control source is confirmed.
- Profiles:
  - `playback-4out`
  - `traktor-dvs-vinyl`
  - `traktor-dvs-cd-line`
  - `phono-recording`
  - `line-recording`
  - `daw-multichannel`
  - `midi-bridge`

Acceptance:

- Control changes have observable hardware effect.
- Profiles report every control they change.
- MIDI traffic does not degrade PCM timing under full-duplex use.

### Phase 5: Packaging

Deliverables:

- Debian/Ubuntu package lane:
  - `opena8dj-tools`
  - `opena8dj-dkms` if out-of-tree is still needed
  - `opena8dj-alsa-ucm`
  - `opena8dj-udev`
  - optional `opena8dj` meta-package
- RPM lane:
  - `akmods` preferred for Fedora-style systems if out-of-tree
  - DKMS where distro policy expects it
  - kmod only when kernel ABI policy supports it
- Candidate metadata:
  - exact git commit;
  - module path/name;
  - kernel target;
  - compiler;
  - hashes;
  - signing/Secure Boot status;
  - validation label.

Acceptance:

- Install does not load, bind, unbind, reset USB, restart audio services, or
  run playback/capture tests.
- Uninstall removes only package-owned files.
- Secure Boot behavior is explicit, not discovered by failure.

### Phase 6: Quality and performance validation

Deliverables:

- Build hygiene report.
- Enumeration report.
- PCM smoke report.
- Routing matrix report.
- CPU/latency report.
- Physical capture report.
- DVS/MIDI report.
- Resilience report.

Acceptance:

- The exact loaded module or patch artifact has a recorded hash.
- The exact Linux kernel and package versions are recorded.
- Physical capture uses real music and an independent capture path where
  available.
- Human listening is allowed only after measurements are clean.

## Linux Diagnostic Export Contract

Every serious Linux candidate must export:

- OpenA8DJ package version.
- Git commit and branch.
- Kernel version and config fingerprint.
- Module name and module hash.
- Driver channel: upstream patch, DKMS, akmods, built module, or in-tree.
- Secure Boot signing status.
- USB descriptor summary and device path.
- ALSA card/device/subdevice listing.
- PCM rates, formats, period/buffer limits.
- A/B/C/D mapping result.
- ALSA control listing and current values.
- rawmidi listing and current client visibility.
- PipeWire/JACK visibility when present.
- XRUN/underrun counters.
- CAIAQ packet anomaly counters.
- CPU and scheduler evidence from validation runs.
- Tests run and tests explicitly not run.
- Readiness label.

## Experimental-to-Validable Acceptance Criteria

Linux remains experimental until all of the following are true:

1. The architecture decision is written and tied to an exact kernel baseline.
2. The selected driver path builds cleanly on at least one supported Linux
   target without touching live hardware.
3. Package install/uninstall dry runs pass without auto-loading or resetting
   hardware.
4. Real Audio 8 DJ enumeration passes under the shared hardware lock.
5. ALSA shows the expected playback, capture, rawmidi, and control surfaces.
6. 44.1 kHz and 48 kHz PCM open/prepare/start/stop pass for playback, capture,
   and full-duplex.
7. A/B/C/D playback and capture routing pass with known tones.
8. Pointer, period elapsed, and xrun behavior are monotonic and honest.
9. CPU and callback behavior are bounded under realistic PipeWire/JACK settings.
10. MIDI in/out works and does not destabilize PCM timing.
11. Input mode, ground-lift, and software-lock controls work as documented.
12. Hotplug, active unplug, suspend/resume, client crash, PipeWire restart, and
    JACK restart are handled safely.
13. Physical music playback through Audio 8 DJ is externally captured and
    compared against reference.
14. No glitches, metallic noise, white noise, clicks, channel swaps, speed
    errors, or unstable CPU behavior are observed in the exact artifact.
15. The candidate metadata, hashes, signing state, diagnostics, docs, rollback,
    and limitations are complete.

Only after items 1-15 pass can the label move from:

```text
diagnostic only, sound quality not validated
```

to:

```text
candidate for human listening
```

It is not a release candidate until human listening and regression gates pass.

## Immediate Next Actions

1. Commit or intentionally park the existing untracked Linux docs after review.
2. Select the Linux kernel baseline pair:
   - distro/LTS packaging baseline;
   - mainline upstream baseline.
3. Add a `linux/LEGAL_AND_PROVENANCE.md` note for GPL kernel code, patch-series
   handling, Native Instruments no-payload policy, and package metadata.
4. Re-run the CAIAQ audit against the selected baseline with line references.
5. Produce an enumeration plan for a real Linux host with Audio 8 DJ attached.
6. Decide whether phase-1 Linux parity accepts the CAIAQ-native four stereo
   pair surface or requires a single 8x8 presentation for specific clients.
7. Implement a safe, read-only `opena8dj-linuxctl status` prototype before any
   hardware-affecting command.
8. Build packaging metadata only after the driver path is selected.
9. Do not acquire hardware or run live tests until a validation window is
   planned and the shared lock is available.

## Hard Stop Rules

- Do not edit `/Users/fer/dev/opena8dj` for Linux implementation work.
- Do not present Linux as a normal-use candidate without exact-artifact physical
  sound-quality validation.
- Do not let a successful build, DKMS install, or ALSA enumeration imply audio
  quality.
- Do not auto-load, auto-bind, unbind, reset USB, or restart audio services from
  package install scripts.
- Do not copy GPL kernel source into MIT-labelled project areas without explicit
  SPDX/license separation.
- Do not hide unsupported or unvalidated rates behind user-friendly docs.
- Do not hide CAIAQ's actual PCM shape behind aspirational 8x8 wording.
