#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "../hal/OpenA8DJDriverMode.h"
#include "../hal/OpenA8DJTimecodeOptimized.h"
#include "../hal/OpenA8DJVintageCompatible.h"

#ifndef OPENA8DJ_PUBLIC_API_SOCKET_PATH
#define OPENA8DJ_PUBLIC_API_SOCKET_PATH "/tmp/opena8dj-control.sock"
#endif

static const char *kSocketPath = OPENA8DJ_PUBLIC_API_SOCKET_PATH;

#ifdef OPENA8DJ_PUBLIC_API_LOCK_PATH
static const char *kPublicAPILockPath = OPENA8DJ_PUBLIC_API_LOCK_PATH;
#endif

static const char *kPublicAPISchema = "org.opena8dj.public-api.response.v1";
static const char *kPublicAPIVersion = "1.0";

#define OPENA8DJ_ERROR_DRIVER_MODE_NOT_ALLOWED "driver_mode_not_allowed"
#define OPENA8DJ_ERROR_DRIVER_MODE_BUSY "driver_mode_busy"
#define OPENA8DJ_ERROR_DRIVER_MODE_APPLY_FAILED "driver_mode_apply_failed"
#define OPENA8DJ_ERROR_DRIVER_MODE_ARM_REQUIRED "driver_mode_arm_required"
#define OPENA8DJ_ERROR_DRIVER_MODE_CONFLICT "driver_mode_conflict"

enum {
    kIPCVersion = 1,
    kIPCMagic = 0x4a443841,
    kIPCTypeHello = 1,
    kIPCTypeMidiToDevice = 2,
    kIPCTypeMidiFromDevice = 3,
    kIPCTypeControlGet = 4,
    kIPCTypeControlSet = 5,
    kIPCTypeControlState = 6,
    kIPCTypeStatus = 7,
    kIPCTypeInputStatsGet = 8,
    kIPCTypeInputStats = 9,
    kIPCTypeStreamStatsGet = 10,
    kIPCTypeStreamStats = 11,
    kIPCTypeDriverModeGet = 12,
    kIPCTypeDriverModeSet = 13,
    kIPCTypeDriverModeState = 14,
    kIPCTypeTimecodeOptimizedGet = 15,
    kIPCTypeTimecodeOptimizedArm = 16,
    kIPCTypeTimecodeOptimizedDisarm = 17,
    kIPCTypeTimecodeOptimizedState = 18,
    kIPCTypeVintageCompatibleGet = 19,
    kIPCTypeVintageCompatibleState = 20
};

enum {
    kInputPairs = 4
};

enum {
    kInputTransformPairMask = 0x0f
};

typedef struct OpenA8DJIPCHeader {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t length;
} __attribute__((packed)) OpenA8DJIPCHeader;

typedef struct OpenA8DJControlPayload {
    uint8_t inputMode;
    uint8_t gndLiftTCVinyl;
    uint8_t gndLiftTCCDLine;
    uint8_t gndLiftPhono;
    uint8_t softwareLock;
    uint8_t inputSwapMask;
    uint8_t inputInvertLeftMask;
    uint8_t inputInvertRightMask;
    uint8_t inputSource[kInputPairs];
    uint8_t inputDecodeEnabled;
} __attribute__((packed)) OpenA8DJControlPayload;

typedef struct OpenA8DJInputStatsPayload {
    uint64_t frames[kInputPairs];
    double leftSquare[kInputPairs];
    double rightSquare[kInputPairs];
    double cross[kInputPairs];
    double leftPeak[kInputPairs];
    double rightPeak[kInputPairs];
} __attribute__((packed)) OpenA8DJInputStatsPayload;

typedef struct OpenA8DJStreamStatsPayload {
    uint8_t streaming;
    uint8_t clockAnchorValid;
    uint8_t playbackExplicitScheduling;
    uint8_t reserved;
    uint32_t outputRingFrames;
    uint32_t outputTargetLatencyFrames;
    uint32_t outputByteInFrame;
    uint32_t playbackLeadFrames;
    uint32_t playbackQueueTarget;
    uint32_t playbackTransfersInFlight;
    double sampleRate;
    double clockSampleTime;
    uint64_t clockHostTime;
    uint64_t clockUSBTime;
    uint64_t clockUSBFrameNumber;
    uint64_t clockUSBFrameHostTime;
    double clockHostTicksPerUSBFrame;
    uint64_t clockUSBFrameSamples;
    uint64_t clockUSBFrameResyncs;
    uint64_t clockSeed;
    uint64_t clockFramesObserved;
    uint64_t clockAcceptedAnchors;
    uint64_t clockRejectedAnchors;
    uint64_t clockAnchorResets;
    uint64_t captureTransfers;
    uint64_t playbackTransfers;
    uint64_t captureTransactions;
    uint64_t playbackTransactions;
    uint64_t captureBytes;
    uint64_t playbackBytes;
    uint64_t captureTransactionFailures;
    uint64_t playbackTransactionFailures;
    uint64_t captureShortTransfers;
    uint64_t playbackShortTransfers;
    uint64_t filteredCaptureTransactions;
    uint64_t captureQueueFailures;
    uint64_t playbackQueueFailures;
    uint64_t outputFramesWritten;
    uint64_t outputFramesRead;
    uint64_t outputUnderruns;
    uint64_t outputActiveUnderruns;
    uint64_t outputStartupSilenceFrames;
    uint64_t outputRingOverruns;
    uint64_t outputElasticDrops;
    uint64_t outputElasticReplays;
    uint64_t outputTimelineResets;
    uint64_t playbackReschedules;
    uint64_t playbackNextFrameNumber;
    uint64_t playbackScheduleResets;
    uint64_t playbackScheduleTooOld;
    uint64_t playbackScheduleTooNew;
    uint64_t playbackScheduleOutOfWindow;
    uint64_t playbackScheduleFallbacks;
    uint64_t inputCheckErrors;
    uint64_t outputPanicFlags;
    double outputPeak;
    uint64_t outputNearClipSamples;
    uint64_t outputClippedSamples;
    uint64_t captureStatusFailures;
    uint64_t captureZeroCompleteTransactions;
    uint64_t captureExpectedTransactions;
    uint64_t captureOtherByteCountTransactions;
    uint64_t captureCompletionDeltaMin;
    uint64_t captureCompletionDeltaMax;
    uint64_t captureCompletionDeltaSum;
    uint64_t captureCompletionDeltaSamples;
    uint64_t playbackCompletionDeltaMin;
    uint64_t playbackCompletionDeltaMax;
    uint64_t playbackCompletionDeltaSum;
    uint64_t playbackCompletionDeltaSamples;
    uint64_t captureToPlaybackQueueDeltaMin;
    uint64_t captureToPlaybackQueueDeltaMax;
    uint64_t captureToPlaybackQueueDeltaSum;
    uint64_t captureToPlaybackQueueDeltaSamples;
    uint64_t cadenceExpectedTransferTicks;
    uint64_t captureCompletionDeltaOutliers;
    uint64_t captureCompletionDeltaOutlierMax;
    uint64_t captureCompletionDeltaOutlierSum;
    uint64_t playbackCompletionDeltaOutliers;
    uint64_t playbackCompletionDeltaOutlierMax;
    uint64_t playbackCompletionDeltaOutlierSum;
    uint64_t captureToPlaybackQueueDeltaOutliers;
    uint64_t captureToPlaybackQueueDeltaOutlierMax;
    uint64_t captureToPlaybackQueueDeltaOutlierSum;
    uint64_t captureUSBTimestampDeltaMin;
    uint64_t captureUSBTimestampDeltaMax;
    uint64_t captureUSBTimestampDeltaSum;
    uint64_t captureUSBTimestampDeltaSamples;
    uint64_t captureUSBTimestampOutOfOrder;
    uint64_t captureUSBTimestampRepeated;
    uint64_t captureUSBTimestampZero;
    uint64_t captureRequestCountMin;
    uint64_t captureRequestCountMax;
    uint64_t captureRequestCountSum;
    uint64_t captureRequestCountSamples;
    uint64_t captureCompleteCountMin;
    uint64_t captureCompleteCountMax;
    uint64_t captureCompleteCountSum;
    uint64_t captureCompleteCountSamples;
    uint64_t captureOffsetMin;
    uint64_t captureOffsetMax;
    uint64_t captureOffsetSum;
    uint64_t captureOffsetSamples;
    uint64_t captureLayoutSignatureSum;
    uint64_t playbackQueueAttempts;
    uint64_t playbackQueueBytesMin;
    uint64_t playbackQueueBytesMax;
    uint64_t playbackQueueBytesSum;
    uint64_t playbackQueueBytesSamples;
    uint64_t playbackQueueTransactionsMin;
    uint64_t playbackQueueTransactionsMax;
    uint64_t playbackQueueTransactionsSum;
    uint64_t playbackQueueTransactionsSamples;
    uint64_t playbackQueueRequestCountMin;
    uint64_t playbackQueueRequestCountMax;
    uint64_t playbackQueueRequestCountSum;
    uint64_t playbackQueueRequestCountSamples;
    uint64_t playbackQueueLayoutSignatureSum;
    uint64_t playbackInFlightAtQueueMin;
    uint64_t playbackInFlightAtQueueMax;
    uint64_t playbackInFlightAtQueueSum;
    uint64_t playbackInFlightAtQueueSamples;
    uint64_t playbackInFlightAtCompletionMin;
    uint64_t playbackInFlightAtCompletionMax;
    uint64_t playbackInFlightAtCompletionSum;
    uint64_t playbackInFlightAtCompletionSamples;
    uint64_t playbackCompleteCountMin;
    uint64_t playbackCompleteCountMax;
    uint64_t playbackCompleteCountSum;
    uint64_t playbackCompleteCountSamples;
    uint64_t playbackRequestCountMin;
    uint64_t playbackRequestCountMax;
    uint64_t playbackRequestCountSum;
    uint64_t playbackRequestCountSamples;
    uint64_t playbackZeroCompleteTransactions;
    uint64_t playbackLayoutSignatureSum;
    uint64_t outputLateWriteFrames;
    uint64_t outputLateWriteBatches;
    uint64_t captureCompletionJitterSamples;
    uint64_t captureCompletionJitterInvalidIntervals;
    uint64_t captureCompletionJitterLe50;
    uint64_t captureCompletionJitterLe100;
    uint64_t captureCompletionJitterLe250;
    uint64_t captureCompletionJitterLe500;
    uint64_t captureCompletionJitterLe1000;
    uint64_t captureCompletionJitterGt1000;
    uint64_t playbackCompletionJitterSamples;
    uint64_t playbackCompletionJitterInvalidIntervals;
    uint64_t playbackCompletionJitterLe50;
    uint64_t playbackCompletionJitterLe100;
    uint64_t playbackCompletionJitterLe250;
    uint64_t playbackCompletionJitterLe500;
    uint64_t playbackCompletionJitterLe1000;
    uint64_t playbackCompletionJitterGt1000;
    uint64_t captureISOCompletionStatusFailures;
    uint64_t captureISOTransactionStatusFailures;
    uint64_t captureISOZeroLengthTransactions;
    uint64_t captureISOShortTransactions;
    uint64_t playbackISOCompletionStatusFailures;
    uint64_t playbackISOTransactionStatusFailures;
    uint64_t playbackISOZeroLengthTransactions;
    uint64_t playbackISOShortTransactions;
    uint64_t qualityInstrumentationEnabled;
    uint64_t deviceInfoAvailable;
    uint64_t deviceFirmwareVersion;
    uint64_t deviceHardwareSubtype;
    uint64_t deviceNumAnalogAudioOut;
    uint64_t deviceNumAnalogAudioIn;
    uint64_t deviceNumDigitalAudioOut;
    uint64_t deviceNumDigitalAudioIn;
    uint64_t deviceNumMidiOut;
    uint64_t deviceNumMidiIn;
    uint64_t deviceDataAlignment;
    uint64_t driverModeSchemaVersion;
    uint64_t driverModeRequested;
    uint64_t driverModeEffective;
    uint64_t driverModePending;
    uint64_t driverModeLastResult;
    uint64_t driverModeRejectionReason;
    uint64_t driverModeGeneration;
    uint64_t driverModeAcceptedRequests;
    uint64_t driverModeRejectedRequests;
    uint64_t driverModeAppliedTransitions;
    uint64_t driverModeApplyFailures;
    uint64_t driverModePendingTransitions;
    uint64_t driverModeOutputStartLatencyFrames;
    uint64_t driverModeOutputRestartLatencyFrames;
    uint64_t driverModeOutputTargetLatencyFrames;
    uint64_t driverModeWorkerQoS;
    OpenA8DJTimecodeStatePayload timecodeOptimized;
    OpenA8DJVintageStatePayload vintageCompatible;
} __attribute__((packed)) OpenA8DJStreamStatsPayload;
_Static_assert(sizeof(OpenA8DJStreamStatsPayload) <= 4096,
               "stream stats must fit the bounded IPC payload");

typedef struct OpenA8DJWakeState {
    AudioDeviceID device;
    AudioDeviceIOProcID ioProcID;
} OpenA8DJWakeState;

static OpenA8DJWakeState gWakeState = {
    .device = kAudioObjectUnknown,
    .ioProcID = NULL
};
static bool gWakeCleanupRegistered = false;

static bool ParseBool(const char *text, uint8_t *outValue);

static const char *InputModeName(uint8_t mode)
{
    switch (mode) {
        case 0:
            return "timecode-vinyl";
        case 1:
            return "timecode-cd-line";
        case 2:
            return "phono";
        default:
            return "unknown";
    }
}

static bool ParseInputMode(const char *text, uint8_t *outMode)
{
    if (strcmp(text, "0") == 0 ||
        strcmp(text, "timecode-vinyl") == 0 ||
        strcmp(text, "tc-vinyl") == 0) {
        *outMode = 0;
        return true;
    }
    if (strcmp(text, "1") == 0 ||
        strcmp(text, "timecode-cd-line") == 0 ||
        strcmp(text, "timecode-cd") == 0 ||
        strcmp(text, "cd-line") == 0 ||
        strcmp(text, "line") == 0) {
        *outMode = 1;
        return true;
    }
    if (strcmp(text, "2") == 0 || strcmp(text, "phono") == 0) {
        *outMode = 2;
        return true;
    }
    return false;
}

static int ParseInputPair(const char *text)
{
    if (text == NULL || text[0] == '\0' || text[1] != '\0') {
        return -1;
    }
    if (text[0] >= 'A' && text[0] <= 'D') {
        return text[0] - 'A';
    }
    if (text[0] >= 'a' && text[0] <= 'd') {
        return text[0] - 'a';
    }
    return -1;
}

static uint8_t InputTransformForPair(const OpenA8DJControlPayload *state, int pair)
{
    uint8_t pairBit = (uint8_t)(1u << pair);
    uint8_t transform = 0;
    if ((state->inputSwapMask & pairBit) != 0) {
        transform |= 1u << 0;
    }
    if ((state->inputInvertLeftMask & pairBit) != 0) {
        transform |= 1u << 1;
    }
    if ((state->inputInvertRightMask & pairBit) != 0) {
        transform |= 1u << 2;
    }
    return transform;
}

static const char *InputTransformName(uint8_t transform)
{
    switch (transform) {
        case 0:
            return "normal";
        case 1:
            return "swap";
        case 2:
            return "invert-left";
        case 4:
            return "invert-right";
        case 6:
            return "invert-both";
        case 3:
            return "swap-invert-left";
        case 5:
            return "swap-invert-right";
        case 7:
            return "swap-invert-both";
        default:
            return "unknown";
    }
}

static bool ParseInputTransform(const char *text, uint8_t *outTransform)
{
    if (strcmp(text, "normal") == 0 || strcmp(text, "none") == 0 || strcmp(text, "off") == 0) {
        *outTransform = 0;
        return true;
    }
    if (strcmp(text, "swap") == 0 || strcmp(text, "swap-lr") == 0) {
        *outTransform = 1;
        return true;
    }
    if (strcmp(text, "invert-left") == 0 || strcmp(text, "invert-l") == 0) {
        *outTransform = 2;
        return true;
    }
    if (strcmp(text, "invert-right") == 0 || strcmp(text, "invert-r") == 0) {
        *outTransform = 4;
        return true;
    }
    if (strcmp(text, "invert-both") == 0 || strcmp(text, "invert") == 0 || strcmp(text, "phase") == 0) {
        *outTransform = 6;
        return true;
    }
    if (strcmp(text, "swap-invert-left") == 0 || strcmp(text, "swap-invert-l") == 0) {
        *outTransform = 3;
        return true;
    }
    if (strcmp(text, "swap-invert-right") == 0 || strcmp(text, "swap-invert-r") == 0) {
        *outTransform = 5;
        return true;
    }
    if (strcmp(text, "swap-invert-both") == 0 || strcmp(text, "swap-invert") == 0) {
        *outTransform = 7;
        return true;
    }
    return false;
}

static void SetInputTransform(OpenA8DJControlPayload *state, int pair, uint8_t transform)
{
    uint8_t pairBit = (uint8_t)(1u << pair);
    state->inputSwapMask &= (uint8_t)~pairBit;
    state->inputInvertLeftMask &= (uint8_t)~pairBit;
    state->inputInvertRightMask &= (uint8_t)~pairBit;
    if ((transform & (1u << 0)) != 0) {
        state->inputSwapMask |= pairBit;
    }
    if ((transform & (1u << 1)) != 0) {
        state->inputInvertLeftMask |= pairBit;
    }
    if ((transform & (1u << 2)) != 0) {
        state->inputInvertRightMask |= pairBit;
    }
    state->inputSwapMask &= kInputTransformPairMask;
    state->inputInvertLeftMask &= kInputTransformPairMask;
    state->inputInvertRightMask &= kInputTransformPairMask;
}

static void ResetInputTransforms(OpenA8DJControlPayload *state)
{
    state->inputSwapMask = 0;
    state->inputInvertLeftMask = 0;
    state->inputInvertRightMask = 0;
    for (uint8_t pair = 0; pair < kInputPairs; pair++) {
        state->inputSource[pair] = pair;
    }
}

static bool ApplyProfile(const char *name, OpenA8DJControlPayload *state)
{
    if (strcmp(name, "playback") == 0 ||
        strcmp(name, "output-only") == 0 ||
        strcmp(name, "spotify") == 0 ||
        strcmp(name, "vlc") == 0) {
        state->inputDecodeEnabled = 0;
        return true;
    }
    if (strcmp(name, "timecode-vinyl") == 0 || strcmp(name, "tc-vinyl") == 0) {
        state->inputMode = 0;
        state->gndLiftTCVinyl = 1;
        state->gndLiftTCCDLine = 0;
        state->gndLiftPhono = 0;
        state->softwareLock = 1;
        state->inputDecodeEnabled = 1;
        ResetInputTransforms(state);
        return true;
    }
    if (strcmp(name, "timecode-cd-line") == 0 ||
        strcmp(name, "timecode-cd") == 0 ||
        strcmp(name, "cd-line") == 0 ||
        strcmp(name, "line") == 0) {
        state->inputMode = 1;
        state->gndLiftTCVinyl = 0;
        state->gndLiftTCCDLine = 1;
        state->gndLiftPhono = 0;
        state->softwareLock = 1;
        state->inputDecodeEnabled = 1;
        ResetInputTransforms(state);
        return true;
    }
    if (strcmp(name, "phono") == 0) {
        state->inputMode = 2;
        state->gndLiftTCVinyl = 0;
        state->gndLiftTCCDLine = 0;
        state->gndLiftPhono = 1;
        state->softwareLock = 1;
        state->inputDecodeEnabled = 1;
        ResetInputTransforms(state);
        return true;
    }
    if (strcmp(name, "unlock") == 0) {
        state->softwareLock = 0;
        return true;
    }
    return false;
}

typedef struct OpenA8DJPreset {
    const char *name;
    const char *title;
    const char *surface;
    const char *summary;
} OpenA8DJPreset;

static const OpenA8DJPreset kBuiltInPresets[] = {
    {
        "playback-4out",
        "Playback / 4 Stereo Outputs",
        "playback",
        "Outputs A/B/C/D active, input decode off"
    },
    {
        "traktor-dvs-vinyl",
        "Traktor DVS Vinyl",
        "dvs",
        "A/B timecode vinyl, input mode 0, vinyl ground lift, software lock"
    },
    {
        "traktor-dvs-cd-line",
        "Traktor DVS CD-Line",
        "dvs",
        "A/B timecode CD or line players, input mode 1, CD-line ground lift, software lock"
    },
    {
        "vinyl-recording",
        "Vinyl Recording",
        "recording",
        "A/B phono capture, input mode 2, phono ground lift, software lock"
    },
    {
        "dj-set-recording",
        "DJ Set Recording",
        "recording",
        "C/D line capture workflow, input decode on, preserves A/B input mode"
    },
    {
        "effects-loop",
        "External Effects Loop",
        "duplex",
        "C/D duplex effects workflow, input decode on, preserves A/B input mode"
    },
    {
        "microphone",
        "Microphone",
        "input",
        "Front XLR mic workflow, input decode on, physical MIC/LINE switch required"
    },
    {
        "midi-only",
        "MIDI Only",
        "midi",
        "MIDI bridge workflow, playback-safe audio state"
    },
    {
        "ground-diagnostics",
        "Ground Diagnostics",
        "diagnostics",
        "Input/noise measurement workflow, input decode on, software lock"
    },
    {
        "engineering-diagnostics",
        "Engineering Diagnostics",
        "diagnostics",
        "Full input/output diagnostics, input decode on, software lock"
    }
};

static const size_t kBuiltInPresetCount = sizeof(kBuiltInPresets) / sizeof(kBuiltInPresets[0]);

