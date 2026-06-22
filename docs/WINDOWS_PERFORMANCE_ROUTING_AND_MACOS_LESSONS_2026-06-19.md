# Windows Performance, Routing, And macOS Lessons - 2026-06-19

## Goal

Windows must inherit the hard lessons from the macOS/C++ direction:

- high-fidelity audio first;
- low and stable CPU;
- no fake readiness;
- full hardware functionality;
- deterministic routing for DJ, DVS, recording, and general playback cases.

The Windows branch should not repeat experiments that were already shown to
trade lower CPU for worse physical audio quality.

## Baseline Principle

Counters are necessary but not sufficient.

For Windows, a build is not an audio-quality candidate just because:

- stream counters are clean;
- underruns are zero;
- the driver installs;
- the control tool works;
- CPU looks low in one synthetic run.

Audio quality needs physical or equivalent high-confidence capture evidence,
route-specific validation, and CPU/DPC evidence under realistic load.

## Lessons To Carry From macOS/C++ Work

### Keep The Hot Path Small

The streaming path must avoid:

- heap allocations;
- logging;
- string formatting;
- broad locks;
- variable-length scans;
- control-plane work;
- UI or diagnostics callbacks;
- blocking calls.

Windows equivalent:

- preallocate URBs/requests/buffers;
- use fixed-size rings;
- keep DPC/completion work bounded;
- move diagnostics aggregation out of the packet completion path;
- use atomics or narrow locks only for counters/state snapshots.

### Do Not Optimize CPU Blindly

macOS work showed that larger/coalesced USB scheduling can reduce CPU while
hurting physical output quality. Windows should treat transfer grouping,
latency, and packet cadence as a quality/performance matrix, not a single CPU
number.

Windows validation must compare:

- CPU;
- DPC latency;
- packet completion cadence;
- late completions;
- underruns/overruns;
- physical output residual/click metrics;
- user listening notes after numeric gates pass.

### Separate Playback, Capture, MIDI, And Controls

Playback-only should not pay input decode cost unless capture/DVS is active.
Capture-only should not pay playback cost. MIDI should not take audio locks.
Control writes should not block the audio path.

Windows implementation rule:

- separate render engine;
- separate capture engine;
- separate MIDI/control queue;
- shared hardware state updated through explicit, bounded transitions.

### Stable Clock Model

The host-facing audio clock must be stable. USB timing data can inform drift
handling, but Windows should not expose unstable re-anchors to the audio engine.

Windows implementation rule:

- stable WaveRT/ACX position model;
- explicit packet-to-frame accounting;
- no timestamp jumps visible to clients;
- drift correction measured against physical quality, not only counters.

### Diagnostics Without Jitter

Diagnostics are mandatory, but diagnostics cannot become the glitch source.

Windows implementation rule:

- counters are atomic or updated in bounded completion code;
- expensive formatting happens only in user-mode tools;
- diagnostic snapshots copy fixed structs;
- no per-packet trace logging in release audio path.

## Routing Model

The Audio 8 DJ must support several routing personalities. Windows should model
these as profiles layered over the same physical endpoints.

### Core Physical Pairs

Render:

- Output A L/R
- Output B L/R
- Output C L/R
- Output D L/R

Capture:

- Input A L/R
- Input B L/R
- Input C L/R
- Input D L/R

### User-Facing Routing Options

1. DJ deck routing:
   - Deck A -> Output A
   - Deck B -> Output B
   - optional C/D aux or monitor routing

2. Traktor DVS vinyl:
   - Input A/B set to timecode vinyl
   - vinyl ground-lift profile
   - software lock enabled
   - output A/B mapped to deck outputs

3. Traktor DVS CD/line:
   - Input A/B set to timecode CD/line
   - CD/line ground-lift profile
   - software lock enabled

4. Phono recording:
   - selected input pair in phono mode
   - recording app sees stable capture endpoint
   - playback path can stay idle

5. General Windows playback:
   - one stereo default endpoint mapped to Output A initially
   - advanced tools expose A/B/C/D selection

6. DAW / multichannel mode:
   - 8-channel render/capture endpoint or four stereo pairs depending on which
     Windows audio model proves more stable with real apps.

## Endpoint Strategy

Windows should start conservative:

1. expose stable 44.1/48 kHz;
2. prefer four stereo pair semantics if DJ apps behave better;
3. keep one 8-channel topology available as a design option;
4. hide 88.2/96 kHz until physical validation passes;
5. expose routing state through `opena8djctl` and future Control Center.

The current surface reports `OPENA8DJ_ENDPOINT_MODEL_DUAL_PROTOTYPE` because
the endpoint decision is not final. That is correct until Windows app behavior
is tested.

## Windows Performance Targets

Initial engineering targets:

- stable install/uninstall/rebind;
- no fake streaming state;
- no unbounded work in IOCTL handlers;
- render/capture packet counters;
- late-completion counters;
- DPC latency report during playback/capture;
- CPU profile under idle, playback, capture, and DVS load.

Candidate targets before "ready for audio testing":

- no sustained DPC spikes that correlate with audible glitches;
- no packet loss during 30-minute playback/capture;
- no CPU-growth trend during sustained playback;
- no routing cross-talk between A/B/C/D;
- no control-write glitch under active stream.

Audiophile/public target:

- physical capture gate passes;
- DVS scope behavior passes;
- routing matrix passes;
- CPU/DPC remains stable under UI/app stress;
- human listening does not report clicks, metallic bass, CPU-correlated noise,
  or image instability.

## Implementation Notes For Windows

### Driver

- Keep KMDF USB transport as the lower layer.
- Add the real isochronous render/capture engine only when packet cadence and
  buffer ownership are explicit.
- Use ACX/WaveRT-facing code only after the USB transport can prove stable
  frame accounting.
- Reject stream start until the engine is real; the current behavior is correct.

### Tools

- `opena8djctl` remains the low-level truth tool.
- Future Control Center should call the same stable IOCTL contract.
- Routing profiles should be declarative and inspectable.
- Tools must never perform high-frequency polling during audio streaming.

### Installer

- Driver installer owns Driver Store and hardware lock.
- Tools installer owns user-mode files.
- Bundle is only orchestration.
- Public installer requires Microsoft-signed catalog.

## Validation Matrix

Minimum Windows validation before audio-quality claims:

- Windows 10 22H2 x64 clean install.
- Windows 11 current x64 clean install.
- install/uninstall/reinstall.
- reboot and hotplug.
- Output A/B/C/D channel isolation.
- Input A/B/C/D channel isolation.
- 44.1 kHz and 48 kHz.
- Traktor DVS vinyl.
- Traktor DVS CD/line.
- MIDI in/out.
- CPU and DPC under UI stress.
- physical capture of real music.
- physical capture of tone/click tests.
- human listening after numeric gates pass.

## Current Repo Hooks

Existing Windows hooks:

- `opena8djctl surface`
- `opena8djctl topology`
- `opena8djctl diagnostics`
- `verify-driver.ps1`
- `package-installer.ps1`

These should grow into the Windows equivalent of the macOS quality ladder:

1. install verification;
2. surface/topology verification;
3. routing verification;
4. stream counter verification;
5. CPU/DPC gate;
6. physical audio-quality gate;
7. Traktor/DVS gate;
8. human listening gate.
