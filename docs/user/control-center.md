# Control Center

OpenA8DJ Control Center changes how the Audio 8 DJ inputs and outputs are used.
It is installed automatically by the OpenA8DJ 0.5.1 DMG; there is no separate
tools download.

Open it from the Applications folder:

```text
/Applications/OpenA8DJ Control Center.app
```

## Choose A Profile

1. Connect the Audio 8 DJ.
2. Close any audio application that is actively using the device.
3. Open OpenA8DJ Control Center.
4. Select the profile that matches the connected equipment.
5. Click `Apply`.
6. Reopen the audio application if necessary.

## Common Profiles

### DVS Vinyl

Use this for Traktor Scratch or another DVS application with control vinyl.
Turntables normally connect to inputs A and B. This is the default profile in
OpenA8DJ 0.5.1.

### DVS CD-Line

Use this for CDJs, media players, or another line-level timecode source. Do not
use the vinyl profile for a line-level signal.

### Playback / 4 Stereo Outputs

Use this when the Audio 8 DJ is providing four stereo outputs and its inputs
are not needed.

### Vinyl Recording

Use this to record records through the phono inputs without a DVS timecode
workflow.

### DJ Set Recording

Use this to record a mixer or other line-level source through the Audio 8 DJ
inputs.

### Effects Loop

Use this when routing audio out to external effects and back into the
interface.

### Microphone

Use this with the front microphone input. The physical MIC/LINE switch on the
Audio 8 DJ must also be in the correct position.

## After Applying A Profile

Open Audio MIDI Setup or the audio application and confirm that `Open Audio 8
DJ` is still selected. Profile changes do not assign Traktor decks; input and
output deck assignments remain inside Traktor.

## Export Support Information

When reporting a problem, use Control Center to export the current
configuration. Attach that file to the GitHub issue together with the macOS
version, application version, sample rate, buffer size, and physical routing.

Normal users do not need the included command-line tool. It is available for
diagnostics and scripted support.
