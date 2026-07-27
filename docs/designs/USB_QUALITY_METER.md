# USB quality meter design

Status: implementation-ready MVP design  
Scope: macOS HAL stream statistics, public API v1, and
`opena8dj-control usb-quality`

## Goal

Provide a truthful, low-overhead, real-time view of the Audio 8 DJ USB audio
link. The meter reports completion-cadence jitter, isochronous errors by
direction and class, audio xruns, and a transparent stability classification.
It does not claim to measure electrical USB signal integrity or bus-level
packet jitter that IOUSBHost does not expose.

The implementation remains independent: it uses OpenA8DJ source and public
macOS APIs only. It does not use or derive from Native Instruments binaries,
headers, private protocols, or proprietary source.

## Existing path and constraints

`OpenA8DJUSBEngine` already records a non-resetting-per-read stream snapshot.
Counters reset when a stream starts. Capture and playback completion handlers
already obtain `mach_absolute_time()`, accumulate transfer/transaction/error
counters, and batch updates under `_streamStatsMutex`. Output fill statistics
are flushed in batches. `kIPCTypeStreamStatsGet` copies the payload without
resetting it, and `opena8dj-control api stats` exposes an append-compatible
public JSON subset.

Existing completion timing contains cumulative minimum, maximum, sum, and
sample count in Mach ticks. This is useful for diagnostics but cannot produce
window percentiles: cumulative minima/maxima cannot be subtracted. Existing
aggregate transaction-failure counters also do not always distinguish a
failed transfer completion from a failed isochronous transaction. The MVP
therefore appends fixed counters and histograms; it does not add a new callback,
event log, sample queue, or resident process.

Hard realtime rule: capture/playback completions and Core Audio callbacks must
perform no allocation, I/O, logging for the meter, sleep, dispatch, percentile
calculation, JSON formatting, or new lock acquisition. Histogram increments
must occur in the existing stream-statistics update batch while
`_streamStatsMutex` is already held, or through already-existing fixed atomic
accumulators that are copied by the snapshot. Output xrun counters continue to
use the existing batched output-fill path.

Implementation note: builds with `OPENA8DJ_HOT_STREAM_STATS_INTERVAL` greater
than one retain completion-quality observations in fixed per-direction batch
storage, following the existing output-fill batching pattern, and flush them
while the existing stream-statistics lock is held. A live snapshot can lag by
one partial batch; stopping flushes the remainder. The first completion after a
restart stays excluded, and every flushed histogram preserves
`sum(bins) == samples`.

## Metric contract

All driver fields are unsigned cumulative counters scoped to the current
stream. The CLI derives a window by subtracting two snapshots. If any required
counter decreases, or streaming changes from false to true, the previous
baseline is discarded and the next result is `warming-up`; unsigned wraparound
is never treated as a valid delta.

### Completion-cadence jitter

This metric is callback/completion cadence jitter, not USB PHY jitter:

```text
nominal_ticks =
    transfer_transaction_count * mach_ticks_per_second / 8000

absolute_jitter_ticks =
    abs((completion_time - previous_completion_time) - nominal_ticks)
```

The capture and playback directions have independent histograms. The first
completion in a stream has no preceding interval and is not sampled. A zero or
backward host-time delta is excluded from the histogram and increments an
`invalidIntervals` counter for that direction. The transfer's actual
transaction count is used, so playback coalescing does not silently use the
capture transfer duration.

The fixed histogram is cumulative and append-only in the private stream
payload. Bins are mutually exclusive and expressed in microseconds after an
integer-safe conversion from Mach ticks:

| Index | JSON key | Absolute jitter |
| --- | --- | --- |
| 0 | `le50` | 0 through 50 us |
| 1 | `le100` | greater than 50 through 100 us |
| 2 | `le250` | greater than 100 through 250 us |
| 3 | `le500` | greater than 250 through 500 us |
| 4 | `le1000` | greater than 500 through 1000 us |
| 5 | `gt1000` | greater than 1000 us |

The thresholds are initialized outside the streaming callbacks and stored as
Mach ticks. The hot path performs subtraction, absolute difference, bounded
integer comparisons, and one counter increment. There is no division or
floating-point percentile calculation in a callback.

For each window, the consumer reports `samples`, all six bin deltas, and p50,
p95, and p99 as histogram upper bounds in microseconds. For example, a p95 in
bin `le250` is reported as `{"upperBoundUs":250,"overflow":false}`. A percentile
in `gt1000` is reported as
`{"upperBoundUs":null,"lowerBoundExclusiveUs":1000,"overflow":true}`; it is not
fabricated as 1000 us. A percentile is `null` when the direction has no window
samples. These are bounded estimates, not exact percentiles.

### Isochronous errors

Window deltas are reported separately for `capture` and `playback`:

- `queueFailures`: IOUSBHost request enqueue failed or no bounded pooled
  transfer was available;
- `completionStatusFailures`: transfer completion status failed, excluding
  intentional `kIOReturnAborted` while stopping;
- `transactionStatusFailures`: an individual isochronous transaction status
  failed;
