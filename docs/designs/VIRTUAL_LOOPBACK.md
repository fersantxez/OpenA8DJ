# Virtual Loopback

Status: implementation contract for the first macOS release.

## Outcome and compatibility boundary

The driver will enumerate a second Core Audio device:

- name: `OpenA8DJ Loopback`;
- device UID: `org.opena8dj.Audio8DJ.loopback`;
- model UID: `org.opena8dj.Audio8DJ.loopback.model`;
- input-only, one interleaved Float32 stereo stream; and
- source selected from physical output pair `A`, `B`, `C`, or `D`.

The existing physical device remains exactly the 8-input/8-output
`Open Audio 8 DJ` device with UID `org.opena8dj.Audio8DJ`. Its object IDs,
device UID, model UID, four physical output pairs, input layout, stream layout,
default-device flags, USB lifecycle, and physical statistics are not repurposed
for loopback.

This is deliberately a second device rather than a fifth input stream on the
physical device. Adding a physical-device stream would change the established
8x8 topology seen by DAWs and compatibility-sensitive software. The plug-in
device list and plug-in-owned object list become additive two-device lists, but
queries made against physical device object `2` continue to describe only its
existing physical streams.

Proposed new stable object IDs are:

| Object | ID | Owner |
| --- | ---: | --- |
| Physical device | 2 | plug-in |
| Existing physical streams | 3-10 | physical device |
| `OpenA8DJ Loopback` device | 11 | plug-in |
| `OpenA8DJ Loopback Source Pair` input stream | 12 | loopback device |

`kAudioPlugInPropertyTranslateUIDToDevice` accepts both exact UIDs. The virtual
device reports virtual transport, is not eligible as the system or default input
device, and remains visible and alive even while disabled. Keeping it enumerated
avoids a topology change and Core Audio configuration interruption on every
privacy transition. Disabled reads return silence.

The product and identifiers remain OpenA8DJ-branded. This feature does not copy
or claim Native Instruments implementation details, firmware behavior, names,
logos, or endorsement.

## Actual tap point

The current HAL receives `kAudioServerPlugInIOOperationWriteMix` once per
physical output stream. `CopyClientOutputToOutput` assembles those post-Core
Audio mixes in the eight-channel `gOutputCycleBuffer`. `FlushOutputCycle` then
passes the assembled Float32 buffer to
`OpenA8DJUSBWriteOutputAtSampleTime`, where the USB engine schedules, converts,
and packs it for the hardware.

The loopback writer taps `gOutputCycleBuffer` inside `FlushOutputCycle`,
immediately before the physical USB write. It copies only the selected two
channels:

| Source pair | Physical buffer channels |
| --- | --- |
| A | 0, 1 |
| B | 2, 3 |
| C | 4, 5 |
| D | 6, 7 |

This is post-application-mix for the selected Core Audio output stream and
pre-USB conversion. The current HAL exposes no output gain or volume controls,
so there is no later software gain stage to tap. It is also before any analog
hardware gain. UI, CLI, API, documentation, and metrics must therefore say
`source pair`, never `master`, unless a future real master-mix stage is added.

The tap inherits the existing mixed-cycle semantics. A source pair not supplied
in a flushed physical cycle is silence because the physical cycle buffer is
zero-initialized. The loopback implementation must not alter the physical
expected-stream mask, flush decision, USB write order, or physical sample-time
argument.

## Session privacy and state transitions

Loopback is session-only and disabled by default. No enabled/source preference
is written to disk, driver host storage, defaults, a plist, or firmware.
`OpenA8DJ_Initialize` explicitly initializes:

- `enabled = false`;
- `sourcePair = A`;
- a new nonzero generation;
- an empty ring; and
- zero session counters.

A plug-in/host reload therefore returns to disabled. Normal physical USB
close/reopen during the same loaded driver session does not silently change the
privacy choice.

While disabled, the physical writer performs one acquire load of `enabled` and
returns. It performs no per-frame traversal, audio copy, ring publication,
allocation, I/O, logging, lock, dispatch, or counter update. The loopback reader
zeroes its caller-provided buffer and does not expose retained samples.

Enable, disable, source-pair change, and sample-rate reset each advance the
generation and place every reader at the current write head. Old ring slots are
invalid for the new generation. Thus re-enable cannot replay audio captured
before disable, and changing from A to B cannot leak a tail from A. A mutation
response is successful only after an authenticated set reply and an exact
getter/read-back on the same connection.

Disabling does not remove the virtual device and does not request a Core Audio
configuration change. Enabling and source selection likewise do not stop,
restart, or reconfigure either device.

## Real-time data path