static bool ApplyPreset(const char *name, OpenA8DJControlPayload *state)
{
    if (strcmp(name, "playback-4out") == 0 ||
        strcmp(name, "playback") == 0 ||
        strcmp(name, "output-only") == 0) {
        return ApplyProfile("playback", state);
    }
    if (strcmp(name, "traktor-dvs-vinyl") == 0 ||
        strcmp(name, "dvs-vinyl") == 0 ||
        strcmp(name, "timecode-vinyl") == 0) {
        return ApplyProfile("timecode-vinyl", state);
    }
    if (strcmp(name, "traktor-dvs-cd-line") == 0 ||
        strcmp(name, "dvs-cd-line") == 0 ||
        strcmp(name, "timecode-cd-line") == 0) {
        return ApplyProfile("timecode-cd-line", state);
    }
    if (strcmp(name, "vinyl-recording") == 0 ||
        strcmp(name, "phono-recording") == 0 ||
        strcmp(name, "phono") == 0) {
        return ApplyProfile("phono", state);
    }
    if (strcmp(name, "dj-set-recording") == 0 ||
        strcmp(name, "effects-loop") == 0 ||
        strcmp(name, "microphone") == 0) {
        state->inputDecodeEnabled = 1;
        state->softwareLock = 0;
        ResetInputTransforms(state);
        return true;
    }
    if (strcmp(name, "midi-only") == 0) {
        return ApplyProfile("playback", state);
    }
    if (strcmp(name, "ground-diagnostics") == 0 ||
        strcmp(name, "engineering-diagnostics") == 0) {
        state->inputDecodeEnabled = 1;
        state->softwareLock = 1;
        ResetInputTransforms(state);
        return true;
    }
    return false;
}

static void PrintProfiles(void)
{
    printf("OpenA8DJ built-in presets\n");
    for (size_t i = 0; i < kBuiltInPresetCount; i++) {
        printf("  %s\n", kBuiltInPresets[i].name);
        printf("    title:   %s\n", kBuiltInPresets[i].title);
        printf("    surface: %s\n", kBuiltInPresets[i].surface);
        printf("    summary: %s\n", kBuiltInPresets[i].summary);
    }
}

static const char *BoolJSON(uint8_t value)
{
    return value ? "true" : "false";
}

static const char *DriverModeName(uint32_t modeID)
{
    switch (modeID) {
        case kOpenA8DJDriverModeBalanced:
            return "balanced";
        case kOpenA8DJDriverModePerformance:
            return "performance";
        case kOpenA8DJDriverModeTimecodeOptimized:
            return "timecode-optimized";
        case kOpenA8DJDriverModeVintageCompatible:
            return "vintage-compatible";
        default:
            return NULL;
    }
}

static bool ParseDriverMode(const char *text, uint32_t *outModeID)
{
    if (strcmp(text, "balanced") == 0) {
        *outModeID = kOpenA8DJDriverModeBalanced;
        return true;
    }
    if (strcmp(text, "performance") == 0) {
        *outModeID = kOpenA8DJDriverModePerformance;
        return true;
    }
    if (strcmp(text, "timecode-optimized") == 0) {
        *outModeID = kOpenA8DJDriverModeTimecodeOptimized;
        return true;
    }
    if (strcmp(text, "vintage-compatible") == 0) {
        *outModeID = kOpenA8DJDriverModeVintageCompatible;
        return true;
    }
    return false;
}

static const char *DriverModeResultName(uint8_t result)
{
    switch (result) {
        case kOpenA8DJDriverModeResultUnchanged:
            return "unchanged";
        case kOpenA8DJDriverModeResultApplied:
            return "applied";
        case kOpenA8DJDriverModeResultPending:
            return "pending";
        case kOpenA8DJDriverModeResultCancelled:
            return "cancelled";
        case kOpenA8DJDriverModeResultInvalid:
            return "invalid";
        case kOpenA8DJDriverModeResultApplyFailed:
            return "apply_failed";
        default:
            return NULL;
    }
}

static const char *DriverModeRejectionName(uint8_t rejection)
{
    switch (rejection) {
        case kOpenA8DJDriverModeRejectionNone:
            return "none";
        case kOpenA8DJDriverModeRejectionBadLength:
            return "bad_length";
        case kOpenA8DJDriverModeRejectionUnsupportedSchema:
            return "unsupported_schema";
        case kOpenA8DJDriverModeRejectionReservedNonzero:
            return "reserved_nonzero";
        case kOpenA8DJDriverModeRejectionUnknownMode:
            return "unknown_mode";
        case kOpenA8DJDriverModeRejectionArmRequired:
            return "arm_required";
        case kOpenA8DJDriverModeRejectionConflict:
            return "conflict";
        default:
            return NULL;
    }
}

static const char *DriverModeQoSName(uint32_t qos)
{
    switch (qos) {
        case kOpenA8DJDriverModeWorkerQoSDefault:
            return "default";
        case kOpenA8DJDriverModeWorkerQoSUserInteractive:
            return "user-interactive";
        default:
            return NULL;
    }
}

static char PairLetter(uint8_t value, int fallback)
{
    return (char)('A' + (value < kInputPairs ? value : fallback));
}

static const char *InferredPresetName(const OpenA8DJControlPayload *state)
{
    if (state->inputDecodeEnabled == 0) {
        return "playback-4out";
    }
    if (state->inputMode == 0 && state->gndLiftTCVinyl && state->softwareLock) {
        return "traktor-dvs-vinyl";
    }
    if (state->inputMode == 1 && state->gndLiftTCCDLine && state->softwareLock) {
        return "traktor-dvs-cd-line";
    }
    if (state->inputMode == 2 && state->gndLiftPhono && state->softwareLock) {
        return "vinyl-recording";
    }
    return "custom";
}

static void ExportConfig(FILE *out, const OpenA8DJControlPayload *state)
{
    fprintf(out, "{\n");
    fprintf(out, "  \"schema\": \"org.opena8dj.control.v1\",\n");
    fprintf(out, "  \"preset\": \"%s\",\n", InferredPresetName(state));
    fprintf(out, "  \"inputMode\": \"%s\",\n", InputModeName(state->inputMode));
    fprintf(out, "  \"inputModeValue\": %u,\n", state->inputMode);
    fprintf(out, "  \"inputDecode\": %s,\n", BoolJSON(state->inputDecodeEnabled));
    fprintf(out, "  \"softwareLock\": %s,\n", BoolJSON(state->softwareLock));
    fprintf(out, "  \"groundLiftVinyl\": %s,\n", BoolJSON(state->gndLiftTCVinyl));
    fprintf(out, "  \"groundLiftCDLine\": %s,\n", BoolJSON(state->gndLiftTCCDLine));
    fprintf(out, "  \"groundLiftPhono\": %s,\n", BoolJSON(state->gndLiftPhono));
    fprintf(out, "  \"inputSourceA\": \"%c\",\n", PairLetter(state->inputSource[0], 0));
    fprintf(out, "  \"inputSourceB\": \"%c\",\n", PairLetter(state->inputSource[1], 1));
    fprintf(out, "  \"inputSourceC\": \"%c\",\n", PairLetter(state->inputSource[2], 2));
    fprintf(out, "  \"inputSourceD\": \"%c\",\n", PairLetter(state->inputSource[3], 3));
    fprintf(out, "  \"inputTransformA\": \"%s\",\n", InputTransformName(InputTransformForPair(state, 0)));
    fprintf(out, "  \"inputTransformB\": \"%s\",\n", InputTransformName(InputTransformForPair(state, 1)));
    fprintf(out, "  \"inputTransformC\": \"%s\",\n", InputTransformName(InputTransformForPair(state, 2)));
    fprintf(out, "  \"inputTransformD\": \"%s\",\n", InputTransformName(InputTransformForPair(state, 3)));
    fprintf(out, "  \"inputSources\": {\n");
    fprintf(out, "    \"A\": \"%c\",\n", PairLetter(state->inputSource[0], 0));
    fprintf(out, "    \"B\": \"%c\",\n", PairLetter(state->inputSource[1], 1));
    fprintf(out, "    \"C\": \"%c\",\n", PairLetter(state->inputSource[2], 2));
    fprintf(out, "    \"D\": \"%c\"\n", PairLetter(state->inputSource[3], 3));
    fprintf(out, "  },\n");
    fprintf(out, "  \"inputTransforms\": {\n");
    fprintf(out, "    \"A\": \"%s\",\n", InputTransformName(InputTransformForPair(state, 0)));
    fprintf(out, "    \"B\": \"%s\",\n", InputTransformName(InputTransformForPair(state, 1)));
    fprintf(out, "    \"C\": \"%s\",\n", InputTransformName(InputTransformForPair(state, 2)));
    fprintf(out, "    \"D\": \"%s\"\n", InputTransformName(InputTransformForPair(state, 3)));
    fprintf(out, "  }\n");
    fprintf(out, "}\n");
}

static bool WriteConfigFile(const char *path, const OpenA8DJControlPayload *state)
{
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        return false;
    }
    ExportConfig(file, state);
    return fclose(file) == 0;
}

static char *ReadTextFile(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long length = ftell(file);
    if (length < 0 || length > 1024 * 1024) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    char *buffer = (char *)calloc((size_t)length + 1, 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }
    size_t readLength = fread(buffer, 1, (size_t)length, file);
    fclose(file);
    buffer[readLength] = '\0';
    return buffer;
}

static const char *FindJSONValue(const char *json, const char *key)
{
    char pattern[128];
    int written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (written < 0 || (size_t)written >= sizeof(pattern)) {
        return NULL;
    }
    const char *match = strstr(json, pattern);
    if (match == NULL) {
        return NULL;
    }
    const char *colon = strchr(match + written, ':');
    if (colon == NULL) {
        return NULL;
    }
    const char *value = colon + 1;
    while (*value != '\0' && isspace((unsigned char)*value)) {
        value++;
    }
    return value;
}

static bool ReadJSONString(const char *json, const char *key, char *out, size_t outSize)
{
    const char *value = FindJSONValue(json, key);
    if (value == NULL || *value != '"' || outSize == 0) {
        return false;
    }
    value++;
    size_t i = 0;
    while (*value != '\0' && *value != '"' && i + 1 < outSize) {
        if (*value == '\\' && value[1] != '\0') {
            value++;
        }
        out[i++] = *value++;
    }
    out[i] = '\0';
    return *value == '"';
}

static bool ReadJSONBool(const char *json, const char *key, uint8_t *out)
{
    const char *value = FindJSONValue(json, key);
    if (value == NULL) {
        return false;
    }
    if (strncmp(value, "true", 4) == 0 || strncmp(value, "1", 1) == 0) {
        *out = 1;
        return true;
    }
    if (strncmp(value, "false", 5) == 0 || strncmp(value, "0", 1) == 0) {
        *out = 0;
        return true;
    }
    char text[16];
    if (ReadJSONString(json, key, text, sizeof(text)) && ParseBool(text, out)) {
        return true;
    }
    return false;
}

static bool ApplyConfigJSON(const char *json, OpenA8DJControlPayload *state)
{
    char text[64];
    if (ReadJSONString(json, "preset", text, sizeof(text)) && strcmp(text, "custom") != 0) {
        if (!ApplyPreset(text, state)) {
            return false;
        }
    }
    if (ReadJSONString(json, "inputMode", text, sizeof(text))) {
        uint8_t mode = 0;
        if (!ParseInputMode(text, &mode)) {
            return false;
        }
        state->inputMode = mode;
    }
    uint8_t value = 0;
    if (ReadJSONBool(json, "inputDecode", &value)) {
        state->inputDecodeEnabled = value;
    }
    if (ReadJSONBool(json, "softwareLock", &value)) {
        state->softwareLock = value;
    }
    if (ReadJSONBool(json, "groundLiftVinyl", &value)) {
        state->gndLiftTCVinyl = value;
    }
    if (ReadJSONBool(json, "groundLiftCDLine", &value)) {
        state->gndLiftTCCDLine = value;
    }
    if (ReadJSONBool(json, "groundLiftPhono", &value)) {
        state->gndLiftPhono = value;
    }
    for (int pair = 0; pair < kInputPairs; pair++) {
        char key[32];
        snprintf(key, sizeof(key), "inputSource%c", 'A' + pair);
        if (ReadJSONString(json, key, text, sizeof(text))) {
            int source = ParseInputPair(text);
            if (source < 0) {
                return false;
            }
            state->inputSource[pair] = (uint8_t)source;
        }
        snprintf(key, sizeof(key), "inputTransform%c", 'A' + pair);
        if (ReadJSONString(json, key, text, sizeof(text))) {
            uint8_t transform = 0;
            if (!ParseInputTransform(text, &transform)) {
                return false;
            }
            SetInputTransform(state, pair, transform);
        }
    }
    return true;
}

static bool ReadFull(int fd, void *buffer, size_t length)
{
    uint8_t *bytes = buffer;
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = read(fd, bytes + offset, length - offset);
        if (count <= 0) return false;
        offset += (size_t)count;
    }
    return true;
}

static bool WriteFull(int fd, const void *buffer, size_t length)
{
    const uint8_t *bytes = buffer;
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = write(fd, bytes + offset, length - offset);
        if (count <= 0) return false;
        offset += (size_t)count;
    }
    return true;
}

static bool SendIPC(int fd, uint8_t type, const void *payload, uint16_t length)
{
    OpenA8DJIPCHeader header = {
        .magic = kIPCMagic,
        .version = kIPCVersion,
        .type = type,
        .length = length
    };
    return WriteFull(fd, &header, sizeof(header)) &&
           (length == 0 || WriteFull(fd, payload, length));
}

static int ConnectSocket(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strlcpy(address.sun_path, kSocketPath, sizeof(address.sun_path));
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static OSStatus WakeIOProc(AudioObjectID deviceID,
                           const AudioTimeStamp *now,
                           const AudioBufferList *inputData,
                           const AudioTimeStamp *inputTime,
                           AudioBufferList *outputData,
                           const AudioTimeStamp *outputTime,
                           void *clientData)
{
    (void)deviceID;
    (void)now;
    (void)inputData;
    (void)inputTime;
    (void)outputTime;
    (void)clientData;
    if (outputData != NULL) {
        for (UInt32 i = 0; i < outputData->mNumberBuffers; i++) {
            if (outputData->mBuffers[i].mData != NULL) {
                memset(outputData->mBuffers[i].mData, 0, outputData->mBuffers[i].mDataByteSize);
            }
        }
    }
    return noErr;
}

static AudioDeviceID FindOpenA8DJDevice(void)
{
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                     &address,
                                                     0,
                                                     NULL,
                                                     &size);
    if (status != noErr || size == 0) {
        return kAudioObjectUnknown;
    }

    UInt32 count = size / sizeof(AudioDeviceID);
    AudioDeviceID *devices = calloc(count, sizeof(AudioDeviceID));
    if (devices == NULL) {
        return kAudioObjectUnknown;
    }

    status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &address,
                                        0,
                                        NULL,
                                        &size,
                                        devices);
    if (status != noErr) {
        free(devices);
        return kAudioObjectUnknown;
    }

    AudioDeviceID result = kAudioObjectUnknown;
    for (UInt32 i = 0; i < count; i++) {
        CFStringRef uid = NULL;
        UInt32 uidSize = sizeof(uid);
        AudioObjectPropertyAddress uidAddress = {
            kAudioDevicePropertyDeviceUID,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        status = AudioObjectGetPropertyData(devices[i],
                                            &uidAddress,
                                            0,
                                            NULL,
                                            &uidSize,
                                            &uid);
        if (status == noErr && uid != NULL) {
            if (CFStringCompare(uid, CFSTR("org.opena8dj.Audio8DJ"), 0) == kCFCompareEqualTo) {
                result = devices[i];
            }
            CFRelease(uid);
        }
        if (result != kAudioObjectUnknown) {
            break;
        }
    }

    free(devices);
    return result;
}

static void StopHALWake(void)
{
    if (gWakeState.device != kAudioObjectUnknown && gWakeState.ioProcID != NULL) {
        AudioDeviceStop(gWakeState.device, gWakeState.ioProcID);
        AudioDeviceDestroyIOProcID(gWakeState.device, gWakeState.ioProcID);
    }
    gWakeState.device = kAudioObjectUnknown;
    gWakeState.ioProcID = NULL;
}

static bool StartHALWake(void)
{
    if (gWakeState.ioProcID != NULL) {
        return true;
    }

    AudioDeviceID device = FindOpenA8DJDevice();
    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "OpenA8DJ Core Audio device not found\n");
        return false;
    }

    AudioDeviceIOProcID ioProcID = NULL;
    OSStatus status = AudioDeviceCreateIOProcID(device, WakeIOProc, NULL, &ioProcID);
    if (status != noErr) {
        fprintf(stderr, "Could not wake OpenA8DJ HAL bridge: AudioDeviceCreateIOProcID failed: %d\n", (int)status);
        return false;
    }

    status = AudioDeviceStart(device, ioProcID);
    if (status != noErr) {
        fprintf(stderr, "Could not wake OpenA8DJ HAL bridge: AudioDeviceStart failed: %d\n", (int)status);
        AudioDeviceDestroyIOProcID(device, ioProcID);
        return false;
    }

    gWakeState.device = device;
    gWakeState.ioProcID = ioProcID;
    if (!gWakeCleanupRegistered) {
        atexit(StopHALWake);
        gWakeCleanupRegistered = true;
    }
    return true;
}

static bool EnvFlagEnabled(const char *name)
{
    const char *value = getenv(name);
    return value != NULL &&
        (strcmp(value, "1") == 0 ||
         strcmp(value, "true") == 0 ||
         strcmp(value, "yes") == 0 ||
         strcmp(value, "on") == 0);
}

static int ConnectSocketWithWake(bool allowWake)
{
    int fd = ConnectSocket();
    if (fd >= 0) {
        return fd;
    }
    if (!allowWake) {
        return -1;
    }

    fprintf(stderr, "OpenA8DJ HAL bridge is not available; waking the device...\n");
    if (!StartHALWake()) {
        return -1;
    }

    for (int attempt = 0; attempt < 120; attempt++) {
        fd = ConnectSocket();
        if (fd >= 0) {
            return fd;
        }
        usleep(100000);
    }
    return -1;
}

static bool ReadOneState(int fd, OpenA8DJControlPayload *state)
{
    for (int message = 0; message < 64; message++) {
        OpenA8DJIPCHeader header;
        if (!ReadFull(fd, &header, sizeof(header))) {
            return false;
        }
        if (header.magic != kIPCMagic || header.version != kIPCVersion || header.length > 4096) {
            return false;
        }
        uint8_t payload[4096];
        if (header.length > 0 && !ReadFull(fd, payload, header.length)) {
            return false;
        }
        if (header.type == kIPCTypeControlState &&
            header.length >= offsetof(OpenA8DJControlPayload, inputDecodeEnabled)) {
            memset(state, 0, sizeof(*state));
            size_t copyLength = header.length < sizeof(*state) ? header.length : sizeof(*state);
            memcpy(state, payload, copyLength);
            if (header.length < sizeof(*state)) {
                state->inputDecodeEnabled = 1;
            }
            return true;
        }
    }
    return false;
}

static bool ReadState(int fd, OpenA8DJControlPayload *state)
{
    if (!SendIPC(fd, kIPCTypeControlGet, NULL, 0)) {
        return false;
    }
    if (!ReadOneState(fd, state)) {
        return false;
    }

    for (int message = 0; message < 64; message++) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(fd, &readSet);
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 20000
        };
        int ready = select(fd + 1, &readSet, NULL, NULL, &timeout);
        if (ready <= 0 || !FD_ISSET(fd, &readSet)) {
            break;
        }
        OpenA8DJControlPayload latest;
        if (!ReadOneState(fd, &latest)) {
            return false;
        }
        *state = latest;
    }
    return true;
}

static bool ConnectAndReadState(bool allowWake, int *outFd, OpenA8DJControlPayload *state)
{
    *outFd = -1;
    for (int attempt = 0; attempt < 3; attempt++) {
        int fd = ConnectSocketWithWake(allowWake);
        if (fd < 0) {
            return false;
        }
        if (ReadState(fd, state)) {
            *outFd = fd;
            return true;
        }
        close(fd);
        usleep(150000);
    }
    return false;
}

static bool SendStateAndReadBack(int *fd, bool allowWake, OpenA8DJControlPayload *state)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        if (*fd >= 0 && SendIPC(*fd, kIPCTypeControlSet, state, sizeof(*state)) && ReadState(*fd, state)) {
            return true;
        }
        if (*fd >= 0) {
            close(*fd);
            *fd = -1;
        }
        usleep(150000);
        *fd = ConnectSocketWithWake(allowWake);
        if (*fd < 0) {
            return false;
        }
    }
    return false;
}

static bool ReadStats(int fd, OpenA8DJInputStatsPayload *stats)
{
    if (!SendIPC(fd, kIPCTypeInputStatsGet, NULL, 0)) {
        return false;
    }
    for (int message = 0; message < 64; message++) {
        OpenA8DJIPCHeader header;
        if (!ReadFull(fd, &header, sizeof(header))) {
            return false;
        }
        if (header.magic != kIPCMagic || header.version != kIPCVersion || header.length > 4096) {
            return false;
        }
        uint8_t payload[4096];
        if (header.length > 0 && !ReadFull(fd, payload, header.length)) {
            return false;
        }
        if (header.type == kIPCTypeInputStats && header.length >= sizeof(*stats)) {
            memcpy(stats, payload, sizeof(*stats));
            return true;
        }
    }
    return false;
}

