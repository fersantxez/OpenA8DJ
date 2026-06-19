# Real-Time Audio Data Plane Design

This document defines the C++ audio data plane target for OpenA8DJ. It is a
design contract, not an implementation patch. The goal is to keep the Core
Audio callback and USB isochronous streaming paths bounded, allocation-free, and
observable without doing unsafe work in the real-time path.

## Scope

The data plane covers:

- Core Audio input/output callback handoff.
- Float32 8-channel interleaved host buffers.
- 24-bit CAIAQ USB input decode and output pack.
- Isochronous capture/playback transfer recycling.
- Clock anchoring, jitter measurement, and stream health counters.

It does not cover installer behavior, MIDI/control IPC, UI, packaging, hardware
probing, or policy decisions about advertised sample rates.

## Real-Time Contract

The callback and isochronous completion paths must not perform:

- `malloc`, `calloc`, `realloc`, `free`, Objective-C allocation, C++ heap
  allocation, autorelease pool churn, or container growth.
- Blocking locks, condition variables, synchronous dispatch, file I/O, socket
  I/O, logging, `printf`, `fprintf`, `NSLog`, `os_log`, or diagnostics that can
  allocate internally.
- Device discovery, USB pipe open/close, sample-rate reconfiguration, IPC, MIDI,
  or property handling.
- Unbounded scans, per-sample dynamic branching that is not tied to fixed channel
  count, or loops whose upper bound is not derived from the negotiated period,
  transfer size, or preallocated ring capacity.

Allowed operations are fixed-bound copies/conversions, atomic loads/stores,
single-producer/single-consumer ring operations, timestamp arithmetic, and
counter increments. Failures are recorded into preallocated counters; detailed
text is emitted later by a non-real-time diagnostics/control thread.

## Thread Model

```text
Core Audio callback thread
  - consumes capture ring into client buffers
  - writes client output into the output timeline ring
  - never touches USB objects, locks, heap, files, sockets, or logs

USB stream queue thread
  - recycles preallocated isochronous transfers
  - decodes capture bytes into the capture ring
  - reads output timeline frames and packs playback transfers
  - records raw timing/counter data only

Control/diagnostic thread
  - starts/stops/reconfigures the engine outside the callback
  - snapshots counters through a seqlock or atomic copy protocol
  - formats logs, IPC replies, and diagnostic files outside real-time paths
```

Ownership must be explicit: Core Audio owns host callback buffers only for the
duration of the callback, USB owns transfer memory only while it is queued, and
the engine owns all rings and transfer pools from stream start until stream stop.

## Preallocated Engine State

All allocations happen before the stream is marked running. Stream start builds a
single `RealtimeEngine` object with cache-aligned storage:

- `CaptureAudioRing`: SPSC ring of decoded Float32 frames, producer USB capture,
  consumer Core Audio input.
- `OutputTimelineRing`: SPSC timeline ring of Float32 frames indexed by Core
  Audio sample time, producer Core Audio output, consumer USB playback.
- `IsoTransferPool`: fixed arrays for capture and playback transfers, including
  USB transaction descriptors and payload bytes.
- `ScratchBuffers`: fixed per-thread blocks for deinterleave/interleave, output
  pack staging, capture decode staging, and silence frames.
- `RealtimeCounters`: atomics or per-thread counters for timing, underrun,
  overrun, queue, and format anomalies.

Capacity is chosen at stream start from the negotiated sample rate, Core Audio
period, channel count, and USB transfer geometry. No capacity changes are
allowed while running; a sample-rate or period change stops the engine, builds a
new preallocated state, and then starts again.

Recommended initial capacities:

| Buffer | Initial capacity | Reason |
| --- | ---: | --- |
| Capture ring | 32768 frames | Existing baseline capacity; enough for transient USB/callback phase jitter. |
| Output timeline | 32768 frames | Matches capture ring and supports elastic correction without reallocating. |
| Capture transfers | 64 transfers x 8 USB microframes | Existing capture queue depth and USB cadence. |
| Playback transfers | 128 transfers x 8 USB microframes | Enough for target plus maximum in-flight playback lead. |
| Callback cycle buffer | 4096 frames x 8 channels | Matches current advertised maximum buffer frame size. |
| USB pack/decode scratch | max packet bytes x transfer microframes | Fixed by CAIAQ bytes-per-packet at selected rate. |

If later measurements prove these values too high or low, change them only as
start-time policy constants with acceptance tests. Do not resize dynamically in
the stream path.

## Ring Buffer Design