Implement the data path as a small portable C11 module, for example
`OpenA8DJVirtualLoopback.{h,c}`, linked into the HAL and directly exercised by
offline tests. Its fixed storage is allocated statically at process load.

### Ring

Use a bounded stereo frame ring with power-of-two capacity (recommended 32768
frames) and a monotonic 64-bit write sequence. Each slot contains:

- atomic left and right Float32 bit patterns; and
- an atomic 64-bit published sequence token.

The writer stores the two sample bit patterns and then release-publishes the
slot token. After a batch it release-publishes the new head. The reader
acquire-loads and validates the slot token before and after atomically loading
the sample bit patterns. A token mismatch produces silence and a gap count.
Atomic sample bit patterns avoid the C data race that a plain seqlock around
non-atomic Float32 storage would retain during overwrite.

The physical writer is single-producer. Readers are multi-consumer and never
modify writer state. There is no backpressure: if `head - cursor` exceeds ring
capacity, only that reader is advanced to the oldest retained frame. Its
overrun/gap counters increase and any unvalidated requested frames are silence.
Playback continues without waiting.

The writer hot path has no mutex, allocation, file/socket/device I/O, Objective-C
message, logging, wait, or retry loop dependent on a reader. Its work is bounded
by the physical cycle frame count. The reader hot path has the same constraints.
Counter updates are relaxed atomics and must not feed the USB quality,
physical-xrun, Timecode failure, or physical output-underrun paths.

### Independent clients

Maintain a fixed table of at least 32 loopback client records keyed by Core
Audio `inClientID`. Register and unregister records in loopback-device
`StartIO`/`StopIO`; no allocation is permitted. Each record has an atomic
generation and next-frame cursor. A new client starts at the current head and
does not receive historical audio.

A read reserves its available source range using a compare/exchange on only
that client's cursor. Clients therefore do not consume one another's audio.
Two clients running concurrently receive the same newly published source,
subject only to their independent callback cadence and lag. Repeated callbacks
for one client reserve disjoint ranges. If a reader has no available physical
playback frames, it returns silence without advancing ahead of the writer.

The implementation may count an entire lost range as overrun and fill the
current request from the oldest still-valid frames, or fill that request
entirely with silence before resuming. Whichever rule is chosen must be fixed,
documented in the module header, and covered by exact offline tests. It may
never replay overwritten or wrong-generation samples.

### Start and stop separation

Keep separate counts and clock state for physical and loopback clients:

- physical `StartIO`/`StopIO` retain their current USB start/stop behavior;
- loopback `StartIO` only registers a virtual reader and starts the virtual
  host-time clock on the first virtual client;
- loopback `StopIO` only unregisters that reader and stops the virtual clock
  after the final virtual client; and
- loopback-only operation must never call `OpenA8DJUSBStart`,
  `OpenA8DJUSBEnsureOpen`, `OpenA8DJUSBStop`, or `OpenA8DJUSBClose`.

When physical playback is stopped, has never started, fails to start, or has not
published a fresh generation, loopback reads are silence. Starting or stopping
the virtual device must not change `gRunningClients`, `gDevicePresent`, the USB
clock anchor, the physical output timeline, or physical stream statistics.

## Clock, timestamps, and configuration

The loopback device has an independent host-time clock and zero-timestamp
anchor, using the same nominal sample rate as the physical source. It must not
call or consume the physical `GetZeroTimeStamp` state and must not use the USB
anchor as its reader cursor.

On first loopback `StartIO`, initialize virtual sample time to zero, host time to
`mach_absolute_time()`, and advance the virtual seed. Loopback
`GetZeroTimeStamp` advances by complete virtual zero-timestamp periods using the
existing `HostTicksPerFrame` calculation and stays monotonic while the virtual
device runs. Multiple virtual clients share this device clock but retain
independent audio cursors.

`BeginIOOperation` and `DoIOOperation` accept only `ReadInput` for loopback
device/stream object 11/12. The supplied frame count is bounded by the loopback
maximum and never changes or consumes the physical IO cycle counter. The
physical path continues to accept its existing `ReadInput` and `WriteMix`
operations.

Both devices advertise 44.1, 48, 88.2, and 96 kHz. There is no resampler.
Nominal sample rate is therefore shared. A nominal-rate request through either
device uses the existing host configuration-change mechanism and existing
driver-mode safety policy. Applying it resets the loopback generation/ring and
both clock seeds, and notifies rate/format properties on both devices. A failed
or aborted change leaves the old shared rate and both effective clocks intact.

