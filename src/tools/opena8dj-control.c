#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CoreFoundation.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static const char *kSocketPath = "/tmp/opena8dj-control.sock";

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
    kIPCTypeStreamStats = 11
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
    uint64_t captureTransferPoolFallbackAllocations;
    uint64_t playbackTransferPoolFallbackAllocations;
} __attribute__((packed)) OpenA8DJStreamStatsPayload;

typedef struct OpenA8DJWakeState {
    AudioDeviceID device;
    AudioDeviceIOProcID ioProcID;
} OpenA8DJWakeState;

static OpenA8DJWakeState gWakeState = {
    .device = kAudioObjectUnknown,
    .ioProcID = NULL
};
static bool gWakeCleanupRegistered = false;

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
    if (strcmp(name, "timecode-vinyl") == 0 || strcmp(name, "tc-vinyl") == 0) {
        state->inputMode = 0;
        state->gndLiftTCVinyl = 1;
        state->gndLiftTCCDLine = 0;
        state->gndLiftPhono = 0;
        state->softwareLock = 1;
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
        ResetInputTransforms(state);
        return true;
    }
    if (strcmp(name, "phono") == 0) {
        state->inputMode = 2;
        state->gndLiftTCVinyl = 0;
        state->gndLiftTCCDLine = 0;
        state->gndLiftPhono = 1;
        state->softwareLock = 1;
        ResetInputTransforms(state);
        return true;
    }
    if (strcmp(name, "unlock") == 0) {
        state->softwareLock = 0;
        return true;
    }
    return false;
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

static int ConnectSocketWithWake(void)
{
    int fd = ConnectSocket();
    if (fd >= 0) {
        return fd;
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
    while (true) {
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
        if (header.type == kIPCTypeControlState && header.length >= sizeof(*state)) {
            memcpy(state, payload, sizeof(*state));
            return true;
        }
    }
}

static bool ReadState(int fd, OpenA8DJControlPayload *state)
{
    if (!SendIPC(fd, kIPCTypeControlGet, NULL, 0)) {
        return false;
    }
    if (!ReadOneState(fd, state)) {
        return false;
    }

    while (true) {
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

static bool ReadStats(int fd, OpenA8DJInputStatsPayload *stats)
{
    if (!SendIPC(fd, kIPCTypeInputStatsGet, NULL, 0)) {
        return false;
    }
    while (true) {
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
}

static bool ReadStreamStats(int fd, OpenA8DJStreamStatsPayload *stats, size_t *payloadLength)
{
    if (!SendIPC(fd, kIPCTypeStreamStatsGet, NULL, 0)) {
        return false;
    }
    while (true) {
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
}

static void PrintState(const OpenA8DJControlPayload *state)
{
    printf("Audio 8 DJ controls\n");
    printf("  input-mode:        %u (%s)\n", state->inputMode, InputModeName(state->inputMode));
    printf("  gnd-vinyl:         %s\n", state->gndLiftTCVinyl ? "on" : "off");
    printf("  gnd-cd-line:       %s\n", state->gndLiftTCCDLine ? "on" : "off");
    printf("  gnd-phono:         %s\n", state->gndLiftPhono ? "on" : "off");
    printf("  software-lock:     %s\n", state->softwareLock ? "on" : "off");
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
    if (STREAM_STATS_HAS_FIELD(payloadLength, playbackTransferPoolFallbackAllocations)) {
        printf("  transfer-pool:          capture-fallback-alloc=%llu playback-fallback-alloc=%llu\n",
               (unsigned long long)stats->captureTransferPoolFallbackAllocations,
               (unsigned long long)stats->playbackTransferPoolFallbackAllocations);
    }
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
    printf("captureStatusFailures=%llu\n", (unsigned long long)stats->captureStatusFailures);
    printf("captureZeroCompleteTransactions=%llu\n", (unsigned long long)stats->captureZeroCompleteTransactions);
    printf("captureExpectedTransactions=%llu\n", (unsigned long long)stats->captureExpectedTransactions);
    printf("captureOtherByteCountTransactions=%llu\n", (unsigned long long)stats->captureOtherByteCountTransactions);
    printf("captureShortTransfers=%llu\n", (unsigned long long)stats->captureShortTransfers);
    printf("filteredCaptureTransactions=%llu\n", (unsigned long long)stats->filteredCaptureTransactions);
    printf("playbackTransfersSubmitted=%llu\n",
           (unsigned long long)(STREAM_STATS_HAS_FIELD(payloadLength, playbackQueueAttempts) ?
                                stats->playbackQueueAttempts : stats->playbackTransfers));
    printf("playbackTransfersCompleted=%llu\n", (unsigned long long)stats->playbackTransfers);
    printf("playbackTransferErrors=%llu\n", (unsigned long long)stats->playbackTransactionFailures);
    printf("captureTransferPoolFallbackAllocations=%llu\n",
           (unsigned long long)(STREAM_STATS_HAS_FIELD(payloadLength, playbackTransferPoolFallbackAllocations) ?
                                stats->captureTransferPoolFallbackAllocations : 0));
    printf("playbackTransferPoolFallbackAllocations=%llu\n",
           (unsigned long long)(STREAM_STATS_HAS_FIELD(payloadLength, playbackTransferPoolFallbackAllocations) ?
                                stats->playbackTransferPoolFallbackAllocations : 0));
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
    fprintf(stderr, "  %s profile timecode-vinyl|timecode-cd-line|phono|unlock\n", argv0);
    fprintf(stderr, "  %s input-mode 0|1|2|timecode-vinyl|timecode-cd-line|phono\n", argv0);
    fprintf(stderr, "  %s gnd-vinyl on|off\n", argv0);
    fprintf(stderr, "  %s gnd-cd-line on|off\n", argv0);
    fprintf(stderr, "  %s gnd-phono on|off\n", argv0);
    fprintf(stderr, "  %s software-lock on|off\n", argv0);
    fprintf(stderr, "  %s input-transform A|B|C|D normal|swap|invert-left|invert-right|invert-both|swap-invert-left|swap-invert-right|swap-invert-both\n", argv0);
    fprintf(stderr, "  %s input-source A|B|C|D A|B|C|D\n", argv0);
}

int main(int argc, char **argv)
{
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        Usage(argv[0]);
        return 0;
    }

    int fd = ConnectSocketWithWake();
    if (fd < 0) {
        fprintf(stderr, "OpenA8DJ HAL bridge is not available at %s\n", kSocketPath);
        return 1;
    }

    OpenA8DJControlPayload state;
    if (!ReadState(fd, &state)) {
        fprintf(stderr, "Could not read Audio 8 DJ controls\n");
        close(fd);
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
        } else {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
    }

    if (!SendIPC(fd, kIPCTypeControlSet, &state, sizeof(state)) || !ReadState(fd, &state)) {
        fprintf(stderr, "Could not write Audio 8 DJ controls\n");
        close(fd);
        return 1;
    }
    PrintState(&state);
    close(fd);
    return 0;
}
