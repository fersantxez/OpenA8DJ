# OpenA8DJ Testing

## Build

```sh
make
```

This builds:

- `build/OpenA8DJ.driver`
- `build/opena8dj-probe`
- `build/audio-list`
- `build/audio-inspect`
- `build/audio-io-test`
- `build/audio-default`
- `build/audio-pair-tone`
- `build/audio-route`
- `build/macbook-mic-record`
- `build/opena8dj-usb-play`
- `build/opena8dj-control`
- `build/opena8dj-midid`
- `build/midi-list`

## Package

```sh
make package
sudo installer -pkg build/OpenA8DJ-<version>.pkg -target /
```

The package installs the HAL bundle, the control/MIDI tools, a user LaunchAgent
in `/Library/LaunchAgents`, and an uninstall helper at:

```sh
/usr/local/bin/opena8dj-uninstall
```

The package is unsigned unless a valid Developer ID Installer identity is passed
with `PKG_SIGN_IDENTITY`. The local test package has been installed with
`installer` and verified on macOS 26.5.

## Installed HAL Driver

The current HAL bundle is installed at:

```sh
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
```

After install/restart, device enumeration should show:

```text
Open Audio 8 DJ  uid=org.opena8dj.Audio8DJ  in=8 out=8 rate=48000
```

Local validation on 2026-06-08:

```text
Dispositivos Core Audio: 3
  Open Audio 8 DJ  uid=org.opena8dj.Audio8DJ  in=8 out=8 rate=48000
```

Detailed channel inspection should show 8 input and 8 output channels, exposed
as four stereo streams in each direction, with A/B/C/D left/right names:

```sh
./build/audio-inspect
```

Expected output shape:

```text
input buffers: 4 [2 channels] [2 channels] [2 channels] [2 channels] total=8
output buffers: 4 [2 channels] [2 channels] [2 channels] [2 channels] total=8
output channel names: 1=Output A Left ... 8=Output D Right
```

## Core Audio I/O Matrix

The Core Audio I/O path can be tested at each advertised HAL sample rate:

```sh
./build/audio-io-test 2 44100
./build/audio-io-test 2 48000
./build/audio-io-test 2 88200
./build/audio-io-test 2 96000
```

Local Traktor validation confirmed clean playback at 44.1 and 48 kHz on
2026-06-08. The HAL still exposes 88.2 and 96 kHz because the hardware accepts
those `AUDIO_PARAMS` values, but they should be treated as probe-level support
until they receive the same listening validation.

Pair routing can be tested with:

```sh
./build/audio-pair-tone A 5 440 0.06
./build/audio-pair-tone B 5 660 0.06
./build/audio-pair-tone C 5 880 0.06
./build/audio-pair-tone D 5 1100 0.06
```

Local Traktor validation on 2026-06-08 confirmed that Output A and Output B
route independently at 44.1 and 48 kHz. Output C/D are exposed and expected to
follow the same mapping, but still need the physical mixer pass before being
marked complete.

## Buffer Size

Core Audio exposes both the modern frame-based buffer properties and the older
byte-based properties used by some legacy audio applications:

```text
buffer-range=15-4096
buffer-byte-range=120-32768
```

Local validation confirmed successful I/O at 48 kHz for 64, 128, 256, 512,
1024, 2048, and 4096 frame buffers. The deprecated byte-size path was also
validated from 512 to 32768 bytes, which maps to the same stereo-frame sizes.
Traktor buffer-size selection was re-tested after the 0.2.4 buffer-property fix
and no longer depends on the invalid sentinel value seen in earlier builds.

## USB Transport Probe

Non-invasive:

```sh
./build/opena8dj-probe
```

Active USB/device-info:

```sh
./build/opena8dj-probe --claim
```

Isochronous transport:

```sh
./build/opena8dj-probe --iso-test
```

Validated protocol facts:

```text
USB ID: 17cc:1978
EP1 bulk OUT/IN: 0x01 / 0x81
Isochronous capture/playback: 0x82 / 0x06
Firmware: 14
Analog audio in/out: 8 / 8
MIDI in/out: 1 / 1
Data alignment: 2
AUDIO_PARAMS 48 kHz: reply 09 01, bpp=352
```

44.1, 48, 88.2, and 96 kHz have all accepted `AUDIO_PARAMS` in probe tests.

The HAL transport now uses an asynchronous isochronous queue. A healthy trace
shows no queue failures, no failed transactions, and output frame consumption
close to the active sample rate.

## MIDI And Controls

The package installs `opena8dj-midid` as a LaunchAgent. It creates CoreMIDI
endpoints:

```text
Open Audio 8 DJ MIDI In
Open Audio 8 DJ MIDI Out
```

Validate with:

```sh
./build/midi-list
/usr/local/bin/opena8dj-control
launchctl print gui/$(id -u)/org.opena8dj.midid
```

The control tool exposes:

- `input-mode`
- `gnd-vinyl`
- `gnd-cd-line`
- `gnd-phono`
- `software-lock`

The input mode mapping is:

```text
0 -> timecode-vinyl
1 -> timecode-cd-line
2 -> phono
```

For Traktor timecode vinyl testing, apply:

```sh
/usr/local/bin/opena8dj-control profile timecode-vinyl
```

The profile sets `input-mode` to `0`, enables the timecode vinyl ground-lift
flag, and enables `software-lock` so the front-panel input switch cannot
accidentally move the hardware out of DVS mode during a test.

Initial Traktor operator validation for Timecode Vinyl passed after the 0.2.4
buffer-property fix. Keep the full physical input-pair and CD/line matrix as a
release regression gate.

## Distribution Limitation

Core Audio enumeration, 8-in/8-out I/O at 44.1/48 kHz, MIDI endpoint
publication, control read/write, buffer-size negotiation, initial Timecode
Vinyl operation, and package install have been validated locally. Public
distribution still requires a Developer ID Installer certificate and Apple
notarization. Treat 88.2/96 kHz as extended rates until they pass the same
release matrix.
