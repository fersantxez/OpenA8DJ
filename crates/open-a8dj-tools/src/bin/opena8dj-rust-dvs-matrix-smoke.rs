#![forbid(unsafe_code)]

use open_a8dj_core::input::{
    decode_mode2_input_bytes, HardwareInputProfile, Mode2InputDecodeConfig,
};
use open_a8dj_core::mode2::{
    max_transfers_for_comparison, pack_transfers, DEFAULT_START_BYTE, DEFAULT_TRANSFER_BYTES,
};
use open_a8dj_core::sample::I24ByteOrder;
use open_a8dj_core::timecode::{analyze_timecode_stereo, TimecodeAnalysisConfig};
use open_a8dj_core::topology::CHANNELS;
use std::env;
use std::f32::consts::TAU;
use std::process;

#[derive(Debug)]
struct Args {
    sample_rate_hz: f32,
    seconds: f32,
    deck_a_carrier_hz: f32,
    deck_b_carrier_hz: f32,
    amplitude: f32,
    max_leakage_rms: f32,
    max_frequency_error_ppm: f32,
    max_jitter_frames_p95: f32,
    min_abs_correlation: f32,
    json_summary: bool,
}

impl Default for Args {
    fn default() -> Self {
        let timecode = TimecodeAnalysisConfig::default();
        Self {
            sample_rate_hz: 48_000.0,
            seconds: 6.0,
            deck_a_carrier_hz: 1_000.0,
            deck_b_carrier_hz: 1_200.0,
            amplitude: 0.7,
            max_leakage_rms: 0.000_1,
            max_frequency_error_ppm: timecode.max_frequency_error_ppm,
            max_jitter_frames_p95: timecode.max_jitter_frames_p95,
            min_abs_correlation: timecode.min_abs_correlation,
            json_summary: false,
        }
    }
}

#[derive(Clone, Copy, Debug)]
struct PairAnalysis {
    status: &'static str,
    rms: f32,
    carrier_hz: f32,
    frequency_error_ppm_abs: f32,
    jitter_frames_p95: f32,
    abs_correlation: f32,
    clipped_samples: usize,
}

fn usage() -> &'static str {
    "usage: opena8dj-rust-dvs-matrix-smoke [--rate HZ] [--seconds F] [--deck-a-carrier-hz F] [--deck-b-carrier-hz F] [--amplitude F] [--max-leakage-rms F] [--max-frequency-error-ppm F] [--max-jitter-frames-p95 F] [--min-abs-correlation F] [--json-summary]"
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
            "--rate" => args.sample_rate_hz = parse_f32("--rate", iter.next())?,
            "--seconds" => args.seconds = parse_f32("--seconds", iter.next())?,
            "--deck-a-carrier-hz" => {
                args.deck_a_carrier_hz = parse_f32("--deck-a-carrier-hz", iter.next())?;
            }
            "--deck-b-carrier-hz" => {
                args.deck_b_carrier_hz = parse_f32("--deck-b-carrier-hz", iter.next())?;
            }
            "--amplitude" => args.amplitude = parse_f32("--amplitude", iter.next())?,
            "--max-leakage-rms" => {
                args.max_leakage_rms = parse_f32("--max-leakage-rms", iter.next())?;
            }
            "--max-frequency-error-ppm" => {
                args.max_frequency_error_ppm = parse_f32("--max-frequency-error-ppm", iter.next())?;
            }
            "--max-jitter-frames-p95" => {
                args.max_jitter_frames_p95 = parse_f32("--max-jitter-frames-p95", iter.next())?;
            }
            "--min-abs-correlation" => {
                args.min_abs_correlation = parse_f32("--min-abs-correlation", iter.next())?;
            }
            "--json-summary" => args.json_summary = true,
            "--help" | "-h" => {
                println!("{}", usage());
                process::exit(0);
            }
            _ => return Err(format!("unknown argument {arg:?}\n{}", usage())),
        }
    }

    if !args.sample_rate_hz.is_finite()
        || args.sample_rate_hz <= 0.0
        || !args.seconds.is_finite()
        || args.seconds <= 0.0
        || !args.amplitude.is_finite()
        || args.amplitude <= 0.0
        || !args.max_leakage_rms.is_finite()
        || args.max_leakage_rms < 0.0
    {
        return Err("invalid rate, seconds, amplitude, or leakage threshold".to_string());
    }
    Ok(args)
}