- `zeroLengthTransactions`: a successful transaction completed zero bytes
  while a nonzero request was expected; and
- `shortTransactions`: a successful transaction completed fewer bytes than
  requested/expected, including zero-length transactions.

The classes may overlap intentionally: `zeroLengthTransactions` is a diagnostic
subset of `shortTransactions`. `totalEvents` is the sum of the five displayed
class counters and is explicitly an event total, not a deduplicated packet-loss
count. Existing public v1 capture/playback counters keep their names, types,
units, and semantics.

### Xruns

The window reports:

- `outputUnderruns`: existing total output underruns;
- `activeOutputUnderruns`: existing underruns after playback became active;
- `outputRingOverruns`: existing producer-side ring overruns;
- `outputLateWriteBatches` and `outputLateWriteFrames`: existing late Core
  Audio writes; and
- `totalHardXruns`:
  `activeOutputUnderruns + outputRingOverruns`.

Startup-silence underruns remain visible through `outputUnderruns` but are not
classified as hard xruns. Late writes are leading indicators, not added to
`totalHardXruns`. The names prevent the meter from presenting every silence
frame or late frame as an independent xrun.

## Stability evaluation

The meter emits a category, never an unexplained percentage:

- `not-streaming`: the driver reports no active stream;
- `warming-up`: no previous compatible snapshot exists;
- `insufficient-data`: an active direction has fewer than 20 completion
  intervals in the window;
- `stable`: all active directions have enough samples, no isochronous error
  event or hard xrun occurred, each direction has p95 at most 250 us and p99 at
  most 500 us, and at most 0.1% of its samples are in `gt1000`;
- `degraded`: no isochronous error event or hard xrun occurred, but a jitter
  limit for `stable` was exceeded while p99 is at most 1000 us and no more than
  1.0% of samples are in `gt1000`; or
- `unstable`: any isochronous error event or hard xrun occurred, any active
  direction's p99 is in `gt1000`, or more than 1.0% of its samples are in
  `gt1000`.

An active direction is one whose transfer counter increased in the window.
Inactive directions are shown with zero samples but do not force
`insufficient-data`. Every result includes:

- `classification`;
- `reasons`, an array of stable machine-readable reason strings such as
  `capture.iso_errors`, `output.hard_xrun`, or `playback.p99_gt_1000us`;
- `thresholds`, containing `minimumSamplesPerActiveDirection: 20`,
  `stableP95UpperBoundUs: 250`, `stableP99UpperBoundUs: 500`,
  `degradedP99UpperBoundUs: 1000`,
  `stableOverflowPermilleMax: 1`, and
  `degradedOverflowPermilleMax: 10`; and
- the input metric deltas used by the decision.

Integer permille comparisons use cross multiplication, so no floating-point
rounding changes the boundary. Error/xrun rules take precedence over sample
count: a short window containing a real error is `unstable`, not
`insufficient-data`.

## Public API v1 extension

`opena8dj-control api stats` remains the single non-destructive snapshot
operation and keeps schema `org.opena8dj.public-api.response.v1`, API version
`1.0`, and all existing members. A minor, additive `quality` member is added to
`data`; old clients that ignore unknown object members remain compatible.
`version.get.data.capabilities` gains `usb-quality.read`.

`data.quality` contains cumulative stream-scoped integers only:

```json
{
  "completionJitter": {
    "unit": "microseconds",
    "binUpperBoundsUs": [50, 100, 250, 500, 1000, null],
    "capture": {
      "samples": 0,
      "invalidIntervals": 0,
      "bins": {"le50": 0, "le100": 0, "le250": 0, "le500": 0, "le1000": 0, "gt1000": 0}
    },
    "playback": {
      "samples": 0,
      "invalidIntervals": 0,
      "bins": {"le50": 0, "le100": 0, "le250": 0, "le500": 0, "le1000": 0, "gt1000": 0}
    }
  },
  "isoErrors": {
    "capture": {
      "queueFailures": 0,
      "completionStatusFailures": 0,
      "transactionStatusFailures": 0,
      "zeroLengthTransactions": 0,
      "shortTransactions": 0
    },
    "playback": {
      "queueFailures": 0,
      "completionStatusFailures": 0,
      "transactionStatusFailures": 0,
      "zeroLengthTransactions": 0,
      "shortTransactions": 0
    }
  }
}
```

`unit` and `binUpperBoundsUs` are metadata; counters are JSON integers. The
driver-private packed payload appends fields only at its end and stays private
IPC version 1. The CLI's duplicate payload definition must remain byte-for-byte
field ordered with the HAL definition. A shorter append-compatible legacy HAL
produces zeros for the absent cumulative fields and an
`instrumentationAvailable: false` flag under `quality`; it must not be reported
as a healthy zero-error sample. Existing payload minimum length through
`sampleRate` is unchanged.

The existing top-level exact-member contract test must be deliberately updated
for the additive v1 member while retaining assertions for every pre-existing
group and legacy shortened-payload behavior. No public field is removed,
renamed, retyped, or given new side effects.

