# OpenA8DJ 0.3.25 Release Notes

Historical note: OpenA8DJ 0.4.0 is the release that promotes the modern C++
macOS driver line to `main`. These 0.3.25 notes are kept only as historical
pre-0.4.0 context.

OpenA8DJ 0.3.25 is a macOS public preview for the Native Instruments Audio 8 DJ
USB interface.

## Highlights

- Restores the Traktor/DVS-facing Core Audio topology used as historical
  context before the 0.4.0 C++ mainline promotion.
- Restores the Traktor/DVS-facing Core Audio topology from the last
  timecode-capable public preview: 8 inputs / 8 outputs.
- Exposes one 8-channel input stream with named Input A/B/C/D channel pairs,
  plus 4 stereo output streams named Output A/B/C/D. This preserves the
  Traktor channel assignment surface while avoiding the multi-input-stream
  Core Audio enumeration instability found during this release.
- Re-enables Audio 8 DJ input decoding so Traktor can be assigned the physical
  timecode input pairs for validation.
- Keeps the capture-paced output transport, stable Core Audio clocking, and
  playback-quality improvements from 0.3.24.
- Keeps Core Audio `IOProcStreamUsage` disabled by default because the
  8-in/8-out build was stable without it and a stream-usage-enabled load could
  drive `coreaudiod` into an enumeration/CPU failure.
- Keeps `GetZeroTimeStamp` on a stable Core Audio timeline rather than forcing
  it to the USB clock.
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

The previous public preview with this topology was 0.2.6, where initial
Timecode Vinyl operator validation passed. Version 0.3.25 restores that Core
Audio channel surface while retaining the newer output transport.

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
- Active output path: PASS at 48 kHz / 512 frames, generator peak `0.200000`,
  driver output peak `0.100000`, active underruns `0`, playback failures `0`.
- Post-playback audio stack guard: PASS, `coreaudiod` `0.0%`, OpenA8DJ driver
  `0.0%`, AirPlayXPCHelper `0.0%`, global idle `90.29%`.
- Core Audio safety load: PASS across two load cycles with `IOProcStreamUsage`
  absent and the candidate left loaded.
- 44.1 kHz and 48 kHz playback validated locally.
- Output transport uses the same 0.3.24 physical iRig and human-listening
  improvement path; current release validation still keeps route-specific
  output-pair testing open.
- Release package installs the HAL driver, MIDI/control LaunchAgent, control
  tools, uninstall helper, and documentation.
- `opena8dj-control profile timecode-vinyl` wakes the HAL temporarily when the
  control socket is closed, applies the hardware profile, and releases Core
  Audio again instead of relying on a permanent background keepalive.

## Known Limits

- This release is not Apple-notarized and is not Developer ID Installer signed.
  macOS may require approving the installer manually.
- Full physical DVS matrix validation still requires turntables/control vinyl:
  every input pair, vinyl mode, CD/line mode, channel order, and low-latency
  behavior.
- 88.2/96 kHz operation remains an extended validation target, not a
  production-quality claim.
- Full output-pair matrix validation beyond the currently tested path remains
  open.

## Install

Download the DMG or PKG from GitHub Releases:

```text
OpenA8DJ-0.3.25.dmg
OpenA8DJ-0.3.25.pkg
OpenA8DJ-0.3.25-checksums.txt
```

Open the DMG, run the package, and reconnect the Audio 8 DJ if it does not
appear immediately.

Uninstall:

```sh
sudo /usr/local/bin/opena8dj-uninstall
```

OpenA8DJ is independent and is not affiliated with, endorsed by, or sponsored by
Native Instruments.
