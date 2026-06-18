# Mainline Baseline Metrics

Date: 2026-06-16

Scope: read-only extraction from `/Users/fer/dev/opena8dj` for objective C/mainline comparison baselines. No hardware, audio playback, driver install, service restart, or USB/CoreAudio mutation was performed for this extraction.

Mandatory warning passed to this extractor:

> PROHIBIDO tocar, editar, formatear, generar archivos, limpiar, resetear, instalar o mutar cualquier cosa en /Users/fer/dev/opena8dj o /Users/fer/dev/audio8djrust. Esos worktrees son READ ONLY. Solo puedes escribir en /Users/fer/dev/audio8djcpp. No tocar hardware/audio/CoreAudio/USB sin lock global y sin autorizacion de ventana.

## Read-Only Commands Used

- `pwd && git -C /Users/fer/dev/audio8djcpp branch --show-current && git -C /Users/fer/dev/audio8djcpp rev-parse --show-toplevel`
- `rg -n "baseline|0\\.3\\.(24|25|135)|sideband|click_outliers|quality_alignment_score|lag_jumps|residual|CPU|cpu|threshold|PASS|FAIL|tone|music" /Users/fer/dev/opena8dj/docs ...`
- `nl -ba /Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md | sed -n '2601,2681p'`
- `nl -ba /Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-15.md | sed -n '1,220p'`
- `nl -ba /Users/fer/dev/opena8dj/docs/RELEASE_NOTES_0.3.24.md | sed -n '1,70p'`
- `nl -ba /Users/fer/dev/opena8dj/docs/RELEASE_NOTES_0.3.25.md | sed -n '1,90p'`
- `nl -ba /Users/fer/dev/opena8dj/docs/PHYSICAL_MUSIC_QUALITY_GATE.md | sed -n '48,86p'`
- `nl -ba /Users/fer/dev/opena8dj/docs/QUALITY_PASS_PROTOCOL.md | sed -n '24,55p'`
- `find /Users/fer/dev/opena8dj/local-analysis ...` with path filters only, then `nl -ba` on selected existing `summary.txt`, `tone-summary.txt`, and `cpu.tsv` files.
- CPU aggregation was derived from the existing 0.3.25 TSV with `rg 'OpenA8DJ\\.driver' ... | awk '{print $3}' | sort -n | awk ...` and an equivalent `coreaudiod` filter.

## Recommended Baseline

Use `0.3.135` as the primary C/mainline baseline for software-only/digital/no-iRig comparison:

- It is explicitly documented as the starting safe baseline on 2026-06-15 with installed HAL hash `0949969d396223257c8207b4454798b6e8a09593b1a6479e87ae2e6bbe24bb6a` (`/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-15.md:10`).
- Its digital gate passed for simulated A/B/C/D with alignment `1.000000`, simulated SNR `75.22 dB`, mid-band residual ratio `0.000669`, mid-band residual `-108.83 dBFS`, and click outliers `0` (`/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-15.md:17-25`).
- Its repeated no-iRig stress runs had driver p95 `6.60-6.70%`, coreaudiod p95 `1.60-1.70%`, stress driver p95 `5.80%`, zero elastic drops, zero late writes, and zero playback completion outliers (`/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-15.md:26-33`).
- After restoring from rejected experiments, it again passed the digital gate with the same A/B/C/D metrics and one stress run at driver p95 `6.80%`, coreaudiod p95 `1.80%`, stress driver p95 `5.80%`, and zero elastic drops/late writes/outliers (`/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-15.md:96-116`).

Do not use `0.3.135` alone as physical readiness proof:

- The documented 0.3.135 physical gate was blocked because iRig was missing from USB/CoreAudio (`/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md:2664-2674`).
- The 0.3.135 conclusion says it was not approved for human listening or release readiness while the physical iRig gate could not run (`/Users/fer/dev/opena8dj/docs/QUALITY_RUNS_2026-06-14.md:2675-2678`).

## Short Baseline Table