The loopback buffer frame size is independent (default 512, bounded 64-4096)
and a loopback buffer-size configuration request targets only virtual device
11. Applying it resets only the virtual clock seed/read generation and never
changes the physical buffer size or USB engine. Physical buffer changes retain
their current normalization policy and also invalidate loopback content so a
reader cannot bridge incompatible cycle geometry.

Enable, disable, and source-pair changes are state changes, not device
configuration changes, and must remain click-free for the physical device.

## HAL property matrix

The loopback device/stream implement the same required object-property contract
as the physical objects, specialized as follows:

| Property | Loopback value |
| --- | --- |
| name | `OpenA8DJ Loopback` |
| manufacturer | `OpenA8DJ` |
| device/model UID | exact UIDs above |
| transport | virtual |
| alive/hidden | 1 / 0 |
| running | virtual client count only |
| can be default/system | 0 for every scope |
| streams, input | object 12 |
| streams, output | empty |
| stream configuration, input/global | one buffer, two channels |
| stream configuration, output | zero buffers |
| stream direction | input |
| starting channel | 1 |
| format | interleaved native-endian packed Float32 stereo |
| sample rates | shared 44.1/48/88.2/96 kHz |
| buffer | independent virtual buffer state |
| latency/safety offset | truthful fixed values, initially 0 |
| controls | empty |

The physical device's related-device result should remain its existing
single-self result for compatibility. The loopback device likewise returns
itself. Do not use related-device or clock-domain coupling as a substitute for
an aggregate device.

Object classification, owner, property-size, property-data, settable, stream
direction/index, `WillDoIOOperation`, configuration-change, and start/stop
dispatch must all branch on the exact device/stream object, not merely on
whether an object is "a device" or "an input stream".

## Private IPC and public API

Extend the existing authenticated bounded private protocol; do not add a JSON
parser to the HAL. Reserve append-only message types after the current highest
type:

- 21: loopback get;
- 22: loopback set;
- 23: loopback state.

The packed v1 set request contains `schemaVersion`, exact boolean `enabled`, and
an exact source-pair enum 0-3. Disable retains the selected pair for observable
read-back but invalidates all buffered audio. Reject wrong length, schema,
boolean values, reserved bytes, or pair values without changing state. The
state response includes:

- schema version;
- `enabled`;
- source pair;
- `sessionOnly = true`;
- physical playback publishing status;
- ring capacity and generation;
- registered reader count;
- source frames published;
- frames delivered;
- silence frames;
- gap frames;
- overrun events; and
- overrun frames.

All metrics are monotonic for the loaded session and reset on HAL reload.
Disable does not reset observability counters, but does advance the content
generation. Loopback counters are separate from the physical stream/xrun/USB
quality counters.

The public process-JSON API remains schema v1 and gains backward-compatible
operations:

```text
opena8dj-control api loopback get
opena8dj-control api loopback enable A
opena8dj-control api loopback disable
```

Use operation names `loopback.get`, `loopback.enable`, and
`loopback.disable`. The version response adds `loopback.read` and
`loopback.write` capabilities; the semantic API version advances from 1.0 to
1.1 while the response schema remains
`org.opena8dj.public-api.response.v1`.

Accepted source text is the exact allowlist `A`, `B`, `C`, `D`; no numeric,
case-folded, free-form channel, UID, path, or raw payload is accepted. Public
errors add `loopback_source_not_allowed` and `loopback_apply_failed`; private
version/length/set-get disagreement maps to the existing
`backend_protocol_error`.

Mutations take the existing public mutation lock, connect through the existing
socket type/owner/server-peer checks, perform set, then getter/read-back on the
same authenticated connection. A successful JSON response includes the full
state and exact user-facing `sourcePair`. `api stats` append-adds a `loopback`
member containing the same state/metrics when the HAL tail is available and
uses `null` for an older HAL. Old stream-stat prefixes and units remain byte for
byte compatible.

## Driver-mode coexistence

Loopback is orthogonal to the driver-mode state machine:

- Balanced and Performance change USB buffering policy, not loopback identity,
  generation semantics, source pair, or reader cursor behavior.
- Timecode Optimized observes physical input evidence. Loopback observes only a
  selected physical output mix, does not count as an input pair, and cannot arm,
  qualify, deoptimize, or fail-open Timecode.
- Vintage Compatible continues to describe the physical device only. Loopback
  objects are excluded from the Vintage physical 8x8 runtime descriptor and do
  not improve its experimental conformance status.
- Requesting or using loopback never changes the requested/effective driver
  mode. All combinations are allowed.

Mode transitions may alter the cadence at which physical cycles arrive. The
ring absorbs that producer cadence without backpressure. A loopback gap or
overrun never increments physical xruns or triggers a mode transition. Physical
sample-rate or buffer configuration transitions invalidate loopback content as
described above.

