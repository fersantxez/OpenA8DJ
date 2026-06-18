#include "OpenA8DJPreparedRuntimeBridge.h"

#include "opena8djcpp/prepared_usb_async_runtime.hpp"

struct OpenA8DJPreparedRuntimeBridge {
  opena8djcpp::PreparedUsbAsyncRuntime runtime;
};

namespace {

opena8djcpp::UsbSlotDirection to_direction(OpenA8DJPreparedRuntimeDirection direction) {
  return direction == kOpenA8DJPreparedRuntimeDirectionPlayback
             ? opena8djcpp::UsbSlotDirection::Playback
             : opena8djcpp::UsbSlotDirection::Capture;
}

opena8djcpp::PreparedUsbRequestHandle to_core_handle(OpenA8DJPreparedRuntimeHandle handle) {
  return opena8djcpp::PreparedUsbRequestHandle{
      .slot = handle.slot,
      .generation = handle.generation,
  };
}

OpenA8DJPreparedRuntimeHandle to_c_handle(opena8djcpp::PreparedUsbRequestHandle handle) {
  return OpenA8DJPreparedRuntimeHandle{
      .slot = handle.slot,
      .generation = handle.generation,
  };
}

OpenA8DJPreparedRuntimeCounters to_c_counters(
    const opena8djcpp::PreparedUsbAsyncRuntimeCounters &counters) {
  return OpenA8DJPreparedRuntimeCounters{
      .submitCalls = counters.submit_calls,
      .captureSubmitCalls = counters.capture_submit_calls,
      .playbackSubmitCalls = counters.playback_submit_calls,
      .completionCalls = counters.completion_calls,
      .captureCompletionCalls = counters.capture_completion_calls,
      .playbackCompletionCalls = counters.playback_completion_calls,
      .cancelCalls = counters.cancel_calls,
      .submitFailures = counters.submit_failures,
      .liveLimitFailures = counters.live_limit_failures,
      .descriptorMismatches = counters.descriptor_mismatches,
      .fallbackAllocations = counters.fallback_allocations,
      .invalidCompletions = counters.invalid_completions,
      .staleCompletions = counters.stale_completions,
      .lateCompletionsAfterCancel = counters.late_completions_after_cancel,
      .liveRequests = counters.live_requests,
      .maxLiveRequests = counters.max_live_requests,
      .submittedFrames = counters.submitted_frames,
      .completedFrames = counters.completed_frames,
      .cancelledFrames = counters.cancelled_frames,
      .submittedBytes = counters.submitted_bytes,
      .completedBytes = counters.completed_bytes,
      .cancelledBytes = counters.cancelled_bytes,
  };
}

}  // namespace

OpenA8DJPreparedRuntimeBridgeRef
OpenA8DJPreparedRuntimeBridgeCreate(const OpenA8DJPreparedRuntimeConfig *config) {
  if (config == nullptr) {
    return nullptr;
  }
  auto *bridge = new OpenA8DJPreparedRuntimeBridge();
  const opena8djcpp::PreparedUsbAsyncRuntimeConfig core_config{
      .request_pool = opena8djcpp::PreparedUsbRequestPoolConfig{
          .request_slots = config->requestSlots,
      },
      .slots_per_submit = config->slotsPerSubmit,
      .frames_per_slot = config->framesPerSlot,
      .bytes_per_slot = config->bytesPerSlot,
      .max_live_requests = config->maxLiveRequests,
  };
  if (!bridge->runtime.start(core_config)) {
    delete bridge;
    return nullptr;
  }
  return bridge;
}

void OpenA8DJPreparedRuntimeBridgeDestroy(OpenA8DJPreparedRuntimeBridgeRef bridge) {
  if (bridge == nullptr) {
    return;
  }
  bridge->runtime.stop();
  delete bridge;
}

OpenA8DJPreparedRuntimeSubmit OpenA8DJPreparedRuntimeBridgeQueueSubmit(
    OpenA8DJPreparedRuntimeBridgeRef bridge,
    OpenA8DJPreparedRuntimeDirection direction,
    uint64_t firstSequence,
    uint64_t firstSampleTimestamp,
    uint64_t slotCount,
    uint64_t frameCount,
    uint64_t byteCount) {
  OpenA8DJPreparedRuntimeSubmit out{};
  if (bridge == nullptr) {
    return out;
  }
  const auto submit = bridge->runtime.submit(to_direction(direction),
                                             firstSequence,
                                             firstSampleTimestamp,
                                             slotCount,
                                             frameCount,
                                             byteCount);
  out.accepted = submit.handle.valid();
  out.handle = to_c_handle(submit.handle);
  out.firstSequence = submit.descriptor.first_sequence;
  out.slotCount = submit.descriptor.slot_count;
  out.firstSampleTimestamp = submit.descriptor.first_sample_timestamp;
  out.frameCount = submit.descriptor.frame_count;
  out.byteCount = submit.descriptor.byte_count;
  return out;
}

bool OpenA8DJPreparedRuntimeBridgeComplete(OpenA8DJPreparedRuntimeBridgeRef bridge,
                                           OpenA8DJPreparedRuntimeHandle handle) {
  if (bridge == nullptr) {
    return false;
  }
  return bridge->runtime.complete(to_core_handle(handle));
}

uint64_t OpenA8DJPreparedRuntimeBridgeCancelAll(OpenA8DJPreparedRuntimeBridgeRef bridge) {
  if (bridge == nullptr) {
    return 0;
  }
  return bridge->runtime.cancel_all();
}

OpenA8DJPreparedRuntimeCounters
OpenA8DJPreparedRuntimeBridgeSnapshotCounters(OpenA8DJPreparedRuntimeBridgeRef bridge) {
  if (bridge == nullptr) {
    return OpenA8DJPreparedRuntimeCounters{};
  }
  return to_c_counters(bridge->runtime.counters());
}
