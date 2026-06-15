use std::f32::consts::TAU;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TimecodeAnalysisConfig {
    pub sample_rate_hz: f32,
    pub expected_carrier_hz: f32,
    pub min_rms: f32,
    pub max_balance_db: f32,
    pub max_frequency_error_ppm: f32,
    pub max_jitter_frames_p95: f32,
    pub min_abs_correlation: f32,
    pub max_clipped_samples: usize,
}

impl Default for TimecodeAnalysisConfig {
    fn default() -> Self {
        Self {
            sample_rate_hz: 48_000.0,
            expected_carrier_hz: 1_000.0,
            min_rms: 0.05,
            max_balance_db: 1.0,
            max_frequency_error_ppm: 50.0,
            max_jitter_frames_p95: 2.0,
            min_abs_correlation: 0.95,
            max_clipped_samples: 0,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TimecodeAnalysis {
    pub frames: usize,
    pub left_rms: f32,
    pub right_rms: f32,
    pub left_peak: f32,
    pub right_peak: f32,
    pub balance_db_abs: f32,
    pub left_carrier_hz: f32,
    pub right_carrier_hz: f32,
    pub frequency_error_ppm_abs: f32,
    pub jitter_frames_p95: f32,
    pub abs_correlation: f32,
    pub clipped_samples: usize,
    pub left_rising_edges: usize,
    pub right_rising_edges: usize,
    pub pass: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TimecodeSignalError {
    InvalidConfig,
    EmptyInput,
    TooFewEdges,
}

pub fn synthetic_timecode_carrier(
    config: TimecodeAnalysisConfig,
    seconds: f32,
    amplitude: f32,
    right_gain: f32,
    right_phase_radians: f32,
) -> Result<Vec<[f32; 2]>, TimecodeSignalError> {
    validate_config(config)?;
    if !seconds.is_finite()
        || seconds <= 0.0
        || !amplitude.is_finite()
        || !right_gain.is_finite()
        || !right_phase_radians.is_finite()
    {
        return Err(TimecodeSignalError::InvalidConfig);
    }

    let frames = (seconds * config.sample_rate_hz).round() as usize;
    if frames == 0 {
        return Err(TimecodeSignalError::EmptyInput);
    }

    let mut out = Vec::with_capacity(frames);
    let phase_step = TAU * config.expected_carrier_hz / config.sample_rate_hz;
    for index in 0..frames {
        let phase = index as f32 * phase_step;
        out.push([
            amplitude * phase.sin(),
            amplitude * right_gain * (phase + right_phase_radians).sin(),
        ]);
    }
    Ok(out)
}

pub fn analyze_timecode_stereo(
    frames: &[[f32; 2]],
    config: TimecodeAnalysisConfig,
) -> Result<TimecodeAnalysis, TimecodeSignalError> {
    validate_config(config)?;
    if frames.is_empty() {
        return Err(TimecodeSignalError::EmptyInput);
    }

    let mut left_sum_sq = 0.0f64;
    let mut right_sum_sq = 0.0f64;
    let mut left_peak = 0.0f32;
    let mut right_peak = 0.0f32;
    let mut clipped_samples = 0usize;
    let mut left = Vec::with_capacity(frames.len());
    let mut right = Vec::with_capacity(frames.len());

    for [left_sample, right_sample] in frames.iter().copied() {
        let left_sample = sanitize_sample(left_sample);
        let right_sample = sanitize_sample(right_sample);
        left_sum_sq += f64::from(left_sample * left_sample);
        right_sum_sq += f64::from(right_sample * right_sample);
        left_peak = left_peak.max(left_sample.abs());
        right_peak = right_peak.max(right_sample.abs());
        clipped_samples += usize::from(left_sample.abs() >= 1.0);
        clipped_samples += usize::from(right_sample.abs() >= 1.0);
        left.push(left_sample);
        right.push(right_sample);
    }

    let left_rms = (left_sum_sq / frames.len() as f64).sqrt() as f32;
    let right_rms = (right_sum_sq / frames.len() as f64).sqrt() as f32;
    let balance_db_abs = if left_rms > 0.0 && right_rms > 0.0 {
        (20.0 * (left_rms / right_rms).log10()).abs()
    } else {
        f32::INFINITY
    };

    let left_edges = rising_zero_crossings(&left);
    let right_edges = rising_zero_crossings(&right);
    if left_edges.len() < 3 || right_edges.len() < 3 {
        return Err(TimecodeSignalError::TooFewEdges);
    }

    let left_periods = periods(&left_edges);
    let right_periods = periods(&right_edges);
    let left_mean_period = mean(&left_periods);
    let right_mean_period = mean(&right_periods);
    let left_carrier_hz = config.sample_rate_hz / left_mean_period;
    let right_carrier_hz = config.sample_rate_hz / right_mean_period;
    let left_error = ppm_abs(left_carrier_hz, config.expected_carrier_hz);
    let right_error = ppm_abs(right_carrier_hz, config.expected_carrier_hz);
    let frequency_error_ppm_abs = left_error.max(right_error);
    let jitter_frames_p95 = p95_abs_deviation(&left_periods, left_mean_period)
        .max(p95_abs_deviation(&right_periods, right_mean_period));
    let abs_correlation = pearson_correlation(&left, &right).abs();

    let pass = left_rms >= config.min_rms
        && right_rms >= config.min_rms
        && balance_db_abs <= config.max_balance_db
        && frequency_error_ppm_abs <= config.max_frequency_error_ppm
        && jitter_frames_p95 <= config.max_jitter_frames_p95
        && abs_correlation >= config.min_abs_correlation
        && clipped_samples <= config.max_clipped_samples;

    Ok(TimecodeAnalysis {
        frames: frames.len(),
        left_rms,
        right_rms,
        left_peak,
        right_peak,
        balance_db_abs,
        left_carrier_hz,
        right_carrier_hz,
        frequency_error_ppm_abs,
        jitter_frames_p95,
        abs_correlation,
        clipped_samples,
        left_rising_edges: left_edges.len(),
        right_rising_edges: right_edges.len(),
        pass,
    })
}

fn validate_config(config: TimecodeAnalysisConfig) -> Result<(), TimecodeSignalError> {
    if !config.sample_rate_hz.is_finite()
        || config.sample_rate_hz <= 0.0
        || !config.expected_carrier_hz.is_finite()
        || config.expected_carrier_hz <= 0.0
        || config.expected_carrier_hz >= config.sample_rate_hz * 0.5
        || !config.min_rms.is_finite()
        || config.min_rms < 0.0
        || !config.max_balance_db.is_finite()
        || config.max_balance_db < 0.0
        || !config.max_frequency_error_ppm.is_finite()
        || config.max_frequency_error_ppm < 0.0
        || !config.max_jitter_frames_p95.is_finite()
        || config.max_jitter_frames_p95 < 0.0
        || !config.min_abs_correlation.is_finite()
        || !(0.0..=1.0).contains(&config.min_abs_correlation)
    {
        return Err(TimecodeSignalError::InvalidConfig);
    }
    Ok(())
}

fn sanitize_sample(sample: f32) -> f32 {
    if sample.is_finite() {
        sample
    } else {
        0.0
    }
}

fn rising_zero_crossings(samples: &[f32]) -> Vec<f32> {
    let mut crossings = Vec::new();
    for index in 1..samples.len() {
        let previous = samples[index - 1];
        let current = samples[index];
        if previous < 0.0 && current >= 0.0 {
            let denom = current - previous;
            let fraction = if denom.abs() > f32::EPSILON {
                -previous / denom
            } else {
                0.0
            };
            crossings.push(index as f32 - 1.0 + fraction.clamp(0.0, 1.0));
        }
    }
    crossings
}

fn periods(crossings: &[f32]) -> Vec<f32> {
    crossings.windows(2).map(|pair| pair[1] - pair[0]).collect()
}

fn mean(values: &[f32]) -> f32 {
    values.iter().sum::<f32>() / values.len() as f32
}

fn ppm_abs(actual: f32, expected: f32) -> f32 {
    ((actual - expected) / expected).abs() * 1_000_000.0
}

fn p95_abs_deviation(values: &[f32], center: f32) -> f32 {
    if values.is_empty() {
        return f32::INFINITY;
    }
    let mut deviations: Vec<f32> = values.iter().map(|value| (value - center).abs()).collect();
    deviations.sort_by(|left, right| left.total_cmp(right));
    let index = ((deviations.len() - 1) as f32 * 0.95).ceil() as usize;
    deviations[index]
}

fn pearson_correlation(left: &[f32], right: &[f32]) -> f32 {
    if left.len() != right.len() || left.is_empty() {
        return 0.0;
    }
    let left_mean = mean(left);
    let right_mean = mean(right);
    let mut numerator = 0.0f64;
    let mut left_den = 0.0f64;
    let mut right_den = 0.0f64;
    for (left_sample, right_sample) in left.iter().zip(right.iter()) {
        let left_delta = f64::from(*left_sample - left_mean);
        let right_delta = f64::from(*right_sample - right_mean);
        numerator += left_delta * right_delta;
        left_den += left_delta * left_delta;
        right_den += right_delta * right_delta;
    }
    if left_den == 0.0 || right_den == 0.0 {
        0.0
    } else {
        (numerator / (left_den.sqrt() * right_den.sqrt())) as f32
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn synthetic_balanced_timecode_passes() {
        let config = TimecodeAnalysisConfig::default();
        let signal = synthetic_timecode_carrier(config, 2.0, 0.7, 1.0, 0.0).unwrap();
        let analysis = analyze_timecode_stereo(&signal, config).unwrap();

        assert!(analysis.pass);
        assert!(analysis.frequency_error_ppm_abs <= 1.0);
        assert!(analysis.jitter_frames_p95 <= 0.01);
        assert!(analysis.abs_correlation >= 0.999);
        assert_eq!(analysis.clipped_samples, 0);
    }

    #[test]
    fn wrong_expected_carrier_fails_frequency_gate() {
        let generator_config = TimecodeAnalysisConfig::default();
        let signal = synthetic_timecode_carrier(generator_config, 2.0, 0.7, 1.0, 0.0).unwrap();
        let analysis_config = TimecodeAnalysisConfig {
            expected_carrier_hz: 1_050.0,
            ..generator_config
        };
        let analysis = analyze_timecode_stereo(&signal, analysis_config).unwrap();

        assert!(!analysis.pass);
        assert!(analysis.frequency_error_ppm_abs > analysis_config.max_frequency_error_ppm);
    }

    #[test]
    fn channel_imbalance_fails_balance_gate() {
        let config = TimecodeAnalysisConfig::default();
        let signal = synthetic_timecode_carrier(config, 2.0, 0.7, 0.25, 0.0).unwrap();
        let analysis = analyze_timecode_stereo(&signal, config).unwrap();

        assert!(!analysis.pass);
        assert!(analysis.balance_db_abs > config.max_balance_db);
    }

    #[test]
    fn clipping_fails_clip_gate() {
        let config = TimecodeAnalysisConfig::default();
        let signal = synthetic_timecode_carrier(config, 2.0, 1.2, 1.0, 0.0).unwrap();
        let analysis = analyze_timecode_stereo(&signal, config).unwrap();

        assert!(!analysis.pass);
        assert!(analysis.clipped_samples > 0);
    }
}
