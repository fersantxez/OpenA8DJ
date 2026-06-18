#include "opena8djcpp/driverkit/audio_device_runtime_binding.hpp"

#include <cstdint>
#include <iostream>

using namespace opena8djcpp;
using namespace opena8djcpp::driverkit;

namespace {

AudioStreamConfig config_for(std::uint32_t sample_rate, std::uint32_t buffer_frames) {
  return AudioStreamConfig{
      .sample_rate = sample_rate,
      .buffer_frames = buffer_frames,
      .transport = PreparedTransportConfig{.iso_frames = 8, .capture_slots = 16, .playback_slots = 16},
      .usb_slots_per_submit = 8,
      .usb_bytes_per_slot = kMode2DefaultTransferBytes,
      .usb_initial_capture_slots = 8,
      .usb_initial_playback_slots = 8,
      .usb_request_slots = 8,
      .usb_request_completion_depth = 4,
      .usb_retain_submit_descriptors = false,
  };
}

std::uint64_t layout_total_bytes(
    const std::array<AudioIOMemoryDescriptorModel, 5>& layout) {
  std::uint64_t total = 0;
  for (const auto& descriptor : layout) {
    total += descriptor.byte_count();
  }
  return total;
}

}  // namespace

int main() {
  AudioDriverSkeleton driver;
  AudioDeviceRuntimeBinding binding(driver);
  const auto cfg480 = config_for(48000, 64);
  const auto cfg441 = config_for(44100, 128);

  std::uint32_t lifecycle_failures = 0;
  std::uint32_t memory_failures = 0;
  std::uint32_t timestamp_failures = 0;
  std::uint32_t config_failures = 0;
  std::uint32_t shutdown_failures = 0;

  if (binding.configure_device(cfg480)) {
    lifecycle_failures += 1;
  }
  if (binding.start_io(0, 1000)) {
    lifecycle_failures += 1;
  }
  if (!driver.start_driver()) {
    lifecycle_failures += 1;
  }
  if (!binding.configure_device(cfg480) || !binding.stream_memory_bound()) {
    lifecycle_failures += 1;
  }
  const auto initial_layout = binding.stream_memory_layout();
  if (initial_layout.size() != 5 || layout_total_bytes(initial_layout) != 4096U) {
    memory_failures += 1;
  }
  if (initial_layout[0].direction != StreamDirection::Input ||
      initial_layout[0].channel_count != 8 ||
      initial_layout[0].frames != 64 ||
      initial_layout[0].byte_count() != 2048U) {
    memory_failures += 1;
  }
  for (std::size_t index = 1; index < initial_layout.size(); ++index) {
    if (initial_layout[index].direction != StreamDirection::Output ||
        initial_layout[index].channel_count != 2 ||
        initial_layout[index].starting_channel != static_cast<std::uint32_t>((index - 1U) * 2U) ||
        initial_layout[index].frames != 64 ||
        initial_layout[index].byte_count() != 512U) {
      memory_failures += 1;
    }
  }
  if (!binding.start_io(0, 1000) || !driver.stream_started() ||
      !binding.zero_timestamp_published()) {
    lifecycle_failures += 1;
  }
  if (driver.zero_timestamp().sample_time != 0 || driver.zero_timestamp().host_time != 1000) {
    timestamp_failures += 1;
  }
  if (binding.start_io(8, 2000)) {
    lifecycle_failures += 1;
  }
  if (binding.request_configuration_change(cfg441)) {
    config_failures += 1;
  }
  if (!binding.abort_configuration_change()) {
    config_failures += 1;
  }
  if (!binding.stop_io() || driver.stream_started() || binding.zero_timestamp_published()) {
    shutdown_failures += 1;
  }
  if (!binding.stop_io()) {
    shutdown_failures += 1;
  }
  if (!binding.request_configuration_change(cfg441) || !binding.stream_memory_bound()) {
    config_failures += 1;
  }
  const auto changed_layout = binding.stream_memory_layout();
  if (layout_total_bytes(changed_layout) != 8192U || changed_layout[0].frames != 128 ||
      changed_layout[1].frames != 128) {
    memory_failures += 1;
  }
  if (!binding.start_io(128, 3000) || !driver.stream_started() ||
      !binding.zero_timestamp_published()) {
    lifecycle_failures += 1;
  }
  if (driver.zero_timestamp().sample_time != 128 || driver.zero_timestamp().host_time != 3000) {
    timestamp_failures += 1;
  }
  if (!binding.shutdown_driver() || driver.state() != AudioDriverState::Stopped ||
      driver.stream_started() || binding.stream_memory_bound() ||
      binding.zero_timestamp_published()) {
    shutdown_failures += 1;
  }

  const auto counters = binding.counters();
  const auto runtime = driver.runtime_counters();
  const bool counters_pass =
      counters.configure_device_calls == 2 &&
      counters.configure_device_failures == 1 &&
      counters.start_io_calls == 4 &&
      counters.start_io_failures == 2 &&
      counters.stop_io_calls == 3 &&
      counters.stop_io_failures == 0 &&
      counters.stop_io_idempotent_noops == 1 &&
      counters.stream_memory_publications == 2 &&
      counters.stream_memory_publication_failures == 0 &&
      counters.zero_timestamp_publications == 2 &&
      counters.zero_timestamp_publication_failures == 0 &&
      counters.configuration_change_accepts == 1 &&
      counters.configuration_change_rejects == 1 &&
      counters.configuration_abort_calls == 1 &&
      counters.shutdown_calls == 1 &&
      counters.shutdown_failures == 0 &&
      runtime.configuration_changes == 1 &&
      runtime.rejected_configuration_changes == 1 &&
      runtime.zero_timestamp_updates == 2 &&
      runtime.zero_timestamp_regressions == 0;

  const bool pass = lifecycle_failures == 0 && memory_failures == 0 &&
                    timestamp_failures == 0 && config_failures == 0 &&
                    shutdown_failures == 0 && counters_pass;

  std::cout
      << "{\n"
      << "  \"schema\": \"opena8djcpp.driverkit-device-binding-contract.v1\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"meaning\": \"offline IOUserAudioDevice runtime binding model; PASS is not dext build or hardware readiness\",\n"
      << "  \"safety\": \"offline_pure_cpp_only_no_driverkit_install_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"lifecycle_failures\": " << lifecycle_failures << ",\n"
      << "  \"memory_failures\": " << memory_failures << ",\n"
      << "  \"timestamp_failures\": " << timestamp_failures << ",\n"
      << "  \"config_failures\": " << config_failures << ",\n"
      << "  \"shutdown_failures\": " << shutdown_failures << ",\n"
      << "  \"initial_io_memory_descriptors\": " << initial_layout.size() << ",\n"
      << "  \"initial_io_memory_total_bytes\": " << layout_total_bytes(initial_layout) << ",\n"
      << "  \"changed_io_memory_total_bytes\": " << layout_total_bytes(changed_layout) << ",\n"
      << "  \"stream_memory_publications\": " << counters.stream_memory_publications << ",\n"
      << "  \"zero_timestamp_publications\": " << counters.zero_timestamp_publications << ",\n"
      << "  \"configuration_change_accepts\": " << counters.configuration_change_accepts << ",\n"
      << "  \"configuration_change_rejects\": " << counters.configuration_change_rejects << ",\n"
      << "  \"stop_io_idempotent_noops\": " << counters.stop_io_idempotent_noops << ",\n"
      << "  \"runtime_zero_timestamp_updates\": " << runtime.zero_timestamp_updates << ",\n"
      << "  \"runtime_zero_timestamp_regressions\": " << runtime.zero_timestamp_regressions << ",\n"
      << "  \"runtime_configuration_changes\": " << runtime.configuration_changes << ",\n"
      << "  \"runtime_rejected_configuration_changes\": "
      << runtime.rejected_configuration_changes << ",\n"
      << "  \"product_driverkit_runtime_ready\": false,\n"
      << "  \"next_required_action\": \"WIRE_AUDIO_DEVICE_RUNTIME_BINDING_INTO_DRIVERKIT_EXTENSION_SOURCE_AND_BUILD_WITH_DRIVERKIT_SDK\",\n"
      << "  \"blocked_claim\": \"NO_DRIVERKIT_RUNTIME_READY_CLAIM_UNTIL_BINDING_IS_COMPILED_IN_REAL_DEXT_AND_PHYSICALLY_VALIDATED\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
