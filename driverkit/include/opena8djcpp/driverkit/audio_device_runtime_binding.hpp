#pragma once

#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"

#include <array>
#include <cstdint>

namespace opena8djcpp::driverkit {

struct AudioDeviceRuntimeBindingCounters {
  std::uint64_t configure_device_calls = 0;
  std::uint64_t configure_device_failures = 0;
  std::uint64_t start_io_calls = 0;
  std::uint64_t start_io_failures = 0;
  std::uint64_t stop_io_calls = 0;
  std::uint64_t stop_io_failures = 0;
  std::uint64_t stop_io_idempotent_noops = 0;
  std::uint64_t stream_memory_publications = 0;
  std::uint64_t stream_memory_publication_failures = 0;
  std::uint64_t zero_timestamp_publications = 0;
  std::uint64_t zero_timestamp_publication_failures = 0;
  std::uint64_t configuration_change_accepts = 0;
  std::uint64_t configuration_change_rejects = 0;
  std::uint64_t configuration_abort_calls = 0;
  std::uint64_t shutdown_calls = 0;
  std::uint64_t shutdown_failures = 0;
};

class AudioDeviceRuntimeBinding {
 public:
  explicit AudioDeviceRuntimeBinding(AudioDriverSkeleton& driver);

  [[nodiscard]] bool configure_device(const AudioStreamConfig& config);
  [[nodiscard]] bool start_io(std::uint64_t sample_time, std::uint64_t host_time);
  [[nodiscard]] bool stop_io();
  [[nodiscard]] bool request_configuration_change(const AudioStreamConfig& config);
  [[nodiscard]] bool abort_configuration_change();
  [[nodiscard]] bool shutdown_driver();

  [[nodiscard]] bool stream_memory_bound() const;
  [[nodiscard]] bool zero_timestamp_published() const;
  [[nodiscard]] const std::array<AudioIOMemoryDescriptorModel, 5>& stream_memory_layout() const;
  [[nodiscard]] const AudioDeviceRuntimeBindingCounters& counters() const;

 private:
  [[nodiscard]] bool publish_stream_memory_layout();
  [[nodiscard]] bool layout_valid(
      const std::array<AudioIOMemoryDescriptorModel, 5>& layout) const;

  AudioDriverSkeleton& driver_;
  std::array<AudioIOMemoryDescriptorModel, 5> stream_memory_layout_{};
  AudioDeviceRuntimeBindingCounters counters_{};
  bool stream_memory_bound_ = false;
  bool zero_timestamp_published_ = false;
};

}  // namespace opena8djcpp::driverkit
