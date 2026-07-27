# opena8dj-tools User Guide

## What Gets Installed

The opena8dj-tools installer is separate from the full OpenA8DJ driver
installer. It installs:

- `/Applications/OpenA8DJ Control Center.app`
- `/usr/local/bin/opena8dj-control`
- `/Library/Documentation/OpenA8DJ/ControlSurfaces`

It does not install, replace, unload, or restart the HAL driver. It does not
install the MIDI LaunchAgent. Use the full OpenA8DJ installer when the driver
itself needs to be installed or updated.

## Build And Install

Local development build:

```sh
make control-center
open build/OpenA8DJControlCenter.app
```

Local installer package:

```sh
make tools-package
sudo installer -pkg build/opena8dj-tools-0.3.135.pkg -target /
```

Disk image:

```sh
make tools-dmg
open build/opena8dj-tools-0.3.135.dmg
```

Manual local install without building a package:

```sh
make install-control-surfaces
```

Uninstall only the control surfaces:

```sh
sudo /Library/Documentation/OpenA8DJ/ControlSurfaces/uninstall-opena8dj-control-surfaces.sh
```

## Control Surface Model

The macOS panel and the CLI share the same backend. The app bundle embeds the
matching CLI binary and calls it for every read/write.

```text
User
  |
  +-- OpenA8DJ Control Center.app
  |      |
  |      +-- Contents/Resources/opena8dj-control
  |
  +-- Terminal: /usr/local/bin/opena8dj-control
         |
         v
  /tmp/opena8dj-control.sock
         |
         v
  OpenA8DJ HAL bridge
         |
         v
  Native Instruments Audio 8 DJ
```

## Safety Rule For Shared Hardware

When the Audio 8 DJ is shared with another development lane, do not use the CLI
or panel casually. Log the hardware access first, acquire the shared hardware
lock before touching live state, restore the previous config, and release the
lock immediately afterwards.

The panel is an end-user UI and does not acquire the development lock itself.
For locked engineering work, prefer the CLI through:

```sh
scripts/shared-hardware-lock-run --gate control-surface-work --run-dir local-analysis/... -- ./build/opena8dj-control ...
```

## Open The Panel

Installed app:

```sh
open "/Applications/OpenA8DJ Control Center.app"
```

Development app:

```sh
open build/OpenA8DJControlCenter.app
```

The panel supports:

- Preset selection.
- Apply selected preset.
- Refresh current hardware state.
- Export current config as JSON.
- Import a saved JSON config.

It intentionally does not yet expose every low-level field as manual controls.
Use the CLI for individual toggles and routing transforms.

## CLI Quick Reference

List presets:

```sh
opena8dj-control list-profiles
```

Read current hardware state:

```sh
opena8dj-control
```

Apply a preset:

```sh
opena8dj-control apply-preset traktor-dvs-vinyl
```

Export state:

```sh
opena8dj-control export-config ~/Desktop/opena8dj-config.json
```

Import state:

```sh
opena8dj-control import-config ~/Desktop/opena8dj-config.json
```

Read without waking the HAL bridge:

```sh
OPENA8DJ_CONTROL_NO_WAKE=1 opena8dj-control export-config -
```

## Public JSON API v1

Local applications can use the versioned process API without linking against a
driver SDK or depending on the private socket protocol:

```sh
opena8dj-control api version
opena8dj-control api profiles
opena8dj-control api profile
opena8dj-control api stats
opena8dj-control api profile set traktor-dvs-vinyl
opena8dj-control api driver-modes
opena8dj-control api driver-mode
opena8dj-control api driver-mode set performance
opena8dj-control api driver-mode set balanced
```

### Driver modes are separate from hardware profiles

A hardware `profile` configures input/electrical and routing-related state such
as `timecode-vinyl`, phono grounding, input mapping, decode, and software lock.
A `driver-mode` selects a safe runtime scheduling/buffering policy. These are
independent axes: selecting `traktor-dvs-vinyl` does not select performance
mode, and selecting performance mode does not alter the input profile, sample
rate, routing, or Core Audio buffer minimum.

Copy/paste discovery and read-back:

```sh
opena8dj-control api driver-modes | python3 -m json.tool
opena8dj-control api driver-mode | python3 -m json.tool
```

Request the experimental performance policy for this Core Audio host process:

```sh
opena8dj-control api driver-mode set performance | python3 -m json.tool
opena8dj-control api driver-mode | python3 -m json.tool
```

Return to the shipping balanced policy:

```sh
opena8dj-control api driver-mode set balanced | python3 -m json.tool
```

Driver-mode selection is session-only. A Core Audio host process restart
restores `balanced`; no plist or defaults entry is written. If a stream is
active, a different valid request is accepted with `pending: true` while
`effectiveMode` continues to describe the current stream. It is promoted only
at a safe stop/next-start boundary. Requesting the current effective mode
cancels a pending change.