fn build_dvs_frames(args: &Args) -> Vec<[f32; CHANNELS]> {
    let frames = (args.seconds * args.sample_rate_hz).round() as usize;
    let mut out = Vec::with_capacity(frames);
    let deck_a_step = TAU * args.deck_a_carrier_hz / args.sample_rate_hz;
    let deck_b_step = TAU * args.deck_b_carrier_hz / args.sample_rate_hz;
    for index in 0..frames {
        let mut frame = [0.0; CHANNELS];
        let deck_a = args.amplitude * (index as f32 * deck_a_step).sin();
        let deck_b = args.amplitude * (index as f32 * deck_b_step).sin();
        frame[0] = deck_a;
        frame[1] = deck_a;
        frame[2] = deck_b;
        frame[3] = deck_b;
        out.push(frame);
    }
    out
}

fn pair_frames(frames: &[[f32; CHANNELS]], pair_index: usize) -> Vec<[f32; 2]> {
    let left = pair_index * 2;
    let right = left + 1;
    frames
        .iter()
        .map(|frame| [frame[left], frame[right]])
        .collect()
}

fn pair_rms(frames: &[[f32; CHANNELS]], pair_index: usize) -> f32 {
    if frames.is_empty() {
        return 0.0;
    }
    let left = pair_index * 2;
    let right = left + 1;
    let mut sum = 0.0f64;
    for frame in frames {
        sum += f64::from(frame[left] * frame[left]);
        sum += f64::from(frame[right] * frame[right]);
    }
    (sum / (frames.len() * 2) as f64).sqrt() as f32
}

