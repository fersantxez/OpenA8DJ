# Linux Driver Agent Implementation Contract

## Branch And Worktree

Recommended branch:

```text
linux/full-driver-agent
```

Recommended worktree:

```text
../opena8dj-linux-agent
```

Bootstrap:

```sh
./scripts/bootstrap-linux-driver-agent
```

## Owned Paths

The Linux agent owns:

- `linux/`
- `agents/linux-driver-agent/`
- `docs/LINUX_*`
- `docs/REALTIME_AUDIO_DRIVER_RESEARCH_WINDOWS_LINUX_2026-06-19.md` only for
  Linux-specific addenda
- `scripts/linux-*`
- `tools/linux/` if created

## Read-Only Or Ask-First Paths

Read-only unless explicitly authorized:

- `src/hal/`
- `src/tools/opena8dj-control.c`
- `macos/`
- `windows/`
- `resources/control-surfaces-*`
- macOS or Windows installer scripts

Shared schema changes are allowed only as narrow, documented patches.

## First Deliverables

1. `linux/README.md`
2. `linux/ARCHITECTURE.md`
3. `linux/QUALITY_AND_PERFORMANCE_GATES.md`
4. `linux/CONFIGURATION_MODEL.md`
5. `linux/SND_USB_CAIAQ_AUDIT.md`
6. initial build scaffold or explicit blocker if no Linux kernel build
   environment is available

## Implementation Quality Rules

- Prefer upstreamable Linux kernel style.
- Keep user-mode prototypes clearly separated from kernel driver work.
- Do not write a fake driver that only simulates success.
- Do not hide XRUNs or packet errors.
- Do not log per packet in the release hot path.
- Do not perform allocations in completion-sensitive paths.
- Do not mix PCM, MIDI, and control locks casually.
- Keep diagnostics cheap and snapshot-based.

## Configuration Model Requirements

Linux must mirror the OpenA8DJ control-surface concepts:

- profile name;
- sample rate;
- buffer/period hint;
- input mode;
- input decode active;
- ground lift flags;
- software lock;
- input pair labels;
- output pair labels;
- routing role per A/B/C/D pair;
- MIDI enabled;
- diagnostics level;
- import/export config.

Linux names and storage can differ, but the semantic model should stay portable.

## Validation Ladder

L0 build hygiene:

- kernel headers found;
- module builds;
- `modpost` clean;
- `sparse`/`checkpatch` where available.

L1 enumeration:

- `lsusb -v`;
- `dmesg`;
- `/proc/asound/cards`;
- `/proc/asound/devices`;
- `/proc/asound/pcm`;
- `aplay -l`;
- `arecord -l`;
- `aconnect -l`.

L2 PCM smoke:

- `speaker-test`;
- `aplay`;
- `arecord`;
- `alsaloop`;
- 44.1/48 kHz.

L3 routing:

- Output A/B/C/D isolation;
- Input A/B/C/D isolation;
- profile switching;
- no crossfeed.

L4 performance:

- CPU profile;
- `cyclictest` baseline;
- ftrace/perf around USB completions;
- ALSA tracepoints;
- XRUN and hwptr checks.

L5 physical quality:

- 1 kHz tone;
- real music;
- sidebands;
- clipping;
- click detection;
- CPU/noise correlation.

L6 DVS/MIDI:

- vinyl timecode;
- CD/line timecode;
- Traktor-compatible scope behavior where available;
- MIDI loopback.

L7 resilience:

- hotplug;
- USB reset;
- suspend/resume;
- long run;
- install/uninstall.

## Reporting Format

Each major update should include:

```text
branch:
worktree:
files changed:
commands run:
tests passed:
tests blocked:
hardware lock used: yes/no
audio quality status:
next action:
```

If hardware validation is unavailable, state:

```text
diagnostic only, sound quality not validated
```