static bool ReadStreamStats(int fd, OpenA8DJStreamStatsPayload *stats, size_t *payloadLength)
{
    if (!SendIPC(fd, kIPCTypeStreamStatsGet, NULL, 0)) {
        return false;
    }
    for (int message = 0; message < 64; message++) {
        OpenA8DJIPCHeader header;
        if (!ReadFull(fd, &header, sizeof(header))) {
            return false;
        }
        if (header.magic != kIPCMagic || header.version != kIPCVersion || header.length > 4096) {
            return false;
        }
        uint8_t payload[4096];
        if (header.length > 0 && !ReadFull(fd, payload, header.length)) {
            return false;
        }
        if (header.type == kIPCTypeStreamStats) {
            memset(stats, 0, sizeof(*stats));
            size_t copyLength = header.length < sizeof(*stats) ? header.length : sizeof(*stats);
            memcpy(stats, payload, copyLength);
            if (payloadLength != NULL) {
                *payloadLength = header.length;
            }
            return true;
        }
    }
    return false;
}

static bool ValidateDriverModeState(const OpenA8DJDriverModeStatePayload *state)
{
    if (state->schemaVersion != kOpenA8DJDriverModeSchemaVersion ||
        state->pending > 1 ||
        state->streaming > 1 ||
        DriverModeName(state->requestedMode) == NULL ||
        DriverModeName(state->effectiveMode) == NULL ||
        DriverModeResultName(state->lastResult) == NULL ||
        DriverModeRejectionName(state->rejectionReason) == NULL ||
        DriverModeQoSName(state->workerQoS) == NULL ||
        ((state->requestedMode != state->effectiveMode) != (state->pending != 0))) {
        return false;
    }
    if ((state->lastResult == kOpenA8DJDriverModeResultInvalid) !=
        (state->rejectionReason != kOpenA8DJDriverModeRejectionNone)) {
        return false;
    }
    OpenA8DJDriverModePolicy policy;
    return OpenA8DJDriverModeLookup(state->effectiveMode, &policy) &&
           state->outputStartLatencyFrames == policy.outputStartLatencyFrames &&
           state->outputRestartLatencyFrames == policy.outputRestartLatencyFrames &&
           state->outputTargetLatencyFrames == policy.outputTargetLatencyFrames &&
           state->workerQoS == policy.workerQoS;
}

static bool DriverModeStatesEqual(const OpenA8DJDriverModeStatePayload *left,
                                  const OpenA8DJDriverModeStatePayload *right)
{
    return left->schemaVersion == right->schemaVersion &&
           left->requestedMode == right->requestedMode &&
           left->effectiveMode == right->effectiveMode &&
           left->pending == right->pending &&
           left->streaming == right->streaming &&
           left->lastResult == right->lastResult &&
           left->rejectionReason == right->rejectionReason &&
           left->generation == right->generation &&
           left->acceptedRequests == right->acceptedRequests &&
           left->rejectedRequests == right->rejectedRequests &&
           left->appliedTransitions == right->appliedTransitions &&
           left->applyFailures == right->applyFailures &&
           left->pendingTransitions == right->pendingTransitions &&
           left->outputStartLatencyFrames == right->outputStartLatencyFrames &&
           left->outputRestartLatencyFrames == right->outputRestartLatencyFrames &&
           left->outputTargetLatencyFrames == right->outputTargetLatencyFrames &&
           left->workerQoS == right->workerQoS;
}

static bool ReadOneDriverModeState(int fd, OpenA8DJDriverModeStatePayload *state)
{
    for (int message = 0; message < 64; message++) {
        OpenA8DJIPCHeader header;
        if (!ReadFull(fd, &header, sizeof(header))) {
            return false;
        }
        if (header.magic != kIPCMagic ||
            header.version != kIPCVersion ||
            header.length > 4096) {
            return false;
        }
        uint8_t payload[4096];
        if (header.length > 0 && !ReadFull(fd, payload, header.length)) {
            return false;
        }
        if (header.type == kIPCTypeDriverModeState) {
            if (header.length < sizeof(*state)) {
                return false;
            }
            memcpy(state, payload, sizeof(*state));
            return ValidateDriverModeState(state);
        }
    }
    return false;
}

static bool ReadDriverModeState(int fd, OpenA8DJDriverModeStatePayload *state)
{
    return SendIPC(fd, kIPCTypeDriverModeGet, NULL, 0) &&
           ReadOneDriverModeState(fd, state);
}

typedef enum OpenA8DJDriverModeTransactionResult {
    kDriverModeTransactionOK = 0,
    kDriverModeTransactionProtocolError,
    kDriverModeTransactionApplyFailed,
    kDriverModeTransactionConflict
} OpenA8DJDriverModeTransactionResult;

static OpenA8DJDriverModeTransactionResult SetDriverModeAndReadBack(
    int fd,
    uint32_t requestedMode,
    OpenA8DJDriverModeStatePayload *outState)
{
    OpenA8DJDriverModeSetPayload request;
    memset(&request, 0, sizeof(request));
    request.schemaVersion = kOpenA8DJDriverModeSchemaVersion;
    request.modeID = requestedMode;
    OpenA8DJDriverModeStatePayload setState;
    OpenA8DJDriverModeStatePayload readBack;
    if (!SendIPC(fd, kIPCTypeDriverModeSet, &request, sizeof(request)) ||
        !ReadOneDriverModeState(fd, &setState) ||
        !ReadDriverModeState(fd, &readBack) ||
        !DriverModeStatesEqual(&setState, &readBack)) {
        return kDriverModeTransactionProtocolError;
    }
    *outState = readBack;
    if (readBack.lastResult == kOpenA8DJDriverModeResultInvalid &&
        readBack.rejectionReason ==
            kOpenA8DJDriverModeRejectionConflict) {
        return kDriverModeTransactionConflict;
    }
    if (readBack.lastResult == kOpenA8DJDriverModeResultApplyFailed) {
        return kDriverModeTransactionApplyFailed;
    }
    if (readBack.requestedMode != requestedMode ||
        (!readBack.pending && readBack.effectiveMode != requestedMode) ||
        (readBack.pending && readBack.effectiveMode == requestedMode)) {
        return kDriverModeTransactionProtocolError;
    }
    return kDriverModeTransactionOK;
}

static bool ValidateTimecodeState(
    const OpenA8DJTimecodeStatePayload *state)
{
    if (state == NULL ||
        state->schemaVersion != kOpenA8DJTimecodeSchemaVersion ||
        state->reserved0 != 0 ||
        state->evidenceKind != 1 ||
        state->intentObserved != 0 ||
        state->rejectionReason >
            kOpenA8DJTimecodeRejectionModeConflict ||
        !ValidateDriverModeState(&state->driverMode) ||
        state->timecode.armed > 1 ||
        state->timecode.profileVerified > 1 ||
        state->timecode.qualified > 1 ||
        state->timecode.optimizedActive > 1 ||
        state->timecode.armState > kOpenA8DJTimecodeFaulted ||
        state->timecode.lastFailOpenReason >
            kOpenA8DJTimecodeFailExplicitDisarm ||
        state->timecode.allowedInputPairMask !=
            (state->timecode.armed ?
                kOpenA8DJTimecodeAllowedPairMask : 0) ||
        state->timecode.fallbackMode <
            kOpenA8DJDriverModeBalanced ||
        state->timecode.fallbackMode >
            kOpenA8DJDriverModePerformance) {
        return false;
    }
    for (size_t index = 0; index < sizeof(state->reserved); index++) {
        if (state->reserved[index] != 0) return false;
    }
    return true;
}

static bool ReadDriverModeAndTimecode(
    int fd,
    OpenA8DJDriverModeStatePayload *driverMode,
    OpenA8DJTimecodeStatePayload *timecode,
    bool *hasTimecode)
{
    *hasTimecode = false;
    if (!SendIPC(fd, kIPCTypeDriverModeGet, NULL, 0)) return false;
    for (int message = 0; message < 64; message++) {
        OpenA8DJIPCHeader header;
        if (!ReadFull(fd, &header, sizeof(header)) ||
            header.magic != kIPCMagic ||
            header.version != kIPCVersion ||
            header.length > 4096) {
            return false;
        }
        uint8_t payload[4096];
        if (header.length > 0 &&
            !ReadFull(fd, payload, header.length)) {
            return false;
        }
        if (header.type != kIPCTypeDriverModeState) continue;
        if (header.length != sizeof(*driverMode) &&
            header.length != sizeof(*driverMode) + sizeof(*timecode)) {
            return false;
        }
        memcpy(driverMode, payload, sizeof(*driverMode));
        if (!ValidateDriverModeState(driverMode)) return false;
        if (header.length > sizeof(*driverMode)) {
            memcpy(timecode, payload + sizeof(*driverMode),
                   sizeof(*timecode));
            if (!ValidateTimecodeState(timecode) ||
                !DriverModeStatesEqual(
                    driverMode, &timecode->driverMode)) {
                return false;
            }
            *hasTimecode = true;
        }
        return true;
    }
    return false;
}

static bool ReadOneTimecodeState(
    int fd,
    OpenA8DJTimecodeStatePayload *state)
{
    for (int message = 0; message < 64; message++) {
        OpenA8DJIPCHeader header;
        if (!ReadFull(fd, &header, sizeof(header)) ||
            header.magic != kIPCMagic ||
            header.version != kIPCVersion ||
            header.length > 4096) {
            return false;
        }
        uint8_t payload[4096];
        if (header.length > 0 &&
            !ReadFull(fd, payload, header.length)) {
            return false;
        }
        if (header.type == kIPCTypeTimecodeOptimizedState) {
            if (header.length != sizeof(*state)) return false;
            memcpy(state, payload, sizeof(*state));
            return ValidateTimecodeState(state);
        }
    }
    return false;
}

static bool ReadTimecodeState(
    int fd,
    OpenA8DJTimecodeStatePayload *state)
{
    return SendIPC(fd, kIPCTypeTimecodeOptimizedGet, NULL, 0) &&
           ReadOneTimecodeState(fd, state);
}

typedef enum OpenA8DJVintageReadResult {
    kVintageReadUnavailable = 0,
    kVintageReadValid,
    kVintageReadInvalid
} OpenA8DJVintageReadResult;

static bool ValidateVintageState(
    const OpenA8DJVintageStatePayload *state)
{
    return OpenA8DJVintageValidateStatePayload(state) &&
           ValidateDriverModeState(&state->driverMode);
}

static OpenA8DJVintageReadResult ReadOneVintageState(
    int fd,
    OpenA8DJVintageStatePayload *state)
{
    for (int message = 0; message < 64; message++) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(fd, &readSet);
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 100000
        };
        int ready = select(
            fd + 1, &readSet, NULL, NULL, &timeout);
        if (ready == 0) {
            return kVintageReadUnavailable;
        }
        if (ready < 0 || !FD_ISSET(fd, &readSet)) {
            return kVintageReadInvalid;
        }
        OpenA8DJIPCHeader header;
        if (!ReadFull(fd, &header, sizeof(header))) {
            return kVintageReadUnavailable;
        }
        if (header.magic != kIPCMagic ||
            header.version != kIPCVersion ||
            header.length > 4096) {
            return kVintageReadInvalid;
        }
        uint8_t payload[4096];
        if (header.length > 0 &&
            !ReadFull(fd, payload, header.length)) {
            return kVintageReadInvalid;
        }
        if (header.type == kIPCTypeVintageCompatibleState) {
            if (header.length != sizeof(*state)) {
                return kVintageReadInvalid;
            }
            memcpy(state, payload, sizeof(*state));
            return ValidateVintageState(state) ?
                kVintageReadValid : kVintageReadInvalid;
        }
    }
    return kVintageReadUnavailable;
}

static OpenA8DJVintageReadResult ReadVintageState(
    int fd,
    OpenA8DJVintageStatePayload *state)
{
    if (!SendIPC(fd, kIPCTypeVintageCompatibleGet, NULL, 0)) {
        return kVintageReadUnavailable;
    }
    return ReadOneVintageState(fd, state);
}

static bool TimecodeStatesEqual(
    const OpenA8DJTimecodeStatePayload *left,
    const OpenA8DJTimecodeStatePayload *right)
{
    OpenA8DJTimecodeState leftState;
    OpenA8DJTimecodeState rightState;
    memcpy(&leftState, &left->timecode, sizeof(leftState));
    memcpy(&rightState, &right->timecode, sizeof(rightState));
    return left->schemaVersion == right->schemaVersion &&
           left->evidenceKind == right->evidenceKind &&
           left->intentObserved == right->intentObserved &&
           left->rejectionReason == kOpenA8DJTimecodeRejectionNone &&
           right->rejectionReason == kOpenA8DJTimecodeRejectionNone &&
           leftState.armed == rightState.armed &&
           leftState.allowedInputPairMask ==
               rightState.allowedInputPairMask &&
           leftState.fallbackMode == rightState.fallbackMode &&
           leftState.electricalProfile ==
               rightState.electricalProfile &&
           leftState.profileVerified ==
               rightState.profileVerified &&
           leftState.sampleRateSnapshot ==
               rightState.sampleRateSnapshot &&
           leftState.bufferFramesSnapshot ==
               rightState.bufferFramesSnapshot &&
           rightState.generation >= leftState.generation &&
           rightState.counters.arms >= leftState.counters.arms &&
           rightState.counters.disarms >= leftState.counters.disarms &&
           rightState.counters.qualifications >=
               leftState.counters.qualifications &&
           rightState.counters.activations >=
               leftState.counters.activations &&
           rightState.counters.deoptimizations >=
               leftState.counters.deoptimizations;
}

static bool MutateTimecodeAndReadBack(
    int fd,
    bool arm,
    OpenA8DJTimecodeStatePayload *outState)
{
    OpenA8DJTimecodeArmPayload request;
    memset(&request, 0, sizeof(request));
    request.schemaVersion = kOpenA8DJTimecodeSchemaVersion;
    request.modeID = kOpenA8DJDriverModeTimecodeOptimized;
    request.allowedInputPairMask =
        kOpenA8DJTimecodeAllowedPairMask;
    OpenA8DJTimecodeStatePayload mutation;
    OpenA8DJTimecodeStatePayload readBack;
    if (!SendIPC(fd,
                 arm ? kIPCTypeTimecodeOptimizedArm :
                       kIPCTypeTimecodeOptimizedDisarm,
                 arm ? (const void *)&request : NULL,
                 arm ? sizeof(request) : 0) ||
        !ReadOneTimecodeState(fd, &mutation) ||
        !ReadTimecodeState(fd, &readBack) ||
        (mutation.rejectionReason !=
                 kOpenA8DJTimecodeRejectionModeConflict &&
         !TimecodeStatesEqual(&mutation, &readBack))) {
        return false;
    }
    if (mutation.rejectionReason ==
        kOpenA8DJTimecodeRejectionModeConflict) {
        OpenA8DJTimecodeStatePayload comparable = mutation;
        comparable.rejectionReason =
            kOpenA8DJTimecodeRejectionNone;
        if (!TimecodeStatesEqual(&comparable, &readBack)) {
            return false;
        }
        *outState = mutation;
        return true;
    }
    *outState = readBack;
    return true;
}

static void PrintState(const OpenA8DJControlPayload *state)
{
    printf("Audio 8 DJ controls\n");
    printf("  input-mode:        %u (%s)\n", state->inputMode, InputModeName(state->inputMode));
    printf("  gnd-vinyl:         %s\n", state->gndLiftTCVinyl ? "on" : "off");
    printf("  gnd-cd-line:       %s\n", state->gndLiftTCCDLine ? "on" : "off");
    printf("  gnd-phono:         %s\n", state->gndLiftPhono ? "on" : "off");
    printf("  software-lock:     %s\n", state->softwareLock ? "on" : "off");
    printf("  input-decode:      %s\n", state->inputDecodeEnabled ? "on" : "off");
    printf("  input-transform:   A=%s B=%s C=%s D=%s\n",
           InputTransformName(InputTransformForPair(state, 0)),
           InputTransformName(InputTransformForPair(state, 1)),
           InputTransformName(InputTransformForPair(state, 2)),
           InputTransformName(InputTransformForPair(state, 3)));
    printf("  input-source:      A=%c B=%c C=%c D=%c\n",
           'A' + (state->inputSource[0] < kInputPairs ? state->inputSource[0] : 0),
           'A' + (state->inputSource[1] < kInputPairs ? state->inputSource[1] : 1),
           'A' + (state->inputSource[2] < kInputPairs ? state->inputSource[2] : 2),
           'A' + (state->inputSource[3] < kInputPairs ? state->inputSource[3] : 3));
}

static void PrintInputStats(const OpenA8DJInputStatsPayload *stats)
{
    printf("Audio 8 DJ input stats since last read\n");
    for (int i = 0; i < kInputPairs; i++) {
        double rmsL = 0.0;
        double rmsR = 0.0;
        double corr = 0.0;
        if (stats->frames[i] > 0) {
            rmsL = sqrt(stats->leftSquare[i] / (double)stats->frames[i]);
            rmsR = sqrt(stats->rightSquare[i] / (double)stats->frames[i]);
            double denom = sqrt(stats->leftSquare[i] * stats->rightSquare[i]);
            if (denom > 0.0) {
                corr = stats->cross[i] / denom;
            }
        }
        printf("  Input %c: frames=%llu rmsL=%.8f rmsR=%.8f peakL=%.8f peakR=%.8f corr=%.4f\n",
               'A' + i,
               (unsigned long long)stats->frames[i],
               rmsL,
               rmsR,
               stats->leftPeak[i],
               stats->rightPeak[i],
               corr);
    }
}

static bool StreamStatsHasField(size_t payloadLength, size_t offset, size_t fieldSize)
{
    return offset <= payloadLength && fieldSize <= payloadLength - offset;
}

#define STREAM_STATS_HAS_FIELD(payloadLength, field) \
    StreamStatsHasField((payloadLength), offsetof(OpenA8DJStreamStatsPayload, field), sizeof(((OpenA8DJStreamStatsPayload *)0)->field))