| Version/run | Best evidence | Worst/gap evidence | Baseline use |
| --- | --- | --- | --- |
| `0.3.24` final ISO5 | Physical 1 kHz iRig tone final `sideband_ratio=0.008407`, strongest sideband `-43.70 dB`; best same-build tone `sideband_ratio=0.004942`, strongest sideband `-48.74 dB`; real music no clipping and repeat with `click_outliers=0` (`RELEASE_NOTES_0.3.24.md:25-34`). | Output-only device shape: `0 inputs / 8 outputs`; timecode capture not supported (`RELEASE_NOTES_0.3.24.md:11`, `RELEASE_NOTES_0.3.24.md:40-41`). Existing final music summaries still failed strict analyzer with lag jumps `41-42` and SNR around `10.5-11.1 dB` (`local-analysis/final-0324-iso5-music-20260613-005459/music/summary.txt:1-19`, `local-analysis/final-0324-iso5-music-repeat-20260613-005553/music/summary.txt:1-19`). | Physical tone/music historical floor only. C++ must beat tone sideband/residual and preserve full 8-in/8-out functionality, so 0.3.24 cannot be the full product baseline. |
| `0.3.25` public preview | Restored 8 inputs / 8 outputs, Input/Output A/B/C/D surface, MIDI endpoints, `timecode-vinyl` profile PASS, active output path PASS at 48 kHz / 512 frames with active underruns `0`, playback failures `0`, and idle post-playback CPU `0.0%` (`RELEASE_NOTES_0.3.25.md:8-24`, `RELEASE_NOTES_0.3.25.md:51-68`). | Human listening rejected one ISO16 run as metallic/noisy/bass saturated; that run had tone sideband `0.047597` vs baseline `0.008407` and residual `0.990972` vs `0.431691` (`QUALITY_RUNS_2026-06-13.md:32-53`). A 0.3.25 playback CPU TSV showed driver avg `26.58%`, p95 `37.60%`, max `37.70%`, while coreaudiod avg `1.79%`, p95 `2.50%`, max `13.30%` from `local-analysis/playback-cpu-clean-0.3.25-20260613-102001/cpu.tsv`. | Functional/timecode topology baseline. C++ must at least preserve 8-in/8-out, A/B/C/D naming/routing, MIDI/control surface, and timecode profile behavior, while beating CPU/noise. |
| `0.3.135` atomic-written/no-iRig | Digital A/B/C/D PASS: alignment `1.000000`, SNR `75.22 dB`, residual ratio `0.000669`, residual `-108.83 dBFS`, click outliers `0`; playback gate around driver p95 `6.5-6.8%`, coreaudiod p95 `1.7-1.8%`, zero timeline resets, underruns, late writes, elastic drops/replays, completion outliers (`QUALITY_RUNS_2026-06-14.md:2631-2654`, `QUALITY_RUNS_2026-06-15.md:17-33`, `QUALITY_RUNS_2026-06-15.md:96-116`). | Physical capture blocked by missing iRig, so no physical music/tone approval and no human listening/release readiness (`QUALITY_RUNS_2026-06-14.md:2664-2678`). Rejected follow-up `0.3.138` proved small hot-counter changes can explode audio-stack CPU to `125.1%` total watched CPU (`QUALITY_RUNS_2026-06-15.md:75-94`). | Primary C CPU/digital/stability baseline. C++ must beat this before any branch promotion is defensible. |

## Practical Thresholds To Carry Forward

Physical music absolute PASS/FAIL thresholds from the current mainline gate
implementation are authoritative when they are stricter than older docs:

- alignment after time-warp `>= 0.970`
- capture peak between `0.020` and `0.920`
- capture RMS between `-28.0` and `-10.0 dBFS`
- clipped frames `0`
- 1-5 kHz residual ratio `<= 1.38`
- 1-5 kHz p95 residual ratio `<= 1.40`
- 1-5 kHz max residual ratio `<= 1.46`
- 1-5 kHz p95/median spread `<= 1.03`
- 1-5 kHz max/median spread `<= 1.06`
- 5-12 kHz residual ratio `<= 1.32`
- metallic coloration score `<= 6 dB`
- quiet 1-5 kHz residual `<= -32.5 dBFS`
- click outliers `0`
- lag jumps over 2 frames `<= 3`
- time-warp lag drift `<= 8.0`
- time-warp score `>= 0.85`
- CPU/residual correlation `<= 0.08` when CPU profile exists
- OpenA8DJ driver CPU avg `< 8%`, p95 `< 12%`
- coreaudiod p95 `< 8%`