fn analyze_pair(
    frames: &[[f32; CHANNELS]],
    pair_index: usize,
    expected_carrier_hz: f32,
    args: &Args,
) -> Result<PairAnalysis, String> {
    let config = TimecodeAnalysisConfig {
        sample_rate_hz: args.sample_rate_hz,
        expected_carrier_hz,
        max_frequency_error_ppm: args.max_frequency_error_ppm,
        max_jitter_frames_p95: args.max_jitter_frames_p95,
        min_abs_correlation: args.min_abs_correlation,
        ..TimecodeAnalysisConfig::default()
    };
    let stereo = pair_frames(frames, pair_index);
    let analysis = analyze_timecode_stereo(&stereo, config)
        .map_err(|error| format!("pair {pair_index} analysis failed: {error:?}"))?;
    Ok(PairAnalysis {
        status: if analysis.pass { "PASS" } else { "FAIL" },
        rms: (analysis.left_rms + analysis.right_rms) * 0.5,
        carrier_hz: (analysis.left_carrier_hz + analysis.right_carrier_hz) * 0.5,
        frequency_error_ppm_abs: analysis.frequency_error_ppm_abs,
        jitter_frames_p95: analysis.jitter_frames_p95,
        abs_correlation: analysis.abs_correlation,
        clipped_samples: analysis.clipped_samples,
    })
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

    let source_frames = build_dvs_frames(&args);
    let transfers = match max_transfers_for_comparison(source_frames.len(), DEFAULT_TRANSFER_BYTES)
    {
        Ok(transfers) => transfers,
        Err(error) => {
            eprintln!("error: transfer planning failed: {error:?}");
            process::exit(2);
        }
    };
    let packed = match pack_transfers(
        &source_frames,
        DEFAULT_START_BYTE,
        DEFAULT_TRANSFER_BYTES,
        transfers,
        1.0,
        I24ByteOrder::BigEndian,
    ) {
        Ok(packed) => packed,
        Err(error) => {
            eprintln!("error: mode2 pack failed: {error:?}");
            process::exit(2);
        }
    };

    let profile = HardwareInputProfile::timecode_vinyl();
    let decoded = match decode_mode2_input_bytes(
        &packed,
        &Mode2InputDecodeConfig {
            input_decode_enabled: profile.input_decode_enabled,
            routing: profile.routing,
            ..Mode2InputDecodeConfig::default()
        },
    ) {
        Ok(decoded) => decoded,
        Err(error) => {
            eprintln!("error: mode2 input decode failed: {error:?}");
            process::exit(2);
        }
    };

    let deck_a = match analyze_pair(&decoded.frames, 0, args.deck_a_carrier_hz, &args) {
        Ok(analysis) => analysis,
        Err(error) => {
            eprintln!("error: {error}");
            process::exit(1);
        }
    };
    let deck_b = match analyze_pair(&decoded.frames, 1, args.deck_b_carrier_hz, &args) {
        Ok(analysis) => analysis,
        Err(error) => {
            eprintln!("error: {error}");
            process::exit(1);
        }
    };
    let input_c_rms = pair_rms(&decoded.frames, 2);
    let input_d_rms = pair_rms(&decoded.frames, 3);
    let leakage_ok = input_c_rms <= args.max_leakage_rms && input_d_rms <= args.max_leakage_rms;
    let stats_ok =
        decoded.check_errors == 0 && decoded.panic_flags == 0 && !decoded.frames.is_empty();
    let ok = deck_a.status == "PASS" && deck_b.status == "PASS" && leakage_ok && stats_ok;
    let status = if ok { "PASS" } else { "FAIL" };

    if args.json_summary {
        println!(
            "{{\"schema\":\"open-a8dj-rust.dvs-matrix-smoke.v1\",\"status\":\"{}\",\"sample_rate_hz\":{},\"seconds\":{},\"frames\":{},\"decoded_frames\":{},\"raw_decoded_frames\":{},\"check_errors\":{},\"panic_flags\":{},\"deck_a_status\":\"{}\",\"deck_a_rms\":{:.9},\"deck_a_carrier_hz\":{:.6},\"deck_a_frequency_error_ppm_abs\":{:.6},\"deck_a_jitter_frames_p95\":{:.6},\"deck_a_abs_correlation\":{:.9},\"deck_a_clipped_samples\":{},\"deck_b_status\":\"{}\",\"deck_b_rms\":{:.9},\"deck_b_carrier_hz\":{:.6},\"deck_b_frequency_error_ppm_abs\":{:.6},\"deck_b_jitter_frames_p95\":{:.6},\"deck_b_abs_correlation\":{:.9},\"deck_b_clipped_samples\":{},\"input_c_rms\":{:.9},\"input_d_rms\":{:.9},\"max_leakage_rms\":{},\"input_mode_raw\":{},\"input_decode_enabled\":{}}}",
            status,
            args.sample_rate_hz,
            args.seconds,
            source_frames.len(),
            decoded.frames.len(),
            decoded.raw_decoded_frames,
            decoded.check_errors,
            decoded.panic_flags,
            deck_a.status,
            deck_a.rms,
            deck_a.carrier_hz,
            deck_a.frequency_error_ppm_abs,
            deck_a.jitter_frames_p95,
            deck_a.abs_correlation,
            deck_a.clipped_samples,
            deck_b.status,
            deck_b.rms,
            deck_b.carrier_hz,
            deck_b.frequency_error_ppm_abs,
            deck_b.jitter_frames_p95,
            deck_b.abs_correlation,
            deck_b.clipped_samples,
            input_c_rms,
            input_d_rms,
            args.max_leakage_rms,
            profile.input_mode.raw_value(),
            profile.input_decode_enabled
        );
    } else {
        println!(
            "{} sample_rate_hz={} seconds={} frames={} decoded_frames={} raw_decoded_frames={} check_errors={} panic_flags={} deck_a_status={} deck_a_rms={:.9} deck_a_carrier_hz={:.6} deck_a_frequency_error_ppm_abs={:.6} deck_a_jitter_frames_p95={:.6} deck_a_abs_correlation={:.9} deck_b_status={} deck_b_rms={:.9} deck_b_carrier_hz={:.6} deck_b_frequency_error_ppm_abs={:.6} deck_b_jitter_frames_p95={:.6} deck_b_abs_correlation={:.9} input_c_rms={:.9} input_d_rms={:.9} max_leakage_rms={} input_mode_raw={} input_decode_enabled={}",
            status,
            args.sample_rate_hz,
            args.seconds,
            source_frames.len(),
            decoded.frames.len(),
            decoded.raw_decoded_frames,
            decoded.check_errors,
            decoded.panic_flags,
            deck_a.status,
            deck_a.rms,
            deck_a.carrier_hz,
            deck_a.frequency_error_ppm_abs,
            deck_a.jitter_frames_p95,
            deck_a.abs_correlation,
            deck_b.status,
            deck_b.rms,
            deck_b.carrier_hz,
            deck_b.frequency_error_ppm_abs,
            deck_b.jitter_frames_p95,
            deck_b.abs_correlation,
            input_c_rms,
            input_d_rms,
            args.max_leakage_rms,
            profile.input_mode.raw_value(),
            profile.input_decode_enabled
        );
    }

    process::exit(if ok { 0 } else { 1 });
}
