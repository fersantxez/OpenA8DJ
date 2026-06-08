# Architecture

OpenA8DJ is a user-space macOS driver stack for the Native Instruments Audio 8
DJ USB audio interface. This project is an independent implementation based on
public APIs, public USB descriptors, public hardware specifications, and live
testing against lawfully owned hardware.

## Components

```text
Core Audio clients
      |
      v
OpenA8DJ HAL plug-in
      |
      +-- IOUSBHost transport
      |     - EP1 CAIAQ command channel
      |     - isochronous capture endpoint 0x82
      |     - isochronous playback endpoint 0x06
      |
      +-- local IPC socket /tmp/opena8dj-control.sock
            |
            v
       opena8dj-midid LaunchAgent
            |
            v
       CoreMIDI endpoints and control tool
```

## HAL driver

The HAL bundle lives at:

```text
/Library/Audio/Plug-Ins/HAL/OpenA8DJ.driver
```

It publishes one Core Audio device:

```text
Open Audio 8 DJ
uid: org.opena8dj.Audio8DJ
```

The device exposes:

- 4 stereo input streams, mapped as Input A/B/C/D
- 4 stereo output streams, mapped as Output A/B/C/D
- 44.1, 48, 88.2, and 96 kHz sample rates
- `Float32` interleaved Core Audio buffers per stereo stream
- discrete channel labels and A/B/C/D left/right channel names

Internally, the USB transport converts between Core Audio `Float32` samples and
the Audio 8 DJ 24-bit big-endian CAIAQ audio stream.

## USB protocol

The hardware uses Native Instruments/CAIAQ vendor-specific USB protocol:

- vendor/product: `17cc:1978`
- bulk control OUT/IN: `0x01` / `0x81`
- isochronous capture/playback: `0x82` / `0x06`
- commands used: `GET_DEVICE_INFO`, `READ_IO`, `WRITE_IO`, `MIDI_READ`,
  `MIDI_WRITE`, `AUDIO_PARAMS`, `AUTO_MSG`

The low-level transport is implemented with `IOUSBHost` and runs in user space.
The audio transport keeps a queue of asynchronous isochronous capture requests
in flight and mirrors the completed input packet layout for playback packets.
This implementation is written for macOS and must remain independent of GPL or
proprietary implementation code.

## MIDI and controls

`opena8dj-midid` runs as a user LaunchAgent from `/Library/LaunchAgents`. It
creates CoreMIDI endpoints and bridges MIDI/control messages to the HAL through
a local socket.

`opena8dj-control` reads and writes Audio 8 DJ-specific settings:

- input mode
- ground lift for timecode vinyl
- ground lift for timecode CD/line
- ground lift for phono
- software lock

## Packaging

The release package installs:

- HAL bundle
- MIDI bridge
- control tool
- uninstall helper
- LaunchAgent

The DMG is a distribution container around the PKG installer.
