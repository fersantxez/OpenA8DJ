# OpenA8DJ 0.4.0 Release Notes

Historical release note: 0.4.0 is preserved for changelog and archival context.
New users should use the latest 0.5.x release from GitHub Releases.

OpenA8DJ 0.4.0 is a macOS public preview for the Native Instruments Audio 8 DJ
USB interface.

OpenA8DJ is an independent, open-source preservation project. It is not
affiliated with, endorsed by, sponsored by, or certified by Native Instruments,
and it does not include Native Instruments driver binaries, firmware,
installers, logos, or proprietary payloads.

## Highlights

- Makes the public `main` branch the current macOS driver line.
- Preserves the previous C/Objective-C implementation on the `legacy` branch
  for baseline comparison, emergency reference, physical-test history, and
  behavior learned from the Linux CAIAQ / `snd-usb-caiaq` reverse-engineering
  lineage.
- Provides an easy macOS download/install path through GitHub Releases:
  download the DMG, open it, and run the bundled PKG installer.
- Uses a modern macOS user-space driver shape: Core Audio HAL for the current
  validated installable preview, IOUSBHost USB transport, CoreMIDI endpoints,
  and a DriverKit/AudioDriverKit structure for the forward System Extension
  path.
- Restores the Traktor/DVS-facing Core Audio topology: 8 inputs / 8 outputs.
- Exposes one 8-channel input stream with named Input A/B/C/D channel pairs,
  plus 4 stereo output streams named Output A/B/C/D. This preserves the
  Traktor channel assignment surface while avoiding the multi-input-stream
  Core Audio enumeration instability found in earlier experiments.
- Re-enables Audio 8 DJ input decoding so Traktor can be assigned the physical
  timecode input pairs.
- Keeps the capture-paced output transport, stable Core Audio clocking, and
  playback-quality improvements accepted in human validation.
- Sets the default build profile to the accepted 4-output-stream, 8-in/8-out
  profile used for the final loaded driver.
- Includes repeatable audio QA tooling for HAL topology, tone sidebands,
  real-music residuals, click outliers, CPU guards, and package safety checks.

## Traktor / Timecode

Use the hardware DVS profile before timecode testing:

```sh
/usr/local/bin/opena8dj-control profile timecode-vinyl
```

Expected Traktor assignments:

```text
Deck A timecode input -> Input A L/R
Deck B timecode input -> Input B L/R
Deck A output -> Output A L/R
Deck B output -> Output B L/R
```

Operator validation for this release reported responsive timecode behavior.
The release still keeps full DVS matrix testing open across every physical
input pair and mode.

## Validation Snapshot

Final local checks for this release line:

- HAL package safety load: PASS.
- HAL parity topology: expected 1 input stream / 4 output streams, 8 input
  channels / 8 output channels.
- Installed Core Audio enumeration: PASS, `Open Audio 8 DJ` appears as
  8 inputs / 8 outputs at 48 kHz.
- Installed channel inspection: PASS, Input A/B/C/D and Output A/B/C/D channel
  names are present.
- MIDI endpoint publication: PASS, `Open Audio 8 DJ MIDI In` and
  `Open Audio 8 DJ MIDI Out` are present.
- `opena8dj-control profile timecode-vinyl`: PASS, including HAL temporary
  wake when the control socket is closed.
- Active output path: PASS at 48 kHz / 512 frames with active underruns `0`
  and playback failures `0` in the accepted loaded build.
- Final idle health: PASS, `coreaudiod=0.0%`, OpenA8DJ driver `0.0%`.
- Idle capture: RMS about `-68 dBFS`, peak about `-42.7 dBFS`.
- Internal diagnostic path: written, consumed, and USB-packed alignment scores
  all `1.000000`; USB check errors `0`; USB start byte `4`; USB byte order
  `big`.
- 44.1 kHz and 48 kHz playback validated locally.

Current branch roles are summarized in
[current branch status](../state/current-branch-status.md).

## Known Limits

- This release is not Apple-notarized and is not Developer ID Installer signed.
  macOS may show a warning saying Apple could not verify the package. If the
  dialog only offers `Move to Trash` and `Done`, click `Done`, then approve the
  package from System Settings -> Privacy & Security -> Open Anyway.
- Full physical DVS matrix validation still requires turntables/control vinyl:
  every input pair, vinyl mode, CD/line mode, channel order, and low-latency
  behavior.
- 88.2/96 kHz operation remains an extended validation target, not a
  production-quality claim.
- Full output-pair matrix validation beyond the currently tested route remains
  open.

## Install

Download the DMG or PKG from GitHub Releases:

```text
OpenA8DJ-0.4.0.dmg
OpenA8DJ-0.4.0.pkg
OpenA8DJ-0.4.0-checksums.txt
```

Open the DMG, run the package, and reconnect the Audio 8 DJ if it does not
appear immediately.

If Gatekeeper blocks the PKG, verify the checksum, then approve it from System
Settings -> Privacy & Security. This manual step goes away only after Developer
ID signing and Apple notarization are available.

Uninstall:

```sh
sudo /usr/local/bin/opena8dj-uninstall
```

OpenA8DJ is independent and is not affiliated with, endorsed by, sponsored by,
or certified by Native Instruments.
