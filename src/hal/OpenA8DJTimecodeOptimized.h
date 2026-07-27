#ifndef OPENA8DJ_TIMECODE_OPTIMIZED_H
#define OPENA8DJ_TIMECODE_OPTIMIZED_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "OpenA8DJDriverMode.h"

enum {
    kOpenA8DJTimecodeSchemaVersion = 1,
    kOpenA8DJTimecodeAllowedPairMask = 0x3,
    kOpenA8DJTimecodeRequiredEligibleWindows = 8,
    kOpenA8DJTimecodeDropoutWindows = 4,
    kOpenA8DJTimecodeWindowMilliseconds = 250,
    kOpenA8DJTimecodeInputLeadCeilingFrames = 2048
};

#define OPENA8DJ_TIMECODE_ENTRY_RMS 0.01
#define OPENA8DJ_TIMECODE_ENTRY_PEAK 0.031623
#define OPENA8DJ_TIMECODE_HOLD_RMS 0.005
#define OPENA8DJ_TIMECODE_HOLD_PEAK 0.015849
#define OPENA8DJ_TIMECODE_FORBIDDEN_RMS 0.001
#define OPENA8DJ_TIMECODE_FORBIDDEN_PEAK 0.003981

typedef enum OpenA8DJTimecodeArmState {
    kOpenA8DJTimecodeDisarmed = 0,
    kOpenA8DJTimecodeWaitingProfile = 1,
    kOpenA8DJTimecodeQualifying = 2,
    kOpenA8DJTimecodeQualifiedPendingBoundary = 3,
    kOpenA8DJTimecodeActive = 4,
    kOpenA8DJTimecodeDeoptPendingBoundary = 5,
    kOpenA8DJTimecodeFaulted = 6
} OpenA8DJTimecodeArmState;

typedef enum OpenA8DJTimecodeFailReason {
    kOpenA8DJTimecodeFailNone = 0,
    kOpenA8DJTimecodeFailOutsideAllowlist = 1,
    kOpenA8DJTimecodeFailAllowedPairDropout = 2,
    kOpenA8DJTimecodeFailStatsMissing = 3,
    kOpenA8DJTimecodeFailStatsInvalid = 4,
    kOpenA8DJTimecodeFailWrongProfile = 5,
    kOpenA8DJTimecodeFailConfigurationChanged = 6,
    kOpenA8DJTimecodeFailXRunOrTransportError = 7,
    kOpenA8DJTimecodeFailInputLeadViolation = 8,
    kOpenA8DJTimecodeFailApplyFailed = 9,
    kOpenA8DJTimecodeFailExplicitDisarm = 10
} OpenA8DJTimecodeFailReason;

typedef enum OpenA8DJTimecodeProfile {
    kOpenA8DJTimecodeProfileUnavailable = 0,
    kOpenA8DJTimecodeProfileVinyl = 1,
    kOpenA8DJTimecodeProfileCDLine = 2,
    kOpenA8DJTimecodeProfilePhono = 3
} OpenA8DJTimecodeProfile;

typedef enum OpenA8DJTimecodeRejection {
    kOpenA8DJTimecodeRejectionNone = 0,
    kOpenA8DJTimecodeRejectionBadLength = 1,
    kOpenA8DJTimecodeRejectionUnsupportedSchema = 2,
    kOpenA8DJTimecodeRejectionReservedNonzero = 3,
    kOpenA8DJTimecodeRejectionUnknownMode = 4,
    kOpenA8DJTimecodeRejectionInvalidPairMask = 5
} OpenA8DJTimecodeRejection;

typedef struct OpenA8DJTimecodeArmPayload {
    uint16_t schemaVersion;
    uint16_t reserved0;
    uint32_t modeID;
    uint8_t allowedInputPairMask;
    uint8_t reserved[7];
} __attribute__((packed)) OpenA8DJTimecodeArmPayload;

typedef struct OpenA8DJTimecodeWindow {
    uint32_t frames[4];
    double sum[4][2];
    double square[4][2];
    double peak[4][2];
    double minimum[4][2];
    double maximum[4][2];
    uint8_t finite;
    uint8_t complete;
    uint8_t reserved[6];
} OpenA8DJTimecodeWindow;