static void PrintStreamStats(const OpenA8DJStreamStatsPayload *stats, size_t payloadLength)
{
    printf("OpenA8DJ stream stats\n");
    printf("  streaming:              %s\n", stats->streaming ? "yes" : "no");
    printf("  sample-rate:            %.0f\n", stats->sampleRate);
    printf("  output-ring:            %u / target %u frames\n",
           stats->outputRingFrames,
           stats->outputTargetLatencyFrames);
    printf("  output-byte-position:   %u\n", stats->outputByteInFrame);
    printf("  playback-lead:          %u frames\n", stats->playbackLeadFrames);
    printf("  playback-queue:         in-flight=%u / target %u next-frame=%llu\n",
           stats->playbackTransfersInFlight,
           stats->playbackQueueTarget,
           (unsigned long long)stats->playbackNextFrameNumber);
    printf("  clock-anchor:           %s sample=%.0f host=%llu seed=%llu accepted=%llu rejected=%llu resets=%llu\n",
           stats->clockAnchorValid ? "valid" : "fallback",
           stats->clockSampleTime,
           (unsigned long long)stats->clockHostTime,
           (unsigned long long)stats->clockSeed,
           (unsigned long long)stats->clockAcceptedAnchors,
           (unsigned long long)stats->clockRejectedAnchors,
           (unsigned long long)stats->clockAnchorResets);
    printf("  usb-frame-clock:        frame=%llu host=%llu ticks/frame=%.3f samples=%llu resyncs=%llu\n",
           (unsigned long long)stats->clockUSBFrameNumber,
           (unsigned long long)stats->clockUSBFrameHostTime,
           stats->clockHostTicksPerUSBFrame,
           (unsigned long long)stats->clockUSBFrameSamples,
           (unsigned long long)stats->clockUSBFrameResyncs);
    printf("  capture:                transfers=%llu tx=%llu bytes=%llu failed=%llu short=%llu filtered=%llu qfail=%llu\n",
           (unsigned long long)stats->captureTransfers,
           (unsigned long long)stats->captureTransactions,
           (unsigned long long)stats->captureBytes,
           (unsigned long long)stats->captureTransactionFailures,
           (unsigned long long)stats->captureShortTransfers,
           (unsigned long long)stats->filteredCaptureTransactions,
           (unsigned long long)stats->captureQueueFailures);
    printf("  capture-detail:         status-failed=%llu zero-complete=%llu expected=%llu other-size=%llu\n",
           (unsigned long long)stats->captureStatusFailures,
           (unsigned long long)stats->captureZeroCompleteTransactions,
           (unsigned long long)stats->captureExpectedTransactions,
           (unsigned long long)stats->captureOtherByteCountTransactions);
    printf("  playback:               transfers=%llu tx=%llu bytes=%llu failed=%llu short=%llu qfail=%llu\n",
           (unsigned long long)stats->playbackTransfers,
           (unsigned long long)stats->playbackTransactions,
           (unsigned long long)stats->playbackBytes,
           (unsigned long long)stats->playbackTransactionFailures,
           (unsigned long long)stats->playbackShortTransfers,
           (unsigned long long)stats->playbackQueueFailures);
    printf("  output:                 written=%llu read=%llu underruns=%llu active-underruns=%llu startup-silence=%llu overruns=%llu elastic-drops=%llu elastic-replays=%llu timeline-resets=%llu late-write-frames=%llu late-write-batches=%llu\n",
           (unsigned long long)stats->outputFramesWritten,
           (unsigned long long)stats->outputFramesRead,
           (unsigned long long)stats->outputUnderruns,
           (unsigned long long)stats->outputActiveUnderruns,
           (unsigned long long)stats->outputStartupSilenceFrames,
           (unsigned long long)stats->outputRingOverruns,
           (unsigned long long)stats->outputElasticDrops,
           (unsigned long long)stats->outputElasticReplays,
           (unsigned long long)stats->outputTimelineResets,
           (unsigned long long)(STREAM_STATS_HAS_FIELD(payloadLength, outputLateWriteBatches) ? stats->outputLateWriteFrames : 0),
           (unsigned long long)(STREAM_STATS_HAS_FIELD(payloadLength, outputLateWriteBatches) ? stats->outputLateWriteBatches : 0));
    printf("  output-level:           peak=%.6f near-clip=%llu clipped=%llu\n",
           stats->outputPeak,
           (unsigned long long)stats->outputNearClipSamples,
           (unsigned long long)stats->outputClippedSamples);
    printf("  scheduling:             explicit=%s reschedules=%llu resets=%llu too-old=%llu too-new=%llu out-of-window=%llu fallbacks=%llu\n",
           stats->playbackExplicitScheduling ? "on" : "off",
           (unsigned long long)stats->playbackReschedules,
           (unsigned long long)stats->playbackScheduleResets,
           (unsigned long long)stats->playbackScheduleTooOld,
           (unsigned long long)stats->playbackScheduleTooNew,
           (unsigned long long)stats->playbackScheduleOutOfWindow,
           (unsigned long long)stats->playbackScheduleFallbacks);
    double captureDeltaAvg = stats->captureCompletionDeltaSamples > 0 ?
        (double)stats->captureCompletionDeltaSum / (double)stats->captureCompletionDeltaSamples : 0.0;
    double playbackDeltaAvg = stats->playbackCompletionDeltaSamples > 0 ?
        (double)stats->playbackCompletionDeltaSum / (double)stats->playbackCompletionDeltaSamples : 0.0;
    double captureToPlaybackAvg = stats->captureToPlaybackQueueDeltaSamples > 0 ?
        (double)stats->captureToPlaybackQueueDeltaSum / (double)stats->captureToPlaybackQueueDeltaSamples : 0.0;
    printf("  timing-ticks:           capture-delta min=%llu max=%llu avg=%.1f samples=%llu playback-delta min=%llu max=%llu avg=%.1f samples=%llu cap-to-play min=%llu max=%llu avg=%.1f samples=%llu\n",
           (unsigned long long)stats->captureCompletionDeltaMin,
           (unsigned long long)stats->captureCompletionDeltaMax,
           captureDeltaAvg,
           (unsigned long long)stats->captureCompletionDeltaSamples,
           (unsigned long long)stats->playbackCompletionDeltaMin,
           (unsigned long long)stats->playbackCompletionDeltaMax,
           playbackDeltaAvg,
           (unsigned long long)stats->playbackCompletionDeltaSamples,
           (unsigned long long)stats->captureToPlaybackQueueDeltaMin,
           (unsigned long long)stats->captureToPlaybackQueueDeltaMax,
           captureToPlaybackAvg,
           (unsigned long long)stats->captureToPlaybackQueueDeltaSamples);
    printf("  mode2:                  input-check-errors=%llu output-panic-flags=%llu\n",
           (unsigned long long)stats->inputCheckErrors,
           (unsigned long long)stats->outputPanicFlags);
    if (STREAM_STATS_HAS_FIELD(payloadLength, playbackLayoutSignatureSum)) {
        double captureOutlierAvg = stats->captureCompletionDeltaOutliers > 0 ?
            (double)stats->captureCompletionDeltaOutlierSum / (double)stats->captureCompletionDeltaOutliers : 0.0;
        double playbackOutlierAvg = stats->playbackCompletionDeltaOutliers > 0 ?
            (double)stats->playbackCompletionDeltaOutlierSum / (double)stats->playbackCompletionDeltaOutliers : 0.0;
        double captureToPlaybackOutlierAvg = stats->captureToPlaybackQueueDeltaOutliers > 0 ?
            (double)stats->captureToPlaybackQueueDeltaOutlierSum / (double)stats->captureToPlaybackQueueDeltaOutliers : 0.0;
        printf("  cadence-outliers:       expected-transfer=%llu capture count=%llu max=%llu avg=%.1f playback count=%llu max=%llu avg=%.1f cap-to-play count=%llu max=%llu avg=%.1f\n",
               (unsigned long long)stats->cadenceExpectedTransferTicks,
               (unsigned long long)stats->captureCompletionDeltaOutliers,
               (unsigned long long)stats->captureCompletionDeltaOutlierMax,
               captureOutlierAvg,
               (unsigned long long)stats->playbackCompletionDeltaOutliers,
               (unsigned long long)stats->playbackCompletionDeltaOutlierMax,
               playbackOutlierAvg,
               (unsigned long long)stats->captureToPlaybackQueueDeltaOutliers,
               (unsigned long long)stats->captureToPlaybackQueueDeltaOutlierMax,
               captureToPlaybackOutlierAvg);
        double usbTimestampAvg = stats->captureUSBTimestampDeltaSamples > 0 ?
            (double)stats->captureUSBTimestampDeltaSum / (double)stats->captureUSBTimestampDeltaSamples : 0.0;
        double captureRequestAvg = stats->captureRequestCountSamples > 0 ?
            (double)stats->captureRequestCountSum / (double)stats->captureRequestCountSamples : 0.0;
        double captureCompleteAvg = stats->captureCompleteCountSamples > 0 ?
            (double)stats->captureCompleteCountSum / (double)stats->captureCompleteCountSamples : 0.0;
        double captureOffsetAvg = stats->captureOffsetSamples > 0 ?
            (double)stats->captureOffsetSum / (double)stats->captureOffsetSamples : 0.0;
        printf("  cadence-capture:        usb-ts min=%llu max=%llu avg=%.1f samples=%llu zero=%llu repeated=%llu out-of-order=%llu request min=%llu max=%llu avg=%.1f complete min=%llu max=%llu avg=%.1f offset min=%llu max=%llu avg=%.1f layout-sum=%llu\n",
               (unsigned long long)stats->captureUSBTimestampDeltaMin,
               (unsigned long long)stats->captureUSBTimestampDeltaMax,
               usbTimestampAvg,
               (unsigned long long)stats->captureUSBTimestampDeltaSamples,
               (unsigned long long)stats->captureUSBTimestampZero,
               (unsigned long long)stats->captureUSBTimestampRepeated,
               (unsigned long long)stats->captureUSBTimestampOutOfOrder,
               (unsigned long long)stats->captureRequestCountMin,
               (unsigned long long)stats->captureRequestCountMax,
               captureRequestAvg,
               (unsigned long long)stats->captureCompleteCountMin,
               (unsigned long long)stats->captureCompleteCountMax,
               captureCompleteAvg,
               (unsigned long long)stats->captureOffsetMin,
               (unsigned long long)stats->captureOffsetMax,
               captureOffsetAvg,
               (unsigned long long)stats->captureLayoutSignatureSum);
        double queueBytesAvg = stats->playbackQueueBytesSamples > 0 ?
            (double)stats->playbackQueueBytesSum / (double)stats->playbackQueueBytesSamples : 0.0;
        double queueTransactionsAvg = stats->playbackQueueTransactionsSamples > 0 ?
            (double)stats->playbackQueueTransactionsSum / (double)stats->playbackQueueTransactionsSamples : 0.0;
        double queueRequestAvg = stats->playbackQueueRequestCountSamples > 0 ?
            (double)stats->playbackQueueRequestCountSum / (double)stats->playbackQueueRequestCountSamples : 0.0;
        double inFlightQueueAvg = stats->playbackInFlightAtQueueSamples > 0 ?
            (double)stats->playbackInFlightAtQueueSum / (double)stats->playbackInFlightAtQueueSamples : 0.0;
        printf("  cadence-playback-queue: attempts=%llu bytes min=%llu max=%llu avg=%.1f tx min=%llu max=%llu avg=%.1f request min=%llu max=%llu avg=%.1f inflight min=%llu max=%llu avg=%.1f layout-sum=%llu\n",
               (unsigned long long)stats->playbackQueueAttempts,
               (unsigned long long)stats->playbackQueueBytesMin,
               (unsigned long long)stats->playbackQueueBytesMax,
               queueBytesAvg,
               (unsigned long long)stats->playbackQueueTransactionsMin,
               (unsigned long long)stats->playbackQueueTransactionsMax,
               queueTransactionsAvg,
               (unsigned long long)stats->playbackQueueRequestCountMin,
               (unsigned long long)stats->playbackQueueRequestCountMax,
               queueRequestAvg,
               (unsigned long long)stats->playbackInFlightAtQueueMin,
               (unsigned long long)stats->playbackInFlightAtQueueMax,
               inFlightQueueAvg,
               (unsigned long long)stats->playbackQueueLayoutSignatureSum);
        double inFlightCompletionAvg = stats->playbackInFlightAtCompletionSamples > 0 ?
            (double)stats->playbackInFlightAtCompletionSum / (double)stats->playbackInFlightAtCompletionSamples : 0.0;
        double playbackRequestAvg = stats->playbackRequestCountSamples > 0 ?
            (double)stats->playbackRequestCountSum / (double)stats->playbackRequestCountSamples : 0.0;
        double playbackCompleteAvg = stats->playbackCompleteCountSamples > 0 ?
            (double)stats->playbackCompleteCountSum / (double)stats->playbackCompleteCountSamples : 0.0;
        printf("  cadence-playback-complete: inflight min=%llu max=%llu avg=%.1f request min=%llu max=%llu avg=%.1f complete min=%llu max=%llu avg=%.1f zero-complete=%llu layout-sum=%llu\n",
               (unsigned long long)stats->playbackInFlightAtCompletionMin,
               (unsigned long long)stats->playbackInFlightAtCompletionMax,
               inFlightCompletionAvg,
               (unsigned long long)stats->playbackRequestCountMin,
               (unsigned long long)stats->playbackRequestCountMax,
               playbackRequestAvg,
               (unsigned long long)stats->playbackCompleteCountMin,
               (unsigned long long)stats->playbackCompleteCountMax,
               playbackCompleteAvg,
               (unsigned long long)stats->playbackZeroCompleteTransactions,
               (unsigned long long)stats->playbackLayoutSignatureSum);
    }

    uint64_t playbackScheduleErrors = stats->playbackScheduleTooOld +
        stats->playbackScheduleTooNew +
        stats->playbackScheduleOutOfWindow +
        stats->playbackScheduleFallbacks;
    printf("streaming=%u\n", stats->streaming);
    printf("captureTransfersCompleted=%llu\n", (unsigned long long)stats->captureTransfers);
    printf("captureTransactionErrors=%llu\n", (unsigned long long)stats->captureTransactionFailures);
    printf("playbackTransfersSubmitted=%llu\n",
           (unsigned long long)(STREAM_STATS_HAS_FIELD(payloadLength, playbackQueueAttempts) ?
                                stats->playbackQueueAttempts : stats->playbackTransfers));
    printf("playbackTransfersCompleted=%llu\n", (unsigned long long)stats->playbackTransfers);
    printf("playbackTransferErrors=%llu\n", (unsigned long long)stats->playbackTransactionFailures);
    printf("playbackScheduleErrors=%llu\n", (unsigned long long)playbackScheduleErrors);
    printf("playbackReschedules=%llu\n", (unsigned long long)stats->playbackReschedules);
    printf("outputRingFrames=%u\n", stats->outputRingFrames);
    printf("outputFramesWritten=%llu\n", (unsigned long long)stats->outputFramesWritten);
    printf("outputFramesRead=%llu\n", (unsigned long long)stats->outputFramesRead);
    printf("outputUnderruns=%llu\n", (unsigned long long)stats->outputUnderruns);
    printf("outputActiveUnderruns=%llu\n", (unsigned long long)stats->outputActiveUnderruns);
    printf("outputElasticDrops=%llu\n", (unsigned long long)stats->outputElasticDrops);
    printf("outputElasticReplays=%llu\n", (unsigned long long)stats->outputElasticReplays);
    printf("outputTimelineResets=%llu\n", (unsigned long long)stats->outputTimelineResets);
    printf("outputLateWriteFrames=%llu\n",
           (unsigned long long)(STREAM_STATS_HAS_FIELD(payloadLength, outputLateWriteBatches) ? stats->outputLateWriteFrames : 0));
    printf("outputLateWriteBatches=%llu\n",
           (unsigned long long)(STREAM_STATS_HAS_FIELD(payloadLength, outputLateWriteBatches) ? stats->outputLateWriteBatches : 0));
    printf("outputPanicFlags=%llu\n", (unsigned long long)stats->outputPanicFlags);
    printf("clockAnchorValid=%u\n", stats->clockAnchorValid);
    printf("clockAcceptedAnchors=%llu\n", (unsigned long long)stats->clockAcceptedAnchors);
    printf("clockRejectedAnchors=%llu\n", (unsigned long long)stats->clockRejectedAnchors);
    printf("clockAnchorResets=%llu\n", (unsigned long long)stats->clockAnchorResets);
    if (STREAM_STATS_HAS_FIELD(payloadLength, playbackLayoutSignatureSum)) {
        printf("cadenceExpectedTransferTicks=%llu\n", (unsigned long long)stats->cadenceExpectedTransferTicks);
        printf("captureCompletionDeltaOutliers=%llu\n", (unsigned long long)stats->captureCompletionDeltaOutliers);
        printf("playbackCompletionDeltaOutliers=%llu\n", (unsigned long long)stats->playbackCompletionDeltaOutliers);
        printf("captureToPlaybackQueueDeltaOutliers=%llu\n", (unsigned long long)stats->captureToPlaybackQueueDeltaOutliers);
        printf("captureUSBTimestampOutOfOrder=%llu\n", (unsigned long long)stats->captureUSBTimestampOutOfOrder);
        printf("captureUSBTimestampRepeated=%llu\n", (unsigned long long)stats->captureUSBTimestampRepeated);
        printf("captureUSBTimestampZero=%llu\n", (unsigned long long)stats->captureUSBTimestampZero);
        printf("playbackZeroCompleteTransactions=%llu\n", (unsigned long long)stats->playbackZeroCompleteTransactions);
        printf("playbackQueueAttempts=%llu\n", (unsigned long long)stats->playbackQueueAttempts);
        printf("playbackInFlightAtQueueMax=%llu\n", (unsigned long long)stats->playbackInFlightAtQueueMax);
        printf("playbackInFlightAtCompletionMax=%llu\n", (unsigned long long)stats->playbackInFlightAtCompletionMax);
        printf("captureLayoutSignatureSum=%llu\n", (unsigned long long)stats->captureLayoutSignatureSum);
        printf("playbackQueueLayoutSignatureSum=%llu\n", (unsigned long long)stats->playbackQueueLayoutSignatureSum);
        printf("playbackLayoutSignatureSum=%llu\n", (unsigned long long)stats->playbackLayoutSignatureSum);
    }
}

#undef STREAM_STATS_HAS_FIELD

typedef enum OpenA8DJPublicBackendResult {
    kPublicBackendOK = 0,
    kPublicBackendUnavailable,
    kPublicBackendPermissionDenied
} OpenA8DJPublicBackendResult;

static void PrintJSONString(FILE *out, const char *value)
{
    static const char hex[] = "0123456789abcdef";
    const unsigned char *cursor = (const unsigned char *)value;
    fputc('"', out);
    while (*cursor != '\0') {
        unsigned char byte = *cursor++;
        switch (byte) {
            case '"':
                fputs("\\\"", out);
                break;
            case '\\':
                fputs("\\\\", out);
                break;
            case '\b':
                fputs("\\b", out);
                break;
            case '\f':
                fputs("\\f", out);
                break;
            case '\n':
                fputs("\\n", out);
                break;
            case '\r':
                fputs("\\r", out);
                break;
            case '\t':
                fputs("\\t", out);
                break;
            default:
                if (byte < 0x20) {
                    fputs("\\u00", out);
                    fputc(hex[byte >> 4], out);
                    fputc(hex[byte & 0x0f], out);
                } else {
                    fputc(byte, out);
                }
                break;
        }
    }
    fputc('"', out);
}

static void PrintPublicEnvelopePrefix(const char *operation, bool ok)
{
    fputs("{\"schema\":", stdout);
    PrintJSONString(stdout, kPublicAPISchema);
    fputs(",\"apiVersion\":", stdout);
    PrintJSONString(stdout, kPublicAPIVersion);
    fprintf(stdout, ",\"ok\":%s,\"operation\":", ok ? "true" : "false");
    PrintJSONString(stdout, operation);
}

static int PrintPublicError(const char *operation,
                            const char *code,
                            const char *message,
                            bool retryable,
                            int exitStatus)
{
    PrintPublicEnvelopePrefix(operation, false);
    fputs(",\"error\":{\"code\":", stdout);
    PrintJSONString(stdout, code);
    fputs(",\"message\":", stdout);
    PrintJSONString(stdout, message);
    fprintf(stdout, ",\"retryable\":%s}}\n", retryable ? "true" : "false");
    return exitStatus;
}

static const OpenA8DJPreset *FindCanonicalPreset(const char *name)
{
    for (size_t i = 0; i < kBuiltInPresetCount; i++) {
        if (strcmp(name, kBuiltInPresets[i].name) == 0) {
            return &kBuiltInPresets[i];
        }
    }
    return NULL;
}

static bool ApplyCanonicalPreset(const char *name, OpenA8DJControlPayload *state)
{
    return FindCanonicalPreset(name) != NULL && ApplyPreset(name, state);
}

static void PrintPublicStateMembers(const OpenA8DJControlPayload *state)
{
    fputs("\"activeProfile\":", stdout);
    PrintJSONString(stdout, InferredPresetName(state));
    fputs(",\"inputMode\":", stdout);
    PrintJSONString(stdout, InputModeName(state->inputMode));
    fprintf(stdout,
            ",\"inputModeValue\":%u"
            ",\"inputDecode\":%s"
            ",\"softwareLock\":%s"
            ",\"groundLiftVinyl\":%s"
            ",\"groundLiftCDLine\":%s"
            ",\"groundLiftPhono\":%s",
            state->inputMode,
            BoolJSON(state->inputDecodeEnabled),
            BoolJSON(state->softwareLock),
            BoolJSON(state->gndLiftTCVinyl),
            BoolJSON(state->gndLiftTCCDLine),
            BoolJSON(state->gndLiftPhono));
    fputs(",\"inputSources\":{", stdout);
    for (int pair = 0; pair < kInputPairs; pair++) {
        if (pair != 0) fputc(',', stdout);
        fprintf(stdout, "\"%c\":\"%c\"", 'A' + pair, PairLetter(state->inputSource[pair], pair));
    }
    fputs("},\"inputTransforms\":{", stdout);
    for (int pair = 0; pair < kInputPairs; pair++) {
        if (pair != 0) fputc(',', stdout);
        fprintf(stdout, "\"%c\":", 'A' + pair);
        PrintJSONString(stdout, InputTransformName(InputTransformForPair(state, pair)));
    }
    fputc('}', stdout);
}

static void PrintPublicVersion(void)
{
    PrintPublicEnvelopePrefix("version.get", true);
    fputs(",\"data\":{\"apiVersion\":", stdout);
    PrintJSONString(stdout, kPublicAPIVersion);
    fputs(",\"schema\":", stdout);
    PrintJSONString(stdout, kPublicAPISchema);
    fputs(",\"transport\":\"process-json\",\"privateIPCVersion\":1,"
          "\"capabilities\":[\"stats.read\",\"usb-quality.read\",\"hardware.read\",\"profiles.list\","
          "\"profile.read\",\"profile.write\",\"driver-mode.read\",\"driver-mode.write\","
          "\"timecode-optimized.read\",\"timecode-optimized.arm\","
          "\"driver-mode.vintage-compatible.read\","
          "\"driver-mode.vintage-compatible.write\"]}}\n",
          stdout);
}

static void PrintPublicNullableDeviceField(size_t payloadLength,
                                           size_t offset,
                                           uint64_t value,
                                           bool available)
{
    if (!available || !StreamStatsHasField(payloadLength, offset, sizeof(value))) {
        fputs("null", stdout);
        return;
    }
    fprintf(stdout, "%llu", (unsigned long long)value);
}

static void PrintPublicHardware(const OpenA8DJStreamStatsPayload *stats,
                                size_t payloadLength)
{
    bool hasTail = StreamStatsHasField(
        payloadLength,
        offsetof(OpenA8DJStreamStatsPayload, deviceDataAlignment),
        sizeof(stats->deviceDataAlignment));
    bool available = hasTail && stats->deviceInfoAvailable == 1;
    PrintPublicEnvelopePrefix("hardware.get", true);
    fprintf(stdout, ",\"data\":{\"deviceInfoAvailable\":%s,\"firmwareVersion\":",
            available ? "true" : "false");
    PrintPublicNullableDeviceField(payloadLength,
                                   offsetof(OpenA8DJStreamStatsPayload, deviceFirmwareVersion),
                                   stats->deviceFirmwareVersion, available);
    fputs(",\"hardwareSubtype\":", stdout);
    PrintPublicNullableDeviceField(payloadLength,
                                   offsetof(OpenA8DJStreamStatsPayload, deviceHardwareSubtype),
                                   stats->deviceHardwareSubtype, available);
    fputs(",\"capabilities\":{\"analogAudioOutputs\":", stdout);
    PrintPublicNullableDeviceField(payloadLength,
                                   offsetof(OpenA8DJStreamStatsPayload, deviceNumAnalogAudioOut),
                                   stats->deviceNumAnalogAudioOut, available);
    fputs(",\"analogAudioInputs\":", stdout);
    PrintPublicNullableDeviceField(payloadLength,
                                   offsetof(OpenA8DJStreamStatsPayload, deviceNumAnalogAudioIn),
                                   stats->deviceNumAnalogAudioIn, available);
    fputs(",\"digitalAudioOutputs\":", stdout);
    PrintPublicNullableDeviceField(payloadLength,
                                   offsetof(OpenA8DJStreamStatsPayload, deviceNumDigitalAudioOut),
                                   stats->deviceNumDigitalAudioOut, available);
    fputs(",\"digitalAudioInputs\":", stdout);
    PrintPublicNullableDeviceField(payloadLength,
                                   offsetof(OpenA8DJStreamStatsPayload, deviceNumDigitalAudioIn),
                                   stats->deviceNumDigitalAudioIn, available);
    fputs(",\"midiOutputs\":", stdout);
    PrintPublicNullableDeviceField(payloadLength,
                                   offsetof(OpenA8DJStreamStatsPayload, deviceNumMidiOut),
                                   stats->deviceNumMidiOut, available);
    fputs(",\"midiInputs\":", stdout);
    PrintPublicNullableDeviceField(payloadLength,
                                   offsetof(OpenA8DJStreamStatsPayload, deviceNumMidiIn),
                                   stats->deviceNumMidiIn, available);
    fputs(",\"dataAlignment\":", stdout);
    PrintPublicNullableDeviceField(payloadLength,
                                   offsetof(OpenA8DJStreamStatsPayload, deviceDataAlignment),
                                   stats->deviceDataAlignment, available);
    fputs("}}}\n", stdout);
}

