# Troubleshooting

## The Device Does Not Appear

1. Unplug the Audio 8 DJ.
2. Plug it back in.
3. Open Audio MIDI Setup.
4. Look for `Open Audio 8 DJ`.
5. Reopen your DJ or audio app.

If it still does not appear, restart the Mac once and check Audio MIDI Setup
again.

## macOS Blocks The Installer

Use only the GitHub release download from this repository. If macOS blocks a
file downloaded from somewhere else, stop and download it again from GitHub.

The public 0.5.1 DMG and its installer are signed and notarized. You should not
need `Open Anyway` or disabled security settings. If macOS blocks the official
download:

1. Delete the downloaded DMG.
2. Download the DMG and checksum file again from the official 0.5.1 release.
3. Verify the checksum using the installation guide.
4. Stop and report the problem if the checksum matches but macOS still blocks
   the installer.

## Installer Reports An Error

If the package opens but macOS Installer reports an install error, verify the
download checksum and use the Terminal fallback for the package inside the
mounted DMG:

```sh
sudo installer -pkg "/Volumes/OpenA8DJ 0.5.1/OpenA8DJ-0.5.1.pkg" -target /
```

## Traktor Does Not Calibrate Timecode Vinyl

1. Confirm `Open Audio 8 DJ` is selected as the audio device.
2. Confirm Deck A input uses Audio 8 DJ input A.
3. Confirm Deck B input uses Audio 8 DJ input B.
4. Open Control Center.
5. Choose `DVS Vinyl`.
6. Click `Apply`.
7. Calibrate the vinyl again inside Traktor.

## Output Comes From The Wrong Mixer Channel

Check the output pairs:

```text
1-2: deck/output A
3-4: deck/output B
5-6: deck/output C
7-8: deck/output D
```

If your mixer channel is different, change the output assignment inside the DJ
or audio app.

## Sound Is Noisy Or Wrong

1. Stop playback.
2. Reconnect the Audio 8 DJ.
3. Reopen the audio app.
4. In Control Center, choose `DVS Vinyl` for timecode vinyl or
   `Playback / 4 Stereo Outputs` for playback-only use.
5. Try again.

If the problem is reproducible, open a GitHub issue with:

- macOS version
- audio app and version
- sample rate
- buffer size
- exact cables and mixer routing
- whether Control Center was set to `DVS Vinyl`, `DVS CD-Line`, or
  `Playback / 4 Stereo Outputs`
