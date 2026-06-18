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
    (void)cancel_usb_requests();
    usb_submit_planner_.stop();
    usb_request_pool_.stop();
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
  if (persistent_usb_transport_enabled() ? !start_persistent_usb_transport_binding()
                                         : !start_usb_submit_binding()) {
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
  if (!persistent_usb_transport_enabled()) {
    (void)cancel_usb_requests();
    usb_submit_planner_.stop();
    usb_request_pool_.stop();
  }
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
  if (!transport_ok) {
    return false;
  }
  return persistent_usb_transport_enabled()
             ? complete_persistent_usb_transport_period(capture_frames, playback_frames)
             : queue_usb_slots_for_period(capture_frames, playback_frames);
}

void AudioDriverSkeleton::finish_usb_submit_binding() {
  if (persistent_usb_transport_enabled()) {
    if (persistent_usb_transport_.started()) {
      (void)persistent_usb_transport_.drain();
      persistent_usb_transport_.stop();
    }
    return;
  }
  usb_submit_planner_.finish();
  if (!drain_new_usb_submit_descriptors()) {
    runtime_counters_.usb_request_drain_failures += 1;
  }
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

const PreparedUsbRequestPoolCounters& AudioDriverSkeleton::usb_request_counters() const {
  return usb_request_pool_.counters();
}

PersistentUsbTransportCounters AudioDriverSkeleton::persistent_usb_transport_counters() const {
  return persistent_usb_transport_.counters();
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

PreparedUsbRequestPoolSafety AudioDriverSkeleton::usb_request_safety() const {
  return usb_request_pool_.safety();
}

PersistentUsbTransportSafety AudioDriverSkeleton::persistent_usb_transport_safety() const {
  return persistent_usb_transport_.safety();
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
         config.usb_initial_playback_slots <= kPreparedTransportMaxSlots &&
         config.usb_request_slots > 0 &&
         config.usb_request_slots <= kPreparedUsbRequestMaxSlots &&
         config.usb_request_completion_depth > 0 &&
         config.usb_request_completion_depth <= config.usb_request_slots &&
         config.usb_capture_bytes_per_slot > 0 &&
         (!config.use_persistent_usb_transport ||
          (config.persistent_capture_queue_depth > 0 &&
           config.persistent_playback_queue_depth > 0 &&
           config.persistent_capture_queue_depth + config.persistent_playback_queue_depth <=
               config.usb_request_slots &&
           config.persistent_capture_queue_depth + config.persistent_playback_queue_depth <=
               kPreparedUsbRequestMaxSlots));
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

bool AudioDriverSkeleton::persistent_usb_transport_enabled() const {
  return stream_config_.use_persistent_usb_transport;
}

bool AudioDriverSkeleton::start_usb_submit_binding() {
  next_capture_usb_sequence_ = 0;
  next_playback_usb_sequence_ = 0;
  submitted_usb_descriptor_count_ = 0;
  inflight_usb_request_head_ = 0;
  inflight_usb_request_count_ = 0;
  if (!usb_request_pool_.start(PreparedUsbRequestPoolConfig{
          .request_slots = stream_config_.usb_request_slots,
      })) {
    return false;
  }
  if (!usb_submit_planner_.start(PreparedUsbSubmitPlannerConfig{
          .slots_per_submit = stream_config_.usb_slots_per_submit,
          .frames_per_slot = stream_config_.transport.iso_frames,
          .bytes_per_slot = stream_config_.usb_bytes_per_slot,
      })) {
    usb_request_pool_.stop();
    return false;
  }
  if (!queue_initial_usb_slots()) {
    finish_usb_submit_binding();
    usb_submit_planner_.stop();
    usb_request_pool_.stop();
    return false;
  }
  return true;
}

bool AudioDriverSkeleton::start_persistent_usb_transport_binding() {
  if (!persistent_usb_transport_.start(PersistentUsbTransportConfig{
          .request_pool = PreparedUsbRequestPoolConfig{
              .request_slots = stream_config_.usb_request_slots,
          },
          .slots_per_submit = stream_config_.usb_slots_per_submit,
          .frames_per_slot = stream_config_.transport.iso_frames,
          .capture_bytes_per_slot = stream_config_.usb_capture_bytes_per_slot,
          .playback_bytes_per_slot = stream_config_.usb_bytes_per_slot,
          .capture_queue_depth = stream_config_.persistent_capture_queue_depth,
          .playback_queue_depth = stream_config_.persistent_playback_queue_depth,
      })) {
    return false;
  }
  if (!persistent_usb_transport_.prime()) {
    persistent_usb_transport_.stop();
    return false;
  }
  return true;
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

bool AudioDriverSkeleton::complete_persistent_usb_transport_period(
    std::span<const S24Frame> capture_frames,
    std::span<const S24Frame> playback_frames) {
  if (!persistent_usb_transport_.started() || stream_config_.transport.iso_frames == 0 ||
      capture_frames.size() != playback_frames.size()) {
    return false;
  }
  const auto expected_frames =
      static_cast<std::size_t>(stream_config_.usb_slots_per_submit) *
      static_cast<std::size_t>(stream_config_.transport.iso_frames);
  if (capture_frames.size() != expected_frames) {
    return false;
  }
  return persistent_usb_transport_.complete_next(UsbSlotDirection::Capture) &&
         persistent_usb_transport_.complete_next(UsbSlotDirection::Playback);
}

bool AudioDriverSkeleton::queue_usb_slot(UsbSlotDirection direction) {
  if (!usb_submit_planner_.queue_slot(direction, next_usb_timestamp(direction))) {
    return false;
  }
  return drain_new_usb_submit_descriptors();
}

bool AudioDriverSkeleton::drain_new_usb_submit_descriptors() {
  const auto descriptors = usb_submit_planner_.descriptors();
  if (submitted_usb_descriptor_count_ > descriptors.size()) {
    runtime_counters_.usb_request_drain_failures += 1;
    return false;
  }
  for (std::uint32_t index = submitted_usb_descriptor_count_;
       index < descriptors.size();
       ++index) {
    if (!submit_usb_descriptor(descriptors[index])) {
      return false;
    }
  }
  submitted_usb_descriptor_count_ = static_cast<std::uint32_t>(descriptors.size());
  if (!stream_config_.usb_retain_submit_descriptors &&
      submitted_usb_descriptor_count_ > 0) {
    usb_submit_planner_.clear_descriptors();
    submitted_usb_descriptor_count_ = 0;
  }
  return true;
}

bool AudioDriverSkeleton::submit_usb_descriptor(const UsbSubmitDescriptor& descriptor) {
  if (inflight_usb_request_count_ >= inflight_usb_requests_.size()) {
    runtime_counters_.usb_request_submit_failures += 1;
    return false;
  }
  const auto handle = usb_request_pool_.submit(descriptor);
  if (!handle.valid()) {
    runtime_counters_.usb_request_submit_failures += 1;
    return false;
  }
  const auto tail =
      (inflight_usb_request_head_ + inflight_usb_request_count_) %
      static_cast<std::uint32_t>(inflight_usb_requests_.size());
  inflight_usb_requests_[tail] = handle;
  inflight_usb_request_count_ += 1;
  if (inflight_usb_request_count_ >= stream_config_.usb_request_completion_depth) {
    return complete_oldest_usb_request();
  }
  return true;
}

bool AudioDriverSkeleton::complete_oldest_usb_request() {
  if (inflight_usb_request_count_ == 0) {
    return true;
  }
  const auto handle = inflight_usb_requests_[inflight_usb_request_head_];
  inflight_usb_requests_[inflight_usb_request_head_] = {};
  inflight_usb_request_head_ =
      (inflight_usb_request_head_ + 1U) %
      static_cast<std::uint32_t>(inflight_usb_requests_.size());
  inflight_usb_request_count_ -= 1;
  if (!usb_request_pool_.complete(handle)) {
    runtime_counters_.usb_request_completion_failures += 1;
    return false;
  }
  return true;
}

bool AudioDriverSkeleton::drain_usb_requests() {
  bool ok = true;
  while (inflight_usb_request_count_ > 0) {
    ok = complete_oldest_usb_request() && ok;
  }
  return ok;
}

std::uint64_t AudioDriverSkeleton::cancel_usb_requests() {
  const auto cancelled = usb_request_pool_.cancel_all();
  for (std::uint32_t index = 0; index < inflight_usb_requests_.size(); ++index) {
    inflight_usb_requests_[index] = {};
  }
  inflight_usb_request_head_ = 0;
  inflight_usb_request_count_ = 0;
  return cancelled;
}

std::uint64_t AudioDriverSkeleton::next_usb_timestamp(UsbSlotDirection direction) {
  auto& sequence =
      direction == UsbSlotDirection::Capture ? next_capture_usb_sequence_
                                             : next_playback_usb_sequence_;
  sequence += 1;
  return sequence * stream_config_.transport.iso_frames;
}

}  // namespace opena8djcpp::driverkit