`performance` currently means the internal 4096-frame output policy plus
user-interactive QoS on the USB worker block. It is an experimental candidate,
not a claim of lower measured latency or equivalent stability. Keep using
`balanced` unless you are deliberately evaluating it.

Read the same mode state and counters inside the non-destructive stats API:

```sh
opena8dj-control api stats |
  python3 -c 'import json,sys; print(json.load(sys.stdin)["data"]["driverMode"])'
```

An older compatible HAL without the append-only mode tail returns
`"driverMode": null`; clients must not interpret that as balanced. Driver-mode
writes share the same authenticated per-user mutation lock as profile writes,
and successful sets are verified by a same-connection read-back.

### USB link quality meter

The read-only meter polls the same non-destructive stream snapshot used by
`api stats`; it does not start Core Audio or wake the interface:

```bash
opena8dj-control usb-quality
opena8dj-control usb-quality --interval-ms 1000 --count 30
opena8dj-control usb-quality --json --interval-ms 1000 --count 30
```

JSON mode is newline-delimited JSON, one complete observation per line.
Percentiles are fixed histogram bounds (`upperBoundUs`) rather than invented
exact values; a result above the last bin is explicitly reported as
`overflow: true`. An older HAL reports `instrumentationAvailable: false`
instead of a false healthy zero-error result.

Every `api` invocation writes exactly one newline-terminated JSON object to
standard output. Check both the process exit status and the `ok` member. For
example, this Python snippet reads a non-destructive statistics snapshot:

```python
import json
import subprocess

result = subprocess.run(
    ["opena8dj-control", "api", "stats"],
    check=False,
    capture_output=True,
    text=True,
)
response = json.loads(result.stdout)
if result.returncode != 0 or not response["ok"]:
    raise RuntimeError(response["error"]["code"])
print(response["data"]["output"]["underruns"])
```

`api version`, `api profiles`, and `api driver-modes` work without a running
driver. API reads and writes never start Core Audio; they return
`backend_unavailable` when the authenticated local HAL bridge is not already
running. The bridge is hosted by the `_coreaudiod` system account,
authenticates every connecting process using Unix peer credentials, and
permits only root, its own host UID, or the current console user. The client
independently verifies the server peer and socket identity. Profile and
driver-mode writes accept only exact IDs returned by their respective list
operations and verify state by reading it back.

Peer enforcement on the server requires the matching updated OpenA8DJ HAL
driver. Installing only a newer control tool cannot add server-side
authentication to an older HAL, although the new client still authenticates
the server before using the public API.

The schema identifier is `org.opena8dj.public-api.response.v1` and the initial
API version is `1.0`. Applications may ignore unknown object members added by a
future compatible minor release. They should branch on documented error codes,
not human-readable messages or JSON member ordering. The packed binary protocol
at `/tmp/opena8dj-control.sock` remains private and is not a supported client
interface.

Build the CLI and run the complete mock-backed contract suite without hardware:

```sh
make driver-mode-offline-test
make public-api-offline-test
make usb-quality-offline-test
```

## Case 1: Playback / Four Stereo Outputs

Preset:

```sh
opena8dj-control apply-preset playback-4out
```

Use when the Audio 8 DJ is acting as an output interface for software playback,
Spotify/VLC testing, DAW playback, or four stereo output pairs into a mixer.

```text
Mac / Core Audio
      |
      v
Audio 8 DJ USB
      |
      +-- OUT A 1/2 ---> Mixer channel 1
      +-- OUT B 3/4 ---> Mixer channel 2
      +-- OUT C 5/6 ---> Mixer channel 3 / aux
      +-- OUT D 7/8 ---> Mixer channel 4 / aux
```

Configured behavior:

- Input decode off.
- Output pairs A/B/C/D remain available.
- No phono/timecode state is required.

## Case 2: Traktor DVS With Timecode Vinyl

Preset:

```sh
opena8dj-control apply-preset traktor-dvs-vinyl
```

Use for turntables with timecode vinyl on deck A and deck B.

```text
Turntable A -- RCA --> Audio 8 DJ CH A IN 1/2
Audio 8 DJ CH A OUT 1/2 --> Mixer channel A

Turntable B -- RCA --> Audio 8 DJ CH B IN 3/4
Audio 8 DJ CH B OUT 3/4 --> Mixer channel B

Ground wires --> Mixer ground and/or Audio 8 DJ GROUND, tested one path at a time
```

Configured behavior:

- Input mode: `timecode-vinyl`
- Vinyl ground lift on.
- Software lock on.
- Input decode on.

Notes:

- Use A/B for phono cartridges.
- C/D are line-level paths, not phono preamps.
- Validate Traktor scope after applying the preset.

## Case 3: Traktor DVS With CDJ / Line Timecode

Preset:

```sh
opena8dj-control apply-preset traktor-dvs-cd-line
```

Use for CDJs, media players, or line-level timecode sources.

