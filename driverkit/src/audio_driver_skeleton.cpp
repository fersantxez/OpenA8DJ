#include "opena8djcpp/driverkit/audio_driver_skeleton.hpp"

namespace opena8djcpp::driverkit {

bool AudioDriverSkeleton::start_driver() {
  if (state_ == AudioDriverState::Started ||
      !validate_driverkit_device_model(device_model_)) {
    return false;
  }
  state_ = AudioDriverState::Started;
  return true;
}

bool AudioDriverSkeleton::stop_driver() {
  if (state_ != AudioDriverState::Started) {
    return false;
  }
  if (transport_.started()) {
    finish_usb_submit_binding();
    transport_.stop();
  }
  stream_config_ = {};
  stream_configured_ = false;
  state_ = AudioDriverState::Stopped;
  return true;
}

bool AudioDriverSkeleton::configure_stream(const AudioStreamConfig& config) {
  if (state_ != AudioDriverState::Started || transport_.started() ||
      !validate_stream_config(config)) {
    return false;
  }
  stream_config_ = config;
  stream_configured_ = true;
  return true;
}

bool AudioDriverSkeleton::request_configuration_change(const AudioStreamConfig& config) {
  if (state_ != AudioDriverState::Started || transport_.started() ||
      !validate_stream_config(config)) {
    runtime_counters_.rejected_configuration_changes += 1;
    return false;
  }
  stream_config_ = config;
  stream_configured_ = true;
  runtime_counters_.configuration_changes += 1;
  return true;
}

bool AudioDriverSkeleton::start_stream() {
  if (state_ != AudioDriverState::Started || !stream_configured_ || transport_.started()) {
    return false;
  }
  if (!transport_.start(stream_config_.transport)) {
    return false;
  }
  if (!start_usb_submit_binding()) {
    transport_.stop();
    return false;
  }
  have_zero_timestamp_ = false;
  zero_timestamp_ = {};
  return true;
}

bool AudioDriverSkeleton::stop_stream() {
  if (!transport_.started()) {
    return false;
  }
  finish_usb_submit_binding();
  transport_.stop();
  return true;
}

bool AudioDriverSkeleton::stream_started() const {
  return transport_.started();
}

std::array<AudioIOMemoryDescriptorModel, 5> AudioDriverSkeleton::io_memory_layout() {
  std::array<AudioIOMemoryDescriptorModel, 5> layout{};
  for (std::size_t index = 0; index < layout.size(); ++index) {
    const auto& stream = device_model_.streams[index];
    layout[index] = AudioIOMemoryDescriptorModel{
        .direction = stream.direction,
        .starting_channel = stream.starting_channel,
        .channel_count = stream.channel_count,
        .frames = stream_config_.buffer_frames,
        .bytes_per_sample = sizeof(float),
    };
  }
  runtime_counters_.io_memory_layout_builds += 1;
  if (!validate_io_memory_layout(layout)) {
    runtime_counters_.io_memory_layout_failures += 1;
  }
  return layout;
}

bool AudioDriverSkeleton::update_zero_timestamp(std::uint64_t sample_time,
                                                std::uint64_t host_time) {
  if (!transport_.started()) {
    runtime_counters_.zero_timestamp_regressions += 1;
    return false;
  }
  if (have_zero_timestamp_ &&
      (sample_time <= zero_timestamp_.sample_time || host_time <= zero_timestamp_.host_time)) {
    runtime_counters_.zero_timestamp_regressions += 1;
    return false;
  }
  zero_timestamp_ = ZeroTimestampModel{.sample_time = sample_time, .host_time = host_time};
  have_zero_timestamp_ = true;
  runtime_counters_.zero_timestamp_updates += 1;
  return true;
}

bool AudioDriverSkeleton::write_playback(std::span<const S24Frame> frames) {
  return transport_.hal_write_playback(frames) == frames.size();
}

std::uint32_t AudioDriverSkeleton::read_capture(std::span<S24Frame> frames) {
  return transport_.hal_read_capture(frames);
}

bool AudioDriverSkeleton::complete_backend_period(std::span<const S24Frame> capture_frames,
                                                  std::span<S24Frame> playback_frames,
                                                  std::uint64_t sample_timestamp) {
  const bool transport_ok =
      transport_.backend_complete_period(capture_frames, playback_frames, sample_timestamp);
  return transport_ok && queue_usb_slots_for_period(capture_frames, playback_frames);
}

void AudioDriverSkeleton::finish_usb_submit_binding() {
  usb_submit_planner_.finish();
}

AudioDriverState AudioDriverSkeleton::state() const {
  return state_;
}

const DriverKitDeviceModel& AudioDriverSkeleton::device_model() const {
  return device_model_;
}

const AudioStreamConfig& AudioDriverSkeleton::stream_config() const {
  return stream_config_;
}

const PreparedTransportCounters& AudioDriverSkeleton::transport_counters() const {
  return transport_.counters();
}

const DriverKitRuntimeCounters& AudioDriverSkeleton::runtime_counters() const {
  return runtime_counters_;
}

const PreparedUsbSubmitPlannerCounters& AudioDriverSkeleton::usb_submit_counters() const {
  return usb_submit_planner_.counters();
}

std::span<const UsbSubmitDescriptor> AudioDriverSkeleton::usb_submit_descriptors() const {
  return usb_submit_planner_.descriptors();
}

ZeroTimestampModel AudioDriverSkeleton::zero_timestamp() const {
  return zero_timestamp_;
}

PreparedTransportSafety AudioDriverSkeleton::transport_safety() const {
  return transport_.safety();
}

PreparedUsbSubmitPlannerSafety AudioDriverSkeleton::usb_submit_safety() const {
  return usb_submit_planner_.safety();
}

bool AudioDriverSkeleton::validate_stream_config(const AudioStreamConfig& config) const {
  const bool sample_rate_valid =
      config.sample_rate == device_model_.sample_rates[0] ||
      config.sample_rate == device_model_.sample_rates[1];
  return sample_rate_valid && config.buffer_frames > 0 && config.buffer_frames <= 4096 &&
         config.transport.iso_frames > 0 && config.transport.capture_slots > 0 &&
         config.transport.playback_slots > 0 &&
         config.transport.capture_slots <= kPreparedTransportMaxSlots &&
         config.transport.playback_slots <= kPreparedTransportMaxSlots &&
         config.usb_slots_per_submit > 0 &&
         config.usb_slots_per_submit <= kPreparedTransportMaxSlots &&
         config.usb_bytes_per_slot > 0 &&
         config.usb_initial_capture_slots <= kPreparedTransportMaxSlots &&
         config.usb_initial_playback_slots <= kPreparedTransportMaxSlots;
}

bool AudioDriverSkeleton::validate_io_memory_layout(
    const std::array<AudioIOMemoryDescriptorModel, 5>& layout) const {
  if (!stream_configured_) {
    return false;
  }
  std::uint32_t input_channels = 0;
  std::uint32_t output_channels = 0;
  for (std::size_t index = 0; index < layout.size(); ++index) {
    const auto& descriptor = layout[index];
    const auto& stream = device_model_.streams[index];
    if (descriptor.direction != stream.direction ||
        descriptor.starting_channel != stream.starting_channel ||
        descriptor.channel_count != stream.channel_count ||
        descriptor.frames != stream_config_.buffer_frames ||
        descriptor.bytes_per_sample != sizeof(float) || descriptor.byte_count() == 0) {
      return false;
    }
    if (descriptor.direction == StreamDirection::Input) {
      input_channels += descriptor.channel_count;
    } else {
      output_channels += descriptor.channel_count;
    }
  }
  return input_channels == kInputChannels && output_channels == kOutputChannels;
}

bool AudioDriverSkeleton::start_usb_submit_binding() {
  next_capture_usb_sequence_ = 0;
  next_playback_usb_sequence_ = 0;
  if (!usb_submit_planner_.start(PreparedUsbSubmitPlannerConfig{
          .slots_per_submit = stream_config_.usb_slots_per_submit,
          .frames_per_slot = stream_config_.transport.iso_frames,
          .bytes_per_slot = stream_config_.usb_bytes_per_slot,
      })) {
    return false;
  }
  return queue_initial_usb_slots();
}

bool AudioDriverSkeleton::queue_initial_usb_slots() {
  for (std::uint32_t index = 0; index < stream_config_.usb_initial_capture_slots; ++index) {
    if (!queue_usb_slot(UsbSlotDirection::Capture)) {
      return false;
    }
  }
  for (std::uint32_t index = 0; index < stream_config_.usb_initial_playback_slots; ++index) {
    if (!queue_usb_slot(UsbSlotDirection::Playback)) {
      return false;
    }
  }
  return true;
}

bool AudioDriverSkeleton::queue_usb_slots_for_period(std::span<const S24Frame> capture_frames,
                                                     std::span<const S24Frame> playback_frames) {
  if (!usb_submit_planner_.started() || stream_config_.transport.iso_frames == 0 ||
      capture_frames.size() != playback_frames.size() ||
      (capture_frames.size() % stream_config_.transport.iso_frames) != 0) {
    return false;
  }
  const auto slots = capture_frames.size() / stream_config_.transport.iso_frames;
  for (std::size_t slot = 0; slot < slots; ++slot) {
    if (!queue_usb_slot(UsbSlotDirection::Capture) ||
        !queue_usb_slot(UsbSlotDirection::Playback)) {
      return false;
    }
  }
  return true;
}

bool AudioDriverSkeleton::queue_usb_slot(UsbSlotDirection direction) {
  return usb_submit_planner_.queue_slot(direction, next_usb_timestamp(direction));
}

std::uint64_t AudioDriverSkeleton::next_usb_timestamp(UsbSlotDirection direction) {
  auto& sequence =
      direction == UsbSlotDirection::Capture ? next_capture_usb_sequence_
                                             : next_playback_usb_sequence_;
  sequence += 1;
  return sequence * stream_config_.transport.iso_frames;
}

}  // namespace opena8djcpp::driverkit
