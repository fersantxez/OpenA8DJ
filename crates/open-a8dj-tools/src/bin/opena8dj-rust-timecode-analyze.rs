#![forbid(unsafe_code)]

use open_a8dj_core::timecode::{
    analyze_timecode_stereo, synthetic_timecode_carrier, TimecodeAnalysisConfig,
    TimecodeSignalError,
};
use std::env;
use std::fs;
use std::path::PathBuf;
use std::process;

#[derive(Debug)]
struct Args {
    raw_f32: Option<PathBuf>,
    channels: usize,
    left_channel: usize,
    right_channel: usize,
    seconds: f32,
    sample_rate_hz: f32,
    expected_carrier_hz: f32,
    amplitude: f32,
    right_gain: f32,
    right_phase_degrees: f32,
    min_rms: f32,
    max_balance_db: f32,
    max_frequency_error_ppm: f32,
    max_jitter_frames_p95: f32,
    min_abs_correlation: f32,
    max_clipped_samples: usize,
    json_summary: bool,
}

impl Default for Args {
    fn default() -> Self {
        let config = TimecodeAnalysisConfig::default();
        Self {
            raw_f32: None,
            channels: 2,
            left_channel: 0,
            right_channel: 1,
            seconds: 6.0,
            sample_rate_hz: config.sample_rate_hz,
            expected_carrier_hz: config.expected_carrier_hz,
            amplitude: 0.7,
            right_gain: 1.0,
            right_phase_degrees: 0.0,
            min_rms: config.min_rms,
            max_balance_db: config.max_balance_db,
            max_frequency_error_ppm: config.max_frequency_error_ppm,
            max_jitter_frames_p95: config.max_jitter_frames_p95,
            min_abs_correlation: config.min_abs_correlation,
            max_clipped_samples: config.max_clipped_samples,
            json_summary: false,
        }
    }
}

fn usage() -> &'static str {
    "usage: opena8dj-rust-timecode-analyze [--raw-f32 PATH --channels N --left-channel N --right-channel N] [--seconds F] [--rate HZ] [--carrier-hz F] [--amplitude F] [--right-gain F] [--right-phase-degrees F] [--min-rms F] [--max-balance-db F] [--max-frequency-error-ppm F] [--max-jitter-frames-p95 F] [--min-abs-correlation F] [--max-clipped-samples N] [--json-summary]"
}

fn parse_usize(name: &str, value: Option<String>) -> Result<usize, String> {
    let value = value.ok_or_else(|| format!("{name} needs a value"))?;
    value
        .parse::<usize>()
        .map_err(|_| format!("{name} expects an unsigned integer, got {value:?}"))
}

fn parse_f32(name: &str, value: Option<String>) -> Result<f32, String> {
    let value = value.ok_or_else(|| format!("{name} needs a value"))?;
    value
        .parse::<f32>()
        .map_err(|_| format!("{name} expects a float, got {value:?}"))
}

