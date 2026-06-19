# Quality Pass Protocol

Every OpenA8DJ build/test pass must leave a numeric record in its run directory.

Required artifacts:

- `build-variant.txt`: change note, bundle version/build, HAL hash, git state,
  output byte order, sample rate, buffer, and pair.
- `quality-summary.txt`: full key-value quality metrics.
- `pass-report.md`: short human-readable report with change, version, verdicts,
  and headline metrics.

The required headline metrics are:

- release gate verdict
- stream verdict
- internal consumed-audio verdict
- USB output-byte verdict
- analog loopback verdict
- raw input correlation verdict
- output peak
- active underruns
- playback failures and playback failure rate
- capture failure rate
- internal min SNR and min correlation
- USB output alignment score, check errors, and panic flags
- analog loopback min SNR and min correlation
- analog loopback 1-5 kHz residual ratio and residual dBFS
- 1-5 kHz residual / CPU correlation source and coefficient
- `coreaudiod`, OpenA8DJ driver, audio/UI service, player, and recorder CPU
  profile when analog capture is available

Use `scripts/verify-loopback-quality.sh --change-note "..."` for hardware
passes. The script writes the report even on failure and removes a failed
temporary HAL install unless `--leave-installed-on-fail` is explicitly used.

The analog loopback must be a post-device capture. VLC, BlackHole, Soundflower,
Loopback Audio, aggregate/multi-output routes, or any other software capture of
the player/system stream are useful only for diagnostic setup; they do not prove
what the Audio 8 DJ DAC/USB cadence produced. `scripts/run-soundcheck` blocks
known virtual capture devices by default. A passing candidate needs either:

- Audio 8 DJ output physically cabled into a working input capture path, or
- Audio 8 DJ output physically cabled into a separate recording interface.

For the current crackle investigation, a candidate is not ready for human
listening if the analog soundcheck shows either of these initial gates:

```text
mid_band_1000_5000_residual_ratio > 0.04
mid_band_cpu_corr > 0.60 when the windowed mid-band residual ratio is above 0.02
```

The run directory must keep `cpu-profile.tsv` and `coupling-profile.json` so a
later pass can explain whether the noise moved with Core Audio, the driver
process, WindowServer, Control Center, or other audio services.

## Human Listening Notification

When a candidate has passed the automated gates, is installed, is loaded, and
is ready for the user's listening test, send an email to
`fernandosanchezmunoz@gmail.com` with this message:

```text
Tengo un driver para tu prueba.
```

Do not send this email for failed candidates, measurement-only builds, or
versions that are not currently installed and loaded.
