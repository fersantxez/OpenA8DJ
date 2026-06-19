# Audio 8 DJ Control Surfaces Demo Runbook - 2026-06-19

## Goal

Record a short demo that shows what the new OpenA8DJ control surfaces can do:

- CLI: `opena8dj-control`
- UI: `OpenA8DJ Control Center`
- Presets and JSON export/import workflow
- Audible output through the Audio 8 DJ
- Close Encounters five-tone motif
- Low-high-low frequency sweep for system checks
- Safe state restore
- Installer artifacts and user documentation

The main demo must not require any cable changes. Cable-specific workflows are
documented as optional appendices for users who want to test those setups later.

## Hard Safety Rules

- Do not touch hardware before writing a pre-use log entry.
- Acquire the shared hardware lock before any live CLI/UI access.
- Keep the lock only while actively demonstrating live hardware state.
- Save the original config before applying presets.
- Restore the original config before releasing the lock.
- Confirm the lock is free after the demo.
- Play only the documented five-tone motif and frequency sweep at moderate
  amplitude.
- Do not run soundcheck in this demo.
- Do not install, unload, replace, or restart the HAL driver.
- Do not change physical cabling.

## Planned Output

Default run directory:

```text
local-analysis/control-surfaces-demo/<timestamp>
```

Expected artifacts:

```text
demo.mov
demo-plan.txt
close-encounters-five-tones.wav
low-high-low-sweep.wav
hardware-use-entry.txt
lock/result.txt
lock/manifest.txt
lock/command.stdout
lock/command.stderr
original.json
playback-4out.json
audio-playback.txt
restored.json
final-state.txt
lock-status-after.txt
```

## Demo Recording Method

macOS supports command-line video recording through `screencapture`:

```sh
screencapture -v -V 150 -k local-analysis/control-surfaces-demo/<timestamp>/demo.mov
```

Notes:

- `-v` records video.
- `-V 150` limits recording to 150 seconds.
- `-k` shows clicks.
- macOS may request Screen Recording permission the first time.

## Main Demo: No Cable Changes And Audible Output

This is the primary demo to execute now.

### Segment 1: Context And Installed Tools

Purpose: show that the control surfaces are separate from the full driver
installer and are safe to update independently.

Commands:

```sh
make tools-package
make tools-dmg
pkgutil --payload-files build/opena8dj-tools-0.3.135.pkg
```

Narration points:

- The package installs the app, CLI, and docs.
- It does not install HAL, LaunchAgents, or MIDI daemon.
- This is the right installer when the driver is already installed.

Diagram:

```text
opena8dj-tools PKG
        |
        +-- /Applications/OpenA8DJ Control Center.app
        +-- /usr/local/bin/opena8dj-control
        +-- /Library/Documentation/OpenA8DJ/ControlSurfaces

Not touched:
        x-- /Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
        x-- /Library/LaunchAgents/org.opena8dj.midid.plist
        x-- /usr/local/bin/opena8dj-midid
```

### Segment 2: CLI Discovery

Purpose: show the CLI can enumerate every built-in use-case preset.

Commands:

```sh
./build/opena8dj-control list-profiles
./build/opena8dj-control --help
```

No hardware lock is required for `list-profiles` or `--help`; they do not touch
the HAL bridge.

Narration points:

- The CLI is the low-level automation surface.
- The panel uses the same backend.
- Presets are named workflows, not arbitrary opaque modes.

### Segment 3: Locked CLI Live State, Film Motif, And Sweep

Purpose: show live read/export/apply/restore and prove audible output without
changing cable setup.

This segment must run under:

```sh
scripts/shared-hardware-lock-run --gate control-surfaces-demo ...
```

Actions inside the lock:

```sh
./build/opena8dj-control export-config original.json
./build/opena8dj-control
./build/opena8dj-control apply-preset playback-4out
./build/audio-wav-play close-encounters-five-tones.wav A
./build/audio-wav-play close-encounters-five-tones.wav B
./build/audio-wav-play close-encounters-five-tones.wav C
./build/audio-wav-play close-encounters-five-tones.wav D
./build/audio-wav-play low-high-low-sweep.wav A
./build/audio-wav-play low-high-low-sweep.wav B
./build/audio-wav-play low-high-low-sweep.wav C
./build/audio-wav-play low-high-low-sweep.wav D
./build/opena8dj-control export-config playback-4out.json
./build/opena8dj-control import-config original.json
./build/opena8dj-control export-config restored.json
./build/opena8dj-control > final-state.txt
```

Narration points:

- `export-config` captures a reversible state.
- `apply-preset playback-4out` demonstrates a safe no-cable-change preset.
- The Close Encounters five-tone motif is played through output pairs A/B/C/D.
- The motif is rendered as D, E, C, C one octave lower, G.
- A logarithmic sweep is played through A/B/C/D from 35 Hz to 16 kHz and back
  down to 35 Hz.
- `import-config` restores the exact original configuration.
- The wrapper releases the hardware lock when the command exits.

Diagram:

```text
Acquire lock
    |
    v
Export original config
    |
    v
Apply no-cable preset: playback-4out
    |
    v
Play five-tone motif through A, B, C, D
    |
    v
Play low-high-low sweep through A, B, C, D
    |
    v
Export changed config
    |
    v
Restore original config
    |
    v
Release lock
```

### Segment 4: UI Demo

Purpose: show the user-facing panel and how it maps to the same backend.

UI actions:

1. Open `build/OpenA8DJControlCenter.app`.
2. Show preset list.
3. Show selected preset details.
4. Show hardware state panel.
5. Show Export/Import controls.
6. Let the runner close the app automatically, or close it manually before the
   timeout.

