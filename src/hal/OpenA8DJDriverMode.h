#ifndef OPENA8DJ_DRIVER_MODE_H
#define OPENA8DJ_DRIVER_MODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    kOpenA8DJDriverModeSchemaVersion = 1
};

typedef enum OpenA8DJDriverModeID {
    kOpenA8DJDriverModeInvalid = 0,
    kOpenA8DJDriverModeBalanced = 1,
    kOpenA8DJDriverModePerformance = 2
} OpenA8DJDriverModeID;

typedef enum OpenA8DJDriverModeWorkerQoS {
    kOpenA8DJDriverModeWorkerQoSDefault = 0,
    kOpenA8DJDriverModeWorkerQoSUserInteractive = 1
} OpenA8DJDriverModeWorkerQoS;

typedef enum OpenA8DJDriverModeResult {
    kOpenA8DJDriverModeResultUnchanged = 0,
    kOpenA8DJDriverModeResultApplied = 1,
    kOpenA8DJDriverModeResultPending = 2,
    kOpenA8DJDriverModeResultCancelled = 3,
    kOpenA8DJDriverModeResultInvalid = 4,
    kOpenA8DJDriverModeResultApplyFailed = 5
} OpenA8DJDriverModeResult;

typedef enum OpenA8DJDriverModeRejection {
    kOpenA8DJDriverModeRejectionNone = 0,
    kOpenA8DJDriverModeRejectionBadLength = 1,
    kOpenA8DJDriverModeRejectionUnsupportedSchema = 2,
    kOpenA8DJDriverModeRejectionReservedNonzero = 3,
    kOpenA8DJDriverModeRejectionUnknownMode = 4
} OpenA8DJDriverModeRejection;

typedef struct OpenA8DJDriverModePolicy {
    uint32_t outputStartLatencyFrames;
    uint32_t outputRestartLatencyFrames;
    uint32_t outputTargetLatencyFrames;
    uint32_t workerQoS;
} OpenA8DJDriverModePolicy;

typedef struct OpenA8DJDriverModeSetPayload {
    uint16_t schemaVersion;
    uint16_t reserved0;
    uint32_t modeID;
    uint8_t reserved[8];
} __attribute__((packed)) OpenA8DJDriverModeSetPayload;

typedef struct OpenA8DJDriverModeStatePayload {
    uint16_t schemaVersion;
    uint16_t reserved0;
    uint32_t requestedMode;
    uint32_t effectiveMode;
    uint8_t pending;
    uint8_t streaming;
    uint8_t lastResult;
    uint8_t rejectionReason;
    uint64_t generation;
    uint64_t acceptedRequests;
    uint64_t rejectedRequests;
    uint64_t appliedTransitions;
    uint64_t applyFailures;
    uint64_t pendingTransitions;
    uint32_t outputStartLatencyFrames;
    uint32_t outputRestartLatencyFrames;
    uint32_t outputTargetLatencyFrames;
    uint32_t workerQoS;
    uint8_t reserved[8];
} __attribute__((packed)) OpenA8DJDriverModeStatePayload;

typedef struct OpenA8DJDriverModeState {
    uint32_t requestedMode;
    uint32_t effectiveMode;
    bool pending;
    uint8_t lastResult;
    uint8_t rejectionReason;
    uint64_t generation;
    uint64_t acceptedRequests;
    uint64_t rejectedRequests;
    uint64_t appliedTransitions;
    uint64_t applyFailures;
    uint64_t pendingTransitions;
} OpenA8DJDriverModeState;

typedef bool (*OpenA8DJDriverModePreflight)(const OpenA8DJDriverModePolicy *policy,
                                            void *context);

static inline bool OpenA8DJDriverModeLookup(uint32_t modeID,
                                            OpenA8DJDriverModePolicy *outPolicy)
{
    OpenA8DJDriverModePolicy policy;
    switch (modeID) {
        case kOpenA8DJDriverModeBalanced:
            policy = (OpenA8DJDriverModePolicy){
                .outputStartLatencyFrames = 8192,
                .outputRestartLatencyFrames = 4096,
                .outputTargetLatencyFrames = 8192,
                .workerQoS = kOpenA8DJDriverModeWorkerQoSDefault
            };
            break;
        case kOpenA8DJDriverModePerformance:
            policy = (OpenA8DJDriverModePolicy){
                .outputStartLatencyFrames = 4096,
                .outputRestartLatencyFrames = 4096,
                .outputTargetLatencyFrames = 4096,
                .workerQoS = kOpenA8DJDriverModeWorkerQoSUserInteractive
            };
            break;
        default:
            return false;
    }
    if (outPolicy != NULL) {
        *outPolicy = policy;
    }
    return true;
}

static inline void OpenA8DJDriverModeStateInit(OpenA8DJDriverModeState *state)
{
    memset(state, 0, sizeof(*state));
    state->requestedMode = kOpenA8DJDriverModeBalanced;
    state->effectiveMode = kOpenA8DJDriverModeBalanced;
    state->lastResult = kOpenA8DJDriverModeResultUnchanged;
}

static inline bool OpenA8DJDriverModeRunPreflight(
    uint32_t modeID,
    OpenA8DJDriverModePreflight preflight,
    void *context)
{
    OpenA8DJDriverModePolicy policy;
    return OpenA8DJDriverModeLookup(modeID, &policy) &&
           (preflight == NULL || preflight(&policy, context));
}

