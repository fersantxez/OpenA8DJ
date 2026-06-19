# Audio 8 DJ Use-Case Implementation Plan

This plan turns the Audio 8 DJ hardware dossier into an implementation roadmap.
It is intentionally staged: first make the hardware safe and observable, then
add user-facing configuration, then broaden to advanced routing and production
packaging.

## Guiding Rule

OpenA8DJ should expose two control layers:

- `opena8dj-control`: low-level CLI for engineering, tests, automation, and
  recovery.
- `OpenA8DJ Control Center`: signed user-facing app for presets, routing,
  status, diagnostics, and safe profile switching.

The CLI already exists and should remain scriptable. The app should call the
same backend instead of inventing a second control path.

## Stage 0: Shared Configuration Backend

Goal: make every use case configure the card through one state model.

Work:

- Define a stable config schema:
  - sample rate
  - buffer size hint
  - active profile
  - input mode
  - input decode on/off
  - ground-lift flags
  - software lock
  - input source map A/B/C/D
  - input transform map A/B/C/D
  - output pair labels and intended role
  - MIDI enabled/disabled
  - diagnostics level
- Add `opena8dj-control export-config` and `import-config`.
- Add `opena8dj-control list-profiles`.
- Add `opena8dj-control apply-preset <name>`.
- Store user presets under `~/Library/Application Support/OpenA8DJ/`.
- Store machine diagnostics under `~/Library/Logs/OpenA8DJ/` or explicit export
  bundles.

Control Center requirements:

- Read current hardware state.
- Show whether HAL bridge and MIDI bridge are alive.
- Apply a preset atomically.
- Warn before switching input mode while an audio app is active.
- Restore previous profile after test presets.

Acceptance:

- CLI and app report the same state.
- A saved preset can be exported, imported, applied, and verified.
- Failed writes leave the previous hardware state visible and recoverable.

## Stage 1: Playback-Only DJ And General Audio

Use cases:

- Playback through A/B/C/D.
- Spotify/VLC/Music/Safari output.
- Four stereo outputs to an external DJ mixer.
- Backup live interface.
- Multi-zone playback.

Card configuration:

- Profile: `playback`.
- Input decode: off.
- Software lock: optional, usually off unless preventing accidental A/B changes.
- Input mode: irrelevant for playback, but preserve last known safe value.
- Sample rates: start with 44.1 and 48 kHz only.
- Outputs:
  - A `1|2`: deck/main 1.
  - B `3|4`: deck/main 2.
  - C `5|6`: aux/deck 3.
  - D `7|8`: aux/deck 4 or backup.

Implementation steps:

1. Keep output pair smoke gate as required baseline.
2. Add a user preset: `Playback / 4 stereo outputs`.
3. Add output labels in Control Center.
4. Add a quick test tone per output pair.
5. Add a "do not disturb deck cabling" warning when testing A/B.
6. Add default-system-output helper for pair D or selected pair.

Control Center functionality:

- Output pair selector.
- Test tone button per pair.
- Pair labels: Deck A, Deck B, Aux C, Aux D, Backup, Zone 1-4.
- Live output meters from driver stats where available.
- "Playback safe mode" button: disables input decode and applies playback
  profile.

Acceptance:

- A/B/C/D output tests pass.
- No input decode CPU path is active in playback mode.
- User can identify which physical mixer channel receives each pair.

## Stage 2: Traktor Scratch / DVS Vinyl

Use cases:

- Deck A/B timecode vinyl.
- Traktor scope validation.
- Low-latency scratch behavior.
- Hybrid playback plus timecode input.

Card configuration:

- Profile: `timecode-vinyl`.
- Input mode: `0`.
- Input decode: on.
- `gnd-vinyl`: on by preset default.
- `gnd-cd-line`: off.
- `gnd-phono`: off.
- Software lock: on.
- Inputs:
  - A `1|2`: Deck A timecode input.
  - B `3|4`: Deck B timecode input.