Use SPSC rings rather than mutex-protected rings.

### CaptureAudioRing

Producer: USB capture completion after CAIAQ decode.

Consumer: Core Audio input callback.

Layout:

```text
alignas(64) atomic<uint64_t> write_frame;
alignas(64) atomic<uint64_t> read_frame;
alignas(64) float frames[capacity_frames][8];
```

Rules:

- Capacity is a power of two so indexing is `frame & (capacity - 1)`.
- Producer writes frame payload first, then publishes `write_frame` with release
  ordering.
- Consumer reads `write_frame` with acquire ordering, copies available frames,
  advances `read_frame` with release ordering, and zero-fills only the missing
  suffix.
- If producer would lap consumer, it drops oldest frames by advancing
  `read_frame` with an atomic compare/exchange and increments
  `capture_ring_overrun_frames`.
- If consumer needs frames that are unavailable, it zero-fills and increments
  `capture_ring_underrun_frames`.

The capture ring carries audio frames only. Input RMS/peak/correlation stats use
separate per-stream accumulators on the USB thread and are published later.

Current C++ core status:

- `SpscFrameRing<Frame, Capacity>` provides the fixed-capacity SPSC contract
  used by offline gates.
- `opena8djcpp_realtime_audit` verifies decoded-frame push/pop through the ring
  with zero hot-path allocations.
- `decode_input_profile_mode2_into` decodes Mode 2 capture bytes into
  caller-owned scratch and optional interleaved Float32 output. Playback profile
  mode keeps input decode disabled while preserving packet stats; timecode
  profiles enable decode without allocating output storage internally.

### OutputTimelineRing

Producer: Core Audio output callback.

Consumer: USB playback packer.

Layout:

```text
alignas(64) atomic<int64_t> write_max_frame;
alignas(64) atomic<int64_t> read_frame;
alignas(64) atomic<uint64_t> generation;
alignas(64) float frames[capacity_frames][8];
alignas(64) int64_t tags[capacity_frames];
```

Rules:

- Each written frame is tagged with its absolute Core Audio sample frame.
- The producer normalizes small sample-time jitter by continuing from
  `write_max_frame + 1` when the callback timestamp delta is inside the jitter
  tolerance.
- A large stale/future gap starts a new generation. The producer records
  `output_timeline_resets` and publishes the new generation atomically.
- The consumer reads by absolute `read_frame`. If the tag at the ring slot does
  not match, it serves silence or a bounded replay/fade policy and increments
  underrun counters.
- High-water correction is deterministic: if buffered frames exceed
  `target_latency + high_water_margin`, advance `read_frame` to target latency
  and increment `output_elastic_drop_frames`.

The output ring is a timeline, not a FIFO. This is important because Core Audio
can provide a valid sample time and because device-clock following must be
observable as frame-position error, not hidden as arbitrary FIFO depth.

## Cache-Friendly Layout

Keep audio payloads contiguous and aligned:

- Store host audio as interleaved `float[frame][8]` because the HAL currently
  exposes Float32 interleaved buffers and USB packing consumes all eight channels
  per device frame.
- Use `alignas(64)` for producer/consumer indices, hot counters, and frame
  arrays. Producer and consumer cursors must live on separate cache lines.
- Keep hot per-transfer state in plain structs: payload pointer, transaction
  pointer, transaction count, byte count, first USB frame number, and pool index.
- Keep cold diagnostics, IPC fields, Objective-C wrappers, file paths, and
  formatted strings out of the real-time structs.
- Avoid per-frame heap objects. Transfer completion receives a pointer/index to
  an existing transfer slot and returns it to the pool with a bounded SPSC or
  freelist operation.

Conversion should work in bounded blocks:

- Capture decode converts a USB transaction's valid bytes into one contiguous
  frame batch, then publishes the batch to `CaptureAudioRing`.
- Playback pack pulls a contiguous batch from `OutputTimelineRing`, converts to
  CAIAQ bytes, and fills the already-owned transfer payload.
- No callback or USB path should call ring read/write once per frame when a
  bulk-copy path can split at most twice around the wrap point.

## Timing Model

The engine has one stream epoch:

```text
epoch.sample_rate
epoch.start_host_time
epoch.start_sample_time
epoch.usb_frame_number
epoch.seed
```

Core Audio zero timestamp is derived from this epoch plus monotonic host time.
USB capture timestamps refine the epoch through filtered anchors, but they do
not directly force discontinuous callback timestamps.

The USB thread records:

- Capture completion host-time delta.
- Playback completion host-time delta.
- Capture USB timestamp delta.
- Capture-to-playback queue delta.
- USB frame-number scheduling lead for playback.
- Accepted/rejected clock anchors and anchor reset count.

The expected isochronous cadence is:

```text
expected_transfer_ticks = mach_ticks_per_second * iso_microframes_per_transfer / 8000
```

For each measured delta, record min, max, sum, sample count, outlier count,
outlier max, and outlier sum. Outlier threshold is a start-time constant derived
from expected cadence, for example `max(2 * expected_transfer_ticks,
expected_transfer_ticks + one_audio_period_ticks)`.

Clock-following policy:

- Treat capture USB timestamps as the best hardware cadence signal when valid
  and monotonic.
- Reject repeated, zero, out-of-order, or impossible USB timestamp anchors.
- Bound correction rate. Do not jump output read position for small drift; use
  elastic drop/replay only when accumulated error crosses explicit thresholds.
- Increment counters for every correction so listening results can be tied to
  measured behavior.

## Underrun, Overrun, and Jitter Accounting

Counters are part of the data plane API. They must be cheap to update and safe
to snapshot without stopping audio.

Minimum counters:

| Counter | Incremented by |
| --- | --- |
| `callback_cycles` | Every Core Audio cycle. |
| `callback_late_cycles` | Callback observed after expected host deadline. |
| `callback_max_duration_ticks` | Max callback duration from entry to exit. |
| `capture_ring_frames_available_min/max` | Snapshot at input read. |
| `capture_ring_underrun_frames` | Input callback zero-fill because capture data is missing. |
| `capture_ring_overrun_frames` | USB producer drops old input frames because consumer is behind. |
| `output_frames_written` | Core Audio writes frames into output timeline. |
| `output_frames_read` | USB consumes frames for playback. |
| `output_active_underrun_frames` | Playback needs non-startup audio but no matching timeline frame exists. |
| `output_startup_silence_frames` | Planned preroll silence before first timeline frame. |
| `output_elastic_drop_frames` | Consumer skips frames to reduce excessive latency. |
| `output_elastic_replay_frames` | Consumer reuses/fades previous frame to mask short gap. |
| `output_timeline_resets` | Producer starts a new timeline generation. |
| `capture_queue_failures` | Capture transfer could not be queued. |
| `playback_queue_failures` | Playback transfer could not be queued. |
| `capture_transaction_failures` | Failed capture USB transactions. |
| `playback_transaction_failures` | Failed playback USB transactions. |
| `capture_short_transfers` | Capture transaction byte count below expected. |
| `playback_short_transfers` | Playback complete count below request count. |
| `usb_timestamp_zero/repeated/out_of_order` | Invalid capture timestamp observations. |
| `clock_anchor_accepted/rejected/reset` | Clock anchor filter decisions. |
| `jitter_capture_completion_*` | Min/max/sum/count/outlier stats for capture completion cadence. |
| `jitter_playback_completion_*` | Min/max/sum/count/outlier stats for playback completion cadence. |
| `jitter_capture_to_playback_queue_*` | Delay from capture completion to playback queueing. |

Use per-thread plain counters for hot paths when possible, then publish with a
seqlock snapshot:

```text
stats.sequence++   // odd, release
copy counters
stats.sequence++   // even, release
```

Readers retry if the sequence changes or is odd. Hot single-value counters that
must be updated by both sides can be `atomic<uint64_t>` with relaxed ordering.
Audio correctness must not depend on diagnostic counter ordering.

## Transfer Pool

`IsoTransferPool` is built at stream start:

```text
struct IsoTransferSlot {
    uint8_t* payload;
    IOUSBHostIsochronousTransaction* transactions;
    uint32_t transaction_count;
    uint32_t payload_capacity;
    uint32_t slot_index;
    atomic<uint32_t> state;
};
```

State transitions are bounded:

```text
free -> filling -> queued -> completed -> free
```

The pool must not allocate in `checkout`, `queue`, completion, or release.
Failure to obtain a slot increments a queue failure counter and returns. The
audio response is silence/replay from preexisting state, never allocation or
blocking.

Playback and capture pools are separate so one direction cannot starve the
other. Queue depth is capped by constants known at start time.

## Callback Algorithm

Input callback:

1. Load capture ring cursors.
2. Copy available interleaved frames to the requested stream slice.
3. Zero-fill unavailable suffix.
4. Publish the new read cursor.
5. Increment underrun and cycle counters.

Output callback:

1. Merge active Core Audio stream buffers into the fixed 8-channel cycle buffer.
2. Determine the absolute start sample frame from callback sample time.
3. Normalize small timestamp jitter against `write_max_frame + 1`.
4. Optionally classify a whole callback buffer as active silence using a
   fixed threshold and bounded frame counter.
5. Write either the original batch or deterministic zero frames into
   `OutputTimelineRing`.
6. Publish `write_max_frame` and update counters.

The active-silence check is intentionally coarse: it only treats a whole
callback buffer below the configured threshold as silence. Normal music buffers
are not zero-floored globally, so low-level musical detail and zero crossings
stay on the ordinary path. If silence persists for the configured hold window,
the playback gate parks output transfer submission while capture and timecode
input remain active.

## USB Algorithm

Capture completion:

1. Record completion timestamp and transaction timing counters.
2. For each successful transaction with expected byte count, decode CAIAQ input
   into a fixed scratch batch.
3. Publish decoded frames to `CaptureAudioRing`.
4. Update the USB clock anchor from valid USB timestamps.
5. Requeue the same transfer slot if streaming is still active.
6. Optionally queue playback from the same cadence signal.

Playback queueing:

1. Checkout a playback transfer slot.
2. Compute desired USB frame scheduling lead if explicit scheduling is enabled.
3. Pull a bounded frame batch from `OutputTimelineRing`.
4. Apply startup silence, bounded replay/fade, or elastic drops according to the
   timeline state.
5. Pack CAIAQ output bytes into the preallocated payload.
6. Queue the transfer and record schedule/queue counters.

Playback completion:

1. Record completion timestamp and transaction status counters.
2. Update in-flight count.
3. Return the slot to the playback pool.
4. Queue more playback only on the USB queue, never from the Core Audio callback.

## Diagnostics Boundary

Diagnostics must be split into two layers:

- Real-time layer: binary counters, timestamp ranges, compact event codes, fixed
  ring snapshots if explicitly enabled before stream start.
- Non-real-time layer: text formatting, files, IPC responses, summaries, and
  user-facing logs.

A diagnostic capture mode may reserve fixed buffers at stream start. If those
buffers fill, the real-time path drops diagnostic events and increments
`diagnostic_events_dropped`; it must not allocate or block.

## Acceptance Gates

A C++ data plane implementation satisfies this design only when these checks
pass:

- Static audit of callback and USB completion paths shows no heap allocation,
  blocking locks, file/socket I/O, logging, Objective-C allocation, or container
  growth.
- Callback code performs only bounded copies/conversions, SPSC ring operations,
  timestamp arithmetic, and counter updates.
- Transfer pool checkout/release is allocation-free under sustained streaming.
- 30-minute playback at 44.1, 48, and 96 kHz records zero active output
  underruns after startup preroll.
- Capture input read under loopback records zero unplanned capture underruns
  after startup.
- Jitter counters remain bounded and explain any audible event through matching
  underrun, overrun, elastic correction, schedule fallback, or transaction
  failure counters.
- Sample-rate change tears down and rebuilds preallocated state outside the
  callback and resumes without stale packet-size frames entering playback.

Any future implementation that needs to violate this contract must document the
specific callback operation, its upper bound, why it cannot move to a
non-realtime thread, and the measurement proving it does not regress audio.

## Prepared Transport Backend Contract

The next transport architecture must split CoreAudio/HAL-facing callbacks from
USB-facing prepared slot ownership:

- HAL steady-state callback work:
  - copy/route frames to or from fixed rings;
  - update bounded counters and timestamps;
  - never issue direct USB transfer enqueue/requeue;
  - never allocate fallback transfer state.
- Transport backend work:
  - prepare capture and playback slots before streaming;
  - own steady-state completion/requeue outside the HAL callback;
  - preserve one-completion-period cadence unless a future physical candidate
    proves a different cadence beats quality and CPU gates;
  - publish ordered timestamps and channel-stable frame batches to the rings.

Offline gate:

- `opena8djcpp_driverkit_prepared_transport_contract`.
- Required safe scenarios have `hal_steady_requeues=0`, no fallback
  allocations, no timestamp regressions, no routing failures, no timecode
  profile failures, and completion gap ratio `<=1.25`.
- The gate is backed by `PreparedTransportBackend` in the pure C++ core, so the
  contract now exercises the reusable ring/counter abstraction rather than a
  tool-local sketch.

This is still weaker than physical readiness. It is the contract a real
DriverKit/USB backend must satisfy before another hardware window is justified.

## Packet Batch Boundary