Source: `/Users/fer/dev/opena8dj/scripts/physical-music-quality-gate:428-459`.
The older prose in `/Users/fer/dev/opena8dj/docs/PHYSICAL_MUSIC_QUALITY_GATE.md:48-69`
is looser on alignment, residual ratios, lag jumps, and CPU/noise correlation;
do not use those looser values for C++ promotion.

Relative-to-baseline thresholds:

- 1-5 kHz residual ratio and p95 must stay within baseline * `1.03`.
- 1-5 kHz max residual must stay within baseline * `1.05`.
- 5-12 kHz residual must stay within baseline * `1.04`.
- coloration deltas must stay within baseline + `0.75 dB`.
- quiet 1-5 kHz noise must stay within baseline + `1 dB`.
- click outliers must satisfy the absolute click limit or baseline * `1.25`.

Source: `/Users/fer/dev/opena8dj/docs/PHYSICAL_MUSIC_QUALITY_GATE.md:71-82`.

Additional quality-pass protocol:

- Preserve capture failure rate, internal min SNR/correlation, USB output alignment, check errors, panic flags, analog loopback SNR/correlation, residuals, CPU correlation, and CPU profiles (`/Users/fer/dev/opena8dj/docs/QUALITY_PASS_PROTOCOL.md:24-31`).
- Physical analog loopback must be post-device capture; software/virtual captures do not prove DAC/USB cadence quality (`/Users/fer/dev/opena8dj/docs/QUALITY_PASS_PROTOCOL.md:37-44`).
- Early crackle-investigation blocks were `mid_band_1000_5000_residual_ratio > 0.04` and CPU correlation `> 0.60` when windowed residual ratio is above `0.02` (`/Users/fer/dev/opena8dj/docs/QUALITY_PASS_PROTOCOL.md:46-52`).

## Objective Promotion Bar For C++

C++ cannot be promoted over C/mainline until it has reproducible evidence for all of these:

1. Functional parity or better than `0.3.25`: 8 inputs, 8 outputs, A/B/C/D names, routing identity, MIDI/control surface if still required, and `timecode-vinyl` profile behavior.
2. Digital/stability parity or better than `0.3.135`: A/B/C/D alignment `1.000000`, simulated SNR at least `75.22 dB`, mid-band residual ratio no worse than `0.000669`, click outliers `0`, no timeline resets, no active underruns, no elastic drops/replays, no late writes, no completion outliers.
3. CPU better than `0.3.135`: driver p95 must be materially below `6.5-6.8%` in the same playback CPU gate, coreaudiod p95 below `1.7-1.8%`, and stress driver p95 below `5.7-6.0%`, without higher start latency or hidden service CPU.
4. Physical tone better than `0.3.24`: sideband ratio below `0.004942` preferred, at minimum below final `0.008407`; strongest sideband more suppressed than `-48.74 dB` preferred, at minimum more suppressed than `-43.70 dB`; no clipping and no new click bursts.
5. Physical music better than the strict gate and the historical floor: PASS under current `physical-music-quality-gate`, no click outliers, lag jumps `<= 3`, residual/coloration no worse than baseline-relative limits, and CPU/noise correlation `<= 0.08`.

## Readiness Conclusion

No readiness is declared by this document. The defensible current C/mainline baseline split is:

- `0.3.135` for CPU, digital quality, no-iRig stability, and offline/stress comparison.
- `0.3.25` for full 8-in/8-out, Traktor/timecode-facing topology, channel naming, and control profile behavior.
- `0.3.24` for historical physical tone/music sound-quality floor.

The C++ branch must beat the relevant baseline in each category before any Legacy/main branch move is technically justified.