fn parse_args() -> Result<Args, String> {
    let mut args = Args::default();
    let mut iter = env::args().skip(1);
    while let Some(arg) = iter.next() {
        match arg.as_str() {
            "--raw-f32" => {
                args.raw_f32 = Some(PathBuf::from(
                    iter.next()
                        .ok_or_else(|| "--raw-f32 needs a value".to_string())?,
                ));
            }
            "--channels" => args.channels = parse_usize("--channels", iter.next())?,
            "--left-channel" => {
                args.left_channel = parse_usize("--left-channel", iter.next())?;
            }
            "--right-channel" => {
                args.right_channel = parse_usize("--right-channel", iter.next())?;
            }
            "--seconds" => args.seconds = parse_f32("--seconds", iter.next())?,
            "--rate" => args.sample_rate_hz = parse_f32("--rate", iter.next())?,
            "--carrier-hz" => args.expected_carrier_hz = parse_f32("--carrier-hz", iter.next())?,
            "--amplitude" => args.amplitude = parse_f32("--amplitude", iter.next())?,
            "--right-gain" => args.right_gain = parse_f32("--right-gain", iter.next())?,
            "--right-phase-degrees" => {
                args.right_phase_degrees = parse_f32("--right-phase-degrees", iter.next())?;
            }
            "--min-rms" => args.min_rms = parse_f32("--min-rms", iter.next())?,
            "--max-balance-db" => args.max_balance_db = parse_f32("--max-balance-db", iter.next())?,
            "--max-frequency-error-ppm" => {
                args.max_frequency_error_ppm = parse_f32("--max-frequency-error-ppm", iter.next())?;
            }
            "--max-jitter-frames-p95" => {
                args.max_jitter_frames_p95 = parse_f32("--max-jitter-frames-p95", iter.next())?;
            }
            "--min-abs-correlation" => {
                args.min_abs_correlation = parse_f32("--min-abs-correlation", iter.next())?;
            }
            "--max-clipped-samples" => {
                args.max_clipped_samples = parse_usize("--max-clipped-samples", iter.next())?;
            }
            "--json-summary" => args.json_summary = true,
            "--help" | "-h" => {
                println!("{}", usage());
                process::exit(0);
            }
            _ => return Err(format!("unknown argument {arg:?}\n{}", usage())),
        }
    }

    if args.channels == 0 {
        return Err("--channels must be positive".to_string());
    }
    if args.left_channel >= args.channels || args.right_channel >= args.channels {
        return Err("selected stereo channels must be within --channels".to_string());
    }
    if args.left_channel == args.right_channel {
        return Err("left and right channels must be different".to_string());
    }
    Ok(args)
}

fn config(args: &Args) -> TimecodeAnalysisConfig {
    TimecodeAnalysisConfig {
        sample_rate_hz: args.sample_rate_hz,
        expected_carrier_hz: args.expected_carrier_hz,
        min_rms: args.min_rms,
        max_balance_db: args.max_balance_db,
        max_frequency_error_ppm: args.max_frequency_error_ppm,
        max_jitter_frames_p95: args.max_jitter_frames_p95,
        min_abs_correlation: args.min_abs_correlation,
        max_clipped_samples: args.max_clipped_samples,
    }
}

fn load_raw_f32(args: &Args) -> Result<Vec<[f32; 2]>, String> {
    let path = args
        .raw_f32
        .as_ref()
        .ok_or_else(|| "missing --raw-f32".to_string())?;
    let bytes =
        fs::read(path).map_err(|error| format!("could not read {}: {error}", path.display()))?;
    let bytes_per_frame = args
        .channels
        .checked_mul(std::mem::size_of::<f32>())
        .ok_or_else(|| "channel count overflow".to_string())?;
    if bytes_per_frame == 0 || bytes.len() < bytes_per_frame {
        return Err("raw f32 input is too short".to_string());
    }

    let usable = bytes.len() / bytes_per_frame;
    let mut frames = Vec::with_capacity(usable);
    for frame_index in 0..usable {
        let base = frame_index * bytes_per_frame;
        let left_offset = base + args.left_channel * 4;
        let right_offset = base + args.right_channel * 4;
        let left = f32::from_le_bytes(
            bytes[left_offset..left_offset + 4]
                .try_into()
                .expect("slice length"),
        );
        let right = f32::from_le_bytes(
            bytes[right_offset..right_offset + 4]
                .try_into()
                .expect("slice length"),
        );
        frames.push([left, right]);
    }
    Ok(frames)
}

fn make_input(args: &Args, config: TimecodeAnalysisConfig) -> Result<Vec<[f32; 2]>, String> {
    if args.raw_f32.is_some() {
        load_raw_f32(args)
    } else {
        synthetic_timecode_carrier(
            config,
            args.seconds,
            args.amplitude,
            args.right_gain,
            args.right_phase_degrees.to_radians(),
        )
        .map_err(|error| format!("synthetic generation failed: {error:?}"))
    }
}

fn error_status(error: TimecodeSignalError) -> &'static str {
    match error {
        TimecodeSignalError::InvalidConfig => "INVALID_CONFIG",
        TimecodeSignalError::EmptyInput => "EMPTY_INPUT",
        TimecodeSignalError::TooFewEdges => "TOO_FEW_EDGES",
    }
}