The prepared transport backend now accepts and emits bounded `S24Frame` spans:

- HAL playback writes can publish a batch into the playback ring.
- Backend completion can publish a decoded capture batch and pop a playback
  batch for packet packing.
- HAL capture reads can consume a batch from the capture ring.

`opena8djcpp_prepared_transport_packet_contract` proves this path with Mode2
pack/decode bytes outside the hot path. A real backend must keep the same span
API allocation-free after stream start.

## Routing And Timecode Batch Boundary

S24 routing is now available as a bounded batch operation. The prepared
transport timecode gate exercises:

- playback routing before publishing to the backend playback ring;
- Mode2 decode into S24 capture frames;
- backend capture-ring traversal;
- profile source-map conversion to float timecode input;
- per-deck timecode analysis with zero leakage in the synthetic fixture.

No logs, allocations, or system calls are required in the modeled real-time
boundary.

## Recovery Boundary

Prepared transport recovery must be a control-plane operation, not a callback
operation:

- invalid stream configs fail closed before any prepared-slot counters are
  published;
- `stop()` prevents all HAL/backend operations and clears prepared rings;
- `start()` for a new session clears capture/playback rings, counters, and
  timestamp history;
- a backend that is not started cannot report `product_safe`;
- no stale capture or playback frames may cross from one stream session into
  the next.

Offline gate:

- `opena8djcpp_prepared_transport_recovery_contract`.
- Required result has zero invalid-config failures, zero false unstarted-safe
  failures, zero stopped-operation failures, zero stale-frame mismatches, zero
  counter-reset failures, and zero timestamp-reset failures.

This is still an offline model. A real DriverKit adapter must satisfy the same
contract before a locked hardware recovery test is requested.

## DriverKit Runtime Bridge Boundary

The offline DriverKit shell now has a concrete runtime bridge:

- configure stream outside the callback;
- validate sample rate and prepared transport sizing before stream start;
- use bounded span-based playback/capture methods for HAL-facing work;
- route backend completion through `PreparedTransportBackend`;
- stop the backend when the driver stops.

Offline gate:

- `opena8djcpp_driverkit_runtime_contract`.
- Required result has zero lifecycle/config/frame/shutdown failures, zero HAL
  steady requeues, zero fallback allocations, zero ring errors, zero timestamp
  regressions, and `running_product_safe=true` only while the stream is active.

This is not a substitute for real `IOUserAudioDriver`, `IOUserAudioDevice`, or
`IOUserAudioStream` binding. It is the measured contract those classes must
preserve.

## USB Submit Descriptor Boundary

The low-CPU path must reduce real USB submit/enqueue calls below the logical
audio cadence without changing the logical ISO8 cadence visible to routing,
timestamps, capture, playback, or timecode.

Offline gate:

- `opena8djcpp_usb_submit_plan_contract`.
- `opena8djcpp_usb_submit_payload_contract`.
- The stable row maps `528` logical capture/playback slots into `66` prepared
  submit descriptors, split `33/33` between capture and playback.
- The payload gate proves those descriptors carry `185856` bytes and `5808`
  Mode2 payload frames with zero decode/check/panic/overflow/prefix failures.
- Required result has zero partial submits, descriptor overflows, slot-order
  errors, and timestamp regressions.
- Negative rows reject unbatched submits, order errors, timestamp regressions,
  and partial flushes.

A real DriverKit/USB backend must preallocate and recycle descriptors according
to this plan. The audio callback must not allocate descriptors, repair order,
flush partial batches, perform direct submit work per logical slot, or recompute
payload layout dynamically.

## DriverKit USB Submit Binding Boundary

The offline DriverKit shell now owns a prepared USB submit planner alongside
the prepared transport backend:

- stream start configures the planner with preallocated submit slots;
- the shell prequeues the initial capture/playback slots before data flow;
- each ISO8 backend period records capture and playback slots at the runtime
  boundary;
- stream stop finishes the planner and prevents stale descriptors from carrying
  into the next stream session.

Offline gate:

- `opena8djcpp_driverkit_usb_submit_binding_contract`.
- Required result has `256` ISO8 periods, `528` logical slots, `66` USB submit
  calls/descriptors, `185856` bytes, `5808` Mode2 payload frames, zero
  transport frame mismatches, zero Mode2 check/panic/overflow failures, zero
  payload prefix mismatches, and clean stop.

This still does not submit real USB requests. It proves the runtime shell must
preserve the low-CPU submit plan before a DriverKit/USBDriverKit adapter is
allowed to become a hardware candidate.