static void PrintPublicProfiles(void)
{
    PrintPublicEnvelopePrefix("profiles.list", true);
    fputs(",\"data\":{\"profiles\":[", stdout);
    for (size_t i = 0; i < kBuiltInPresetCount; i++) {
        const OpenA8DJPreset *preset = &kBuiltInPresets[i];
        if (i != 0) fputc(',', stdout);
        fputs("{\"id\":", stdout);
        PrintJSONString(stdout, preset->name);
        fputs(",\"title\":", stdout);
        PrintJSONString(stdout, preset->title);
        fputs(",\"surface\":", stdout);
        PrintJSONString(stdout, preset->surface);
        fputs(",\"summary\":", stdout);
        PrintJSONString(stdout, preset->summary);
        fputc('}', stdout);
    }
    fputs("]}}\n", stdout);
}

static void PrintPublicDriverModes(void)
{
    PrintPublicEnvelopePrefix("driver_modes.list", true);
    fputs(",\"data\":{\"schemaVersion\":1,\"driverModes\":["
          "{\"id\":\"balanced\",\"name\":\"Balanced\",\"default\":true,"
          "\"requiresIdleBoundary\":true},"
          "{\"id\":\"performance\",\"name\":\"Performance\",\"default\":false,"
          "\"requiresIdleBoundary\":true},"
          "{\"id\":\"timecode-optimized\",\"name\":\"Timecode Optimized\","
          "\"default\":false,\"requiresArm\":true,"
          "\"requiresIdleBoundary\":true},"
          "{\"id\":\"vintage-compatible\","
          "\"name\":\"Vintage Compatible (Experimental — Unverified)\","
          "\"default\":false,\"experimental\":true,"
          "\"conformanceVerified\":false,"
          "\"requiresIdleBoundary\":true}]}}\n",
          stdout);
}

static const char *TimecodeArmStateName(uint8_t state)
{
    static const char *names[] = {
        "disarmed", "waiting_profile", "qualifying",
        "qualified_pending_boundary", "active",
        "deopt_pending_boundary", "faulted"
    };
    return state < sizeof(names) / sizeof(names[0]) ?
        names[state] : "unknown";
}

static const char *TimecodeFailReasonName(uint8_t reason)
{
    static const char *names[] = {
        "none", "outside_allowlist", "allowed_pair_dropout",
        "stats_missing", "stats_invalid", "wrong_profile",
        "configuration_changed", "xrun_or_transport_error",
        "input_lead_violation", "apply_failed", "explicit_disarm"
    };
    return reason < sizeof(names) / sizeof(names[0]) ?
        names[reason] : "unknown";
}

static const char *TimecodeProfileName(uint8_t profile)
{
    switch (profile) {
        case kOpenA8DJTimecodeProfileVinyl:
            return "traktor-dvs-vinyl";
        case kOpenA8DJTimecodeProfileCDLine:
            return "traktor-dvs-cd-line";
        case kOpenA8DJTimecodeProfilePhono:
            return "vinyl-recording";
        default:
            return "unavailable";
    }
}

static void PrintPublicTimecodeMembers(
    const OpenA8DJTimecodeStatePayload *payload)
{
    OpenA8DJTimecodeState stateStorage;
    memcpy(&stateStorage, &payload->timecode, sizeof(stateStorage));
    const OpenA8DJTimecodeState *state = &stateStorage;
    fprintf(stdout,
            "\"armed\":%s,\"armState\":",
            state->armed ? "true" : "false");
    PrintJSONString(stdout, TimecodeArmStateName(state->armState));
    fputs(",\"allowedInputPairs\":", stdout);
    fputs(state->armed ? "[\"A\",\"B\"]" : "[]", stdout);
    fputs(",\"evidenceKind\":\"observed_activity\","
          "\"intentObserved\":false,\"electricalProfile\":", stdout);
    PrintJSONString(stdout, TimecodeProfileName(state->electricalProfile));
    fprintf(stdout,
            ",\"profileVerified\":%s,\"qualified\":%s,"
            "\"optimizedActive\":%s,\"fallbackMode\":",
            state->profileVerified ? "true" : "false",
            state->qualified ? "true" : "false",
            state->optimizedActive ? "true" : "false");
    PrintJSONString(stdout, DriverModeName(state->fallbackMode));
    fprintf(stdout,
            ",\"eligibleWindows\":%u,\"requiredEligibleWindows\":%u,"
            "\"dropoutWindows\":%u,\"windowFrames\":%u,"
            "\"lastFailOpenReason\":",
            state->eligibleWindows,
            kOpenA8DJTimecodeRequiredEligibleWindows,
            state->dropoutWindows,
            state->windowFrames);
    PrintJSONString(stdout,
                    TimecodeFailReasonName(state->lastFailOpenReason));
    fprintf(stdout,
            ",\"inputLeadCeilingFrames\":%u,"
            "\"inputLeadFrames\":%u,\"counters\":{\"arms\":%llu,"
            "\"disarms\":%llu,\"qualifications\":%llu,"
            "\"activations\":%llu,\"deoptimizations\":%llu,"
            "\"outsideAllowlistTrips\":%llu,"
            "\"missingEvidenceTrips\":%llu,\"profileTrips\":%llu,"
            "\"configurationTrips\":%llu,\"xrunErrorTrips\":%llu}",
            kOpenA8DJTimecodeInputLeadCeilingFrames,
            state->inputLeadFrames,
            (unsigned long long)state->counters.arms,
            (unsigned long long)state->counters.disarms,
            (unsigned long long)state->counters.qualifications,
            (unsigned long long)state->counters.activations,
            (unsigned long long)state->counters.deoptimizations,
            (unsigned long long)state->counters.outsideAllowlistTrips,
            (unsigned long long)state->counters.missingEvidenceTrips,
            (unsigned long long)state->counters.profileTrips,
            (unsigned long long)state->counters.configurationTrips,
            (unsigned long long)state->counters.xrunErrorTrips);
    OpenA8DJTimecodeWindow window;
    memcpy(&window, &payload->latestWindow, sizeof(window));
    bool fresh = OpenA8DJTimecodeWindowValid(
        &window, state->windowFrames);
    fprintf(stdout,
            ",\"thresholds\":{\"allowedEntryACRMS\":%.6g,"
            "\"allowedEntryACPeak\":%.6g,\"allowedHoldACRMS\":%.6g,"
            "\"allowedHoldACPeak\":%.6g,\"forbiddenTripACRMS\":%.6g,"
            "\"forbiddenTripPeak\":%.6g},\"windowFresh\":%s,"
            "\"pairWindows\":{",
            OPENA8DJ_TIMECODE_ENTRY_RMS,
            OPENA8DJ_TIMECODE_ENTRY_PEAK,
            OPENA8DJ_TIMECODE_HOLD_RMS,
            OPENA8DJ_TIMECODE_HOLD_PEAK,
            OPENA8DJ_TIMECODE_FORBIDDEN_RMS,
            OPENA8DJ_TIMECODE_FORBIDDEN_PEAK,
            fresh ? "true" : "false");
    for (uint32_t pair = 0; pair < 4; pair++) {
        if (pair != 0) fputc(',', stdout);
        fprintf(stdout, "\"%c\":", (int)('A' + pair));
        if (!fresh) {
            fputs("null", stdout);
            continue;
        }
        bool active = pair < 2 ?
            OpenA8DJTimecodePairEntry(&window, pair) :
            (OpenA8DJTimecodeWindowRMS(&window, pair, 0) >=
                 OPENA8DJ_TIMECODE_FORBIDDEN_RMS ||
             OpenA8DJTimecodeWindowRMS(&window, pair, 1) >=
                 OPENA8DJ_TIMECODE_FORBIDDEN_RMS ||
             window.peak[pair][0] >=
                 OPENA8DJ_TIMECODE_FORBIDDEN_PEAK ||
             window.peak[pair][1] >=
                 OPENA8DJ_TIMECODE_FORBIDDEN_PEAK);
        fprintf(stdout,
                "{\"active\":%s,\"leftACRMS\":%.9g,"
                "\"rightACRMS\":%.9g,\"leftACPeak\":%.9g,"
                "\"rightACPeak\":%.9g,\"leftRawPeak\":%.9g,"
                "\"rightRawPeak\":%.9g}",
                active ? "true" : "false",
                OpenA8DJTimecodeWindowRMS(&window, pair, 0),
                OpenA8DJTimecodeWindowRMS(&window, pair, 1),
                OpenA8DJTimecodeWindowACPeak(&window, pair, 0),
                OpenA8DJTimecodeWindowACPeak(&window, pair, 1),
                window.peak[pair][0], window.peak[pair][1]);
    }
    fputc('}', stdout);
}

static void PrintPublicDriverModeMembers(const OpenA8DJDriverModeStatePayload *state)
{
    fprintf(stdout, "\"schemaVersion\":%u,\"requestedMode\":",
            (unsigned)state->schemaVersion);
    PrintJSONString(stdout, DriverModeName(state->requestedMode));
    fputs(",\"effectiveMode\":", stdout);
    PrintJSONString(stdout, DriverModeName(state->effectiveMode));
    fprintf(stdout,
            ",\"pending\":%s,\"streaming\":%s,\"lastResult\":",
            state->pending ? "true" : "false",
            state->streaming ? "true" : "false");
    PrintJSONString(stdout, DriverModeResultName(state->lastResult));
    fputs(",\"rejectionReason\":", stdout);
    PrintJSONString(stdout, DriverModeRejectionName(state->rejectionReason));
    fprintf(stdout,
            ",\"generation\":%llu,\"counters\":{\"acceptedRequests\":%llu,"
            "\"rejectedRequests\":%llu,\"appliedTransitions\":%llu,"
            "\"applyFailures\":%llu,\"pendingTransitions\":%llu},"
            "\"effectivePolicy\":{\"outputStartLatencyFrames\":%u,"
            "\"outputRestartLatencyFrames\":%u,\"outputTargetLatencyFrames\":%u,"
            "\"workerQoS\":",
            (unsigned long long)state->generation,
            (unsigned long long)state->acceptedRequests,
            (unsigned long long)state->rejectedRequests,
            (unsigned long long)state->appliedTransitions,
            (unsigned long long)state->applyFailures,
            (unsigned long long)state->pendingTransitions,
            state->outputStartLatencyFrames,
            state->outputRestartLatencyFrames,
            state->outputTargetLatencyFrames);
    PrintJSONString(stdout, DriverModeQoSName(state->workerQoS));
    OpenA8DJDriverModePolicy policy;
    (void)OpenA8DJDriverModeLookup(state->effectiveMode, &policy);
    fprintf(stdout,
            ",\"inputLeadGuardEnabled\":%s,"
            "\"inputLeadCeilingFrames\":%u,"
            "\"timecodeEvidenceRequired\":%s",
            policy.inputLeadGuardEnabled ? "true" : "false",
            policy.inputLeadCeilingFrames,
            policy.timecodeEvidenceRequired ? "true" : "false");
    fputs("}", stdout);
}

static const char *VintageConformanceName(uint8_t status)
{
    switch (status) {
        case kOpenA8DJVintageConformanceUnverified:
            return "unverified";
        case kOpenA8DJVintageConformancePartial:
            return "partial";
        case kOpenA8DJVintageConformanceCompatible:
            return "compatible";
        default:
            return NULL;
    }
}

typedef struct OpenA8DJVintageReasonName {
    uint64_t bit;
    const char *name;
} OpenA8DJVintageReasonName;

static const OpenA8DJVintageReasonName kVintageReasonNames[] = {
    {kOpenA8DJVintageReasonNotRequested, "not_requested"},
    {kOpenA8DJVintageReasonPreflightNotRun, "preflight_not_run"},
    {kOpenA8DJVintageReasonAudioParamsResetDisabled,
     "audio_params_reset_disabled"},
    {kOpenA8DJVintageReasonCapturePacedOutputDisabled,
     "capture_paced_output_disabled"},
    {kOpenA8DJVintageReasonOutputStartByteNot4,
     "output_start_byte_not_4"},
    {kOpenA8DJVintageReasonExplicitUSBSchedulingEnabled,
     "explicit_usb_scheduling_enabled"},
    {kOpenA8DJVintageReasonUSBHALTimestampEnabled,
     "usb_hal_timestamp_enabled"},
    {kOpenA8DJVintageReasonTimestampPeriodMismatch,
     "timestamp_period_mismatch"},
    {kOpenA8DJVintageReasonBufferNot512, "buffer_not_512"},
    {kOpenA8DJVintageReasonRateSurfaceMismatch,
     "rate_surface_mismatch"},
    {kOpenA8DJVintageReasonRate192000NotImplemented,
     "rate_192000_not_implemented"},
    {kOpenA8DJVintageReasonLegacyStreamPartitionMismatch,
     "legacy_stream_partition_mismatch"},
    {kOpenA8DJVintageReasonLegacyClientFormatMismatch,
     "legacy_client_format_mismatch"},
    {kOpenA8DJVintageReasonLegacyQueueGeometryMismatch,
     "legacy_queue_geometry_mismatch"},
    {kOpenA8DJVintageReasonTimecodeModeConflict,
     "timecode_mode_conflict"},
    {kOpenA8DJVintageReasonApplyFailed, "apply_failed"},
    {kOpenA8DJVintageReasonPhysicalMatrixMissing,
     "physical_matrix_missing"},
    {kOpenA8DJVintageReasonTimestampRateMismatch,
     "timestamp_rate_mismatch"}
};

static void PrintPublicVintageMembers(
    const OpenA8DJVintageStatePayload *state)
{
    fprintf(stdout,
            "\"schemaVersion\":%u,\"status\":",
            (unsigned)state->schemaVersion);
    PrintJSONString(stdout, VintageConformanceName(state->status));
    fputs(",\"experimental\":true,\"claim\":\"unverified\","
          "\"reasons\":[", stdout);
    bool first = true;
    for (size_t index = 0;
         index < sizeof(kVintageReasonNames) /
                     sizeof(kVintageReasonNames[0]);
         index++) {
        if ((state->reasons & kVintageReasonNames[index].bit) == 0) {
            continue;
        }
        if (!first) fputc(',', stdout);
        PrintJSONString(stdout, kVintageReasonNames[index].name);
        first = false;
    }
    fprintf(stdout,
            "],\"capabilities\":{\"audioParamsReset\":%s,"
            "\"capturePacedOutput\":%s,\"outputStartByte4\":%s,"
            "\"hostMonotonicTimestamp\":%s,\"fixedBuffer512\":%s,"
            "\"rate44100\":%s,\"rate48000\":%s,\"rate88200\":%s,"
            "\"rate96000\":%s,\"rate192000\":%s},"
            "\"preflight\":{\"generation\":%llu,"
            "\"failureCounter\":%llu,\"currentBufferFrames\":%u,"
            "\"supportedRateMask\":%u,\"transportRateMask\":%u,"
            "\"effectiveRateMask\":%u,"
            "\"timestampPeriodFrames\":%u,"
            "\"bufferNormalization\":",
            (state->capabilities &
                 kOpenA8DJVintageCapabilityAudioParamsReset) ?
                "true" : "false",
            (state->capabilities &
                 kOpenA8DJVintageCapabilityCapturePacedOutput) ?
                "true" : "false",
            (state->capabilities &
                 kOpenA8DJVintageCapabilityOutputStartByte4) ?
                "true" : "false",
            (state->capabilities &
                 kOpenA8DJVintageCapabilityHostMonotonicTimestamp) ?
                "true" : "false",
            (state->capabilities &
                 kOpenA8DJVintageCapabilityFixedBuffer512) ?
                "true" : "false",
            (state->capabilities &
                 kOpenA8DJVintageCapabilityRate44100) ?
                "true" : "false",
            (state->capabilities &
                 kOpenA8DJVintageCapabilityRate48000) ?
                "true" : "false",
            (state->capabilities &
                 kOpenA8DJVintageCapabilityRate88200) ?
                "true" : "false",
            (state->capabilities &
                 kOpenA8DJVintageCapabilityRate96000) ?
                "true" : "false",
            (state->capabilities &
                 kOpenA8DJVintageCapabilityRate192000) ?
                "true" : "false",
            (unsigned long long)state->preflightGeneration,
            (unsigned long long)state->failureCounter,
            state->descriptor.currentBufferFrames,
            state->descriptor.supportedRateMask,
            state->descriptor.transportRateMask,
            state->descriptor.effectiveRateMask,
            state->descriptor.timestampPeriodFrames);
    PrintJSONString(
        stdout,
        state->bufferNormalization ==
                kOpenA8DJVintageBufferNormalizationFixed ?
            "fixed" : "shipping_table");
    fprintf(stdout, ",\"normalizedBufferFrames\":%u}",
            state->normalizedBufferFrames);
}

static void PrintPublicDriverMode(
    const char *operation,
    const OpenA8DJDriverModeStatePayload *state,
    const OpenA8DJTimecodeStatePayload *timecode,
    const OpenA8DJVintageStatePayload *vintage)
{
    PrintPublicEnvelopePrefix(operation, true);
    fputs(",\"data\":{", stdout);
    PrintPublicDriverModeMembers(state);
    fputs(",\"timecodeOptimized\":", stdout);
    if (timecode == NULL) {
        fputs("null", stdout);
    } else {
        fputc('{', stdout);
        PrintPublicTimecodeMembers(timecode);
        fputc('}', stdout);
    }
    fputs(",\"vintageCompatible\":", stdout);
    if (vintage == NULL) {
        fputs("null", stdout);
    } else {
        fputc('{', stdout);
        PrintPublicVintageMembers(vintage);
        fputc('}', stdout);
    }
    fputs("}}\n", stdout);
}

typedef enum OpenA8DJStatsDriverModeResult {
    kStatsDriverModeUnavailable = 0,
    kStatsDriverModeValid,
    kStatsDriverModeInvalid
} OpenA8DJStatsDriverModeResult;

static OpenA8DJStatsDriverModeResult DriverModeFromStreamStats(
    const OpenA8DJStreamStatsPayload *stats,
    size_t payloadLength,
    OpenA8DJDriverModeStatePayload *outState)
{
    if (!StreamStatsHasField(
            payloadLength,
            offsetof(OpenA8DJStreamStatsPayload, driverModeWorkerQoS),
            sizeof(stats->driverModeWorkerQoS))) {
        return kStatsDriverModeUnavailable;
    }
    if (stats->driverModeSchemaVersion > UINT16_MAX ||
        stats->driverModeRequested > UINT32_MAX ||
        stats->driverModeEffective > UINT32_MAX ||
        stats->driverModePending > UINT8_MAX ||
        stats->driverModeLastResult > UINT8_MAX ||
        stats->driverModeRejectionReason > UINT8_MAX ||
        stats->driverModeOutputStartLatencyFrames > UINT32_MAX ||
        stats->driverModeOutputRestartLatencyFrames > UINT32_MAX ||
        stats->driverModeOutputTargetLatencyFrames > UINT32_MAX ||
        stats->driverModeWorkerQoS > UINT32_MAX) {
        return kStatsDriverModeInvalid;
    }
    memset(outState, 0, sizeof(*outState));
    outState->schemaVersion = (uint16_t)stats->driverModeSchemaVersion;
    outState->requestedMode = (uint32_t)stats->driverModeRequested;
    outState->effectiveMode = (uint32_t)stats->driverModeEffective;
    outState->pending = (uint8_t)stats->driverModePending;
    outState->streaming = stats->streaming;
    outState->lastResult = (uint8_t)stats->driverModeLastResult;
    outState->rejectionReason = (uint8_t)stats->driverModeRejectionReason;
    outState->generation = stats->driverModeGeneration;
    outState->acceptedRequests = stats->driverModeAcceptedRequests;
    outState->rejectedRequests = stats->driverModeRejectedRequests;
    outState->appliedTransitions = stats->driverModeAppliedTransitions;
    outState->applyFailures = stats->driverModeApplyFailures;
    outState->pendingTransitions = stats->driverModePendingTransitions;
    outState->outputStartLatencyFrames =
        (uint32_t)stats->driverModeOutputStartLatencyFrames;
    outState->outputRestartLatencyFrames =
        (uint32_t)stats->driverModeOutputRestartLatencyFrames;
    outState->outputTargetLatencyFrames =
        (uint32_t)stats->driverModeOutputTargetLatencyFrames;
    outState->workerQoS = (uint32_t)stats->driverModeWorkerQoS;
    return ValidateDriverModeState(outState) ?
        kStatsDriverModeValid : kStatsDriverModeInvalid;
}

static OpenA8DJStatsDriverModeResult TimecodeFromStreamStats(
    const OpenA8DJStreamStatsPayload *stats,
    size_t payloadLength,
    OpenA8DJTimecodeStatePayload *outState)
{
    if (!StreamStatsHasField(
            payloadLength,
            offsetof(OpenA8DJStreamStatsPayload, timecodeOptimized),
            sizeof(stats->timecodeOptimized))) {
        return kStatsDriverModeUnavailable;
    }
    memcpy(outState, &stats->timecodeOptimized, sizeof(*outState));
    return ValidateTimecodeState(outState) ?
        kStatsDriverModeValid : kStatsDriverModeInvalid;
}

