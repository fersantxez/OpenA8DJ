# Native Instruments Audio 8 DJ Hardware Dossier

This is the working hardware map for OpenA8DJ development. It combines the
Audio 8 DJ manual, local driver-package documentation, and behavior already
validated in this repository.

## Source Baseline

- Local NI driver documentation:
  `local-analysis/audio8dj-2.8.0/payload-files/Applications/Native Instruments/Audio 8 DJ Driver/Documentation/Audio 8 DJ Manual English.pdf`
- Existing project map: `docs/AUDIO8DJ_CONNECTORS_AND_CAPTURE.md`
- Existing timecode map: `docs/TRAKTOR_TIMECODE.md`
- Current OpenA8DJ channel names: `src/hal/OpenA8DJHAL.c`
- Current OpenA8DJ USB identity and endpoints: `src/hal/OpenA8DJUSB.m`
- Public manual mirror used only as a readable cross-check:
  `https://www.manualslib.com/manual/1247231/Native-Instruments-Audio-8-Dj.html`
- Public Traktor Scratch Pro manual snippet used to cross-check multicore
  cabling:
  `https://imagescdn.juno.co.uk/manual/354445-01U.pdf`

## Device Identity

- Product: Native Instruments Audio 8 DJ.
- USB vendor ID: `0x17cc` / decimal `6092`.
- USB product ID: `0x1978` / decimal `6520`.
- USB class: vendor-specific, not USB Audio Class compliant.
- Computer link: USB 2.0, bus-powered.
- OpenA8DJ Core Audio device name: `Open Audio 8 DJ`.
- OpenA8DJ Core Audio UID: `org.opena8dj.Audio8DJ`.
- Hardware channel surface: 8 analog inputs, 8 analog outputs, one MIDI input,
  one MIDI output.
- Logical audio layout: four stereo pairs named A, B, C, and D.

The important driver implication is that modern macOS will not expose this unit
as a class-compliant audio device. OpenA8DJ must claim the vendor-specific USB
interface, configure the hardware, and publish Core Audio objects itself.

## Signal Topology

The Audio 8 DJ is a DJ interface, not a generic studio interface with eight
identical front-panel inputs. Channels A and B are special because they can
handle phono and timecode turntable workflows. Channels C and D are line-level
auxiliary/record paths.

| Pair | Channels | Input role | Output role | Level behavior |
| --- | --- | --- | --- | --- |
| A | `1|2` | Deck A input, timecode vinyl/CD-line/phono capable | Deck A output | Phono-capable input |
| B | `3|4` | Deck B input, timecode vinyl/CD-line/phono capable | Deck B output | Phono-capable input |
| C | `5|6` | Line input or front-panel mic source | Aux/send-effects output | Line input unless MIC selected |
| D | `7|8` | Line input, often recording/mixer second output | Aux/send-effects output | Line input |

Practical rule: normal phono-level turntables belong on A or B. C and D are line
inputs, so feeding a phono cartridge into C/D will be the wrong level and EQ.
Line-level turntables or CD players can use CD/line mode on A/B or line inputs
on C/D.

## Rear Panel

- `IN 1|2 - OUT 1|2 (CH A)`: Deck A RCA input/output cluster. This is the main
  DVS path for Deck A and is used by the color-matched Traktor multicore cable.
- `IN 3|4 - OUT 3|4 (CH B)`: Deck B RCA input/output cluster. Same role for
  Deck B.
- `OUT 5|6 (CH C)`: stereo output pair C, normally an auxiliary mixer input or
  send-effects return path.
- `OUT 7|8 (CH D)`: stereo output pair D, normally another auxiliary mixer input,
  send-effects return, or alternate playback pair.
- `USB`: USB 2.0 connection to the computer.
- `USB Security Hook`: strain relief for the USB cable. Useful live because a
  disconnect drops the whole audio and MIDI surface.
- `MIDI IN / MIDI OUT`: 5-pin DIN MIDI only, not audio.
- `GROUND`: optional turntable ground point. Use it when grounding the turntables
  at the mixer gives poor noise results.

The rear A/B connectors are RCA audio jacks grouped for the multicore workflow;
do not treat them as XLR microphone connectors. The front-panel mic connector is
the XLR connector.

## Front Panel

- `MIC`: XLR microphone input.
- `IN 5|6 (CH C)`: line input C, documented for mixer effects output.
- `MIC GAIN`: analog gain control for the microphone input.
- `MIC/LINE`: chooses whether Channel C input comes from the XLR mic path or the
  line input path.
- `IN 7|8 (CH D)`: line input D, documented for second mixer output or recording.
- `INPUT MODE`: front-panel selector for Channels A and B. The modes are
  timecode vinyl, timecode CD/line, and phono.
- `HEADPHONES`: headphone output.
- `SOURCE`: headphone source selector. The manual describes toggling between
  `IN 5|6` and `OUT 7|8`.
- `Volume`: headphone volume control.

The XLR microphone input does not provide phantom power. Condenser microphones
need external power.

## LEDs And State Feedback

- Channel A-D LEDs show input/output activity and turn red on clipping.
- USB LED indicates a live computer connection.
- MIDI LED indicates MIDI-device activity/connection.
- Input-level LEDs show level and red clipping.
- Source LEDs show the selected MIC/LINE or monitor source.
- Input-mode LEDs identify the selected A/B mode. The manual maps the colored
  modes as timecode vinyl, timecode CD/line, and phono; older units may use
  green LEDs only.
- Headphone level LED shows headphone volume level.