typedef struct OpenA8DJTimecodeClassifier {
    OpenA8DJTimecodeWindow accumulating;
    OpenA8DJTimecodeWindow latest;
    uint32_t windowFrames;
    uint32_t accumulatedFrames;
    uint64_t completedWindows;
} OpenA8DJTimecodeClassifier;

typedef struct OpenA8DJTimecodeCounters {
    uint64_t arms;
    uint64_t disarms;
    uint64_t qualifications;
    uint64_t activations;
    uint64_t deoptimizations;
    uint64_t outsideAllowlistTrips;
    uint64_t missingEvidenceTrips;
    uint64_t profileTrips;
    uint64_t configurationTrips;
    uint64_t xrunErrorTrips;
} OpenA8DJTimecodeCounters;

typedef struct OpenA8DJTimecodeState {
    uint8_t armed;
    uint8_t armState;
    uint8_t allowedInputPairMask;
    uint8_t profileVerified;
    uint8_t qualified;
    uint8_t optimizedActive;
    uint8_t fallbackMode;
    uint8_t electricalProfile;
    uint8_t eligibleWindows;
    uint8_t dropoutWindows;
    uint8_t lastFailOpenReason;
    uint8_t reserved0;
    uint32_t windowFrames;
    uint32_t inputLeadFrames;
    uint64_t generation;
    double sampleRateSnapshot;
    uint32_t bufferFramesSnapshot;
    OpenA8DJTimecodeCounters counters;
} OpenA8DJTimecodeState;

typedef struct OpenA8DJTimecodeStatePayload {
    uint16_t schemaVersion;
    uint16_t reserved0;
    OpenA8DJDriverModeStatePayload driverMode;
    OpenA8DJTimecodeState timecode;
    OpenA8DJTimecodeWindow latestWindow;
    uint8_t evidenceKind;
    uint8_t intentObserved;
    uint8_t rejectionReason;
    uint8_t reserved[5];
} __attribute__((packed)) OpenA8DJTimecodeStatePayload;

static inline uint8_t OpenA8DJTimecodeProfileForElectricalState(
    uint8_t inputMode,
    uint8_t gndVinyl,
    uint8_t gndCDLine,
    uint8_t gndPhono,
    uint8_t softwareLock,
    uint8_t inputDecodeEnabled,
    uint8_t swapMask,
    uint8_t invertLeftMask,
    uint8_t invertRightMask,
    const uint8_t sources[4])
{
    if (!softwareLock || !inputDecodeEnabled ||
        swapMask != 0 || invertLeftMask != 0 ||
        invertRightMask != 0 || sources == NULL) {
        return kOpenA8DJTimecodeProfileUnavailable;
    }
    for (uint32_t pair = 0; pair < 4; pair++) {
        if (sources[pair] != pair) {
            return kOpenA8DJTimecodeProfileUnavailable;
        }
    }
    if (inputMode == 0 && gndVinyl &&
        !gndCDLine && !gndPhono) {
        return kOpenA8DJTimecodeProfileVinyl;
    }
    if (inputMode == 1 && !gndVinyl &&
        gndCDLine && !gndPhono) {
        return kOpenA8DJTimecodeProfileCDLine;
    }
    if (inputMode == 2 && !gndVinyl &&
        !gndCDLine && gndPhono) {
        return kOpenA8DJTimecodeProfilePhono;
    }
    return kOpenA8DJTimecodeProfileUnavailable;
}

static inline uint32_t OpenA8DJTimecodeFramesPerWindow(double sampleRate)
{
    if (!isfinite(sampleRate) || sampleRate < 1.0 ||
        sampleRate > (double)UINT32_MAX / 4.0) {
        return 0;
    }
    return (uint32_t)llround(sampleRate / 4.0);
}

static inline void OpenA8DJTimecodeClassifierInit(
    OpenA8DJTimecodeClassifier *classifier,
    double sampleRate)
{
    memset(classifier, 0, sizeof(*classifier));
    classifier->windowFrames = OpenA8DJTimecodeFramesPerWindow(sampleRate);
    classifier->accumulating.finite = 1;
    for (uint32_t pair = 0; pair < 4; pair++) {
        for (uint32_t channel = 0; channel < 2; channel++) {
            classifier->accumulating.minimum[pair][channel] = INFINITY;
            classifier->accumulating.maximum[pair][channel] = -INFINITY;
        }
    }
}