## Real-time CLI

The bundled utility is an extension of the existing control binary:

```text
opena8dj-control usb-quality
opena8dj-control usb-quality --interval-ms 1000 --count 30
opena8dj-control usb-quality --json --interval-ms 1000 --count 30
```

Default interval is 1000 ms; accepted range is 100 through 60000 ms. Default
count is unlimited until SIGINT; `--count` accepts 1 through 86400. Invalid
arguments return 2 before connecting. The command uses the same authenticated,
bounded, non-waking connection and the same private snapshot reader as
`api stats`. It reconnects for each snapshot so a HAL restart is recoverable.
It never initializes HAL, USB, or Core Audio.

Human mode prints a compact header and one line per completed window containing
classification, capture/playback p95 and p99 bounds, ISO error event totals,
hard xruns, and actual window duration. Overflow percentiles are printed
`>1000us`; unavailable values are `-`. Reasons and thresholds are printed when
the category is not `stable`.

`--json` writes newline-delimited JSON, one complete document per observation:

```json
{
  "schema": "org.opena8dj.usb-quality.sample.v1",
  "sequence": 2,
  "windowMilliseconds": 1001,
  "streaming": true,
  "sampleRateHz": 48000,
  "instrumentationAvailable": true,
  "jitter": {},
  "isoErrors": {},
  "xruns": {},
  "stability": {}
}
```

The first document is `warming-up`; subsequent documents contain deltas. JSON
uses the metric and stability shapes defined above. stdout contains only
NDJSON in `--json` mode. Backend errors use one JSON error document with the
same sample schema, an `error` object compatible with public API v1 codes, and
a nonzero process exit. SIGINT exits 0 after the last complete line.

The window calculator and renderers are pure functions over two snapshots and
an elapsed duration. Both `api stats` and `usb-quality` call the same quality
snapshot serialization helpers; the calculation is not reimplemented in a
script.

## Testing and acceptance

Offline tests are deterministic and require no Audio 8 DJ, HAL load, USB
session, or Core Audio initialization:

1. Build the shipping control tool and HAL with
   `-Wall -Wextra -Wpedantic`; warnings fail the feature review.
2. Extend the public API mock-socket contract test with fixed payload fixtures
   for every new counter/bin, the additive `quality` JSON shape, integer types,
   legacy shortened payloads, and non-destructive single-snapshot behavior.
3. Add window-calculator fixtures/snapshots for: first sample; not streaming;
   clean stable; stable/degraded boundaries; overflow p99; one error per ISO
   class and direction; startup-only underrun; active underrun; ring overrun;
   late writes; inactive direction; insufficient samples; counter reset; and
   missing instrumentation.
4. Exercise human and NDJSON CLI output against a sequence-serving mock Unix
   socket with a short test interval. Assert parseable one-document-per-line
   JSON, no prose on JSON stdout, percentile overflow representation, visible
   thresholds/reasons, bounded exit on unavailable/malformed backends, and no
   input-stat/reset request.
5. Assert HAL and CLI packed stream-payload definitions have identical ordered
   fields and all additions occur after the former
   `outputLateWriteBatches` tail.
6. Existing public API, HAL smoke/parity, and repository offline tests continue
   to pass.

Live verification is optional for acceptance because it depends on the
installed driver/hardware. If the active HAL supports the appended payload, run
a read-only 10-window meter and verify increasing sample counts, plausible
8/16-ms completion cadence handling, valid NDJSON, and no new xruns caused by
observation. Every command that builds/loads or initializes HAL/USB/Core Audio,
runs against hardware, or measures performance must use:

```text
./scripts/shared-hardware-lock-run \
  --gate usb-quality \
  --run-dir <unique-run-directory> \
  -- <command>
```

Do not remove or override a live shared lock.

The MVP is accepted when all offline cases pass without hardware; both
directions and all error/xrun classes are visible; percentiles are honest
histogram bounds; stability output includes its thresholds, reasons, and input
deltas; legacy public API fields and shortened private payloads remain
compatible; repeated observation is non-destructive; and code inspection
confirms there is no new allocation, I/O, lock acquisition, or heavy
calculation in a callback/completion.

## Deferred work and risks

- IOUSBHost does not expose PHY error counters, bus retries, or true
  microframe-arrival jitter here. Future hardware-specific observability must
  use a separately documented public interface and must not relabel completion
  cadence.
- Histogram resolution intentionally trades exact percentiles for bounded,
  constant hot-path cost. Changing bin thresholds would change public
  semantics and requires new fields/schema members rather than reinterpretation.
- Snapshot fields are not captured by one global atomic transaction. A counter
  may advance while the struct is copied. This can shift a single event between
  adjacent windows but cannot reset or consume it; a future generation/sequence
  field could strengthen cross-field consistency.
- Counter reset detection cannot distinguish stream restart from unsigned
  wraparound, so both correctly discard the window.
- The default one-second window is diagnostic, not a release performance
  benchmark. It can miss transient ordering inside a window but retains the
  histogram tail and error counters.