## Implementation sequence and commit boundaries

Use atomic commits so a reviewer can validate each layer:

1. portable ring/state module plus exhaustive C11 tests;
2. additive HAL objects, property dispatch, virtual clock, reader lifecycle,
   physical flush tap, and HAL smoke/parity updates;
3. private IPC state/get/set and append-only stream-stat tail;
4. public API/CLI, JSON, read-back/auth validation, and mock-backend tests;
5. Makefile targets and final compatibility/conformance fixtures.

Do not install or reload the driver, use `sudo`, reset USB/Core Audio, play or
capture audio, launch OBS, or claim live interoperability as part of offline
implementation.

## Required tests

### Portable/offline state tests

- ring wrap at and beyond capacity with exact stereo samples;
- monotonic sequence and generation changes for initialize, enable, disable,
  source change, rate reset, and buffer reset;
- reader lag and overwrite, including exact overrun/gap/silence counters;
- publish/read races under Thread Sanitizer when available;
- disabled writer publishes/copies zero frames and reader returns all silence;
- no stale pre-disable or old-pair audio after re-enable/source change;
- A/B/C/D mapping from distinct eight-channel fixtures;
- two independent clients see the same new source and do not consume each
  other; repeated callbacks for one client reserve disjoint ranges;
- no-producer, stopped-physical, partial availability, mixed frame sizes,
  ring wrap, and writer-overwrite-during-read semantics;
- invalid/missing client IDs fail closed to silence;
- counter saturation/64-bit wrap policy does not expose stale slots.

### HAL contract tests

- plug-in enumerates exactly the physical and loopback devices and translates
  both UIDs;
- physical object ID, UID, 8x8 channels, stream IDs/layout, formats, flags,
  buffer normalization, and property sizes remain unchanged;
- loopback is one stereo input stream, zero outputs, virtual, visible,
  non-default, and uses only object IDs 11/12;
- loopback `StartIO`/`StopIO` succeeds without invoking USB lifecycle functions
  and does not change physical running/alive state;
- loopback `WillDoIOOperation` accepts only `ReadInput`; physical operation
  support is unchanged;
- loopback-only read before physical playback is exact silence;
- physical mixed-cycle publication preserves the physical USB write and selected
  pair content;
- virtual zero timestamps are monotonic, seed on restart/reset, and independent
  of physical/USB timeline queries;
- shared sample-rate apply/abort and independent loopback buffer changes have
  the documented reset and notification behavior;
- enable/disable/source mutation does not request a device configuration change;
- all modes coexist without changing physical stats/xruns or physical topology.

Update `hal-smoke` and `hal-parity-smoke` to find devices by exact UID instead
of assuming list index zero, retain every current physical assertion, require
the new loopback assertions, and fail on unexpected objects. Do not weaken or
delete the existing physical 8-channel and default-device checks.

### API/IPC tests

- version 1.1 and additive capabilities while all prior fields/operations stay
  compatible;
- get, enable A/B/C/D, disable, status/metrics, JSON escaping, and exact types;
- reject lowercase/numeric/oversized/hostile source values before connecting;
- private wrong schema, length, reserved/boolean/pair values, truncation,
  unexpected message type, and set/get mismatch;
- authenticated same-connection set/read-back and existing UID peer policy;
- older HAL behavior: loopback stats `null`, mutation protocol error rather
  than fabricated success;
- public mutation lock covers loopback writes;
- state resets disabled on simulated HAL reload and never persists.

Provide a pure `make virtual-loopback-offline-test` target for tests that cannot
touch Core Audio, USB, installation, playback, or capture.

HAL bundle smoke/parity can initialize code that pre-opens USB in the shipping
configuration. Every such execution, and every future Core Audio, USB, install,
playback, capture, OBS, or live recording check, must run as:

```text
./scripts/shared-hardware-lock-run \
  --gate virtual-loopback \
  --run-dir <unique-run-directory> \
  -- <command>
```

Do not delete, bypass, or reset shared locks.

## Acceptance and honest limitations

The feature is implementation-complete when the portable, HAL, IPC, public API,
legacy compatibility, and locked smoke suites pass; the tree is clean; and
review confirms no lock/allocation/I/O in either loopback hot path and no USB
lifecycle call in loopback start/stop.

Offline success proves topology, state, samples, counters, timestamps, API
contract, privacy reset, and noninterference by fixtures. It does not prove
recording application, streaming application, aggregate-device, long-duration
drift, or OBS behavior. Those require a separately authorized live matrix under
the shared hardware lock and must not be claimed until executed.

