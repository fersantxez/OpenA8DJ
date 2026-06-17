#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

struct Stereo {
  float left{};
  float right{};
};

std::vector<std::uint8_t> ReadBytes(const std::string& path, std::size_t max_bytes) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot open " + path);
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
  if (max_bytes > 0 && bytes.size() > max_bytes) {
    bytes.resize(max_bytes);
  }
  return bytes;
}

std::uint16_t ReadLE16(const std::uint8_t* ptr) {
  return static_cast<std::uint16_t>(ptr[0]) |
         static_cast<std::uint16_t>(ptr[1] << 8U);
}

std::uint32_t ReadLE32(const std::uint8_t* ptr) {
  return static_cast<std::uint32_t>(ptr[0]) |
         (static_cast<std::uint32_t>(ptr[1]) << 8U) |
         (static_cast<std::uint32_t>(ptr[2]) << 16U) |
         (static_cast<std::uint32_t>(ptr[3]) << 24U);
}

std::vector<Stereo> ReadWav16Stereo(const std::string& path) {
  const auto bytes = ReadBytes(path, 0);
  if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
      std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
    throw std::runtime_error("not a RIFF/WAVE file: " + path);
  }
  std::uint16_t channels = 0;
  std::uint32_t data_offset = 0;
  std::uint32_t data_size = 0;
  std::uint16_t bits = 0;
  for (std::size_t offset = 12; offset + 8 <= bytes.size();) {
    const std::uint8_t* chunk = bytes.data() + offset;
    const std::uint32_t chunk_size = ReadLE32(chunk + 4);
    offset += 8;
    if (offset + chunk_size > bytes.size()) {
      break;
    }
    if (std::memcmp(chunk, "fmt ", 4) == 0 && chunk_size >= 16) {
      const std::uint16_t format = ReadLE16(bytes.data() + offset);
      channels = ReadLE16(bytes.data() + offset + 2);
      bits = ReadLE16(bytes.data() + offset + 14);
      if (format != 1) {
        throw std::runtime_error("only PCM WAV is supported");
      }
    } else if (std::memcmp(chunk, "data", 4) == 0) {
      data_offset = static_cast<std::uint32_t>(offset);
      data_size = chunk_size;
    }
    offset += (chunk_size + 1U) & ~1U;
  }
  if (data_offset == 0 || channels == 0 || channels > 2 || bits != 16) {
    throw std::runtime_error("unsupported WAV layout");
  }
  const std::uint32_t frames = data_size / (channels * sizeof(std::int16_t));
  std::vector<Stereo> out;
  out.reserve(frames);
  const auto* pcm = reinterpret_cast<const std::int16_t*>(bytes.data() + data_offset);
  for (std::uint32_t frame = 0; frame < frames; ++frame) {
    const float left = static_cast<float>(pcm[frame * channels]) / 32768.0F;
    const float right = channels > 1 ? static_cast<float>(pcm[frame * channels + 1]) / 32768.0F : left;
    out.push_back({left, right});
  }
  return out;
}

float S24ToFloat(const std::uint8_t* bytes, bool native_order) {
  std::int32_t value = 0;
  if (native_order) {
    value = static_cast<std::int32_t>(bytes[0]) |
            (static_cast<std::int32_t>(bytes[1]) << 8) |
            (static_cast<std::int32_t>(bytes[2]) << 16);
  } else {
    value = (static_cast<std::int32_t>(bytes[0]) << 16) |
            (static_cast<std::int32_t>(bytes[1]) << 8) |
            static_cast<std::int32_t>(bytes[2]);
  }
  if ((value & 0x800000) != 0) {
    value |= ~0x00ffffff;
  }
  return static_cast<float>(value) / 8388608.0F;
}

std::uint8_t Mode2CheckByte(std::uint32_t stream, std::size_t byte_index) {
  const std::size_t group = byte_index / 16U;
  return static_cast<std::uint8_t>((stream << 1U) | ((~group) & 1U));
}

struct DecodeResult {
  std::vector<Stereo> pair;
  std::uint64_t checks{};
  std::uint64_t check_errors{};
  std::uint64_t panic_flags{};
};