fn main() {
    let args = match parse_args() {
        Ok(args) => args,
        Err(error) => {
            eprintln!("error: {error}");
            eprintln!("{}", usage());
            process::exit(2);
        }
    };
    let config = config(&args);
    let frames = match make_input(&args, config) {
        Ok(frames) => frames,
        Err(error) => {
            eprintln!("error: {error}");
            process::exit(2);
        }
    };

    let analysis = match analyze_timecode_stereo(&frames, config) {
        Ok(analysis) => analysis,
        Err(error) => {
            if args.json_summary {
                println!(
                    "{{\"schema\":\"open-a8dj-rust.timecode-analyze.v1\",\"status\":\"FAIL\",\"reason\":\"{}\"}}",
                    error_status(error)
                );
            } else {
                println!("FAIL reason={}", error_status(error));
            }
            process::exit(1);
        }
    };
    let status = if analysis.pass { "PASS" } else { "FAIL" };
    let source = if args.raw_f32.is_some() {
        "raw-f32"
    } else {
        "synthetic"
    };

    if args.json_summary {
        println!(
            "{{\"schema\":\"open-a8dj-rust.timecode-analyze.v1\",\"status\":\"{}\",\"source\":\"{}\",\"frames\":{},\"sample_rate_hz\":{},\"expected_carrier_hz\":{},\"left_rms\":{:.9},\"right_rms\":{:.9},\"left_peak\":{:.9},\"right_peak\":{:.9},\"balance_db_abs\":{:.6},\"left_carrier_hz\":{:.6},\"right_carrier_hz\":{:.6},\"frequency_error_ppm_abs\":{:.6},\"jitter_frames_p95\":{:.6},\"abs_correlation\":{:.9},\"clipped_samples\":{},\"left_rising_edges\":{},\"right_rising_edges\":{},\"min_rms\":{},\"max_balance_db\":{},\"max_frequency_error_ppm\":{},\"max_jitter_frames_p95\":{},\"min_abs_correlation\":{},\"max_clipped_samples\":{}}}",
            status,
            source,
            analysis.frames,
            config.sample_rate_hz,
            config.expected_carrier_hz,
            analysis.left_rms,
            analysis.right_rms,
            analysis.left_peak,
            analysis.right_peak,
            analysis.balance_db_abs,
            analysis.left_carrier_hz,
            analysis.right_carrier_hz,
            analysis.frequency_error_ppm_abs,
            analysis.jitter_frames_p95,
            analysis.abs_correlation,
            analysis.clipped_samples,
            analysis.left_rising_edges,
            analysis.right_rising_edges,
            config.min_rms,
            config.max_balance_db,
            config.max_frequency_error_ppm,
            config.max_jitter_frames_p95,
            config.min_abs_correlation,
            config.max_clipped_samples
        );
    } else {
        println!(
            "{} source={} frames={} sample_rate_hz={} expected_carrier_hz={} left_rms={:.9} right_rms={:.9} left_peak={:.9} right_peak={:.9} balance_db_abs={:.6} left_carrier_hz={:.6} right_carrier_hz={:.6} frequency_error_ppm_abs={:.6} jitter_frames_p95={:.6} abs_correlation={:.9} clipped_samples={} left_rising_edges={} right_rising_edges={}",
            status,
            source,
            analysis.frames,
            config.sample_rate_hz,
            config.expected_carrier_hz,
            analysis.left_rms,
            analysis.right_rms,
            analysis.left_peak,
            analysis.right_peak,
            analysis.balance_db_abs,
            analysis.left_carrier_hz,
            analysis.right_carrier_hz,
            analysis.frequency_error_ppm_abs,
            analysis.jitter_frames_p95,
            analysis.abs_correlation,
            analysis.clipped_samples,
            analysis.left_rising_edges,
            analysis.right_rising_edges
        );
    }

    process::exit(if analysis.pass { 0 } else { 1 });
}
