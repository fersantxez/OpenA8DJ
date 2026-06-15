#![forbid(unsafe_code)]

use open_a8dj_core::mode2::{
    synthetic_frame, validate_start_byte, validate_transfer_bytes, Mode2OutputPacker,
    OutputFrameProvider, DEFAULT_START_BYTE, DEFAULT_TRANSFER_BYTES,
};
use open_a8dj_core::sample::I24ByteOrder;
use open_a8dj_core::topology::CHANNELS;
use std::env;
use std::process;
use std::time::Instant;

#[derive(Debug)]
struct Args {
    transfers: usize,
    warmup_transfers: usize,
    transfer_bytes: usize,
    start_byte: usize,
    gain: f32,
    byte_order: I24ByteOrder,
    min_mib_per_second: f64,
    min_frames_per_second: f64,
    json_summary: bool,
}

impl Default for Args {
    fn default() -> Self {
        Self {
            transfers: 20_000,
            warmup_transfers: 1_000,
            transfer_bytes: DEFAULT_TRANSFER_BYTES,
            start_byte: DEFAULT_START_BYTE,
            gain: 0.5,
            byte_order: I24ByteOrder::BigEndian,
            min_mib_per_second: 100.0,
            min_frames_per_second: 1_000_000.0,
            json_summary: false,
        }
    }
}

struct SyntheticProvider {
    next: usize,
}

impl OutputFrameProvider for SyntheticProvider {
    fn next_frame(&mut self) -> [f32; CHANNELS] {
        let frame = synthetic_frame(self.next);
        self.next += 1;
        frame
    }
}

struct BenchResult {
    transfers: usize,
    transfer_bytes: usize,
    packed_bytes: usize,
    frames_loaded: usize,
    elapsed_seconds: f64,
    transfers_per_second: f64,
    frames_per_second: f64,
    mib_per_second: f64,
    ns_per_transfer: f64,
    checksum: u64,
}

fn usage() -> &'static str {
    "usage: opena8dj-rust-pack-bench [--transfers N] [--warmup-transfers N] [--transfer-bytes N] [--start-byte 0..5] [--gain F] [--byte-order big|native] [--min-mib-per-second F] [--min-frames-per-second F] [--json-summary]"
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

fn parse_f64(name: &str, value: Option<String>) -> Result<f64, String> {
    let value = value.ok_or_else(|| format!("{name} needs a value"))?;
    value
        .parse::<f64>()
        .map_err(|_| format!("{name} expects a float, got {value:?}"))
}

fn parse_args() -> Result<Args, String> {
    let mut args = Args::default();
    let mut iter = env::args().skip(1);
    while let Some(arg) = iter.next() {
        match arg.as_str() {
            "--transfers" => args.transfers = parse_usize("--transfers", iter.next())?,
            "--warmup-transfers" => {
                args.warmup_transfers = parse_usize("--warmup-transfers", iter.next())?
            }
            "--transfer-bytes" => {
                args.transfer_bytes = parse_usize("--transfer-bytes", iter.next())?
            }
            "--start-byte" => args.start_byte = parse_usize("--start-byte", iter.next())?,
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
            "--min-mib-per-second" => {
                args.min_mib_per_second = parse_f64("--min-mib-per-second", iter.next())?
            }
            "--min-frames-per-second" => {
                args.min_frames_per_second = parse_f64("--min-frames-per-second", iter.next())?
            }
            "--json-summary" => args.json_summary = true,
            "--help" | "-h" => {
                println!("{}", usage());
                process::exit(0);
            }
            _ => return Err(format!("unknown argument {arg:?}\n{}", usage())),
        }
    }

    if args.transfers == 0 {
        return Err("--transfers must be positive".to_string());
    }
    if !args.gain.is_finite() || args.gain < 0.0 {
        return Err("--gain must be a finite non-negative float".to_string());
    }
    if args.min_mib_per_second < 0.0 || args.min_frames_per_second < 0.0 {
        return Err("minimum throughput values must be non-negative".to_string());
    }
    validate_start_byte(args.start_byte).map_err(|_| "--start-byte must be between 0 and 5")?;
    validate_transfer_bytes(args.transfer_bytes)
        .map_err(|_| "--transfer-bytes must be positive and divisible by 16")?;
    Ok(args)
}

fn byte_order_name(byte_order: I24ByteOrder) -> &'static str {
    match byte_order {
        I24ByteOrder::BigEndian => "big",
        I24ByteOrder::NativeLittleEndian => "native",
    }
}

