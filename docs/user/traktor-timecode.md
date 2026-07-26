# Traktor And Timecode Vinyl

This guide configures an Audio 8 DJ for two control-vinyl decks and an external
DJ mixer. The same channel map can be adapted for one deck or for inputs C and
D.

## Connect The Equipment

For the common two-deck setup:

```text
Left turntable  -> Audio 8 DJ input A
Right turntable -> Audio 8 DJ input B
Audio 8 DJ output A -> mixer channel for Deck A
Audio 8 DJ output B -> mixer channel for Deck B
```

Connect turntables to phono inputs. A CDJ or another line-level timecode source
needs the `DVS CD-Line` profile instead.

## Select The Vinyl Profile

1. Open **OpenA8DJ Control Center** from Applications.
2. Select **DVS Vinyl**.
3. Click **Apply**.
4. Close and reopen Traktor if it was already running.

OpenA8DJ selects DVS Vinyl during installation, so these steps normally only
need to be repeated after using the interface for another purpose.

## Select The Audio Device In Traktor

1. Open Traktor Preferences.
2. Open **Audio Setup**.
3. Choose **Open Audio 8 DJ** as the audio device.
4. Choose either 44.1 kHz or 48 kHz.

If the device is not in the list, close Traktor, reconnect the Audio 8 DJ, and
open Traktor again.

## Assign Outputs

For an external mixer, assign each Traktor deck to its matching stereo output:

| Traktor deck | OpenA8DJ channels | Audio 8 DJ socket |
| --- | --- | --- |
| Deck A | Output A left/right | A |
| Deck B | Output B left/right | B |
| Deck C | Output C left/right | C |
| Deck D | Output D left/right | D |

In Traktor, these may also appear as numbered channel pairs:

```text
1-2: Output A
3-4: Output B
5-6: Output C
7-8: Output D
```

## Assign Timecode Inputs

Open Traktor's input routing and assign:

| Traktor deck | OpenA8DJ input |
| --- | --- |
| Deck A | Input A left/right |
| Deck B | Input B left/right |

Use Input C or D only when the turntable is physically connected to that input
pair.

## Calibrate The Vinyl

1. Put a deck in Traktor's Scratch Control mode.
2. Start the control record.
3. Open Traktor's calibration or scope view.
4. Run calibration for that deck.
5. Repeat for the other deck.

A healthy setup should produce a stable scope and a deck that follows the
record promptly in both directions. Lifting the needle should stop the control
signal.

## If Calibration Fails

Check these items in order:

1. **Open Audio 8 DJ** is still selected in Traktor.
2. The Traktor input pair matches the physical Audio 8 DJ input.
3. Control Center is set to **DVS Vinyl**, not **DVS CD-Line** or a recording
   profile.
4. The turntable, cartridge, and cables produce a signal on both left and right
   channels.
5. The deck is using the correct timecode-vinyl type and Scratch Control mode.
6. The control record is clean enough to calibrate.

If the scope is present but unstable, test one deck at a time and exchange the
cartridge or cable with the working deck. This helps separate a routing problem
from a physical signal problem.

## CDJs Or Line-Level Timecode

For a CDJ or media player:

1. Open Control Center.
2. Select **DVS CD-Line**.
3. Click **Apply**.
4. Keep the Traktor input assignments matched to the physical input pairs.
5. Calibrate the timecode source again.

Do not send a line-level player into a vinyl/phono profile.

## What OpenA8DJ Does Not Configure

OpenA8DJ makes the Audio 8 DJ channels and hardware profiles available to
macOS. Traktor still owns:

- deck input assignments;
- deck output assignments;
- internal or external mixing mode;
- Scratch Control mode;
- timecode calibration.

Changing a Control Center profile does not replace those Traktor settings.

For general device, installer, or sound problems, continue with
[Troubleshooting](troubleshooting.md).
