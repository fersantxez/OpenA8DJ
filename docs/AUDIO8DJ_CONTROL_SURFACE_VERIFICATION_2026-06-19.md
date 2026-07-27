# Audio 8 DJ Control Surface Verification - 2026-06-19

## Modern Control Center offline verification — 2026-07-27

The current panel supersedes the preset/import/export UI described later in
this historical record. Its supported trust boundary is the bundled public API
1.1 process interface plus the bundled read-only hardware profiler. The panel
does not directly use private IPC, Core Audio, USB, a shell, `PATH`, or a
network.

Offline acceptance commands:

```sh
make control-center-offline-test
make control-center-smoke-test
```

The fixture suite covers compatible 1.1, older compatible partial tails,
profiler `UNKNOWN`, schema/API/operation/type/enum/cross-field mismatches,
invalid UTF-8, missing newline, duplicate/trailing objects, stdout/stderr caps,
timeout/cancellation, non-overlap, refresh coalescing, visibility cancellation,
2/4/8/15-second backoff, baseline/delta/counter reset/reconnect/generation
rollback, stale last-good retention, action errors, requested/effective pending,
Timecode armed/waiting, Vintage partial/unverified, loopback privacy/default
disabled, and package/source policy.

The smoke target performs a release build at deployment target macOS 13.0,
constructs the root and every top-level SwiftUI section offscreen from checked-in
fixtures, lints the plist, verifies the ad-hoc signature, rejects symlinked or
missing bundled tools, verifies the control/profiler/catalog hashes, and checks
for prohibited vendor assets and APIs.

No app launch, Core Audio/API live read, live profiler invocation, install, or
screenshot is part of those commands. Consequently this implementation is
offline-verified only. Live metrics and final visual QA remain unverified until
normal, dark, increased-contrast, large-text, and Reduce Motion screenshots are
captured under:

```sh
./scripts/shared-hardware-lock-run \
  --gate modern-control-panel \
  --run-dir local-analysis/modern-control-panel/<unique-run> \
  -- open build/OpenA8DJControlCenter.app
```

The older locked results below remain historical evidence for the old control
surface; they are not evidence for the modern dashboard.

## Scope

This verifies the two current OpenA8DJ control surfaces:

- CLI: `opena8dj-control`
- macOS panel: `OpenA8DJ Control Center`

Hardware access was logged before use in:

```text
local-analysis/HARDWARE_USE_LOG_20260619_CONTROL_SURFACES.md
```

The live hardware section was run only through the shared lock wrapper. The lock
was confirmed free after the run with `scripts/shared-hardware-lock-status`.

## Build And Bundle Checks

Commands run:

```sh
make build/opena8dj-control control-center
./build/opena8dj-control list-profiles
codesign --verify --deep --strict build/OpenA8DJControlCenter.app
plutil -lint build/OpenA8DJControlCenter.app/Contents/Info.plist
build/OpenA8DJControlCenter.app/Contents/Resources/opena8dj-control list-profiles
spctl --assess --type execute build/OpenA8DJControlCenter.app
```

Result:

- CLI build passed.
- macOS app build passed.
- App bundle contains the matching `opena8dj-control` binary.
- Code signature verification passed for the local ad-hoc signature.
- `Info.plist` lint passed.
- Gatekeeper assessment rejected the app because it is ad-hoc signed, which is
  expected for local development. Public distribution still needs Developer ID
  signing and notarization.

## Locked Hardware Smoke Test

Command shape:

```sh
scripts/shared-hardware-lock-run \
  --gate control-surface-preset-smoke \
  --run-dir local-analysis/control-surface-preset-smoke/20260619-logged-verify \
  --wait-lock 0 \
  -- bash -lc '<save original; apply presets; verify exported fields; restore original>'
```

Result:

```text
shared_hardware_lock_run=PASS
exit_code=0
run_dir=local-analysis/control-surface-preset-smoke/20260619-logged-verify
finished_at=2026-06-19T16:33:05Z
```

Restored final state:

```text
input-mode:        0 (timecode-vinyl)
gnd-vinyl:         off
gnd-cd-line:       off
gnd-phono:         off
software-lock:     on
input-decode:      on
input-transform:   A=normal B=normal C=normal D=normal
input-source:      A=A B=B C=C D=D
```

## Preset Coverage

The CLI successfully applied and exported every built-in preset:

| Preset | Hardware field verification | Panel coverage |
| --- | --- | --- |
| `playback-4out` | `inputDecode=false` | Apply button calls same CLI preset |
| `traktor-dvs-vinyl` | `inputMode=timecode-vinyl`, vinyl ground lift, software lock | Apply button calls same CLI preset |
| `traktor-dvs-cd-line` | `inputMode=timecode-cd-line`, CD-line ground lift, software lock | Apply button calls same CLI preset |
| `vinyl-recording` | `inputMode=phono`, phono ground lift, software lock | Apply button calls same CLI preset |
| `dj-set-recording` | `inputDecode=true`, `softwareLock=false` | Apply button calls same CLI preset |
| `effects-loop` | `inputDecode=true`, `softwareLock=false` | Apply button calls same CLI preset |
| `microphone` | `inputDecode=true`, `softwareLock=false` | Apply button calls same CLI preset |
| `midi-only` | `inputDecode=false` | Apply button calls same CLI preset |
| `ground-diagnostics` | `inputDecode=true`, `softwareLock=true` | Apply button calls same CLI preset |
| `engineering-diagnostics` | `inputDecode=true`, `softwareLock=true` | Apply button calls same CLI preset |

## What The Panel Can Configure Now

The panel can configure all current use-case presets at preset granularity:

- Select a preset.
- Apply it through the embedded CLI backend.
- Refresh current hardware state from exported JSON.
- Export the current config to JSON.
- Import a JSON config generated by the CLI/panel.

The panel is intentionally not yet a full advanced editor. It does not expose
manual controls for every low-level field:

- Individual `inputMode`, ground-lift, software-lock, and input-decode toggles.
- Per-pair input source remapping.
- Per-pair swap/invert transforms.
- Sample rate, buffer, stream, and MIDI service controls.
- Physical validation workflows such as meters, test tone, loopback capture, or
  sound-quality gates.

Those are still available through the CLI and should become a second panel
surface only after the preset workflow stays stable.

## Implementation Note

`opena8dj-control` now retries connection/readback around HAL bridge wake and
control writes. This addresses the prior transient failure where the socket
became available but the first control read failed.
