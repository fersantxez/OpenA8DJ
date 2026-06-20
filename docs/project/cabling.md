# Cable And Routing Options

The Audio 8 DJ exposes four stereo pairs. OpenA8DJ keeps that model visible to
macOS and to DJ applications.

```text
A: channels 1-2
B: channels 3-4
C: channels 5-6
D: channels 7-8
```

## Playback To An External Mixer

Use this for normal DJ playback.

```text
Mac / DJ app
  -> Open Audio 8 DJ
  -> OUT A 1-2 -> mixer channel 1
  -> OUT B 3-4 -> mixer channel 2
  -> OUT C 5-6 -> mixer channel 3
  -> OUT D 7-8 -> mixer channel 4
```

In the DJ app, assign deck outputs to the matching stereo pairs.

## Traktor Timecode Vinyl

Use this for turntables with timecode vinyl.

```text
Turntable A -> Audio 8 DJ input A
Turntable B -> Audio 8 DJ input B
Audio 8 DJ output A -> mixer channel A
Audio 8 DJ output B -> mixer channel B
```

Use the `DVS Vinyl` profile in Control Center and calibrate inside Traktor.

## CDJ Or Line-Level Timecode

Use this for CDJs, media players, or other line-level timecode sources.

```text
Player A -> Audio 8 DJ input A
Player B -> Audio 8 DJ input B
Audio 8 DJ output A -> mixer channel A
Audio 8 DJ output B -> mixer channel B
```

Use the `DVS CD-Line` profile in Control Center.

## Recording Vinyl

Use inputs A/B for turntables. Do not assume C/D are correct for phono
cartridges unless the hardware path has been specifically validated for that
use.

For archiving records, monitor input level and clipping in the recording app.
If the recording is noisy, document the cartridge, mixer, ground wire, cable
path, and Control Center profile before changing driver settings.
