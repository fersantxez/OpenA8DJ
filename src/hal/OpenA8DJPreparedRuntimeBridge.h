#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenA8DJPreparedRuntimeBridge OpenA8DJPreparedRuntimeBridge;
typedef OpenA8DJPreparedRuntimeBridge *OpenA8DJPreparedRuntimeBridgeRef;

typedef enum OpenA8DJPreparedRuntimeDirection {
  kOpenA8DJPreparedRuntimeDirectionCapture = 0,
  kOpenA8DJPreparedRuntimeDirectionPlayback = 1,
} OpenA8DJPreparedRuntimeDirection;

typedef struct OpenA8DJPreparedRuntimeConfig {
  uint32_t requestSlots;
  uint32_t maxLiveRequests;
  uint32_t slotsPerSubmit;
  uint32_t framesPerSlot;
  uint32_t captureBytesPerSlot;
  uint32_t playbackBytesPerSlot;
} OpenA8DJPreparedRuntimeConfig;

typedef struct OpenA8DJPreparedRuntimeHandle {
  uint32_t slot;
  uint32_t generation;
} OpenA8DJPreparedRuntimeHandle;

typedef struct OpenA8DJPreparedRuntimeSubmit {
  bool accepted;
  OpenA8DJPreparedRuntimeHandle handle;
  uint64_t firstSequence;
  uint64_t slotCount;
  uint64_t firstSampleTimestamp;
  uint64_t frameCount;
  uint64_t byteCount;
} OpenA8DJPreparedRuntimeSubmit;

typedef struct OpenA8DJPreparedRuntimeCounters {
  uint64_t submitCalls;
  uint64_t captureSubmitCalls;
  uint64_t playbackSubmitCalls;
  uint64_t completionCalls;
  uint64_t captureCompletionCalls;
  uint64_t playbackCompletionCalls;
  uint64_t cancelCalls;
  uint64_t submitFailures;
  uint64_t liveLimitFailures;
  uint64_t descriptorMismatches;
  uint64_t fallbackAllocations;
  uint64_t invalidCompletions;
  uint64_t staleCompletions;
  uint64_t lateCompletionsAfterCancel;
  uint64_t liveRequests;
  uint64_t maxLiveRequests;
  uint64_t submittedFrames;
  uint64_t completedFrames;
  uint64_t cancelledFrames;
  uint64_t submittedBytes;
  uint64_t completedBytes;
  uint64_t cancelledBytes;
} OpenA8DJPreparedRuntimeCounters;

OpenA8DJPreparedRuntimeBridgeRef
OpenA8DJPreparedRuntimeBridgeCreate(const OpenA8DJPreparedRuntimeConfig *config);

void OpenA8DJPreparedRuntimeBridgeDestroy(OpenA8DJPreparedRuntimeBridgeRef bridge);

OpenA8DJPreparedRuntimeSubmit OpenA8DJPreparedRuntimeBridgeQueueSubmit(
    OpenA8DJPreparedRuntimeBridgeRef bridge,
    OpenA8DJPreparedRuntimeDirection direction,
    uint64_t firstSequence,
    uint64_t firstSampleTimestamp,
    uint64_t slotCount,
    uint64_t frameCount,
    uint64_t byteCount);

bool OpenA8DJPreparedRuntimeBridgeComplete(OpenA8DJPreparedRuntimeBridgeRef bridge,
                                           OpenA8DJPreparedRuntimeHandle handle);

uint64_t OpenA8DJPreparedRuntimeBridgeCancelAll(OpenA8DJPreparedRuntimeBridgeRef bridge);

OpenA8DJPreparedRuntimeCounters
OpenA8DJPreparedRuntimeBridgeSnapshotCounters(OpenA8DJPreparedRuntimeBridgeRef bridge);

#ifdef __cplusplus
}
#endif