static inline bool OpenA8DJTimecodeClassifierFeedFrame(
    OpenA8DJTimecodeClassifier *classifier,
    const float samples[8],
    OpenA8DJTimecodeWindow *outWindow)
{
    if (classifier == NULL || samples == NULL || classifier->windowFrames == 0) {
        return false;
    }
    for (uint32_t pair = 0; pair < 4; pair++) {
        classifier->accumulating.frames[pair]++;
        for (uint32_t channel = 0; channel < 2; channel++) {
            double value = samples[pair * 2 + channel];
            if (!isfinite(value)) {
                classifier->accumulating.finite = 0;
                continue;
            }
            double magnitude = fabs(value);
            classifier->accumulating.sum[pair][channel] += value;
            classifier->accumulating.square[pair][channel] += value * value;
            if (magnitude > classifier->accumulating.peak[pair][channel]) {
                classifier->accumulating.peak[pair][channel] = magnitude;
            }
            if (value < classifier->accumulating.minimum[pair][channel]) {
                classifier->accumulating.minimum[pair][channel] = value;
            }
            if (value > classifier->accumulating.maximum[pair][channel]) {
                classifier->accumulating.maximum[pair][channel] = value;
            }
        }
    }
    classifier->accumulatedFrames++;
    if (classifier->accumulatedFrames < classifier->windowFrames) {
        return false;
    }
    classifier->accumulating.complete = 1;
    for (uint32_t pair = 0; pair < 4; pair++) {
        if (classifier->accumulating.frames[pair] != classifier->windowFrames) {
            classifier->accumulating.complete = 0;
        }
    }
    classifier->latest = classifier->accumulating;
    classifier->completedWindows++;
    if (outWindow != NULL) {
        *outWindow = classifier->latest;
    }
    memset(&classifier->accumulating, 0, sizeof(classifier->accumulating));
    classifier->accumulating.finite = 1;
    for (uint32_t pair = 0; pair < 4; pair++) {
        for (uint32_t channel = 0; channel < 2; channel++) {
            classifier->accumulating.minimum[pair][channel] = INFINITY;
            classifier->accumulating.maximum[pair][channel] = -INFINITY;
        }
    }
    classifier->accumulatedFrames = 0;
    return true;
}

static inline bool OpenA8DJTimecodeWindowValid(
    const OpenA8DJTimecodeWindow *window,
    uint32_t expectedFrames)
{
    if (window == NULL || !window->finite || !window->complete ||
        expectedFrames == 0) {
        return false;
    }
    for (uint32_t pair = 0; pair < 4; pair++) {
        if (window->frames[pair] != expectedFrames) {
            return false;
        }
        for (uint32_t channel = 0; channel < 2; channel++) {
            if (!isfinite(window->minimum[pair][channel]) ||
                !isfinite(window->maximum[pair][channel]) ||
                window->minimum[pair][channel] >
                    window->maximum[pair][channel]) {
                return false;
            }
        }
    }
    return true;
}

static inline double OpenA8DJTimecodeWindowRMS(
    const OpenA8DJTimecodeWindow *window,
    uint32_t pair,
    uint32_t channel)
{
    if (window == NULL || pair >= 4 || channel >= 2 ||
        window->frames[pair] == 0 || window->square[pair][channel] < 0.0) {
        return NAN;
    }
    double frames = window->frames[pair];
    double mean = window->sum[pair][channel] / frames;
    double variance = window->square[pair][channel] / frames -
                      mean * mean;
    if (variance < 0.0 && variance > -1e-15) variance = 0.0;
    return variance >= 0.0 ? sqrt(variance) : NAN;
}

static inline double OpenA8DJTimecodeWindowACPeak(
    const OpenA8DJTimecodeWindow *window,
    uint32_t pair,
    uint32_t channel)
{
    if (window == NULL || pair >= 4 || channel >= 2 ||
        window->frames[pair] == 0 ||
        !isfinite(window->minimum[pair][channel]) ||
        !isfinite(window->maximum[pair][channel])) {
        return NAN;
    }
    double mean = window->sum[pair][channel] /
                  window->frames[pair];
    double below = fabs(window->minimum[pair][channel] - mean);
    double above = fabs(window->maximum[pair][channel] - mean);
    return below > above ? below : above;
}