static OpenA8DJStatsDriverModeResult VintageFromStreamStats(
    const OpenA8DJStreamStatsPayload *stats,
    size_t payloadLength,
    OpenA8DJVintageStatePayload *outState)
{
    if (!StreamStatsHasField(
            payloadLength,
            offsetof(OpenA8DJStreamStatsPayload, vintageCompatible),
            sizeof(stats->vintageCompatible))) {
        return kStatsDriverModeUnavailable;
    }
    memcpy(outState, &stats->vintageCompatible, sizeof(*outState));
    return ValidateVintageState(outState) ?
        kStatsDriverModeValid : kStatsDriverModeInvalid;
}

static uint64_t PublicStreamField(const OpenA8DJStreamStatsPayload *stats,
                                  size_t payloadLength,
                                  size_t offset,
                                  size_t size,
                                  uint64_t value)
{
    (void)stats;
    return StreamStatsHasField(payloadLength, offset, size) ? value : 0;
}

#define PUBLIC_STREAM_FIELD(stats, length, field) \
    PublicStreamField((stats), (length), offsetof(OpenA8DJStreamStatsPayload, field), \
                      sizeof((stats)->field), (uint64_t)((stats)->field))

static bool QualityInstrumentationAvailable(const OpenA8DJStreamStatsPayload *stats,
                                            size_t payloadLength)
{
    return StreamStatsHasField(
               payloadLength,
               offsetof(OpenA8DJStreamStatsPayload, qualityInstrumentationEnabled),
               sizeof(stats->qualityInstrumentationEnabled)) &&
           stats->qualityInstrumentationEnabled == 1;
}

static void PrintCumulativeJitterDirectionJSON(
    const OpenA8DJStreamStatsPayload *stats,
    size_t payloadLength,
    bool capture)
{
    uint64_t samples = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureCompletionJitterSamples) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackCompletionJitterSamples);
    uint64_t invalid = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureCompletionJitterInvalidIntervals) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackCompletionJitterInvalidIntervals);
    uint64_t le50 = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureCompletionJitterLe50) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackCompletionJitterLe50);
    uint64_t le100 = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureCompletionJitterLe100) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackCompletionJitterLe100);
    uint64_t le250 = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureCompletionJitterLe250) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackCompletionJitterLe250);
    uint64_t le500 = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureCompletionJitterLe500) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackCompletionJitterLe500);
    uint64_t le1000 = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureCompletionJitterLe1000) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackCompletionJitterLe1000);
    uint64_t gt1000 = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureCompletionJitterGt1000) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackCompletionJitterGt1000);
    fprintf(stdout,
            "{\"samples\":%llu,\"invalidIntervals\":%llu,"
            "\"bins\":{\"le50\":%llu,\"le100\":%llu,\"le250\":%llu,"
            "\"le500\":%llu,\"le1000\":%llu,\"gt1000\":%llu}}",
            (unsigned long long)samples,
            (unsigned long long)invalid,
            (unsigned long long)le50,
            (unsigned long long)le100,
            (unsigned long long)le250,
            (unsigned long long)le500,
            (unsigned long long)le1000,
            (unsigned long long)gt1000);
}

static void PrintCumulativeISOErrorsDirectionJSON(
    const OpenA8DJStreamStatsPayload *stats,
    size_t payloadLength,
    bool capture)
{
    uint64_t queueFailures = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureQueueFailures) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackQueueFailures);
    uint64_t completionFailures = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureISOCompletionStatusFailures) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackISOCompletionStatusFailures);
    uint64_t transactionFailures = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureISOTransactionStatusFailures) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackISOTransactionStatusFailures);
    uint64_t zeroLength = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureISOZeroLengthTransactions) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackISOZeroLengthTransactions);
    uint64_t shortTransactions = capture ?
        PUBLIC_STREAM_FIELD(stats, payloadLength, captureISOShortTransactions) :
        PUBLIC_STREAM_FIELD(stats, payloadLength, playbackISOShortTransactions);
    fprintf(stdout,
            "{\"queueFailures\":%llu,\"completionStatusFailures\":%llu,"
            "\"transactionStatusFailures\":%llu,\"zeroLengthTransactions\":%llu,"
            "\"shortTransactions\":%llu}",
            (unsigned long long)queueFailures,
            (unsigned long long)completionFailures,
            (unsigned long long)transactionFailures,
            (unsigned long long)zeroLength,
            (unsigned long long)shortTransactions);
}

static void PrintCumulativeQualityJSON(const OpenA8DJStreamStatsPayload *stats,
                                       size_t payloadLength)
{
    fputs("{\"instrumentationAvailable\":", stdout);
    fputs(QualityInstrumentationAvailable(stats, payloadLength) ? "true" : "false", stdout);
    fputs(",\"completionJitter\":{\"unit\":\"microseconds\","
          "\"binUpperBoundsUs\":[50,100,250,500,1000,null],\"capture\":",
          stdout);
    PrintCumulativeJitterDirectionJSON(stats, payloadLength, true);
    fputs(",\"playback\":", stdout);
    PrintCumulativeJitterDirectionJSON(stats, payloadLength, false);
    fputs("},\"isoErrors\":{\"capture\":", stdout);
    PrintCumulativeISOErrorsDirectionJSON(stats, payloadLength, true);
    fputs(",\"playback\":", stdout);
    PrintCumulativeISOErrorsDirectionJSON(stats, payloadLength, false);
    fputs("}}", stdout);
}

static void PrintPublicStats(const OpenA8DJStreamStatsPayload *stats,
                             size_t payloadLength,
                             const OpenA8DJDriverModeStatePayload *driverMode,
                             const OpenA8DJTimecodeStatePayload *timecode,
                             const OpenA8DJVintageStatePayload *vintage)
{
    bool hasStreaming = StreamStatsHasField(payloadLength,
                                             offsetof(OpenA8DJStreamStatsPayload, streaming),
                                             sizeof(stats->streaming));
    bool hasSampleRate = StreamStatsHasField(payloadLength,
                                              offsetof(OpenA8DJStreamStatsPayload, sampleRate),
                                              sizeof(stats->sampleRate));
    PrintPublicEnvelopePrefix("stats.get", true);
    fprintf(stdout,
            ",\"data\":{\"stream\":{\"streaming\":%s,\"sampleRate\":%.17g,"
            "\"outputRingFrames\":%llu,\"outputTargetLatencyFrames\":%llu},"
            "\"clock\":{\"anchorValid\":%s,\"acceptedAnchors\":%llu,"
            "\"rejectedAnchors\":%llu,\"anchorResets\":%llu,\"usbFrameResyncs\":%llu},"
            "\"capture\":{\"transfers\":%llu,\"transactions\":%llu,\"bytes\":%llu,"
            "\"transactionFailures\":%llu,\"shortTransfers\":%llu,\"queueFailures\":%llu},"
            "\"playback\":{\"transfers\":%llu,\"transactions\":%llu,\"bytes\":%llu,"
            "\"transactionFailures\":%llu,\"shortTransfers\":%llu,\"queueFailures\":%llu},"
            "\"output\":{\"framesWritten\":%llu,\"framesRead\":%llu,\"underruns\":%llu,"
            "\"activeUnderruns\":%llu,\"ringOverruns\":%llu,\"timelineResets\":%llu,"
            "\"lateWriteFrames\":%llu,\"lateWriteBatches\":%llu},"
            "\"health\":{\"inputCheckErrors\":%llu,\"outputPanicFlags\":%llu}",
            hasStreaming && stats->streaming ? "true" : "false",
            hasSampleRate && isfinite(stats->sampleRate) ? stats->sampleRate : 0.0,
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, outputRingFrames),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, outputTargetLatencyFrames),
            StreamStatsHasField(payloadLength,
                                offsetof(OpenA8DJStreamStatsPayload, clockAnchorValid),
                                sizeof(stats->clockAnchorValid)) && stats->clockAnchorValid ? "true" : "false",
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, clockAcceptedAnchors),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, clockRejectedAnchors),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, clockAnchorResets),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, clockUSBFrameResyncs),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, captureTransfers),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, captureTransactions),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, captureBytes),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, captureTransactionFailures),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, captureShortTransfers),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, captureQueueFailures),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, playbackTransfers),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, playbackTransactions),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, playbackBytes),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, playbackTransactionFailures),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, playbackShortTransfers),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, playbackQueueFailures),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, outputFramesWritten),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, outputFramesRead),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, outputUnderruns),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, outputActiveUnderruns),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, outputRingOverruns),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, outputTimelineResets),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, outputLateWriteFrames),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, outputLateWriteBatches),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, inputCheckErrors),
            (unsigned long long)PUBLIC_STREAM_FIELD(stats, payloadLength, outputPanicFlags));
    fputs(",\"quality\":", stdout);
    PrintCumulativeQualityJSON(stats, payloadLength);
    fputs(",\"driverMode\":", stdout);
    if (driverMode == NULL) {
        fputs("null", stdout);
    } else {
        fputc('{', stdout);
        PrintPublicDriverModeMembers(driverMode);
        fputc('}', stdout);
    }
    fputs(",\"timecodeOptimized\":", stdout);
    if (timecode == NULL) {
        fputs("null", stdout);
    } else {
        fputc('{', stdout);
        PrintPublicTimecodeMembers(timecode);
        fputc('}', stdout);
    }
    fputs(",\"vintageCompatible\":", stdout);
    if (vintage == NULL) {
        fputs("null", stdout);
    } else {
        fputc('{', stdout);
        PrintPublicVintageMembers(vintage);
        fputc('}', stdout);
    }
    fputs("}}\n", stdout);
}

#undef PUBLIC_STREAM_FIELD

enum {
    kQualityBinCount = 6,
    kQualityMinimumSamples = 20
};

typedef struct OpenA8DJQualityJitterWindow {
    uint64_t samples;
    uint64_t invalidIntervals;
    uint64_t bins[kQualityBinCount];
    int percentile50Bin;
    int percentile95Bin;
    int percentile99Bin;
    bool active;
} OpenA8DJQualityJitterWindow;

typedef struct OpenA8DJQualityISOErrorWindow {
    uint64_t queueFailures;
    uint64_t completionStatusFailures;
    uint64_t transactionStatusFailures;
    uint64_t zeroLengthTransactions;
    uint64_t shortTransactions;
    uint64_t totalEvents;
} OpenA8DJQualityISOErrorWindow;

typedef struct OpenA8DJQualityXrunWindow {
    uint64_t outputUnderruns;
    uint64_t activeOutputUnderruns;
    uint64_t outputRingOverruns;
    uint64_t outputLateWriteBatches;
    uint64_t outputLateWriteFrames;
    uint64_t totalHardXruns;
} OpenA8DJQualityXrunWindow;

typedef struct OpenA8DJQualityWindow {
    const char *classification;
    const char *reasons[16];
    size_t reasonCount;
    bool streaming;
    bool instrumentationAvailable;
    bool captureActive;
    bool playbackActive;
    double sampleRate;
    uint64_t windowMilliseconds;
    OpenA8DJQualityJitterWindow captureJitter;
    OpenA8DJQualityJitterWindow playbackJitter;
    OpenA8DJQualityISOErrorWindow captureISOErrors;
    OpenA8DJQualityISOErrorWindow playbackISOErrors;
    OpenA8DJQualityXrunWindow xruns;
} OpenA8DJQualityWindow;

static bool QualityCounterDecreased(const OpenA8DJStreamStatsPayload *previous,
                                    const OpenA8DJStreamStatsPayload *current)
{
#define QUALITY_COUNTER_DECREASED(field) (current->field < previous->field)
    return
        QUALITY_COUNTER_DECREASED(captureTransfers) ||
        QUALITY_COUNTER_DECREASED(playbackTransfers) ||
        QUALITY_COUNTER_DECREASED(captureQueueFailures) ||
        QUALITY_COUNTER_DECREASED(playbackQueueFailures) ||
        QUALITY_COUNTER_DECREASED(outputUnderruns) ||
        QUALITY_COUNTER_DECREASED(outputActiveUnderruns) ||
        QUALITY_COUNTER_DECREASED(outputRingOverruns) ||
        QUALITY_COUNTER_DECREASED(outputLateWriteFrames) ||
        QUALITY_COUNTER_DECREASED(outputLateWriteBatches) ||
        QUALITY_COUNTER_DECREASED(captureCompletionJitterSamples) ||
        QUALITY_COUNTER_DECREASED(captureCompletionJitterInvalidIntervals) ||
        QUALITY_COUNTER_DECREASED(captureCompletionJitterLe50) ||
        QUALITY_COUNTER_DECREASED(captureCompletionJitterLe100) ||
        QUALITY_COUNTER_DECREASED(captureCompletionJitterLe250) ||
        QUALITY_COUNTER_DECREASED(captureCompletionJitterLe500) ||
        QUALITY_COUNTER_DECREASED(captureCompletionJitterLe1000) ||
        QUALITY_COUNTER_DECREASED(captureCompletionJitterGt1000) ||
        QUALITY_COUNTER_DECREASED(playbackCompletionJitterSamples) ||
        QUALITY_COUNTER_DECREASED(playbackCompletionJitterInvalidIntervals) ||
        QUALITY_COUNTER_DECREASED(playbackCompletionJitterLe50) ||
        QUALITY_COUNTER_DECREASED(playbackCompletionJitterLe100) ||
        QUALITY_COUNTER_DECREASED(playbackCompletionJitterLe250) ||
        QUALITY_COUNTER_DECREASED(playbackCompletionJitterLe500) ||
        QUALITY_COUNTER_DECREASED(playbackCompletionJitterLe1000) ||
        QUALITY_COUNTER_DECREASED(playbackCompletionJitterGt1000) ||
        QUALITY_COUNTER_DECREASED(captureISOCompletionStatusFailures) ||
        QUALITY_COUNTER_DECREASED(captureISOTransactionStatusFailures) ||
        QUALITY_COUNTER_DECREASED(captureISOZeroLengthTransactions) ||
        QUALITY_COUNTER_DECREASED(captureISOShortTransactions) ||
        QUALITY_COUNTER_DECREASED(playbackISOCompletionStatusFailures) ||
        QUALITY_COUNTER_DECREASED(playbackISOTransactionStatusFailures) ||
        QUALITY_COUNTER_DECREASED(playbackISOZeroLengthTransactions) ||
        QUALITY_COUNTER_DECREASED(playbackISOShortTransactions);
#undef QUALITY_COUNTER_DECREASED
}

static int QualityPercentileBin(const uint64_t bins[kQualityBinCount],
                                uint64_t samples,
                                uint64_t percentile)
{
    if (samples == 0) {
        return -1;
    }
    uint64_t rank = samples / 100 * percentile;
    uint64_t remainder = samples % 100;
    rank += (remainder * percentile + 99) / 100;
    uint64_t cumulative = 0;
    for (int bin = 0; bin < kQualityBinCount; bin++) {
        cumulative += bins[bin];
        if (cumulative >= rank) {
            return bin;
        }
    }
    return -1;
}

static bool QualityPermilleAtMost(uint64_t numerator,
                                  uint64_t denominator,
                                  uint64_t maximumPermille)
{
    if (denominator == 0) {
        return true;
    }
    uint64_t maximumNumerator =
        (denominator / 1000) * maximumPermille +
        ((denominator % 1000) * maximumPermille) / 1000;
    return numerator <= maximumNumerator;
}

static void QualityAddReason(OpenA8DJQualityWindow *window, const char *reason)
{
    if (window->reasonCount <
        sizeof(window->reasons) / sizeof(window->reasons[0])) {
        window->reasons[window->reasonCount++] = reason;
    }
}

static void QualityFinalizeJitter(OpenA8DJQualityJitterWindow *jitter)
{
    jitter->percentile50Bin = QualityPercentileBin(jitter->bins, jitter->samples, 50);
    jitter->percentile95Bin = QualityPercentileBin(jitter->bins, jitter->samples, 95);
    jitter->percentile99Bin = QualityPercentileBin(jitter->bins, jitter->samples, 99);
}

static bool QualityJitterCountsAreConsistent(
    const OpenA8DJQualityJitterWindow *jitter)
{
    uint64_t sum = 0;
    for (int bin = 0; bin < kQualityBinCount; bin++) {
        if (UINT64_MAX - sum < jitter->bins[bin]) {
            return false;
        }
        sum += jitter->bins[bin];
    }
    return sum == jitter->samples;
}

static OpenA8DJQualityWindow CalculateQualityWindow(
    const OpenA8DJStreamStatsPayload *previous,
    size_t previousLength,
    bool hasPrevious,
    const OpenA8DJStreamStatsPayload *current,
    size_t currentLength,
    uint64_t windowMilliseconds)
{
    OpenA8DJQualityWindow window;
    memset(&window, 0, sizeof(window));
    window.classification = "warming-up";
    window.streaming = current->streaming != 0;
    window.instrumentationAvailable =
        QualityInstrumentationAvailable(current, currentLength);
    window.sampleRate = isfinite(current->sampleRate) ? current->sampleRate : 0.0;
    window.windowMilliseconds = windowMilliseconds;

    if (!window.streaming) {
        window.classification = "not-streaming";
        return window;
    }
    if (!window.instrumentationAvailable) {
        window.classification = "insufficient-data";
        QualityAddReason(&window, "instrumentation_unavailable");
        return window;
    }
    if (!hasPrevious ||
        !QualityInstrumentationAvailable(previous, previousLength) ||
        (!previous->streaming && current->streaming)) {
        return window;
    }
    if (QualityCounterDecreased(previous, current)) {
        QualityAddReason(&window, "counter_reset");
        return window;
    }

#define QUALITY_DELTA(field) (current->field - previous->field)
    window.captureActive = QUALITY_DELTA(captureTransfers) > 0;
    window.playbackActive = QUALITY_DELTA(playbackTransfers) > 0;
    window.captureJitter.active = window.captureActive;
    window.playbackJitter.active = window.playbackActive;

    window.captureJitter.samples = QUALITY_DELTA(captureCompletionJitterSamples);
    window.captureJitter.invalidIntervals =
        QUALITY_DELTA(captureCompletionJitterInvalidIntervals);
    window.captureJitter.bins[0] = QUALITY_DELTA(captureCompletionJitterLe50);
    window.captureJitter.bins[1] = QUALITY_DELTA(captureCompletionJitterLe100);
    window.captureJitter.bins[2] = QUALITY_DELTA(captureCompletionJitterLe250);
    window.captureJitter.bins[3] = QUALITY_DELTA(captureCompletionJitterLe500);
    window.captureJitter.bins[4] = QUALITY_DELTA(captureCompletionJitterLe1000);
    window.captureJitter.bins[5] = QUALITY_DELTA(captureCompletionJitterGt1000);
    window.playbackJitter.samples = QUALITY_DELTA(playbackCompletionJitterSamples);
    window.playbackJitter.invalidIntervals =
        QUALITY_DELTA(playbackCompletionJitterInvalidIntervals);
    window.playbackJitter.bins[0] = QUALITY_DELTA(playbackCompletionJitterLe50);
    window.playbackJitter.bins[1] = QUALITY_DELTA(playbackCompletionJitterLe100);
    window.playbackJitter.bins[2] = QUALITY_DELTA(playbackCompletionJitterLe250);
    window.playbackJitter.bins[3] = QUALITY_DELTA(playbackCompletionJitterLe500);
    window.playbackJitter.bins[4] = QUALITY_DELTA(playbackCompletionJitterLe1000);
    window.playbackJitter.bins[5] = QUALITY_DELTA(playbackCompletionJitterGt1000);

    window.captureISOErrors.queueFailures = QUALITY_DELTA(captureQueueFailures);
    window.captureISOErrors.completionStatusFailures =
        QUALITY_DELTA(captureISOCompletionStatusFailures);
    window.captureISOErrors.transactionStatusFailures =
        QUALITY_DELTA(captureISOTransactionStatusFailures);
    window.captureISOErrors.zeroLengthTransactions =
        QUALITY_DELTA(captureISOZeroLengthTransactions);
    window.captureISOErrors.shortTransactions =
        QUALITY_DELTA(captureISOShortTransactions);
    window.playbackISOErrors.queueFailures = QUALITY_DELTA(playbackQueueFailures);
    window.playbackISOErrors.completionStatusFailures =
        QUALITY_DELTA(playbackISOCompletionStatusFailures);
    window.playbackISOErrors.transactionStatusFailures =
        QUALITY_DELTA(playbackISOTransactionStatusFailures);
    window.playbackISOErrors.zeroLengthTransactions =
        QUALITY_DELTA(playbackISOZeroLengthTransactions);
    window.playbackISOErrors.shortTransactions =
        QUALITY_DELTA(playbackISOShortTransactions);
    window.captureISOErrors.totalEvents =
        window.captureISOErrors.queueFailures +
        window.captureISOErrors.completionStatusFailures +
        window.captureISOErrors.transactionStatusFailures +
        window.captureISOErrors.zeroLengthTransactions +
        window.captureISOErrors.shortTransactions;
    window.playbackISOErrors.totalEvents =
        window.playbackISOErrors.queueFailures +
        window.playbackISOErrors.completionStatusFailures +
        window.playbackISOErrors.transactionStatusFailures +
        window.playbackISOErrors.zeroLengthTransactions +
        window.playbackISOErrors.shortTransactions;

    window.xruns.outputUnderruns = QUALITY_DELTA(outputUnderruns);
    window.xruns.activeOutputUnderruns = QUALITY_DELTA(outputActiveUnderruns);
    window.xruns.outputRingOverruns = QUALITY_DELTA(outputRingOverruns);
    window.xruns.outputLateWriteBatches = QUALITY_DELTA(outputLateWriteBatches);
    window.xruns.outputLateWriteFrames = QUALITY_DELTA(outputLateWriteFrames);
    window.xruns.totalHardXruns =
        window.xruns.activeOutputUnderruns + window.xruns.outputRingOverruns;
#undef QUALITY_DELTA

    QualityFinalizeJitter(&window.captureJitter);
    QualityFinalizeJitter(&window.playbackJitter);
    if (!QualityJitterCountsAreConsistent(&window.captureJitter) ||
        !QualityJitterCountsAreConsistent(&window.playbackJitter)) {
        window.classification = "insufficient-data";
        QualityAddReason(&window, "instrumentation_inconsistent");
        return window;
    }

    bool unstable = false;
    if (window.captureISOErrors.totalEvents > 0) {
        QualityAddReason(&window, "capture.iso_errors");
        unstable = true;
    }
    if (window.playbackISOErrors.totalEvents > 0) {
        QualityAddReason(&window, "playback.iso_errors");
        unstable = true;
    }
    if (window.xruns.totalHardXruns > 0) {
        QualityAddReason(&window, "output.hard_xrun");
        unstable = true;
    }
    if (window.captureActive && window.captureJitter.percentile99Bin == 5) {
        QualityAddReason(&window, "capture.p99_gt_1000us");
        unstable = true;
    }
    if (window.playbackActive && window.playbackJitter.percentile99Bin == 5) {
        QualityAddReason(&window, "playback.p99_gt_1000us");
        unstable = true;
    }
    if (window.captureActive &&
        !QualityPermilleAtMost(window.captureJitter.bins[5],
                               window.captureJitter.samples,
                               10)) {
        QualityAddReason(&window, "capture.overflow_gt_10_permille");
        unstable = true;
    }
    if (window.playbackActive &&
        !QualityPermilleAtMost(window.playbackJitter.bins[5],
                               window.playbackJitter.samples,
                               10)) {
        QualityAddReason(&window, "playback.overflow_gt_10_permille");
        unstable = true;
    }
    if (unstable) {
        window.classification = "unstable";
        return window;
    }

    if (!window.captureActive && !window.playbackActive) {
        window.classification = "insufficient-data";
        QualityAddReason(&window, "no_active_directions");
        return window;
    }

    bool insufficient = false;
    if (window.captureActive && window.captureJitter.samples < kQualityMinimumSamples) {
        QualityAddReason(&window, "capture.insufficient_samples");
        insufficient = true;
    }
    if (window.playbackActive && window.playbackJitter.samples < kQualityMinimumSamples) {
        QualityAddReason(&window, "playback.insufficient_samples");
        insufficient = true;
    }
    if (insufficient) {
        window.classification = "insufficient-data";
        return window;
    }

    bool stable = true;
    if (window.captureActive && window.captureJitter.percentile95Bin > 2) {
        QualityAddReason(&window, "capture.p95_gt_250us");
        stable = false;
    }
    if (window.captureActive && window.captureJitter.percentile99Bin > 3) {
        QualityAddReason(&window, "capture.p99_gt_500us");
        stable = false;
    }
    if (window.captureActive &&
        !QualityPermilleAtMost(window.captureJitter.bins[5],
                               window.captureJitter.samples,
                               1)) {
        QualityAddReason(&window, "capture.overflow_gt_1_permille");
        stable = false;
    }
    if (window.playbackActive && window.playbackJitter.percentile95Bin > 2) {
        QualityAddReason(&window, "playback.p95_gt_250us");
        stable = false;
    }
    if (window.playbackActive && window.playbackJitter.percentile99Bin > 3) {
        QualityAddReason(&window, "playback.p99_gt_500us");
        stable = false;
    }
    if (window.playbackActive &&
        !QualityPermilleAtMost(window.playbackJitter.bins[5],
                               window.playbackJitter.samples,
                               1)) {
        QualityAddReason(&window, "playback.overflow_gt_1_permille");
        stable = false;
    }
    window.classification = stable ? "stable" : "degraded";
    return window;
}