```text
CDJ A / line player --> Audio 8 DJ CH A IN 1/2 --> Mixer channel A
CDJ B / line player --> Audio 8 DJ CH B IN 3/4 --> Mixer channel B

Mac / Traktor <--> Audio 8 DJ USB
```

Configured behavior:

- Input mode: `timecode-cd-line`
- CD-line ground lift on.
- Software lock on.
- Input decode on.

## Case 4: Vinyl Recording

Preset:

```sh
opena8dj-control apply-preset vinyl-recording
```

Use for recording vinyl from turntables connected to A/B.

```text
Turntable A ---> Audio 8 DJ CH A IN 1/2 ---> Recording app inputs 1/2
Turntable B ---> Audio 8 DJ CH B IN 3/4 ---> Recording app inputs 3/4
```

Configured behavior:

- Input mode: `phono`
- Phono ground lift on.
- Software lock on.
- Input decode on.

## Case 5: DJ Set Recording

Preset:

```sh
opena8dj-control apply-preset dj-set-recording
```

Use when the mixer already contains the final DJ mix and you want to capture a
stereo record output.

```text
Mixer REC OUT / BOOTH OUT / second MASTER
      |
      v
Audio 8 DJ CH C/D line input path
      |
      v
Recording app
```

Configured behavior:

- Input decode on.
- Software lock off.
- A/B input mode is preserved where possible.

## Case 6: External Effects Loop

Preset:

```sh
opena8dj-control apply-preset effects-loop
```

Use for a software effects send/return or an external processing loop.

```text
Mixer send ---> Audio 8 DJ input C/D ---> Software FX
Software FX ---> Audio 8 DJ output C/D ---> Mixer return
```

Configured behavior:

- Input decode on.
- Software lock off.
- Input transforms reset.

Start with sends and returns low to avoid feedback.

## Case 7: Microphone

Preset:

```sh
opena8dj-control apply-preset microphone
```

Use the front XLR mic path.

```text
Microphone -- XLR --> Audio 8 DJ front MIC input
                       |
                       +-- physical MIC/LINE switch must be set correctly
                       |
                       v
                    Recording / streaming app
```

Configured behavior:

- Input decode on.
- Software lock off.

Hardware requirements:

- Set the physical MIC/LINE switch to MIC.
- No phantom power is provided by the Audio 8 DJ.

## Case 8: MIDI Only

Preset:

```sh
opena8dj-control apply-preset midi-only
```

Use when the Audio 8 DJ is mainly a DIN MIDI bridge and audio routing should be
kept playback-safe.

```text
MIDI controller OUT --> Audio 8 DJ MIDI IN --> macOS CoreMIDI
macOS CoreMIDI -----> Audio 8 DJ MIDI OUT --> external MIDI device
```

Configured behavior:

- Input decode off.
- Playback-safe state.

The separate MIDI LaunchAgent comes from the full driver installer, not from the
control-surfaces-only installer.

## Case 9: Ground Diagnostics

Preset:

```sh
opena8dj-control apply-preset ground-diagnostics
```

Use to compare noise with different grounding arrangements.

```text
Test one change at a time:

Turntable ground -> Mixer ground
Turntable ground -> Audio 8 DJ GROUND
Ground lift state -> preset-controlled state
Mixer power path -> same outlet / different outlet
```

Configured behavior:

- Input decode on.
- Software lock on.
- Input transforms reset.

Record each physical cabling change before measuring, because software cannot
infer the actual ground wire placement.

## Case 10: Engineering Diagnostics

Preset:

```sh
opena8dj-control apply-preset engineering-diagnostics
```

Use for low-level debug sessions where the current state is saved before the
test and restored afterwards.

```text
Save config -> Apply diagnostics preset -> Run one measurement -> Restore config
```

Recommended wrapper:

```sh
scripts/shared-hardware-lock-run --gate engineering-diagnostics --run-dir local-analysis/... -- bash -lc '
  ./build/opena8dj-control export-config original.json
  trap "./build/opena8dj-control import-config original.json" EXIT
  ./build/opena8dj-control apply-preset engineering-diagnostics
  ./build/opena8dj-control export-config after.json
'
```

## JSON Config Workflow

The JSON export/import workflow is the bridge between GUI, CLI, tests, and
support cases.

```text
Export JSON
   |
   +-- attach to bug report
   +-- import on another machine
   +-- restore after diagnostics
   +-- compare before/after preset changes
```

Example:

```sh
opena8dj-control export-config before.json
opena8dj-control apply-preset traktor-dvs-vinyl
opena8dj-control export-config after.json
diff -u before.json after.json
```

## Current Limitations

The current panel configures complete presets. It does not yet provide a full
advanced editor for:

- Individual low-level toggles.
- Per-pair source remapping.
- Per-pair swap/invert transforms.
- Sample rate and buffer size.
- Input meters and test tone workflows.
- Sound-quality validation gates.

Those controls remain CLI/engineering workflows until the preset surface is
stable enough to add an advanced mode.