DecodeResult DecodeMode2(const std::vector<std::uint8_t>& data,
                         int check_offset,
                         int start_byte,
                         bool native_order,
                         int pair_index) {
  constexpr int kStreams = 4;
  constexpr int kFrameBytes = 6;
  std::uint8_t pending[kStreams][kFrameBytes] = {};
  bool present[kStreams][kFrameBytes] = {};
  int byte_position = start_byte;
  int lane_streams = 0;
  DecodeResult result;
  result.pair.reserve(data.size() / 32U);
  for (std::size_t index = 0; index < data.size(); ++index) {
    const int group_offset = static_cast<int>(index % 16U);
    const std::uint8_t value = data[index];
    if (group_offset >= check_offset && group_offset < check_offset + kStreams) {
      ++result.checks;
      const std::uint32_t stream = static_cast<std::uint32_t>(group_offset - check_offset);
      if ((value & 0x80U) != 0) {
        ++result.panic_flags;
      }
      if ((value & 0x3fU) != Mode2CheckByte(stream, index)) {
        ++result.check_errors;
      }
      continue;
    }
    const int stream = group_offset % kStreams;
    if (stream == 0 && byte_position == 0) {
      std::memset(present, 0, sizeof(present));
      lane_streams = 0;
    }
    pending[stream][byte_position] = value;
    present[stream][byte_position] = true;
    ++lane_streams;
    if (lane_streams == kStreams) {
      if (byte_position == kFrameBytes - 1) {
        bool complete = true;
        for (int stream_index = 0; stream_index < kStreams; ++stream_index) {
          for (int byte = 0; byte < kFrameBytes; ++byte) {
            complete = complete && present[stream_index][byte];
          }
        }
        if (complete) {
          const int stream = pair_index;
          result.pair.push_back({
              S24ToFloat(&pending[stream][0], native_order),
              S24ToFloat(&pending[stream][3], native_order),
          });
        }
        std::memset(present, 0, sizeof(present));
      }
      byte_position = (byte_position + 1) % kFrameBytes;
      lane_streams = 0;
    }
  }
  return result;
}

std::vector<float> Mono(const std::vector<Stereo>& stereo, std::size_t max_frames) {
  const std::size_t count = std::min(stereo.size(), max_frames);
  std::vector<float> mono;
  mono.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    mono.push_back(0.5F * (stereo[i].left + stereo[i].right));
  }
  return mono;
}

struct Score {
  double abs_corr{};
  double corr{};
  std::size_t lag{};
  double rms{};
  double peak{};
};

Score BestCorrelation(const std::vector<float>& reference, const std::vector<float>& got);

void FloatToI24(float sample, bool native_order, std::uint8_t* out) {
  sample = std::clamp(sample, -1.0F, 1.0F);
  std::int32_t value = 0;
  if (sample >= 1.0F) {
    value = 0x7fffff;
  } else if (sample <= -1.0F) {
    value = -0x800000;
  } else {
    value = static_cast<std::int32_t>(std::lrint(sample * 8388607.0F));
  }
  const std::uint32_t raw = static_cast<std::uint32_t>(value) & 0x00ffffffU;
  if (native_order) {
    out[0] = static_cast<std::uint8_t>(raw & 0xffU);
    out[1] = static_cast<std::uint8_t>((raw >> 8U) & 0xffU);
    out[2] = static_cast<std::uint8_t>((raw >> 16U) & 0xffU);
  } else {
    out[0] = static_cast<std::uint8_t>((raw >> 16U) & 0xffU);
    out[1] = static_cast<std::uint8_t>((raw >> 8U) & 0xffU);
    out[2] = static_cast<std::uint8_t>(raw & 0xffU);
  }
}

std::vector<std::uint8_t> PackSyntheticMode2(const std::vector<Stereo>& frames,
                                             int start_byte,
                                             bool native_order,
                                             std::size_t byte_count) {
  constexpr int kStreams = 4;
  constexpr int kFrameBytes = 6;
  std::uint8_t frame_bytes[kStreams][kFrameBytes] = {};
  std::size_t frame_index = 0;
  int output_byte = start_byte;
  bool loaded = false;
  std::vector<std::uint8_t> out;
  out.reserve(byte_count);
  std::size_t i = 0;
  while (i < byte_count) {
    if ((i % 16U) == 8U) {
      for (std::uint32_t stream = 0; stream < 4U && i < byte_count; ++stream, ++i) {
        out.push_back(Mode2CheckByte(stream, i));
      }
      continue;
    }
    if (!loaded || output_byte == 0) {
      const Stereo frame = frame_index < frames.size() ? frames[frame_index++] : Stereo{};
      for (int stream = 0; stream < kStreams; ++stream) {
        FloatToI24(frame.left, native_order, &frame_bytes[stream][0]);
        FloatToI24(frame.right, native_order, &frame_bytes[stream][3]);
      }
      loaded = true;
    }
    for (int stream = 0; stream < kStreams && i < byte_count; ++stream, ++i) {
      out.push_back(frame_bytes[stream][output_byte]);
    }
    output_byte = (output_byte + 1) % kFrameBytes;
  }
  return out;
}