static inline bool OpenA8DJTimecodeChannelEntry(
    const OpenA8DJTimecodeWindow *window,
    uint32_t pair,
    uint32_t channel)
{
    return OpenA8DJTimecodeWindowRMS(window, pair, channel) >=
               OPENA8DJ_TIMECODE_ENTRY_RMS &&
           OpenA8DJTimecodeWindowACPeak(window, pair, channel) >=
               OPENA8DJ_TIMECODE_ENTRY_PEAK;
}

static inline bool OpenA8DJTimecodeChannelHold(
    const OpenA8DJTimecodeWindow *window,
    uint32_t pair,
    uint32_t channel)
{
    return OpenA8DJTimecodeWindowRMS(window, pair, channel) >=
               OPENA8DJ_TIMECODE_HOLD_RMS ||
           OpenA8DJTimecodeWindowACPeak(window, pair, channel) >=
               OPENA8DJ_TIMECODE_HOLD_PEAK;
}

static inline bool OpenA8DJTimecodePairEntry(
    const OpenA8DJTimecodeWindow *window,
    uint32_t pair)
{
    return OpenA8DJTimecodeChannelEntry(window, pair, 0) &&
           OpenA8DJTimecodeChannelEntry(window, pair, 1);
}

static inline bool OpenA8DJTimecodePairHold(
    const OpenA8DJTimecodeWindow *window,
    uint32_t pair)
{
    return OpenA8DJTimecodeChannelHold(window, pair, 0) &&
           OpenA8DJTimecodeChannelHold(window, pair, 1);
}

static inline bool OpenA8DJTimecodeForbiddenTrip(
    const OpenA8DJTimecodeWindow *window)
{
    for (uint32_t pair = 2; pair < 4; pair++) {
        for (uint32_t channel = 0; channel < 2; channel++) {
            if (OpenA8DJTimecodeWindowRMS(window, pair, channel) >=
                    OPENA8DJ_TIMECODE_FORBIDDEN_RMS ||
                window->peak[pair][channel] >=
                    OPENA8DJ_TIMECODE_FORBIDDEN_PEAK) {
                return true;
            }
        }
    }
    return false;
}

static inline void OpenA8DJTimecodeStateInit(OpenA8DJTimecodeState *state)
{
    memset(state, 0, sizeof(*state));
    state->armState = kOpenA8DJTimecodeDisarmed;
    state->fallbackMode = kOpenA8DJDriverModeBalanced;
}

static inline bool OpenA8DJTimecodeValidateArmPayloadDetailed(
    const void *bytes,
    size_t length,
    OpenA8DJTimecodeArmPayload *outPayload,
    uint8_t *outRejection)
{
    OpenA8DJTimecodeArmPayload payload;
    uint8_t rejection = kOpenA8DJTimecodeRejectionNone;
    memset(&payload, 0, sizeof(payload));
    if (bytes == NULL || length != sizeof(payload)) {
        rejection = kOpenA8DJTimecodeRejectionBadLength;
    } else {
        memcpy(&payload, bytes, sizeof(payload));
        if (payload.schemaVersion != kOpenA8DJTimecodeSchemaVersion) {
            rejection =
                kOpenA8DJTimecodeRejectionUnsupportedSchema;
        } else if (payload.reserved0 != 0) {
            rejection =
                kOpenA8DJTimecodeRejectionReservedNonzero;
        } else if (payload.modeID !=
                   kOpenA8DJDriverModeTimecodeOptimized) {
            rejection = kOpenA8DJTimecodeRejectionUnknownMode;
        } else if (payload.allowedInputPairMask !=
                   kOpenA8DJTimecodeAllowedPairMask) {
            rejection =
                kOpenA8DJTimecodeRejectionInvalidPairMask;
        } else {
            for (size_t index = 0;
                 index < sizeof(payload.reserved);
                 index++) {
                if (payload.reserved[index] != 0) {
                    rejection =
                        kOpenA8DJTimecodeRejectionReservedNonzero;
                    break;
                }
            }
        }
    }
    if (outRejection != NULL) *outRejection = rejection;
    if (rejection != kOpenA8DJTimecodeRejectionNone) return false;
    if (outPayload != NULL) {
        *outPayload = payload;
    }
    return true;
}

static inline bool OpenA8DJTimecodeValidateArmPayload(
    const void *bytes,
    size_t length,
    OpenA8DJTimecodeArmPayload *outPayload)
{
    return OpenA8DJTimecodeValidateArmPayloadDetailed(
        bytes, length, outPayload, NULL);
}

