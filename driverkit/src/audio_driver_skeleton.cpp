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

bool AudioDriverSkeleton::start_stream() {
  if (state_ != AudioDriverState::Started || !stream_configured_ || transport_.started()) {
    return false;
  }
  return transport_.start(stream_config_.transport);
}

bool AudioDriverSkeleton::stop_stream() {
  if (!transport_.started()) {
    return false;
  }
  transport_.stop();
  return true;
}

bool AudioDriverSkeleton::stream_started() const {
  return transport_.started();
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
  return transport_.backend_complete_period(capture_frames, playback_frames, sample_timestamp);
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

PreparedTransportSafety AudioDriverSkeleton::transport_safety() const {
  return transport_.safety();
}

bool AudioDriverSkeleton::validate_stream_config(const AudioStreamConfig& config) const {
  const bool sample_rate_valid =
      config.sample_rate == device_model_.sample_rates[0] ||
      config.sample_rate == device_model_.sample_rates[1];
  return sample_rate_valid && config.buffer_frames > 0 && config.buffer_frames <= 4096 &&
         config.transport.iso_frames > 0 && config.transport.capture_slots > 0 &&
         config.transport.playback_slots > 0 &&
         config.transport.capture_slots <= kPreparedTransportMaxSlots &&
         config.transport.playback_slots <= kPreparedTransportMaxSlots;
}

}  // namespace opena8djcpp::driverkit
