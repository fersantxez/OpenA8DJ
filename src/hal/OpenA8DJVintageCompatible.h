#ifndef OPENA8DJ_VINTAGE_COMPATIBLE_H
#define OPENA8DJ_VINTAGE_COMPATIBLE_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "OpenA8DJDriverMode.h"

enum {
    kOpenA8DJVintageSchemaVersion = 1,
    kOpenA8DJVintageRequiredRateMask = 0x0f,
    kOpenA8DJVintageRequiredBufferFrames = 512,
    kOpenA8DJVintageRequiredTimestampPeriodFrames = 16384
};

typedef enum OpenA8DJVintageRate {
    kOpenA8DJVintageRate44100 = 1u << 0,
    kOpenA8DJVintageRate48000 = 1u << 1,
    kOpenA8DJVintageRate88200 = 1u << 2,
    kOpenA8DJVintageRate96000 = 1u << 3,
    kOpenA8DJVintageRate192000 = 1u << 4
} OpenA8DJVintageRate;

typedef enum OpenA8DJVintageConformanceStatus {
    kOpenA8DJVintageConformanceUnverified = 0,
    kOpenA8DJVintageConformancePartial = 1,
    kOpenA8DJVintageConformanceCompatible = 2
} OpenA8DJVintageConformanceStatus;

typedef enum OpenA8DJVintageBufferNormalization {
    kOpenA8DJVintageBufferNormalizationShippingTable = 0,
    kOpenA8DJVintageBufferNormalizationFixed = 1
} OpenA8DJVintageBufferNormalization;

typedef enum OpenA8DJVintageClientSampleFormat {
    kOpenA8DJVintageClientSampleFormatUnknown = 0,
    kOpenA8DJVintageClientSampleFormatFloat32 = 1,
    kOpenA8DJVintageClientSampleFormatInt24 = 2
} OpenA8DJVintageClientSampleFormat;

typedef enum OpenA8DJVintageReason {
    kOpenA8DJVintageReasonNotRequested = 1ull << 0,
    kOpenA8DJVintageReasonPreflightNotRun = 1ull << 1,
    kOpenA8DJVintageReasonAudioParamsResetDisabled = 1ull << 2,
    kOpenA8DJVintageReasonCapturePacedOutputDisabled = 1ull << 3,
    kOpenA8DJVintageReasonOutputStartByteNot4 = 1ull << 4,
    kOpenA8DJVintageReasonExplicitUSBSchedulingEnabled = 1ull << 5,
    kOpenA8DJVintageReasonUSBHALTimestampEnabled = 1ull << 6,
    kOpenA8DJVintageReasonTimestampPeriodMismatch = 1ull << 7,
    kOpenA8DJVintageReasonBufferNot512 = 1ull << 8,
    kOpenA8DJVintageReasonRateSurfaceMismatch = 1ull << 9,
    kOpenA8DJVintageReasonRate192000NotImplemented = 1ull << 10,
    kOpenA8DJVintageReasonLegacyStreamPartitionMismatch = 1ull << 11,
    kOpenA8DJVintageReasonLegacyClientFormatMismatch = 1ull << 12,
    kOpenA8DJVintageReasonLegacyQueueGeometryMismatch = 1ull << 13,
    kOpenA8DJVintageReasonTimecodeModeConflict = 1ull << 14,
    kOpenA8DJVintageReasonApplyFailed = 1ull << 15,
    kOpenA8DJVintageReasonPhysicalMatrixMissing = 1ull << 16,
    kOpenA8DJVintageReasonTimestampRateMismatch = 1ull << 17
} OpenA8DJVintageReason;

#define OPENA8DJ_VINTAGE_KNOWN_REASON_MASK ((uint64_t)((1ull << 18) - 1ull))
#define OPENA8DJ_VINTAGE_MANDATORY_REASON_MASK \
    ((uint64_t)(kOpenA8DJVintageReasonAudioParamsResetDisabled | \
                kOpenA8DJVintageReasonCapturePacedOutputDisabled | \
                kOpenA8DJVintageReasonOutputStartByteNot4 | \
                kOpenA8DJVintageReasonExplicitUSBSchedulingEnabled | \
                kOpenA8DJVintageReasonUSBHALTimestampEnabled | \
                kOpenA8DJVintageReasonTimestampPeriodMismatch | \
                kOpenA8DJVintageReasonBufferNot512 | \
                kOpenA8DJVintageReasonRateSurfaceMismatch | \
                kOpenA8DJVintageReasonTimestampRateMismatch))

