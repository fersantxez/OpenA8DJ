# macOS and Windows Hardware Lessons Applied to Linux

Status:

```text
diagnostic only, sound quality not validated
```

This document records the hardware lessons that Linux must inherit from the
more advanced macOS line and the Windows transition work. It is a Linux design
contract, not a claim that Linux audio quality has passed.

## Source Material

Reference-only source paths used for this pass:

- macOS hardware map: `docs/AUDIO8DJ_HARDWARE_DOSSIER.md`
- macOS cadence plan: `docs/MACOS_USB_CADENCE_IMPLEMENTATION_PLAN_2026-06-12.md`
- macOS quality gate: `docs/PHYSICAL_MUSIC_QUALITY_GATE.md`
- macOS control surface: `src/tools/opena8dj-control.c`
- macOS USB implementation: `src/hal/OpenA8DJUSB.m`
- Windows shared surface: `windows/include/OpenA8DJShared.h`
- Windows offline engine: `windows/audio/OpenA8DJAudioEngine.c`
- Windows routing/performance lessons:
  `docs/WINDOWS_PERFORMANCE_ROUTING_AND_MACOS_LESSONS_2026-06-19.md`
- Windows final implementation plan:
  `docs-state/windows/windows-final-design-and-implementation-plan.md`

The Linux implementation remains isolated in
`/Users/fer/dev/opena8dj-linux-agent`.

## Hardware Facts Carried Forward

Audio 8 DJ is treated as vendor-specific CAIAQ hardware, not USB Audio Class:

| Fact | Linux requirement |
| --- | --- |
| USB ID `17cc:1978` | Detect and diagnose this exact device ID. |
| Bulk/control/MIDI EPs `0x01` out and `0x81` in | Keep EP1 command/MIDI traffic out of the audio hot path. |
| Isochronous capture EP `0x82` | Measure capture packet status, short packets, and completion cadence. |
| Isochronous playback EP `0x06` | Measure output URB starvation, short transfers, and queue failures. |
| Four stereo pairs A/B/C/D | Preserve stable pair order in ALSA, PipeWire, JACK, DAWs, and DVS. |
| 8 physical inputs and 8 physical outputs | Define parity by physical routing first, not by forcing one 8x8 PCM prematurely. |
| MIDI in/out | Validate ALSA rawmidi visibility and MIDI burst behavior during PCM use. |

## Control Surface Parity

macOS exposes these controls through `opena8dj-control`; Linux must map them to
ALSA controls when provided by `snd-usb-caiaq`:

| User control | macOS behavior | Linux mapping |
| --- | --- | --- |
| `input-mode` | `0=timecode-vinyl`, `1=timecode-cd-line`, `2=phono` | ALSA `Current input mode` |
| `gnd-vinyl` | Vinyl ground lift flag | ALSA `GND lift for TC Vinyl mode` |
| `gnd-cd-line` | CD/line ground lift flag | ALSA `GND lift for TC CD/Line mode` |
| `gnd-phono` | Phono ground lift flag | ALSA `GND lift for phono mode` |
| `software-lock` | Prevents accidental mode changes | ALSA `Software lock` |

Linux does not currently expose the macOS internal `input-decode` switch as a
separate ALSA control. The Linux equivalent is an implementation rule:
playback-only profiles must not pay capture/DVS cost unless capture is actually
active.

Profile behavior now follows the macOS/Windows contract:

| Profile | Linux control posture |
| --- | --- |
| `playback-4out` | No hardware-control change; output-only workflow. |
| `traktor-dvs-vinyl` | Vinyl input mode, vinyl ground lift on, other ground lifts off, software lock on. |
| `traktor-dvs-cd-line` | CD/line input mode, CD/line ground lift on, other ground lifts off, software lock on. |
| `vinyl-recording` / `phono-recording` | Phono input mode, phono ground lift on, other ground lifts off, software lock on. |
| `line-recording` | CD/line input mode, CD/line ground lift on, other ground lifts off, software lock on. |
| `dj-set-recording` | No hidden A/B mode change; C/D and physical switch state must be validated. |
| `microphone` | No hidden control write; physical MIC/LINE switch is part of the test record. |
| `midi-only` / `midi-bridge` | No audio-control change. |
| `ground-diagnostics` / `engineering-diagnostics` | Software lock on; no hidden audio-quality claim. |