## DriverKit USB Request Lifecycle Boundary

The USB request lifecycle model is stricter than the descriptor model:

- request slots are preallocated before stream flow;
- submit returns a generation-checked handle;
- completion must match a live handle;
- completion recycles the slot and advances its generation;
- stale handles, invalid handles, pool pressure, and fallback allocation are
  product failures.

Offline gate:

- `opena8djcpp_driverkit_usb_request_lifecycle_contract`.
- Required stable result has `66` submit calls, `66` completion calls, `66`
  recycle calls, max `4` live requests, `185856` completed bytes, `5808`
  completed frames, zero fallback allocations, zero invalid completions, zero
  stale completions, and zero live requests after drain.
- Negative rows must reject pool pressure and stale completion handles.

The future real USB adapter must preserve this lifecycle with real DriverKit or
USBDriverKit async request objects. The audio path must not allocate replacement
requests or repair lifecycle errors while audio is flowing.

## DriverKit USB Shutdown Boundary

Stream stop must not pretend that live asynchronous USB requests completed
normally. The runtime boundary has separate accounting for normal completion
and shutdown cancellation:

- stop first flushes any newly formed submit descriptors;
- live requests are cancelled explicitly;
- cancelled bytes/frames are not counted as completed bytes/frames;
- late completions after cancellation are rejected by generation checks;
- restart after cancellation must start from a clean request pool.

Offline gate:

- `opena8djcpp_driverkit_usb_request_shutdown_contract`.
- Required result has `inflight_requests_at_stop=3`, `cancelled_requests=3`,
  `live_requests_after_stop=0`, `fallback_allocations=0`,
  `invalid_completions=0`, `stale_completions=0`,
  `late_completions_after_cancel=0` for the skeleton shutdown row, plus a
  separate negative row proving late completion after cancel is rejected.
- `restart_after_cancel_safe=true` is mandatory.

This prevents a future DriverKit/USB adapter from hiding stop/restart bugs as
successful completions. It still does not prove physical USB behavior.

## Playback Lead Scheduler Boundary

The next low-CPU path must reduce playback submit work without changing capture
cadence or the logical ISO8 audio model.

Offline gate:

- `opena8djcpp_playback_scheduler_contract`.
- Required stable row keeps `capture_request_submit_calls=256`.
- Required stable row submits `264` logical playback slots through no more than
  `33` playback request submit calls.
- Required playback submit reduction ratio is at least `8`.
- Required total submit reduction ratio is greater than `1.5`.
- Playback lead must stay bounded between the configured low/high watermarks;
  current stable model observes `5..12` slots.
- Required failures are zero playback underflows, pool overflows, batch
  overflows, capture gaps, and logical cadence violations.

This is still an offline model. A real HAL or DriverKit binding must keep all
buffers and request objects preallocated, must not allocate or log in the audio
callback, and must not hide a playback gap by sacrificing capture continuity.

## Playback Scheduler Runtime Boundary

The runtime binding model turns the scheduler decision into request lifecycle
accounting without changing the stable HAL default.

Offline gate:

- `opena8djcpp_playback_scheduler_runtime_contract`.
- Required stable row keeps capture at `256` runtime submit calls.
- Required stable row batches playback to `33` runtime submit calls for `264`
  logical playback slots.
- Required playback submit reduction ratio is at least `8`.
- Required total submit reduction ratio is greater than `1.5`.
- Submitted and completed frame/byte accounting must match exactly.
- Runtime submit failures, completion failures, fallback allocations, invalid
  completions, stale completions, and live requests at snapshot must all be
  zero.

This binding is the contract for the next HAL opt-in candidate. It must be
inserted before the real submit boundary, around the playback refill decision,
while `submitPlaybackTransfer` remains the submit/observability boundary.

## HAL Playback Scheduler Candidate Boundary

The default-off HAL candidate preserves the real-time rule learned from failed
physical variants: do not reduce capture cadence before a same-session A/B
proves it is safe.

Implementation constraints:

- capture remains direct logical ISO8;
- playback may coalesce eight logical ISO8 slots into one prepared runtime
  submit;
- the prepared runtime bridge remains preallocated at stream start;
- no extra allocation, logging, disk I/O, or blocking IPC is introduced in the
  submit/completion path;
- completion ownership remains with the USB completion handlers;
- stream stats must expose prepared runtime submit/completion counters so the
  physical A/B can compare CPU/resource behavior against mainline.

The candidate is build-only until a hardware lock window explicitly installs or
loads it.
