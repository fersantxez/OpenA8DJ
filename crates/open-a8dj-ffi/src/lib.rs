#![deny(unsafe_op_in_unsafe_fn)]

use open_a8dj_core::mode2::{
    stream_frame_bytes, validate_start_byte, validate_transfer_bytes, Mode2OutputPacker,
    DEFAULT_START_BYTE, DEFAULT_TRANSFER_BYTES, FRAME_BYTES_PER_STREAM, STREAMS,
};
use open_a8dj_core::sample::I24ByteOrder;
use open_a8dj_core::topology::CHANNELS;
use std::mem;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::slice;

pub const OPENA8DJ_RUST_CONFIG_VERSION: u32 = 1;
pub const OPENA8DJ_RUST_COUNTERS_VERSION: u32 = 1;
pub const OPENA8DJ_RUST_BYTE_ORDER_BIG_ENDIAN: u32 = 0;
pub const OPENA8DJ_RUST_BYTE_ORDER_NATIVE_LITTLE_ENDIAN: u32 = 1;

#[repr(i32)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum OpenA8DJRustStatus {
    Ok = 0,
    NullPointer = -1,
    InvalidConfig = -2,
    InvalidHandle = -3,
    InvalidChannels = -4,
    InvalidBuffer = -5,
    Panic = -100,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct OpenA8DJRustConfig {
    pub size: u32,
    pub version: u32,
    pub start_byte: u32,
    pub transfer_bytes: u32,
    pub output_gain: f32,
    pub byte_order: u32,
}