static inline bool OpenA8DJTimecodeArm(
    OpenA8DJTimecodeState *state,
    uint32_t fallbackMode,
    uint8_t profile,
    double sampleRate,
    uint32_t bufferFrames)
{
    if (state == NULL ||
        (fallbackMode != kOpenA8DJDriverModeBalanced &&
         fallbackMode != kOpenA8DJDriverModePerformance) ||
        OpenA8DJTimecodeFramesPerWindow(sampleRate) == 0 ||
        bufferFrames == 0) {
        return false;
    }
    if (state->armed) {
        return state->allowedInputPairMask == kOpenA8DJTimecodeAllowedPairMask;
    }
    state->armed = 1;
    state->armState =
        profile == kOpenA8DJTimecodeProfileUnavailable ?
            kOpenA8DJTimecodeWaitingProfile :
            kOpenA8DJTimecodeQualifying;
    state->allowedInputPairMask = kOpenA8DJTimecodeAllowedPairMask;
    state->profileVerified =
        profile != kOpenA8DJTimecodeProfileUnavailable;
    state->fallbackMode = (uint8_t)fallbackMode;
    state->electricalProfile = profile;
    state->windowFrames = OpenA8DJTimecodeFramesPerWindow(sampleRate);
    state->sampleRateSnapshot = sampleRate;
    state->bufferFramesSnapshot = bufferFrames;
    state->eligibleWindows = 0;
    state->dropoutWindows = 0;
    state->lastFailOpenReason = kOpenA8DJTimecodeFailNone;
    state->counters.arms++;
    state->generation++;
    return true;
}

static inline void OpenA8DJTimecodeDisarm(
    OpenA8DJTimecodeState *state,
    uint8_t reason)
{
    if (state == NULL) return;
    if (state->armed) state->counters.disarms++;
    state->armed = 0;
    state->qualified = 0;
    state->optimizedActive = 0;
    state->allowedInputPairMask = 0;
    state->eligibleWindows = 0;
    state->dropoutWindows = 0;
    state->armState = reason == kOpenA8DJTimecodeFailNone ||
                              reason == kOpenA8DJTimecodeFailExplicitDisarm ?
        kOpenA8DJTimecodeDisarmed : kOpenA8DJTimecodeFaulted;
    state->lastFailOpenReason = reason;
    state->generation++;
}

static inline uint8_t OpenA8DJTimecodeObserveWindow(
    OpenA8DJTimecodeState *state,
    const OpenA8DJTimecodeWindow *window,
    bool streaming)
{
    if (state == NULL || !state->armed) return kOpenA8DJTimecodeFailNone;
    if (!OpenA8DJTimecodeWindowValid(window, state->windowFrames)) {
        return kOpenA8DJTimecodeFailStatsInvalid;
    }
    if (OpenA8DJTimecodeForbiddenTrip(window)) {
        state->eligibleWindows = 0;
        return kOpenA8DJTimecodeFailOutsideAllowlist;
    }
    bool entry = OpenA8DJTimecodePairEntry(window, 0) &&
                 OpenA8DJTimecodePairEntry(window, 1);
    bool hold = OpenA8DJTimecodePairHold(window, 0) &&
                OpenA8DJTimecodePairHold(window, 1);
    if (!state->optimizedActive) {
        state->eligibleWindows = entry && state->eligibleWindows < UINT8_MAX ?
            (uint8_t)(state->eligibleWindows + 1) : 0;
        if (state->eligibleWindows >= kOpenA8DJTimecodeRequiredEligibleWindows) {
            state->qualified = 1;
            state->armState = streaming ?
                kOpenA8DJTimecodeQualifiedPendingBoundary :
                kOpenA8DJTimecodeActive;
            state->counters.qualifications++;
            state->generation++;
            return UINT8_MAX; /* qualification decision */
        }
        state->armState = kOpenA8DJTimecodeQualifying;
        return kOpenA8DJTimecodeFailNone;
    }
    if (hold) {
        state->dropoutWindows = 0;
    } else if (++state->dropoutWindows >= kOpenA8DJTimecodeDropoutWindows) {
        return kOpenA8DJTimecodeFailAllowedPairDropout;
    }
    return kOpenA8DJTimecodeFailNone;
}

#endif
