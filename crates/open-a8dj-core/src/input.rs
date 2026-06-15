use crate::mode2::{
    decode_mode2_usb_bytes, Mode2Error, DEFAULT_START_BYTE, DEFAULT_TRANSFER_BYTES,
};
use crate::routing::RoutingMatrix;
use crate::sample::{I24ByteOrder, S24_SCALE};
use crate::topology::{CHANNELS, PAIRS, SIDES_PER_PAIR};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u8)]
pub enum InputMode {
    TimecodeVinyl = 0,
    TimecodeCdLine = 1,
    Phono = 2,
}

impl InputMode {
    pub const fn raw_value(self) -> u8 {
        self as u8
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct HardwareInputProfile {
    pub input_mode: InputMode,
    pub gnd_vinyl: bool,
    pub gnd_cd_line: bool,
    pub gnd_phono: bool,
    pub software_lock: bool,
    pub input_decode_enabled: bool,
    pub routing: RoutingMatrix,
}

impl HardwareInputProfile {
    pub fn playback() -> Self {
        Self {
            input_mode: InputMode::TimecodeCdLine,
            gnd_vinyl: false,
            gnd_cd_line: false,
            gnd_phono: false,
            software_lock: true,
            input_decode_enabled: false,
            routing: RoutingMatrix::identity(),
        }
    }

    pub fn timecode_vinyl() -> Self {
        Self {
            input_mode: InputMode::TimecodeVinyl,
            gnd_vinyl: true,
            gnd_cd_line: false,
            gnd_phono: false,
            software_lock: true,
            input_decode_enabled: true,
            routing: RoutingMatrix::identity(),
        }
    }

    pub fn timecode_cd_line() -> Self {
        Self {
            input_mode: InputMode::TimecodeCdLine,
            gnd_vinyl: false,
            gnd_cd_line: true,
            gnd_phono: false,
            software_lock: true,
            input_decode_enabled: true,
            routing: RoutingMatrix::identity(),
        }
    }

    pub fn phono() -> Self {
        Self {
            input_mode: InputMode::Phono,
            gnd_vinyl: false,
            gnd_cd_line: false,
            gnd_phono: true,
            software_lock: true,
            input_decode_enabled: true,
            routing: RoutingMatrix::identity(),
        }
    }
}

impl Default for HardwareInputProfile {
    fn default() -> Self {
        Self::playback()
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Mode2InputDecodeConfig {
    pub start_byte: usize,
    pub transfer_bytes: usize,
    pub byte_order: I24ByteOrder,
    pub input_decode_enabled: bool,
    pub routing: RoutingMatrix,
}

impl Default for Mode2InputDecodeConfig {
    fn default() -> Self {
        Self {
            start_byte: DEFAULT_START_BYTE,
            transfer_bytes: DEFAULT_TRANSFER_BYTES,
            byte_order: I24ByteOrder::BigEndian,
            input_decode_enabled: true,
            routing: RoutingMatrix::identity(),
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Mode2InputDecodeResult {
    pub frames: Vec<[f32; CHANNELS]>,
    pub raw_decoded_frames: usize,
    pub checks: usize,
    pub check_errors: usize,
    pub panic_flags: usize,
    pub sample_bytes: usize,
    pub decode_enabled: bool,
}

impl Mode2InputDecodeResult {
    pub fn pair_rms(&self, pair_index: usize) -> Option<f32> {
        if pair_index >= PAIRS || self.frames.is_empty() {
            return None;
        }
        let left = pair_index * SIDES_PER_PAIR;
        let right = left + 1;
        let mut sum = 0.0f64;
        for frame in &self.frames {
            sum += f64::from(frame[left] * frame[left]);
            sum += f64::from(frame[right] * frame[right]);
        }
        Some((sum / (self.frames.len() * SIDES_PER_PAIR) as f64).sqrt() as f32)
    }
}

pub fn decode_mode2_input_bytes(
    data: &[u8],
    config: &Mode2InputDecodeConfig,
) -> Result<Mode2InputDecodeResult, Mode2Error> {
    let decoded = decode_mode2_usb_bytes(
        data,
        config.start_byte,
        config.transfer_bytes,
        config.byte_order,
    )?;
    let raw_decoded_frames = decoded.frames.len();
    let frames = if config.input_decode_enabled {
        decoded
            .frames
            .iter()
            .map(|frame| {
                let mut as_float = [0.0; CHANNELS];
                for (channel, value) in frame.iter().copied().enumerate() {
                    as_float[channel] = value as f32 / S24_SCALE;
                }
                config.routing.apply_frame(&as_float)
            })
            .collect()
    } else {
        Vec::new()
    };

    Ok(Mode2InputDecodeResult {
        frames,
        raw_decoded_frames,
        checks: decoded.checks,
        check_errors: decoded.check_errors,
        panic_flags: decoded.panic_flags,
        sample_bytes: decoded.sample_bytes,
        decode_enabled: config.input_decode_enabled,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::mode2::{pack_transfers, synthetic_frame};

    #[test]
    fn dvs_profiles_match_mainline_control_policy() {
        let playback = HardwareInputProfile::playback();
        assert_eq!(playback.input_mode, InputMode::TimecodeCdLine);
        assert!(!playback.input_decode_enabled);
        assert!(playback.software_lock);
        assert_eq!(playback.routing, RoutingMatrix::identity());

        let vinyl = HardwareInputProfile::timecode_vinyl();
        assert_eq!(vinyl.input_mode.raw_value(), 0);
        assert!(vinyl.gnd_vinyl);
        assert!(!vinyl.gnd_cd_line);
        assert!(!vinyl.gnd_phono);
        assert!(vinyl.input_decode_enabled);
        assert_eq!(vinyl.routing, RoutingMatrix::identity());

        let cd_line = HardwareInputProfile::timecode_cd_line();
        assert_eq!(cd_line.input_mode.raw_value(), 1);
        assert!(!cd_line.gnd_vinyl);
        assert!(cd_line.gnd_cd_line);
        assert!(!cd_line.gnd_phono);
        assert!(cd_line.input_decode_enabled);
        assert_eq!(cd_line.routing, RoutingMatrix::identity());
    }

    #[test]
    fn mode2_input_decode_preserves_active_pairs() {
        let frames: Vec<[f32; CHANNELS]> = (0..128).map(synthetic_frame).collect();
        let packed = pack_transfers(
            &frames,
            DEFAULT_START_BYTE,
            DEFAULT_TRANSFER_BYTES,
            24,
            1.0,
            I24ByteOrder::BigEndian,
        )
        .unwrap();
        let decoded =
            decode_mode2_input_bytes(&packed, &Mode2InputDecodeConfig::default()).unwrap();

        assert_eq!(decoded.check_errors, 0);
        assert_eq!(decoded.panic_flags, 0);
        assert!(!decoded.frames.is_empty());
        assert_eq!(decoded.frames.len(), decoded.raw_decoded_frames);
        assert!(decoded.pair_rms(0).unwrap() > 0.0);
        assert!(decoded.pair_rms(3).unwrap() > 0.0);
    }

    #[test]
    fn decode_off_preserves_stats_without_publishing_frames() {
        let frames: Vec<[f32; CHANNELS]> = (0..128).map(synthetic_frame).collect();
        let packed = pack_transfers(
            &frames,
            DEFAULT_START_BYTE,
            DEFAULT_TRANSFER_BYTES,
            24,
            1.0,
            I24ByteOrder::BigEndian,
        )
        .unwrap();
        let config = Mode2InputDecodeConfig {
            input_decode_enabled: false,
            ..Mode2InputDecodeConfig::default()
        };
        let decoded = decode_mode2_input_bytes(&packed, &config).unwrap();

        assert_eq!(decoded.check_errors, 0);
        assert_eq!(decoded.panic_flags, 0);
        assert!(decoded.raw_decoded_frames > 0);
        assert!(decoded.frames.is_empty());
        assert!(!decoded.decode_enabled);
    }
}