typedef enum OpenA8DJVintageCapability {
    kOpenA8DJVintageCapabilityAudioParamsReset = 1ull << 0,
    kOpenA8DJVintageCapabilityCapturePacedOutput = 1ull << 1,
    kOpenA8DJVintageCapabilityOutputStartByte4 = 1ull << 2,
    kOpenA8DJVintageCapabilityHostMonotonicTimestamp = 1ull << 3,
    kOpenA8DJVintageCapabilityFixedBuffer512 = 1ull << 4,
    kOpenA8DJVintageCapabilityRate44100 = 1ull << 5,
    kOpenA8DJVintageCapabilityRate48000 = 1ull << 6,
    kOpenA8DJVintageCapabilityRate88200 = 1ull << 7,
    kOpenA8DJVintageCapabilityRate96000 = 1ull << 8,
    kOpenA8DJVintageCapabilityRate192000 = 1ull << 9,
    kOpenA8DJVintageCapabilityLegacyStereoStreamPartition = 1ull << 10,
    kOpenA8DJVintageCapabilityLegacyClientInt24 = 1ull << 11,
    kOpenA8DJVintageCapabilityLegacyCaptureDepth64 = 1ull << 12,
    kOpenA8DJVintageCapabilityLegacyOutputSlots128 = 1ull << 13
} OpenA8DJVintageCapability;

#define OPENA8DJ_VINTAGE_KNOWN_CAPABILITY_MASK ((uint64_t)((1ull << 14) - 1ull))

typedef struct OpenA8DJVintageBuildDescriptor {
    uint8_t resetAudioParamsBeforeStream;
    uint8_t capturePacedOutput;
    uint8_t explicitUSBScheduling;
    uint8_t usbHALTimestampEnabled;
    uint8_t outputStartByte;
    uint8_t inputChannels;
    uint8_t outputChannels;
    uint8_t inputStreams;
    uint8_t outputStreams;
    uint8_t clientSampleFormat;
    uint8_t captureQueueDepth;
    uint8_t playbackQueueDepth;
    uint16_t captureTransactions;
    uint16_t playbackTransactions;
    uint32_t timestampPeriodFrames;
    uint32_t supportedRateMask;
    uint32_t effectiveRateMask;
    uint32_t currentBufferFrames;
    double effectiveSampleRate;
    double timestampSampleRate;
} OpenA8DJVintageBuildDescriptor;

typedef struct OpenA8DJVintagePreflightResult {
    uint64_t reasons;
    uint64_t capabilities;
    uint64_t knownCapabilities;
    uint8_t mandatoryPassed;
    uint8_t reserved[7];
} OpenA8DJVintagePreflightResult;

typedef struct OpenA8DJVintageStatePayload {
    uint16_t schemaVersion;
    uint8_t status;
    uint8_t experimental;
    uint64_t reasons;
    uint64_t capabilities;
    uint64_t knownCapabilities;
    uint64_t preflightGeneration;
    uint64_t failureCounter;
    OpenA8DJVintageBuildDescriptor descriptor;
    uint8_t bufferNormalization;
    uint8_t reserved0[3];
    uint32_t normalizedBufferFrames;
    OpenA8DJDriverModeStatePayload driverMode;
    uint8_t reserved[8];
} __attribute__((packed)) OpenA8DJVintageStatePayload;

static inline uint32_t OpenA8DJVintageRateMaskForRate(double rate)
{
    if (!isfinite(rate)) {
        return 0;
    }
    if (rate == 44100.0) return kOpenA8DJVintageRate44100;
    if (rate == 48000.0) return kOpenA8DJVintageRate48000;
    if (rate == 88200.0) return kOpenA8DJVintageRate88200;
    if (rate == 96000.0) return kOpenA8DJVintageRate96000;
    if (rate == 192000.0) return kOpenA8DJVintageRate192000;
    return 0;
}

static inline uint32_t OpenA8DJVintageNormalizeBufferFrames(
    uint32_t requestedFrames,
    bool effective)
{
    if (effective) {
        return requestedFrames >= 1 && requestedFrames <= 4096 ?
            kOpenA8DJVintageRequiredBufferFrames : 0;
    }
    if (requestedFrames == 0 || requestedFrames > 4096) return 0;
    if (requestedFrames <= 512) return 512;
    if (requestedFrames <= 1024) return 1024;
    if (requestedFrames <= 2048) return 2048;
    return 4096;
}

