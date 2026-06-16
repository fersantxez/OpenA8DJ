#include "opena8djcpp/audio_model.hpp"
#include "opena8djcpp/mode2_packet.hpp"
#include "opena8djcpp/policies.hpp"
#include "opena8djcpp/protocol.hpp"

#include <cstdint>
#include <iostream>

using namespace opena8djcpp;

namespace {

void print_rate_row(std::uint32_t rate, bool advertised) {
  std::cout << "    {\"sample_rate\": " << rate << ", \"caiaq_rate_code\": "
            << static_cast<std::uint32_t>(caiaq_rate_code(rate))
            << ", \"advertised\": " << (advertised ? "true" : "false") << "}";
}

}  // namespace

int main() {
  const auto surface = make_audio8dj_surface();
  const bool constants_ok = kNativeInstrumentsVendorId == 0x17cc &&
                            kAudio8DjProductId == 0x1978 &&
                            kUsbInterfaceNumber == 0 &&
                            kUsbConfigurationValue == 1 &&
                            kUsbAlternateSetting == 1 &&
                            kEndpointControlOut == 0x01 &&
                            kEndpointControlIn == 0x81 &&
                            kEndpointIsoCapture == 0x82 &&
                            kEndpointIsoPlayback == 0x06 &&
                            kCommandGetDeviceInfo == 0x01 &&
                            kCommandReadIo == 0x04 &&
                            kCommandWriteIo == 0x05 &&
                            kCommandAudioParams == 0x09 &&
                            kCommandMidiRead == 0x06 &&
                            kCommandMidiWrite == 0x07 &&
                            kCommandAutoMsg == 0x0b;
  const bool mode2_ok = kMode2Streams == 4 &&
                        kMode2GroupBytes == 16 &&
                        kMode2FullFrameBytes == 32 &&
                        kMode2BytesPerSample == 3 &&
                        kMode2UsbBytesPerSample == 4 &&
                        kMode2CheckOffset == 8 &&
                        kMode2DefaultStartByte == 4 &&
                        kMode2DefaultTransferBytes == 352;
  const bool surface_ok = surface.input_channels == 8 &&
                          surface.output_channels == 8 &&
                          kStereoPairs == 4 &&
                          kChannelsPerPair == 2;
  const bool rates_ok = SampleRatePolicy::is_supported(44100) &&
                        SampleRatePolicy::is_supported(48000) &&
                        !SampleRatePolicy::is_supported(88200) &&
                        !SampleRatePolicy::is_supported(96000) &&
                        caiaq_rate_code(44100) == 0 &&
                        caiaq_rate_code(48000) == 1 &&
                        caiaq_rate_code(96000) == 2 &&
                        caiaq_rate_code(88200) == 4;
  const bool pass = constants_ok && mode2_ok && surface_ok && rates_ok;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.protocol-contract.v1\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"source_references\": [\"mainline OpenA8DJUSB.m\", \"rust probe/README\"],\n"
            << "  \"usb\": {\n"
            << "    \"vendor_id\": \"0x17cc\",\n"
            << "    \"product_id\": \"0x1978\",\n"
            << "    \"interface\": " << static_cast<std::uint32_t>(kUsbInterfaceNumber) << ",\n"
            << "    \"configuration\": " << static_cast<std::uint32_t>(kUsbConfigurationValue) << ",\n"
            << "    \"alternate_setting\": " << static_cast<std::uint32_t>(kUsbAlternateSetting)
            << ",\n"
            << "    \"endpoints\": {\n"
            << "      \"control_out\": \"0x01\",\n"
            << "      \"control_in\": \"0x81\",\n"
            << "      \"iso_capture\": \"0x82\",\n"
            << "      \"iso_playback\": \"0x06\"\n"
            << "    }\n"
            << "  },\n"
            << "  \"commands\": {\n"
            << "    \"get_device_info\": \"0x01\",\n"
            << "    \"read_io\": \"0x04\",\n"
            << "    \"write_io\": \"0x05\",\n"
            << "    \"audio_params\": \"0x09\",\n"
            << "    \"midi_read\": \"0x06\",\n"
            << "    \"midi_write\": \"0x07\",\n"
            << "    \"auto_msg\": \"0x0b\"\n"
            << "  },\n"
            << "  \"mode2\": {\n"
            << "    \"streams\": " << kMode2Streams << ",\n"
            << "    \"channels_per_stream\": " << kChannelsPerPair << ",\n"
            << "    \"check_cadence_bytes\": " << kMode2GroupBytes << ",\n"
            << "    \"full_frame_bytes\": " << kMode2FullFrameBytes << ",\n"
            << "    \"bytes_per_sample\": " << kMode2BytesPerSample << ",\n"
            << "    \"usb_bytes_per_sample\": " << kMode2UsbBytesPerSample << ",\n"
            << "    \"check_offset\": " << kMode2CheckOffset << ",\n"
            << "    \"default_start_byte\": " << kMode2DefaultStartByte << ",\n"
            << "    \"default_transfer_bytes\": " << kMode2DefaultTransferBytes << "\n"
            << "  },\n"
            << "  \"surface\": {\n"
            << "    \"input_channels\": " << surface.input_channels << ",\n"
            << "    \"output_channels\": " << surface.output_channels << ",\n"
            << "    \"stereo_pairs\": " << kStereoPairs << ",\n"
            << "    \"channels_per_pair\": " << kChannelsPerPair << "\n"
            << "  },\n"
            << "  \"sample_rates\": [\n";
  print_rate_row(44100, true);
  std::cout << ",\n";
  print_rate_row(48000, true);
  std::cout << ",\n";
  print_rate_row(88200, false);
  std::cout << ",\n";
  print_rate_row(96000, false);
  std::cout << "\n"
            << "  ],\n"
            << "  \"deferred_rates_reason\": \"88200 and 96000 are known CAIAQ codes but not advertised until physical quality gates pass\",\n"
            << "  \"checks\": {\n"
            << "    \"constants_ok\": " << (constants_ok ? "true" : "false") << ",\n"
            << "    \"mode2_ok\": " << (mode2_ok ? "true" : "false") << ",\n"
            << "    \"surface_ok\": " << (surface_ok ? "true" : "false") << ",\n"
            << "    \"rates_ok\": " << (rates_ok ? "true" : "false") << "\n"
            << "  }\n"
            << "}\n";

  return pass ? 0 : 1;
}
