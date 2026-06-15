#![forbid(unsafe_code)]

use open_a8dj_core::mode2::{
    decode_mode2_usb_bytes, expected_s24_frame, pack_transfers, pack_until_comparable,
    synthetic_frame, DEFAULT_START_BYTE, DEFAULT_TRANSFER_BYTES, FRAME_BYTES_PER_STREAM,
};
use open_a8dj_core::sample::I24ByteOrder;
use open_a8dj_core::topology::CHANNELS;
use std::env;
use std::fs;
use std::path::PathBuf;
use std::process;

#[derive(Debug)]
struct Args {
    frames: usize,
    start_byte: usize,
    transfer_bytes: usize,
    transfers: Option<usize>,
    gain: f32,
    byte_order: I24ByteOrder,
    output: Option<PathBuf>,
    json_summary: bool,
}

impl Default for Args {
    fn default() -> Self {
        Self {
            frames: 64,
            start_byte: DEFAULT_START_BYTE,
            transfer_bytes: DEFAULT_TRANSFER_BYTES,
            transfers: None,
            gain: 1.0,
            byte_order: I24ByteOrder::BigEndian,
            output: None,
            json_summary: false,
        }
    }
}

fn usage() -> &'static str {
    "usage: opena8dj-rust-pack-sim [--frames N] [--start-byte 0..5] [--transfer-bytes N] [--transfers N] [--gain F] [--byte-order big|native] [--output PATH] [--json-summary]"
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
            "--frames" => args.frames = parse_usize("--frames", iter.next())?,
            "--start-byte" => args.start_byte = parse_usize("--start-byte", iter.next())?,
            "--transfer-bytes" => {
                args.transfer_bytes = parse_usize("--transfer-bytes", iter.next())?
            }
            "--transfers" => args.transfers = Some(parse_usize("--transfers", iter.next())?),
            "--gain" => args.gain = parse_f32("--gain", iter.next())?,
            "--byte-order" => {
                let value = iter
                    .next()
                    .ok_or_else(|| "--byte-order needs a value".to_string())?;
                args.byte_order = match value.as_str() {
                    "big" => I24ByteOrder::BigEndian,
                    "native" | "little" => I24ByteOrder::NativeLittleEndian,
                    _ => return Err(format!("--byte-order expects big or native, got {value:?}")),
                };
            }
            "--output" => {
                let value = iter
                    .next()
                    .ok_or_else(|| "--output needs a value".to_string())?;
                args.output = Some(PathBuf::from(value));
            }
            "--json-summary" => args.json_summary = true,
            "--help" | "-h" => {
                println!("{}", usage());
                process::exit(0);
            }
            _ => return Err(format!("unknown argument {arg:?}\n{}", usage())),
        }
    }

    if args.frames == 0 {
        return Err("--frames must be positive".to_string());
    }
    if args.start_byte >= FRAME_BYTES_PER_STREAM {
        return Err("--start-byte must be between 0 and 5".to_string());
    }
    if args.start_byte != 0 && args.frames < 2 {
        return Err("--frames must be at least 2 when --start-byte is nonzero".to_string());
    }
    Ok(args)
}

fn byte_order_name(byte_order: I24ByteOrder) -> &'static str {
    match byte_order {
        I24ByteOrder::BigEndian => "big",
        I24ByteOrder::NativeLittleEndian => "native",
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

    let frames: Vec<[f32; CHANNELS]> = (0..args.frames).map(synthetic_frame).collect();
    let source_start_frame = if args.start_byte == 0 { 0 } else { 1 };
    let expected_count = args.frames - source_start_frame;
    let (packed, decoded) = match args.transfers {
        Some(transfers) => {
            let packed = match pack_transfers(
                &frames,
                args.start_byte,
                args.transfer_bytes,
                transfers,
                args.gain,
                args.byte_order,
            ) {
                Ok(packed) => packed,
                Err(error) => {
                    eprintln!("error: mode2 pack failed: {error:?}");
                    process::exit(2);
                }
            };
            let decoded = match decode_mode2_usb_bytes(
                &packed,
                args.start_byte,
                args.transfer_bytes,
                args.byte_order,
            ) {
                Ok(decoded) => decoded,
                Err(error) => {
                    eprintln!("error: mode2 decode failed: {error:?}");
                    process::exit(2);
                }
            };
            (packed, decoded)
        }
        None => match pack_until_comparable(
            &frames,
            args.start_byte,
            args.transfer_bytes,
            args.gain,
            expected_count,
            args.byte_order,
        ) {
            Ok(result) => result,
            Err(error) => {
                eprintln!("error: mode2 comparable pack failed: {error:?}");
                process::exit(2);
            }
        },
    };
    let transfers = packed.len() / args.transfer_bytes;
    let compared_frames = expected_count.min(decoded.frames.len());
    let mut mismatches = 0usize;
    for index in 0..compared_frames {
        let expected = expected_s24_frame(&frames[source_start_frame + index], args.gain);
        if decoded.frames[index] != expected {
            mismatches += 1;
        }
    }
    let ok = decoded.frames.len() >= expected_count
        && compared_frames == expected_count
        && decoded.check_errors == 0
        && decoded.panic_flags == 0
        && mismatches == 0;

    if let Some(path) = &args.output {
        if let Err(error) = fs::write(path, &packed) {
            eprintln!("error: could not write {}: {error}", path.display());
            process::exit(1);
        }
    }

    if args.json_summary {
        println!(
            "{{\"schema\":\"open-a8dj-rust.pack-sim.v1\",\"status\":\"{}\",\"frames\":{},\"source_start_frame\":{},\"packed_bytes\":{},\"transfer_bytes\":{},\"transfers\":{},\"start_byte\":{},\"byte_order\":\"{}\",\"decoded_frames\":{},\"compared_frames\":{},\"checks\":{},\"check_errors\":{},\"panic_flags\":{},\"sample_bytes\":{},\"mismatches\":{}}}",
            if ok { "PASS" } else { "FAIL" },
            args.frames,
            source_start_frame,
            packed.len(),
            args.transfer_bytes,
            transfers,
            args.start_byte,
            byte_order_name(args.byte_order),
            decoded.frames.len(),
            compared_frames,
            decoded.checks,
            decoded.check_errors,
            decoded.panic_flags,
            decoded.sample_bytes,
            mismatches
        );
    } else {
        println!(
            "{} frames={} source_start_frame={} packed_bytes={} transfer_bytes={} transfers={} start_byte={} byte_order={} decoded_frames={} compared_frames={} checks={} check_errors={} panic_flags={} sample_bytes={} mismatches={}",
            if ok { "PASS" } else { "FAIL" },
            args.frames,
            source_start_frame,
            packed.len(),
            args.transfer_bytes,
            transfers,
            args.start_byte,
            byte_order_name(args.byte_order),
            decoded.frames.len(),
            compared_frames,
            decoded.checks,
            decoded.check_errors,
            decoded.panic_flags,
            decoded.sample_bytes,
            mismatches
        );
    }

    process::exit(if ok { 0 } else { 1 });
}
