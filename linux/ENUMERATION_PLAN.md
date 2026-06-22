# Linux Enumeration Plan

Current readiness:

```text
diagnostic only, sound quality not validated
```

This plan defines the first real Linux hardware observation pass. It does not
authorize playback, capture, module replacement, USB reset, or service restart.

## Lock Rule

Enumeration against a physically attached Audio 8 DJ should use the shared
hardware lock if it can affect another agent's measurement window. Build-only
and offline read-only work does not need the lock.

Set:

```sh
export AUDIO_GATE_LOCK_ROOT="$HOME/.opena8dj/hardware-gate.lock"
```

Do not force the lock if it is occupied.

## Goal

Collect the exact Linux-visible surface for Audio 8 DJ:

- USB identity and descriptors.
- bound kernel module.
- ALSA card identity.
- PCM playback/capture subdevice shape.
- sample formats/rates from ALSA.
- rawmidi visibility.
- ALSA controls for input mode, ground lift, and software lock.
- kernel logs from probe.

No sound-quality claim can come from this pass.

## Read-Only Commands

Recommended first pass:

```sh
linux/tools/opena8dj-linuxctl diagnostics --json
lsusb -d 17cc:1978
cat /proc/asound/cards
cat /proc/asound/pcm
aplay -l
arecord -l
aconnect -l
amixer -c Audio8DJ controls
amixer -c Audio8DJ contents
dmesg --ctime | grep -Ei 'caiaq|audio8|audio 8|17cc|1978'
```

If the card id is not `Audio8DJ`, use the numeric card index reported by
`/proc/asound/cards`.

## Evidence Directory

Write results under a run directory outside package install paths, for example:

```text
local-analysis/linux-enumeration/<timestamp>/
```

Required files:

- `manifest.txt`
- `linuxctl-diagnostics.json`
- `lsusb.txt`
- `proc-asound-cards.txt`
- `proc-asound-pcm.txt`
- `aplay-l.txt`
- `arecord-l.txt`
- `aconnect-l.txt`
- `amixer-controls.txt`
- `amixer-contents.txt`
- `dmesg-caiaq.txt`

## Pass Criteria

Enumeration pass means:

- Audio 8 DJ appears as USB `17cc:1978`.
- A kernel module binds without manual recovery.
- ALSA card appears.
- PCM playback and capture surfaces are visible.
- rawmidi is visible if firmware reports MIDI.
- Audio 8 DJ controls are visible.
- The actual PCM shape is documented.

This label is still not sound quality:

```text
enumerates only
```

## Stop Conditions

Stop immediately and do not proceed to PCM smoke if:

- USB identity is not `17cc:1978`.
- ALSA card does not appear.
- controls are missing.
- rawmidi is missing when expected.
- dmesg shows probe failure.
- another hardware lock owner is active.
- the test host is using the device for another audio session.

