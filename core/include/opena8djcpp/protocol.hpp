#pragma once

#include <array>
#include <cstdint>

namespace opena8djcpp {

inline constexpr std::uint16_t kNativeInstrumentsVendorId = 0x17cc;
inline constexpr std::uint16_t kAudio8DjProductId = 0x1978;

inline constexpr std::uint8_t kUsbInterfaceNumber = 0;
inline constexpr std::uint8_t kUsbConfigurationValue = 1;
inline constexpr std::uint8_t kUsbAlternateSetting = 1;

inline constexpr std::uint8_t kEndpointControlOut = 0x01;
inline constexpr std::uint8_t kEndpointControlIn = 0x81;
inline constexpr std::uint8_t kEndpointIsoCapture = 0x82;
inline constexpr std::uint8_t kEndpointIsoPlayback = 0x06;

inline constexpr std::uint8_t kCommandGetDeviceInfo = 0x01;
inline constexpr std::uint8_t kCommandReadIo = 0x04;
inline constexpr std::uint8_t kCommandWriteIo = 0x05;
inline constexpr std::uint8_t kCommandAudioParams = 0x09;
inline constexpr std::uint8_t kCommandMidiRead = 0x06;
inline constexpr std::uint8_t kCommandMidiWrite = 0x07;
inline constexpr std::uint8_t kCommandAutoMsg = 0x0b;

inline constexpr std::array<std::uint32_t, 2> kValidatedSampleRates{44100, 48000};
inline constexpr std::array<std::uint32_t, 2> kDeferredSampleRates{88200, 96000};

[[nodiscard]] constexpr std::uint8_t caiaq_rate_code(std::uint32_t sample_rate) {
  switch (sample_rate) {
    case 44100:
      return 0;
    case 48000:
      return 1;
    case 96000:
      return 2;
    case 88200:
      return 4;
    default:
      return 0xff;
  }
}

}  // namespace opena8djcpp