fn checksum_transfer(output: &[u8]) -> u64 {
    output
        .iter()
        .fold(0u64, |acc, byte| acc.rotate_left(5) ^ u64::from(*byte))
}

fn run_pack_bench(args: &Args, transfers: usize) -> Result<BenchResult, String> {
    let mut packer = Mode2OutputPacker::new(args.start_byte, args.gain, args.byte_order)
        .map_err(|error| format!("mode2 packer failed: {error:?}"))?;
    let mut provider = SyntheticProvider { next: 0 };
    let mut output = vec![0u8; args.transfer_bytes];
    let mut frames_loaded = 0usize;
    let mut checksum = 0u64;

    let started = Instant::now();
    for _ in 0..transfers {
        frames_loaded += packer.fill_with_provider(&mut provider, &mut output);
        checksum = checksum.wrapping_add(checksum_transfer(&output));
    }
    let elapsed_seconds = started.elapsed().as_secs_f64().max(f64::EPSILON);
    let packed_bytes = args.transfer_bytes * transfers;
    let transfers_per_second = transfers as f64 / elapsed_seconds;
    let frames_per_second = frames_loaded as f64 / elapsed_seconds;
    let mib_per_second = packed_bytes as f64 / (1024.0 * 1024.0) / elapsed_seconds;
    let ns_per_transfer = elapsed_seconds * 1_000_000_000.0 / transfers as f64;

    Ok(BenchResult {
        transfers,
        transfer_bytes: args.transfer_bytes,
        packed_bytes,
        frames_loaded,
        elapsed_seconds,
        transfers_per_second,
        frames_per_second,
        mib_per_second,
        ns_per_transfer,
        checksum,
    })
}

fn print_result(args: &Args, result: &BenchResult, ok: bool) {
    if args.json_summary {
        println!(
            "{{\"schema\":\"open-a8dj-rust.pack-bench.v1\",\"status\":\"{}\",\"transfers\":{},\"transfer_bytes\":{},\"packed_bytes\":{},\"frames_loaded\":{},\"elapsed_seconds\":{:.9},\"transfers_per_second\":{:.3},\"frames_per_second\":{:.3},\"mib_per_second\":{:.3},\"ns_per_transfer\":{:.3},\"start_byte\":{},\"byte_order\":\"{}\",\"gain\":{},\"min_mib_per_second\":{},\"min_frames_per_second\":{},\"checksum\":{}}}",
            if ok { "PASS" } else { "FAIL" },
            result.transfers,
            result.transfer_bytes,
            result.packed_bytes,
            result.frames_loaded,
            result.elapsed_seconds,
            result.transfers_per_second,
            result.frames_per_second,
            result.mib_per_second,
            result.ns_per_transfer,
            args.start_byte,
            byte_order_name(args.byte_order),
            args.gain,
            args.min_mib_per_second,
            args.min_frames_per_second,
            result.checksum
        );
    } else {
        println!(
            "{} transfers={} transfer_bytes={} packed_bytes={} frames_loaded={} elapsed_seconds={:.9} transfers_per_second={:.3} frames_per_second={:.3} mib_per_second={:.3} ns_per_transfer={:.3} start_byte={} byte_order={} gain={} min_mib_per_second={} min_frames_per_second={} checksum={}",
            if ok { "PASS" } else { "FAIL" },
            result.transfers,
            result.transfer_bytes,
            result.packed_bytes,
            result.frames_loaded,
            result.elapsed_seconds,
            result.transfers_per_second,
            result.frames_per_second,
            result.mib_per_second,
            result.ns_per_transfer,
            args.start_byte,
            byte_order_name(args.byte_order),
            args.gain,
            args.min_mib_per_second,
            args.min_frames_per_second,
            result.checksum
        );
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

    if args.warmup_transfers > 0 {
        if let Err(error) = run_pack_bench(&args, args.warmup_transfers) {
            eprintln!("error: warmup failed: {error}");
            process::exit(2);
        }
    }

    let result = match run_pack_bench(&args, args.transfers) {
        Ok(result) => result,
        Err(error) => {
            eprintln!("error: bench failed: {error}");
            process::exit(2);
        }
    };
    let ok = result.mib_per_second >= args.min_mib_per_second
        && result.frames_per_second >= args.min_frames_per_second;
    print_result(&args, &result, ok);
    process::exit(if ok { 0 } else { 1 });
}