## Streaming Lessons

macOS showed that audio quality can fail even when counters look acceptable.
Windows design carried the same rule into ACX/KMDF: do not fake streaming, do
not call a build ready because it installs, and do not optimize CPU in isolation.

Linux implementation rules:

- Prefer the CAIAQ capture-paced model first. `snd-usb-caiaq` already mirrors
  playback packet layout from completed capture URBs, which matches the
  strongest macOS timing clue.
- Keep the host-facing PCM position stable. USB cadence can inform correction,
  but Linux must not expose jittery re-anchors as a clean client clock.
- Treat 44.1 kHz and 48 kHz as first-class validation rates. 88.2 and 96 kHz
  remain planned/diagnostic until physical capture passes.
- Preserve `S24_3BE` transport honesty. User-space conversion belongs in
  ALSA/PipeWire/JACK layers unless kernel evidence proves otherwise.
- If CAIAQ exposes four stereo substreams, validate that shape first. A single
  8x8 presentation is a user-space/client compatibility target until real app
  evidence requires a kernel change.

## Hot-Path Contract

Linux must inherit the same real-time hygiene from macOS and Windows:

- No heap allocation after stream start in isochronous completion paths.
- No steady-state logging, string formatting, file I/O, UI callbacks, or
  user-space policy work in packet completion.
- No broad lock shared by playback, capture, MIDI, controls, and diagnostics.
- Use fixed URB pools, fixed ring buffers, precomputed channel layout, bounded
  sample copy/pack/unpack, period accounting, and URB resubmission.
- Control writes and MIDI command traffic are serialized away from the PCM
  timing path.

## Diagnostics Linux Must Add Around CAIAQ

The macOS stream stats and Windows diagnostic structs define the minimum Linux
counter set:

- stream active/rate/format/buffer/period state;
- playback and capture frames submitted/delivered;
- ALSA XRUN counters and ALSA pointer failure reasons;
- capture URBs completed, failed, short, zero-length, and status-failed;
- playback URBs completed, failed, short, queue failed, and starvation count;
- period elapsed count per direction;
- packet byte count anomalies and layout signatures;
- input/output panic flags with cause counters;
- MIDI bytes in/out and EP1 command errors;
- control writes/profile applies and last control error;
- CPU and scheduling evidence collected outside the hot path.

Counters are evidence for debugging. They are not a sound-quality pass.

## Validation Mapping

| macOS/Windows lesson | Linux gate |
| --- | --- |
| No listening handoff from counters alone | Exact Linux package/module must pass physical capture first. |
| Real music caught failures synthetic tones missed | Linux quality gate includes music playback and external capture. |
| Metallic noise, clicks, white noise, and CPU/UI coupling are real bugs | Linux analyzer must record residual, click, coloration, xrun, CPU, and scheduling metrics. |
| Start/unpause white-noise bugs existed historically in CAIAQ | Linux tests must include repeated start/unpause and client restart at 44.1/48 kHz. |
| Channel mapping bugs break DVS even if audio exists | Linux must run A/B/C/D output and input isolation before Traktor/DVS claims. |
| Windows rejects `start` until streaming is real | Linux tools/packages must not imply PCM readiness from package installation. |
| Installer split prevents hidden driver risk | Linux tools, udev/UCM, and any DKMS/akmods path remain independently installable. |
| Hardware is shared | Any playback/capture/reset/bind/module action requires the shared hardware lock and log. |

## Implementation Actions From This Pass

Completed in this Linux worktree:

- `opena8dj-linuxctl` now exports a static hardware model derived from the
  macOS and Windows source oracles.
- `opena8dj-linuxctl verify` writes `hardware-model.json` alongside
  diagnostics.
- Linux profiles now include macOS-aligned ground-lift and software-lock
  behavior for DVS, phono, and line workflows.
- The packaged profile schema now records USB endpoints, A/B/C/D topology,
  source oracles, profile aliases, hot-path rules, and validation policy.

Still required before a normal Linux candidate:

- Re-run the CAIAQ source audit against the selected kernel baseline.
- Verify actual ALSA controls and substreams on a Linux host with Audio 8 DJ.
- Add kernel-side diagnostics without hot-path regressions.
- Run route matrix, MIDI, CPU/xrun, start/unpause, PipeWire/JACK, DVS, and
  physical capture gates on the exact built artifact.
