#pragma once

#include "opena8djcpp/driverkit_model.hpp"
#include "opena8djcpp/prepared_transport.hpp"
#include "opena8djcpp/usb_request_pool.hpp"
#include "opena8djcpp/usb_submit_plan.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace opena8djcpp::driverkit {

enum class AudioDriverState {
  Created,
  Started,
  Stopped,
};

struct AudioStreamConfig {
  std::uint32_t sample_rate = 48000;
  std::uint32_t buffer_frames = 64;
  PreparedTransportConfig transport{};
  std::uint32_t usb_slots_per_submit = 8;
  std::uint32_t usb_bytes_per_slot = kMode2DefaultTransferBytes;
  std::uint32_t usb_initial_capture_slots = 8;
  std::uint32_t usb_initial_playback_slots = 8;
  std::uint32_t usb_request_slots = 8;
  std::uint32_t usb_request_completion_depth = 4;
  bool usb_retain_submit_descriptors = false;
};

struct AudioIOMemoryDescriptorModel {
  StreamDirection direction = StreamDirection::Output;
  std::uint32_t starting_channel = 0;
  std::uint32_t channel_count = 0;
  std::uint32_t frames = 0;
  std::uint32_t bytes_per_sample = sizeof(float);

  [[nodiscard]] constexpr std::uint64_t byte_count() const {
    return static_cast<std::uint64_t>(channel_count) * frames * bytes_per_sample;
  }
};

struct ZeroTimestampModel {
  std::uint64_t sample_time = 0;
  std::uint64_t host_time = 0;
};

struct DriverKitRuntimeCounters {
  std::uint64_t io_memory_layout_builds = 0;
  std::uint64_t io_memory_layout_failures = 0;
  std::uint64_t zero_timestamp_updates = 0;
  std::uint64_t zero_timestamp_regressions = 0;
  std::uint64_t configuration_changes = 0;
  std::uint64_t rejected_configuration_changes = 0;
  std::uint64_t usb_request_submit_failures = 0;
  std::uint64_t usb_request_completion_failures = 0;
  std::uint64_t usb_request_drain_failures = 0;
};

// Compile this target only inside a DriverKit-capable Xcode project. The pure
// C++ core stays independent so offline gates can run without system mutation.
class AudioDriverSkeleton {
 public:
  [[nodiscard]] bool start_driver();
  [[nodiscard]] bool stop_driver();
  [[nodiscard]] bool configure_stream(const AudioStreamConfig& config);
  [[nodiscard]] bool request_configuration_change(const AudioStreamConfig& config);
  [[nodiscard]] bool start_stream();
  [[nodiscard]] bool stop_stream();
  [[nodiscard]] bool stream_started() const;
  [[nodiscard]] std::array<AudioIOMemoryDescriptorModel, 5> io_memory_layout();
  [[nodiscard]] bool update_zero_timestamp(std::uint64_t sample_time,
                                           std::uint64_t host_time);
  [[nodiscard]] bool write_playback(std::span<const S24Frame> frames);
  [[nodiscard]] std::uint32_t read_capture(std::span<S24Frame> frames);
  [[nodiscard]] bool complete_backend_period(std::span<const S24Frame> capture_frames,
                                             std::span<S24Frame> playback_frames,
                                             std::uint64_t sample_timestamp);
  void finish_usb_submit_binding();
  [[nodiscard]] AudioDriverState state() const;
  [[nodiscard]] const DriverKitDeviceModel& device_model() const;
  [[nodiscard]] const AudioStreamConfig& stream_config() const;
  [[nodiscard]] const PreparedTransportCounters& transport_counters() const;
  [[nodiscard]] const DriverKitRuntimeCounters& runtime_counters() const;
  [[nodiscard]] const PreparedUsbSubmitPlannerCounters& usb_submit_counters() const;
  [[nodiscard]] const PreparedUsbRequestPoolCounters& usb_request_counters() const;
  [[nodiscard]] std::span<const UsbSubmitDescriptor> usb_submit_descriptors() const;
  [[nodiscard]] ZeroTimestampModel zero_timestamp() const;
  [[nodiscard]] PreparedTransportSafety transport_safety() const;
  [[nodiscard]] PreparedUsbSubmitPlannerSafety usb_submit_safety() const;
  [[nodiscard]] PreparedUsbRequestPoolSafety usb_request_safety() const;

 private:
  [[nodiscard]] bool validate_stream_config(const AudioStreamConfig& config) const;
  [[nodiscard]] bool validate_io_memory_layout(
      const std::array<AudioIOMemoryDescriptorModel, 5>& layout) const;
  [[nodiscard]] bool start_usb_submit_binding();
  [[nodiscard]] bool queue_initial_usb_slots();
  [[nodiscard]] bool queue_usb_slots_for_period(std::span<const S24Frame> capture_frames,
                                                std::span<const S24Frame> playback_frames);
  [[nodiscard]] bool queue_usb_slot(UsbSlotDirection direction);
  [[nodiscard]] bool drain_new_usb_submit_descriptors();
  [[nodiscard]] bool submit_usb_descriptor(const UsbSubmitDescriptor& descriptor);
  [[nodiscard]] bool complete_oldest_usb_request();
  [[nodiscard]] bool drain_usb_requests();
  [[nodiscard]] std::uint64_t cancel_usb_requests();
  [[nodiscard]] std::uint64_t next_usb_timestamp(UsbSlotDirection direction);

  DriverKitDeviceModel device_model_ = make_driverkit_device_model();
  AudioStreamConfig stream_config_{};
  PreparedTransportBackend transport_{};
  PreparedUsbSubmitPlanner usb_submit_planner_{};
  PreparedUsbRequestPool usb_request_pool_{};
  std::array<PreparedUsbRequestHandle, kPreparedUsbRequestMaxSlots> inflight_usb_requests_{};
  DriverKitRuntimeCounters runtime_counters_{};
  ZeroTimestampModel zero_timestamp_{};
  AudioDriverState state_ = AudioDriverState::Created;
  std::uint64_t next_capture_usb_sequence_ = 0;
  std::uint64_t next_playback_usb_sequence_ = 0;
  std::uint32_t submitted_usb_descriptor_count_ = 0;
  std::uint32_t inflight_usb_request_head_ = 0;
  std::uint32_t inflight_usb_request_count_ = 0;
  bool stream_configured_ = false;
  bool have_zero_timestamp_ = false;
};

}  // namespace opena8djcpp::driverkit
