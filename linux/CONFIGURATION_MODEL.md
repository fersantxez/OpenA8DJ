# Linux Configuration Model

This document defines the Linux-facing configuration and profile model for
Audio 8 DJ. It is design intent only until implemented and validated.

```text
diagnostic only, sound quality not validated
```

## ALSA Card Identity

Preferred names:

- Card long name: `OpenA8DJ Audio 8 DJ`
- Short name: `OpenA8DJ`
- Driver name: `snd-opena8dj` only if a new module is created.
- If extending CAIAQ, preserve kernel naming expectations while adding clear
  Audio 8 DJ controls and channel maps.

## Channel Pairs

The driver must keep pair naming stable across ALSA, PipeWire, JACK, and DAW
use:

| Pair | Playback channels | Capture channels | Primary use |
| --- | --- | --- | --- |
| A | 1-2 | 1-2 | Deck A / input A |
| B | 3-4 | 3-4 | Deck B / input B |
| C | 5-6 | 5-6 | Deck C / input C |
| D | 7-8 | 7-8 | Deck D / input D |

Channel maps must not swap pairs to satisfy a user-space naming preference.
Compatibility remaps belong in user-space profiles, not the kernel hot path.

## Hardware Controls

The required Audio 8 DJ controls are:

| Control | Values | Notes |
| --- | --- | --- |
| Input Mode | `timecode-vinyl`, `timecode-cd-line`, `phono` | Backed by the CAIAQ input mode byte. |
| GND Lift Vinyl | `off`, `on` | Applies to timecode vinyl mode. |
| GND Lift CD/Line | `off`, `on` | Applies to CD/line mode. |
| GND Lift Phono | `off`, `on` | Applies to phono mode. |
| Software Lock | `off`, `on` | Prevents accidental hardware/control changes where supported. |
| Profile | See below | Should be user-readable and scriptable. |
| Diagnostics Reset | trigger | Clears non-critical counters, never hides XRUN history from active clients. |

The upstream CAIAQ control names for Audio 8 DJ already identify input mode,
three ground-lift flags, and software lock. The Linux track must preserve the
underlying semantics while improving discoverability where possible.

## Profiles

Profiles are convenience presets. They must not hide what controls changed.

### `playback-4out`

- Playback A/B/C/D enabled.
- Capture left available but not required by the profile.
- Input mode unchanged unless explicitly requested.
- Intended for DJ playback and multichannel output checks.

### `traktor-dvs-vinyl`

- Playback A/B/C/D enabled.
- Capture A/B/C/D enabled.
- Input mode: `timecode-vinyl`.
- Ground lift: vinyl on; CD/line and phono off.
- Software lock: `on`.
- Ground-lift state explicit in the profile output.

### `traktor-dvs-cd-line`

- Playback A/B/C/D enabled.
- Capture A/B/C/D enabled.
- Input mode: `timecode-cd-line`.
- Ground lift: CD/line on; vinyl and phono off.
- Software lock: `on`.
- Ground-lift CD/line state explicit in the profile output.

### `vinyl-recording` / `phono-recording`

- Capture A/B/C/D enabled.
- Input mode: `phono`.
- Ground lift: phono on; vinyl and CD/line off.
- Software lock: `on`.
- Playback available but not required.
- Diagnostics should report input level and packet anomalies, not perform DSP.

### `line-recording`

- Capture A/B/C/D enabled.
- Input mode: `timecode-cd-line`.
- Ground lift: CD/line on; vinyl and phono off.
- Software lock: `on`.
- Intended for DJ set capture or line-level recording.

### `dj-set-recording`

- C/D line capture workflow.
- No hidden A/B input-mode change.
- Physical front-panel MIC/LINE state must be recorded during validation.

### `effects-loop`

- C/D duplex workflow.
- No hidden routing or hardware-control change.
- Pair isolation must prove send/return mapping.

### `microphone`

- Front XLR workflow.
- No hidden control write.
- Physical MIC/LINE switch and analog gain setting must be recorded.

### `daw-multichannel`

- 8 playback and 8 capture channels exposed.
- No deck-specific assumptions.
- Stable channel maps and full-duplex timing are mandatory.

### `midi-bridge`

- rawmidi input/output active and visible to ALSA clients.
- PCM configuration unchanged.

### `ground-diagnostics` / `engineering-diagnostics`

- Software lock: `on`.
- Used for controlled measurement; does not imply sound-quality validation.

### `unlock`

- Software lock: `off`.
- Must be an explicit user action.

## Persistence

Kernel controls should reflect device state, not hidden policy. Any persistent
user preference should live in user-space configuration such as:

- ALSA UCM profile snippets.
- PipeWire/WirePlumber configuration.
- A future OpenA8DJ Linux control tool.

The kernel path should remain deterministic across hotplug and avoid implicit
profile restore until the restore behavior is designed and validated.

## CLI and User-Space Tooling

The current diagnostic package provides `opena8dj-linuxctl` with:

```sh
opena8dj-linuxctl status
opena8dj-linuxctl diagnostics --json --controls
opena8dj-linuxctl controls
opena8dj-linuxctl list-profiles
opena8dj-linuxctl apply-profile traktor-dvs-vinyl --yes
opena8dj-linuxctl set-control input-mode phono --yes
opena8dj-linuxctl hardware-model --json
opena8dj-linuxctl verify --controls --report-dir ~/opena8dj-linux-report
```

Hardware-control writes require `--yes`. Package install, status, diagnostics,
hardware-model, list-profiles, and verify do not play audio, record audio, load
modules, reset USB, or restart audio services.

## Compatibility Targets

The target configuration must be usable from:

- ALSA direct clients.
- PipeWire.
- JACK.
- Traktor under supported Linux/Wine or compatible routing setups, if used.
- DAWs using ALSA/PipeWire/JACK.
- `aconnect` and rawmidi-compatible tools.

Compatibility is validated by real enumeration and routing checks, not by
names alone.