static inline bool OpenA8DJDriverModeSet(OpenA8DJDriverModeState *state,
                                         uint32_t modeID,
                                         bool streaming,
                                         OpenA8DJDriverModePreflight preflight,
                                         void *context)
{
    if (!OpenA8DJDriverModeLookup(modeID, NULL)) {
        state->rejectedRequests++;
        state->lastResult = kOpenA8DJDriverModeResultInvalid;
        state->rejectionReason = kOpenA8DJDriverModeRejectionUnknownMode;
        return false;
    }

    state->acceptedRequests++;
    state->rejectionReason = kOpenA8DJDriverModeRejectionNone;
    if (streaming) {
        if (modeID == state->effectiveMode) {
            if (state->pending) {
                state->requestedMode = state->effectiveMode;
                state->pending = false;
                state->lastResult = kOpenA8DJDriverModeResultCancelled;
            } else {
                state->lastResult = kOpenA8DJDriverModeResultUnchanged;
            }
            return true;
        }
        if (!state->pending || state->requestedMode != modeID) {
            state->requestedMode = modeID;
            state->pending = true;
            state->pendingTransitions++;
        }
        state->lastResult = kOpenA8DJDriverModeResultPending;
        return true;
    }

    if (modeID == state->effectiveMode) {
        state->requestedMode = state->effectiveMode;
        state->pending = false;
        state->lastResult = kOpenA8DJDriverModeResultUnchanged;
        return true;
    }
    if (!OpenA8DJDriverModeRunPreflight(modeID, preflight, context)) {
        state->applyFailures++;
        state->lastResult = kOpenA8DJDriverModeResultApplyFailed;
        return false;
    }
    state->requestedMode = modeID;
    state->effectiveMode = modeID;
    state->pending = false;
    state->lastResult = kOpenA8DJDriverModeResultApplied;
    state->appliedTransitions++;
    state->generation++;
    return true;
}

static inline bool OpenA8DJDriverModePromotePending(
    OpenA8DJDriverModeState *state,
    OpenA8DJDriverModePreflight preflight,
    void *context)
{
    if (!state->pending) {
        return true;
    }
    if (!OpenA8DJDriverModeRunPreflight(state->requestedMode, preflight, context)) {
        state->applyFailures++;
        state->lastResult = kOpenA8DJDriverModeResultApplyFailed;
        return false;
    }
    state->effectiveMode = state->requestedMode;
    state->pending = false;
    state->lastResult = kOpenA8DJDriverModeResultApplied;
    state->appliedTransitions++;
    state->generation++;
    return true;
}

static inline void OpenA8DJDriverModeReject(OpenA8DJDriverModeState *state,
                                            uint8_t reason)
{
    state->rejectedRequests++;
    state->lastResult = kOpenA8DJDriverModeResultInvalid;
    state->rejectionReason = reason;
}

static inline bool OpenA8DJDriverModeValidateSetPayload(
    const void *bytes,
    size_t length,
    OpenA8DJDriverModeSetPayload *outPayload,
    uint8_t *outRejection)
{
    uint8_t rejection = kOpenA8DJDriverModeRejectionNone;
    OpenA8DJDriverModeSetPayload payload;
    memset(&payload, 0, sizeof(payload));
    if (bytes == NULL || length != sizeof(payload)) {
        rejection = kOpenA8DJDriverModeRejectionBadLength;
    } else {
        memcpy(&payload, bytes, sizeof(payload));
        if (payload.schemaVersion != kOpenA8DJDriverModeSchemaVersion) {
            rejection = kOpenA8DJDriverModeRejectionUnsupportedSchema;
        } else if (payload.reserved0 != 0) {
            rejection = kOpenA8DJDriverModeRejectionReservedNonzero;
        } else {
            for (size_t index = 0; index < sizeof(payload.reserved); index++) {
                if (payload.reserved[index] != 0) {
                    rejection = kOpenA8DJDriverModeRejectionReservedNonzero;
                    break;
                }
            }
            if (rejection == kOpenA8DJDriverModeRejectionNone &&
                !OpenA8DJDriverModeLookup(payload.modeID, NULL)) {
                rejection = kOpenA8DJDriverModeRejectionUnknownMode;
            }
        }
    }
    if (outRejection != NULL) {
        *outRejection = rejection;
    }
    if (rejection != kOpenA8DJDriverModeRejectionNone) {
        return false;
    }
    if (outPayload != NULL) {
        *outPayload = payload;
    }
    return true;
}

static inline void OpenA8DJDriverModeMakeStatePayload(
    const OpenA8DJDriverModeState *state,
    bool streaming,
    OpenA8DJDriverModeStatePayload *outPayload)
{
    OpenA8DJDriverModePolicy policy;
    (void)OpenA8DJDriverModeLookup(state->effectiveMode, &policy);
    memset(outPayload, 0, sizeof(*outPayload));
    outPayload->schemaVersion = kOpenA8DJDriverModeSchemaVersion;
    outPayload->requestedMode = state->requestedMode;
    outPayload->effectiveMode = state->effectiveMode;
    outPayload->pending = state->pending ? 1 : 0;
    outPayload->streaming = streaming ? 1 : 0;
    outPayload->lastResult = state->lastResult;
    outPayload->rejectionReason = state->rejectionReason;
    outPayload->generation = state->generation;
    outPayload->acceptedRequests = state->acceptedRequests;
    outPayload->rejectedRequests = state->rejectedRequests;
    outPayload->appliedTransitions = state->appliedTransitions;
    outPayload->applyFailures = state->applyFailures;
    outPayload->pendingTransitions = state->pendingTransitions;
    outPayload->outputStartLatencyFrames = policy.outputStartLatencyFrames;
    outPayload->outputRestartLatencyFrames = policy.outputRestartLatencyFrames;
    outPayload->outputTargetLatencyFrames = policy.outputTargetLatencyFrames;
    outPayload->workerQoS = policy.workerQoS;
}

#endif