int RunSelfTest() {
  std::vector<Stereo> frames;
  frames.reserve(48000);
  for (int i = 0; i < 48000; ++i) {
    const float t = static_cast<float>(i) / 48000.0F;
    frames.push_back({
        0.21F * std::sin(2.0F * 3.14159265358979323846F * 997.0F * t),
        0.17F * std::sin(2.0F * 3.14159265358979323846F * 1237.0F * t),
    });
  }
  const auto raw = PackSyntheticMode2(frames, 4, false, 2U * 1024U * 1024U);
  const auto decoded = DecodeMode2(raw, 8, 4, false, 0);
  const auto score = BestCorrelation(Mono(frames, 48000), Mono(decoded.pair, 180000));
  std::cout << "self_test_frames=" << decoded.pair.size() << "\n";
  std::cout << "self_test_check_errors=" << decoded.check_errors << "\n";
  std::cout << "self_test_panic_flags=" << decoded.panic_flags << "\n";
  std::cout << "self_test_abs_corr=" << score.abs_corr << "\n";
  const bool pass = decoded.check_errors == 0 && decoded.panic_flags == 0 && score.abs_corr > 0.99;
  std::cout << "self_test_result=" << (pass ? "PASS" : "FAIL") << "\n";
  return pass ? 0 : 1;
}

Score BestCorrelation(const std::vector<float>& reference, const std::vector<float>& got) {
  constexpr std::size_t kStride = 64;
  constexpr std::size_t kLagStep = 16;
  const std::size_t window = std::min<std::size_t>(reference.size(), 48000);
  const std::size_t got_limit = std::min<std::size_t>(got.size(), 180000);
  Score best;
  if (window < 1000 || got_limit <= window) {
    return best;
  }
  double ref_energy = 0.0;
  for (std::size_t i = 0; i < window; i += kStride) {
    ref_energy += static_cast<double>(reference[i]) * reference[i];
  }
  for (std::size_t lag = 0; lag + window < got_limit; lag += kLagStep) {
    double dot = 0.0;
    double got_energy = 0.0;
    for (std::size_t i = 0; i < window; i += kStride) {
      const double g = got[lag + i];
      dot += static_cast<double>(reference[i]) * g;
      got_energy += g * g;
    }
    if (ref_energy <= 0.0 || got_energy <= 0.0) {
      continue;
    }
    const double corr = dot / std::sqrt(ref_energy * got_energy);
    if (std::fabs(corr) > best.abs_corr) {
      best.abs_corr = std::fabs(corr);
      best.corr = corr;
      best.lag = lag;
    }
  }
  for (std::size_t i = 0; i < got_limit; ++i) {
    const double value = got[i];
    best.rms += value * value;
    best.peak = std::max(best.peak, std::fabs(value));
  }
  best.rms = std::sqrt(best.rms / static_cast<double>(got_limit));
  return best;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--self-test") {
    return RunSelfTest();
  }
  if (argc < 3) {
    std::cerr << "usage: " << argv[0] << " reference.wav packed-usb.raw [pair A-D] [max-bytes]\n";
    return 2;
  }
  try {
    const std::string reference_path = argv[1];
    const std::string usb_path = argv[2];
    int pair_index = 0;
    if (argc > 3) {
      const char pair = static_cast<char>(std::toupper(argv[3][0]));
      if (pair < 'A' || pair > 'D') {
        throw std::runtime_error("pair must be A-D");
      }
      pair_index = pair - 'A';
    }
    std::size_t max_bytes = 6U * 1024U * 1024U;
    if (argc > 4) {
      max_bytes = static_cast<std::size_t>(std::stoull(argv[4]));
    }
    const auto reference = Mono(ReadWav16Stereo(reference_path), 48000);
    const auto raw = ReadBytes(usb_path, max_bytes);
    std::cout << "reference=" << reference_path << "\n";
    std::cout << "usb_raw=" << usb_path << "\n";
    std::cout << "raw_bytes=" << raw.size() << "\n";
    for (const int check_offset : {0, 8}) {
      for (int start_byte = 0; start_byte < 6; ++start_byte) {
        for (const bool native_order : {false, true}) {
          const auto decoded = DecodeMode2(raw, check_offset, start_byte, native_order, pair_index);
          const auto got = Mono(decoded.pair, 180000);
          const auto score = BestCorrelation(reference, got);
          std::cout << "candidate"
                    << " check_offset=" << check_offset
                    << " start_byte=" << start_byte
                    << " byte_order=" << (native_order ? "native" : "big")
                    << " frames=" << decoded.pair.size()
                    << " checks=" << decoded.checks
                    << " check_errors=" << decoded.check_errors
                    << " panic_flags=" << decoded.panic_flags
                    << " abs_corr=" << score.abs_corr
                    << " corr=" << score.corr
                    << " lag=" << score.lag
                    << " rms=" << score.rms
                    << " peak=" << score.peak
                    << "\n";
        }
      }
    }
  } catch (const std::exception& ex) {
    std::cerr << "error=" << ex.what() << "\n";
    return 1;
  }
  return 0;
}