impl Default for OpenA8DJRustConfig {
    fn default() -> Self {
        Self {
            size: mem::size_of::<Self>() as u32,
            version: OPENA8DJ_RUST_CONFIG_VERSION,
            start_byte: DEFAULT_START_BYTE as u32,
            transfer_bytes: DEFAULT_TRANSFER_BYTES as u32,
            output_gain: 1.0,
            byte_order: OPENA8DJ_RUST_BYTE_ORDER_BIG_ENDIAN,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OpenA8DJRustCounters {
    pub size: u32,
    pub version: u32,
    pub frames_submitted: u64,
    pub frames_consumed: u64,
    pub bytes_packed: u64,
    pub fill_calls: u64,
    pub invalid_calls: u64,
    pub last_status: i32,
}

impl OpenA8DJRustCounters {
    fn new() -> Self {
        Self {
            size: mem::size_of::<Self>() as u32,
            version: OPENA8DJ_RUST_COUNTERS_VERSION,
            ..Self::default()
        }
    }
}

pub struct OpenA8DJRustEngine {
    config: OpenA8DJRustConfig,
    packer: Mode2OutputPacker,
    counters: OpenA8DJRustCounters,
}

fn ffi_boundary(function: impl FnOnce() -> OpenA8DJRustStatus) -> OpenA8DJRustStatus {
    match catch_unwind(AssertUnwindSafe(function)) {
        Ok(status) => status,
        Err(_) => OpenA8DJRustStatus::Panic,
    }
}

fn set_status(engine: &mut OpenA8DJRustEngine, status: OpenA8DJRustStatus) -> OpenA8DJRustStatus {
    engine.counters.last_status = status as i32;
    if status != OpenA8DJRustStatus::Ok {
        engine.counters.invalid_calls = engine.counters.invalid_calls.saturating_add(1);
    }
    status
}

fn byte_order(value: u32) -> Option<I24ByteOrder> {
    match value {
        OPENA8DJ_RUST_BYTE_ORDER_BIG_ENDIAN => Some(I24ByteOrder::BigEndian),
        OPENA8DJ_RUST_BYTE_ORDER_NATIVE_LITTLE_ENDIAN => Some(I24ByteOrder::NativeLittleEndian),
        _ => None,
    }
}

fn validate_config(config: OpenA8DJRustConfig) -> Result<OpenA8DJRustConfig, OpenA8DJRustStatus> {
    if config.size < mem::size_of::<OpenA8DJRustConfig>() as u32 {
        return Err(OpenA8DJRustStatus::InvalidConfig);
    }
    if config.version != OPENA8DJ_RUST_CONFIG_VERSION {
        return Err(OpenA8DJRustStatus::InvalidConfig);
    }
    validate_start_byte(config.start_byte as usize)
        .map_err(|_| OpenA8DJRustStatus::InvalidConfig)?;
    validate_transfer_bytes(config.transfer_bytes as usize)
        .map_err(|_| OpenA8DJRustStatus::InvalidConfig)?;
    if !config.output_gain.is_finite() || config.output_gain < 0.0 {
        return Err(OpenA8DJRustStatus::InvalidConfig);
    }
    if byte_order(config.byte_order).is_none() {
        return Err(OpenA8DJRustStatus::InvalidConfig);
    }
    Ok(config)
}

#[no_mangle]
/// # Safety
///
/// `out_config` must be non-null and valid for writing one
/// `OpenA8DJRustConfig`.
pub unsafe extern "C" fn opena8dj_rust_config_default(
    out_config: *mut OpenA8DJRustConfig,
) -> OpenA8DJRustStatus {
    ffi_boundary(|| {
        if out_config.is_null() {
            return OpenA8DJRustStatus::NullPointer;
        }
        unsafe {
            ptr::write(out_config, OpenA8DJRustConfig::default());
        }
        OpenA8DJRustStatus::Ok
    })
}

#[no_mangle]
pub extern "C" fn opena8dj_rust_channels() -> u32 {
    CHANNELS as u32
}

#[no_mangle]
pub extern "C" fn opena8dj_rust_default_start_byte() -> u32 {
    DEFAULT_START_BYTE as u32
}

#[no_mangle]
pub extern "C" fn opena8dj_rust_default_transfer_bytes() -> u32 {
    DEFAULT_TRANSFER_BYTES as u32
}

#[no_mangle]
/// # Safety
///
/// `input_frame` must point to at least `channels` readable `f32` values, with
/// `channels == 8`. `output_bytes` must point to at least 24 writable bytes:
/// four streams times six signed-24 sample bytes per stream.
pub unsafe extern "C" fn opena8dj_rust_stream_frame_bytes(
    input_frame: *const f32,
    channels: u32,
    output_bytes: *mut u8,
    output_len: usize,
    output_gain: f32,
    byte_order_value: u32,
) -> OpenA8DJRustStatus {
    ffi_boundary(|| {
        if input_frame.is_null() || output_bytes.is_null() {
            return OpenA8DJRustStatus::NullPointer;
        }
        if channels as usize != CHANNELS {
            return OpenA8DJRustStatus::InvalidChannels;
        }
        if output_len < STREAMS * FRAME_BYTES_PER_STREAM {
            return OpenA8DJRustStatus::InvalidBuffer;
        }
        if !output_gain.is_finite() || output_gain < 0.0 {
            return OpenA8DJRustStatus::InvalidConfig;
        }
        let Some(order) = byte_order(byte_order_value) else {
            return OpenA8DJRustStatus::InvalidConfig;
        };

        let input = unsafe { slice::from_raw_parts(input_frame, CHANNELS) };
        let mut frame = [0.0; CHANNELS];
        frame.copy_from_slice(input);
        let streams = stream_frame_bytes(&frame, output_gain, order);
        let output = unsafe { slice::from_raw_parts_mut(output_bytes, output_len) };
        for (stream, bytes) in streams.iter().enumerate() {
            let dst = stream * FRAME_BYTES_PER_STREAM;
            output[dst..dst + FRAME_BYTES_PER_STREAM].copy_from_slice(bytes);
        }
        OpenA8DJRustStatus::Ok
    })
}

#[no_mangle]
/// # Safety
///
/// When non-null, `config` must point to a readable `OpenA8DJRustConfig`.
/// `out_engine` must be non-null and valid for writing one engine pointer.
pub unsafe extern "C" fn opena8dj_rust_engine_create(
    config: *const OpenA8DJRustConfig,
    out_engine: *mut *mut OpenA8DJRustEngine,
) -> OpenA8DJRustStatus {
    ffi_boundary(|| {
        if out_engine.is_null() {
            return OpenA8DJRustStatus::NullPointer;
        }
        unsafe {
            ptr::write(out_engine, ptr::null_mut());
        }

        let config = if config.is_null() {
            OpenA8DJRustConfig::default()
        } else {
            unsafe { ptr::read(config) }
        };
        let Ok(config) = validate_config(config) else {
            return OpenA8DJRustStatus::InvalidConfig;
        };
        let Some(order) = byte_order(config.byte_order) else {
            return OpenA8DJRustStatus::InvalidConfig;
        };
        let Ok(packer) =
            Mode2OutputPacker::new(config.start_byte as usize, config.output_gain, order)
        else {
            return OpenA8DJRustStatus::InvalidConfig;
        };

        let engine = Box::new(OpenA8DJRustEngine {
            config,
            packer,
            counters: OpenA8DJRustCounters::new(),
        });
        unsafe {
            ptr::write(out_engine, Box::into_raw(engine));
        }
        OpenA8DJRustStatus::Ok
    })
}

#[no_mangle]
/// # Safety
///
/// `engine` must be null or a pointer returned by `opena8dj_rust_engine_create`
/// that has not already been destroyed.
pub unsafe extern "C" fn opena8dj_rust_engine_destroy(
    engine: *mut OpenA8DJRustEngine,
) -> OpenA8DJRustStatus {
    ffi_boundary(|| {
        if !engine.is_null() {
            unsafe {
                drop(Box::from_raw(engine));
            }
        }
        OpenA8DJRustStatus::Ok
    })
}

#[no_mangle]
/// # Safety
///
/// `engine` must be a live engine pointer. `input_frames` must point to at
/// least `frame_count * channels` readable `f32` values unless `frame_count` is
/// zero. `output_bytes` must point to `output_len` writable bytes unless
/// `output_len` is zero. `out_frames_consumed` may be null or writable.
pub unsafe extern "C" fn opena8dj_rust_engine_fill_playback_bytes(
    engine: *mut OpenA8DJRustEngine,
    input_frames: *const f32,
    frame_count: u32,
    channels: u32,
    output_bytes: *mut u8,
    output_len: usize,
    out_frames_consumed: *mut u32,
) -> OpenA8DJRustStatus {
    ffi_boundary(|| {
        if engine.is_null() {
            return OpenA8DJRustStatus::InvalidHandle;
        }
        let engine = unsafe { &mut *engine };

        if channels as usize != CHANNELS {
            return set_status(engine, OpenA8DJRustStatus::InvalidChannels);
        }
        if output_len > 0 && output_bytes.is_null() {
            return set_status(engine, OpenA8DJRustStatus::NullPointer);
        }
        if frame_count > 0 && input_frames.is_null() {
            return set_status(engine, OpenA8DJRustStatus::NullPointer);
        }

        let input_len = frame_count as usize * channels as usize;
        let input = if input_len == 0 {
            &[]
        } else {
            unsafe { slice::from_raw_parts(input_frames, input_len) }
        };
        let output = if output_len == 0 {
            &mut []
        } else {
            unsafe { slice::from_raw_parts_mut(output_bytes, output_len) }
        };

        let mut frame_index = 0usize;
        let consumed = engine.packer.fill_from_interleaved_channels(
            input,
            channels as usize,
            &mut frame_index,
            output,
        );
        if !out_frames_consumed.is_null() {
            unsafe {
                ptr::write(out_frames_consumed, consumed as u32);
            }
        }

        engine.counters.fill_calls = engine.counters.fill_calls.saturating_add(1);
        engine.counters.frames_submitted = engine
            .counters
            .frames_submitted
            .saturating_add(frame_count as u64);
        engine.counters.frames_consumed = engine
            .counters
            .frames_consumed
            .saturating_add(consumed as u64);
        engine.counters.bytes_packed = engine
            .counters
            .bytes_packed
            .saturating_add(output_len as u64);
        set_status(engine, OpenA8DJRustStatus::Ok)
    })
}

#[no_mangle]
/// # Safety
///
/// `engine` must be a live engine pointer. `out_counters` must be non-null and
/// valid for writing one `OpenA8DJRustCounters`.
pub unsafe extern "C" fn opena8dj_rust_engine_snapshot_counters(
    engine: *const OpenA8DJRustEngine,
    out_counters: *mut OpenA8DJRustCounters,
) -> OpenA8DJRustStatus {
    ffi_boundary(|| {
        if engine.is_null() || out_counters.is_null() {
            return OpenA8DJRustStatus::NullPointer;
        }
        let engine = unsafe { &*engine };
        unsafe {
            ptr::write(out_counters, engine.counters);
        }
        OpenA8DJRustStatus::Ok
    })
}

#[no_mangle]
/// # Safety
///
/// `engine` must be a live engine pointer. When non-null, `config` must point
/// to a readable `OpenA8DJRustConfig`.
pub unsafe extern "C" fn opena8dj_rust_engine_reset(
    engine: *mut OpenA8DJRustEngine,
    config: *const OpenA8DJRustConfig,
) -> OpenA8DJRustStatus {
    ffi_boundary(|| {
        if engine.is_null() {
            return OpenA8DJRustStatus::InvalidHandle;
        }
        let engine = unsafe { &mut *engine };
        let config = if config.is_null() {
            engine.config
        } else {
            unsafe { ptr::read(config) }
        };
        let Ok(config) = validate_config(config) else {
            return set_status(engine, OpenA8DJRustStatus::InvalidConfig);
        };
        let Some(order) = byte_order(config.byte_order) else {
            return set_status(engine, OpenA8DJRustStatus::InvalidConfig);
        };
        let Ok(packer) =
            Mode2OutputPacker::new(config.start_byte as usize, config.output_gain, order)
        else {
            return set_status(engine, OpenA8DJRustStatus::InvalidConfig);
        };

        engine.config = config;
        engine.packer = packer;
        set_status(engine, OpenA8DJRustStatus::Ok)
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn creates_engine_with_default_config_and_packs_bytes() {
        let mut config = OpenA8DJRustConfig::default();
        assert_eq!(
            unsafe { opena8dj_rust_config_default(&mut config) },
            OpenA8DJRustStatus::Ok
        );

        let mut engine = ptr::null_mut();
        assert_eq!(
            unsafe { opena8dj_rust_engine_create(&config, &mut engine) },
            OpenA8DJRustStatus::Ok
        );
        assert!(!engine.is_null());

        let frames = [[0.0f32; CHANNELS]; 8];
        let mut output = [0u8; DEFAULT_TRANSFER_BYTES];
        let mut consumed = 0u32;
        assert_eq!(
            unsafe {
                opena8dj_rust_engine_fill_playback_bytes(
                    engine,
                    frames.as_ptr().cast(),
                    frames.len() as u32,
                    CHANNELS as u32,
                    output.as_mut_ptr(),
                    output.len(),
                    &mut consumed,
                )
            },
            OpenA8DJRustStatus::Ok
        );
        assert!(consumed > 0);

        let mut counters = OpenA8DJRustCounters::default();
        assert_eq!(
            unsafe { opena8dj_rust_engine_snapshot_counters(engine, &mut counters) },
            OpenA8DJRustStatus::Ok
        );
        assert_eq!(counters.fill_calls, 1);
        assert_eq!(counters.bytes_packed, DEFAULT_TRANSFER_BYTES as u64);
        assert_eq!(counters.last_status, OpenA8DJRustStatus::Ok as i32);

        assert_eq!(
            unsafe { opena8dj_rust_engine_destroy(engine) },
            OpenA8DJRustStatus::Ok
        );
    }

    #[test]
    fn rejects_wrong_channel_count_without_panic() {
        let mut engine = ptr::null_mut();
        assert_eq!(
            unsafe { opena8dj_rust_engine_create(ptr::null(), &mut engine) },
            OpenA8DJRustStatus::Ok
        );
        let frames = [0.0f32; CHANNELS];
        let mut output = [0u8; DEFAULT_TRANSFER_BYTES];
        assert_eq!(
            unsafe {
                opena8dj_rust_engine_fill_playback_bytes(
                    engine,
                    frames.as_ptr(),
                    1,
                    2,
                    output.as_mut_ptr(),
                    output.len(),
                    ptr::null_mut(),
                )
            },
            OpenA8DJRustStatus::InvalidChannels
        );
        assert_eq!(
            unsafe { opena8dj_rust_engine_destroy(engine) },
            OpenA8DJRustStatus::Ok
        );
    }

    #[test]
    fn stateless_stream_frame_encoder_matches_big_endian_vectors() {
        let frame = [0.0f32, 1.0, -1.0, 0.5, -0.5, 0.25, -0.25, 0.125];
        let mut output = [0u8; STREAMS * FRAME_BYTES_PER_STREAM];
        assert_eq!(
            unsafe {
                opena8dj_rust_stream_frame_bytes(
                    frame.as_ptr(),
                    CHANNELS as u32,
                    output.as_mut_ptr(),
                    output.len(),
                    1.0,
                    OPENA8DJ_RUST_BYTE_ORDER_BIG_ENDIAN,
                )
            },
            OpenA8DJRustStatus::Ok
        );

        assert_eq!(&output[0..6], &[0x00, 0x00, 0x00, 0x7f, 0xff, 0xff]);
        assert_eq!(&output[6..9], &[0x80, 0x00, 0x00]);
    }
}