- Outputs:
  - A `1|2`: Deck A output.
  - B `3|4`: Deck B output.

Implementation steps:

1. Preserve current `timecode-smoke-gate` as non-physical gate.
2. Add physical input-tone gate for A/B.
3. Add Traktor validation checklist/export.
4. Add input scope meter: RMS, peak, phase/correlation per pair.
5. Add profile restore after validation.
6. Add latency profile collection under Traktor.
7. Validate at 44.1 kHz, then 48 kHz.

Control Center functionality:

- DVS Vinyl preset.
- Per-deck input meter.
- Warnings if software lock is off.
- Ground-lift toggle with current state.
- "Traktor setup checklist" panel:
  - Audio device selected.
  - Deck A input A L/R.
  - Deck B input B L/R.
  - Deck A output A L/R.
  - Deck B output B L/R.
- Export diagnostics bundle for failed scope.

Acceptance:

- Traktor scope stable on A and B.
- A input does not leak to B, and B does not leak to A.
- Output A/B remains isolated while inputs are active.
- No white noise, speed drift, channel swap, or timecode dropout.

## Stage 3: Traktor Scratch / DVS CD-Line

Use cases:

- CDJ timecode CDs.
- Line-level DVS players.
- Line-level turntables.

Card configuration:

- Profile: `timecode-cd-line`.
- Input mode: `1`.
- Input decode: on.
- `gnd-cd-line`: on by preset default.
- `gnd-vinyl`: off.
- `gnd-phono`: off.
- Software lock: on.
- Inputs A/B for CDJ or line-level timecode sources.

Implementation steps:

1. Clone vinyl physical matrix but with mode `1`.
2. Add input-level warning: CD/line should be hotter than phono.
3. Validate CDJ timecode on A/B.
4. Validate hybrid: one vinyl deck, one CD-line deck, if hardware mode permits
   per-use workflow without unsafe global switching.

Control Center functionality:

- DVS CD/Line preset.
- Level meter calibrated for line-level sanity.
- Warning when user chooses CD-line but reports turntable/phono source.

Acceptance:

- Stable Traktor scope from CD/line source.
- Correct level and no clipping.
- No deck crossfeed or output disturbance.

## Stage 4: Normal Phono / Vinyl Digitization

Use cases:

- Record normal vinyl into DAW.
- Archive records.
- Compare cartridge/phono noise.

Card configuration:

- Profile: `phono`.
- Input mode: `2`.
- Input decode: on.
- `gnd-phono`: on by preset default.
- Software lock: on.
- Inputs:
  - A/B only for phono cartridges.
  - C/D not allowed for phono cartridges.

Implementation steps:

1. Validate clean input capture from A and B.
2. Add RIAA/phono sanity test with known vinyl or test record if available.
3. Add recording preset in Control Center.
4. Add gain/clipping monitor.
5. Add "vinyl archive" recording helper or handoff to DAW.

Control Center functionality:

- Vinyl Recording preset.
- Big warning: phono only on A/B.
- Grounding guidance: mixer ground vs Audio 8 GROUND vs lift.
- Input clipping history.
- Export WAV test recording.

Acceptance:

- A/B capture has correct speed, no channel swap, acceptable noise floor.
- C/D phono misuse is blocked or warned.
- Recording path works in a DAW or local recorder.

## Stage 5: Line Recording And DJ-Set Capture

Use cases:

- Mixer REC OUT into C/D.
- Mixer BOOTH/second master into C/D.
- Record DJ set while playing through A/B.
- External capture fallback when iRig is absent, once Audio 8 input is validated.

Card configuration:

- Input mode: playback-safe or line mode depending on A/B needs.
- Input decode: on only while recording.
- C/D line inputs as preferred capture path.
- Software lock: optional.
- Ground-lift flags: do not change unless A/B mode is involved.

Implementation steps:

1. Validate C/D line input physical capture.
2. Add duplex test: play A/B while recording C/D.
3. Add REC OUT preset.
4. Add overload warning for mixer line output.
5. Add long recording test with no dropouts.

Control Center functionality:

- "Record DJ Set" preset.
- Capture source selector: C `5|6` or D `7|8`.
- Input meters and clipping latch.
- Record a short test WAV.
- Export capture diagnostics.

Acceptance:

- C/D line capture works while A/B playback is active.
- No playback crackle caused by enabling input capture.
- Captured WAV has expected level and no clipping.

## Stage 6: External Effects Send/Return

Use cases:

- Mixer send -> Audio 8 input C.
- Audio 8 output C/D -> external effect return.
- Software effects insert path.

Card configuration:

- C/D inputs: line.
- C/D outputs: effect return.
- Input decode: on.
- A/B mode preserved for deck routing.
- Low buffer required.

Implementation steps:

1. Validate duplex latency C input to C/D output.
2. Add round-trip latency measurement.
3. Add DAW/plugin host routing recipe.
4. Add safe feedback warning.
5. Validate under Traktor plus external host if applicable.

Control Center functionality:

- Effects Loop preset.
- Round-trip latency display.
- Feedback warning.
- C/D input and output meters.

Acceptance:

- Latency is measured and repeatable.
- No feedback unless user deliberately patches it.
- Playback remains stable with duplex effect path active.

## Stage 7: Microphone

Use cases:

- XLR dynamic mic into Channel C.
- Talkover or recording.
- Emergency mic input.

Card configuration:

- Channel C source: MIC via physical `MIC/LINE` selector.
- Input decode: on.
- Mic gain set physically.
- No phantom power.

Implementation steps:

1. Validate mic input appears on expected C pair.
2. Add mic level meter.
3. Add no-phantom-power warning.
4. Add basic talkover/recording routing recipe.

Control Center functionality:

- Mic Input preset.
- "MIC/LINE must be set to MIC" physical checklist.
- Level meter and clipping indicator.
- No phantom power warning.

Acceptance:

- Dynamic mic records on expected pair.
- Line C is not accidentally treated as mic.
- Clipping indicator catches excessive mic gain.

## Stage 8: MIDI Interface

Use cases:

- Audio 8 DJ as standalone MIDI interface.
- Traktor controller integration.
- MIDI clock/control tests.

Card configuration:

- MIDI bridge running.
- Audio profile independent.
- No audio-mode dependency.

Implementation steps:

1. Verify LaunchAgent after install/login/hotplug.
2. Add MIDI loopback test.
3. Add long-run MIDI byte-loss test.
4. Add MIDI endpoint reset/restart command.

Control Center functionality:

- MIDI endpoint status.
- MIDI activity LEDs.
- Send/receive test.
- Restart MIDI bridge button.

Acceptance:

- MIDI In/Out visible after login, sleep/wake, hotplug.
- Loopback has zero dropped bytes in long test.

## Stage 9: Ground-Noise And Cabling Lab

Use cases:

- Compare turntable ground to mixer vs Audio 8 GROUND.
- Compare ground-lift flags.
- Diagnose hum, buzz, CPU-coupled noise, or channel noise.

Card configuration:

- Per-test profile: timecode-vinyl, timecode-cd-line, or phono.
- Ground-lift flag varied one at a time.
- Software lock on during measurement.
- Input decode on for input noise measurement.

Implementation steps:

1. Add noise-floor measurement per input pair.
2. Add ground-lift sweep script.
3. Add user checklist for physical ground cable position.
4. Export comparison report.

Control Center functionality:

- Ground Diagnostics wizard.
- Step-by-step physical prompts:
  - ground to mixer
  - ground to Audio 8
  - no additional ground
  - lift on/off
- Noise graph per step.
- Recommended setting based on measured hum/noise.

Acceptance:

- Report identifies lowest-noise configuration.
- Ground-lift changes are visible in measured noise where applicable.

## Stage 10: QA / Driver Engineering Matrix

Use cases:

- DAC pair comparison.
- Input/output loopback.
- Channel swap/invert/source testing.
- Low-latency benchmark.
- Sample-rate validation.
- Long-run stability.

Card configuration:

- Test-owned profile.
- Explicit sample rate and buffer.
- Input source/transform map under test.
- Output pair under test.
- Software lock on.
- Restore previous profile after test.

Implementation steps:

1. Add config snapshots before/after every gate.
2. Expand output-pair smoke to physical output verification.
3. Expand input tests to A/B/C/D.
4. Add transform tests:
   - normal
   - swap
   - invert-left
   - invert-right
   - invert-both
5. Add sample-rate matrix:
   - 44.1
   - 48
   - 96
   - 88.2 only if kept exposed and validated.
6. Add long-run gates for playback, duplex, and MIDI.

Control Center functionality:

- Engineering Diagnostics mode, hidden from casual users or clearly marked.
- Run selected gate.
- Save result bundle.
- Restore profile.
- Show loaded driver version/hash.

Acceptance:

- Test gates never leave the card in the wrong profile.
- Result bundle contains config, logs, metrics, and hardware state.
- Failures name the physical pair and mode that failed.

## Stage 11: Control Center Productization

Goal: replace manual terminal configuration for normal users.

Minimum UI:

- Device status:
  - connected/disconnected
  - driver loaded
  - sample rate
  - stream active
  - MIDI active
  - current profile
- Presets:
  - Playback / 4 stereo outputs
  - Traktor DVS Vinyl
  - Traktor DVS CD-Line
  - Vinyl Recording
  - DJ Set Recording
  - Effects Loop
  - Microphone
  - MIDI Only
  - Diagnostics
- Controls:
  - input mode
  - input decode
  - software lock
  - ground-lift flags
  - input source map
  - input transform map
  - output labels
  - MIDI bridge restart
- Diagnostics:
  - output test tone
  - input meter
  - MIDI loopback
  - capture short WAV
  - export support bundle
- Safety:
  - active-app warning
  - A/B deck-cabling warning
  - phono-vs-line warning
  - no-phantom-power warning
  - restore previous profile

Implementation steps:

1. Add backend JSON schema and CLI import/export.
2. Add a small local control service or extend the HAL bridge protocol.
3. Build native macOS app around the service.
4. Code sign and notarize app/helper.
5. Add app-driven smoke tests.
6. Integrate app into installer.

Acceptance:

- Normal user can configure every supported use case without Terminal.
- Engineering can still reproduce every app action through CLI.
- App never hides dangerous states; it names the physical cabling requirement.

## Stage 12: DriverKit / Production Driver

Goal: move from the current HAL driver plus user-space USB bridge to a
production-quality Apple System Extension driver architecture.

Work:

- Host app with System Extension activation.
- AudioDriverKit audio device/streams/controls.
- USBDriverKit transport for control, MIDI, capture, and playback.
- Same configuration schema as the HAL-era tool.
- Same preset model.
- Same diagnostics bundle format.

Acceptance:

- Clean install/uninstall.
- Hotplug and sleep/wake recovery.
- 8 in / 8 out plus MIDI plus controls.
- All supported use-case presets pass their physical validation gates.

## Recommended Build Order

1. Shared config backend and preset schema.
2. Playback preset and output test UI.
3. Timecode vinyl/CD-line presets and physical A/B validation.
4. Phono/vinyl recording input validation.
5. C/D line recording and DJ-set capture.
6. MIDI diagnostics.
7. Ground-noise wizard.
8. Effects loop and mic workflows.
9. Full engineering matrix.
10. Signed Control Center app.
11. DriverKit migration.

This order keeps the current validated playback path stable while expanding
input-heavy and duplex workflows only after the card state is observable,
restorable, and testable.
