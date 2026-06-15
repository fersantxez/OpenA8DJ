pub const S24_MIN: i32 = -0x800000;
pub const S24_MAX: i32 = 0x7fffff;
pub const S24_SCALE: f32 = 8_388_608.0;
const OUTPUT_Q31_SCALE: f32 = 2_147_483_647.0;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum I24ByteOrder {
    BigEndian,
    NativeLittleEndian,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct SampleFlags {
    pub clipped: bool,
    pub non_finite: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct QuantizedI24 {
    pub value: i32,
    pub flags: SampleFlags,
}

impl QuantizedI24 {
    pub const fn silence() -> Self {
        Self {
            value: 0,
            flags: SampleFlags {
                clipped: false,
                non_finite: false,
            },
        }
    }
}

/// Convert Core Audio-style `f32` samples to the Audio 8 DJ output sample.
///
/// Finite samples intentionally match the current HAL/Python validator policy:
/// apply gain, clamp to `[-1.0, 1.0]`, quantize through signed Q31, then keep
/// the upper signed 24 bits. Non-finite samples are a Rust safety improvement:
/// they become flagged silence instead of crossing the realtime boundary as an
/// undefined float-to-integer conversion.
pub fn f32_to_output_i24(sample: f32, gain: f32) -> QuantizedI24 {
    let mut flags = SampleFlags::default();
    let mut scaled = sample * gain;

    if !scaled.is_finite() {
        flags.non_finite = true;
        return QuantizedI24 { value: 0, flags };
    }

    if scaled > 1.0 {
        scaled = 1.0;
        flags.clipped = true;
    } else if scaled < -1.0 {
        scaled = -1.0;
        flags.clipped = true;
    }

    let q31 = if scaled >= 1.0 {
        i32::MAX
    } else if scaled <= -1.0 {
        i32::MIN
    } else {
        (scaled * OUTPUT_Q31_SCALE).round_ties_even() as i32
    };

    QuantizedI24 {
        value: q31 >> 8,
        flags,
    }
}

pub fn encode_i24(value: i32, byte_order: I24ByteOrder) -> [u8; 3] {
    let raw = value & 0x00ff_ffff;
    match byte_order {
        I24ByteOrder::BigEndian => [
            ((raw >> 16) & 0xff) as u8,
            ((raw >> 8) & 0xff) as u8,
            (raw & 0xff) as u8,
        ],
        I24ByteOrder::NativeLittleEndian => [
            (raw & 0xff) as u8,
            ((raw >> 8) & 0xff) as u8,
            ((raw >> 16) & 0xff) as u8,
        ],
    }
}

pub fn decode_i24(bytes: [u8; 3], byte_order: I24ByteOrder) -> i32 {
    let raw = match byte_order {
        I24ByteOrder::BigEndian => {
            ((bytes[0] as i32) << 16) | ((bytes[1] as i32) << 8) | bytes[2] as i32
        }
        I24ByteOrder::NativeLittleEndian => {
            bytes[0] as i32 | ((bytes[1] as i32) << 8) | ((bytes[2] as i32) << 16)
        }
    };
    if raw & 0x0080_0000 != 0 {
        raw | !0x00ff_ffff
    } else {
        raw
    }
}

pub fn decode_i24_to_f32(bytes: [u8; 3], byte_order: I24ByteOrder) -> f32 {
    decode_i24(bytes, byte_order) as f32 / S24_SCALE
}

pub fn encode_f32_output_i24(
    sample: f32,
    gain: f32,
    byte_order: I24ByteOrder,
) -> ([u8; 3], SampleFlags) {
    let quantized = f32_to_output_i24(sample, gain);
    (encode_i24(quantized.value, byte_order), quantized.flags)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn conversion_vectors_match_mainline_big_endian_output() {
        let cases = [
            (0.0, [0x00, 0x00, 0x00], 0),
            (1.0, [0x7f, 0xff, 0xff], S24_MAX),
            (-1.0, [0x80, 0x00, 0x00], S24_MIN),
        ];

        for (sample, expected_bytes, expected_value) in cases {
            let quantized = f32_to_output_i24(sample, 1.0);
            assert_eq!(quantized.value, expected_value);
            assert_eq!(
                encode_i24(quantized.value, I24ByteOrder::BigEndian),
                expected_bytes
            );
            assert_eq!(
                decode_i24(expected_bytes, I24ByteOrder::BigEndian),
                expected_value
            );
        }
    }

    #[test]
    fn conversion_vectors_match_mainline_native_little_output() {
        assert_eq!(
            encode_i24(S24_MAX, I24ByteOrder::NativeLittleEndian),
            [0xff, 0xff, 0x7f]
        );
        assert_eq!(
            encode_i24(S24_MIN, I24ByteOrder::NativeLittleEndian),
            [0x00, 0x00, 0x80]
        );
    }

    #[test]
    fn out_of_range_samples_saturate_and_report_clipping() {
        let high = f32_to_output_i24(2.0, 1.0);
        let low = f32_to_output_i24(-2.0, 1.0);
        assert_eq!(high.value, S24_MAX);
        assert_eq!(low.value, S24_MIN);
        assert!(high.flags.clipped);
        assert!(low.flags.clipped);
    }

    #[test]
    fn non_finite_samples_become_reported_silence() {
        for sample in [f32::NAN, f32::INFINITY, f32::NEG_INFINITY] {
            let quantized = f32_to_output_i24(sample, 1.0);
            assert_eq!(quantized.value, 0);
            assert!(quantized.flags.non_finite);
            assert!(!quantized.flags.clipped);
        }
    }

    #[test]
    fn i24_round_trips_through_both_byte_orders() {
        for value in [S24_MIN, -1_000_000, -1, 0, 1, 1_000_000, S24_MAX] {
            for order in [I24ByteOrder::BigEndian, I24ByteOrder::NativeLittleEndian] {
                assert_eq!(decode_i24(encode_i24(value, order), order), value);
            }
        }
    }
}