static void PrintQualityPercentileJSON(int bin)
{
    static const unsigned upperBounds[5] = {50, 100, 250, 500, 1000};
    if (bin < 0) {
        fputs("null", stdout);
    } else if (bin < 5) {
        fprintf(stdout,
                "{\"upperBoundUs\":%u,\"overflow\":false}",
                upperBounds[bin]);
    } else {
        fputs("{\"upperBoundUs\":null,\"lowerBoundExclusiveUs\":1000,"
              "\"overflow\":true}",
              stdout);
    }
}

static void PrintQualityJitterWindowJSON(
    const OpenA8DJQualityJitterWindow *jitter)
{
    fprintf(stdout,
            "{\"active\":%s,\"samples\":%llu,\"invalidIntervals\":%llu,"
            "\"bins\":{\"le50\":%llu,\"le100\":%llu,\"le250\":%llu,"
            "\"le500\":%llu,\"le1000\":%llu,\"gt1000\":%llu},\"p50\":",
            jitter->active ? "true" : "false",
            (unsigned long long)jitter->samples,
            (unsigned long long)jitter->invalidIntervals,
            (unsigned long long)jitter->bins[0],
            (unsigned long long)jitter->bins[1],
            (unsigned long long)jitter->bins[2],
            (unsigned long long)jitter->bins[3],
            (unsigned long long)jitter->bins[4],
            (unsigned long long)jitter->bins[5]);
    PrintQualityPercentileJSON(jitter->percentile50Bin);
    fputs(",\"p95\":", stdout);
    PrintQualityPercentileJSON(jitter->percentile95Bin);
    fputs(",\"p99\":", stdout);
    PrintQualityPercentileJSON(jitter->percentile99Bin);
    fputc('}', stdout);
}

static void PrintQualityISOErrorWindowJSON(
    const OpenA8DJQualityISOErrorWindow *errors)
{
    fprintf(stdout,
            "{\"queueFailures\":%llu,\"completionStatusFailures\":%llu,"
            "\"transactionStatusFailures\":%llu,\"zeroLengthTransactions\":%llu,"
            "\"shortTransactions\":%llu,\"totalEvents\":%llu}",
            (unsigned long long)errors->queueFailures,
            (unsigned long long)errors->completionStatusFailures,
            (unsigned long long)errors->transactionStatusFailures,
            (unsigned long long)errors->zeroLengthTransactions,
            (unsigned long long)errors->shortTransactions,
            (unsigned long long)errors->totalEvents);
}

static void PrintQualityThresholdsJSON(void)
{
    fputs("{\"minimumSamplesPerActiveDirection\":20,"
          "\"stableP95UpperBoundUs\":250,\"stableP99UpperBoundUs\":500,"
          "\"degradedP99UpperBoundUs\":1000,"
          "\"stableOverflowPermilleMax\":1,"
          "\"degradedOverflowPermilleMax\":10}",
          stdout);
}

static void PrintQualitySampleJSON(const OpenA8DJQualityWindow *window,
                                   uint64_t sequence)
{
    fprintf(stdout,
            "{\"schema\":\"org.opena8dj.usb-quality.sample.v1\","
            "\"sequence\":%llu,\"windowMilliseconds\":%llu,"
            "\"streaming\":%s,\"sampleRateHz\":%.17g,"
            "\"instrumentationAvailable\":%s,"
            "\"jitter\":{\"unit\":\"microseconds\","
            "\"binUpperBoundsUs\":[50,100,250,500,1000,null],\"capture\":",
            (unsigned long long)sequence,
            (unsigned long long)window->windowMilliseconds,
            window->streaming ? "true" : "false",
            window->sampleRate,
            window->instrumentationAvailable ? "true" : "false");
    PrintQualityJitterWindowJSON(&window->captureJitter);
    fputs(",\"playback\":", stdout);
    PrintQualityJitterWindowJSON(&window->playbackJitter);
    fputs("},\"isoErrors\":{\"capture\":", stdout);
    PrintQualityISOErrorWindowJSON(&window->captureISOErrors);
    fputs(",\"playback\":", stdout);
    PrintQualityISOErrorWindowJSON(&window->playbackISOErrors);
    fprintf(stdout,
            "},\"xruns\":{\"outputUnderruns\":%llu,"
            "\"activeOutputUnderruns\":%llu,\"outputRingOverruns\":%llu,"
            "\"outputLateWriteBatches\":%llu,\"outputLateWriteFrames\":%llu,"
            "\"totalHardXruns\":%llu},\"stability\":{\"classification\":",
            (unsigned long long)window->xruns.outputUnderruns,
            (unsigned long long)window->xruns.activeOutputUnderruns,
            (unsigned long long)window->xruns.outputRingOverruns,
            (unsigned long long)window->xruns.outputLateWriteBatches,
            (unsigned long long)window->xruns.outputLateWriteFrames,
            (unsigned long long)window->xruns.totalHardXruns);
    PrintJSONString(stdout, window->classification);
    fputs(",\"reasons\":[", stdout);
    for (size_t reason = 0; reason < window->reasonCount; reason++) {
        if (reason != 0) {
            fputc(',', stdout);
        }
        PrintJSONString(stdout, window->reasons[reason]);
    }
    fputs("],\"thresholds\":", stdout);
    PrintQualityThresholdsJSON();
    fprintf(stdout,
            ",\"inputs\":{\"captureActive\":%s,\"playbackActive\":%s,"
            "\"captureSamples\":%llu,\"playbackSamples\":%llu,"
            "\"captureISOErrorEvents\":%llu,\"playbackISOErrorEvents\":%llu,"
            "\"totalHardXruns\":%llu}}}\n",
            window->captureActive ? "true" : "false",
            window->playbackActive ? "true" : "false",
            (unsigned long long)window->captureJitter.samples,
            (unsigned long long)window->playbackJitter.samples,
            (unsigned long long)window->captureISOErrors.totalEvents,
            (unsigned long long)window->playbackISOErrors.totalEvents,
            (unsigned long long)window->xruns.totalHardXruns);
}

static void FormatQualityPercentile(char *buffer, size_t size, int bin)
{
    static const unsigned upperBounds[5] = {50, 100, 250, 500, 1000};
    if (bin < 0) {
        (void)snprintf(buffer, size, "-");
    } else if (bin < 5) {
        (void)snprintf(buffer, size, "<=%uus", upperBounds[bin]);
    } else {
        (void)snprintf(buffer, size, ">1000us");
    }
}

static void PrintQualitySampleHuman(const OpenA8DJQualityWindow *window,
                                    bool printHeader)
{
    if (printHeader) {
        puts("classification       cap-p95  cap-p99  play-p95 play-p99 "
             "iso-cap iso-play hard-xruns window-ms");
    }
    char captureP95[16];
    char captureP99[16];
    char playbackP95[16];
    char playbackP99[16];
    FormatQualityPercentile(captureP95, sizeof(captureP95),
                            window->captureJitter.percentile95Bin);
    FormatQualityPercentile(captureP99, sizeof(captureP99),
                            window->captureJitter.percentile99Bin);
    FormatQualityPercentile(playbackP95, sizeof(playbackP95),
                            window->playbackJitter.percentile95Bin);
    FormatQualityPercentile(playbackP99, sizeof(playbackP99),
                            window->playbackJitter.percentile99Bin);
    printf("%-20s %-8s %-8s %-8s %-8s %7llu %8llu %10llu %9llu\n",
           window->classification,
           captureP95,
           captureP99,
           playbackP95,
           playbackP99,
           (unsigned long long)window->captureISOErrors.totalEvents,
           (unsigned long long)window->playbackISOErrors.totalEvents,
           (unsigned long long)window->xruns.totalHardXruns,
           (unsigned long long)window->windowMilliseconds);
    if (strcmp(window->classification, "stable") != 0) {
        fputs("  reasons: ", stdout);
        if (window->reasonCount == 0) {
            fputc('-', stdout);
        }
        for (size_t reason = 0; reason < window->reasonCount; reason++) {
            if (reason != 0) {
                fputc(',', stdout);
            }
            fputs(window->reasons[reason], stdout);
        }
        fputs("\n  thresholds: min-samples=20 stable-p95<=250us "
              "stable-p99<=500us degraded-p99<=1000us "
              "overflow<=1/10 permille\n",
              stdout);
    }
}

static OpenA8DJPublicBackendResult ConnectPublicSocket(int *outFD)
{
    struct stat pathState;
    *outFD = -1;
    if (lstat(kSocketPath, &pathState) != 0) {
        return errno == EACCES ? kPublicBackendPermissionDenied : kPublicBackendUnavailable;
    }
    struct passwd *coreAudioAccount = getpwnam("_coreaudiod");
    bool expectedOwner = pathState.st_uid == 0 ||
                         (coreAudioAccount != NULL && pathState.st_uid == coreAudioAccount->pw_uid);
#ifdef OPENA8DJ_PUBLIC_API_TEST_TRUST_CURRENT_UID
    expectedOwner = expectedOwner || pathState.st_uid == geteuid();
#endif
    if (!S_ISSOCK(pathState.st_mode) ||
        !expectedOwner ||
        (pathState.st_mode & 0111) != 0) {
        return kPublicBackendPermissionDenied;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return kPublicBackendUnavailable;
    }
#ifdef SO_NOSIGPIPE
    int noSignal = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
#endif
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(fd);
        return kPublicBackendUnavailable;
    }

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    if (strlcpy(address.sun_path, kSocketPath, sizeof(address.sun_path)) >= sizeof(address.sun_path)) {
        close(fd);
        return kPublicBackendUnavailable;
    }
    int result = connect(fd, (struct sockaddr *)&address, sizeof(address));
    bool connected = result == 0;
    if (!connected && errno == EINPROGRESS) {
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(fd, &writeSet);
        struct timeval connectTimeout = {.tv_sec = 0, .tv_usec = 500000};
        result = select(fd + 1, NULL, &writeSet, NULL, &connectTimeout);
        if (result > 0) {
            int socketError = 0;
            socklen_t errorSize = sizeof(socketError);
            connected = getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &errorSize) == 0 &&
                        socketError == 0;
        }
    }
    if (!connected) {
        close(fd);
        return kPublicBackendUnavailable;
    }

#ifdef OPENA8DJ_PUBLIC_API_TEST_POST_CONNECT_DELAY_USEC
    usleep(OPENA8DJ_PUBLIC_API_TEST_POST_CONNECT_DELAY_USEC);
#endif
    uid_t peerUID = (uid_t)-1;
    gid_t peerGID = (gid_t)-1;
    struct stat verifiedPathState;
    if (getpeereid(fd, &peerUID, &peerGID) != 0 ||
        lstat(kSocketPath, &verifiedPathState) != 0 ||
        !S_ISSOCK(verifiedPathState.st_mode) ||
        (verifiedPathState.st_mode & 0111) != 0 ||
        verifiedPathState.st_dev != pathState.st_dev ||
        verifiedPathState.st_ino != pathState.st_ino ||
        verifiedPathState.st_uid != pathState.st_uid ||
        peerUID != verifiedPathState.st_uid) {
        close(fd);
        return kPublicBackendPermissionDenied;
    }
    if (fcntl(fd, F_SETFL, flags) != 0) {
        close(fd);
        return kPublicBackendUnavailable;
    }
    struct timeval ioTimeout = {.tv_sec = 1, .tv_usec = 0};
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &ioTimeout, sizeof(ioTimeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &ioTimeout, sizeof(ioTimeout));
    *outFD = fd;
    return kPublicBackendOK;
}

static const char *PublicMutationLockPath(char *buffer, size_t bufferSize)
{
#ifdef OPENA8DJ_PUBLIC_API_LOCK_PATH
    (void)buffer;
    (void)bufferSize;
    return kPublicAPILockPath;
#else
    snprintf(buffer, bufferSize, "/tmp/opena8dj-public-api-%u.lock", (unsigned)geteuid());
    return buffer;
#endif
}

static int AcquirePublicMutationLock(void)
{
    char generatedPath[128];
    const char *path = PublicMutationLockPath(generatedPath, sizeof(generatedPath));
    int fd = open(path, O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
    if (fd < 0) {
        return -1;
    }
    struct stat lockState;
    if (fstat(fd, &lockState) != 0 ||
        !S_ISREG(lockState.st_mode) ||
        lockState.st_uid != geteuid() ||
        (lockState.st_mode & 0777) != 0600) {
        close(fd);
        return -1;
    }
    for (int attempt = 0; attempt < 100; attempt++) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
            return fd;
        }
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            break;
        }
        usleep(10000);
    }
    close(fd);
    return -1;
}

static int PublicBackendError(const char *operation, OpenA8DJPublicBackendResult result)
{
    if (result == kPublicBackendPermissionDenied) {
        return PrintPublicError(operation,
                                "backend_permission_denied",
                                "The HAL bridge socket or peer credentials failed local authentication.",
                                false,
                                4);
    }
    return PrintPublicError(operation,
                            "backend_unavailable",
                            "The HAL control bridge is not running.",
                            true,
                            3);
}

static volatile sig_atomic_t gUSBQualityInterrupted = 0;

static void HandleUSBQualityInterrupt(int signalNumber)
{
    (void)signalNumber;
    gUSBQualityInterrupted = 1;
}

static bool ParseBoundedUnsigned(const char *text,
                                 uint64_t minimum,
                                 uint64_t maximum,
                                 uint64_t *outValue)
{
    if (text == NULL || *text == '\0' || *text == '-' || *text == '+') {
        return false;
    }
    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed < minimum || parsed > maximum) {
        return false;
    }
    *outValue = (uint64_t)parsed;
    return true;
}

static uint64_t MonotonicMilliseconds(void)
{
    struct timespec now = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    return (uint64_t)now.tv_sec * 1000 +
           (uint64_t)now.tv_nsec / 1000000;
}

static void SleepUSBQualityInterval(uint64_t intervalMilliseconds)
{
    struct timespec remaining = {
        .tv_sec = (time_t)(intervalMilliseconds / 1000),
        .tv_nsec = (long)((intervalMilliseconds % 1000) * 1000000)
    };
    while (!gUSBQualityInterrupted &&
           nanosleep(&remaining, &remaining) != 0 &&
           errno == EINTR) {
    }
}

static int PrintUSBQualityBackendError(bool json,
                                       uint64_t sequence,
                                       const char *code,
                                       const char *message,
                                       bool retryable,
                                       int exitCode)
{
    if (!json) {
        fprintf(stderr, "usb-quality: %s: %s\n", code, message);
        return exitCode;
    }
    fprintf(stdout,
            "{\"schema\":\"org.opena8dj.usb-quality.sample.v1\","
            "\"sequence\":%llu,\"error\":{\"code\":",
            (unsigned long long)sequence);
    PrintJSONString(stdout, code);
    fputs(",\"message\":", stdout);
    PrintJSONString(stdout, message);
    fprintf(stdout, ",\"retryable\":%s}}\n", retryable ? "true" : "false");
    return exitCode;
}

static int RunUSBQuality(int argc, char **argv)
{
    uint64_t intervalMilliseconds = 1000;
    uint64_t count = UINT64_MAX;
    bool json = false;
    bool sawInterval = false;
    bool sawCount = false;
    for (int argument = 2; argument < argc; argument++) {
        if (strcmp(argv[argument], "--json") == 0 && !json) {
            json = true;
        } else if (strcmp(argv[argument], "--interval-ms") == 0 &&
                   !sawInterval && argument + 1 < argc) {
            sawInterval = true;
            if (!ParseBoundedUnsigned(argv[++argument], 100, 60000,
                                      &intervalMilliseconds)) {
                fprintf(stderr, "usb-quality: --interval-ms must be 100..60000\n");
                return 2;
            }
        } else if (strcmp(argv[argument], "--count") == 0 &&
                   !sawCount && argument + 1 < argc) {
            sawCount = true;
            if (!ParseBoundedUnsigned(argv[++argument], 1, 86400, &count)) {
                fprintf(stderr, "usb-quality: --count must be 1..86400\n");
                return 2;
            }
        } else {
            fprintf(stderr, "usb-quality: invalid argument\n");
            return 2;
        }
    }

    (void)signal(SIGPIPE, SIG_IGN);
    (void)signal(SIGINT, HandleUSBQualityInterrupt);
    gUSBQualityInterrupted = 0;
    OpenA8DJStreamStatsPayload previous;
    memset(&previous, 0, sizeof(previous));
    size_t previousLength = 0;
    bool hasPrevious = false;
    uint64_t previousTime = 0;

    for (uint64_t sequence = 1;
         sequence <= count && !gUSBQualityInterrupted;
         sequence++) {
        if (sequence > 1) {
            SleepUSBQualityInterval(intervalMilliseconds);
            if (gUSBQualityInterrupted) {
                break;
            }
        }

        int fd = -1;
        OpenA8DJPublicBackendResult backend = ConnectPublicSocket(&fd);
        if (backend != kPublicBackendOK) {
            return PrintUSBQualityBackendError(
                json,
                sequence,
                backend == kPublicBackendPermissionDenied ?
                    "backend_permission_denied" : "backend_unavailable",
                backend == kPublicBackendPermissionDenied ?
                    "The HAL bridge socket or peer credentials failed local authentication." :
                    "The HAL control bridge is not running.",
                backend != kPublicBackendPermissionDenied,
                backend == kPublicBackendPermissionDenied ? 4 : 3);
        }

        OpenA8DJStreamStatsPayload current;
        size_t currentLength = 0;
        size_t minimumPayloadLength =
            offsetof(OpenA8DJStreamStatsPayload, sampleRate) +
            sizeof(current.sampleRate);
        bool readOK = ReadStreamStats(fd, &current, &currentLength);
        close(fd);
        if (!readOK || currentLength < minimumPayloadLength) {
            return PrintUSBQualityBackendError(
                json,
                sequence,
                "backend_protocol_error",
                "The HAL bridge returned an invalid statistics reply.",
                true,
                4);
        }

        uint64_t currentTime = MonotonicMilliseconds();
        uint64_t elapsed = hasPrevious && currentTime >= previousTime ?
            currentTime - previousTime : 0;
        OpenA8DJQualityWindow window =
            CalculateQualityWindow(&previous,
                                   previousLength,
                                   hasPrevious,
                                   &current,
                                   currentLength,
                                   elapsed);
        if (json) {
            PrintQualitySampleJSON(&window, sequence);
        } else {
            PrintQualitySampleHuman(&window, sequence == 1);
        }
        fflush(stdout);
        previous = current;
        previousLength = currentLength;
        previousTime = currentTime;
        hasPrevious = true;
    }
    return 0;
}