static inline OpenA8DJVintagePreflightResult OpenA8DJVintageEvaluatePreflight(
    const OpenA8DJVintageBuildDescriptor *descriptor)
{
    OpenA8DJVintagePreflightResult result;
    memset(&result, 0, sizeof(result));
    result.knownCapabilities = OPENA8DJ_VINTAGE_KNOWN_CAPABILITY_MASK;
    result.reasons =
        kOpenA8DJVintageReasonRate192000NotImplemented |
        kOpenA8DJVintageReasonLegacyStreamPartitionMismatch |
        kOpenA8DJVintageReasonLegacyClientFormatMismatch |
        kOpenA8DJVintageReasonLegacyQueueGeometryMismatch |
        kOpenA8DJVintageReasonPhysicalMatrixMissing;
    if (descriptor == NULL) {
        result.reasons |= OPENA8DJ_VINTAGE_MANDATORY_REASON_MASK;
        return result;
    }
    if (descriptor->resetAudioParamsBeforeStream == 1) {
        result.capabilities |= kOpenA8DJVintageCapabilityAudioParamsReset;
    } else {
        result.reasons |= kOpenA8DJVintageReasonAudioParamsResetDisabled;
    }
    if (descriptor->capturePacedOutput == 1) {
        result.capabilities |= kOpenA8DJVintageCapabilityCapturePacedOutput;
    } else {
        result.reasons |= kOpenA8DJVintageReasonCapturePacedOutputDisabled;
    }
    if (descriptor->outputStartByte == 4) {
        result.capabilities |= kOpenA8DJVintageCapabilityOutputStartByte4;
    } else {
        result.reasons |= kOpenA8DJVintageReasonOutputStartByteNot4;
    }
    if (descriptor->explicitUSBScheduling != 0) {
        result.reasons |= kOpenA8DJVintageReasonExplicitUSBSchedulingEnabled;
    }
    if (descriptor->usbHALTimestampEnabled != 0) {
        result.reasons |= kOpenA8DJVintageReasonUSBHALTimestampEnabled;
    }
    if (descriptor->timestampPeriodFrames ==
        kOpenA8DJVintageRequiredTimestampPeriodFrames) {
        if (descriptor->usbHALTimestampEnabled == 0) {
            result.capabilities |=
                kOpenA8DJVintageCapabilityHostMonotonicTimestamp;
        }
    } else {
        result.reasons |= kOpenA8DJVintageReasonTimestampPeriodMismatch;
    }
    if (descriptor->currentBufferFrames ==
        kOpenA8DJVintageRequiredBufferFrames) {
        result.capabilities |= kOpenA8DJVintageCapabilityFixedBuffer512;
    } else {
        result.reasons |= kOpenA8DJVintageReasonBufferNot512;
    }
    if (descriptor->supportedRateMask !=
            kOpenA8DJVintageRequiredRateMask ||
        descriptor->effectiveRateMask == 0 ||
        (descriptor->effectiveRateMask &
             ~kOpenA8DJVintageRequiredRateMask) != 0 ||
        (descriptor->effectiveRateMask &
             (descriptor->effectiveRateMask - 1)) != 0) {
        result.reasons |= kOpenA8DJVintageReasonRateSurfaceMismatch;
    }
    if (isfinite(descriptor->effectiveSampleRate) &&
        descriptor->effectiveRateMask ==
            OpenA8DJVintageRateMaskForRate(
                descriptor->effectiveSampleRate) &&
        descriptor->effectiveSampleRate ==
            descriptor->timestampSampleRate) {
        result.capabilities |=
            descriptor->effectiveRateMask ==
                kOpenA8DJVintageRate44100 ?
                    kOpenA8DJVintageCapabilityRate44100 : 0;
        result.capabilities |=
            descriptor->effectiveRateMask ==
                kOpenA8DJVintageRate48000 ?
                    kOpenA8DJVintageCapabilityRate48000 : 0;
        result.capabilities |=
            descriptor->effectiveRateMask ==
                kOpenA8DJVintageRate88200 ?
                    kOpenA8DJVintageCapabilityRate88200 : 0;
        result.capabilities |=
            descriptor->effectiveRateMask ==
                kOpenA8DJVintageRate96000 ?
                    kOpenA8DJVintageCapabilityRate96000 : 0;
    } else {
        result.reasons |= kOpenA8DJVintageReasonTimestampRateMismatch;
    }
    if ((descriptor->supportedRateMask & kOpenA8DJVintageRate44100) != 0) {
        result.capabilities |= kOpenA8DJVintageCapabilityRate44100;
    }
    if ((descriptor->supportedRateMask & kOpenA8DJVintageRate48000) != 0) {
        result.capabilities |= kOpenA8DJVintageCapabilityRate48000;
    }
    if ((descriptor->supportedRateMask & kOpenA8DJVintageRate88200) != 0) {
        result.capabilities |= kOpenA8DJVintageCapabilityRate88200;
    }
    if ((descriptor->supportedRateMask & kOpenA8DJVintageRate96000) != 0) {
        result.capabilities |= kOpenA8DJVintageCapabilityRate96000;
    }
    if ((result.reasons & OPENA8DJ_VINTAGE_MANDATORY_REASON_MASK) == 0) {
        result.mandatoryPassed = 1;
    }
    return result;
}

