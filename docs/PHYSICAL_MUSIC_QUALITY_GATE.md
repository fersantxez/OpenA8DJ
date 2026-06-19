# Physical music quality gate

`scripts/physical-music-quality-gate` compares a digital music reference WAV with
the physical iRig capture from the Audio 8 DJ -> mixer -> iRig path. It is
intended to reject false-good runs where the audio is aligned and audible but
sounds metallic, noisy, clicky, or coupled to CPU/UI activity.

The helper does not record or play audio. It analyzes existing files, so it can
be run after `scripts/run-soundcheck` or on archived `local-analysis` runs.

## Run on a soundcheck directory

```sh
python3 scripts/physical-music-quality-gate \
  --run-dir local-analysis/music-clean-baseline-after-stalefix-20260613-121659 \
  --json-out local-analysis/music-clean-baseline-after-stalefix-20260613-121659/physical-music-gate.json \
  --coupling-profile-out local-analysis/music-clean-baseline-after-stalefix-20260613-121659/physical-music-coupling.json
```

`--run-dir` expects:

- `fixture/reference.wav`
- `captured.wav`
- optional `cpu-profile.tsv`

## Run on explicit files

```sh
python3 scripts/physical-music-quality-gate \
  --reference local-analysis/soundcheck/candidate-0232-p2-irig-20260612-170819/fixture/reference.wav \
  --capture local-analysis/soundcheck/candidate-0232-p2-irig-20260612-170819/captured.wav \
  --cpu-profile local-analysis/soundcheck/candidate-0232-p2-irig-20260612-170819/cpu-profile.tsv \
  --json-out local-analysis/soundcheck/candidate-0232-p2-irig-20260612-170819/physical-music-gate.json
```

## Compare against a known-good physical baseline

Use a baseline JSON from a known-good run to make the gate relative instead of
only absolute:

```sh
python3 scripts/physical-music-quality-gate \
  --run-dir local-analysis/listen-gate/hotpath-pack-after-irig-recovery-20260613-123028/music \
  --baseline-json local-analysis/music-clean-baseline-after-stalefix-20260613-121659/physical-music-gate.json \
  --json-out local-analysis/listen-gate/hotpath-pack-after-irig-recovery-20260613-123028/music/physical-music-gate.json
```

## Default PASS/FAIL checks

The absolute gate is deliberately conservative for the physical analog route:

- alignment after time-warp must be at least `0.925`
- capture peak must be between `0.020` and `0.920`
- clipped frames must be `0`
- 1-5 kHz residual ratio must be at most `1.45`
- 1-5 kHz window p95 residual ratio must be at most `1.48`
- 1-5 kHz window max residual ratio must be at most `1.55`
- 1-5 kHz window p95/median residual spread must be at most `1.05`
- 1-5 kHz window max/median residual spread must be at most `1.10`
- 5-12 kHz residual ratio must be at most `1.355`
- mid-vs-low spectral coloration must stay within `+/-5 dB`
- high-vs-low spectral coloration must stay within `+/-6 dB`
- metallic coloration score must be at most `6 dB`
- quiet-segment 1-5 kHz residual must be at most `-32.5 dBFS`
- click outliers must be `0`
- lag jumps over 2 frames must be at most `45`
- CPU/residual correlation must be at most `0.16` when a CPU profile is present
- OpenA8DJ driver CPU must stay below avg `8%` and p95 `12%`
- coreaudiod p95 CPU must stay below `8%`

With `--baseline-json`, the candidate must also stay close to the baseline:

- 1-5 kHz residual ratio: baseline * `1.03`
- 1-5 kHz window p95: baseline * `1.03`
- 1-5 kHz window max: baseline * `1.05`
- 1-5 kHz p95/median and max/median spread: baseline * `1.05`
- 5-12 kHz residual ratio: baseline * `1.04`
- spectral coloration score/deltas: baseline + `0.75 dB` when the baseline
  JSON contains the same metric
- quiet 1-5 kHz noise: baseline + `1 dB`
- click outliers: max absolute click limit or baseline * `1.25`

The p95/median and max/median spread checks are intentionally important for the
Audio 8 DJ -> mixer -> iRig route. A fixed analog tone/EQ coloration can raise
absolute residuals without sounding like intermittent crackle; window-to-window
spread catches bursts, clicks, and CPU-coupled noise that are much closer to the
reported listening failures.

The spectral-coloration checks are aimed at the reported "metallic" or
high-pass-filtered failures. They compare the physical capture to the digital
reference in low, mid, and high bands, then reject large mid/high shifts against
the bass band even when residual and alignment metrics look superficially
plausible.

The text output is short key/value form and ends with `verdict=PASS` or
`verdict=FAIL`. The JSON output preserves the full metric set for release notes
or later comparison.

For a fast deterministic check of the spectral-coloration helper without
hardware or WAV fixtures:

```sh
make physical-music-quality-gate-selftest
```

This synthetic test verifies that uniform gain does not trigger the metallic
coloration score while a strong mid/high tilt does.
