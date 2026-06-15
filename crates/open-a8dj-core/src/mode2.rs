use crate::sample::{decode_i24, encode_i24, f32_to_output_i24, I24ByteOrder, S24_MAX};
use crate::topology::{CHANNELS, PAIRS, SIDES_PER_PAIR};

pub const STREAMS: usize = PAIRS;
pub const CHANNELS_PER_STREAM: usize = SIDES_PER_PAIR;
pub const BYTES_PER_SAMPLE: usize = 3;
pub const BYTES_PER_SAMPLE_USB: usize = 4;
pub const FRAME_BYTES_PER_STREAM: usize = CHANNELS_PER_STREAM * BYTES_PER_SAMPLE;
pub const GROUP_BYTES: usize = STREAMS * BYTES_PER_SAMPLE_USB;
pub const CHECK_OFFSET: usize = STREAMS * CHANNELS_PER_STREAM;
pub const DEFAULT_START_BYTE: usize = BYTES_PER_SAMPLE + 1;
pub const DEFAULT_TRANSFER_BYTES: usize = 352;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Mode2Error {
    InvalidStartByte,
    InvalidTransferBytes,
    InsufficientDecodedFrames,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DecodeResult {
    pub frames: Vec<[i32; CHANNELS]>,
    pub checks: usize,
    pub check_errors: usize,
    pub panic_flags: usize,
    pub sample_bytes: usize,
}

#[derive(Clone, Debug)]
pub struct Mode2OutputPacker {
    output_byte_in_frame: usize,
    output_frame_loaded: bool,
    output_frame_bytes: [[u8; FRAME_BYTES_PER_STREAM]; STREAMS],
    gain: f32,
    byte_order: I24ByteOrder,
}

pub trait OutputFrameProvider {
    fn next_frame(&mut self) -> [f32; CHANNELS];
}

impl Mode2OutputPacker {
    pub fn new(start_byte: usize, gain: f32, byte_order: I24ByteOrder) -> Result<Self, Mode2Error> {
        validate_start_byte(start_byte)?;
        Ok(Self {
            output_byte_in_frame: start_byte,
            output_frame_loaded: false,
            output_frame_bytes: [[0; FRAME_BYTES_PER_STREAM]; STREAMS],
            gain,
            byte_order,
        })
    }

    pub fn fill_from_frames(
        &mut self,
        frames: &[[f32; CHANNELS]],
        frame_index: &mut usize,
        out: &mut [u8],
    ) {
        let mut i = 0;
        while i < out.len() {
            if (i % GROUP_BYTES) == CHECK_OFFSET {
                for stream in 0..STREAMS {
                    if i >= out.len() {
                        break;
                    }
                    out[i] = mode2_check_byte(stream, i);
                    i += 1;
                }
                continue;
            }

            self.load_next_output_frame_if_needed(frames, frame_index);
            for stream in 0..STREAMS {
                if i >= out.len() {
                    break;
                }
                out[i] = self.output_frame_bytes[stream][self.output_byte_in_frame];
                i += 1;
            }
            self.output_byte_in_frame += 1;
            if self.output_byte_in_frame >= FRAME_BYTES_PER_STREAM {
                self.output_byte_in_frame = 0;
            }
        }
    }

    pub fn fill_from_interleaved_channels(
        &mut self,
        input: &[f32],
        channels: usize,
        frame_index: &mut usize,
        out: &mut [u8],
    ) -> usize {
        let start_frame = *frame_index;
        let frame_count = input.len().checked_div(channels).unwrap_or(0);

        let mut i = 0;
        while i < out.len() {
            if (i % GROUP_BYTES) == CHECK_OFFSET {
                for stream in 0..STREAMS {
                    if i >= out.len() {
                        break;
                    }
                    out[i] = mode2_check_byte(stream, i);
                    i += 1;
                }
                continue;
            }

            self.load_next_interleaved_frame_if_needed(input, channels, frame_count, frame_index);
            for stream in 0..STREAMS {
                if i >= out.len() {
                    break;
                }
                out[i] = self.output_frame_bytes[stream][self.output_byte_in_frame];
                i += 1;
            }
            self.output_byte_in_frame += 1;
            if self.output_byte_in_frame >= FRAME_BYTES_PER_STREAM {
                self.output_byte_in_frame = 0;
            }
        }

        (*frame_index).saturating_sub(start_frame)
    }

    pub fn fill_with_provider<P: OutputFrameProvider>(
        &mut self,
        provider: &mut P,
        out: &mut [u8],
    ) -> usize {
        let mut frames_loaded = 0;
        let mut i = 0;
        while i < out.len() {
            if (i % GROUP_BYTES) == CHECK_OFFSET {
                for stream in 0..STREAMS {
                    if i >= out.len() {
                        break;
                    }
                    out[i] = mode2_check_byte(stream, i);
                    i += 1;
                }
                continue;
            }

            if self.load_next_provider_frame_if_needed(provider) {
                frames_loaded += 1;
            }
            for stream in 0..STREAMS {
                if i >= out.len() {
                    break;
                }
                out[i] = self.output_frame_bytes[stream][self.output_byte_in_frame];
                i += 1;
            }
            self.output_byte_in_frame += 1;
            if self.output_byte_in_frame >= FRAME_BYTES_PER_STREAM {
                self.output_byte_in_frame = 0;
            }
        }
        frames_loaded
    }

    pub const fn output_byte_in_frame(&self) -> usize {
        self.output_byte_in_frame
    }

    fn load_next_output_frame_if_needed(
        &mut self,
        frames: &[[f32; CHANNELS]],
        frame_index: &mut usize,
    ) {
        if self.output_frame_loaded && self.output_byte_in_frame != 0 {
            return;
        }

        if let Some(frame) = frames.get(*frame_index) {
            self.output_frame_bytes = stream_frame_bytes(frame, self.gain, self.byte_order);
            *frame_index += 1;
        } else {
            self.output_frame_bytes = [[0; FRAME_BYTES_PER_STREAM]; STREAMS];
        }
        self.output_frame_loaded = true;
    }

    fn load_next_interleaved_frame_if_needed(
        &mut self,
        input: &[f32],
        channels: usize,
        frame_count: usize,
        frame_index: &mut usize,
    ) {
        if self.output_frame_loaded && self.output_byte_in_frame != 0 {
            return;
        }

        if *frame_index < frame_count && channels >= CHANNELS {
            let offset = *frame_index * channels;
            let mut frame = [0.0; CHANNELS];
            frame.copy_from_slice(&input[offset..offset + CHANNELS]);
            self.output_frame_bytes = stream_frame_bytes(&frame, self.gain, self.byte_order);
            *frame_index += 1;
        } else {
            self.output_frame_bytes = [[0; FRAME_BYTES_PER_STREAM]; STREAMS];
        }
        self.output_frame_loaded = true;
    }

    fn load_next_provider_frame_if_needed<P: OutputFrameProvider>(
        &mut self,
        provider: &mut P,
    ) -> bool {
        if self.output_frame_loaded && self.output_byte_in_frame != 0 {
            return false;
        }

        let frame = provider.next_frame();
        self.output_frame_bytes = stream_frame_bytes(&frame, self.gain, self.byte_order);
        self.output_frame_loaded = true;
        true
    }
}

pub fn validate_start_byte(start_byte: usize) -> Result<(), Mode2Error> {
    if start_byte < FRAME_BYTES_PER_STREAM {
        Ok(())
    } else {
        Err(Mode2Error::InvalidStartByte)
    }
}

pub fn validate_transfer_bytes(transfer_bytes: usize) -> Result<(), Mode2Error> {
    if transfer_bytes > 0 && transfer_bytes % GROUP_BYTES == 0 {
        Ok(())
    } else {
        Err(Mode2Error::InvalidTransferBytes)
    }
}

pub fn mode2_check_byte(stream: usize, byte_index: usize) -> u8 {
    let group = byte_index / GROUP_BYTES;
    ((stream << 1) as u8) | (((!group) & 1) as u8)
}

pub fn stream_frame_bytes(
    frame: &[f32; CHANNELS],
    gain: f32,
    byte_order: I24ByteOrder,
) -> [[u8; FRAME_BYTES_PER_STREAM]; STREAMS] {
    let mut streams = [[0; FRAME_BYTES_PER_STREAM]; STREAMS];
    for stream in 0..STREAMS {
        let left = f32_to_output_i24(frame[stream * 2], gain).value;
        let right = f32_to_output_i24(frame[stream * 2 + 1], gain).value;
        let left_bytes = encode_i24(left, byte_order);
        let right_bytes = encode_i24(right, byte_order);
        streams[stream][0..3].copy_from_slice(&left_bytes);
        streams[stream][3..6].copy_from_slice(&right_bytes);
    }
    streams
}

pub fn pack_transfers(
    frames: &[[f32; CHANNELS]],
    start_byte: usize,
    transfer_bytes: usize,
    transfers: usize,
    gain: f32,
    byte_order: I24ByteOrder,
) -> Result<Vec<u8>, Mode2Error> {
    validate_transfer_bytes(transfer_bytes)?;
    let mut packer = Mode2OutputPacker::new(start_byte, gain, byte_order)?;
    let mut frame_index = 0;
    let mut packed = Vec::with_capacity(transfer_bytes * transfers);
    for _ in 0..transfers {
        let offset = packed.len();
        packed.resize(offset + transfer_bytes, 0);
        packer.fill_from_frames(frames, &mut frame_index, &mut packed[offset..]);
    }
    Ok(packed)
}

pub fn pack_until_comparable(
    frames: &[[f32; CHANNELS]],
    start_byte: usize,
    transfer_bytes: usize,
    gain: f32,
    expected_count: usize,
    byte_order: I24ByteOrder,
) -> Result<(Vec<u8>, DecodeResult), Mode2Error> {
    validate_transfer_bytes(transfer_bytes)?;
    validate_start_byte(start_byte)?;

    let mut packer = Mode2OutputPacker::new(start_byte, gain, byte_order)?;
    let mut frame_index = 0;
    let mut packed = Vec::new();
    let max_transfers = max_transfers_for_comparison(frames.len(), transfer_bytes)?;

    for _ in 0..max_transfers {
        let offset = packed.len();
        packed.resize(offset + transfer_bytes, 0);
        packer.fill_from_frames(frames, &mut frame_index, &mut packed[offset..]);

        let decoded = decode_mode2_usb_bytes(&packed, start_byte, transfer_bytes, byte_order)?;
        if decoded.frames.len() >= expected_count {
            return Ok((packed, decoded));
        }
    }

    Err(Mode2Error::InsufficientDecodedFrames)
}

pub fn decode_mode2_usb_bytes(
    data: &[u8],
    start_byte: usize,
    transfer_bytes: usize,
    byte_order: I24ByteOrder,
) -> Result<DecodeResult, Mode2Error> {
    validate_start_byte(start_byte)?;
    validate_transfer_bytes(transfer_bytes)?;

    let mut pending = [[None; FRAME_BYTES_PER_STREAM]; STREAMS];
    let mut decoded = Vec::new();
    let mut checks = 0;
    let mut check_errors = 0;
    let mut panic_flags = 0;
    let mut sample_bytes = 0;
    let mut lane_streams = 0;
    let mut byte_position = start_byte;

    for (index, value) in data.iter().copied().enumerate() {
        let local_index = index % transfer_bytes;
        let group_offset = local_index % GROUP_BYTES;
        if (CHECK_OFFSET..CHECK_OFFSET + STREAMS).contains(&group_offset) {
            let stream = group_offset - CHECK_OFFSET;
            checks += 1;
            if value & 0x80 != 0 {
                panic_flags += 1;
            }
            if (value & 0x3f) != mode2_check_byte(stream, local_index) {
                check_errors += 1;
            }
            continue;
        }

        let stream = group_offset % STREAMS;
        if stream == 0 && byte_position == 0 {
            pending = [[None; FRAME_BYTES_PER_STREAM]; STREAMS];
            lane_streams = 0;
        }

        pending[stream][byte_position] = Some(value);
        sample_bytes += 1;
        lane_streams += 1;

        if lane_streams == STREAMS {
            if byte_position == FRAME_BYTES_PER_STREAM - 1 {
                let mut frame = [0; CHANNELS];
                let mut complete = true;
                for stream in 0..STREAMS {
                    for byte in pending[stream] {
                        if byte.is_none() {
                            complete = false;
                            break;
                        }
                    }
                    if !complete {
                        break;
                    }
                    let left = [
                        pending[stream][0].expect("complete"),
                        pending[stream][1].expect("complete"),
                        pending[stream][2].expect("complete"),
                    ];
                    let right = [
                        pending[stream][3].expect("complete"),
                        pending[stream][4].expect("complete"),
                        pending[stream][5].expect("complete"),
                    ];
                    frame[stream * 2] = decode_i24(left, byte_order);
                    frame[stream * 2 + 1] = decode_i24(right, byte_order);
                }
                if complete {
                    decoded.push(frame);
                }
                pending = [[None; FRAME_BYTES_PER_STREAM]; STREAMS];
            }
            byte_position = (byte_position + 1) % FRAME_BYTES_PER_STREAM;
            lane_streams = 0;
        }
    }

    Ok(DecodeResult {
        frames: decoded,
        checks,
        check_errors,
        panic_flags,
        sample_bytes,
    })
}

pub fn synthetic_s24_value(frame_index: usize, channel: usize) -> i32 {
    let stream = channel / CHANNELS_PER_STREAM;
    let side = channel % CHANNELS_PER_STREAM;
    let magnitude = ((stream + 1) * 1_000_000) + (side * 250_000) + ((frame_index % 8192) * 257);
    if side == 0 {
        magnitude as i32
    } else {
        -(magnitude as i32)
    }
}

pub fn synthetic_frame(frame_index: usize) -> [f32; CHANNELS] {
    let mut frame = [0.0; CHANNELS];
    for (channel, sample) in frame.iter_mut().enumerate() {
        *sample = synthetic_s24_value(frame_index, channel) as f32 / S24_MAX as f32;
    }
    frame
}

pub fn expected_s24_frame(frame: &[f32; CHANNELS], gain: f32) -> [i32; CHANNELS] {
    let mut expected = [0; CHANNELS];
    for (channel, value) in frame.iter().copied().enumerate() {
        expected[channel] = f32_to_output_i24(value, gain).value;
    }
    expected
}

pub fn max_transfers_for_comparison(
    frame_count: usize,
    transfer_bytes: usize,
) -> Result<usize, Mode2Error> {
    validate_transfer_bytes(transfer_bytes)?;
    let rough_bytes = (frame_count + 8) * GROUP_BYTES * 2;
    Ok(4.max(rough_bytes.div_ceil(transfer_bytes) + 4))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn synthetic_frames(frame_count: usize) -> Vec<[f32; CHANNELS]> {
        (0..frame_count).map(synthetic_frame).collect()
    }

    struct SliceProvider {
        frames: Vec<[f32; CHANNELS]>,
        next: usize,
    }

    impl OutputFrameProvider for SliceProvider {
        fn next_frame(&mut self) -> [f32; CHANNELS] {
            let frame = self
                .frames
                .get(self.next)
                .copied()
                .unwrap_or([0.0; CHANNELS]);
            self.next += 1;
            frame
        }
    }

    #[test]
    fn constants_match_mainline_mode2_layout() {
        assert_eq!(STREAMS, 4);
        assert_eq!(CHANNELS_PER_STREAM, 2);
        assert_eq!(FRAME_BYTES_PER_STREAM, 6);
        assert_eq!(GROUP_BYTES, 16);
        assert_eq!(CHECK_OFFSET, 8);
        assert_eq!(DEFAULT_START_BYTE, 4);
    }

    #[test]
    fn check_byte_alternates_by_group_and_encodes_stream() {
        assert_eq!(mode2_check_byte(0, 0), 1);
        assert_eq!(mode2_check_byte(1, 0), 3);
        assert_eq!(mode2_check_byte(2, 0), 5);
        assert_eq!(mode2_check_byte(3, 0), 7);
        assert_eq!(mode2_check_byte(0, GROUP_BYTES), 0);
        assert_eq!(mode2_check_byte(3, GROUP_BYTES), 6);
    }

    #[test]
    fn default_start_byte_round_trips_synthetic_frames() {
        let frames = synthetic_frames(64);
        let transfers = max_transfers_for_comparison(frames.len(), DEFAULT_TRANSFER_BYTES).unwrap();
        let packed = pack_transfers(
            &frames,
            DEFAULT_START_BYTE,
            DEFAULT_TRANSFER_BYTES,
            transfers,
            1.0,
            I24ByteOrder::BigEndian,
        )
        .unwrap();
        let decoded = decode_mode2_usb_bytes(
            &packed,
            DEFAULT_START_BYTE,
            DEFAULT_TRANSFER_BYTES,
            I24ByteOrder::BigEndian,
        )
        .unwrap();

        assert_eq!(decoded.check_errors, 0);
        assert_eq!(decoded.panic_flags, 0);
        assert!(decoded.frames.len() >= frames.len() - 1);
        for (decoded_frame, source_frame) in decoded.frames.iter().zip(frames.iter().skip(1)) {
            assert_eq!(*decoded_frame, expected_s24_frame(source_frame, 1.0));
        }
    }

    #[test]
    fn every_start_byte_round_trips_with_expected_source_offset() {
        let frames = synthetic_frames(16);
        for start_byte in 0..FRAME_BYTES_PER_STREAM {
            let transfers =
                max_transfers_for_comparison(frames.len(), DEFAULT_TRANSFER_BYTES).unwrap();
            let packed = pack_transfers(
                &frames,
                start_byte,
                DEFAULT_TRANSFER_BYTES,
                transfers,
                1.0,
                I24ByteOrder::BigEndian,
            )
            .unwrap();
            let decoded = decode_mode2_usb_bytes(
                &packed,
                start_byte,
                DEFAULT_TRANSFER_BYTES,
                I24ByteOrder::BigEndian,
            )
            .unwrap();
            let source_start = if start_byte == 0 { 0 } else { 1 };
            assert_eq!(decoded.check_errors, 0);
            assert_eq!(decoded.panic_flags, 0);
            assert!(decoded.frames.len() >= frames.len() - source_start);
            for (decoded_frame, source_frame) in
                decoded.frames.iter().zip(frames.iter().skip(source_start))
            {
                assert_eq!(*decoded_frame, expected_s24_frame(source_frame, 1.0));
            }
        }
    }

    #[test]
    fn pack_until_comparable_stops_at_python_validator_boundary() {
        let frames = synthetic_frames(64);
        let expected_count = frames.len() - 1;
        let (packed, decoded) = pack_until_comparable(
            &frames,
            DEFAULT_START_BYTE,
            DEFAULT_TRANSFER_BYTES,
            1.0,
            expected_count,
            I24ByteOrder::BigEndian,
        )
        .unwrap();

        assert_eq!(packed.len(), 2112);
        assert!(decoded.frames.len() >= expected_count);
        assert_eq!(decoded.check_errors, 0);
        assert_eq!(decoded.panic_flags, 0);
    }

    #[test]
    fn callback_provider_matches_slice_packer() {
        let frames = synthetic_frames(64);
        let mut provider = SliceProvider {
            frames: frames.clone(),
            next: 0,
        };
        let mut packer =
            Mode2OutputPacker::new(DEFAULT_START_BYTE, 1.0, I24ByteOrder::BigEndian).unwrap();
        let mut callback_packed = vec![0; 2112];
        let frames_loaded = packer.fill_with_provider(&mut provider, &mut callback_packed);

        let transfer_packed = pack_transfers(
            &frames,
            DEFAULT_START_BYTE,
            DEFAULT_TRANSFER_BYTES,
            6,
            1.0,
            I24ByteOrder::BigEndian,
        )
        .unwrap();

        assert_eq!(callback_packed, transfer_packed);
        assert_eq!(frames_loaded, provider.next);
        assert_eq!(packer.output_byte_in_frame(), 4);
    }
}