static inline uint8_t OpenA8DJVintageEvaluateConformance(
    bool requested,
    bool preflightRan,
    uint64_t reasons,
    bool completeSyntheticEvidence)
{
    if (!requested || !preflightRan ||
        (reasons & OPENA8DJ_VINTAGE_MANDATORY_REASON_MASK) != 0) {
        return kOpenA8DJVintageConformanceUnverified;
    }
    if (completeSyntheticEvidence && reasons == 0) {
        return kOpenA8DJVintageConformanceCompatible;
    }
    return kOpenA8DJVintageConformancePartial;
}

static inline bool OpenA8DJVintageModePresent(
    const OpenA8DJDriverModeStatePayload *driverMode)
{
    return driverMode != NULL &&
        (driverMode->requestedMode == kOpenA8DJDriverModeVintageCompatible ||
         driverMode->effectiveMode == kOpenA8DJDriverModeVintageCompatible);
}

static inline bool OpenA8DJVintageValidateStatePayload(
    const OpenA8DJVintageStatePayload *payload)
{
    if (payload == NULL ||
        payload->schemaVersion != kOpenA8DJVintageSchemaVersion ||
        payload->status > kOpenA8DJVintageConformanceCompatible ||
        payload->experimental != 1 ||
        payload->knownCapabilities !=
            OPENA8DJ_VINTAGE_KNOWN_CAPABILITY_MASK ||
        (payload->capabilities & ~payload->knownCapabilities) != 0 ||
        (payload->reasons & ~OPENA8DJ_VINTAGE_KNOWN_REASON_MASK) != 0 ||
        payload->bufferNormalization >
            kOpenA8DJVintageBufferNormalizationFixed) {
        return false;
    }
    for (size_t index = 0; index < sizeof(payload->reserved0); index++) {
        if (payload->reserved0[index] != 0) return false;
    }
    for (size_t index = 0; index < sizeof(payload->reserved); index++) {
        if (payload->reserved[index] != 0) return false;
    }
    bool modePresent = OpenA8DJVintageModePresent(&payload->driverMode);
    if (!modePresent) {
        return payload->status == kOpenA8DJVintageConformanceUnverified &&
               (payload->reasons &
                    kOpenA8DJVintageReasonNotRequested) != 0;
    }
    if ((payload->reasons & kOpenA8DJVintageReasonNotRequested) != 0 ||
        payload->preflightGeneration == 0 ||
        payload->bufferNormalization !=
            (payload->driverMode.effectiveMode ==
                     kOpenA8DJDriverModeVintageCompatible ?
                 kOpenA8DJVintageBufferNormalizationFixed :
                 kOpenA8DJVintageBufferNormalizationShippingTable) ||
        payload->normalizedBufferFrames !=
            (payload->driverMode.effectiveMode ==
                     kOpenA8DJDriverModeVintageCompatible ?
                 kOpenA8DJVintageRequiredBufferFrames : 0)) {
        return false;
    }
    uint8_t expected = OpenA8DJVintageEvaluateConformance(
        true, true, payload->reasons, true);
    return payload->status == expected;
}

#endif
