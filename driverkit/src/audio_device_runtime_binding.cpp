#include "opena8djcpp/driverkit/audio_device_runtime_binding.hpp"

namespace opena8djcpp::driverkit {

AudioDeviceRuntimeBinding::AudioDeviceRuntimeBinding(AudioDriverSkeleton& driver)
    : driver_(driver) {}

bool AudioDeviceRuntimeBinding::configure_device(const AudioStreamConfig& config) {
  counters_.configure_device_calls += 1;
  stream_memory_bound_ = false;
  zero_timestamp_published_ = false;
  stream_memory_layout_ = {};
  if (!driver_.configure_stream(config)) {
    counters_.configure_device_failures += 1;
    return false;
  }
  return publish_stream_memory_layout();
}

bool AudioDeviceRuntimeBinding::start_io(std::uint64_t sample_time, std::uint64_t host_time) {
  counters_.start_io_calls += 1;
  zero_timestamp_published_ = false;
  if (!stream_memory_bound_ || !driver_.start_stream()) {
    counters_.start_io_failures += 1;
    return false;
  }
  if (!driver_.update_zero_timestamp(sample_time, host_time)) {
    counters_.zero_timestamp_publication_failures += 1;
    counters_.start_io_failures += 1;
    if (driver_.stream_started()) {
      (void)driver_.stop_stream();
    }
    return false;
  }
  zero_timestamp_published_ = true;
  counters_.zero_timestamp_publications += 1;
  return true;
}

bool AudioDeviceRuntimeBinding::stop_io() {
  counters_.stop_io_calls += 1;
  zero_timestamp_published_ = false;
  if (!driver_.stream_started()) {
    counters_.stop_io_idempotent_noops += 1;
    return true;
  }
  if (!driver_.stop_stream()) {
    counters_.stop_io_failures += 1;
    return false;
  }
  return true;
}

bool AudioDeviceRuntimeBinding::request_configuration_change(const AudioStreamConfig& config) {
  stream_memory_bound_ = false;
  zero_timestamp_published_ = false;
  stream_memory_layout_ = {};
  if (!driver_.request_configuration_change(config)) {
    counters_.configuration_change_rejects += 1;
    return false;
  }
  counters_.configuration_change_accepts += 1;
  return publish_stream_memory_layout();
}

bool AudioDeviceRuntimeBinding::abort_configuration_change() {
  counters_.configuration_abort_calls += 1;
  return true;
}

bool AudioDeviceRuntimeBinding::shutdown_driver() {
  counters_.shutdown_calls += 1;
  bool ok = true;
  if (driver_.stream_started()) {
    ok = stop_io() && ok;
  }
  if (driver_.state() == AudioDriverState::Started) {
    ok = driver_.stop_driver() && ok;
  }
  if (!ok) {
    counters_.shutdown_failures += 1;
  }
  stream_memory_bound_ = false;
  zero_timestamp_published_ = false;
  stream_memory_layout_ = {};
  return ok;
}

bool AudioDeviceRuntimeBinding::stream_memory_bound() const {
  return stream_memory_bound_;
}

bool AudioDeviceRuntimeBinding::zero_timestamp_published() const {
  return zero_timestamp_published_;
}

const std::array<AudioIOMemoryDescriptorModel, 5>&
AudioDeviceRuntimeBinding::stream_memory_layout() const {
  return stream_memory_layout_;
}

const AudioDeviceRuntimeBindingCounters& AudioDeviceRuntimeBinding::counters() const {
  return counters_;
}

bool AudioDeviceRuntimeBinding::publish_stream_memory_layout() {
  stream_memory_layout_ = driver_.io_memory_layout();
  if (!layout_valid(stream_memory_layout_)) {
    counters_.stream_memory_publication_failures += 1;
    stream_memory_bound_ = false;
    return false;
  }
  counters_.stream_memory_publications += 1;
  stream_memory_bound_ = true;
  return true;
}

bool AudioDeviceRuntimeBinding::layout_valid(
    const std::array<AudioIOMemoryDescriptorModel, 5>& layout) const {
  std::uint32_t input_channels = 0;
  std::uint32_t output_channels = 0;
  std::uint64_t total_bytes = 0;
  for (const auto& descriptor : layout) {
    total_bytes += descriptor.byte_count();
    if (descriptor.byte_count() == 0 || descriptor.frames != driver_.stream_config().buffer_frames ||
        descriptor.bytes_per_sample != sizeof(float)) {
      return false;
    }
    if (descriptor.direction == StreamDirection::Input) {
      input_channels += descriptor.channel_count;
    } else {
      output_channels += descriptor.channel_count;
    }
  }
  return input_channels == kInputChannels && output_channels == kOutputChannels &&
         total_bytes == (static_cast<std::uint64_t>(kInputChannels + kOutputChannels) *
                         driver_.stream_config().buffer_frames * sizeof(float));
}

}  // namespace opena8djcpp::driverkit