Important:

- The app refreshes live state through the CLI backend.
- Therefore the app must be opened during the locked demo window.
- The app should be closed promptly.
- The script restores the saved original state after the app closes.
- The runner auto-closes the app after 25 seconds by default so the lock is not
  held indefinitely.

Diagram:

```text
OpenA8DJ Control Center.app
        |
        v
Embedded opena8dj-control
        |
        v
Same HAL control socket as CLI
        |
        v
Audio 8 DJ control state
```

### Segment 5: Final Proof

Commands:

```sh
./scripts/shared-hardware-lock-status
diff -u original.json restored.json
```

Narration points:

- Hardware lock is free.
- Original state was restored.
- Demo artifacts are stored in `local-analysis/control-surfaces-demo/<timestamp>`.

## Optional Cable-Specific Demos

These are not part of the no-cable-change demo. They are documented for users
who intentionally wire the matching setup.

### Optional A: Traktor DVS Vinyl

Preset:

```sh
opena8dj-control apply-preset traktor-dvs-vinyl
```

Cable diagram:

```text
Turntable A -- RCA --> CH A IN 1/2     CH A OUT 1/2 --> Mixer channel A
Turntable B -- RCA --> CH B IN 3/4     CH B OUT 3/4 --> Mixer channel B

Turntable ground wires --> Mixer ground and/or Audio 8 DJ GROUND
Mac USB <----------------------------------------------> Audio 8 DJ USB
```

Use when:

- Turntables are connected to A/B.
- Traktor scope needs timecode vinyl input.

Config effect:

- `inputMode=timecode-vinyl`
- vinyl ground lift on
- software lock on
- input decode on

### Optional B: Traktor DVS CD-Line

Preset:

```sh
opena8dj-control apply-preset traktor-dvs-cd-line
```

Cable diagram:

```text
CDJ / media player A --> CH A IN 1/2     CH A OUT 1/2 --> Mixer channel A
CDJ / media player B --> CH B IN 3/4     CH B OUT 3/4 --> Mixer channel B

Mac USB <------------------------------------------------> Audio 8 DJ USB
```

Use when:

- CDJs or line players are feeding timecode or line signal.

Config effect:

- `inputMode=timecode-cd-line`
- CD-line ground lift on
- software lock on
- input decode on

### Optional C: Vinyl Recording

Preset:

```sh
opena8dj-control apply-preset vinyl-recording
```

Cable diagram:

```text
Turntable A --> CH A IN 1/2 --> Recording app inputs 1/2
Turntable B --> CH B IN 3/4 --> Recording app inputs 3/4
```

Use when:

- Recording phono cartridges through A/B.

Config effect:

- `inputMode=phono`
- phono ground lift on
- software lock on
- input decode on

### Optional D: DJ Set Recording

Preset:

```sh
opena8dj-control apply-preset dj-set-recording
```

Cable diagram:

```text
Mixer REC OUT / BOOTH OUT / second MASTER
        |
        v
Audio 8 DJ C/D line input path
        |
        v
Recording app
```

Use when:

- Capturing the mixer final stereo output.

Config effect:

- input decode on
- software lock off

### Optional E: Effects Loop

Preset:

```sh
opena8dj-control apply-preset effects-loop
```

Cable diagram:

```text
Mixer send ---> Audio 8 DJ input C/D ---> Software FX
Software FX ---> Audio 8 DJ output C/D ---> Mixer return
```

Use when:

- Routing audio through software or external effects.

Safety:

- Start sends and returns low.
- Watch for feedback.

### Optional F: Microphone

Preset:

```sh
opena8dj-control apply-preset microphone
```

Cable diagram:

```text
Microphone -- XLR --> Front MIC input
                       |
                       v
                 Recording / streaming app
```

Use when:

- A dynamic microphone is connected to the front XLR input.

Safety:

- Set the physical MIC/LINE switch correctly.
- Audio 8 DJ does not provide phantom power.

## Execution Command

Prepared runner:

```sh
./scripts/control-surfaces-demo-run
```

Dry run:

```sh
./scripts/control-surfaces-demo-run --dry-run
```

Custom UI dwell time:

```sh
./scripts/control-surfaces-demo-run --ui-seconds 40
```

External recording mode:

```sh
RUN_DIR=local-analysis/control-surfaces-demo/$(date +%Y%m%d-%H%M%S)-external-record
mkdir -p "$RUN_DIR"
screencapture -v -V 80 -k "$PWD/$RUN_DIR/demo.mov" &
./scripts/control-surfaces-demo-run --no-record --run-dir "$RUN_DIR" --ui-seconds 20
```

Use external recording mode when Terminal can run the visible demo but does not
have macOS Screen Recording permission. The external recorder must be started by
the process that has permission to record the screen.

The real run will:

1. Write a hardware-use log entry.
2. Start screen recording.
3. Generate the Close Encounters five-tone WAV.
4. Generate the low-high-low sweep WAV.
5. Run no-hardware CLI discovery.
6. Acquire the shared hardware lock.
7. Save original hardware config.
8. Demonstrate live CLI export/apply/import.
9. Play the five-tone motif through output pairs A/B/C/D.
10. Play the sweep through output pairs A/B/C/D.
11. Open the UI while the lock is held.
12. Restore original config.
13. Release the lock.
14. Confirm `shared_hardware_lock=FREE`.

## Ready Criteria

Before execution:

- `make control-center` passes.
- `screencapture -h` shows `-v` and `-V`.
- The user is available to approve macOS Screen Recording permission if macOS
  prompts.
- No one else is using the Audio 8 DJ hardware.
- The user confirms: execute the demo.