static bool PublicSendStateAndReadBack(int fd,
                                       const OpenA8DJControlPayload *expected,
                                       OpenA8DJControlPayload *actual)
{
    return SendIPC(fd, kIPCTypeControlSet, expected, sizeof(*expected)) &&
           ReadState(fd, actual);
}

static int RunPublicAPI(int argc, char **argv)
{
    const char *operation = "unknown";
    if (argc >= 3 && strcmp(argv[2], "version") == 0) operation = "version.get";
    if (argc >= 3 && strcmp(argv[2], "stats") == 0) operation = "stats.get";
    if (argc >= 3 && strcmp(argv[2], "hardware") == 0) operation = "hardware.get";
    if (argc >= 3 && strcmp(argv[2], "profiles") == 0) operation = "profiles.list";
    if (argc >= 3 && strcmp(argv[2], "driver-modes") == 0) operation = "driver_modes.list";
    if (argc >= 3 && strcmp(argv[2], "driver-mode") == 0) {
        if (argc >= 4 && strcmp(argv[3], "set") == 0)
            operation = "driver_mode.set";
        else if (argc >= 4 && strcmp(argv[3], "arm") == 0)
            operation = "driver_mode.arm";
        else if (argc >= 4 && strcmp(argv[3], "disarm") == 0)
            operation = "driver_mode.disarm";
        else
            operation = "driver_mode.get";
    }
    if (argc >= 3 && strcmp(argv[2], "profile") == 0) {
        operation = argc >= 4 && strcmp(argv[3], "set") == 0 ? "profile.set" : "profile.get";
    }

    if (argc == 3 && strcmp(argv[2], "version") == 0) {
        PrintPublicVersion();
        return 0;
    }
    if (argc == 3 && strcmp(argv[2], "profiles") == 0) {
        PrintPublicProfiles();
        return 0;
    }
    if (argc == 3 && strcmp(argv[2], "driver-modes") == 0) {
        PrintPublicDriverModes();
        return 0;
    }

    bool profileRead = argc == 3 && strcmp(argv[2], "profile") == 0;
    bool driverModeRead = argc == 3 && strcmp(argv[2], "driver-mode") == 0;
    bool statsRead = argc == 3 && strcmp(argv[2], "stats") == 0;
    bool hardwareRead = argc == 3 && strcmp(argv[2], "hardware") == 0;
    bool profileWrite = argc == 5 &&
                        strcmp(argv[2], "profile") == 0 &&
                        strcmp(argv[3], "set") == 0;
    bool driverModeWrite = argc == 5 &&
                           strcmp(argv[2], "driver-mode") == 0 &&
                           strcmp(argv[3], "set") == 0;
    bool timecodeArmSyntax = argc >= 4 &&
                             strcmp(argv[2], "driver-mode") == 0 &&
                             strcmp(argv[3], "arm") == 0;
    bool timecodeArm = argc == 7 &&
                       timecodeArmSyntax &&
                       strcmp(argv[4], "timecode-optimized") == 0 &&
                       strcmp(argv[5], "--input-pairs") == 0 &&
                       strcmp(argv[6], "A,B") == 0;
    bool timecodeDisarm = argc == 5 &&
                          strcmp(argv[2], "driver-mode") == 0 &&
                          strcmp(argv[3], "disarm") == 0 &&
                          strcmp(argv[4], "timecode-optimized") == 0;
    if (timecodeArmSyntax && !timecodeArm) {
        return PrintPublicError(
            "driver_mode.arm", "timecode_pair_allowlist_invalid",
            "The MVP allowlist must be exactly --input-pairs A,B.",
            false, 2);
    }
    if (!profileRead && !driverModeRead && !statsRead && !hardwareRead &&
        !profileWrite && !driverModeWrite &&
        !timecodeArm && !timecodeDisarm) {
        return PrintPublicError(operation,
                                "invalid_request",
                                "The public API request has an unknown operation or wrong arity.",
                                false,
                                2);
    }

    const char *requestedProfile = profileWrite ? argv[4] : NULL;
    uint32_t requestedDriverMode = kOpenA8DJDriverModeInvalid;
    if (profileWrite &&
        (strlen(requestedProfile) > 64 || FindCanonicalPreset(requestedProfile) == NULL)) {
        return PrintPublicError("profile.set",
                                "profile_not_allowed",
                                "The requested profile is not in the built-in allowlist.",
                                false,
                                2);
    }
    if (driverModeWrite &&
        (strlen(argv[4]) > 64 || !ParseDriverMode(argv[4], &requestedDriverMode))) {
        return PrintPublicError("driver_mode.set",
                                OPENA8DJ_ERROR_DRIVER_MODE_NOT_ALLOWED,
                                "The requested driver mode is not in the public allowlist.",
                                false,
                                2);
    }
    if (driverModeWrite &&
        requestedDriverMode ==
            kOpenA8DJDriverModeTimecodeOptimized) {
        return PrintPublicError("driver_mode.set",
                                OPENA8DJ_ERROR_DRIVER_MODE_ARM_REQUIRED,
                                "Timecode Optimized requires an explicit A,B arm request.",
                                false,
                                2);
    }

    int lockFD = -1;
    if (profileWrite || driverModeWrite ||
        timecodeArm || timecodeDisarm) {
        lockFD = AcquirePublicMutationLock();
        if (lockFD < 0) {
            return PrintPublicError(operation,
                                    (driverModeWrite || timecodeArm ||
                                     timecodeDisarm) ?
                                        OPENA8DJ_ERROR_DRIVER_MODE_APPLY_FAILED :
                                        "profile_apply_failed",
                                    (driverModeWrite || timecodeArm ||
                                     timecodeDisarm) ?
                                        "The driver-mode mutation lock could not be acquired safely." :
                                        "The profile mutation lock could not be acquired safely.",
                                    true,
                                    5);
        }
    }

    int fd = -1;
    OpenA8DJPublicBackendResult backend = ConnectPublicSocket(&fd);
    if (backend != kPublicBackendOK) {
        if (lockFD >= 0) close(lockFD);
        return PublicBackendError(operation, backend);
    }

    if (statsRead || hardwareRead) {
        OpenA8DJStreamStatsPayload stats;
        size_t payloadLength = 0;
        size_t minimumPayloadLength =
            offsetof(OpenA8DJStreamStatsPayload, sampleRate) + sizeof(stats.sampleRate);
        if (!ReadStreamStats(fd, &stats, &payloadLength) ||
            payloadLength < minimumPayloadLength) {
            close(fd);
            return PrintPublicError(operation,
                                    "backend_protocol_error",
                                    "The HAL bridge returned an invalid statistics reply.",
                                    true,
                                    4);
        }
        OpenA8DJDriverModeStatePayload statsDriverMode;
        OpenA8DJTimecodeStatePayload statsTimecode;
        OpenA8DJVintageStatePayload statsVintage;
        OpenA8DJStatsDriverModeResult modeResult = kStatsDriverModeUnavailable;
        OpenA8DJStatsDriverModeResult timecodeResult =
            kStatsDriverModeUnavailable;
        OpenA8DJStatsDriverModeResult vintageResult =
            kStatsDriverModeUnavailable;
        if (statsRead) {
            modeResult = DriverModeFromStreamStats(&stats,
                                                   payloadLength,
                                                   &statsDriverMode);
            timecodeResult = TimecodeFromStreamStats(
                &stats, payloadLength, &statsTimecode);
            vintageResult = VintageFromStreamStats(
                &stats, payloadLength, &statsVintage);
        }
        if (statsRead &&
            (modeResult == kStatsDriverModeInvalid ||
             timecodeResult == kStatsDriverModeInvalid ||
             vintageResult == kStatsDriverModeInvalid ||
             (vintageResult == kStatsDriverModeValid &&
              modeResult == kStatsDriverModeValid &&
              !DriverModeStatesEqual(
                  &statsDriverMode,
                  &statsVintage.driverMode)))) {
            close(fd);
            return PrintPublicError(operation,
                                    "backend_protocol_error",
                                    "The HAL bridge returned invalid driver-mode statistics.",
                                    true,
                                    4);
        }
        close(fd);
        if (hardwareRead) {
            PrintPublicHardware(&stats, payloadLength);
        } else {
            PrintPublicStats(&stats,
                             payloadLength,
                             modeResult == kStatsDriverModeValid ?
                                 &statsDriverMode : NULL,
                             timecodeResult == kStatsDriverModeValid ?
                                 &statsTimecode : NULL,
                             vintageResult == kStatsDriverModeValid ?
                                 &statsVintage : NULL);
        }
        return 0;
    }

    if (driverModeRead || driverModeWrite) {
        OpenA8DJDriverModeStatePayload driverModeState;
        if (driverModeRead) {
            OpenA8DJTimecodeStatePayload timecodeState;
            OpenA8DJVintageStatePayload vintageState;
            bool hasTimecode = false;
            if (!ReadDriverModeAndTimecode(
                    fd, &driverModeState,
                    &timecodeState, &hasTimecode)) {
                close(fd);
                return PrintPublicError(operation,
                                        "backend_protocol_error",
                                        "The HAL bridge returned an invalid driver-mode reply.",
                                        true,
                                        4);
            }
            OpenA8DJVintageReadResult vintageResult =
                ReadVintageState(fd, &vintageState);
            if (vintageResult == kVintageReadInvalid ||
                (vintageResult == kVintageReadValid &&
                 !DriverModeStatesEqual(
                     &driverModeState,
                     &vintageState.driverMode))) {
                close(fd);
                return PrintPublicError(
                    operation,
                    "backend_protocol_error",
                    "The HAL Vintage Compatible state disagreed with the driver-mode reply.",
                    true,
                    4);
            }
            close(fd);
            PrintPublicDriverMode(
                "driver_mode.get",
                &driverModeState,
                hasTimecode ? &timecodeState : NULL,
                vintageResult == kVintageReadValid ?
                    &vintageState : NULL);
            return 0;
        }
        OpenA8DJDriverModeTransactionResult transaction =
            SetDriverModeAndReadBack(fd, requestedDriverMode, &driverModeState);
        OpenA8DJVintageStatePayload vintageState;
        OpenA8DJVintageReadResult vintageResult =
            ReadVintageState(fd, &vintageState);
        close(fd);
        close(lockFD);
        if (transaction == kDriverModeTransactionConflict) {
            return PrintPublicError("driver_mode.set",
                                    OPENA8DJ_ERROR_DRIVER_MODE_CONFLICT,
                                    "Timecode Optimized and Vintage Compatible cannot be requested together.",
                                    false,
                                    3);
        }
        if (transaction == kDriverModeTransactionApplyFailed) {
            return PrintPublicError("driver_mode.set",
                                    OPENA8DJ_ERROR_DRIVER_MODE_APPLY_FAILED,
                                    "The HAL kept the previous effective driver-mode policy.",
                                    true,
                                    5);
        }
        if (transaction != kDriverModeTransactionOK ||
            vintageResult == kVintageReadInvalid ||
            (requestedDriverMode ==
                 kOpenA8DJDriverModeVintageCompatible &&
             vintageResult != kVintageReadValid) ||
            (vintageResult == kVintageReadValid &&
             !DriverModeStatesEqual(
                 &driverModeState,
                 &vintageState.driverMode))) {
            return PrintPublicError("driver_mode.set",
                                    "backend_protocol_error",
                                    "The HAL driver-mode set/read-back transaction disagreed.",
                                    true,
                                    4);
        }
        PrintPublicDriverMode(
            "driver_mode.set",
            &driverModeState,
            NULL,
            vintageResult == kVintageReadValid ?
                &vintageState : NULL);
        return 0;
    }

    if (timecodeArm || timecodeDisarm) {
        OpenA8DJTimecodeStatePayload timecodeState;
        bool ok = MutateTimecodeAndReadBack(
            fd, timecodeArm, &timecodeState);
        OpenA8DJVintageStatePayload vintageState;
        OpenA8DJVintageReadResult vintageResult =
            ReadVintageState(fd, &vintageState);
        close(fd);
        close(lockFD);
        if (!ok || vintageResult == kVintageReadInvalid ||
            (vintageResult == kVintageReadValid &&
             !DriverModeStatesEqual(
                 &timecodeState.driverMode,
                 &vintageState.driverMode))) {
            return PrintPublicError(
                operation, "backend_protocol_error",
                "The HAL timecode arm/disarm read-back transaction disagreed.",
                true, 4);
        }
        if (timecodeState.rejectionReason ==
            kOpenA8DJTimecodeRejectionModeConflict) {
            return PrintPublicError(
                operation,
                OPENA8DJ_ERROR_DRIVER_MODE_CONFLICT,
                "Timecode Optimized and Vintage Compatible cannot be requested together.",
                false,
                3);
        }
        PrintPublicDriverMode(
            timecodeArm ? "driver_mode.arm" :
                          "driver_mode.disarm",
            &timecodeState.driverMode,
            &timecodeState,
            vintageResult == kVintageReadValid ?
                &vintageState : NULL);
        return 0;
    }

    OpenA8DJControlPayload state;
    if (!ReadState(fd, &state)) {
        close(fd);
        if (lockFD >= 0) close(lockFD);
        return PrintPublicError(operation,
                                "backend_protocol_error",
                                "The HAL bridge returned an invalid control reply.",
                                true,
                                4);
    }
    if (profileRead) {
        close(fd);
        PrintPublicEnvelopePrefix("profile.get", true);
        fputs(",\"data\":{", stdout);
        PrintPublicStateMembers(&state);
        fputs("}}\n", stdout);
        return 0;
    }

    OpenA8DJControlPayload expected = state;
    OpenA8DJControlPayload actual = expected;
    if (!ApplyCanonicalPreset(requestedProfile, &expected) ||
        !PublicSendStateAndReadBack(fd, &expected, &actual) ||
        memcmp(&actual, &expected, sizeof(expected)) != 0) {
        if (fd >= 0) close(fd);
        close(lockFD);
        return PrintPublicError("profile.set",
                                "profile_apply_failed",
                                "The requested profile could not be applied and verified.",
                                true,
                                5);
    }
    close(fd);
    close(lockFD);
    PrintPublicEnvelopePrefix("profile.set", true);
    fputs(",\"data\":{\"requestedProfile\":", stdout);
    PrintJSONString(stdout, requestedProfile);
    fputs(",\"applied\":true,", stdout);
    PrintPublicStateMembers(&actual);
    fputs("}}\n", stdout);
    return 0;
}

static bool ParseBool(const char *text, uint8_t *outValue)
{
    if (strcmp(text, "on") == 0 || strcmp(text, "1") == 0 || strcmp(text, "true") == 0) {
        *outValue = 1;
        return true;
    }
    if (strcmp(text, "off") == 0 || strcmp(text, "0") == 0 || strcmp(text, "false") == 0) {
        *outValue = 0;
        return true;
    }
    return false;
}

static void Usage(const char *argv0)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s\n", argv0);
    fprintf(stderr, "  %s input-stats\n", argv0);
    fprintf(stderr, "  %s stream-stats\n", argv0);
    fprintf(stderr, "    Set OPENA8DJ_CONTROL_NO_WAKE=1 to read without starting Core Audio.\n");
    fprintf(stderr, "  %s api version|stats|hardware|profiles|profile|driver-modes|driver-mode\n",
            argv0);
    fprintf(stderr, "  %s api profile set canonical-profile-id\n", argv0);
    fprintf(stderr, "  %s api driver-mode set balanced|performance\n", argv0);
    fprintf(stderr, "    Driver modes are session-only; performance is experimental.\n");
    fprintf(stderr, "    A change while streaming may be accepted as pending until a safe boundary.\n");
    fprintf(stderr, "  %s usb-quality [--json] [--interval-ms 100..60000] [--count 1..86400]\n",
            argv0);
    fprintf(stderr, "  %s list-profiles\n", argv0);
    fprintf(stderr, "  %s export-config [path]\n", argv0);
    fprintf(stderr, "  %s import-config path\n", argv0);
    fprintf(stderr, "  %s apply-preset name\n", argv0);
    fprintf(stderr, "  %s profile playback|timecode-vinyl|timecode-cd-line|phono|unlock\n", argv0);
    fprintf(stderr, "  %s input-mode 0|1|2|timecode-vinyl|timecode-cd-line|phono\n", argv0);
    fprintf(stderr, "  %s input-decode on|off\n", argv0);
    fprintf(stderr, "  %s gnd-vinyl on|off\n", argv0);
    fprintf(stderr, "  %s gnd-cd-line on|off\n", argv0);
    fprintf(stderr, "  %s gnd-phono on|off\n", argv0);
    fprintf(stderr, "  %s software-lock on|off\n", argv0);
    fprintf(stderr, "  %s input-transform A|B|C|D normal|swap|invert-left|invert-right|invert-both|swap-invert-left|swap-invert-right|swap-invert-both\n", argv0);
    fprintf(stderr, "  %s input-source A|B|C|D A|B|C|D\n", argv0);
}

int main(int argc, char **argv)
{
#ifdef OPENA8DJ_PUBLIC_API_TEST_ESCAPE
    if (argc == 3 && strcmp(argv[1], "--public-api-test-escape") == 0) {
        PrintJSONString(stdout, argv[2]);
        fputc('\n', stdout);
        return 0;
    }
#endif
    if (argc >= 2 && strcmp(argv[1], "api") == 0) {
        (void)signal(SIGPIPE, SIG_IGN);
        return RunPublicAPI(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "usb-quality") == 0) {
        return RunUSBQuality(argc, argv);
    }

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        Usage(argv[0]);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "list-profiles") == 0) {
        PrintProfiles();
        return 0;
    }

    bool allowWake = !EnvFlagEnabled("OPENA8DJ_CONTROL_NO_WAKE");
    int fd = -1;
    OpenA8DJControlPayload state;
    if (!ConnectAndReadState(allowWake, &fd, &state)) {
        fprintf(stderr, "OpenA8DJ HAL bridge is not available at %s\n", kSocketPath);
        return 1;
    }

    if (argc == 1) {
        PrintState(&state);
        close(fd);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "input-stats") == 0) {
        OpenA8DJInputStatsPayload stats;
        if (!ReadStats(fd, &stats)) {
            fprintf(stderr, "Could not read Audio 8 DJ input stats\n");
            close(fd);
            return 1;
        }
        PrintInputStats(&stats);
        close(fd);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "stream-stats") == 0) {
        OpenA8DJStreamStatsPayload stats;
        size_t payloadLength = 0;
        if (!ReadStreamStats(fd, &stats, &payloadLength)) {
            fprintf(stderr, "Could not read OpenA8DJ stream stats\n");
            close(fd);
            return 1;
        }
        PrintStreamStats(&stats, payloadLength);
        close(fd);
        return 0;
    }

    if ((argc == 2 || argc == 3) && strcmp(argv[1], "export-config") == 0) {
        if (argc == 2 || strcmp(argv[2], "-") == 0) {
            ExportConfig(stdout, &state);
        } else if (!WriteConfigFile(argv[2], &state)) {
            fprintf(stderr, "Could not write config to %s\n", argv[2]);
            close(fd);
            return 1;
        }
        close(fd);
        return 0;
    }

    if (argc == 4 && strcmp(argv[1], "input-transform") == 0) {
        int pair = ParseInputPair(argv[2]);
        uint8_t transform = 0;
        if (pair < 0 || !ParseInputTransform(argv[3], &transform)) {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
        SetInputTransform(&state, pair, transform);
    } else if (argc == 4 && strcmp(argv[1], "input-source") == 0) {
        int pair = ParseInputPair(argv[2]);
        int source = ParseInputPair(argv[3]);
        if (pair < 0 || source < 0) {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
        state.inputSource[pair] = (uint8_t)source;
    } else if (argc == 3 && strcmp(argv[1], "import-config") == 0) {
        char *json = ReadTextFile(argv[2]);
        if (json == NULL) {
            fprintf(stderr, "Could not read config from %s\n", argv[2]);
            close(fd);
            return 1;
        }
        bool ok = ApplyConfigJSON(json, &state);
        free(json);
        if (!ok) {
            fprintf(stderr, "Could not parse or apply config from %s\n", argv[2]);
            close(fd);
            return 2;
        }
    } else if (argc != 3) {
        Usage(argv[0]);
        close(fd);
        return 2;
    } else if (strcmp(argv[1], "input-mode") == 0) {
        uint8_t mode = 0;
        if (!ParseInputMode(argv[2], &mode)) {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
        state.inputMode = mode;
    } else if (strcmp(argv[1], "profile") == 0) {
        if (!ApplyProfile(argv[2], &state)) {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
    } else if (strcmp(argv[1], "apply-preset") == 0) {
        if (!ApplyPreset(argv[2], &state)) {
            fprintf(stderr, "Unknown preset: %s\n", argv[2]);
            Usage(argv[0]);
            close(fd);
            return 2;
        }
    } else {
        uint8_t value = 0;
        if (!ParseBool(argv[2], &value)) {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
        if (strcmp(argv[1], "gnd-vinyl") == 0) {
            state.gndLiftTCVinyl = value;
        } else if (strcmp(argv[1], "gnd-cd-line") == 0) {
            state.gndLiftTCCDLine = value;
        } else if (strcmp(argv[1], "gnd-phono") == 0) {
            state.gndLiftPhono = value;
        } else if (strcmp(argv[1], "software-lock") == 0) {
            state.softwareLock = value;
        } else if (strcmp(argv[1], "input-decode") == 0) {
            state.inputDecodeEnabled = value;
        } else {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
    }

    if (!SendStateAndReadBack(&fd, allowWake, &state)) {
        fprintf(stderr, "Could not write Audio 8 DJ controls\n");
        if (fd >= 0) {
            close(fd);
        }
        return 1;
    }
    PrintState(&state);
    close(fd);
    return 0;
}