For driver work, the LEDs matter because control writes should visibly match the
state that `opena8dj-control` reports. A control profile that reports success but
does not update the expected hardware state is not a full pass.

## Hardware Controls Exposed By OpenA8DJ

OpenA8DJ exposes the current hardware control state through
`opena8dj-control`:

```text
input-mode:        0 (timecode-vinyl)
gnd-vinyl:         on|off
gnd-cd-line:       on|off
gnd-phono:         on|off
software-lock:     on|off
input-decode:      on|off
```

Input-mode mapping:

```text
0 -> timecode-vinyl
1 -> timecode-cd-line
2 -> phono
```

Profiles:

```text
opena8dj-control profile playback
opena8dj-control profile timecode-vinyl
opena8dj-control profile timecode-cd-line
opena8dj-control profile phono
opena8dj-control profile unlock
```

The timecode and phono profiles enable input decoding. Playback-oriented
profiles disable input decoding to keep CPU and audio-output risk lower when
inputs are not needed.

`software-lock` disables front-panel `INPUT MODE` changes. Use it live or during
tests when accidental mode switching would invalidate results.

Ground-lift flags separate the screens of connected audio cables from chassis
ground for the selected path. Default is off in the original tool behavior, which
means the cable screen is connected to chassis ground.

## Technical Specifications

Known/manual-level specifications:

- A/D channels: 8.
- D/A channels: 8.
- Sample rates: 44.1, 48, and 96 kHz in the manual. OpenA8DJ currently exposes
  44.1, 48, 88.2, and 96 kHz, with 44.1/48 kHz as the locally validated focus.
- Bit resolution: 24-bit.
- Converter family: Cirrus Logic.
- MIDI: one input, one output.
- Dimensions: approximately `45 x 174 x 103 mm`.
- Weight: approximately `825 g`.
- Input full-scale levels in the manual:
  - Line: about `11.9 dBu`.
  - Microphone: about `-40 dBu`.
  - Phono: about `-29 dBu`.
- Output maximum line level: about `+9.7 dBu`.
- Headphone output: about `1.7 Veff @ 100 Ohms`.

Treat published electrical numbers as hardware-reference values, not proof that
our current driver is outputting clean audio. Candidate readiness still requires
physical sound-quality validation.

## Use Cases

Playback-only DJ use:

- Route Deck A to Output A `1|2`.
- Route Deck B to Output B `3|4`.
- Use Output C/D for auxiliary mixer paths, recording feeds, or extra decks if
  the app supports four stereo output pairs.
- Use `opena8dj-control profile playback` unless input/timecode is actively
  being tested.

Traktor Scratch / DVS:

- Connect turntables or CD players through A/B multicore paths.
- For vinyl control, use `opena8dj-control profile timecode-vinyl`.
- For CD/line control, use `opena8dj-control profile timecode-cd-line`.
- In Traktor, map Deck A timecode input to Input A L/R and Deck B timecode input
  to Input B L/R.
- Validate timecode scope, input isolation, output isolation, speed, and
  low-latency behavior at 44.1 and 48 kHz before treating it as ready.

Normal vinyl recording:

- Use A or B in `phono` mode for phono-level turntables.
- Do not use C/D for phono cartridges.
- With the original NI driver, the manual describes selecting Audio 8 DJ as a
  recording device. With OpenA8DJ, do not assume physical input capture is
  release-ready unless current input validation says so.

CD player or line source:

- Use A/B in timecode CD/line mode, or C/D as regular line inputs.
- For mixed CD-player plus phono setup, leave the turntable on A or B in phono
  mode and put the CD/line source on C/D.

Microphone:

- Connect XLR mic to `MIC`.
- Select MIC on `MIC/LINE`.
- Adjust `MIC GAIN`.
- No phantom power is available.

MIDI:

- Connect external controller MIDI OUT to Audio 8 DJ MIDI IN.
- Connect Audio 8 DJ MIDI OUT to the external device MIDI IN.
- Select the Audio 8 DJ MIDI ports in the host application.

## OpenA8DJ Transport Facts

Current transport constants from `OpenA8DJUSB.m`:

- Control out endpoint: `0x01`.
- Control in endpoint: `0x81`.
- Isochronous capture endpoint: `0x82`.
- Isochronous playback endpoint: `0x06`.
- USB interface number: `0`.
- Configuration value: `1`.
- Alternate setting: `1`.
- Output frame layout: four stereo streams, 8 channels total.
- Internal USB sample packing currently treats hardware samples as 24-bit audio
  carried in 4-byte USB slots.

This makes the driver problem more like a vendor-specific real-time USB engine
than a normal class-compliant USB audio configuration.

## Driver And QA Implications

- Do not confuse logical Core Audio pairs with physical connector safety. Output
  A/B can disturb real deck/multicore wiring; use C/D or external mixer outputs
  for non-invasive tests when possible.
- Do not feed mixer line outputs into phono inputs. That path has the wrong gain
  and RIAA treatment.
- Do not use the MacBook mic or software loopback as release-quality proof of
  Audio 8 DJ analog output. Use a separate known-good capture interface such as
  iRig Stream when available.
- A complete Traktor validation must cover A/B input modes, A/B output routing,
  C/D auxiliary routing, ground-lift behavior, software lock, MIDI visibility,
  and low-latency stability.
- A playback-only build can pass output gates while still being incomplete for
  DVS. Timecode readiness requires input quality and physical scope behavior.
- A normal candidate handoff still requires real sound-quality evidence for the
  exact loaded artifact, not just correct USB/Core Audio enumeration.
