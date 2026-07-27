#import "OpenA8DJUSB.h"
#import "OpenA8DJDriverMode.h"
#import "OpenA8DJTimecodeOptimized.h"
#import "OpenA8DJIPCAuth.h"

#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/usb/AppleUSBDefinitions.h>
#import <IOKit/usb/IOUSBHostFamilyDefinitions.h>
#import <IOUSBHost/AppleUSBDescriptorParsing.h>
#import <IOUSBHost/IOUSBHost.h>
#import <CoreMIDI/CoreMIDI.h>
#import <libkern/OSByteOrder.h>
#import <dispatch/dispatch.h>
#import <mach/mach_time.h>

#include <math.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

typedef void (^OpenA8DJIsoCompletionHandler)(IOReturn status,
                                             IOUSBHostIsochronousTransaction *transactionList);

#ifndef OPENA8DJ_ENABLE_TRACE
#define OPENA8DJ_ENABLE_TRACE 0
#endif

#ifndef OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
#define OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE 0
#endif

#ifndef OPENA8DJ_OUTPUT_START_BYTE
#define OPENA8DJ_OUTPUT_START_BYTE (kBytesPerSample + 1)
#endif

#ifndef OPENA8DJ_OUTPUT_GAIN
#define OPENA8DJ_OUTPUT_GAIN 1.0f
#endif

#ifndef OPENA8DJ_OUTPUT_PREFETCH_FRAMES
#define OPENA8DJ_OUTPUT_PREFETCH_FRAMES 256
#endif

#ifndef OPENA8DJ_ENABLE_INPUT_DECODE
#define OPENA8DJ_ENABLE_INPUT_DECODE 1
#endif

#ifndef OPENA8DJ_ENABLE_INPUT_CHECKS
#define OPENA8DJ_ENABLE_INPUT_CHECKS 1
#endif

#ifndef OPENA8DJ_OUTPUT_NATIVE_I24
#define OPENA8DJ_OUTPUT_NATIVE_I24 0
#endif

#ifndef OPENA8DJ_FAST_OUTPUT_QUANTIZER
#define OPENA8DJ_FAST_OUTPUT_QUANTIZER 0
#endif

#ifndef OPENA8DJ_FAST_OUTPUT_PREFETCH_CLEAR
#define OPENA8DJ_FAST_OUTPUT_PREFETCH_CLEAR 0
#endif

#ifndef OPENA8DJ_ISO_FRAMES_PER_TRANSFER
#define OPENA8DJ_ISO_FRAMES_PER_TRANSFER 8
#endif

#ifndef OPENA8DJ_CAPTURE_QUEUE_DEPTH
#define OPENA8DJ_CAPTURE_QUEUE_DEPTH 64
#endif

#ifndef OPENA8DJ_PLAYBACK_QUEUE_TARGET
#define OPENA8DJ_PLAYBACK_QUEUE_TARGET 64
#endif

#ifndef OPENA8DJ_PLAYBACK_CAPTURE_PACED
#define OPENA8DJ_PLAYBACK_CAPTURE_PACED 1
#endif

#ifndef OPENA8DJ_CAPTURE_PACED_OUT_LEAD
#define OPENA8DJ_CAPTURE_PACED_OUT_LEAD 1
#endif

#ifndef OPENA8DJ_PLAYBACK_COALESCE_TRANSFERS
#define OPENA8DJ_PLAYBACK_COALESCE_TRANSFERS 1
#endif

#ifndef OPENA8DJ_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE
#define OPENA8DJ_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE 0
#endif

#ifndef OPENA8DJ_ENABLE_USB_CLOCK_ANCHOR
#define OPENA8DJ_ENABLE_USB_CLOCK_ANCHOR 1
#endif

#ifndef OPENA8DJ_ENABLE_USB_STABLE_FRAME_POLL
#define OPENA8DJ_ENABLE_USB_STABLE_FRAME_POLL 0
#endif

#ifndef OPENA8DJ_USB_ANCHOR_FILTER
#define OPENA8DJ_USB_ANCHOR_FILTER 1
#endif

#ifndef OPENA8DJ_ENABLE_EXPLICIT_ISOC_SCHEDULING
#define OPENA8DJ_ENABLE_EXPLICIT_ISOC_SCHEDULING 0
#endif

#ifndef OPENA8DJ_ENABLE_ELASTIC_OUTPUT
#define OPENA8DJ_ENABLE_ELASTIC_OUTPUT 1
#endif

#ifndef OPENA8DJ_ENABLE_TRANSFER_POOL
#define OPENA8DJ_ENABLE_TRANSFER_POOL 0
#endif

#ifndef OPENA8DJ_FAST_ISO_TRANSFER_CONFIG
#define OPENA8DJ_FAST_ISO_TRANSFER_CONFIG 0
#endif

#ifndef OPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM
#define OPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM 1
#endif

#ifndef OPENA8DJ_STRICT_TRANSFER_POOL
#define OPENA8DJ_STRICT_TRANSFER_POOL 0
#endif

#ifndef OPENA8DJ_TRANSFER_POOL_CURSOR
#define OPENA8DJ_TRANSFER_POOL_CURSOR 0
#endif

#ifndef OPENA8DJ_REUSE_ISOC_COMPLETION_HANDLERS
#define OPENA8DJ_REUSE_ISOC_COMPLETION_HANDLERS 0
#endif

#ifndef OPENA8DJ_ENABLE_LEGACY_OUT_SLOTS
#define OPENA8DJ_ENABLE_LEGACY_OUT_SLOTS 0
#endif

#ifndef OPENA8DJ_USB_QUEUE_QOS
#define OPENA8DJ_USB_QUEUE_QOS 0
#endif

#ifndef OPENA8DJ_ENABLE_OUTPUT_SAMPLE_TIME_FOLLOWER
#define OPENA8DJ_ENABLE_OUTPUT_SAMPLE_TIME_FOLLOWER 1
#endif

#ifndef OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
#define OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC 0
#endif

#ifndef OPENA8DJ_ENABLE_STREAM_KEEPALIVE
#define OPENA8DJ_ENABLE_STREAM_KEEPALIVE 1
#endif

#ifndef OPENA8DJ_ENABLE_OUTPUT_AMPLITUDE_STATS
#define OPENA8DJ_ENABLE_OUTPUT_AMPLITUDE_STATS 0
#endif

#ifndef OPENA8DJ_ENABLE_HOT_STREAM_STATS
#define OPENA8DJ_ENABLE_HOT_STREAM_STATS 1
#endif

#ifndef OPENA8DJ_HOT_STREAM_STATS_INTERVAL
#define OPENA8DJ_HOT_STREAM_STATS_INTERVAL 1
#endif

#ifndef OPENA8DJ_ENABLE_OUTPUT_WRITE_STATS
#define OPENA8DJ_ENABLE_OUTPUT_WRITE_STATS 1
#endif

static const uint16_t kVendorID = 0x17cc;
static const uint16_t kProductID = 0x1978;
static const uint8_t kEndpointControlOut = 0x01;
static const uint8_t kEndpointControlIn = 0x81;
static const uint8_t kEndpointIsoCapture = 0x82;
static const uint8_t kEndpointIsoPlayback = 0x06;
static const uint8_t kCommandGetDeviceInfo = 0x01;
static const uint8_t kCommandReadIO = 0x04;
static const uint8_t kCommandWriteIO = 0x05;
static const uint8_t kCommandAudioParams = 0x09;
static const uint8_t kCommandMidiRead = 0x06;
static const uint8_t kCommandMidiWrite = 0x07;
static const uint8_t kCommandAutoMsg = 0x0b;
static const uint8_t kInterfaceNumber = 0;
static const uint8_t kConfigurationValue = 1;
static const uint8_t kAlternateSetting = 1;
enum {
    kChannels = 8,
    kStreams = 4,
    kChannelsPerStream = 2,
    kBytesPerSample = 3,
    kBytesPerSampleUSB = 4,
    kOutputUSBBytesPerFrame = kStreams * kBytesPerSampleUSB * kChannelsPerStream,
    kIsoFramesPerTransfer = OPENA8DJ_ISO_FRAMES_PER_TRANSFER,
    kPlaybackCoalesceTransfers = OPENA8DJ_PLAYBACK_COALESCE_TRANSFERS < 1 ? 1 : OPENA8DJ_PLAYBACK_COALESCE_TRANSFERS,
    kPlaybackIsoFramesPerTransfer = kIsoFramesPerTransfer * kPlaybackCoalesceTransfers,
    kIsoBytesPerFrame = 512,
    kCaptureQueueDepth = OPENA8DJ_CAPTURE_QUEUE_DEPTH,
    kPlaybackQueueTarget = OPENA8DJ_PLAYBACK_QUEUE_TARGET,
    kPlaybackQueueMax = OPENA8DJ_PLAYBACK_QUEUE_TARGET * 2,
    kCapturePacedOutputLead = OPENA8DJ_CAPTURE_PACED_OUT_LEAD,
    kRingFrames = 32768,
    kOutputPrefetchFrames = OPENA8DJ_OUTPUT_PREFETCH_FRAMES,
    kOutputElasticHighWaterFrames = 24576,
    kOutputReplayHoldFrames = 8,
    kOutputMaxReplayFrames = 192,
    kOutputStatsFlushTransferInterval = 16,
    kOutputSampleTimeJitterToleranceFrames = 1024,
    kPlaybackScheduleLeadFrames = 100,
    kPlaybackScheduleMaxLeadFrames = kPlaybackScheduleLeadFrames + (kPlaybackQueueTarget * kIsoFramesPerTransfer) + 64,
    kPlaybackScheduleFallbackThreshold = 64,
    kClockDriftTolerance = 5,
    kA8DJControlStateBytes = 6
};

enum {
    kInputTransformPairMask = 0x0f
};

static const uint32_t kInputSourceIdentityMap = 0x3210;
static const uint8_t kInputMode2LeftFirstStreamMask = (1u << 1) | (1u << 3);

static const char *kIPCSocketPath = "/tmp/opena8dj-control.sock";

static bool IPCPeerIsAuthorized(int fd)
{
    uid_t peerUID = (uid_t)-1;
    gid_t peerGID = (gid_t)-1;
    if (getpeereid(fd, &peerUID, &peerGID) != 0) {
        return false;
    }
    (void)peerGID;
    struct stat consoleState;
    uid_t consoleUID = (uid_t)-1;
    if (stat("/dev/console", &consoleState) == 0) {
        consoleUID = consoleState.st_uid;
    }
    return OpenA8DJIPCPeerUIDIsAuthorized(peerUID, geteuid(), consoleUID);
}
#if OPENA8DJ_ENABLE_STREAM_KEEPALIVE
static const uint64_t kStreamKeepaliveIntervalNsec = 250000000ull;
static const uint64_t kStreamKeepaliveLeewayNsec = 50000000ull;
#endif
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
enum {
    kDiagnosticCaptureMaxFrames = 1200000,
    kDiagnosticEventMaxCount = 262144
};
static const uint64_t kDiagnosticPackedMaxBytes = (uint64_t)kDiagnosticCaptureMaxFrames * kStreams * kBytesPerSampleUSB;
#endif

enum {
    kIPCVersion = 1,
    kIPCMagic = 0x4a443841, /* "A8DJ" little-endian */
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
    kIPCTypeTimecodeOptimizedState = 18
};

static atomic_bool gInputDecodeEnabledPreference = ATOMIC_VAR_INIT(false);
static atomic_uint gCoreAudioBufferFrames = ATOMIC_VAR_INIT(512);
static atomic_bool gTimecodeClassificationArmed = ATOMIC_VAR_INIT(false);
static char gTimecodeWriterQueueSpecificKey;
static pthread_mutex_t gDriverModeMutex = PTHREAD_MUTEX_INITIALIZER;
static OpenA8DJDriverModeState gDriverModeState = {
    .requestedMode = kOpenA8DJDriverModeBalanced,
    .effectiveMode = kOpenA8DJDriverModeBalanced,
    .lastResult = kOpenA8DJDriverModeResultUnchanged
};
static bool gDriverModeStreaming = false;
static OpenA8DJTimecodeState gTimecodeState = {
    .armState = kOpenA8DJTimecodeDisarmed,
    .fallbackMode = kOpenA8DJDriverModeBalanced
};

static bool DriverModeProductionPreflight(const OpenA8DJDriverModePolicy *policy,
                                          void *context);

static void TimecodeFailOpenLocked(uint8_t reason, bool disarm)
{
    if (!gTimecodeState.armed && !gTimecodeState.optimizedActive) {
        if (disarm) {
            atomic_store(&gTimecodeClassificationArmed, false);
        }
        return;
    }
    uint32_t fallback = gTimecodeState.fallbackMode;
    if (!disarm &&
        gTimecodeState.armState ==
            kOpenA8DJTimecodeDeoptPendingBoundary &&
        gTimecodeState.lastFailOpenReason == reason &&
        gDriverModeState.requestedMode == fallback) {
        return;
    }
    bool wasOptimized = gTimecodeState.optimizedActive ||
                        gDriverModeState.effectiveMode ==
                            kOpenA8DJDriverModeTimecodeOptimized;
    gTimecodeState.qualified = 0;
    gTimecodeState.optimizedActive = 0;
    gTimecodeState.eligibleWindows = 0;
    gTimecodeState.dropoutWindows = 0;
    gTimecodeState.lastFailOpenReason = reason;
    if (wasOptimized) {
        gTimecodeState.counters.deoptimizations++;
    }
    switch (reason) {
        case kOpenA8DJTimecodeFailOutsideAllowlist:
            gTimecodeState.counters.outsideAllowlistTrips++;
            break;
        case kOpenA8DJTimecodeFailStatsMissing:
        case kOpenA8DJTimecodeFailStatsInvalid:
            gTimecodeState.counters.missingEvidenceTrips++;
            break;
        case kOpenA8DJTimecodeFailWrongProfile:
            gTimecodeState.counters.profileTrips++;
            break;
        case kOpenA8DJTimecodeFailConfigurationChanged:
        case kOpenA8DJTimecodeFailInputLeadViolation:
            gTimecodeState.counters.configurationTrips++;
            break;
        case kOpenA8DJTimecodeFailXRunOrTransportError:
            gTimecodeState.counters.xrunErrorTrips++;
            break;
        default:
            break;
    }
    if (disarm) {
        atomic_store(&gTimecodeClassificationArmed, false);
        OpenA8DJTimecodeDisarm(&gTimecodeState, reason);
    } else {
        gTimecodeState.armState = kOpenA8DJTimecodeDeoptPendingBoundary;
        gTimecodeState.generation++;
    }
    (void)OpenA8DJDriverModeSet(&gDriverModeState,
                                fallback,
                                gDriverModeStreaming,
                                DriverModeProductionPreflight,
                                NULL);
    if (!gDriverModeState.pending &&
        gDriverModeState.effectiveMode == fallback &&
        !disarm) {
        gTimecodeState.armState = kOpenA8DJTimecodeQualifying;
    }
}

static void TimecodeMarkActivatedLocked(void)
{
    if (!gTimecodeState.optimizedActive) {
        gTimecodeState.optimizedActive = 1;
        gTimecodeState.counters.activations++;
        gTimecodeState.generation++;
    }
    gTimecodeState.armState = kOpenA8DJTimecodeActive;
}

static bool DriverModeProductionPreflight(const OpenA8DJDriverModePolicy *policy,
                                          void *context)
{
    (void)context;
    if (!OpenA8DJDriverModePolicyIsSafe(policy, kRingFrames)) {
        return false;
    }
    if (policy->timecodeEvidenceRequired) {
        uint32_t bufferFrames = atomic_load(&gCoreAudioBufferFrames);
        return gTimecodeState.armed && gTimecodeState.qualified &&
               gTimecodeState.profileVerified &&
               gTimecodeState.allowedInputPairMask ==
                   kOpenA8DJTimecodeAllowedPairMask &&
               gTimecodeState.sampleRateSnapshot > 0.0 &&
               gTimecodeState.bufferFramesSnapshot == bufferFrames &&
               policy->inputLeadCeilingFrames >= bufferFrames * 4 &&
               policy->inputLeadCeilingFrames < kRingFrames;
    }
    return true;
}

static OpenA8DJDriverModePolicy DriverModeBeginStream(void)
{
    OpenA8DJDriverModePolicy policy;
    pthread_mutex_lock(&gDriverModeMutex);
    (void)OpenA8DJDriverModePromotePending(&gDriverModeState,
                                           DriverModeProductionPreflight,
                                           NULL);
    (void)OpenA8DJDriverModeLookup(gDriverModeState.effectiveMode, &policy);
    if (gDriverModeState.effectiveMode ==
        kOpenA8DJDriverModeTimecodeOptimized) {
        TimecodeMarkActivatedLocked();
    } else if (gTimecodeState.armed &&
               gTimecodeState.armState ==
                   kOpenA8DJTimecodeDeoptPendingBoundary) {
        gTimecodeState.armState =
            gTimecodeState.profileVerified ?
                kOpenA8DJTimecodeQualifying :
                kOpenA8DJTimecodeWaitingProfile;
    }
    gDriverModeStreaming = true;
    pthread_mutex_unlock(&gDriverModeMutex);
    return policy;
}

static void DriverModeEndStream(void)
{
    pthread_mutex_lock(&gDriverModeMutex);
    gDriverModeStreaming = false;
    (void)OpenA8DJDriverModePromotePending(&gDriverModeState,
                                           DriverModeProductionPreflight,
                                           NULL);
    if (gDriverModeState.effectiveMode ==
        kOpenA8DJDriverModeTimecodeOptimized) {
        TimecodeMarkActivatedLocked();
    } else if (gTimecodeState.armed) {
        gTimecodeState.optimizedActive = 0;
        gTimecodeState.armState =
            gTimecodeState.profileVerified ?
                kOpenA8DJTimecodeQualifying :
                kOpenA8DJTimecodeWaitingProfile;
    }
    pthread_mutex_unlock(&gDriverModeMutex);
}

static OpenA8DJDriverModeStatePayload DriverModeStateSnapshot(void)
{
    OpenA8DJDriverModeStatePayload payload;
    pthread_mutex_lock(&gDriverModeMutex);
    OpenA8DJDriverModeMakeStatePayload(&gDriverModeState,
                                       gDriverModeStreaming,
                                       &payload);
    pthread_mutex_unlock(&gDriverModeMutex);
    return payload;
}

static OpenA8DJDriverModeStatePayload DriverModeRejectRequest(uint8_t rejection)
{
    OpenA8DJDriverModeStatePayload payload;
    pthread_mutex_lock(&gDriverModeMutex);
    OpenA8DJDriverModeReject(&gDriverModeState, rejection);
    OpenA8DJDriverModeMakeStatePayload(&gDriverModeState,
                                       gDriverModeStreaming,
                                       &payload);
    pthread_mutex_unlock(&gDriverModeMutex);
    return payload;
}

static OpenA8DJDriverModeStatePayload DriverModeSetRequested(uint32_t modeID)
{
    OpenA8DJDriverModeStatePayload payload;
    pthread_mutex_lock(&gDriverModeMutex);
    if (modeID == kOpenA8DJDriverModeBalanced ||
        modeID == kOpenA8DJDriverModePerformance) {
        atomic_store(&gTimecodeClassificationArmed, false);
        OpenA8DJTimecodeDisarm(&gTimecodeState,
                               kOpenA8DJTimecodeFailExplicitDisarm);
    }
    (void)OpenA8DJDriverModeSet(&gDriverModeState,
                                modeID,
                                gDriverModeStreaming,
                                DriverModeProductionPreflight,
                                NULL);
    OpenA8DJDriverModeMakeStatePayload(&gDriverModeState,
                                       gDriverModeStreaming,
                                       &payload);
    pthread_mutex_unlock(&gDriverModeMutex);
    return payload;
}

static dispatch_queue_attr_t OpenA8DJUSBQueueAttributes(void)
{
#if OPENA8DJ_USB_QUEUE_QOS >= 2
    return dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,
                                                   QOS_CLASS_USER_INTERACTIVE,
                                                   0);
#elif OPENA8DJ_USB_QUEUE_QOS == 1
    return dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,
                                                   QOS_CLASS_USER_INITIATED,
                                                   0);
#else
    return DISPATCH_QUEUE_SERIAL;
#endif
}

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
    uint8_t inputSource[kStreams];
    uint8_t inputDecodeEnabled;
} __attribute__((packed)) OpenA8DJControlPayload;

static uint8_t TimecodeProfileForControl(
    const OpenA8DJControlPayload *control)
{
    if (control == NULL) {
        return kOpenA8DJTimecodeProfileUnavailable;
    }
    return OpenA8DJTimecodeProfileForElectricalState(
        control->inputMode,
        control->gndLiftTCVinyl,
        control->gndLiftTCCDLine,
        control->gndLiftPhono,
        control->softwareLock,
        control->inputDecodeEnabled,
        control->inputSwapMask,
        control->inputInvertLeftMask,
        control->inputInvertRightMask,
        control->inputSource);
}

typedef struct OpenA8DJInputStatsPayload {
    uint64_t frames[kStreams];
    double leftSquare[kStreams];
    double rightSquare[kStreams];
    double cross[kStreams];
    double leftPeak[kStreams];
    double rightPeak[kStreams];
} __attribute__((packed)) OpenA8DJInputStatsPayload;

static void InputStatsAccumulate(OpenA8DJInputStatsPayload *stats,
                                 uint32_t stream,
                                 float left,
                                 float right)
{
    if (stats == NULL || stream >= kStreams) {
        return;
    }
    double l = left;
    double r = right;
    double la = fabs(l);
    double ra = fabs(r);
    stats->frames[stream]++;
    stats->leftSquare[stream] += l * l;
    stats->rightSquare[stream] += r * r;
    stats->cross[stream] += l * r;
    if (la > stats->leftPeak[stream]) {
        stats->leftPeak[stream] = la;
    }
    if (ra > stats->rightPeak[stream]) {
        stats->rightPeak[stream] = ra;
    }
}

static bool InputStatsHasFrames(const OpenA8DJInputStatsPayload *stats)
{
    if (stats == NULL) {
        return false;
    }
    for (uint32_t stream = 0; stream < kStreams; stream++) {
        if (stats->frames[stream] != 0) {
            return true;
        }
    }
    return false;
}

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
} __attribute__((packed)) OpenA8DJStreamStatsPayload;
_Static_assert(sizeof(OpenA8DJStreamStatsPayload) <= 4096,
               "stream stats must fit the bounded IPC payload");

typedef struct OpenA8DJOutputFillStats {
    uint64_t framesRead;
    uint64_t underruns;
    uint64_t activeUnderruns;
    uint64_t startupSilenceFrames;
    uint64_t elasticDrops;
    uint64_t elasticReplays;
    double peak;
    uint64_t nearClipSamples;
    uint64_t clippedSamples;
} OpenA8DJOutputFillStats;

#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
typedef struct OpenA8DJDiagnosticEvent {
    uint64_t hostTime;
    int64_t frameNumber;
    uint32_t count;
    uint16_t type;
    uint16_t flags;
    uint32_t value;
    uint32_t extra;
} __attribute__((packed)) OpenA8DJDiagnosticEvent;

enum {
    kDiagnosticEventWrite = 1,
    kDiagnosticEventReadAnomaly = 2,
    kDiagnosticEventPackedTransfer = 3
};

enum {
    kDiagnosticFlagSampleTimeValid = 1u << 0,
    kDiagnosticFlagTimelineReset = 1u << 1,
    kDiagnosticFlagDropped = 1u << 2,
    kDiagnosticFlagHaveFrame = 1u << 3,
    kDiagnosticFlagStartupSilence = 1u << 4,
    kDiagnosticFlagReplayedFrame = 1u << 5,
    kDiagnosticFlagExplicitSchedule = 1u << 6
};

static const char *DiagnosticEventName(uint16_t type)
{
    switch (type) {
        case kDiagnosticEventWrite:
            return "write";
        case kDiagnosticEventReadAnomaly:
            return "read-anomaly";
        case kDiagnosticEventPackedTransfer:
            return "packed-transfer";
        default:
            return "unknown";
    }
}

static const char *DiagnosticEventTimelineName(const OpenA8DJDiagnosticEvent *event)
{
    if (event == NULL) {
        return "unknown";
    }
    switch (event->type) {
        case kDiagnosticEventWrite:
            return "core-audio-sample-time";
        case kDiagnosticEventReadAnomaly:
            return "output-served-frame";
        case kDiagnosticEventPackedTransfer:
            return (event->flags & kDiagnosticFlagExplicitSchedule) ?
                "usb-frame-number" : "unscheduled-transfer";
        default:
            return "unknown";
    }
}
#endif

typedef struct CaiaqDeviceSpec {
    uint16_t fwVersion;
    uint8_t hwSubtype;
    uint8_t numErp;
    uint8_t numAnalogIn;
    uint8_t numDigitalIn;
    uint8_t numDigitalOut;
    uint8_t numAnalogAudioOut;
    uint8_t numAnalogAudioIn;
    uint8_t numDigitalAudioOut;
    uint8_t numDigitalAudioIn;
    uint8_t numMidiOut;
    uint8_t numMidiIn;
    uint8_t dataAlignment;
} __attribute__((packed)) CaiaqDeviceSpec;

typedef struct FloatRing {
    pthread_mutex_t mutex;
    float *data;
    uint32_t capacityFrames;
    uint32_t channels;
    uint32_t readFrame;
    uint32_t writeFrame;
    uint32_t availableFrames;
} FloatRing;

typedef struct OutputTimelineRing {
    pthread_mutex_t mutex;
    float *data;
    int64_t *frameNumbers;
    uint32_t capacityFrames;
    uint32_t channels;
    int64_t readFrame;
    int64_t firstWrittenFrame;
    int64_t maxWrittenFrame;
    bool hasWritten;
} OutputTimelineRing;

static void USBTrace(const char *format, ...)
{
#if OPENA8DJ_ENABLE_TRACE
    FILE *file = fopen("/tmp/opena8dj-usb.log", "a");
    if (file == NULL) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fputc('\n', file);
    fclose(file);
#else
    (void)format;
#endif
}

static const char *NSErrorText(NSError *error)
{
    const char *text = error.localizedDescription.UTF8String;
    return text != NULL ? text : "";
}

static double MachTicksPerSecond(void)
{
    static mach_timebase_info_data_t timebase;
    if (timebase.denom == 0) {
        (void)mach_timebase_info(&timebase);
    }
    if (timebase.numer == 0) {
        return 0.0;
    }
    return 1000000000.0 * (double)timebase.denom / (double)timebase.numer;
}

static uint64_t ExpectedIsoTransferTicks(void)
{
    double ticksPerSecond = MachTicksPerSecond();
    double ticks = (ticksPerSecond * (double)kIsoFramesPerTransfer) / 8000.0;
    return ticks > 0.0 ? (uint64_t)llround(ticks) : 0;
}

static uint64_t MachTicksForNanoseconds(uint64_t nanoseconds)
{
    mach_timebase_info_data_t timebase = {0};
    if (mach_timebase_info(&timebase) != KERN_SUCCESS ||
        timebase.numer == 0 ||
        timebase.denom == 0 ||
        nanoseconds > (UINT64_MAX - (uint64_t)timebase.numer + 1) /
                      (uint64_t)timebase.denom) {
        return 0;
    }
    uint64_t scaled = nanoseconds * (uint64_t)timebase.denom;
    return (scaled + (uint64_t)timebase.numer - 1) / (uint64_t)timebase.numer;
}

typedef struct OpenA8DJCompletionQualityBatch {
    uint64_t jitterSamples;
    uint64_t jitterInvalidIntervals;
    uint64_t jitterBins[6];
    uint64_t completionStatusFailures;
    uint64_t transactionStatusFailures;
    uint64_t zeroLengthTransactions;
    uint64_t shortTransactions;
} OpenA8DJCompletionQualityBatch;

#if OPENA8DJ_ENABLE_HOT_STREAM_STATS
static void CompletionQualityRecordJitter(
    OpenA8DJCompletionQualityBatch *batch,
    bool invalidInterval,
    uint64_t completionDelta,
    uint64_t transactionCount,
    uint64_t ticksPerUSBMicroframe,
    const uint64_t binThresholdTicks[5])
{
    if (invalidInterval || ticksPerUSBMicroframe == 0) {
        batch->jitterInvalidIntervals++;
        return;
    }

    uint64_t nominalTicks =
        transactionCount <= UINT64_MAX / ticksPerUSBMicroframe ?
            transactionCount * ticksPerUSBMicroframe :
            UINT64_MAX;
    uint64_t jitterTicks = completionDelta >= nominalTicks ?
        completionDelta - nominalTicks :
        nominalTicks - completionDelta;

    batch->jitterSamples++;
    if (jitterTicks <= binThresholdTicks[0]) {
        batch->jitterBins[0]++;
    } else if (jitterTicks <= binThresholdTicks[1]) {
        batch->jitterBins[1]++;
    } else if (jitterTicks <= binThresholdTicks[2]) {
        batch->jitterBins[2]++;
    } else if (jitterTicks <= binThresholdTicks[3]) {
        batch->jitterBins[3]++;
    } else if (jitterTicks <= binThresholdTicks[4]) {
        batch->jitterBins[4]++;
    } else {
        batch->jitterBins[5]++;
    }
}
#endif

static void StreamStatsFlushCompletionQualityLocked(
    OpenA8DJStreamStatsPayload *stats,
    OpenA8DJCompletionQualityBatch *batch,
    bool capture)
{
    if (capture) {
        stats->captureCompletionJitterSamples += batch->jitterSamples;
        stats->captureCompletionJitterInvalidIntervals += batch->jitterInvalidIntervals;
        stats->captureCompletionJitterLe50 += batch->jitterBins[0];
        stats->captureCompletionJitterLe100 += batch->jitterBins[1];
        stats->captureCompletionJitterLe250 += batch->jitterBins[2];
        stats->captureCompletionJitterLe500 += batch->jitterBins[3];
        stats->captureCompletionJitterLe1000 += batch->jitterBins[4];
        stats->captureCompletionJitterGt1000 += batch->jitterBins[5];
        stats->captureISOCompletionStatusFailures += batch->completionStatusFailures;
        stats->captureISOTransactionStatusFailures += batch->transactionStatusFailures;
        stats->captureISOZeroLengthTransactions += batch->zeroLengthTransactions;
        stats->captureISOShortTransactions += batch->shortTransactions;
    } else {
        stats->playbackCompletionJitterSamples += batch->jitterSamples;
        stats->playbackCompletionJitterInvalidIntervals += batch->jitterInvalidIntervals;
        stats->playbackCompletionJitterLe50 += batch->jitterBins[0];
        stats->playbackCompletionJitterLe100 += batch->jitterBins[1];
        stats->playbackCompletionJitterLe250 += batch->jitterBins[2];
        stats->playbackCompletionJitterLe500 += batch->jitterBins[3];
        stats->playbackCompletionJitterLe1000 += batch->jitterBins[4];
        stats->playbackCompletionJitterGt1000 += batch->jitterBins[5];
        stats->playbackISOCompletionStatusFailures += batch->completionStatusFailures;
        stats->playbackISOTransactionStatusFailures += batch->transactionStatusFailures;
        stats->playbackISOZeroLengthTransactions += batch->zeroLengthTransactions;
        stats->playbackISOShortTransactions += batch->shortTransactions;
    }
    memset(batch, 0, sizeof(*batch));
}

#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
static uint64_t CadenceOutlierThresholdTicks(void)
{
    uint64_t expected = ExpectedIsoTransferTicks();
    return expected > 0 ? expected * 2 : 0;
}

typedef struct OpenA8DJCadenceRange {
    atomic_uint_fast64_t min;
    atomic_uint_fast64_t max;
    atomic_uint_fast64_t sum;
    atomic_uint_fast64_t samples;
} OpenA8DJCadenceRange;

typedef struct OpenA8DJCadenceRangeSnapshot {
    uint64_t min;
    uint64_t max;
    uint64_t sum;
    uint64_t samples;
} OpenA8DJCadenceRangeSnapshot;

typedef struct OpenA8DJCadenceDiagnostics {
    OpenA8DJCadenceRange captureUSBTimestampDelta;
    OpenA8DJCadenceRange captureRequestCount;
    OpenA8DJCadenceRange captureCompleteCount;
    OpenA8DJCadenceRange captureOffset;
    OpenA8DJCadenceRange playbackQueueBytes;
    OpenA8DJCadenceRange playbackQueueTransactions;
    OpenA8DJCadenceRange playbackQueueRequestCount;
    OpenA8DJCadenceRange playbackInFlightAtQueue;
    OpenA8DJCadenceRange playbackInFlightAtCompletion;
    OpenA8DJCadenceRange playbackCompleteCount;
    OpenA8DJCadenceRange playbackRequestCount;
    atomic_uint_fast64_t captureCompletionDeltaOutliers;
    atomic_uint_fast64_t captureCompletionDeltaOutlierMax;
    atomic_uint_fast64_t captureCompletionDeltaOutlierSum;
    atomic_uint_fast64_t playbackCompletionDeltaOutliers;
    atomic_uint_fast64_t playbackCompletionDeltaOutlierMax;
    atomic_uint_fast64_t playbackCompletionDeltaOutlierSum;
    atomic_uint_fast64_t captureToPlaybackQueueDeltaOutliers;
    atomic_uint_fast64_t captureToPlaybackQueueDeltaOutlierMax;
    atomic_uint_fast64_t captureToPlaybackQueueDeltaOutlierSum;
    atomic_uint_fast64_t captureUSBTimestampOutOfOrder;
    atomic_uint_fast64_t captureUSBTimestampRepeated;
    atomic_uint_fast64_t captureUSBTimestampZero;
    atomic_uint_fast64_t captureLayoutSignatureSum;
    atomic_uint_fast64_t playbackQueueAttempts;
    atomic_uint_fast64_t playbackQueueLayoutSignatureSum;
    atomic_uint_fast64_t playbackZeroCompleteTransactions;
    atomic_uint_fast64_t playbackLayoutSignatureSum;
} OpenA8DJCadenceDiagnostics;

static void CadenceResetRange(OpenA8DJCadenceRange *range)
{
    atomic_store(&range->min, 0);
    atomic_store(&range->max, 0);
    atomic_store(&range->sum, 0);
    atomic_store(&range->samples, 0);
}

static void CadenceReset(OpenA8DJCadenceDiagnostics *diagnostics)
{
    CadenceResetRange(&diagnostics->captureUSBTimestampDelta);
    CadenceResetRange(&diagnostics->captureRequestCount);
    CadenceResetRange(&diagnostics->captureCompleteCount);
    CadenceResetRange(&diagnostics->captureOffset);
    CadenceResetRange(&diagnostics->playbackQueueBytes);
    CadenceResetRange(&diagnostics->playbackQueueTransactions);
    CadenceResetRange(&diagnostics->playbackQueueRequestCount);
    CadenceResetRange(&diagnostics->playbackInFlightAtQueue);
    CadenceResetRange(&diagnostics->playbackInFlightAtCompletion);
    CadenceResetRange(&diagnostics->playbackCompleteCount);
    CadenceResetRange(&diagnostics->playbackRequestCount);
    atomic_store(&diagnostics->captureCompletionDeltaOutliers, 0);
    atomic_store(&diagnostics->captureCompletionDeltaOutlierMax, 0);
    atomic_store(&diagnostics->captureCompletionDeltaOutlierSum, 0);
    atomic_store(&diagnostics->playbackCompletionDeltaOutliers, 0);
    atomic_store(&diagnostics->playbackCompletionDeltaOutlierMax, 0);
    atomic_store(&diagnostics->playbackCompletionDeltaOutlierSum, 0);
    atomic_store(&diagnostics->captureToPlaybackQueueDeltaOutliers, 0);
    atomic_store(&diagnostics->captureToPlaybackQueueDeltaOutlierMax, 0);
    atomic_store(&diagnostics->captureToPlaybackQueueDeltaOutlierSum, 0);
    atomic_store(&diagnostics->captureUSBTimestampOutOfOrder, 0);
    atomic_store(&diagnostics->captureUSBTimestampRepeated, 0);
    atomic_store(&diagnostics->captureUSBTimestampZero, 0);
    atomic_store(&diagnostics->captureLayoutSignatureSum, 0);
    atomic_store(&diagnostics->playbackQueueAttempts, 0);
    atomic_store(&diagnostics->playbackQueueLayoutSignatureSum, 0);
    atomic_store(&diagnostics->playbackZeroCompleteTransactions, 0);
    atomic_store(&diagnostics->playbackLayoutSignatureSum, 0);
}

static void CadenceRecordRange(OpenA8DJCadenceRange *range, uint64_t value)
{
    uint_fast64_t currentMin = atomic_load(&range->min);
    while ((currentMin == 0 || value < currentMin) &&
           !atomic_compare_exchange_weak(&range->min, &currentMin, value)) {
    }
    uint_fast64_t currentMax = atomic_load(&range->max);
    while (value > currentMax &&
           !atomic_compare_exchange_weak(&range->max, &currentMax, value)) {
    }
    atomic_fetch_add(&range->sum, value);
    atomic_fetch_add(&range->samples, 1);
}

static OpenA8DJCadenceRangeSnapshot CadenceSnapshotRange(OpenA8DJCadenceRange *range)
{
    OpenA8DJCadenceRangeSnapshot snapshot;
    snapshot.min = atomic_load(&range->min);
    snapshot.max = atomic_load(&range->max);
    snapshot.sum = atomic_load(&range->sum);
    snapshot.samples = atomic_load(&range->samples);
    return snapshot;
}

static void CadenceRecordOutlier(atomic_uint_fast64_t *count,
                                 atomic_uint_fast64_t *maxValue,
                                 atomic_uint_fast64_t *sumValue,
                                 uint64_t delta)
{
    uint64_t threshold = CadenceOutlierThresholdTicks();
    if (threshold == 0 || delta <= threshold) {
        return;
    }
    atomic_fetch_add(count, 1);
    atomic_fetch_add(sumValue, delta);
    uint_fast64_t currentMax = atomic_load(maxValue);
    while (delta > currentMax &&
           !atomic_compare_exchange_weak(maxValue, &currentMax, delta)) {
    }
}

static uint64_t LayoutSignatureStep(uint64_t signature, uint64_t a, uint64_t b, uint64_t c)
{
    signature ^= a + 0x9e3779b97f4a7c15ULL + (signature << 6) + (signature >> 2);
    signature ^= b + 0x9e3779b97f4a7c15ULL + (signature << 6) + (signature >> 2);
    signature ^= c + 0x9e3779b97f4a7c15ULL + (signature << 6) + (signature >> 2);
    return signature;
}
#endif

#if OPENA8DJ_ENABLE_USB_CLOCK_ANCHOR
static double MachTicksPerFrame(double sampleRate)
{
    double ticksPerSecond = MachTicksPerSecond();
    if (sampleRate <= 0.0 || ticksPerSecond <= 0.0) {
        return 0.0;
    }
    return ticksPerSecond / sampleRate;
}

static bool HostTimeLooksLikeMachTime(uint64_t hostTime)
{
    double ticksPerSecond = MachTicksPerSecond();
    if (hostTime == 0 || ticksPerSecond <= 0.0) {
        return false;
    }
    uint64_t now = mach_absolute_time();
    uint64_t delta = hostTime > now ? hostTime - now : now - hostTime;
    return (double)delta < ticksPerSecond * 5.0;
}

static uint64_t JitterFilter(uint64_t weight, uint64_t previous, uint64_t current)
{
    if (weight == 0) {
        return current;
    }
    return (current + ((weight - 1) * previous) + (weight / 2)) / weight;
}
#endif

static void RingInit(FloatRing *ring, uint32_t capacityFrames, uint32_t channels)
{
    memset(ring, 0, sizeof(*ring));
    pthread_mutex_init(&ring->mutex, NULL);
    ring->capacityFrames = capacityFrames;
    ring->channels = channels;
    ring->data = calloc((size_t)capacityFrames * channels, sizeof(float));
}

static void RingDestroy(FloatRing *ring)
{
    pthread_mutex_destroy(&ring->mutex);
    free(ring->data);
    memset(ring, 0, sizeof(*ring));
}

static void RingClear(FloatRing *ring)
{
    pthread_mutex_lock(&ring->mutex);
    ring->readFrame = 0;
    ring->writeFrame = 0;
    ring->availableFrames = 0;
    if (ring->data != NULL) {
        memset(ring->data, 0, (size_t)ring->capacityFrames * ring->channels * sizeof(float));
    }
    pthread_mutex_unlock(&ring->mutex);
}

#if !OPENA8DJ_ENABLE_ELASTIC_OUTPUT
static void RingTrimToLatest(FloatRing *ring, uint32_t maxFrames)
{
    pthread_mutex_lock(&ring->mutex);
    if (ring->availableFrames > maxFrames) {
        uint32_t dropFrames = ring->availableFrames - maxFrames;
        ring->readFrame = (ring->readFrame + dropFrames) % ring->capacityFrames;
        ring->availableFrames = maxFrames;
    }
    pthread_mutex_unlock(&ring->mutex);
}
#endif

static uint32_t RingWriteWithDropped(FloatRing *ring,
                                     const float *frames,
                                     uint32_t frameCount,
                                     uint32_t *droppedFrames)
{
    if (ring->data == NULL || frames == NULL || frameCount == 0) {
        return 0;
    }
    pthread_mutex_lock(&ring->mutex);
    uint32_t written = 0;
    uint32_t dropped = 0;
    for (; written < frameCount; written++) {
        if (ring->availableFrames == ring->capacityFrames) {
            ring->readFrame = (ring->readFrame + 1) % ring->capacityFrames;
            ring->availableFrames--;
            dropped++;
        }
        memcpy(&ring->data[(size_t)ring->writeFrame * ring->channels],
               &frames[(size_t)written * ring->channels],
               (size_t)ring->channels * sizeof(float));
        ring->writeFrame = (ring->writeFrame + 1) % ring->capacityFrames;
        ring->availableFrames++;
    }
    pthread_mutex_unlock(&ring->mutex);
    if (droppedFrames != NULL) {
        *droppedFrames = dropped;
    }
    return written;
}

static uint32_t RingRead(FloatRing *ring, float *frames, uint32_t frameCount, bool zeroFill)
{
    if (frames == NULL || frameCount == 0) {
        return 0;
    }
    uint32_t read = 0;
    pthread_mutex_lock(&ring->mutex);
    for (; read < frameCount && ring->availableFrames > 0; read++) {
        memcpy(&frames[(size_t)read * ring->channels],
               &ring->data[(size_t)ring->readFrame * ring->channels],
               (size_t)ring->channels * sizeof(float));
        ring->readFrame = (ring->readFrame + 1) % ring->capacityFrames;
        ring->availableFrames--;
    }
    pthread_mutex_unlock(&ring->mutex);
    if (zeroFill && read < frameCount) {
        memset(&frames[(size_t)read * ring->channels],
               0,
               (size_t)(frameCount - read) * ring->channels * sizeof(float));
    }
    return read;
}

static uint32_t RingAvailable(FloatRing *ring)
{
    pthread_mutex_lock(&ring->mutex);
    uint32_t available = ring->availableFrames;
    pthread_mutex_unlock(&ring->mutex);
    return available;
}

static uint32_t TimelineIndexForFrame(int64_t frameNumber, uint32_t capacityFrames)
{
    int64_t index = frameNumber % (int64_t)capacityFrames;
    if (index < 0) {
        index += capacityFrames;
    }
    return (uint32_t)index;
}

static void OutputTimelineInit(OutputTimelineRing *ring, uint32_t capacityFrames, uint32_t channels)
{
    memset(ring, 0, sizeof(*ring));
    pthread_mutex_init(&ring->mutex, NULL);
    ring->capacityFrames = capacityFrames;
    ring->channels = channels;
    ring->data = calloc((size_t)capacityFrames * channels, sizeof(float));
    ring->frameNumbers = malloc((size_t)capacityFrames * sizeof(int64_t));
    if (ring->frameNumbers != NULL) {
        for (uint32_t i = 0; i < capacityFrames; i++) {
            ring->frameNumbers[i] = INT64_MIN;
        }
    }
}

static void OutputTimelineDestroy(OutputTimelineRing *ring)
{
    pthread_mutex_destroy(&ring->mutex);
    free(ring->data);
    free(ring->frameNumbers);
    memset(ring, 0, sizeof(*ring));
}

static void OutputTimelineInvalidateLocked(OutputTimelineRing *ring)
{
    if (ring->data != NULL) {
        memset(ring->data, 0, (size_t)ring->capacityFrames * ring->channels * sizeof(float));
    }
    if (ring->frameNumbers != NULL) {
        for (uint32_t i = 0; i < ring->capacityFrames; i++) {
            ring->frameNumbers[i] = INT64_MIN;
        }
    }
}

static void OutputTimelineClear(OutputTimelineRing *ring)
{
    pthread_mutex_lock(&ring->mutex);
    ring->readFrame = 0;
    ring->firstWrittenFrame = 0;
    ring->maxWrittenFrame = 0;
    ring->hasWritten = false;
    OutputTimelineInvalidateLocked(ring);
    pthread_mutex_unlock(&ring->mutex);
}

static void OutputTimelineResetLocked(OutputTimelineRing *ring, int64_t startFrame, uint32_t prerollFrames)
{
    OutputTimelineInvalidateLocked(ring);
    ring->readFrame = startFrame - (int64_t)prerollFrames;
    ring->firstWrittenFrame = startFrame;
    ring->maxWrittenFrame = startFrame - 1;
    ring->hasWritten = true;
}

static uint32_t OutputTimelineAvailable(OutputTimelineRing *ring)
{
    uint32_t available = 0;
    pthread_mutex_lock(&ring->mutex);
    if (ring->hasWritten && ring->maxWrittenFrame >= ring->readFrame) {
        int64_t frames = ring->maxWrittenFrame - ring->readFrame + 1;
        available = frames > (int64_t)ring->capacityFrames ? ring->capacityFrames : (uint32_t)frames;
    }
    pthread_mutex_unlock(&ring->mutex);
    return available;
}

static uint32_t OutputTimelineWrite(OutputTimelineRing *ring,
                                    const float *frames,
                                    uint32_t frameCount,
                                    int64_t startFrame,
                                    uint32_t startLatencyFrames,
                                    uint32_t restartLatencyFrames,
                                    uint32_t targetLatencyFrames,
                                    uint32_t *outResetCount,
                                    uint32_t *outLateWriteFrames)
{
    if (ring->data == NULL || ring->frameNumbers == NULL || frames == NULL || frameCount == 0) {
        return 0;
    }

    uint32_t dropped = 0;
    uint32_t resetCount = 0;
    uint32_t lateWriteFrames = 0;
    pthread_mutex_lock(&ring->mutex);
    if (!ring->hasWritten) {
        OutputTimelineResetLocked(ring, startFrame, startLatencyFrames);
    } else {
        int64_t lastFrame = startFrame + (int64_t)frameCount - 1;
        int64_t staleGap = ring->readFrame - lastFrame;
        int64_t futureGap = startFrame - ring->readFrame;
        if (staleGap > 0 ||
            futureGap > (int64_t)ring->capacityFrames / 2) {
            uint32_t preroll = restartLatencyFrames > 0 ? restartLatencyFrames : startLatencyFrames;
            OutputTimelineResetLocked(ring, startFrame, preroll);
            resetCount = 1;
        }
    }

    for (uint32_t frame = 0; frame < frameCount; frame++) {
        int64_t frameNumber = startFrame + (int64_t)frame;
        if (frameNumber < ring->readFrame) {
            lateWriteFrames++;
            continue;
        }
        uint32_t index = TimelineIndexForFrame(frameNumber, ring->capacityFrames);
        memcpy(&ring->data[(size_t)index * ring->channels],
               &frames[(size_t)frame * ring->channels],
               (size_t)ring->channels * sizeof(float));
        ring->frameNumbers[index] = frameNumber;
        if (frameNumber > ring->maxWrittenFrame) {
            ring->maxWrittenFrame = frameNumber;
        }
    }

    if (ring->maxWrittenFrame - ring->readFrame >= (int64_t)ring->capacityFrames) {
        int64_t newReadFrame = ring->maxWrittenFrame - (int64_t)targetLatencyFrames;
        if (newReadFrame > ring->readFrame) {
            dropped = (uint32_t)(newReadFrame - ring->readFrame);
            ring->readFrame = newReadFrame;
        }
    }
    pthread_mutex_unlock(&ring->mutex);
    if (outResetCount != NULL) {
        *outResetCount = resetCount;
    }
    if (outLateWriteFrames != NULL) {
        *outLateWriteFrames = lateWriteFrames;
    }
    return dropped;
}

static uint32_t OutputTimelineReadFrames(OutputTimelineRing *ring,
                                         float *frames,
                                         uint32_t frameCount,
                                         uint32_t channels,
                                         uint32_t targetLatencyFrames,
                                         bool *outHaveFrame,
                                         bool *outStartupSilence,
                                         uint32_t *outElasticDropFrames)
{
    if (frames == NULL || frameCount == 0 || channels == 0) {
        return 0;
    }

#if !OPENA8DJ_FAST_OUTPUT_PREFETCH_CLEAR
    memset(frames, 0, (size_t)frameCount * channels * sizeof(float));
    if (outHaveFrame != NULL) {
        memset(outHaveFrame, 0, (size_t)frameCount * sizeof(bool));
    }
    if (outStartupSilence != NULL) {
        memset(outStartupSilence, 0, (size_t)frameCount * sizeof(bool));
    }
    if (outElasticDropFrames != NULL) {
        memset(outElasticDropFrames, 0, (size_t)frameCount * sizeof(uint32_t));
    }
#endif

    pthread_mutex_lock(&ring->mutex);
    for (uint32_t frameIndex = 0; frameIndex < frameCount; frameIndex++) {
        bool haveFrame = false;
        bool startupSilence = false;
        uint32_t elasticDrops = 0;
        float *frame = &frames[(size_t)frameIndex * channels];

        if (!ring->hasWritten) {
            startupSilence = true;
        } else {
            if (ring->maxWrittenFrame - ring->readFrame > (int64_t)kOutputElasticHighWaterFrames) {
                int64_t newReadFrame = ring->maxWrittenFrame - (int64_t)targetLatencyFrames;
                if (newReadFrame > ring->readFrame) {
                    elasticDrops = (uint32_t)(newReadFrame - ring->readFrame);
                    ring->readFrame = newReadFrame;
                }
            }

            uint32_t index = TimelineIndexForFrame(ring->readFrame, ring->capacityFrames);
            if (ring->frameNumbers != NULL &&
                ring->frameNumbers[index] == ring->readFrame &&
                ring->data != NULL) {
                memcpy(frame,
                       &ring->data[(size_t)index * ring->channels],
                       (size_t)channels * sizeof(float));
                haveFrame = true;
            } else if (ring->readFrame < ring->firstWrittenFrame) {
                startupSilence = true;
            }
            ring->readFrame++;
        }

#if OPENA8DJ_FAST_OUTPUT_PREFETCH_CLEAR
        if (!haveFrame) {
            memset(frame, 0, (size_t)channels * sizeof(float));
        }
#endif
        if (outHaveFrame != NULL) {
            outHaveFrame[frameIndex] = haveFrame;
        }
        if (outStartupSilence != NULL) {
            outStartupSilence[frameIndex] = startupSilence;
        }
        if (outElasticDropFrames != NULL) {
            outElasticDropFrames[frameIndex] = elasticDrops;
        }
    }
    pthread_mutex_unlock(&ring->mutex);

    return frameCount;
}
static CFMutableDictionaryRef CreateDeviceMatchingDictionary(void)
{
    return [IOUSBHostDevice createMatchingDictionaryWithVendorID:@(kVendorID)
                                                       productID:@(kProductID)
                                                       bcdDevice:nil
                                                     deviceClass:nil
                                                  deviceSubclass:nil
                                                  deviceProtocol:nil
                                                           speed:nil
                                                  productIDArray:nil];
}

static CFMutableDictionaryRef CreateInterfaceMatchingDictionary(void)
{
    return [IOUSBHostInterface createMatchingDictionaryWithVendorID:@(kVendorID)
                                                          productID:@(kProductID)
                                                          bcdDevice:nil
                                                    interfaceNumber:@(kInterfaceNumber)
                                                 configurationValue:@(kConfigurationValue)
                                                     interfaceClass:nil
                                                  interfaceSubclass:nil
                                                  interfaceProtocol:nil
                                                              speed:nil
                                                     productIDArray:nil];
}

static io_service_t FindDeviceService(void)
{
    CFMutableDictionaryRef matching = CreateDeviceMatchingDictionary();
    io_iterator_t iterator = IO_OBJECT_NULL;
    if (matching == NULL ||
        IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) != KERN_SUCCESS ||
        iterator == IO_OBJECT_NULL) {
        return IO_OBJECT_NULL;
    }
    io_service_t service = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    return service;
}

static io_service_t FindInterfaceServiceWithRetry(NSTimeInterval timeout)
{
    const useconds_t stepUsec = 100000;
    int attempts = (int)((timeout * 1000000.0) / (double)stepUsec);
    if (attempts < 1) {
        attempts = 1;
    }

    for (int attempt = 0; attempt < attempts; attempt++) {
        CFMutableDictionaryRef matching = CreateInterfaceMatchingDictionary();
        io_iterator_t iterator = IO_OBJECT_NULL;
        if (matching != NULL &&
            IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) == KERN_SUCCESS &&
            iterator != IO_OBJECT_NULL) {
            io_service_t service = IOIteratorNext(iterator);
            IOObjectRelease(iterator);
            if (service != IO_OBJECT_NULL) {
                return service;
            }
        } else if (iterator != IO_OBJECT_NULL) {
            IOObjectRelease(iterator);
        }
        usleep(stepUsec);
    }

    return IO_OBJECT_NULL;
}

static uint16_t le16(uint16_t value)
{
    return OSSwapLittleToHostInt16(value);
}

static int SampleRateCode(double sampleRate)
{
    if (sampleRate == 44100.0) return 0;
    if (sampleRate == 48000.0) return 1;
    if (sampleRate == 96000.0) return 2;
    if (sampleRate == 88200.0) return 4;
    return -1;
}

static uint8_t MaxU8(uint8_t a, uint8_t b)
{
    return a > b ? a : b;
}

static bool ReadFull(int fd, void *buffer, size_t length)
{
    uint8_t *bytes = buffer;
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = read(fd, bytes + offset, length - offset);
        if (count <= 0) {
            return false;
        }
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
        if (count <= 0) {
            return false;
        }
        offset += (size_t)count;
    }
    return true;
}

static bool IPCSend(int fd, uint8_t type, const void *payload, uint16_t length)
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

static int AudioStreamCount(const CaiaqDeviceSpec *spec)
{
    uint8_t inputChannels = MaxU8(spec->numAnalogAudioIn, spec->numDigitalAudioIn);
    uint8_t outputChannels = MaxU8(spec->numAnalogAudioOut, spec->numDigitalAudioOut);
    int inputStreams = inputChannels / kChannelsPerStream;
    int outputStreams = outputChannels / kChannelsPerStream;
    int streams = inputStreams > outputStreams ? inputStreams : outputStreams;
    return streams > 0 ? streams : 1;
}

static uint16_t CalculateBytesPerPacket(const CaiaqDeviceSpec *spec, double sampleRate)
{
    int bytesPerSample = kBytesPerSample;
    if (spec->dataAlignment >= 2) {
        bytesPerSample++;
    }
    int bpp = (((int)sampleRate / 8000) + kClockDriftTolerance) *
        bytesPerSample * kChannelsPerStream * AudioStreamCount(spec);
    if (bpp > (int)kIsoBytesPerFrame) {
        bpp = (int)kIsoBytesPerFrame;
    }
    return (uint16_t)bpp;
}

static int32_t S24BEToS32(const uint8_t *bytes)
{
    int32_t value = ((int32_t)bytes[0] << 16) | ((int32_t)bytes[1] << 8) | (int32_t)bytes[2];
    if (value & 0x00800000) {
        value |= (int32_t)0xff000000;
    }
    return value;
}

static void FloatToOutputI24(float sample, uint8_t *bytes)
{
    sample *= OPENA8DJ_OUTPUT_GAIN;
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;
    int32_t sample24;
#if OPENA8DJ_FAST_OUTPUT_QUANTIZER
    if (sample >= 1.0f) {
        sample24 = 0x7fffff;
    } else if (sample <= -1.0f) {
        sample24 = -0x800000;
    } else {
        sample24 = (int32_t)(sample * 8388607.0f);
    }
#else
    int32_t value;
    if (sample >= 1.0f) {
        value = INT32_MAX;
    } else if (sample <= -1.0f) {
        value = INT32_MIN;
    } else {
        value = (int32_t)lrintf(sample * 2147483647.0f);
    }
    sample24 = value >> 8;
#endif
#if OPENA8DJ_OUTPUT_NATIVE_I24
    bytes[0] = (uint8_t)(sample24 & 0xff);
    bytes[1] = (uint8_t)((sample24 >> 8) & 0xff);
    bytes[2] = (uint8_t)((sample24 >> 16) & 0xff);
#else
    bytes[0] = (uint8_t)((sample24 >> 16) & 0xff);
    bytes[1] = (uint8_t)((sample24 >> 8) & 0xff);
    bytes[2] = (uint8_t)(sample24 & 0xff);
#endif
}

#if OPENA8DJ_ENABLE_OUTPUT_AMPLITUDE_STATS
static void OutputFillStatsAccumulateAmplitude(OpenA8DJOutputFillStats *stats, const float *frame)
{
    if (stats == NULL || frame == NULL) {
        return;
    }
    double peak = 0.0;
    uint64_t nearClipSamples = 0;
    uint64_t clippedSamples = 0;
    for (uint32_t channel = 0; channel < kChannels; channel++) {
        double scaled = fabs((double)frame[channel] * (double)OPENA8DJ_OUTPUT_GAIN);
        if (scaled > peak) {
            peak = scaled;
        }
        if (scaled >= 0.98) {
            nearClipSamples++;
        }
        if (scaled > 1.0) {
            clippedSamples++;
        }
    }
    if (peak > stats->peak) {
        stats->peak = peak;
    }
    stats->nearClipSamples += nearClipSamples;
    stats->clippedSamples += clippedSamples;
}
#endif

#if OPENA8DJ_ENABLE_HOT_STREAM_STATS
static void StreamStatsAddTimingLocked(OpenA8DJStreamStatsPayload *stats,
                                       size_t minOffset,
                                       size_t maxOffset,
                                       size_t sumOffset,
                                       size_t samplesOffset,
                                       uint64_t delta)
{
    if (stats == NULL || delta == 0) {
        return;
    }
    uint64_t *minValue = (uint64_t *)((uint8_t *)stats + minOffset);
    uint64_t *maxValue = (uint64_t *)((uint8_t *)stats + maxOffset);
    uint64_t *sumValue = (uint64_t *)((uint8_t *)stats + sumOffset);
    uint64_t *samplesValue = (uint64_t *)((uint8_t *)stats + samplesOffset);
    if (*samplesValue == 0 || delta < *minValue) {
        *minValue = delta;
    }
    if (delta > *maxValue) {
        *maxValue = delta;
    }
    *sumValue += delta;
    *samplesValue += 1;
}
#endif

static uint8_t Mode2CheckByte(uint32_t stream, NSUInteger byteIndex)
{
    NSUInteger group = byteIndex / (kStreams * kBytesPerSampleUSB);
    return (uint8_t)((stream << 1) | ((~group) & 1));
}

@class OpenA8DJIsoTransfer;

@interface OpenA8DJUSBEngine : NSObject
@property(nonatomic) double sampleRate;
- (instancetype)initWithSampleRate:(double)sampleRate;
- (BOOL)start;
- (BOOL)isStreaming;
- (BOOL)ensureOpen;
- (void)stop;
- (void)close;
- (void)setInputDecodeActive:(BOOL)active;
- (uint32_t)readInput:(float *)outInterleaved frames:(uint32_t)frames channels:(uint32_t)channels;
- (void)writeOutput:(const float *)inInterleaved frames:(uint32_t)frames channels:(uint32_t)channels;
- (void)writeOutput:(const float *)inInterleaved
              frames:(uint32_t)frames
            channels:(uint32_t)channels
          sampleTime:(double)sampleTime
     sampleTimeValid:(BOOL)sampleTimeValid;
- (void)handleMIDIPacketList:(const MIDIPacketList *)packetList;
- (BOOL)sendCommandNoReply:(uint8_t)command payload:(const uint8_t *)payload payloadLength:(NSUInteger)payloadLength;
- (BOOL)sendAudioParamsWithRateCode:(uint8_t)rateCode
                               depth:(uint8_t)depth
                      bytesPerPacket:(uint16_t)bpp
                                name:(const char *)name;
- (void)resetAudioParamsBeforeStream;
- (void)startIPCServer;
- (void)stopIPCServer;
- (void)broadcastIPCType:(uint8_t)type bytes:(const uint8_t *)bytes length:(NSUInteger)length;
- (BOOL)getClockAnchor:(OpenA8DJUSBClockAnchor *)outAnchor;
- (BOOL)sampleStableUSBFrame:(uint64_t *)outFrame hostTime:(uint64_t *)outHostTime;
- (void)updateUSBFrameClockWithFrame:(uint64_t)frameNumber hostTime:(uint64_t)hostTime;
- (void)queueCaptureTransfer;
- (void)handleCaptureTransfer:(OpenA8DJIsoTransfer *)transfer
                        status:(IOReturn)status
                  transactions:(IOUSBHostIsochronousTransaction *)transactions;
- (void)fillPlaybackQueue;
- (BOOL)queuePlaybackTransfer;
- (BOOL)queueCapturePacedPlaybackWithRequests:(const uint32_t *)requests count:(NSUInteger)count;
- (BOOL)queuePlaybackWithRequests:(const uint32_t *)requests count:(NSUInteger)count;
- (OpenA8DJIsoTransfer *)checkoutTransferFromPool:(NSMutableArray<OpenA8DJIsoTransfer *> *)pool
                                         requests:(const uint32_t *)requests
                                            count:(NSUInteger)count
                                        nextIndex:(NSUInteger *)nextIndex;
- (void)releasePooledTransfer:(OpenA8DJIsoTransfer *)transfer;
- (void)resyncPlaybackSchedule;
- (void)handlePlaybackTransfer:(OpenA8DJIsoTransfer *)transfer
                         status:(IOReturn)status
                   transactions:(IOUSBHostIsochronousTransaction *)transactions;
- (void)addTimingMinOffset:(size_t)minOffset
                 maxOffset:(size_t)maxOffset
                 sumOffset:(size_t)sumOffset
             samplesOffset:(size_t)samplesOffset
                     delta:(uint64_t)delta;
- (void)addTimingMinOffset:(size_t)minOffset
                 maxOffset:(size_t)maxOffset
                 sumOffset:(size_t)sumOffset
             samplesOffset:(size_t)samplesOffset
             outlierOffset:(size_t)outlierOffset
                     delta:(uint64_t)delta
                  threshold:(uint64_t)threshold;
- (void)addInputStatsBatch:(const OpenA8DJInputStatsPayload *)stats;
- (void)accumulateOutputFillStats:(const OpenA8DJOutputFillStats *)stats force:(BOOL)force;
- (void)setInputDecodeEnabled:(BOOL)enabled;
@end

@interface OpenA8DJIsoTransfer : NSObject
@property(nonatomic, strong) NSMutableData *data;
@property(nonatomic, strong) NSMutableData *transactions;
@property(nonatomic) NSUInteger transactionCount;
@property(nonatomic) NSUInteger capacityBytes;
@property(nonatomic) NSUInteger capacityTransactions;
@property(nonatomic) BOOL configured;
@property(nonatomic) BOOL pooled;
@property(nonatomic) BOOL inUse;
@property(nonatomic) NSUInteger slotIndex;
@property(nonatomic) uint64_t slotGeneration;
@property(nonatomic, copy) OpenA8DJIsoCompletionHandler captureCompletionHandler;
@property(nonatomic, copy) OpenA8DJIsoCompletionHandler playbackCompletionHandler;
@end

@implementation OpenA8DJIsoTransfer
@end

static BOOL ConfigureIsoTransfer(OpenA8DJIsoTransfer *transfer, const uint32_t *requests, NSUInteger count)
{
    if (transfer == nil || requests == NULL || count == 0 || count > transfer.capacityTransactions) {
        return NO;
    }

    NSUInteger dataLength = 0;
    for (NSUInteger index = 0; index < count; index++) {
        dataLength += requests[index];
    }
    if (dataLength > transfer.capacityBytes) {
        return NO;
    }

    IOUSBHostIsochronousTransaction *transactions = transfer.transactions.mutableBytes;
#if OPENA8DJ_FAST_ISO_TRANSFER_CONFIG
    BOOL sameLayout = transfer.transactionCount == count &&
        transfer.data.length == dataLength &&
        transfer.transactions.length == sizeof(IOUSBHostIsochronousTransaction) * count;
    if (transfer.configured && sameLayout) {
        BOOL reusableLayout = YES;
        uint32_t offset = 0;
        for (NSUInteger index = 0; index < count; index++) {
            if (transactions[index].requestCount != requests[index] ||
                transactions[index].offset != offset) {
                reusableLayout = NO;
                break;
            }
            offset += requests[index];
        }
        if (!reusableLayout) {
            sameLayout = NO;
        }
    }
    if (transfer.configured && sameLayout) {
        for (NSUInteger index = 0; index < count; index++) {
            transactions[index].status = kIOReturnInvalid;
            transactions[index].completeCount = 0;
            transactions[index].timeStamp = 0;
            transactions[index].options = IOUSBHostIsochronousTransactionOptionsNone;
        }
        return YES;
    }
#endif

    transfer.data.length = dataLength;
    transfer.transactions.length = sizeof(IOUSBHostIsochronousTransaction) * count;
    transfer.transactionCount = count;
    transactions = transfer.transactions.mutableBytes;

    uint32_t offset = 0;
    for (NSUInteger index = 0; index < count; index++) {
        transactions[index].status = kIOReturnInvalid;
        transactions[index].requestCount = requests[index];
        transactions[index].offset = offset;
        transactions[index].completeCount = 0;
        transactions[index].timeStamp = 0;
        transactions[index].options = IOUSBHostIsochronousTransactionOptionsNone;
        offset += requests[index];
    }
    transfer.configured = YES;

    return YES;
}

static OpenA8DJIsoTransfer *CreateIsoTransferWithCapacity(NSUInteger maxTransactions, NSUInteger maxBytes)
{
    if (maxTransactions == 0 || maxBytes == 0) {
        return nil;
    }
    OpenA8DJIsoTransfer *transfer = [OpenA8DJIsoTransfer new];
    transfer.data = [NSMutableData dataWithLength:maxBytes];
    transfer.transactions = [NSMutableData dataWithLength:sizeof(IOUSBHostIsochronousTransaction) * maxTransactions];
    transfer.capacityBytes = maxBytes;
    transfer.capacityTransactions = maxTransactions;
    transfer.transactionCount = maxTransactions;
    return transfer;
}

static OpenA8DJIsoTransfer *CreateIsoTransfer(const uint32_t *requests, NSUInteger count)
{
    if (requests == NULL || count == 0) {
        return nil;
    }

    NSUInteger dataLength = 0;
    for (NSUInteger index = 0; index < count; index++) {
        dataLength += requests[index];
    }

    OpenA8DJIsoTransfer *transfer = CreateIsoTransferWithCapacity(count, dataLength);
    if (!ConfigureIsoTransfer(transfer, requests, count)) {
        return nil;
    }
    return transfer;
}

@implementation OpenA8DJUSBEngine {
    dispatch_queue_t _queue;
    dispatch_queue_t _ep1Queue;
    dispatch_queue_t _ipcQueue;
    IOUSBHostDevice *_device;
    IOUSBHostInterface *_interface;
    IOUSBHostPipe *_bulkOutPipe;
    IOUSBHostPipe *_bulkInPipe;
    IOUSBHostPipe *_capturePipe;
    IOUSBHostPipe *_playbackPipe;
    CaiaqDeviceSpec _spec;
    bool _deviceInfoAvailable;
    FloatRing _inputRing;
    OutputTimelineRing _outputTimeline;
    OpenA8DJDriverModePolicy _streamDriverModePolicy;
    OpenA8DJStreamStatsPayload _streamStats;
    pthread_mutex_t _streamStatsMutex;
    atomic_uint_fast64_t _outputFramesWrittenAtomic;
    OpenA8DJUSBClockAnchor _clockAnchor;
    pthread_mutex_t _clockAnchorMutex;
    uint64_t _clockAnchorResets;
    uint64_t _inputMode2Index;
    uint8_t _inputBytes[kStreams][kChannelsPerStream * kBytesPerSample];
    uint8_t _inputByteCount[kStreams];
    float _pendingInput[kChannels];
    float _pendingPhysicalInput[kChannels];
    uint8_t _pendingInputMask;
    OpenA8DJInputStatsPayload _inputStats;
    pthread_mutex_t _inputStatsMutex;
    OpenA8DJTimecodeClassifier _timecodeClassifier;
    OpenA8DJTimecodeWindow _timecodePublishedWindow;
    pthread_mutex_t _timecodePublishedWindowMutex;
    atomic_ullong _timecodeLastCompleteHostTime;
    atomic_uint _inputSwapMask;
    atomic_uint _inputInvertLeftMask;
    atomic_uint _inputInvertRightMask;
    atomic_uint _inputSourceMap;
    atomic_bool _inputDecodeEnabled;
    atomic_bool _inputDecodeActive;
    uint8_t _outputFrameBytes[kStreams][kChannelsPerStream * kBytesPerSample];
    uint8_t _outputByteInFrame;
    bool _outputFrameLoaded;
    bool _outputPlaybackPrimed;
    bool _outputHasStartedPlayback;
    uint64_t _outputFramesServed;
    float _outputLastFrame[kChannels];
    bool _outputLastFrameValid;
    uint32_t _outputReplayRunFrames;
    uint32_t _outputElasticCorrectionCountdown;
    float _outputPrefetch[kOutputPrefetchFrames * kChannels];
    bool _outputPrefetchHaveFrame[kOutputPrefetchFrames];
    bool _outputPrefetchStartupSilence[kOutputPrefetchFrames];
    uint32_t _outputPrefetchElasticDrops[kOutputPrefetchFrames];
    uint32_t _outputPrefetchIndex;
    uint32_t _outputPrefetchCount;
    OpenA8DJOutputFillStats _pendingOutputFillStats;
    uint32_t _pendingOutputFillTransferCount;
    bool _playbackScheduleValid;
    uint64_t _nextPlaybackFrameNumber;
    uint32_t _pendingPlaybackRequests[kPlaybackIsoFramesPerTransfer];
    NSUInteger _pendingPlaybackRequestCount;
    atomic_bool _playbackUseExplicitScheduling;
    atomic_uint _playbackScheduleFailureStreak;
    atomic_uint _playbackTransfersInFlight;
    atomic_ullong _captureHotStreamStatsCounter;
    atomic_ullong _playbackHotStreamStatsCounter;
    uint64_t _lastCaptureCompletionHostTime;
    uint64_t _lastPlaybackCompletionHostTime;
    uint64_t _lastCaptureTransactionUSBTime;
    uint64_t _ticksPerUSBMicroframe;
    uint64_t _completionJitterBinThresholdTicks[5];
    OpenA8DJCompletionQualityBatch _pendingCaptureCompletionQuality;
    OpenA8DJCompletionQualityBatch _pendingPlaybackCompletionQuality;
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
    OpenA8DJCadenceDiagnostics _cadenceDiagnostics;
#endif
    NSMutableArray<OpenA8DJIsoTransfer *> *_captureTransferPool;
    NSMutableArray<OpenA8DJIsoTransfer *> *_playbackTransferPool;
    NSUInteger _captureTransferPoolCursor;
    NSUInteger _playbackTransferPoolCursor;
    pthread_mutex_t _transferPoolMutex;
#if OPENA8DJ_ENABLE_TRACE
    uint64_t _debugCaptureCheckGroups;
    uint64_t _debugInputCheckErrors;
    uint64_t _debugOutputPanicFlags;
    uint64_t _debugOutputFramesWritten;
    uint64_t _debugOutputFramesRead;
    uint64_t _debugOutputUnderruns;
    float _debugOutputPeak;
    uint64_t _debugCaptureTransfers;
    uint64_t _debugPlaybackTransfers;
    uint64_t _debugPlaybackTransactionFailures;
    uint64_t _debugPlaybackShortTransfers;
    uint64_t _debugPlaybackBytes;
    uint64_t _debugCaptureQueueFailures;
    uint64_t _debugPlaybackQueueFailures;
#endif
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
    pthread_mutex_t _diagnosticMutex;
    float *_diagnosticWrittenBuffer;
    float *_diagnosticConsumedBuffer;
    float *_diagnosticInputBuffer;
    uint8_t *_diagnosticPackedInputBuffer;
    uint8_t *_diagnosticPackedOutputBuffer;
    OpenA8DJDiagnosticEvent *_diagnosticEvents;
    uint64_t _diagnosticWrittenFrames;
    uint64_t _diagnosticConsumedFrames;
    uint64_t _diagnosticInputFrames;
    uint64_t _diagnosticPackedInputBytes;
    uint64_t _diagnosticPackedInputDroppedBytes;
    uint64_t _diagnosticPackedOutputBytes;
    uint64_t _diagnosticPackedOutputDroppedBytes;
    atomic_ullong _diagnosticEventCount;
#endif
    atomic_bool _running;
    atomic_bool _streaming;
    pthread_mutex_t _bulkOutMutex;
    pthread_mutex_t _ep1Mutex;
    pthread_cond_t _ep1Cond;
    uint8_t _pendingCommand;
    bool _pendingActive;
    bool _pendingReady;
    NSData *_pendingReply;
    uint8_t _controlState[256];
    pthread_mutex_t _ipcClientsMutex;
    int _ipcListenFd;
    int _ipcClients[16];
    size_t _ipcClientCount;
    dispatch_source_t _streamKeepaliveTimer;
    MIDIClientRef _midiClient;
    MIDIEndpointRef _midiSource;
    MIDIEndpointRef _midiDestination;
}

- (instancetype)initWithSampleRate:(double)sampleRate
{
    self = [super init];
    if (self != nil) {
        _sampleRate = sampleRate;
        atomic_init(&_running, false);
        atomic_init(&_streaming, false);
        atomic_init(&_inputSwapMask, 0);
        atomic_init(&_inputInvertLeftMask, 0);
        atomic_init(&_inputInvertRightMask, 0);
        atomic_init(&_inputSourceMap, kInputSourceIdentityMap);
        atomic_init(&_inputDecodeEnabled, atomic_load(&gInputDecodeEnabledPreference));
        atomic_init(&_inputDecodeActive, false);
        atomic_init(&_playbackUseExplicitScheduling, true);
        atomic_init(&_playbackScheduleFailureStreak, 0);
        atomic_init(&_playbackTransfersInFlight, 0);
        atomic_init(&_outputFramesWrittenAtomic, 0);
        atomic_init(&_captureHotStreamStatsCounter, 0);
        atomic_init(&_playbackHotStreamStatsCounter, 0);
        (void)OpenA8DJDriverModeLookup(kOpenA8DJDriverModeBalanced,
                                       &_streamDriverModePolicy);
        _ticksPerUSBMicroframe = MachTicksForNanoseconds(125000);
        _completionJitterBinThresholdTicks[0] = MachTicksForNanoseconds(50000);
        _completionJitterBinThresholdTicks[1] = MachTicksForNanoseconds(100000);
        _completionJitterBinThresholdTicks[2] = MachTicksForNanoseconds(250000);
        _completionJitterBinThresholdTicks[3] = MachTicksForNanoseconds(500000);
        _completionJitterBinThresholdTicks[4] = MachTicksForNanoseconds(1000000);
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
        CadenceReset(&_cadenceDiagnostics);
#endif
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
        pthread_mutex_init(&_diagnosticMutex, NULL);
        atomic_init(&_diagnosticEventCount, 0);
#endif
        _queue = dispatch_queue_create("org.opena8dj.driver.usb", OpenA8DJUSBQueueAttributes());
        dispatch_queue_set_specific(
            _queue,
            &gTimecodeWriterQueueSpecificKey,
            &gTimecodeWriterQueueSpecificKey,
            NULL);
        _ep1Queue = dispatch_queue_create("org.opena8dj.driver.ep1", DISPATCH_QUEUE_SERIAL);
        _ipcQueue = dispatch_queue_create("org.opena8dj.driver.ipc", DISPATCH_QUEUE_SERIAL);
        pthread_mutex_init(&_bulkOutMutex, NULL);
        pthread_mutex_init(&_ep1Mutex, NULL);
        pthread_cond_init(&_ep1Cond, NULL);
        pthread_mutex_init(&_inputStatsMutex, NULL);
        pthread_mutex_init(&_timecodePublishedWindowMutex, NULL);
        OpenA8DJTimecodeClassifierInit(&_timecodeClassifier, sampleRate);
        atomic_init(
            &_timecodeLastCompleteHostTime,
            atomic_load(&gTimecodeClassificationArmed) ?
                mach_absolute_time() : 0);
        pthread_mutex_init(&_streamStatsMutex, NULL);
        pthread_mutex_init(&_clockAnchorMutex, NULL);
        pthread_mutex_init(&_transferPoolMutex, NULL);
        pthread_mutex_init(&_ipcClientsMutex, NULL);
        _ipcListenFd = -1;
        for (size_t i = 0; i < sizeof(_ipcClients) / sizeof(_ipcClients[0]); i++) {
            _ipcClients[i] = -1;
        }
        RingInit(&_inputRing, kRingFrames, kChannels);
        OutputTimelineInit(&_outputTimeline, kRingFrames, kChannels);
#if OPENA8DJ_ENABLE_TRANSFER_POOL
        _captureTransferPool = [NSMutableArray arrayWithCapacity:kCaptureQueueDepth];
        _playbackTransferPool = [NSMutableArray arrayWithCapacity:kPlaybackQueueMax];
#if OPENA8DJ_REUSE_ISOC_COMPLETION_HANDLERS
        __weak OpenA8DJUSBEngine *weakSelf = self;
#endif
        for (uint32_t i = 0; i < kCaptureQueueDepth; i++) {
            OpenA8DJIsoTransfer *transfer =
                CreateIsoTransferWithCapacity(kIsoFramesPerTransfer,
                                              (NSUInteger)kIsoFramesPerTransfer * kIsoBytesPerFrame);
            if (transfer != nil) {
                transfer.pooled = YES;
#if OPENA8DJ_REUSE_ISOC_COMPLETION_HANDLERS
                __unsafe_unretained OpenA8DJIsoTransfer *rawTransfer = transfer;
                transfer.captureCompletionHandler =
                    ^(IOReturn status, IOUSBHostIsochronousTransaction *transactionList) {
                        OpenA8DJUSBEngine *strongSelf = weakSelf;
                        if (strongSelf != nil) {
                            [strongSelf handleCaptureTransfer:rawTransfer
                                                       status:status
                                                 transactions:transactionList];
                        }
                    };
#endif
                [_captureTransferPool addObject:transfer];
            }
        }
        for (uint32_t i = 0; i < kPlaybackQueueMax; i++) {
            OpenA8DJIsoTransfer *transfer =
                CreateIsoTransferWithCapacity(kPlaybackIsoFramesPerTransfer,
                                              (NSUInteger)kPlaybackIsoFramesPerTransfer * kIsoBytesPerFrame);
            if (transfer != nil) {
                transfer.pooled = YES;
                transfer.slotIndex = i;
#if OPENA8DJ_REUSE_ISOC_COMPLETION_HANDLERS
                __unsafe_unretained OpenA8DJIsoTransfer *rawTransfer = transfer;
                transfer.playbackCompletionHandler =
                    ^(IOReturn status, IOUSBHostIsochronousTransaction *transactionList) {
                        OpenA8DJUSBEngine *strongSelf = weakSelf;
                        if (strongSelf != nil) {
                            [strongSelf handlePlaybackTransfer:rawTransfer
                                                        status:status
                                                  transactions:transactionList];
                        }
                    };
#endif
                [_playbackTransferPool addObject:transfer];
            }
        }
#endif
    }
    return self;
}

- (void)dealloc
{
    [self close];
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
    pthread_mutex_lock(&_diagnosticMutex);
    free(_diagnosticWrittenBuffer);
    free(_diagnosticConsumedBuffer);
    free(_diagnosticInputBuffer);
    free(_diagnosticPackedInputBuffer);
    free(_diagnosticPackedOutputBuffer);
    free(_diagnosticEvents);
    pthread_mutex_unlock(&_diagnosticMutex);
    pthread_mutex_destroy(&_diagnosticMutex);
#endif
    pthread_mutex_destroy(&_ipcClientsMutex);
    pthread_mutex_destroy(&_clockAnchorMutex);
    pthread_mutex_destroy(&_streamStatsMutex);
    pthread_mutex_destroy(&_inputStatsMutex);
    pthread_mutex_destroy(&_timecodePublishedWindowMutex);
    pthread_mutex_destroy(&_transferPoolMutex);
    pthread_cond_destroy(&_ep1Cond);
    pthread_mutex_destroy(&_ep1Mutex);
    pthread_mutex_destroy(&_bulkOutMutex);
    RingDestroy(&_inputRing);
    OutputTimelineDestroy(&_outputTimeline);
}

#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
- (void)openDiagnosticCapture
{
    pthread_mutex_lock(&_diagnosticMutex);
    free(_diagnosticWrittenBuffer);
    free(_diagnosticConsumedBuffer);
    free(_diagnosticInputBuffer);
    free(_diagnosticPackedInputBuffer);
    free(_diagnosticPackedOutputBuffer);
    free(_diagnosticEvents);
    _diagnosticWrittenBuffer = calloc((size_t)kDiagnosticCaptureMaxFrames * kChannels, sizeof(float));
    _diagnosticConsumedBuffer = calloc((size_t)kDiagnosticCaptureMaxFrames * kChannels, sizeof(float));
    _diagnosticInputBuffer = calloc((size_t)kDiagnosticCaptureMaxFrames * kChannels, sizeof(float));
    _diagnosticPackedInputBuffer = calloc((size_t)kDiagnosticPackedMaxBytes, sizeof(uint8_t));
    _diagnosticPackedOutputBuffer = calloc((size_t)kDiagnosticPackedMaxBytes, sizeof(uint8_t));
    _diagnosticEvents = calloc(kDiagnosticEventMaxCount, sizeof(OpenA8DJDiagnosticEvent));
    _diagnosticWrittenFrames = 0;
    _diagnosticConsumedFrames = 0;
    _diagnosticInputFrames = 0;
    _diagnosticPackedInputBytes = 0;
    _diagnosticPackedInputDroppedBytes = 0;
    _diagnosticPackedOutputBytes = 0;
    _diagnosticPackedOutputDroppedBytes = 0;
    atomic_store(&_diagnosticEventCount, 0);
    FILE *meta = fopen("/tmp/opena8dj-output-capture.txt", "w");
    if (meta != NULL) {
        fprintf(meta,
                "sample_rate=%.0f\nchannels=%u\nformat=f32le-interleaved\n"
                "input_loopback_file=/tmp/opena8dj-input-loopback-f32.raw\n"
                "input_packed_usb_file=/tmp/opena8dj-input-packed-usb.raw\n"
                "packed_usb_file=/tmp/opena8dj-output-packed-usb.raw\n"
                "events_file=/tmp/opena8dj-output-events.tsv\n",
                _sampleRate,
                (unsigned)kChannels);
        fclose(meta);
    }
    pthread_mutex_unlock(&_diagnosticMutex);
}

- (void)appendDiagnosticFrames:(float **)bufferPtr counter:(uint64_t *)counter frames:(const float *)frames count:(uint32_t)count
{
    pthread_mutex_lock(&_diagnosticMutex);
    float *buffer = bufferPtr != NULL ? *bufferPtr : NULL;
    if (buffer == NULL || counter == NULL || frames == NULL || count == 0) {
        pthread_mutex_unlock(&_diagnosticMutex);
        return;
    }
    uint64_t current = *counter;
    if (current >= kDiagnosticCaptureMaxFrames) {
        pthread_mutex_unlock(&_diagnosticMutex);
        return;
    }
    uint32_t writable = count;
    if (current + writable > kDiagnosticCaptureMaxFrames) {
        writable = (uint32_t)(kDiagnosticCaptureMaxFrames - current);
    }
    memcpy(&buffer[(size_t)current * kChannels],
           frames,
           (size_t)writable * kChannels * sizeof(float));
    *counter = current + writable;
    pthread_mutex_unlock(&_diagnosticMutex);
}

- (void)appendDiagnosticPackedBytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    pthread_mutex_lock(&_diagnosticMutex);
    if (_diagnosticPackedOutputBuffer == NULL || bytes == NULL || length == 0) {
        pthread_mutex_unlock(&_diagnosticMutex);
        return;
    }
    uint64_t current = _diagnosticPackedOutputBytes;
    if (current >= kDiagnosticPackedMaxBytes) {
        _diagnosticPackedOutputDroppedBytes += length;
        pthread_mutex_unlock(&_diagnosticMutex);
        return;
    }
    uint64_t writable = (uint64_t)length;
    if (current + writable > kDiagnosticPackedMaxBytes) {
        writable = kDiagnosticPackedMaxBytes - current;
        _diagnosticPackedOutputDroppedBytes += (uint64_t)length - writable;
    }
    memcpy(_diagnosticPackedOutputBuffer + current, bytes, (size_t)writable);
    _diagnosticPackedOutputBytes = current + writable;
    pthread_mutex_unlock(&_diagnosticMutex);
}

- (void)appendDiagnosticPackedInputBytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    pthread_mutex_lock(&_diagnosticMutex);
    if (_diagnosticPackedInputBuffer == NULL || bytes == NULL || length == 0) {
        pthread_mutex_unlock(&_diagnosticMutex);
        return;
    }
    uint64_t current = _diagnosticPackedInputBytes;
    if (current >= kDiagnosticPackedMaxBytes) {
        _diagnosticPackedInputDroppedBytes += length;
        pthread_mutex_unlock(&_diagnosticMutex);
        return;
    }
    uint64_t writable = (uint64_t)length;
    if (current + writable > kDiagnosticPackedMaxBytes) {
        writable = kDiagnosticPackedMaxBytes - current;
        _diagnosticPackedInputDroppedBytes += (uint64_t)length - writable;
    }
    memcpy(_diagnosticPackedInputBuffer + current, bytes, (size_t)writable);
    _diagnosticPackedInputBytes = current + writable;
    pthread_mutex_unlock(&_diagnosticMutex);
}

- (void)appendDiagnosticEventType:(uint16_t)type
                      frameNumber:(int64_t)frameNumber
                            count:(uint32_t)count
                            flags:(uint16_t)flags
                            value:(uint32_t)value
                            extra:(uint32_t)extra
{
    pthread_mutex_lock(&_diagnosticMutex);
    if (_diagnosticEvents == NULL) {
        pthread_mutex_unlock(&_diagnosticMutex);
        return;
    }
    unsigned long long index = atomic_fetch_add(&_diagnosticEventCount, 1);
    if (index >= kDiagnosticEventMaxCount) {
        pthread_mutex_unlock(&_diagnosticMutex);
        return;
    }
    OpenA8DJDiagnosticEvent *event = &_diagnosticEvents[index];
    event->hostTime = mach_absolute_time();
    event->frameNumber = frameNumber;
    event->count = count;
    event->type = type;
    event->flags = flags;
    event->value = value;
    event->extra = extra;
    pthread_mutex_unlock(&_diagnosticMutex);
}

- (void)closeDiagnosticCapture
{
    pthread_mutex_lock(&_diagnosticMutex);
    if (_diagnosticWrittenBuffer != NULL) {
        FILE *file = fopen("/tmp/opena8dj-output-written-f32.raw", "wb");
        if (file != NULL) {
            fwrite(_diagnosticWrittenBuffer,
                   sizeof(float) * kChannels,
                   (size_t)_diagnosticWrittenFrames,
                   file);
            fclose(file);
        }
        free(_diagnosticWrittenBuffer);
        _diagnosticWrittenBuffer = NULL;
    }
    if (_diagnosticConsumedBuffer != NULL) {
        FILE *file = fopen("/tmp/opena8dj-output-consumed-f32.raw", "wb");
        if (file != NULL) {
            fwrite(_diagnosticConsumedBuffer,
                   sizeof(float) * kChannels,
                   (size_t)_diagnosticConsumedFrames,
                   file);
            fclose(file);
        }
        free(_diagnosticConsumedBuffer);
        _diagnosticConsumedBuffer = NULL;
    }
    if (_diagnosticInputBuffer != NULL) {
        FILE *file = fopen("/tmp/opena8dj-input-loopback-f32.raw", "wb");
        if (file != NULL) {
            fwrite(_diagnosticInputBuffer,
                   sizeof(float) * kChannels,
                   (size_t)_diagnosticInputFrames,
                   file);
            fclose(file);
        }
        free(_diagnosticInputBuffer);
        _diagnosticInputBuffer = NULL;
    }
    if (_diagnosticPackedInputBuffer != NULL) {
        FILE *file = fopen("/tmp/opena8dj-input-packed-usb.raw", "wb");
        if (file != NULL) {
            fwrite(_diagnosticPackedInputBuffer,
                   sizeof(uint8_t),
                   (size_t)_diagnosticPackedInputBytes,
                   file);
            fclose(file);
        }
        free(_diagnosticPackedInputBuffer);
        _diagnosticPackedInputBuffer = NULL;
    }
    if (_diagnosticPackedOutputBuffer != NULL) {
        FILE *file = fopen("/tmp/opena8dj-output-packed-usb.raw", "wb");
        if (file != NULL) {
            fwrite(_diagnosticPackedOutputBuffer,
                   sizeof(uint8_t),
                   (size_t)_diagnosticPackedOutputBytes,
                   file);
            fclose(file);
        }
        free(_diagnosticPackedOutputBuffer);
        _diagnosticPackedOutputBuffer = NULL;
    }
    unsigned long long eventCount = atomic_load(&_diagnosticEventCount);
    unsigned long long eventWriteCount = eventCount;
    if (eventWriteCount > kDiagnosticEventMaxCount) {
        eventWriteCount = kDiagnosticEventMaxCount;
    }
    if (_diagnosticEvents != NULL) {
        FILE *file = fopen("/tmp/opena8dj-output-events.tsv", "w");
        if (file != NULL) {
            fprintf(file, "index\ttype\ttimeline\thost_time\tframe_number\tcount\tflags\tvalue\textra\n");
            for (unsigned long long index = 0; index < eventWriteCount; index++) {
                const OpenA8DJDiagnosticEvent *event = &_diagnosticEvents[index];
                fprintf(file,
                        "%llu\t%s\t%s\t%llu\t%lld\t%u\t0x%04x\t%u\t%u\n",
                        index,
                        DiagnosticEventName(event->type),
                        DiagnosticEventTimelineName(event),
                        (unsigned long long)event->hostTime,
                        (long long)event->frameNumber,
                        event->count,
                        (unsigned)event->flags,
                        event->value,
                        event->extra);
            }
            fclose(file);
        }
        free(_diagnosticEvents);
        _diagnosticEvents = NULL;
    }
    FILE *meta = fopen("/tmp/opena8dj-output-capture.txt", "a");
    if (meta != NULL) {
        fprintf(meta,
                "written_frames=%llu\nconsumed_frames=%llu\ninput_loopback_frames=%llu\n"
                "packed_input_bytes=%llu\npacked_input_dropped_bytes=%llu\n"
                "packed_output_bytes=%llu\npacked_output_dropped_bytes=%llu\n"
                "diagnostic_events=%llu\ndiagnostic_events_written=%llu\n",
                _diagnosticWrittenFrames,
                _diagnosticConsumedFrames,
                _diagnosticInputFrames,
                _diagnosticPackedInputBytes,
                _diagnosticPackedInputDroppedBytes,
                _diagnosticPackedOutputBytes,
                _diagnosticPackedOutputDroppedBytes,
                eventCount,
                eventWriteCount);
        fclose(meta);
    }
    pthread_mutex_unlock(&_diagnosticMutex);
}
#endif

- (BOOL)sendCommand:(uint8_t)command
            payload:(const uint8_t *)payload
      payloadLength:(NSUInteger)payloadLength
              reply:(NSMutableData **)replyOut
        replyLength:(NSUInteger *)replyLengthOut
{
    for (int attempt = 0; attempt < 3; attempt++) {
        NSMutableData *outData = [NSMutableData dataWithLength:1 + payloadLength];
        uint8_t *outBytes = outData.mutableBytes;
        outBytes[0] = command;
        if (payload != NULL && payloadLength > 0) {
            memcpy(outBytes + 1, payload, payloadLength);
        }

        bool expectsReply = replyOut != NULL || replyLengthOut != NULL ||
                            command == kCommandGetDeviceInfo ||
                            command == kCommandAudioParams ||
                            command == kCommandReadIO;
        pthread_mutex_lock(&_ep1Mutex);
        while (_pendingActive) {
            pthread_cond_wait(&_ep1Cond, &_ep1Mutex);
        }
        if (expectsReply) {
            _pendingCommand = command;
            _pendingActive = true;
            _pendingReady = false;
            _pendingReply = nil;
        }
        pthread_mutex_unlock(&_ep1Mutex);

        NSUInteger bytesTransferred = 0;
        NSError *error = nil;
        pthread_mutex_lock(&_bulkOutMutex);
        BOOL sent = [_bulkOutPipe sendIORequestWithData:outData
                                       bytesTransferred:&bytesTransferred
                                      completionTimeout:1.0
                                                  error:&error];
        pthread_mutex_unlock(&_bulkOutMutex);
        if (!sent) {
            USBTrace("bulk OUT failed command=0x%02x error=%s", command, NSErrorText(error));
            pthread_mutex_lock(&_ep1Mutex);
            if (expectsReply && _pendingActive && _pendingCommand == command) {
                _pendingActive = false;
                _pendingReady = false;
                _pendingReply = nil;
                pthread_cond_broadcast(&_ep1Cond);
            }
            pthread_mutex_unlock(&_ep1Mutex);
            return NO;
        }

        if (!expectsReply) {
            return YES;
        }

        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += 2;

        pthread_mutex_lock(&_ep1Mutex);
        int waitStatus = 0;
        while (!_pendingReady && _pendingActive && waitStatus == 0) {
            waitStatus = pthread_cond_timedwait(&_ep1Cond, &_ep1Mutex, &deadline);
        }
        NSData *reply = _pendingReply;
        bool ok = _pendingReady && reply != nil;
        _pendingActive = false;
        _pendingReady = false;
        _pendingReply = nil;
        pthread_cond_broadcast(&_ep1Cond);
        pthread_mutex_unlock(&_ep1Mutex);

        if (ok) {
            if (replyOut != NULL) *replyOut = [reply mutableCopy];
            if (replyLengthOut != NULL) *replyLengthOut = reply.length;
            return YES;
        }

        USBTrace("bulk command timeout command=0x%02x wait=%d", command, waitStatus);
        usleep(100000);
    }
    return NO;
}

- (BOOL)sendCommandNoReply:(uint8_t)command payload:(const uint8_t *)payload payloadLength:(NSUInteger)payloadLength
{
    NSMutableData *outData = [NSMutableData dataWithLength:1 + payloadLength];
    uint8_t *outBytes = outData.mutableBytes;
    outBytes[0] = command;
    if (payload != NULL && payloadLength > 0) {
        memcpy(outBytes + 1, payload, payloadLength);
    }

    NSUInteger bytesTransferred = 0;
    NSError *error = nil;
    pthread_mutex_lock(&_bulkOutMutex);
    BOOL sent = [_bulkOutPipe sendIORequestWithData:outData
                                   bytesTransferred:&bytesTransferred
                                  completionTimeout:0.2
                                              error:&error];
    pthread_mutex_unlock(&_bulkOutMutex);
    if (!sent) {
        USBTrace("bulk OUT no-reply failed command=0x%02x error=%s", command, NSErrorText(error));
    }
    return sent;
}

- (BOOL)getDeviceInfo
{
    _deviceInfoAvailable = false;
    memset(&_spec, 0, sizeof(_spec));
    NSMutableData *reply = nil;
    NSUInteger replyLength = 0;
    if (![self sendCommand:kCommandGetDeviceInfo payload:NULL payloadLength:0 reply:&reply replyLength:&replyLength]) {
        return NO;
    }
    if (replyLength < 1 + sizeof(CaiaqDeviceSpec)) {
        USBTrace("short GET_DEVICE_INFO reply length=%lu", (unsigned long)replyLength);
        return NO;
    }
    const uint8_t *bytes = reply.bytes;
    if (bytes[0] != kCommandGetDeviceInfo) {
        USBTrace("unexpected GET_DEVICE_INFO reply command=0x%02x", bytes[0]);
        return NO;
    }
    memcpy(&_spec, bytes + 1, sizeof(_spec));
    _spec.fwVersion = le16(_spec.fwVersion);
    _deviceInfoAvailable = true;
    USBTrace("device info fw=%u in=%u out=%u midi=%u/%u align=%u",
             _spec.fwVersion,
             _spec.numAnalogAudioIn,
             _spec.numAnalogAudioOut,
             _spec.numMidiIn,
             _spec.numMidiOut,
             _spec.dataAlignment);
    return YES;
}

- (BOOL)sendAudioParamsWithRateCode:(uint8_t)rateCode
                               depth:(uint8_t)depth
                      bytesPerPacket:(uint16_t)bpp
                                name:(const char *)name
{
    uint8_t payload[5] = {
        (uint8_t)rateCode,
        depth,
        (uint8_t)(bpp & 0xff),
        (uint8_t)(bpp >> 8),
        1
    };
    NSMutableData *reply = nil;
    NSUInteger replyLength = 0;
    if (![self sendCommand:kCommandAudioParams payload:payload payloadLength:sizeof(payload) reply:&reply replyLength:&replyLength]) {
        return NO;
    }
    const uint8_t *bytes = reply.bytes;
    BOOL ok = replyLength >= 2 && bytes[0] == kCommandAudioParams && bytes[1] == 1;
    USBTrace("audio params %s rateCode=0x%02x depth=%u bpp=%u ok=%d",
             name != NULL ? name : "set",
             rateCode,
             depth,
             bpp,
             ok);
    return ok;
}

- (void)resetAudioParamsBeforeStream
{
#if OPENA8DJ_RESET_AUDIO_PARAMS_BEFORE_STREAM
    uint16_t bpp = CalculateBytesPerPacket(&_spec, _sampleRate);
    if (![self sendAudioParamsWithRateCode:0xff depth:0 bytesPerPacket:bpp name:"reset"]) {
        USBTrace("audio params reset did not complete; continuing to stream setup");
    }
#else
    USBTrace("audio params reset skipped before stream setup");
#endif
}

- (BOOL)setAudioParams
{
    int rateCode = SampleRateCode(_sampleRate);
    if (rateCode < 0) {
        USBTrace("unsupported sample rate %.0f", _sampleRate);
        return NO;
    }
    uint16_t bpp = CalculateBytesPerPacket(&_spec, _sampleRate);
    return [self sendAudioParamsWithRateCode:(uint8_t)rateCode
                                       depth:2
                              bytesPerPacket:bpp
                                        name:"stream"];
}

- (void)loadControlPayload:(OpenA8DJControlPayload *)payload
{
    if (payload == NULL) {
        return;
    }
    pthread_mutex_lock(&_ep1Mutex);
    payload->inputMode = _controlState[0];
    payload->gndLiftTCVinyl = (_controlState[3] & (1u << 0)) ? 1 : 0;
    payload->gndLiftTCCDLine = (_controlState[3] & (1u << 1)) ? 1 : 0;
    payload->gndLiftPhono = (_controlState[3] & (1u << 2)) ? 1 : 0;
    payload->softwareLock = (_controlState[5] & (1u << 0)) ? 1 : 0;
    pthread_mutex_unlock(&_ep1Mutex);
    payload->inputSwapMask = (uint8_t)(atomic_load(&_inputSwapMask) & kInputTransformPairMask);
    payload->inputInvertLeftMask = (uint8_t)(atomic_load(&_inputInvertLeftMask) & kInputTransformPairMask);
    payload->inputInvertRightMask = (uint8_t)(atomic_load(&_inputInvertRightMask) & kInputTransformPairMask);
    uint32_t sourceMap = atomic_load(&_inputSourceMap);
    for (uint32_t stream = 0; stream < kStreams; stream++) {
        uint8_t source = (uint8_t)((sourceMap >> (stream * 4)) & 0x0f);
        payload->inputSource[stream] = source < kStreams ? source : (uint8_t)stream;
    }
    payload->inputDecodeEnabled = atomic_load(&_inputDecodeEnabled) ? 1 : 0;
}

- (void)storeControlPayload:(const OpenA8DJControlPayload *)payload
{
    if (payload == NULL) {
        return;
    }
    pthread_mutex_lock(&_ep1Mutex);
    _controlState[0] = payload->inputMode <= 2 ? payload->inputMode : 0;
    if (payload->gndLiftTCVinyl) _controlState[3] |= (1u << 0); else _controlState[3] &= ~(1u << 0);
    if (payload->gndLiftTCCDLine) _controlState[3] |= (1u << 1); else _controlState[3] &= ~(1u << 1);
    if (payload->gndLiftPhono) _controlState[3] |= (1u << 2); else _controlState[3] &= ~(1u << 2);
    if (payload->softwareLock) _controlState[5] |= (1u << 0); else _controlState[5] &= ~(1u << 0);
    pthread_mutex_unlock(&_ep1Mutex);
    atomic_store(&_inputSwapMask, payload->inputSwapMask & kInputTransformPairMask);
    atomic_store(&_inputInvertLeftMask, payload->inputInvertLeftMask & kInputTransformPairMask);
    atomic_store(&_inputInvertRightMask, payload->inputInvertRightMask & kInputTransformPairMask);
    uint32_t sourceMap = 0;
    for (uint32_t stream = 0; stream < kStreams; stream++) {
        uint8_t source = payload->inputSource[stream] < kStreams ? payload->inputSource[stream] : (uint8_t)stream;
        sourceMap |= ((uint32_t)source) << (stream * 4);
    }
    atomic_store(&_inputSourceMap, sourceMap);
    [self setInputDecodeEnabled:(payload->inputDecodeEnabled != 0)];
}

- (BOOL)readControls
{
    const uint8_t autoMsgPayload[3] = {1, 0, 0};
    (void)[self sendCommandNoReply:kCommandAutoMsg payload:autoMsgPayload payloadLength:sizeof(autoMsgPayload)];

    NSMutableData *reply = nil;
    NSUInteger replyLength = 0;
    if (![self sendCommand:kCommandReadIO payload:NULL payloadLength:0 reply:&reply replyLength:&replyLength]) {
        USBTrace("READ_IO failed");
        return NO;
    }
    if (replyLength < 2 || ((const uint8_t *)reply.bytes)[0] != kCommandReadIO) {
        USBTrace("READ_IO unexpected reply length=%lu", (unsigned long)replyLength);
        return NO;
    }
    NSUInteger copyLength = replyLength - 1;
    if (copyLength > sizeof(_controlState)) {
        copyLength = sizeof(_controlState);
    }
    pthread_mutex_lock(&_ep1Mutex);
    memset(_controlState, 0, sizeof(_controlState));
    memcpy(_controlState, ((const uint8_t *)reply.bytes) + 1, copyLength);
    bool needsDefaultFixup = _controlState[1] != 2 || _controlState[2] != 3 || _controlState[4] != 2;
    if (needsDefaultFixup) {
        _controlState[1] = 2;
        _controlState[2] = 3;
        _controlState[4] = 2;
    }
    pthread_mutex_unlock(&_ep1Mutex);
    USBTrace("READ_IO ok bytes=%lu", (unsigned long)copyLength);
    if (needsDefaultFixup) {
        uint8_t state[6];
        pthread_mutex_lock(&_ep1Mutex);
        memcpy(state, _controlState, sizeof(state));
        pthread_mutex_unlock(&_ep1Mutex);
        (void)[self sendCommandNoReply:kCommandWriteIO payload:state payloadLength:sizeof(state)];
    }
    return YES;
}

- (void)applyTimecodeVinylDefaults
{
    pthread_mutex_lock(&_ep1Mutex);
    _controlState[0] = 0;
    _controlState[3] = (uint8_t)((_controlState[3] & ~(uint8_t)0x07) | (1u << 0));
    _controlState[5] |= (1u << 0);
    pthread_mutex_unlock(&_ep1Mutex);
    (void)[self writeControls];
}

- (BOOL)writeControls
{
    uint8_t state[kA8DJControlStateBytes];
    pthread_mutex_lock(&_ep1Mutex);
    memcpy(state, _controlState, sizeof(state));
    pthread_mutex_unlock(&_ep1Mutex);
    BOOL ok = [self sendCommandNoReply:kCommandWriteIO payload:state payloadLength:sizeof(state)];
    USBTrace("WRITE_IO ok=%d", ok);
    return ok;
}

- (BOOL)openUSB
{
    io_service_t deviceService = FindDeviceService();
    if (deviceService == IO_OBJECT_NULL) {
        USBTrace("Audio 8 DJ USB device not found");
        return NO;
    }

    NSError *error = nil;
    _device = [[IOUSBHostDevice alloc] initWithIOService:deviceService
                                                 options:IOUSBHostObjectInitOptionsDeviceSeize
                                                   queue:_queue
                                                   error:&error
                                         interestHandler:nil];
    IOObjectRelease(deviceService);
    if (_device == nil) {
        USBTrace("open device failed: %s", NSErrorText(error));
        return NO;
    }

    error = nil;
    if (![_device configureWithValue:kConfigurationValue matchInterfaces:YES error:&error]) {
        USBTrace("configure failed: %s", NSErrorText(error));
        return NO;
    }

    io_service_t interfaceService = FindInterfaceServiceWithRetry(2.0);
    if (interfaceService == IO_OBJECT_NULL) {
        USBTrace("interface service not found");
        return NO;
    }

    error = nil;
    _interface = [[IOUSBHostInterface alloc] initWithIOService:interfaceService
                                                      options:IOUSBHostObjectInitOptionsDeviceSeize
                                                        queue:_queue
                                                        error:&error
                                              interestHandler:nil];
    IOObjectRelease(interfaceService);
    if (_interface == nil) {
        USBTrace("open interface failed: %s", NSErrorText(error));
        return NO;
    }

    error = nil;
    if (![_interface selectAlternateSetting:kAlternateSetting error:&error]) {
        USBTrace("select alt failed: %s", NSErrorText(error));
        return NO;
    }

    _bulkOutPipe = [_interface copyPipeWithAddress:kEndpointControlOut error:&error];
    if (_bulkOutPipe == nil) {
        USBTrace("open bulk OUT failed: %s", NSErrorText(error));
        return NO;
    }
    _bulkInPipe = [_interface copyPipeWithAddress:kEndpointControlIn error:&error];
    if (_bulkInPipe == nil) {
        USBTrace("open bulk IN failed: %s", NSErrorText(error));
        return NO;
    }

    atomic_store(&_running, true);
    __weak OpenA8DJUSBEngine *weakSelf = self;
    dispatch_async(_ep1Queue, ^{
        [weakSelf ep1ReadLoop];
    });

    if (![self getDeviceInfo]) {
        return NO;
    }
    (void)[self readControls];

    _capturePipe = [_interface copyPipeWithAddress:kEndpointIsoCapture error:&error];
    if (_capturePipe == nil) {
        USBTrace("open isoc IN failed: %s", NSErrorText(error));
        return NO;
    }
    _playbackPipe = [_interface copyPipeWithAddress:kEndpointIsoPlayback error:&error];
    if (_playbackPipe == nil) {
        USBTrace("open isoc OUT failed: %s", NSErrorText(error));
        return NO;
    }
    [self startIPCServer];
    return YES;
}

- (void)setupMIDI
{
    USBTrace("CoreMIDI bridge disabled in HAL helper; requires user-domain/DriverKit bridge");
    return;
#if 0
    if (_spec.numMidiIn == 0 && _spec.numMidiOut == 0) {
        return;
    }
    OSStatus status = MIDIClientCreate(CFSTR("OpenA8DJ MIDI Client"), NULL, NULL, &_midiClient);
    if (status != noErr) {
        USBTrace("MIDIClientCreate failed status=%d", (int)status);
        return;
    }
    if (_spec.numMidiIn > 0) {
        status = MIDISourceCreate(_midiClient, CFSTR("Open Audio 8 DJ MIDI Out"), &_midiSource);
        USBTrace("MIDISourceCreate status=%d source=%u", (int)status, (unsigned)_midiSource);
    }
    if (_spec.numMidiOut > 0) {
        status = MIDIDestinationCreate(_midiClient,
                                       CFSTR("Open Audio 8 DJ MIDI In"),
                                       MIDIReadCallback,
                                       (__bridge void *)self,
                                       &_midiDestination);
        USBTrace("MIDIDestinationCreate status=%d destination=%u", (int)status, (unsigned)_midiDestination);
    }
#endif
}

- (void)addIPCClient:(int)fd
{
    pthread_mutex_lock(&_ipcClientsMutex);
    size_t capacity = sizeof(_ipcClients) / sizeof(_ipcClients[0]);
    if (_ipcClientCount >= capacity) {
        pthread_mutex_unlock(&_ipcClientsMutex);
        close(fd);
        return;
    }
    for (size_t i = 0; i < capacity; i++) {
        if (_ipcClients[i] < 0) {
            _ipcClients[i] = fd;
            _ipcClientCount++;
            break;
        }
    }
    pthread_mutex_unlock(&_ipcClientsMutex);
}

- (void)removeIPCClient:(int)fd
{
    pthread_mutex_lock(&_ipcClientsMutex);
    size_t capacity = sizeof(_ipcClients) / sizeof(_ipcClients[0]);
    for (size_t i = 0; i < capacity; i++) {
        if (_ipcClients[i] == fd) {
            _ipcClients[i] = -1;
            if (_ipcClientCount > 0) {
                _ipcClientCount--;
            }
            break;
        }
    }
    pthread_mutex_unlock(&_ipcClientsMutex);
    close(fd);
}

- (void)broadcastIPCType:(uint8_t)type bytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    if (length > UINT16_MAX) {
        length = UINT16_MAX;
    }
    pthread_mutex_lock(&_ipcClientsMutex);
    size_t capacity = sizeof(_ipcClients) / sizeof(_ipcClients[0]);
    for (size_t i = 0; i < capacity; i++) {
        int fd = _ipcClients[i];
        if (fd < 0) {
            continue;
        }
        if (!IPCSend(fd, type, bytes, (uint16_t)length)) {
            close(fd);
            _ipcClients[i] = -1;
            if (_ipcClientCount > 0) {
                _ipcClientCount--;
            }
        }
    }
    pthread_mutex_unlock(&_ipcClientsMutex);
}

- (void)sendControlStateToClient:(int)fd
{
    OpenA8DJControlPayload state;
    [self loadControlPayload:&state];
    (void)IPCSend(fd, kIPCTypeControlState, &state, sizeof(state));
}

- (void)addStreamStatAtOffset:(size_t)offset value:(uint64_t)value
{
    pthread_mutex_lock(&_streamStatsMutex);
    uint64_t *counter = (uint64_t *)((uint8_t *)&_streamStats + offset);
    *counter += value;
    pthread_mutex_unlock(&_streamStatsMutex);
    if (value != 0 &&
        (offset == offsetof(OpenA8DJStreamStatsPayload, captureTransactionFailures) ||
         offset == offsetof(OpenA8DJStreamStatsPayload, playbackTransactionFailures) ||
         offset == offsetof(OpenA8DJStreamStatsPayload, captureShortTransfers) ||
         offset == offsetof(OpenA8DJStreamStatsPayload, playbackShortTransfers) ||
         offset == offsetof(OpenA8DJStreamStatsPayload, captureQueueFailures) ||
         offset == offsetof(OpenA8DJStreamStatsPayload, playbackQueueFailures) ||
         offset == offsetof(OpenA8DJStreamStatsPayload, outputActiveUnderruns) ||
         offset == offsetof(OpenA8DJStreamStatsPayload, outputRingOverruns) ||
         offset == offsetof(OpenA8DJStreamStatsPayload, outputTimelineResets) ||
         offset == offsetof(OpenA8DJStreamStatsPayload, inputCheckErrors) ||
         offset == offsetof(OpenA8DJStreamStatsPayload, outputPanicFlags) ||
         offset == offsetof(OpenA8DJStreamStatsPayload, outputLateWriteFrames))) {
        pthread_mutex_lock(&gDriverModeMutex);
        TimecodeFailOpenLocked(
            kOpenA8DJTimecodeFailXRunOrTransportError, true);
        pthread_mutex_unlock(&gDriverModeMutex);
    }
}

- (void)addTimingMinOffset:(size_t)minOffset
                 maxOffset:(size_t)maxOffset
                 sumOffset:(size_t)sumOffset
             samplesOffset:(size_t)samplesOffset
                     delta:(uint64_t)delta
{
    pthread_mutex_lock(&_streamStatsMutex);
    uint64_t *minValue = (uint64_t *)((uint8_t *)&_streamStats + minOffset);
    uint64_t *maxValue = (uint64_t *)((uint8_t *)&_streamStats + maxOffset);
    uint64_t *sumValue = (uint64_t *)((uint8_t *)&_streamStats + sumOffset);
    uint64_t *samplesValue = (uint64_t *)((uint8_t *)&_streamStats + samplesOffset);
    if (*samplesValue == 0 || delta < *minValue) {
        *minValue = delta;
    }
    if (delta > *maxValue) {
        *maxValue = delta;
    }
    *sumValue += delta;
    *samplesValue += 1;
    pthread_mutex_unlock(&_streamStatsMutex);
}

- (void)addTimingMinOffset:(size_t)minOffset
                 maxOffset:(size_t)maxOffset
                 sumOffset:(size_t)sumOffset
             samplesOffset:(size_t)samplesOffset
             outlierOffset:(size_t)outlierOffset
                     delta:(uint64_t)delta
                  threshold:(uint64_t)threshold
{
    pthread_mutex_lock(&_streamStatsMutex);
    uint64_t *minValue = (uint64_t *)((uint8_t *)&_streamStats + minOffset);
    uint64_t *maxValue = (uint64_t *)((uint8_t *)&_streamStats + maxOffset);
    uint64_t *sumValue = (uint64_t *)((uint8_t *)&_streamStats + sumOffset);
    uint64_t *samplesValue = (uint64_t *)((uint8_t *)&_streamStats + samplesOffset);
    uint64_t *outliersValue = (uint64_t *)((uint8_t *)&_streamStats + outlierOffset);
    if (*samplesValue == 0 || delta < *minValue) {
        *minValue = delta;
    }
    if (delta > *maxValue) {
        *maxValue = delta;
    }
    *sumValue += delta;
    *samplesValue += 1;
    if (threshold > 0 && delta > threshold) {
        *outliersValue += 1;
    }
    pthread_mutex_unlock(&_streamStatsMutex);
}

- (void)addOutputFillStats:(const OpenA8DJOutputFillStats *)stats
{
    if (stats == NULL ||
        (stats->framesRead == 0 &&
         stats->underruns == 0 &&
         stats->activeUnderruns == 0 &&
         stats->startupSilenceFrames == 0 &&
         stats->elasticDrops == 0 &&
         stats->elasticReplays == 0 &&
         stats->nearClipSamples == 0 &&
         stats->clippedSamples == 0 &&
         stats->peak <= 0.0)) {
        return;
    }
    pthread_mutex_lock(&_streamStatsMutex);
    _streamStats.outputFramesRead += stats->framesRead;
    _streamStats.outputUnderruns += stats->underruns;
    _streamStats.outputActiveUnderruns += stats->activeUnderruns;
    _streamStats.outputStartupSilenceFrames += stats->startupSilenceFrames;
    _streamStats.outputElasticDrops += stats->elasticDrops;
    _streamStats.outputElasticReplays += stats->elasticReplays;
    if (stats->peak > _streamStats.outputPeak) {
        _streamStats.outputPeak = stats->peak;
    }
    _streamStats.outputNearClipSamples += stats->nearClipSamples;
    _streamStats.outputClippedSamples += stats->clippedSamples;
    pthread_mutex_unlock(&_streamStatsMutex);
}

- (void)accumulateOutputFillStats:(const OpenA8DJOutputFillStats *)stats force:(BOOL)force
{
    if (stats != NULL &&
        (stats->framesRead != 0 ||
         stats->underruns != 0 ||
         stats->activeUnderruns != 0 ||
         stats->startupSilenceFrames != 0 ||
         stats->elasticDrops != 0 ||
         stats->elasticReplays != 0 ||
         stats->nearClipSamples != 0 ||
         stats->clippedSamples != 0 ||
         stats->peak > 0.0)) {
        _pendingOutputFillStats.framesRead += stats->framesRead;
        _pendingOutputFillStats.underruns += stats->underruns;
        _pendingOutputFillStats.activeUnderruns += stats->activeUnderruns;
        _pendingOutputFillStats.startupSilenceFrames += stats->startupSilenceFrames;
        _pendingOutputFillStats.elasticDrops += stats->elasticDrops;
        _pendingOutputFillStats.elasticReplays += stats->elasticReplays;
        _pendingOutputFillStats.nearClipSamples += stats->nearClipSamples;
        _pendingOutputFillStats.clippedSamples += stats->clippedSamples;
        if (stats->peak > _pendingOutputFillStats.peak) {
            _pendingOutputFillStats.peak = stats->peak;
        }
        _pendingOutputFillTransferCount++;
    }

    if (!force && _pendingOutputFillTransferCount < kOutputStatsFlushTransferInterval) {
        return;
    }
    if (_pendingOutputFillTransferCount == 0) {
        return;
    }

    OpenA8DJOutputFillStats flushStats = _pendingOutputFillStats;
    memset(&_pendingOutputFillStats, 0, sizeof(_pendingOutputFillStats));
    _pendingOutputFillTransferCount = 0;
    [self addOutputFillStats:&flushStats];
}

- (OpenA8DJIsoTransfer *)checkoutTransferFromPool:(NSMutableArray<OpenA8DJIsoTransfer *> *)pool
                                         requests:(const uint32_t *)requests
                                            count:(NSUInteger)count
                                        nextIndex:(NSUInteger *)nextIndex
{
#if !OPENA8DJ_ENABLE_TRANSFER_POOL
    (void)pool;
    (void)nextIndex;
    return CreateIsoTransfer(requests, count);
#else
    OpenA8DJIsoTransfer *selected = nil;
    pthread_mutex_lock(&_transferPoolMutex);
    NSUInteger poolCount = pool.count;
    BOOL legacyPlaybackPool = NO;
#if OPENA8DJ_ENABLE_LEGACY_OUT_SLOTS
    legacyPlaybackPool = (pool == _playbackTransferPool);
#endif
#if OPENA8DJ_TRANSFER_POOL_CURSOR
    NSUInteger startIndex = (nextIndex != NULL && poolCount > 0) ? (*nextIndex % poolCount) : 0;
#else
    NSUInteger startIndex = 0;
    if (legacyPlaybackPool && nextIndex != NULL && poolCount > 0) {
        startIndex = *nextIndex % poolCount;
    }
#endif
    for (NSUInteger attempt = 0; attempt < poolCount; attempt++) {
        NSUInteger index = (startIndex + attempt) % poolCount;
        OpenA8DJIsoTransfer *transfer = pool[index];
        if (transfer.inUse) {
            continue;
        }
        if (!ConfigureIsoTransfer(transfer, requests, count)) {
            continue;
        }
        transfer.inUse = YES;
        if (legacyPlaybackPool) {
            transfer.slotGeneration++;
        }
#if OPENA8DJ_TRANSFER_POOL_CURSOR
        if (nextIndex != NULL && poolCount > 0) {
            *nextIndex = (index + 1) % poolCount;
        }
#else
        if (legacyPlaybackPool && nextIndex != NULL && poolCount > 0) {
            *nextIndex = (index + 1) % poolCount;
        }
#endif
        selected = transfer;
        break;
    }
    pthread_mutex_unlock(&_transferPoolMutex);

    if (selected != nil) {
        return selected;
    }
#if OPENA8DJ_ENABLE_LEGACY_OUT_SLOTS
    if (legacyPlaybackPool) {
        return nil;
    }
#endif
#if OPENA8DJ_STRICT_TRANSFER_POOL
    if (nextIndex != NULL) {
        return nil;
    }
#endif
    return CreateIsoTransfer(requests, count);
#endif
}

- (void)releasePooledTransfer:(OpenA8DJIsoTransfer *)transfer
{
    if (transfer == nil || !transfer.pooled) {
        return;
    }
    pthread_mutex_lock(&_transferPoolMutex);
    transfer.inUse = NO;
    pthread_mutex_unlock(&_transferPoolMutex);
}

- (void)resetStreamStats
{
    pthread_mutex_lock(&_streamStatsMutex);
    memset(&_streamStats, 0, sizeof(_streamStats));
    memset(&_pendingCaptureCompletionQuality, 0, sizeof(_pendingCaptureCompletionQuality));
    memset(&_pendingPlaybackCompletionQuality, 0, sizeof(_pendingPlaybackCompletionQuality));
    pthread_mutex_unlock(&_streamStatsMutex);
    atomic_store(&_outputFramesWrittenAtomic, 0);
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
    CadenceReset(&_cadenceDiagnostics);
#endif
}

- (void)flushPendingCompletionQuality
{
    pthread_mutex_lock(&_streamStatsMutex);
    StreamStatsFlushCompletionQualityLocked(
        &_streamStats, &_pendingCaptureCompletionQuality, true);
    StreamStatsFlushCompletionQualityLocked(
        &_streamStats, &_pendingPlaybackCompletionQuality, false);
    pthread_mutex_unlock(&_streamStatsMutex);
}

- (OpenA8DJStreamStatsPayload)streamStatsSnapshot
{
    OpenA8DJStreamStatsPayload stats;
    OpenA8DJDriverModeStatePayload driverMode = DriverModeStateSnapshot();
    pthread_mutex_lock(&_streamStatsMutex);
    stats = _streamStats;
    pthread_mutex_unlock(&_streamStatsMutex);

    stats.outputFramesWritten = atomic_load(&_outputFramesWrittenAtomic);
    stats.streaming = atomic_load(&_streaming) ? 1 : 0;
    stats.outputRingFrames = OutputTimelineAvailable(&_outputTimeline);
    stats.outputTargetLatencyFrames = driverMode.outputTargetLatencyFrames;
    stats.outputByteInFrame = _outputByteInFrame;
    stats.playbackLeadFrames = kPlaybackScheduleLeadFrames;
    stats.playbackQueueTarget = kPlaybackQueueTarget;
    stats.playbackTransfersInFlight = atomic_load(&_playbackTransfersInFlight);
    stats.sampleRate = _sampleRate;
    stats.playbackExplicitScheduling = atomic_load(&_playbackUseExplicitScheduling) ? 1 : 0;
    stats.cadenceExpectedTransferTicks = ExpectedIsoTransferTicks();
#if OPENA8DJ_ENABLE_HOT_STREAM_STATS
    stats.qualityInstrumentationEnabled = 1;
#else
    stats.qualityInstrumentationEnabled = 0;
#endif
    if (_deviceInfoAvailable) {
        stats.deviceFirmwareVersion = _spec.fwVersion;
        stats.deviceHardwareSubtype = _spec.hwSubtype;
        stats.deviceNumAnalogAudioOut = _spec.numAnalogAudioOut;
        stats.deviceNumAnalogAudioIn = _spec.numAnalogAudioIn;
        stats.deviceNumDigitalAudioOut = _spec.numDigitalAudioOut;
        stats.deviceNumDigitalAudioIn = _spec.numDigitalAudioIn;
        stats.deviceNumMidiOut = _spec.numMidiOut;
        stats.deviceNumMidiIn = _spec.numMidiIn;
        stats.deviceDataAlignment = _spec.dataAlignment;
        stats.deviceInfoAvailable = 1;
    } else {
        stats.deviceInfoAvailable = 0;
    }
    stats.driverModeSchemaVersion = driverMode.schemaVersion;
    stats.driverModeRequested = driverMode.requestedMode;
    stats.driverModeEffective = driverMode.effectiveMode;
    stats.driverModePending = driverMode.pending;
    stats.driverModeLastResult = driverMode.lastResult;
    stats.driverModeRejectionReason = driverMode.rejectionReason;
    stats.driverModeGeneration = driverMode.generation;
    stats.driverModeAcceptedRequests = driverMode.acceptedRequests;
    stats.driverModeRejectedRequests = driverMode.rejectedRequests;
    stats.driverModeAppliedTransitions = driverMode.appliedTransitions;
    stats.driverModeApplyFailures = driverMode.applyFailures;
    stats.driverModePendingTransitions = driverMode.pendingTransitions;
    stats.driverModeOutputStartLatencyFrames = driverMode.outputStartLatencyFrames;
    stats.driverModeOutputRestartLatencyFrames = driverMode.outputRestartLatencyFrames;
    stats.driverModeOutputTargetLatencyFrames = driverMode.outputTargetLatencyFrames;
    stats.driverModeWorkerQoS = driverMode.workerQoS;
    OpenA8DJTimecodeStatePayload timecode =
        [self timecodeStateSnapshot];
    memcpy(&stats.timecodeOptimized, &timecode, sizeof(timecode));

    pthread_mutex_lock(&_clockAnchorMutex);
    stats.clockAnchorValid = _clockAnchor.valid ? 1 : 0;
    stats.clockSampleTime = _clockAnchor.sampleTime;
    stats.clockHostTime = _clockAnchor.hostTime;
    stats.clockUSBTime = _clockAnchor.usbTime;
    stats.clockUSBFrameNumber = _clockAnchor.usbFrameNumber;
    stats.clockUSBFrameHostTime = _clockAnchor.usbFrameHostTime;
    stats.clockHostTicksPerUSBFrame = _clockAnchor.hostTicksPerUSBFrame;
    stats.clockUSBFrameSamples = _clockAnchor.usbFrameSamples;
    stats.clockUSBFrameResyncs = _clockAnchor.usbFrameResyncs;
    stats.clockSeed = _clockAnchor.seed;
    stats.clockFramesObserved = _clockAnchor.framesObserved;
    stats.clockAcceptedAnchors = _clockAnchor.acceptedAnchors;
    stats.clockRejectedAnchors = _clockAnchor.rejectedAnchors;
    stats.clockAnchorResets = _clockAnchorResets;
    pthread_mutex_unlock(&_clockAnchorMutex);

    stats.playbackNextFrameNumber = _nextPlaybackFrameNumber;

#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
    OpenA8DJCadenceRangeSnapshot cadenceSnapshot =
        CadenceSnapshotRange(&_cadenceDiagnostics.captureUSBTimestampDelta);
    stats.captureUSBTimestampDeltaMin = cadenceSnapshot.min;
    stats.captureUSBTimestampDeltaMax = cadenceSnapshot.max;
    stats.captureUSBTimestampDeltaSum = cadenceSnapshot.sum;
    stats.captureUSBTimestampDeltaSamples = cadenceSnapshot.samples;
    cadenceSnapshot = CadenceSnapshotRange(&_cadenceDiagnostics.captureRequestCount);
    stats.captureRequestCountMin = cadenceSnapshot.min;
    stats.captureRequestCountMax = cadenceSnapshot.max;
    stats.captureRequestCountSum = cadenceSnapshot.sum;
    stats.captureRequestCountSamples = cadenceSnapshot.samples;
    cadenceSnapshot = CadenceSnapshotRange(&_cadenceDiagnostics.captureCompleteCount);
    stats.captureCompleteCountMin = cadenceSnapshot.min;
    stats.captureCompleteCountMax = cadenceSnapshot.max;
    stats.captureCompleteCountSum = cadenceSnapshot.sum;
    stats.captureCompleteCountSamples = cadenceSnapshot.samples;
    cadenceSnapshot = CadenceSnapshotRange(&_cadenceDiagnostics.captureOffset);
    stats.captureOffsetMin = cadenceSnapshot.min;
    stats.captureOffsetMax = cadenceSnapshot.max;
    stats.captureOffsetSum = cadenceSnapshot.sum;
    stats.captureOffsetSamples = cadenceSnapshot.samples;
    cadenceSnapshot = CadenceSnapshotRange(&_cadenceDiagnostics.playbackQueueBytes);
    stats.playbackQueueBytesMin = cadenceSnapshot.min;
    stats.playbackQueueBytesMax = cadenceSnapshot.max;
    stats.playbackQueueBytesSum = cadenceSnapshot.sum;
    stats.playbackQueueBytesSamples = cadenceSnapshot.samples;
    cadenceSnapshot = CadenceSnapshotRange(&_cadenceDiagnostics.playbackQueueTransactions);
    stats.playbackQueueTransactionsMin = cadenceSnapshot.min;
    stats.playbackQueueTransactionsMax = cadenceSnapshot.max;
    stats.playbackQueueTransactionsSum = cadenceSnapshot.sum;
    stats.playbackQueueTransactionsSamples = cadenceSnapshot.samples;
    cadenceSnapshot = CadenceSnapshotRange(&_cadenceDiagnostics.playbackQueueRequestCount);
    stats.playbackQueueRequestCountMin = cadenceSnapshot.min;
    stats.playbackQueueRequestCountMax = cadenceSnapshot.max;
    stats.playbackQueueRequestCountSum = cadenceSnapshot.sum;
    stats.playbackQueueRequestCountSamples = cadenceSnapshot.samples;
    cadenceSnapshot = CadenceSnapshotRange(&_cadenceDiagnostics.playbackInFlightAtQueue);
    stats.playbackInFlightAtQueueMin = cadenceSnapshot.min;
    stats.playbackInFlightAtQueueMax = cadenceSnapshot.max;
    stats.playbackInFlightAtQueueSum = cadenceSnapshot.sum;
    stats.playbackInFlightAtQueueSamples = cadenceSnapshot.samples;
    cadenceSnapshot = CadenceSnapshotRange(&_cadenceDiagnostics.playbackInFlightAtCompletion);
    stats.playbackInFlightAtCompletionMin = cadenceSnapshot.min;
    stats.playbackInFlightAtCompletionMax = cadenceSnapshot.max;
    stats.playbackInFlightAtCompletionSum = cadenceSnapshot.sum;
    stats.playbackInFlightAtCompletionSamples = cadenceSnapshot.samples;
    cadenceSnapshot = CadenceSnapshotRange(&_cadenceDiagnostics.playbackCompleteCount);
    stats.playbackCompleteCountMin = cadenceSnapshot.min;
    stats.playbackCompleteCountMax = cadenceSnapshot.max;
    stats.playbackCompleteCountSum = cadenceSnapshot.sum;
    stats.playbackCompleteCountSamples = cadenceSnapshot.samples;
    cadenceSnapshot = CadenceSnapshotRange(&_cadenceDiagnostics.playbackRequestCount);
    stats.playbackRequestCountMin = cadenceSnapshot.min;
    stats.playbackRequestCountMax = cadenceSnapshot.max;
    stats.playbackRequestCountSum = cadenceSnapshot.sum;
    stats.playbackRequestCountSamples = cadenceSnapshot.samples;
    stats.captureCompletionDeltaOutliers = atomic_load(&_cadenceDiagnostics.captureCompletionDeltaOutliers);
    stats.captureCompletionDeltaOutlierMax = atomic_load(&_cadenceDiagnostics.captureCompletionDeltaOutlierMax);
    stats.captureCompletionDeltaOutlierSum = atomic_load(&_cadenceDiagnostics.captureCompletionDeltaOutlierSum);
    stats.playbackCompletionDeltaOutliers = atomic_load(&_cadenceDiagnostics.playbackCompletionDeltaOutliers);
    stats.playbackCompletionDeltaOutlierMax = atomic_load(&_cadenceDiagnostics.playbackCompletionDeltaOutlierMax);
    stats.playbackCompletionDeltaOutlierSum = atomic_load(&_cadenceDiagnostics.playbackCompletionDeltaOutlierSum);
    stats.captureToPlaybackQueueDeltaOutliers = atomic_load(&_cadenceDiagnostics.captureToPlaybackQueueDeltaOutliers);
    stats.captureToPlaybackQueueDeltaOutlierMax = atomic_load(&_cadenceDiagnostics.captureToPlaybackQueueDeltaOutlierMax);
    stats.captureToPlaybackQueueDeltaOutlierSum = atomic_load(&_cadenceDiagnostics.captureToPlaybackQueueDeltaOutlierSum);
    stats.captureUSBTimestampOutOfOrder = atomic_load(&_cadenceDiagnostics.captureUSBTimestampOutOfOrder);
    stats.captureUSBTimestampRepeated = atomic_load(&_cadenceDiagnostics.captureUSBTimestampRepeated);
    stats.captureUSBTimestampZero = atomic_load(&_cadenceDiagnostics.captureUSBTimestampZero);
    stats.captureLayoutSignatureSum = atomic_load(&_cadenceDiagnostics.captureLayoutSignatureSum);
    stats.playbackQueueAttempts = atomic_load(&_cadenceDiagnostics.playbackQueueAttempts);
    stats.playbackQueueLayoutSignatureSum = atomic_load(&_cadenceDiagnostics.playbackQueueLayoutSignatureSum);
    stats.playbackZeroCompleteTransactions = atomic_load(&_cadenceDiagnostics.playbackZeroCompleteTransactions);
    stats.playbackLayoutSignatureSum = atomic_load(&_cadenceDiagnostics.playbackLayoutSignatureSum);
#endif

    return stats;
}

- (void)startStreamKeepalive
{
#if OPENA8DJ_ENABLE_STREAM_KEEPALIVE
    if (_streamKeepaliveTimer != nil) {
        return;
    }
    __weak OpenA8DJUSBEngine *weakSelf = self;
    dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
                                                     0,
                                                     0,
                                                     dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
    if (timer == nil) {
        return;
    }
    dispatch_source_set_timer(timer,
                              dispatch_time(DISPATCH_TIME_NOW, kStreamKeepaliveIntervalNsec),
                              kStreamKeepaliveIntervalNsec,
                              kStreamKeepaliveLeewayNsec);
    dispatch_source_set_event_handler(timer, ^{
        OpenA8DJUSBEngine *strongSelf = weakSelf;
        if (strongSelf == nil || !atomic_load(&strongSelf->_streaming)) {
            return;
        }
        (void)atomic_load(&strongSelf->_playbackTransfersInFlight);
    });
    _streamKeepaliveTimer = timer;
    dispatch_resume(timer);
#endif
}

- (void)stopStreamKeepalive
{
#if OPENA8DJ_ENABLE_STREAM_KEEPALIVE
    dispatch_source_t timer = _streamKeepaliveTimer;
    _streamKeepaliveTimer = nil;
    if (timer != nil) {
        dispatch_source_cancel(timer);
    }
#endif
}

- (void)sendStreamStatsToClient:(int)fd
{
    OpenA8DJStreamStatsPayload stats = [self streamStatsSnapshot];
    (void)IPCSend(fd, kIPCTypeStreamStats, &stats, sizeof(stats));
}

- (void)resetClockAnchorWithSeedBump:(BOOL)bumpSeed
{
    pthread_mutex_lock(&_clockAnchorMutex);
    uint64_t seed = _clockAnchor.seed;
    uint64_t rejectedAnchors = _clockAnchor.rejectedAnchors;
    uint64_t acceptedAnchors = _clockAnchor.acceptedAnchors;
    memset(&_clockAnchor, 0, sizeof(_clockAnchor));
    _clockAnchor.sampleRate = _sampleRate;
    _clockAnchor.seed = bumpSeed ? seed + 1 : seed;
    _clockAnchor.rejectedAnchors = rejectedAnchors;
    _clockAnchor.acceptedAnchors = acceptedAnchors;
    _clockAnchorResets++;
    pthread_mutex_unlock(&_clockAnchorMutex);
}

- (void)resetClockAnchorForNewStream
{
    pthread_mutex_lock(&_clockAnchorMutex);
    uint64_t seed = _clockAnchor.seed + 1;
    memset(&_clockAnchor, 0, sizeof(_clockAnchor));
    _clockAnchor.sampleRate = _sampleRate;
    _clockAnchor.seed = seed;
    _clockAnchorResets = 0;
    pthread_mutex_unlock(&_clockAnchorMutex);
}

- (BOOL)getClockAnchor:(OpenA8DJUSBClockAnchor *)outAnchor
{
    if (outAnchor == NULL) {
        return NO;
    }
    pthread_mutex_lock(&_clockAnchorMutex);
    *outAnchor = _clockAnchor;
    BOOL valid = _clockAnchor.valid;
    pthread_mutex_unlock(&_clockAnchorMutex);
    return valid;
}

- (BOOL)sampleStableUSBFrame:(uint64_t *)outFrame hostTime:(uint64_t *)outHostTime
{
#if !OPENA8DJ_ENABLE_USB_CLOCK_ANCHOR || !OPENA8DJ_ENABLE_USB_STABLE_FRAME_POLL
    (void)outFrame;
    (void)outHostTime;
    return NO;
#else
    if (_interface == nil || outFrame == NULL || outHostTime == NULL) {
        return NO;
    }

    for (uint32_t attempt = 0; attempt < 4; attempt++) {
        uint64_t firstFrame = [_interface frameNumberWithTime:NULL];
        uint64_t hostTime = mach_absolute_time();
        uint64_t secondFrame = [_interface frameNumberWithTime:NULL];
        if (firstFrame != 0 && firstFrame == secondFrame) {
            *outFrame = firstFrame;
            *outHostTime = hostTime;
            return YES;
        }
    }
    return NO;
#endif
}

- (void)updateUSBFrameClockWithFrame:(uint64_t)frameNumber hostTime:(uint64_t)hostTime
{
    if (frameNumber == 0 || hostTime == 0) {
        return;
    }

    double ticksPerSecond = MachTicksPerSecond();
    pthread_mutex_lock(&_clockAnchorMutex);
    if (_clockAnchor.usbFrameNumber != 0 && frameNumber > _clockAnchor.usbFrameNumber) {
        uint64_t frameDelta = frameNumber - _clockAnchor.usbFrameNumber;
        uint64_t hostDelta = hostTime > _clockAnchor.usbFrameHostTime ?
            hostTime - _clockAnchor.usbFrameHostTime : 0;
        if (hostDelta > 0 && ticksPerSecond > 0.0) {
            double measuredTicks = (double)hostDelta / (double)frameDelta;
            double maxReasonableTicks = ticksPerSecond / 100.0;
            if (measuredTicks > 0.0 && measuredTicks < maxReasonableTicks) {
                if (_clockAnchor.hostTicksPerUSBFrame > 0.0) {
                    _clockAnchor.hostTicksPerUSBFrame =
                        ((_clockAnchor.hostTicksPerUSBFrame * 1023.0) + measuredTicks) / 1024.0;
                } else {
                    _clockAnchor.hostTicksPerUSBFrame = measuredTicks;
                }
                _clockAnchor.usbFrameSamples++;
            } else {
                _clockAnchor.usbFrameResyncs++;
            }
        }
    } else if (_clockAnchor.usbFrameNumber != 0 && frameNumber < _clockAnchor.usbFrameNumber) {
        _clockAnchor.usbFrameResyncs++;
    }
    _clockAnchor.usbFrameNumber = frameNumber;
    _clockAnchor.usbFrameHostTime = hostTime;
    pthread_mutex_unlock(&_clockAnchorMutex);
}

- (void)updateClockAnchorWithUSBTime:(uint64_t)usbTime decodedFrames:(uint32_t)decodedFrames
{
#if OPENA8DJ_ENABLE_USB_CLOCK_ANCHOR
    uint64_t usbFrame = 0;
    uint64_t usbFrameHostTime = 0;
    if ([self sampleStableUSBFrame:&usbFrame hostTime:&usbFrameHostTime]) {
        [self updateUSBFrameClockWithFrame:usbFrame hostTime:usbFrameHostTime];
    }

    if (decodedFrames == 0 || !HostTimeLooksLikeMachTime(usbTime)) {
        if (decodedFrames > 0 && usbTime != 0) {
            pthread_mutex_lock(&_clockAnchorMutex);
            _clockAnchor.rejectedAnchors++;
            pthread_mutex_unlock(&_clockAnchorMutex);
        }
        return;
    }

    pthread_mutex_lock(&_clockAnchorMutex);
    double nextSampleTime = _clockAnchor.sampleTime + (double)decodedFrames;
    bool accept = true;
    bool reset = false;
    if (_clockAnchor.valid) {
        if (usbTime <= _clockAnchor.usbTime || nextSampleTime <= _clockAnchor.sampleTime) {
            accept = false;
        } else {
            double ticksPerFrame = MachTicksPerFrame(_sampleRate);
            if (ticksPerFrame > 0.0) {
                double expectedFrames = (double)(usbTime - _clockAnchor.usbTime) / ticksPerFrame;
                double sampleDelta = nextSampleTime - _clockAnchor.sampleTime;
                double frameError = fabs(expectedFrames - sampleDelta);
                if (frameError > _sampleRate) {
                    reset = true;
                }
            }
        }
    }

    if (reset) {
        uint64_t seed = _clockAnchor.seed + 1;
        uint64_t acceptedAnchors = _clockAnchor.acceptedAnchors;
        uint64_t rejectedAnchors = _clockAnchor.rejectedAnchors;
        memset(&_clockAnchor, 0, sizeof(_clockAnchor));
        _clockAnchor.seed = seed;
        _clockAnchor.acceptedAnchors = acceptedAnchors;
        _clockAnchor.rejectedAnchors = rejectedAnchors;
        _clockAnchorResets++;
    }

    if (accept) {
        uint64_t anchorHostTime = usbTime;
        if (_clockAnchor.valid && _clockAnchor.hostTime != 0) {
            double ticksPerFrame = MachTicksPerFrame(_sampleRate);
            if (ticksPerFrame > 0.0) {
                uint64_t predictedHostTime =
                    _clockAnchor.hostTime + (uint64_t)llround((double)decodedFrames * ticksPerFrame);
                if (predictedHostTime > _clockAnchor.hostTime) {
                    anchorHostTime = JitterFilter(OPENA8DJ_USB_ANCHOR_FILTER,
                                                  predictedHostTime,
                                                  usbTime);
                    if (anchorHostTime <= _clockAnchor.hostTime) {
                        anchorHostTime = predictedHostTime;
                    }
                }
            }
        }
        _clockAnchor.valid = true;
        _clockAnchor.sampleRate = _sampleRate;
        _clockAnchor.sampleTime = nextSampleTime;
        _clockAnchor.hostTime = anchorHostTime;
        _clockAnchor.usbTime = usbTime;
        _clockAnchor.framesObserved += decodedFrames;
        _clockAnchor.acceptedAnchors++;
    } else {
        _clockAnchor.rejectedAnchors++;
    }
    pthread_mutex_unlock(&_clockAnchorMutex);
#else
    (void)usbTime;
    (void)decodedFrames;
#endif
}

- (void)addInputStatsForStream:(uint32_t)stream left:(float)left right:(float)right
{
    if (stream >= kStreams) {
        return;
    }
    OpenA8DJInputStatsPayload stats = {0};
    InputStatsAccumulate(&stats, stream, left, right);
    [self addInputStatsBatch:&stats];
}

- (void)armTimecodeOnWriterQueueWithProfile:(uint8_t)profile
{
    void (^armBlock)(void) = ^{
        pthread_mutex_lock(&gDriverModeMutex);
        uint32_t fallback = gDriverModeState.effectiveMode;
        if (fallback ==
            kOpenA8DJDriverModeTimecodeOptimized) {
            fallback = gTimecodeState.fallbackMode;
        }
        if (!gTimecodeState.armed) {
            OpenA8DJTimecodeClassifierInit(
                &self->_timecodeClassifier, self->_sampleRate);
            pthread_mutex_lock(
                &self->_timecodePublishedWindowMutex);
            memset(&self->_timecodePublishedWindow, 0,
                   sizeof(self->_timecodePublishedWindow));
            pthread_mutex_unlock(
                &self->_timecodePublishedWindowMutex);
            atomic_store(
                &self->_timecodeLastCompleteHostTime,
                mach_absolute_time());
        }
        bool accepted = OpenA8DJTimecodeArm(
            &gTimecodeState, fallback, profile,
            self->_sampleRate,
            atomic_load(&gCoreAudioBufferFrames));
        if (accepted && gTimecodeState.armed) {
            atomic_store(&gTimecodeClassificationArmed, true);
        }
        pthread_mutex_unlock(&gDriverModeMutex);
    };
    if (dispatch_get_specific(
            &gTimecodeWriterQueueSpecificKey) ==
        &gTimecodeWriterQueueSpecificKey) {
        armBlock();
    } else {
        dispatch_sync(_queue, armBlock);
    }
}

- (void)publishTimecodeWindow:
    (const OpenA8DJTimecodeWindow *)window
{
    pthread_mutex_lock(&_timecodePublishedWindowMutex);
    _timecodePublishedWindow = *window;
    pthread_mutex_unlock(&_timecodePublishedWindowMutex);
}

- (void)evaluateTimecodeWindow:(const OpenA8DJTimecodeWindow *)window
{
    OpenA8DJControlPayload control;
    [self loadControlPayload:&control];
    uint8_t profile = TimecodeProfileForControl(&control);

    pthread_mutex_lock(&gDriverModeMutex);
    if (!gTimecodeState.armed) {
        pthread_mutex_unlock(&gDriverModeMutex);
        return;
    }
    if (profile == kOpenA8DJTimecodeProfileUnavailable) {
        bool alreadyWaiting =
            gTimecodeState.armState ==
                kOpenA8DJTimecodeWaitingProfile &&
            !gTimecodeState.profileVerified;
        gTimecodeState.profileVerified = 0;
        gTimecodeState.electricalProfile =
            kOpenA8DJTimecodeProfileUnavailable;
        gTimecodeState.qualified = 0;
        gTimecodeState.eligibleWindows = 0;
        if (gTimecodeState.optimizedActive ||
            gDriverModeState.effectiveMode ==
                kOpenA8DJDriverModeTimecodeOptimized) {
            TimecodeFailOpenLocked(
                kOpenA8DJTimecodeFailWrongProfile, false);
        } else {
            gTimecodeState.armState =
                kOpenA8DJTimecodeWaitingProfile;
            gTimecodeState.lastFailOpenReason =
                kOpenA8DJTimecodeFailWrongProfile;
            if (!alreadyWaiting) {
                gTimecodeState.counters.profileTrips++;
                gTimecodeState.generation++;
            }
        }
        pthread_mutex_unlock(&gDriverModeMutex);
        return;
    }
    if (!gTimecodeState.profileVerified ||
        profile != gTimecodeState.electricalProfile) {
        bool wasOptimized =
            gTimecodeState.optimizedActive ||
            gDriverModeState.effectiveMode ==
                kOpenA8DJDriverModeTimecodeOptimized;
        gTimecodeState.electricalProfile = profile;
        gTimecodeState.profileVerified = 1;
        gTimecodeState.qualified = 0;
        gTimecodeState.eligibleWindows = 0;
        gTimecodeState.dropoutWindows = 0;
        if (wasOptimized) {
            TimecodeFailOpenLocked(
                kOpenA8DJTimecodeFailWrongProfile, false);
        } else {
            gTimecodeState.armState =
                kOpenA8DJTimecodeQualifying;
            gTimecodeState.generation++;
        }
        pthread_mutex_unlock(&gDriverModeMutex);
        return;
    }
    uint8_t decision = OpenA8DJTimecodeObserveWindow(
        &gTimecodeState, window, gDriverModeStreaming);
    if (decision == UINT8_MAX) {
        if (!OpenA8DJDriverModeSet(
                &gDriverModeState,
                kOpenA8DJDriverModeTimecodeOptimized,
                gDriverModeStreaming,
                DriverModeProductionPreflight,
                NULL)) {
            TimecodeFailOpenLocked(kOpenA8DJTimecodeFailApplyFailed, true);
        } else if (!gDriverModeState.pending) {
            TimecodeMarkActivatedLocked();
        }
    } else if (decision != kOpenA8DJTimecodeFailNone) {
        bool disarm = decision != kOpenA8DJTimecodeFailAllowedPairDropout;
        TimecodeFailOpenLocked(decision, disarm);
    }
    pthread_mutex_unlock(&gDriverModeMutex);
}

- (void)addTimecodePhysicalFrame:(const float *)samples
{
    if (!atomic_load(&gTimecodeClassificationArmed)) {
        return;
    }
    OpenA8DJTimecodeWindow window;
    bool complete = OpenA8DJTimecodeClassifierFeedFrame(
        &_timecodeClassifier, samples, &window);
    if (complete) {
        [self publishTimecodeWindow:&window];
        atomic_store(&_timecodeLastCompleteHostTime,
                     mach_absolute_time());
        [self evaluateTimecodeWindow:&window];
    }
}

- (void)addInputStatsBatch:(const OpenA8DJInputStatsPayload *)stats
{
    if (!InputStatsHasFrames(stats)) {
        return;
    }
    pthread_mutex_lock(&_inputStatsMutex);
    for (uint32_t stream = 0; stream < kStreams; stream++) {
        _inputStats.frames[stream] += stats->frames[stream];
        _inputStats.leftSquare[stream] += stats->leftSquare[stream];
        _inputStats.rightSquare[stream] += stats->rightSquare[stream];
        _inputStats.cross[stream] += stats->cross[stream];
        if (stats->leftPeak[stream] > _inputStats.leftPeak[stream]) {
            _inputStats.leftPeak[stream] = stats->leftPeak[stream];
        }
        if (stats->rightPeak[stream] > _inputStats.rightPeak[stream]) {
            _inputStats.rightPeak[stream] = stats->rightPeak[stream];
        }
    }
    pthread_mutex_unlock(&_inputStatsMutex);
}

- (void)sendInputStatsToClient:(int)fd
{
    OpenA8DJInputStatsPayload stats;
    pthread_mutex_lock(&_inputStatsMutex);
    stats = _inputStats;
    memset(&_inputStats, 0, sizeof(_inputStats));
    pthread_mutex_unlock(&_inputStatsMutex);
    (void)IPCSend(fd, kIPCTypeInputStats, &stats, sizeof(stats));
}

- (void)sendDriverModeStateToClient:(int)fd
{
    struct {
        OpenA8DJDriverModeStatePayload state;
        OpenA8DJTimecodeStatePayload timecode;
    } __attribute__((packed)) reply;
    OpenA8DJTimecodeStatePayload timecode =
        [self timecodeStateSnapshot];
    reply.state = timecode.driverMode;
    reply.timecode = timecode;
    (void)IPCSend(fd, kIPCTypeDriverModeState, &reply, sizeof(reply));
}

- (OpenA8DJTimecodeStatePayload)timecodeStateSnapshot
{
    OpenA8DJTimecodeStatePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.schemaVersion = kOpenA8DJTimecodeSchemaVersion;
    payload.evidenceKind = 1; /* observed_activity */
    payload.intentObserved = 0;
    pthread_mutex_lock(&gDriverModeMutex);
    OpenA8DJDriverModeMakeStatePayload(
        &gDriverModeState, gDriverModeStreaming, &payload.driverMode);
    payload.timecode = gTimecodeState;
    pthread_mutex_unlock(&gDriverModeMutex);
    pthread_mutex_lock(&_timecodePublishedWindowMutex);
    payload.latestWindow = _timecodePublishedWindow;
    pthread_mutex_unlock(&_timecodePublishedWindowMutex);
    uint64_t lastComplete =
        atomic_load(&_timecodeLastCompleteHostTime);
    uint64_t freshness =
        MachTicksForNanoseconds(500000000ull);
    uint64_t now = mach_absolute_time();
    if (lastComplete == 0 || freshness == 0 ||
        now < lastComplete || now - lastComplete > freshness) {
        memset(&payload.latestWindow, 0,
               sizeof(payload.latestWindow));
    }
    return payload;
}

- (void)sendTimecodeStateToClient:(int)fd
{
    OpenA8DJTimecodeStatePayload payload = [self timecodeStateSnapshot];
    (void)IPCSend(fd, kIPCTypeTimecodeOptimizedState,
                  &payload, sizeof(payload));
}

- (void)sendTimecodeRejectionToClient:(int)fd
                               reason:(uint8_t)reason
{
    OpenA8DJTimecodeStatePayload payload =
        [self timecodeStateSnapshot];
    payload.rejectionReason = reason;
    (void)IPCSend(fd, kIPCTypeTimecodeOptimizedState,
                  &payload, sizeof(payload));
}

- (void)armTimecodeForClient:(int)fd
                     payload:(const uint8_t *)bytes
                      length:(NSUInteger)length
{
    OpenA8DJTimecodeArmPayload request;
    uint8_t rejection = kOpenA8DJTimecodeRejectionNone;
    if (!OpenA8DJTimecodeValidateArmPayloadDetailed(
            bytes, length, &request, &rejection)) {
        [self sendTimecodeRejectionToClient:fd
                                     reason:rejection];
        return;
    }
    OpenA8DJControlPayload control;
    bool freshControl = [self readControls];
    if (freshControl) {
        [self loadControlPayload:&control];
    } else {
        memset(&control, 0, sizeof(control));
    }
    uint8_t profile = freshControl ?
        TimecodeProfileForControl(&control) :
        kOpenA8DJTimecodeProfileUnavailable;
    [self armTimecodeOnWriterQueueWithProfile:profile];
    [self sendTimecodeStateToClient:fd];
}

- (void)disarmTimecodeForClient:(int)fd
{
    pthread_mutex_lock(&gDriverModeMutex);
    TimecodeFailOpenLocked(
        kOpenA8DJTimecodeFailExplicitDisarm, true);
    pthread_mutex_unlock(&gDriverModeMutex);
    [self sendTimecodeStateToClient:fd];
}

- (void)rejectDriverModeRequestForClient:(int)fd reason:(uint8_t)reason
{
    OpenA8DJDriverModeStatePayload state = DriverModeRejectRequest(reason);
    (void)IPCSend(fd, kIPCTypeDriverModeState, &state, sizeof(state));
}

- (void)handleIPCMessageType:(uint8_t)type payload:(const uint8_t *)payload length:(NSUInteger)length client:(int)fd
{
    switch (type) {
        case kIPCTypeHello: {
            const char status[] = "OpenA8DJ HAL USB bridge ready";
            (void)IPCSend(fd, kIPCTypeStatus, status, (uint16_t)(sizeof(status) - 1));
            [self sendControlStateToClient:fd];
            break;
        }
        case kIPCTypeMidiToDevice:
            [self sendMIDIBytes:payload length:length];
            break;
        case kIPCTypeControlGet:
            [self sendControlStateToClient:fd];
            break;
        case kIPCTypeControlSet:
            if (length >= offsetof(OpenA8DJControlPayload, inputDecodeEnabled)) {
                pthread_mutex_lock(&gDriverModeMutex);
                TimecodeFailOpenLocked(
                    kOpenA8DJTimecodeFailWrongProfile, false);
                pthread_mutex_unlock(&gDriverModeMutex);
                OpenA8DJControlPayload state;
                [self loadControlPayload:&state];
                NSUInteger copyLength = length < sizeof(state) ? length : sizeof(state);
                memcpy(&state, payload, copyLength);
                [self storeControlPayload:&state];
                (void)[self writeControls];
                [self loadControlPayload:&state];
                uint8_t profile = TimecodeProfileForControl(&state);
                pthread_mutex_lock(&gDriverModeMutex);
                if (gTimecodeState.armed) {
                    gTimecodeState.electricalProfile = profile;
                    gTimecodeState.profileVerified =
                        profile != kOpenA8DJTimecodeProfileUnavailable;
                    gTimecodeState.qualified = 0;
                    gTimecodeState.eligibleWindows = 0;
                    if (gDriverModeState.effectiveMode !=
                            kOpenA8DJDriverModeTimecodeOptimized &&
                        !gDriverModeState.pending) {
                        gTimecodeState.armState =
                            gTimecodeState.profileVerified ?
                                kOpenA8DJTimecodeQualifying :
                                kOpenA8DJTimecodeWaitingProfile;
                    }
                }
                pthread_mutex_unlock(&gDriverModeMutex);
                [self sendControlStateToClient:fd];
            }
            break;
        case kIPCTypeInputStatsGet:
            [self sendInputStatsToClient:fd];
            break;
        case kIPCTypeStreamStatsGet:
            [self sendStreamStatsToClient:fd];
            break;
        case kIPCTypeDriverModeGet:
            if (length == 0) {
                [self sendDriverModeStateToClient:fd];
            } else {
                [self rejectDriverModeRequestForClient:fd
                                                reason:kOpenA8DJDriverModeRejectionBadLength];
            }
            break;
        case kIPCTypeDriverModeSet: {
            OpenA8DJDriverModeSetPayload request;
            uint8_t rejection = kOpenA8DJDriverModeRejectionNone;
            if (!OpenA8DJDriverModeValidateSetPayload(payload,
                                                       length,
                                                       &request,
                                                       &rejection)) {
                [self rejectDriverModeRequestForClient:fd reason:rejection];
                break;
            }
            OpenA8DJDriverModeStatePayload state =
                DriverModeSetRequested(request.modeID);
            (void)IPCSend(fd, kIPCTypeDriverModeState, &state, sizeof(state));
            break;
        }
        case kIPCTypeTimecodeOptimizedGet:
            if (length == 0) {
                [self sendTimecodeStateToClient:fd];
            } else {
                [self sendTimecodeRejectionToClient:fd
                                             reason:kOpenA8DJTimecodeRejectionBadLength];
            }
            break;
        case kIPCTypeTimecodeOptimizedArm:
            [self armTimecodeForClient:fd payload:payload length:length];
            break;
        case kIPCTypeTimecodeOptimizedDisarm:
            if (length == 0) {
                [self disarmTimecodeForClient:fd];
            } else {
                [self sendTimecodeRejectionToClient:fd
                                             reason:kOpenA8DJTimecodeRejectionBadLength];
            }
            break;
        default:
            break;
    }
}

- (void)handleIPCClient:(int)fd
{
    @autoreleasepool {
        while (atomic_load(&_running)) {
            OpenA8DJIPCHeader header;
            if (!ReadFull(fd, &header, sizeof(header))) {
                break;
            }
            if (header.magic != kIPCMagic || header.version != kIPCVersion || header.length > 4096) {
                break;
            }
            uint8_t payload[4096];
            if (header.length > 0 && !ReadFull(fd, payload, header.length)) {
                break;
            }
            [self handleIPCMessageType:header.type payload:payload length:header.length client:fd];
        }
        [self removeIPCClient:fd];
    }
}

- (void)startIPCServer
{
    if (_ipcListenFd >= 0) {
        return;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        USBTrace("IPC socket create failed");
        return;
    }

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strlcpy(address.sun_path, kIPCSocketPath, sizeof(address.sun_path));
    unlink(kIPCSocketPath);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        USBTrace("IPC bind failed errno=%d %s", errno, strerror(errno));
        close(fd);
        return;
    }
    if (chmod(kIPCSocketPath, 0666) != 0) {
        USBTrace("IPC chmod failed errno=%d %s", errno, strerror(errno));
        close(fd);
        unlink(kIPCSocketPath);
        return;
    }
    if (listen(fd, 8) != 0) {
        USBTrace("IPC listen failed errno=%d %s", errno, strerror(errno));
        close(fd);
        unlink(kIPCSocketPath);
        return;
    }

    _ipcListenFd = fd;
    __weak OpenA8DJUSBEngine *weakSelf = self;
    dispatch_async(_ipcQueue, ^{
        OpenA8DJUSBEngine *strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }
        USBTrace("IPC server listening at %s", kIPCSocketPath);
        while (atomic_load(&strongSelf->_running)) {
            int client = accept(fd, NULL, NULL);
            if (client < 0) {
                if (atomic_load(&strongSelf->_running)) {
                    USBTrace("IPC accept failed");
                }
                break;
            }
            if (!IPCPeerIsAuthorized(client)) {
                USBTrace("IPC peer rejected");
                close(client);
                continue;
            }
            [strongSelf addIPCClient:client];
            [strongSelf sendControlStateToClient:client];
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                [strongSelf handleIPCClient:client];
            });
        }
    });
}

- (void)stopIPCServer
{
    int fd = _ipcListenFd;
    _ipcListenFd = -1;
    if (fd >= 0) {
        close(fd);
    }

    pthread_mutex_lock(&_ipcClientsMutex);
    size_t capacity = sizeof(_ipcClients) / sizeof(_ipcClients[0]);
    for (size_t i = 0; i < capacity; i++) {
        if (_ipcClients[i] >= 0) {
            close(_ipcClients[i]);
            _ipcClients[i] = -1;
        }
    }
    _ipcClientCount = 0;
    pthread_mutex_unlock(&_ipcClientsMutex);
    unlink(kIPCSocketPath);
}

- (BOOL)ensureOpen
{
    if (atomic_load(&_running)) {
        return YES;
    }
    if (![self openUSB]) {
        [self close];
        return NO;
    }
    USBTrace("USB engine opened");
    return YES;
}

- (BOOL)start
{
    if (atomic_load(&_streaming)) {
        return YES;
    }
    if (![self ensureOpen]) {
        return NO;
    }
    [self resetAudioParamsBeforeStream];
    if (![self setAudioParams]) {
        return NO;
    }
    _streamDriverModePolicy = DriverModeBeginStream();
    [self resetStreamStats];
    [self resetClockAnchorForNewStream];
    RingClear(&_inputRing);
    atomic_store(&_inputDecodeActive, false);
    OutputTimelineClear(&_outputTimeline);
    memset(_inputBytes, 0, sizeof(_inputBytes));
    memset(_inputByteCount, 0, sizeof(_inputByteCount));
    if (_spec.dataAlignment == 2) {
        for (uint32_t stream = 0; stream < kStreams; stream++) {
            bool leftFirst = (kInputMode2LeftFirstStreamMask & (1u << stream)) != 0;
            _inputByteCount[stream] = leftFirst ? 0 : kBytesPerSample;
        }
    }
    memset(_pendingInput, 0, sizeof(_pendingInput));
    memset(_pendingPhysicalInput, 0, sizeof(_pendingPhysicalInput));
    _pendingInputMask = 0;
    _inputMode2Index = 0;
    memset(_outputFrameBytes, 0, sizeof(_outputFrameBytes));
    /*
     * The legacy kext initializes its mode-2 input/read cursor at byte offset 2.
     * Playback starts at the second PCM sample byte so that check bytes are not
     * sent as audio payload.
     */
    _outputByteInFrame = OPENA8DJ_OUTPUT_START_BYTE;
    _outputFrameLoaded = false;
    _outputPlaybackPrimed = false;
    _outputHasStartedPlayback = false;
    _outputFramesServed = 0;
    memset(_outputLastFrame, 0, sizeof(_outputLastFrame));
    _outputLastFrameValid = false;
    _outputReplayRunFrames = 0;
    _outputElasticCorrectionCountdown = 0;
    _outputPrefetchIndex = 0;
    _outputPrefetchCount = 0;
    memset(&_pendingOutputFillStats, 0, sizeof(_pendingOutputFillStats));
    _pendingOutputFillTransferCount = 0;
    _playbackScheduleValid = false;
    _nextPlaybackFrameNumber = 0;
#if OPENA8DJ_ENABLE_EXPLICIT_ISOC_SCHEDULING
    atomic_store(&_playbackUseExplicitScheduling, true);
#else
    atomic_store(&_playbackUseExplicitScheduling, false);
#endif
    atomic_store(&_playbackScheduleFailureStreak, 0);
    atomic_store(&_playbackTransfersInFlight, 0);
    _lastCaptureCompletionHostTime = 0;
    _lastPlaybackCompletionHostTime = 0;
    _lastCaptureTransactionUSBTime = 0;
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
    CadenceReset(&_cadenceDiagnostics);
#endif
#if OPENA8DJ_ENABLE_TRACE
    _debugCaptureCheckGroups = 0;
    _debugInputCheckErrors = 0;
    _debugOutputPanicFlags = 0;
    _debugOutputFramesWritten = 0;
    _debugOutputFramesRead = 0;
    _debugOutputUnderruns = 0;
    _debugOutputPeak = 0.0f;
    _debugCaptureTransfers = 0;
    _debugPlaybackTransfers = 0;
    _debugPlaybackTransactionFailures = 0;
    _debugPlaybackShortTransfers = 0;
    _debugPlaybackBytes = 0;
    _debugCaptureQueueFailures = 0;
    _debugPlaybackQueueFailures = 0;
#endif
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
    [self openDiagnosticCapture];
#endif

    atomic_store(&_streaming, true);
    [self startStreamKeepalive];
    __weak OpenA8DJUSBEngine *weakSelf = self;
    dispatch_block_t workerBlock = ^{
        [weakSelf workerLoop];
    };
    if (_streamDriverModePolicy.workerQoS ==
        kOpenA8DJDriverModeWorkerQoSUserInteractive) {
        workerBlock = dispatch_block_create_with_qos_class(
            0, QOS_CLASS_USER_INTERACTIVE, 0, workerBlock);
    }
    dispatch_async(_queue, workerBlock);
    USBTrace("USB engine started");
    return YES;
}

- (BOOL)isStreaming
{
    return atomic_load(&_streaming);
}

- (void)waitForPlaybackTransfersToDrain
{
    for (uint32_t attempt = 0; attempt < 250; attempt++) {
        if (atomic_load(&_playbackTransfersInFlight) == 0) {
            return;
        }
        usleep(1000);
    }
}

- (void)stop
{
    if (!atomic_exchange(&_streaming, false)) {
        return;
    }
    [self stopStreamKeepalive];
    [_capturePipe abortWithError:nil];
    [_playbackPipe abortWithError:nil];
    dispatch_sync(_queue, ^{
    });
    [self waitForPlaybackTransfersToDrain];
    [self flushPendingCompletionQuality];
    [self accumulateOutputFillStats:NULL force:YES];
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
    [self closeDiagnosticCapture];
#endif
    DriverModeEndStream();
    USBTrace("USB engine stopped");
}

- (void)close
{
    [self stop];
    if (!atomic_exchange(&_running, false)) {
        return;
    }
    [self stopIPCServer];
    [_bulkInPipe abortWithError:nil];
    [_bulkOutPipe abortWithError:nil];
    dispatch_sync(_ep1Queue, ^{
    });
    [self closeUSB];
    USBTrace("USB engine closed");
}

- (void)closeUSB
{
    [_capturePipe abortWithError:nil];
    [_playbackPipe abortWithError:nil];
    [_bulkInPipe abortWithError:nil];
    [_bulkOutPipe abortWithError:nil];
    [_interface destroy];
    [_device destroy];
    _capturePipe = nil;
    _playbackPipe = nil;
    _bulkInPipe = nil;
    _bulkOutPipe = nil;
    _interface = nil;
    _device = nil;
    if (_midiSource != 0) {
        MIDIEndpointDispose(_midiSource);
        _midiSource = 0;
    }
    if (_midiDestination != 0) {
        MIDIEndpointDispose(_midiDestination);
        _midiDestination = 0;
    }
    if (_midiClient != 0) {
        MIDIClientDispose(_midiClient);
        _midiClient = 0;
    }
}

- (void)publishMIDIBytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    if (bytes != NULL && length > 0) {
        [self broadcastIPCType:kIPCTypeMidiFromDevice bytes:bytes length:length];
    }
    if (_midiSource == 0 || bytes == NULL || length == 0) {
        return;
    }
    Byte packetStorage[sizeof(MIDIPacketList) + 256];
    MIDIPacketList *packetList = (MIDIPacketList *)packetStorage;
    MIDIPacket *packet = MIDIPacketListInit(packetList);
    packet = MIDIPacketListAdd(packetList,
                               sizeof(packetStorage),
                               packet,
                               mach_absolute_time(),
                               length > 256 ? 256 : (UInt16)length,
                               bytes);
    if (packet != NULL) {
        OSStatus status = MIDIReceived(_midiSource, packetList);
        if (status != noErr) {
            USBTrace("MIDIReceived failed status=%d", (int)status);
        }
    }
}

- (void)sendMIDIBytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    if (_bulkOutPipe == nil || bytes == NULL || length == 0) {
        return;
    }
    NSUInteger offset = 0;
    while (offset < length) {
        NSUInteger chunk = length - offset;
        if (chunk > 61) {
            chunk = 61;
        }
        NSMutableData *outData = [NSMutableData dataWithLength:3 + chunk];
        uint8_t *out = outData.mutableBytes;
        out[0] = kCommandMidiWrite;
        out[1] = 0;
        out[2] = (uint8_t)chunk;
        memcpy(out + 3, bytes + offset, chunk);

        if (![self sendCommandNoReply:kCommandMidiWrite payload:out + 1 payloadLength:2 + chunk]) {
            USBTrace("MIDI_WRITE failed");
            return;
        }
        offset += chunk;
    }
}

- (void)handleMIDIPacketList:(const MIDIPacketList *)packetList
{
    if (packetList == NULL) {
        return;
    }
    const MIDIPacket *packet = &packetList->packet[0];
    for (UInt32 index = 0; index < packetList->numPackets; index++) {
        [self sendMIDIBytes:packet->data length:packet->length];
        packet = MIDIPacketNext(packet);
    }
}

- (void)handleEP1Bytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    if (bytes == NULL || length == 0) {
        return;
    }
    pthread_mutex_lock(&_ep1Mutex);
    if (_pendingActive && bytes[0] == _pendingCommand) {
        _pendingReply = [NSData dataWithBytes:bytes length:length];
        _pendingReady = true;
        pthread_cond_broadcast(&_ep1Cond);
        pthread_mutex_unlock(&_ep1Mutex);
        return;
    }
    pthread_mutex_unlock(&_ep1Mutex);

    if (bytes[0] == kCommandMidiRead && length >= 3) {
        NSUInteger midiLength = bytes[2];
        if (3 + midiLength > length) {
            midiLength = length - 3;
        }
        [self publishMIDIBytes:bytes + 3 length:midiLength];
    }
}

- (void)ep1ReadLoop
{
    while (atomic_load(&_running)) {
        @autoreleasepool {
            NSMutableData *inData = [NSMutableData dataWithLength:64];
            dispatch_semaphore_t readDone = dispatch_semaphore_create(0);
            __block IOReturn readStatus = kIOReturnInvalid;
            __block NSUInteger readBytes = 0;
            NSError *error = nil;
            BOOL queued = [_bulkInPipe enqueueIORequestWithData:inData
                                              completionTimeout:1.0
                                                          error:&error
                                              completionHandler:^(IOReturn status, NSUInteger bytesTransferred) {
                                                  readStatus = status;
                                                  readBytes = bytesTransferred;
                                                  dispatch_semaphore_signal(readDone);
                                              }];
            if (!queued) {
                if (atomic_load(&_running)) {
                    USBTrace("EP1 read queue failed: %s", NSErrorText(error));
                }
                usleep(100000);
                continue;
            }

            dispatch_semaphore_wait(readDone, DISPATCH_TIME_FOREVER);
            if (readStatus == kIOReturnSuccess && readBytes > 0) {
                [self handleEP1Bytes:inData.bytes length:readBytes];
            } else if (readStatus != kIOReturnTimeout &&
                       readStatus != kIOReturnAborted &&
                       atomic_load(&_running)) {
                USBTrace("EP1 read status=0x%08x bytes=%lu", readStatus, (unsigned long)readBytes);
            }
        }
    }
}

- (BOOL)appendInputByte:(uint8_t)byte stream:(uint32_t)stream
{
    if (stream >= kStreams) {
        return NO;
    }
    uint8_t pos = _inputByteCount[stream];
    _inputBytes[stream][pos++] = byte;
    _inputByteCount[stream] = pos;
    if (pos < kChannelsPerStream * kBytesPerSample) {
        return NO;
    }

    const uint8_t *left = &_inputBytes[stream][0];
    const uint8_t *right = &_inputBytes[stream][3];
    float leftSample = (float)S24BEToS32(left) / 8388608.0f;
    float rightSample = (float)S24BEToS32(right) / 8388608.0f;
    _pendingPhysicalInput[stream * 2] = leftSample;
    _pendingPhysicalInput[stream * 2 + 1] = rightSample;
    uint32_t pairBit = 1u << stream;
    if ((atomic_load(&_inputSwapMask) & pairBit) != 0) {
        float tmp = leftSample;
        leftSample = rightSample;
        rightSample = tmp;
    }
    if ((atomic_load(&_inputInvertLeftMask) & pairBit) != 0) {
        leftSample = -leftSample;
    }
    if ((atomic_load(&_inputInvertRightMask) & pairBit) != 0) {
        rightSample = -rightSample;
    }
    _pendingInput[stream * 2] = leftSample;
    _pendingInput[stream * 2 + 1] = rightSample;
    [self addInputStatsForStream:stream left:leftSample right:rightSample];
    _inputByteCount[stream] = 0;
    _pendingInputMask |= (uint8_t)(1u << stream);
    if (_pendingInputMask == 0x0f) {
        [self addTimecodePhysicalFrame:_pendingPhysicalInput];
        float routedInput[kChannels];
        uint32_t sourceMap = atomic_load(&_inputSourceMap);
        for (uint32_t destination = 0; destination < kStreams; destination++) {
            uint32_t source = (sourceMap >> (destination * 4)) & 0x0f;
            if (source >= kStreams) {
                source = destination;
            }
            routedInput[destination * 2] = _pendingInput[source * 2];
            routedInput[destination * 2 + 1] = _pendingInput[source * 2 + 1];
        }
        uint32_t droppedInputFrames = 0;
        RingWriteWithDropped(&_inputRing, routedInput, 1,
                             &droppedInputFrames);
        if (droppedInputFrames != 0) {
            pthread_mutex_lock(&gDriverModeMutex);
            TimecodeFailOpenLocked(
                kOpenA8DJTimecodeFailXRunOrTransportError, true);
            pthread_mutex_unlock(&gDriverModeMutex);
        }
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
        [self appendDiagnosticFrames:&_diagnosticInputBuffer counter:&_diagnosticInputFrames frames:routedInput count:1];
#endif
        _pendingInputMask = 0;
        return YES;
    }
    return NO;
}

- (uint32_t)decodeCaptureBytes:(const uint8_t *)bytes length:(NSUInteger)length
{
    if (_spec.dataAlignment != 2) {
        return 0;
    }
    uint32_t decodedFrames = 0;
    uint64_t inputCheckErrors = 0;
    uint64_t outputPanicFlags = 0;
    OpenA8DJInputStatsPayload inputStatsBatch = {0};
#if OPENA8DJ_ENABLE_INPUT_DECODE && !OPENA8DJ_ENABLE_INPUT_CHECKS
    if (!atomic_load(&_inputDecodeActive)) {
        const NSUInteger groupSize = kStreams * kBytesPerSampleUSB;
        _inputMode2Index += length;
        return (uint32_t)(length / (groupSize * kChannelsPerStream));
    }
#endif
#if !OPENA8DJ_ENABLE_INPUT_DECODE
    const NSUInteger groupSize = kStreams * kBytesPerSampleUSB;
#if !OPENA8DJ_ENABLE_INPUT_CHECKS
    _inputMode2Index += length;
    decodedFrames = (uint32_t)(length / (groupSize * kChannelsPerStream));
#else
    for (NSUInteger offset = 0; offset < length; offset++, _inputMode2Index++) {
        uint32_t groupOffset = (uint32_t)(_inputMode2Index % groupSize);
        if (groupOffset < kStreams) {
            uint32_t stream = groupOffset;
            uint8_t expected = Mode2CheckByte(stream, _inputMode2Index);
            if ((_inputMode2Index / groupSize) >= 4 &&
                (bytes[offset] & 0x3f) != expected) {
                inputCheckErrors++;
            }
            if ((bytes[offset] & 0x80) != 0) {
                outputPanicFlags++;
            }
        }
    }
    decodedFrames = (uint32_t)(length / (groupSize * kChannelsPerStream));
#endif
#else
    for (NSUInteger offset = 0; offset < length; offset++, _inputMode2Index++) {
        uint32_t groupOffset = (uint32_t)(_inputMode2Index % (kStreams * kBytesPerSampleUSB));
        if (groupOffset < kStreams) {
            uint32_t stream = groupOffset;
            uint8_t expected = Mode2CheckByte(stream, _inputMode2Index);
            if ((_inputMode2Index / (kStreams * kBytesPerSampleUSB)) >= 4 &&
                (bytes[offset] & 0x3f) != expected) {
                inputCheckErrors++;
            }
            if ((bytes[offset] & 0x80) != 0) {
                outputPanicFlags++;
            }
#if OPENA8DJ_ENABLE_TRACE
            if (_debugCaptureCheckGroups >= 4 && (bytes[offset] & 0x3f) != expected) {
                _debugInputCheckErrors++;
            }
            if (bytes[offset] & 0x80) {
                _debugOutputPanicFlags++;
            }
            if (stream == kStreams - 1) {
                _debugCaptureCheckGroups++;
            }
#endif
            continue;
        }
        uint32_t stream = groupOffset % kStreams;
        uint8_t pos = _inputByteCount[stream];
        _inputBytes[stream][pos++] = bytes[offset];
        _inputByteCount[stream] = pos;
        if (pos < kChannelsPerStream * kBytesPerSample) {
            continue;
        }

        const uint8_t *left = &_inputBytes[stream][0];
        const uint8_t *right = &_inputBytes[stream][3];
        float leftSample = (float)S24BEToS32(left) / 8388608.0f;
        float rightSample = (float)S24BEToS32(right) / 8388608.0f;
        _pendingPhysicalInput[stream * 2] = leftSample;
        _pendingPhysicalInput[stream * 2 + 1] = rightSample;
        uint32_t pairBit = 1u << stream;
        if ((atomic_load(&_inputSwapMask) & pairBit) != 0) {
            float tmp = leftSample;
            leftSample = rightSample;
            rightSample = tmp;
        }
        if ((atomic_load(&_inputInvertLeftMask) & pairBit) != 0) {
            leftSample = -leftSample;
        }
        if ((atomic_load(&_inputInvertRightMask) & pairBit) != 0) {
            rightSample = -rightSample;
        }
        _pendingInput[stream * 2] = leftSample;
        _pendingInput[stream * 2 + 1] = rightSample;
        InputStatsAccumulate(&inputStatsBatch, stream, leftSample, rightSample);
        _inputByteCount[stream] = 0;
        _pendingInputMask |= (uint8_t)(1u << stream);
        if (_pendingInputMask == 0x0f) {
            [self addTimecodePhysicalFrame:_pendingPhysicalInput];
            float routedInput[kChannels];
            uint32_t sourceMap = atomic_load(&_inputSourceMap);
            for (uint32_t destination = 0; destination < kStreams; destination++) {
                uint32_t source = (sourceMap >> (destination * 4)) & 0x0f;
                if (source >= kStreams) {
                    source = destination;
                }
                routedInput[destination * 2] = _pendingInput[source * 2];
                routedInput[destination * 2 + 1] = _pendingInput[source * 2 + 1];
            }
            uint32_t droppedInputFrames = 0;
            RingWriteWithDropped(&_inputRing, routedInput, 1,
                                 &droppedInputFrames);
            if (droppedInputFrames != 0) {
                pthread_mutex_lock(&gDriverModeMutex);
                TimecodeFailOpenLocked(
                    kOpenA8DJTimecodeFailXRunOrTransportError, true);
                pthread_mutex_unlock(&gDriverModeMutex);
            }
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
            [self appendDiagnosticFrames:&_diagnosticInputBuffer counter:&_diagnosticInputFrames frames:routedInput count:1];
#endif
            _pendingInputMask = 0;
            decodedFrames++;
        }
    }
#endif
    [self addInputStatsBatch:&inputStatsBatch];
    if (inputCheckErrors > 0) {
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, inputCheckErrors)
                               value:inputCheckErrors];
    }
    if (outputPanicFlags > 0) {
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, outputPanicFlags)
                               value:outputPanicFlags];
    }
    return decodedFrames;
}

- (void)refillOutputPrefetch
{
    _outputPrefetchIndex = 0;
    _outputPrefetchCount = OutputTimelineReadFrames(&_outputTimeline,
                                                    _outputPrefetch,
                                                    kOutputPrefetchFrames,
                                                    kChannels,
                                                    _streamDriverModePolicy.outputTargetLatencyFrames,
                                                    _outputPrefetchHaveFrame,
                                                    _outputPrefetchStartupSilence,
                                                    _outputPrefetchElasticDrops);
}

- (void)loadNextOutputFrameWithStats:(OpenA8DJOutputFillStats *)stats
{
    float frame[kChannels] = {0};
    bool haveFrame = false;
    bool startupSilence = false;
    bool replayedFrame = false;
    uint32_t elasticDrops = 0;
    if (_outputPrefetchIndex >= _outputPrefetchCount) {
        [self refillOutputPrefetch];
    }
    if (_outputPrefetchIndex < _outputPrefetchCount) {
        uint32_t prefetchIndex = _outputPrefetchIndex++;
        memcpy(frame,
               &_outputPrefetch[(size_t)prefetchIndex * kChannels],
               sizeof(frame));
        haveFrame = _outputPrefetchHaveFrame[prefetchIndex];
        startupSilence = _outputPrefetchStartupSilence[prefetchIndex];
        elasticDrops = _outputPrefetchElasticDrops[prefetchIndex];
    } else {
        startupSilence = true;
    }
    if (stats != NULL && elasticDrops > 0) {
        stats->elasticDrops += elasticDrops;
    }
    if (haveFrame) {
        memcpy(_outputLastFrame, frame, sizeof(_outputLastFrame));
        _outputLastFrameValid = true;
        _outputReplayRunFrames = 0;
        _outputPlaybackPrimed = true;
        _outputHasStartedPlayback = true;
    } else if (!startupSilence && _outputLastFrameValid &&
               _outputReplayRunFrames < kOutputMaxReplayFrames) {
        float scale = 1.0f;
        if (_outputReplayRunFrames >= kOutputReplayHoldFrames &&
            kOutputMaxReplayFrames > kOutputReplayHoldFrames + 1) {
            uint32_t fadeFrame = _outputReplayRunFrames - kOutputReplayHoldFrames;
            uint32_t fadeFrames = kOutputMaxReplayFrames - kOutputReplayHoldFrames;
            scale = 1.0f - ((float)fadeFrame / (float)fadeFrames);
        }
        for (uint32_t channel = 0; channel < kChannels; channel++) {
            frame[channel] = _outputLastFrame[channel] * scale;
        }
        _outputReplayRunFrames++;
        replayedFrame = true;
    } else if (startupSilence) {
        _outputReplayRunFrames = 0;
    }
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
    uint16_t diagnosticFlags = 0;
    if (haveFrame) diagnosticFlags |= kDiagnosticFlagHaveFrame;
    if (startupSilence) diagnosticFlags |= kDiagnosticFlagStartupSilence;
    if (replayedFrame) diagnosticFlags |= kDiagnosticFlagReplayedFrame;
    if (!haveFrame || startupSilence || replayedFrame || elasticDrops > 0) {
        [self appendDiagnosticEventType:kDiagnosticEventReadAnomaly
                            frameNumber:(int64_t)_outputFramesServed
                                  count:1
                                  flags:diagnosticFlags
                                  value:elasticDrops
                                  extra:_outputReplayRunFrames];
    }
#endif
    _outputFramesServed++;
    if (stats != NULL) {
        stats->framesRead++;
    }
#if OPENA8DJ_ENABLE_TRACE
    _debugOutputFramesRead++;
    if (!haveFrame && !startupSilence) {
        _debugOutputUnderruns++;
    }
#endif
    for (uint32_t stream = 0; stream < kStreams; stream++) {
        FloatToOutputI24(frame[stream * 2], &_outputFrameBytes[stream][0]);
        FloatToOutputI24(frame[stream * 2 + 1], &_outputFrameBytes[stream][3]);
    }
#if OPENA8DJ_ENABLE_OUTPUT_AMPLITUDE_STATS
    if (haveFrame) {
        OutputFillStatsAccumulateAmplitude(stats, frame);
    }
#endif
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
    [self appendDiagnosticFrames:&_diagnosticConsumedBuffer counter:&_diagnosticConsumedFrames frames:frame count:1];
#endif
    if (startupSilence) {
        if (stats != NULL) {
            stats->startupSilenceFrames++;
        }
    } else if (!haveFrame) {
        if (stats != NULL) {
            stats->underruns++;
            stats->activeUnderruns++;
        }
        if (replayedFrame) {
            if (stats != NULL) {
                stats->elasticReplays++;
            }
        }
    }
    _outputFrameLoaded = true;
}

- (void)loadNextOutputFrameIfNeededWithStats:(OpenA8DJOutputFillStats *)stats
{
    if (!_outputFrameLoaded || _outputByteInFrame == 0) {
        [self loadNextOutputFrameWithStats:stats];
    }
}

- (void)fillPlaybackBytes:(uint8_t *)bytes length:(NSUInteger)length
{
    if (_spec.dataAlignment != 2) {
        memset(bytes, 0, length);
        return;
    }
    OpenA8DJOutputFillStats stats = {0};
    NSUInteger i = 0;
    NSUInteger group = 0;
#define OPENA8DJ_LOAD_OUTPUT_FRAME_IF_NEEDED() \
    do { \
        if (!_outputFrameLoaded || _outputByteInFrame == 0) { \
            [self loadNextOutputFrameWithStats:&stats]; \
        } \
    } while (0)
    while (i + (kStreams * kBytesPerSampleUSB) <= length) {
        OPENA8DJ_LOAD_OUTPUT_FRAME_IF_NEEDED();
        bytes[i + 0] = _outputFrameBytes[0][_outputByteInFrame];
        bytes[i + 1] = _outputFrameBytes[1][_outputByteInFrame];
        bytes[i + 2] = _outputFrameBytes[2][_outputByteInFrame];
        bytes[i + 3] = _outputFrameBytes[3][_outputByteInFrame];
        _outputByteInFrame++;
        if (_outputByteInFrame >= kChannelsPerStream * kBytesPerSample) {
            _outputByteInFrame = 0;
        }

        OPENA8DJ_LOAD_OUTPUT_FRAME_IF_NEEDED();
        bytes[i + 4] = _outputFrameBytes[0][_outputByteInFrame];
        bytes[i + 5] = _outputFrameBytes[1][_outputByteInFrame];
        bytes[i + 6] = _outputFrameBytes[2][_outputByteInFrame];
        bytes[i + 7] = _outputFrameBytes[3][_outputByteInFrame];
        _outputByteInFrame++;
        if (_outputByteInFrame >= kChannelsPerStream * kBytesPerSample) {
            _outputByteInFrame = 0;
        }

        uint8_t checkLowBit = (uint8_t)((~group) & 1);
        bytes[i + 8] = checkLowBit;
        bytes[i + 9] = (uint8_t)(2u | checkLowBit);
        bytes[i + 10] = (uint8_t)(4u | checkLowBit);
        bytes[i + 11] = (uint8_t)(6u | checkLowBit);

        OPENA8DJ_LOAD_OUTPUT_FRAME_IF_NEEDED();
        bytes[i + 12] = _outputFrameBytes[0][_outputByteInFrame];
        bytes[i + 13] = _outputFrameBytes[1][_outputByteInFrame];
        bytes[i + 14] = _outputFrameBytes[2][_outputByteInFrame];
        bytes[i + 15] = _outputFrameBytes[3][_outputByteInFrame];
        _outputByteInFrame++;
        if (_outputByteInFrame >= kChannelsPerStream * kBytesPerSample) {
            _outputByteInFrame = 0;
        }

        i += kStreams * kBytesPerSampleUSB;
        group++;
    }
    while (i < length) {
        if ((i % (kStreams * kBytesPerSampleUSB)) == (kStreams * kChannelsPerStream)) {
            for (uint32_t stream = 0; stream < kStreams && i < length; stream++, i++) {
                bytes[i] = Mode2CheckByte(stream, i);
            }
            continue;
        }

        OPENA8DJ_LOAD_OUTPUT_FRAME_IF_NEEDED();
        for (uint32_t stream = 0; stream < kStreams && i < length; stream++, i++) {
            bytes[i] = _outputFrameBytes[stream][_outputByteInFrame];
        }
        _outputByteInFrame++;
        if (_outputByteInFrame >= kChannelsPerStream * kBytesPerSample) {
            _outputByteInFrame = 0;
        }
    }
#undef OPENA8DJ_LOAD_OUTPUT_FRAME_IF_NEEDED
    [self accumulateOutputFillStats:&stats force:NO];
}

- (void)workerLoop
{
    for (uint32_t transfer = 0; transfer < kCaptureQueueDepth && atomic_load(&_streaming); transfer++) {
        [self queueCaptureTransfer];
    }
#if OPENA8DJ_PLAYBACK_CAPTURE_PACED
    USBTrace("isoc async pipeline started captureDepth=%u playback=capture-paced",
             kCaptureQueueDepth);
#else
    USBTrace("isoc async pipeline started captureDepth=%u playback=prefill-on-output target=%u max=%u",
             kCaptureQueueDepth,
             kPlaybackQueueTarget,
             kPlaybackQueueMax);
#endif
}

- (void)queueCaptureTransfer
{
    if (!atomic_load(&_streaming) || _capturePipe == nil) {
        return;
    }

    uint32_t requests[kIsoFramesPerTransfer];
    for (uint32_t frame = 0; frame < kIsoFramesPerTransfer; frame++) {
        requests[frame] = kIsoBytesPerFrame;
    }

    OpenA8DJIsoTransfer *transfer = [self checkoutTransferFromPool:_captureTransferPool
                                                          requests:requests
                                                             count:kIsoFramesPerTransfer
                                                         nextIndex:&_captureTransferPoolCursor];
    if (transfer == nil) {
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, captureQueueFailures)
                               value:1];
        return;
    }
    IOUSBHostIsochronousTransaction *transactions = transfer.transactions.mutableBytes;
#if OPENA8DJ_REUSE_ISOC_COMPLETION_HANDLERS
    OpenA8DJIsoCompletionHandler completionHandler = transfer.captureCompletionHandler;
#else
    OpenA8DJIsoCompletionHandler completionHandler = nil;
#endif
    __weak OpenA8DJUSBEngine *weakSelf = self;
    if (completionHandler == nil) {
        completionHandler = ^(IOReturn status, IOUSBHostIsochronousTransaction *transactionList) {
            OpenA8DJUSBEngine *strongSelf = weakSelf;
            if (strongSelf != nil) {
                [strongSelf handleCaptureTransfer:transfer
                                           status:status
                                     transactions:transactionList];
            }
        };
    }
    NSError *error = nil;
    BOOL queued = [_capturePipe enqueueIORequestWithData:transfer.data
                                         transactionList:transactions
                                    transactionListCount:transfer.transactionCount
                                        firstFrameNumber:0
                                                 options:IOUSBHostIsochronousTransferOptionsNone
                                                   error:&error
                                       completionHandler:completionHandler];
    if (!queued) {
        [self releasePooledTransfer:transfer];
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, captureQueueFailures)
                               value:1];
#if OPENA8DJ_ENABLE_TRACE
        _debugCaptureQueueFailures++;
#endif
        USBTrace("isoc IN queue failed: %s", NSErrorText(error));
        if (atomic_load(&_streaming)) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1000000), _queue, ^{
                [weakSelf queueCaptureTransfer];
            });
        }
    }
}

- (void)handleCaptureTransfer:(OpenA8DJIsoTransfer *)transfer
                        status:(IOReturn)status
                  transactions:(IOUSBHostIsochronousTransaction *)transactions
{
    uint64_t captureCompletionTime = mach_absolute_time();
    uint64_t captureCompletionDelta = 0;
    bool captureHadPreviousCompletion = _lastCaptureCompletionHostTime != 0;
    bool captureInvalidCompletionInterval =
        captureHadPreviousCompletion && captureCompletionTime <= _lastCaptureCompletionHostTime;
    if (captureHadPreviousCompletion && !captureInvalidCompletionInterval) {
        captureCompletionDelta = captureCompletionTime - _lastCaptureCompletionHostTime;
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
        CadenceRecordOutlier(&_cadenceDiagnostics.captureCompletionDeltaOutliers,
                             &_cadenceDiagnostics.captureCompletionDeltaOutlierMax,
                             &_cadenceDiagnostics.captureCompletionDeltaOutlierSum,
                             captureCompletionDelta);
#endif
    }
    _lastCaptureCompletionHostTime = captureCompletionTime;

    uint32_t expectedBytes = CalculateBytesPerPacket(&_spec, _sampleRate);
    uint64_t captureTransactions = 0;
    uint64_t captureByteCount = 0;
    uint64_t failedTransactions = 0;
    uint64_t statusFailures = 0;
    uint64_t zeroCompleteTransactions = 0;
    uint64_t expectedTransactions = 0;
    uint64_t otherByteCountTransactions = 0;
    uint64_t filteredTransactions = 0;
    uint64_t shortTransfers = 0;
    uint64_t completionStatusFailures =
        status != kIOReturnSuccess &&
        !(status == kIOReturnAborted && !atomic_load(&_streaming)) ? 1 : 0;
    uint64_t transactionStatusFailures = 0;
    uint64_t zeroLengthTransactions = 0;
    uint64_t shortTransactions = 0;
#if OPENA8DJ_PLAYBACK_CAPTURE_PACED
    uint32_t playbackRequests[kIsoFramesPerTransfer];
    NSUInteger playbackRequestCount = 0;
#endif
#if OPENA8DJ_ENABLE_TRACE
    uint32_t minCompleteCount = UINT32_MAX;
    uint32_t maxCompleteCount = 0;
    uint32_t traceFailedTransactions = 0;
    uint32_t traceFilteredTransactions = 0;
#endif

    if (status == kIOReturnSuccess && transactions != NULL) {
        const uint8_t *captureBytes = transfer.data.bytes;
        NSUInteger captureLength = transfer.data.length;
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
        uint64_t captureLayoutSignature = 0;
#endif
        for (NSUInteger frame = 0; frame < transfer.transactionCount; frame++) {
            IOUSBHostIsochronousTransaction *transaction = &transactions[frame];
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
            CadenceRecordRange(&_cadenceDiagnostics.captureRequestCount, transaction->requestCount);
            CadenceRecordRange(&_cadenceDiagnostics.captureCompleteCount, transaction->completeCount);
            CadenceRecordRange(&_cadenceDiagnostics.captureOffset, transaction->offset);
            captureLayoutSignature = LayoutSignatureStep(captureLayoutSignature,
                                                         transaction->requestCount,
                                                         transaction->completeCount,
                                                         transaction->offset);
            if (transaction->timeStamp == 0) {
                atomic_fetch_add(&_cadenceDiagnostics.captureUSBTimestampZero, 1);
            } else if (_lastCaptureTransactionUSBTime != 0) {
                if (transaction->timeStamp > _lastCaptureTransactionUSBTime) {
                    CadenceRecordRange(&_cadenceDiagnostics.captureUSBTimestampDelta,
                                       transaction->timeStamp - _lastCaptureTransactionUSBTime);
                } else if (transaction->timeStamp == _lastCaptureTransactionUSBTime) {
                    atomic_fetch_add(&_cadenceDiagnostics.captureUSBTimestampRepeated, 1);
                } else {
                    atomic_fetch_add(&_cadenceDiagnostics.captureUSBTimestampOutOfOrder, 1);
                }
            }
            if (transaction->timeStamp != 0) {
                _lastCaptureTransactionUSBTime = transaction->timeStamp;
            }
#endif
            if (transaction->status != kIOReturnSuccess) {
                statusFailures++;
                failedTransactions++;
                transactionStatusFailures++;
#if OPENA8DJ_ENABLE_TRACE
                traceFailedTransactions++;
#endif
                continue;
            }
            if (transaction->requestCount > 0 &&
                transaction->completeCount < transaction->requestCount) {
                shortTransactions++;
                if (transaction->completeCount == 0) {
                    zeroLengthTransactions++;
                }
            }
            if (transaction->completeCount == 0) {
                zeroCompleteTransactions++;
                failedTransactions++;
                continue;
            }

            uint32_t count = transaction->completeCount;
            if ((NSUInteger)transaction->offset + count > captureLength) {
                failedTransactions++;
#if OPENA8DJ_ENABLE_TRACE
                traceFailedTransactions++;
#endif
                continue;
            }

#if OPENA8DJ_ENABLE_TRACE
            if (count < minCompleteCount) minCompleteCount = count;
            if (count > maxCompleteCount) maxCompleteCount = count;
#endif
#if OPENA8DJ_PLAYBACK_CAPTURE_PACED && !OPENA8DJ_VALID_CAPTURE_OUT_LAYOUT
            if (playbackRequestCount < kIsoFramesPerTransfer) {
                playbackRequests[playbackRequestCount++] = count;
            }
#endif
            /* Drop stale isochronous frames left over from the previous sample rate. */
            if (count != expectedBytes) {
                otherByteCountTransactions++;
                filteredTransactions++;
                if (count < expectedBytes) {
                    shortTransfers++;
                }
#if OPENA8DJ_ENABLE_TRACE
                traceFilteredTransactions++;
#endif
                continue;
            }
#if OPENA8DJ_PLAYBACK_CAPTURE_PACED && OPENA8DJ_VALID_CAPTURE_OUT_LAYOUT
            if (playbackRequestCount < kIsoFramesPerTransfer) {
                playbackRequests[playbackRequestCount++] = count;
            }
#endif
            expectedTransactions++;
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
            [self appendDiagnosticPackedInputBytes:captureBytes + transaction->offset length:count];
#endif
            uint32_t decodedFrames = [self decodeCaptureBytes:captureBytes + transaction->offset length:count];
            [self updateClockAnchorWithUSBTime:transaction->timeStamp decodedFrames:decodedFrames];
            captureTransactions++;
            captureByteCount += count;
        }
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
        if (captureLayoutSignature != 0) {
            atomic_fetch_add(&_cadenceDiagnostics.captureLayoutSignatureSum, captureLayoutSignature);
        }
#endif
    } else if (status != kIOReturnAborted && atomic_load(&_streaming)) {
        failedTransactions++;
        USBTrace("isoc IN complete status=0x%08x", status);
    }

#if OPENA8DJ_ENABLE_TRACE
    _debugCaptureTransfers++;
    if ((_debugCaptureTransfers % 256) == 1) {
        uint32_t ringFrames = OutputTimelineAvailable(&_outputTimeline);
        USBTrace("isoc stats cap=%llu play=%llu rate=%.0f outflight=%u minmax=%u/%u written=%llu read=%llu underruns=%llu peak=%.6f gain=%.3f ring=%u bytepos=%u outbytes=%llu outfail=%llu outshort=%llu failtx=%u filtered=%u qfail=%llu/%llu",
                 _debugCaptureTransfers,
                 _debugPlaybackTransfers,
                 _sampleRate,
                 atomic_load(&_playbackTransfersInFlight),
                 minCompleteCount == UINT32_MAX ? 0 : minCompleteCount,
                 maxCompleteCount,
                 _debugOutputFramesWritten,
                 _debugOutputFramesRead,
                 _debugOutputUnderruns,
                 _debugOutputPeak,
                 (double)OPENA8DJ_OUTPUT_GAIN,
                 ringFrames,
                 _outputByteInFrame,
                 _debugPlaybackBytes,
                 _debugPlaybackTransactionFailures,
                 _debugPlaybackShortTransfers,
                 traceFailedTransactions,
                 traceFilteredTransactions,
                 _debugCaptureQueueFailures,
                 _debugPlaybackQueueFailures);
        USBTrace("mode2 checks groups=%llu inputCheckErrors=%llu outputPanicFlags=%llu outputStartByte=%u",
                 _debugCaptureCheckGroups,
                 _debugInputCheckErrors,
                 _debugOutputPanicFlags,
                 (unsigned)OPENA8DJ_OUTPUT_START_BYTE);
    }
#endif

    uint64_t captureToPlaybackQueueDelta = 0;
#if OPENA8DJ_PLAYBACK_CAPTURE_PACED
    if (playbackRequestCount > 0 && atomic_load(&_streaming)) {
        uint64_t playbackQueueTime = mach_absolute_time();
        if (playbackQueueTime > captureCompletionTime) {
            captureToPlaybackQueueDelta = playbackQueueTime - captureCompletionTime;
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
            CadenceRecordOutlier(&_cadenceDiagnostics.captureToPlaybackQueueDeltaOutliers,
                                 &_cadenceDiagnostics.captureToPlaybackQueueDeltaOutlierMax,
                                 &_cadenceDiagnostics.captureToPlaybackQueueDeltaOutlierSum,
                                 captureToPlaybackQueueDelta);
#endif
        }
    }
#endif

#if OPENA8DJ_ENABLE_HOT_STREAM_STATS
    if (captureHadPreviousCompletion) {
        CompletionQualityRecordJitter(
            &_pendingCaptureCompletionQuality,
            captureInvalidCompletionInterval,
            captureCompletionDelta,
            transfer.transactionCount,
            _ticksPerUSBMicroframe,
            _completionJitterBinThresholdTicks);
    }
    _pendingCaptureCompletionQuality.completionStatusFailures += completionStatusFailures;
    _pendingCaptureCompletionQuality.transactionStatusFailures += transactionStatusFailures;
    _pendingCaptureCompletionQuality.zeroLengthTransactions += zeroLengthTransactions;
    _pendingCaptureCompletionQuality.shortTransactions += shortTransactions;

    uint64_t captureStatsInterval = OPENA8DJ_HOT_STREAM_STATS_INTERVAL < 1 ? 1 : OPENA8DJ_HOT_STREAM_STATS_INTERVAL;
    uint64_t captureStatsCounter = atomic_fetch_add(&_captureHotStreamStatsCounter, 1) + 1;
    if (captureStatsInterval == 1 || (captureStatsCounter % captureStatsInterval) == 0) {
        pthread_mutex_lock(&_streamStatsMutex);
        StreamStatsAddTimingLocked(&_streamStats,
                                   offsetof(OpenA8DJStreamStatsPayload, captureCompletionDeltaMin),
                                   offsetof(OpenA8DJStreamStatsPayload, captureCompletionDeltaMax),
                                   offsetof(OpenA8DJStreamStatsPayload, captureCompletionDeltaSum),
                                   offsetof(OpenA8DJStreamStatsPayload, captureCompletionDeltaSamples),
                                   captureCompletionDelta);
        StreamStatsAddTimingLocked(&_streamStats,
                                   offsetof(OpenA8DJStreamStatsPayload, captureToPlaybackQueueDeltaMin),
                                   offsetof(OpenA8DJStreamStatsPayload, captureToPlaybackQueueDeltaMax),
                                   offsetof(OpenA8DJStreamStatsPayload, captureToPlaybackQueueDeltaSum),
                                   offsetof(OpenA8DJStreamStatsPayload, captureToPlaybackQueueDeltaSamples),
                                   captureToPlaybackQueueDelta);
        StreamStatsFlushCompletionQualityLocked(
            &_streamStats, &_pendingCaptureCompletionQuality, true);
        _streamStats.captureTransfers += 1;
        _streamStats.captureTransactions += captureTransactions;
        _streamStats.captureBytes += captureByteCount;
        _streamStats.captureTransactionFailures += failedTransactions;
        _streamStats.captureStatusFailures += statusFailures;
        _streamStats.captureZeroCompleteTransactions += zeroCompleteTransactions;
        _streamStats.captureExpectedTransactions += expectedTransactions;
        _streamStats.captureOtherByteCountTransactions += otherByteCountTransactions;
        _streamStats.filteredCaptureTransactions += filteredTransactions;
        _streamStats.captureShortTransfers += shortTransfers;
        pthread_mutex_unlock(&_streamStatsMutex);
    }
#else
    (void)captureCompletionDelta;
    (void)captureToPlaybackQueueDelta;
    (void)captureTransactions;
    (void)captureByteCount;
    (void)failedTransactions;
    (void)statusFailures;
    (void)zeroCompleteTransactions;
    (void)expectedTransactions;
    (void)otherByteCountTransactions;
    (void)filteredTransactions;
    (void)shortTransfers;
    (void)completionStatusFailures;
    (void)transactionStatusFailures;
    (void)zeroLengthTransactions;
    (void)shortTransactions;
#endif

#if OPENA8DJ_PLAYBACK_CAPTURE_PACED
#if OPENA8DJ_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE
    if (playbackRequestCount > 0 && atomic_load(&_streaming)) {
        if (kCapturePacedOutputLead <= 1) {
            (void)[self queueCapturePacedPlaybackWithRequests:playbackRequests count:playbackRequestCount];
        } else {
            uint32_t desiredLead = kCapturePacedOutputLead;
            if (desiredLead > kPlaybackQueueMax) {
                desiredLead = kPlaybackQueueMax;
            }
            for (uint32_t queued = 0; queued < desiredLead && atomic_load(&_streaming); queued++) {
                if (atomic_load(&_playbackTransfersInFlight) >= desiredLead) {
                    break;
                }
                if (![self queuePlaybackWithRequests:playbackRequests count:playbackRequestCount]) {
                    break;
                }
            }
        }
    }
#endif
#endif

    [self releasePooledTransfer:transfer];
    if (atomic_load(&_streaming)) {
        [self queueCaptureTransfer];
    }
#if OPENA8DJ_PLAYBACK_CAPTURE_PACED && !OPENA8DJ_QUEUE_PLAYBACK_BEFORE_CAPTURE_REQUEUE
    if (playbackRequestCount > 0 && atomic_load(&_streaming)) {
        if (kCapturePacedOutputLead <= 1) {
            (void)[self queueCapturePacedPlaybackWithRequests:playbackRequests count:playbackRequestCount];
        } else {
            uint32_t desiredLead = kCapturePacedOutputLead;
            if (desiredLead > kPlaybackQueueMax) {
                desiredLead = kPlaybackQueueMax;
            }
            for (uint32_t queued = 0; queued < desiredLead && atomic_load(&_streaming); queued++) {
                if (atomic_load(&_playbackTransfersInFlight) >= desiredLead) {
                    break;
                }
                if (![self queuePlaybackWithRequests:playbackRequests count:playbackRequestCount]) {
                    break;
                }
            }
        }
    }
#endif
}

- (void)softResetPlaybackPipelineWithScheduleReset:(BOOL)scheduleReset
{
    OutputTimelineClear(&_outputTimeline);
    memset(_outputFrameBytes, 0, sizeof(_outputFrameBytes));
    memset(_outputLastFrame, 0, sizeof(_outputLastFrame));
    _outputByteInFrame = OPENA8DJ_OUTPUT_START_BYTE;
    _outputFrameLoaded = false;
    _outputPlaybackPrimed = false;
    _outputHasStartedPlayback = false;
    _pendingPlaybackRequestCount = 0;
    _outputLastFrameValid = false;
    _outputReplayRunFrames = 0;
    _outputElasticCorrectionCountdown = 0;
    _outputPrefetchIndex = 0;
    _outputPrefetchCount = 0;
    if (scheduleReset) {
        _playbackScheduleValid = false;
        _nextPlaybackFrameNumber = 0;
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, playbackScheduleOutOfWindow)
                               value:1];
    }
}

- (void)resyncPlaybackSchedule
{
    _playbackScheduleValid = false;
    _nextPlaybackFrameNumber = 0;
    [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, playbackReschedules)
                           value:1];
}

- (uint64_t)nextPlaybackFirstFrameNumberForCount:(NSUInteger)count
{
#if OPENA8DJ_ENABLE_EXPLICIT_ISOC_SCHEDULING
    if (_interface == nil ||
        count == 0 ||
        !atomic_load(&_playbackUseExplicitScheduling)) {
        return 0;
    }
    uint64_t currentFrame = 0;
    uint64_t currentHostTime = 0;
    if ([self sampleStableUSBFrame:&currentFrame hostTime:&currentHostTime]) {
        [self updateUSBFrameClockWithFrame:currentFrame hostTime:currentHostTime];
    } else {
        currentFrame = [_interface frameNumberWithTime:NULL];
    }
    if (currentFrame == 0) {
        return 0;
    }
    uint64_t minimumFrame = currentFrame + kPlaybackScheduleLeadFrames;
    uint64_t maximumFrame = currentFrame + kPlaybackScheduleMaxLeadFrames;
    if (!_playbackScheduleValid ||
        _nextPlaybackFrameNumber < minimumFrame ||
        _nextPlaybackFrameNumber > maximumFrame) {
        if (_playbackScheduleValid) {
            [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, playbackScheduleOutOfWindow)
                                   value:1];
        }
        _playbackScheduleValid = true;
        _nextPlaybackFrameNumber = minimumFrame;
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, playbackScheduleResets)
                               value:1];
    }
    uint64_t firstFrame = _nextPlaybackFrameNumber;
    _nextPlaybackFrameNumber += count;
    return firstFrame;
#else
    (void)count;
    return 0;
#endif
}

- (void)disableExplicitPlaybackScheduling
{
    if (atomic_exchange(&_playbackUseExplicitScheduling, false)) {
        _playbackScheduleValid = false;
        _nextPlaybackFrameNumber = 0;
        atomic_store(&_playbackScheduleFailureStreak, 0);
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, playbackScheduleFallbacks)
                               value:1];
    }
}

- (void)recordPlaybackScheduleStatus:(IOReturn)status
{
    if (status == kIOReturnIsoTooOld) {
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, playbackScheduleTooOld)
                               value:1];
        [self resyncPlaybackSchedule];
    } else if (status == kIOReturnIsoTooNew) {
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, playbackScheduleTooNew)
                               value:1];
        [self resyncPlaybackSchedule];
    } else {
        return;
    }

    if (atomic_load(&_playbackUseExplicitScheduling)) {
        unsigned int failureStreak = atomic_fetch_add(&_playbackScheduleFailureStreak, 1) + 1;
        if (failureStreak >= kPlaybackScheduleFallbackThreshold) {
            [self disableExplicitPlaybackScheduling];
        }
    }
}

- (void)fillPlaybackQueue
{
    if (!atomic_load(&_streaming) || _playbackPipe == nil) {
        return;
    }

#if !OPENA8DJ_PLAYBACK_CAPTURE_PACED
    uint32_t requestBytes = CalculateBytesPerPacket(&_spec, _sampleRate);
    if (requestBytes == 0) {
        return;
    }
    uint32_t framesPerTransfer =
        (uint32_t)(((uint64_t)requestBytes * (uint64_t)kIsoFramesPerTransfer) /
                   (uint64_t)kOutputUSBBytesPerFrame);
    if (framesPerTransfer == 0) {
        return;
    }
#endif

    for (uint32_t attempt = 0; attempt < kPlaybackQueueTarget; attempt++) {
        uint32_t inFlight = atomic_load(&_playbackTransfersInFlight);
        if (inFlight >= kPlaybackQueueTarget || inFlight >= kPlaybackQueueMax) {
            return;
        }
#if !OPENA8DJ_PLAYBACK_CAPTURE_PACED
        if (OutputTimelineAvailable(&_outputTimeline) < framesPerTransfer) {
            return;
        }
#endif
        if (![self queuePlaybackTransfer]) {
            return;
        }
    }
}

- (BOOL)queuePlaybackTransfer
{
    uint32_t requestBytes = CalculateBytesPerPacket(&_spec, _sampleRate);
    if (requestBytes == 0) {
        return NO;
    }

    uint32_t requests[kPlaybackIsoFramesPerTransfer];
    for (uint32_t frame = 0; frame < kPlaybackIsoFramesPerTransfer; frame++) {
        requests[frame] = requestBytes;
    }
    return [self queuePlaybackWithRequests:requests count:kPlaybackIsoFramesPerTransfer];
}

- (BOOL)queueCapturePacedPlaybackWithRequests:(const uint32_t *)requests count:(NSUInteger)count
{
    if (requests == NULL || count == 0) {
        return NO;
    }
    if (kPlaybackCoalesceTransfers <= 1) {
        return [self queuePlaybackWithRequests:requests count:count];
    }
    if (count > kPlaybackIsoFramesPerTransfer) {
        _pendingPlaybackRequestCount = 0;
        return [self queuePlaybackWithRequests:requests count:count];
    }
    if (_pendingPlaybackRequestCount + count > kPlaybackIsoFramesPerTransfer) {
        BOOL queued = [self queuePlaybackWithRequests:_pendingPlaybackRequests
                                                count:_pendingPlaybackRequestCount];
        _pendingPlaybackRequestCount = 0;
        if (!queued) {
            return NO;
        }
    }
    memcpy(&_pendingPlaybackRequests[_pendingPlaybackRequestCount],
           requests,
           count * sizeof(requests[0]));
    _pendingPlaybackRequestCount += count;
    if (_pendingPlaybackRequestCount >= kPlaybackIsoFramesPerTransfer) {
        BOOL queued = [self queuePlaybackWithRequests:_pendingPlaybackRequests
                                                count:_pendingPlaybackRequestCount];
        _pendingPlaybackRequestCount = 0;
        return queued;
    }
    return YES;
}

- (BOOL)queuePlaybackWithRequests:(const uint32_t *)requests count:(NSUInteger)count
{
    if (!atomic_load(&_streaming) || _playbackPipe == nil || requests == NULL || count == 0) {
        return NO;
    }
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
    atomic_fetch_add(&_cadenceDiagnostics.playbackQueueAttempts, 1);
#endif
    if (atomic_load(&_playbackTransfersInFlight) >= kPlaybackQueueMax) {
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, playbackQueueFailures)
                               value:1];
        return NO;
    }

    OpenA8DJIsoTransfer *transfer = [self checkoutTransferFromPool:_playbackTransferPool
                                                          requests:requests
                                                             count:count
                                                         nextIndex:&_playbackTransferPoolCursor];
    if (transfer == nil) {
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, playbackQueueFailures)
                               value:1];
        return NO;
    }
    uint64_t firstFrameNumber = [self nextPlaybackFirstFrameNumberForCount:count];
    [self fillPlaybackBytes:transfer.data.mutableBytes length:transfer.data.length];

    IOUSBHostIsochronousTransaction *transactions = transfer.transactions.mutableBytes;
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
    uint64_t playbackLayoutSignature = 0;
    uint64_t requestBytesTotal = 0;
    for (NSUInteger index = 0; index < count; index++) {
        requestBytesTotal += requests[index];
        playbackLayoutSignature = LayoutSignatureStep(playbackLayoutSignature,
                                                      requests[index],
                                                      index,
                                                      requestBytesTotal);
        CadenceRecordRange(&_cadenceDiagnostics.playbackQueueRequestCount, requests[index]);
    }
    CadenceRecordRange(&_cadenceDiagnostics.playbackQueueBytes, (uint64_t)transfer.data.length);
    CadenceRecordRange(&_cadenceDiagnostics.playbackQueueTransactions, (uint64_t)count);
    if (playbackLayoutSignature != 0) {
        atomic_fetch_add(&_cadenceDiagnostics.playbackQueueLayoutSignatureSum, playbackLayoutSignature);
    }
#endif
#if OPENA8DJ_REUSE_ISOC_COMPLETION_HANDLERS
    OpenA8DJIsoCompletionHandler completionHandler = transfer.playbackCompletionHandler;
#else
    OpenA8DJIsoCompletionHandler completionHandler = nil;
#endif
    __weak OpenA8DJUSBEngine *weakSelf = self;
    if (completionHandler == nil) {
        completionHandler = ^(IOReturn status, IOUSBHostIsochronousTransaction *transactionList) {
            OpenA8DJUSBEngine *strongSelf = weakSelf;
            if (strongSelf != nil) {
                [strongSelf handlePlaybackTransfer:transfer
                                            status:status
                                      transactions:transactionList];
            }
        };
    }
    NSError *error = nil;
    uint32_t inFlightAfterQueue = atomic_fetch_add(&_playbackTransfersInFlight, 1) + 1;
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
    CadenceRecordRange(&_cadenceDiagnostics.playbackInFlightAtQueue, inFlightAfterQueue);
#else
    (void)inFlightAfterQueue;
#endif
    BOOL queued = [_playbackPipe enqueueIORequestWithData:transfer.data
                                          transactionList:transactions
                                     transactionListCount:transfer.transactionCount
                                         firstFrameNumber:firstFrameNumber
                                                  options:IOUSBHostIsochronousTransferOptionsNone
                                                    error:&error
                                        completionHandler:completionHandler];
    if (!queued) {
        atomic_fetch_sub(&_playbackTransfersInFlight, 1);
        [self releasePooledTransfer:transfer];
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, playbackQueueFailures)
                               value:1];
        [self recordPlaybackScheduleStatus:error != nil ? (IOReturn)error.code : kIOReturnError];
#if OPENA8DJ_ENABLE_TRACE
        _debugPlaybackQueueFailures++;
#endif
        USBTrace("isoc OUT queue failed: %s", NSErrorText(error));
        (void)weakSelf;
        return NO;
    }
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
    [self appendDiagnosticPackedBytes:transfer.data.bytes length:transfer.data.length];
    uint16_t diagnosticFlags = atomic_load(&_playbackUseExplicitScheduling) ?
        kDiagnosticFlagExplicitSchedule : 0;
    [self appendDiagnosticEventType:kDiagnosticEventPackedTransfer
                        frameNumber:(int64_t)firstFrameNumber
                              count:(uint32_t)count
                              flags:diagnosticFlags
                              value:(uint32_t)transfer.data.length
                              extra:atomic_load(&_playbackTransfersInFlight)];
#endif
    return YES;
}

- (void)handlePlaybackTransfer:(OpenA8DJIsoTransfer *)transfer
                         status:(IOReturn)status
                   transactions:(IOUSBHostIsochronousTransaction *)transactions
{
    uint64_t playbackCompletionTime = mach_absolute_time();
    uint64_t playbackCompletionDelta = 0;
    bool playbackHadPreviousCompletion = _lastPlaybackCompletionHostTime != 0;
    bool playbackInvalidCompletionInterval =
        playbackHadPreviousCompletion && playbackCompletionTime <= _lastPlaybackCompletionHostTime;
    if (playbackHadPreviousCompletion && !playbackInvalidCompletionInterval) {
        playbackCompletionDelta = playbackCompletionTime - _lastPlaybackCompletionHostTime;
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
        CadenceRecordOutlier(&_cadenceDiagnostics.playbackCompletionDeltaOutliers,
                             &_cadenceDiagnostics.playbackCompletionDeltaOutlierMax,
                             &_cadenceDiagnostics.playbackCompletionDeltaOutlierSum,
                             playbackCompletionDelta);
#endif
    }
    _lastPlaybackCompletionHostTime = playbackCompletionTime;

#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
    uint32_t inFlightAtCompletion = atomic_load(&_playbackTransfersInFlight);
    CadenceRecordRange(&_cadenceDiagnostics.playbackInFlightAtCompletion, inFlightAtCompletion);
#endif
    if (atomic_load(&_playbackTransfersInFlight) > 0) {
        atomic_fetch_sub(&_playbackTransfersInFlight, 1);
    }
    uint64_t playbackTransactions = 0;
    uint64_t playbackBytes = 0;
    uint64_t failedTransactions = 0;
    uint64_t shortTransfers = 0;
    uint64_t completionStatusFailures =
        status != kIOReturnSuccess &&
        !(status == kIOReturnAborted && !atomic_load(&_streaming)) ? 1 : 0;
    uint64_t transactionStatusFailures = 0;
    uint64_t zeroLengthTransactions = 0;
    uint64_t shortTransactions = 0;
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
    uint64_t playbackLayoutSignature = 0;
#endif
    [self recordPlaybackScheduleStatus:status];
    if (status == kIOReturnSuccess && transactions != NULL) {
        for (NSUInteger index = 0; index < transfer.transactionCount; index++) {
            IOUSBHostIsochronousTransaction *transaction = &transactions[index];
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
            CadenceRecordRange(&_cadenceDiagnostics.playbackRequestCount, transaction->requestCount);
            CadenceRecordRange(&_cadenceDiagnostics.playbackCompleteCount, transaction->completeCount);
            playbackLayoutSignature = LayoutSignatureStep(playbackLayoutSignature,
                                                          transaction->requestCount,
                                                          transaction->completeCount,
                                                          index);
            if (transaction->completeCount == 0) {
                atomic_fetch_add(&_cadenceDiagnostics.playbackZeroCompleteTransactions, 1);
            }
#endif
            [self recordPlaybackScheduleStatus:transaction->status];
            if (transaction->status != kIOReturnSuccess) {
                failedTransactions++;
                transactionStatusFailures++;
                continue;
            }
            if (transaction->requestCount > 0 &&
                transaction->completeCount < transaction->requestCount) {
                shortTransactions++;
                if (transaction->completeCount == 0) {
                    zeroLengthTransactions++;
                }
            }
            playbackTransactions++;
            playbackBytes += transaction->completeCount;
            if (transaction->completeCount != transaction->requestCount) {
                shortTransfers++;
            }
        }
#if OPENA8DJ_ENABLE_CADENCE_DIAGNOSTIC
        if (playbackLayoutSignature != 0) {
            atomic_fetch_add(&_cadenceDiagnostics.playbackLayoutSignatureSum, playbackLayoutSignature);
        }
#endif
    } else if (status != kIOReturnAborted) {
        failedTransactions++;
    }
    if (status == kIOReturnSuccess && failedTransactions == 0 && shortTransfers == 0) {
        atomic_store(&_playbackScheduleFailureStreak, 0);
    }
#if OPENA8DJ_ENABLE_TRACE
    if (status == kIOReturnSuccess && transactions != NULL) {
        _debugPlaybackTransfers++;
        for (NSUInteger index = 0; index < transfer.transactionCount; index++) {
            IOUSBHostIsochronousTransaction *transaction = &transactions[index];
            if (transaction->status != kIOReturnSuccess) {
                _debugPlaybackTransactionFailures++;
                continue;
            }
            _debugPlaybackBytes += transaction->completeCount;
            if (transaction->completeCount != transaction->requestCount) {
                _debugPlaybackShortTransfers++;
            }
        }
    }
#else
    (void)transactions;
#endif
    if (status != kIOReturnSuccess && status != kIOReturnAborted && atomic_load(&_streaming)) {
        USBTrace("isoc OUT complete status=0x%08x", status);
    }
#if OPENA8DJ_ENABLE_HOT_STREAM_STATS
    if (playbackHadPreviousCompletion) {
        CompletionQualityRecordJitter(
            &_pendingPlaybackCompletionQuality,
            playbackInvalidCompletionInterval,
            playbackCompletionDelta,
            transfer.transactionCount,
            _ticksPerUSBMicroframe,
            _completionJitterBinThresholdTicks);
    }
    _pendingPlaybackCompletionQuality.completionStatusFailures += completionStatusFailures;
    _pendingPlaybackCompletionQuality.transactionStatusFailures += transactionStatusFailures;
    _pendingPlaybackCompletionQuality.zeroLengthTransactions += zeroLengthTransactions;
    _pendingPlaybackCompletionQuality.shortTransactions += shortTransactions;

    uint64_t playbackStatsInterval = OPENA8DJ_HOT_STREAM_STATS_INTERVAL < 1 ? 1 : OPENA8DJ_HOT_STREAM_STATS_INTERVAL;
    uint64_t playbackStatsCounter = atomic_fetch_add(&_playbackHotStreamStatsCounter, 1) + 1;
    if (playbackStatsInterval == 1 || (playbackStatsCounter % playbackStatsInterval) == 0) {
        pthread_mutex_lock(&_streamStatsMutex);
        StreamStatsAddTimingLocked(&_streamStats,
                                   offsetof(OpenA8DJStreamStatsPayload, playbackCompletionDeltaMin),
                                   offsetof(OpenA8DJStreamStatsPayload, playbackCompletionDeltaMax),
                                   offsetof(OpenA8DJStreamStatsPayload, playbackCompletionDeltaSum),
                                   offsetof(OpenA8DJStreamStatsPayload, playbackCompletionDeltaSamples),
                                   playbackCompletionDelta);
        StreamStatsFlushCompletionQualityLocked(
            &_streamStats, &_pendingPlaybackCompletionQuality, false);
        _streamStats.playbackTransfers += 1;
        _streamStats.playbackTransactions += playbackTransactions;
        _streamStats.playbackBytes += playbackBytes;
        _streamStats.playbackTransactionFailures += failedTransactions;
        _streamStats.playbackShortTransfers += shortTransfers;
        pthread_mutex_unlock(&_streamStatsMutex);
    }
#else
    (void)playbackCompletionDelta;
    (void)playbackTransactions;
    (void)playbackBytes;
    (void)failedTransactions;
    (void)shortTransfers;
    (void)completionStatusFailures;
    (void)transactionStatusFailures;
    (void)zeroLengthTransactions;
    (void)shortTransactions;
#endif
    [self releasePooledTransfer:transfer];
#if !OPENA8DJ_PLAYBACK_CAPTURE_PACED
    if (atomic_load(&_streaming)) {
        [self fillPlaybackQueue];
    }
#endif
}

- (uint32_t)readInput:(float *)outInterleaved frames:(uint32_t)frames channels:(uint32_t)channels
{
    BOOL decodeEnabled = atomic_load(&_inputDecodeEnabled);
    atomic_store(&_inputDecodeActive, decodeEnabled);
    if (channels != kChannels) {
        memset(outInterleaved, 0, (size_t)frames * channels * sizeof(float));
        return 0;
    }
    if (!decodeEnabled) {
        memset(outInterleaved, 0, (size_t)frames * channels * sizeof(float));
        return 0;
    }
    if (_streamDriverModePolicy.inputLeadGuardEnabled) {
        uint32_t available = RingAvailable(&_inputRing);
        if (available > _streamDriverModePolicy.inputLeadCeilingFrames) {
            pthread_mutex_lock(&gDriverModeMutex);
            gTimecodeState.inputLeadFrames = available;
            TimecodeFailOpenLocked(
                kOpenA8DJTimecodeFailInputLeadViolation, true);
            pthread_mutex_unlock(&gDriverModeMutex);
        }
    }
    uint64_t lastComplete =
        atomic_load(&_timecodeLastCompleteHostTime);
    uint64_t missingThreshold =
        MachTicksForNanoseconds(500000000ull);
    if (lastComplete != 0 && missingThreshold != 0 &&
        mach_absolute_time() - lastComplete > missingThreshold) {
        pthread_mutex_lock(&gDriverModeMutex);
        TimecodeFailOpenLocked(
            kOpenA8DJTimecodeFailStatsMissing, true);
        pthread_mutex_unlock(&gDriverModeMutex);
        atomic_store(&_timecodeLastCompleteHostTime, 0);
    }
    uint32_t read = RingRead(
        &_inputRing, outInterleaved, frames, true);
    if (read < frames) {
        pthread_mutex_lock(&gDriverModeMutex);
        TimecodeFailOpenLocked(
            kOpenA8DJTimecodeFailXRunOrTransportError, true);
        pthread_mutex_unlock(&gDriverModeMutex);
    }
    return read;
}

- (void)setInputDecodeEnabled:(BOOL)enabled
{
    atomic_store(&gInputDecodeEnabledPreference, enabled);
    atomic_store(&_inputDecodeEnabled, enabled);
    if (!enabled) {
        atomic_store(&_inputDecodeActive, false);
        RingClear(&_inputRing);
    }
}

- (void)setInputDecodeActive:(BOOL)active
{
    BOOL decodeEnabled = atomic_load(&_inputDecodeEnabled);
    atomic_store(&_inputDecodeActive, active && decodeEnabled);
}

- (void)writeOutput:(const float *)inInterleaved
              frames:(uint32_t)frames
            channels:(uint32_t)channels
          sampleTime:(double)sampleTime
     sampleTimeValid:(BOOL)sampleTimeValid
{
    if (channels != kChannels || inInterleaved == NULL) {
        return;
    }
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
    [self appendDiagnosticFrames:&_diagnosticWrittenBuffer counter:&_diagnosticWrittenFrames frames:inInterleaved count:frames];
#endif
#if OPENA8DJ_ENABLE_TRACE
    for (uint32_t frame = 0; frame < frames; frame++) {
        for (uint32_t channel = 0; channel < channels; channel++) {
            float value = fabsf(inInterleaved[(size_t)frame * channels + channel]);
            if (value > _debugOutputPeak) {
                _debugOutputPeak = value;
            }
        }
    }
#endif
    int64_t startFrame = 0;
    if (sampleTimeValid && isfinite(sampleTime)) {
        startFrame = (int64_t)llround(sampleTime);
#if OPENA8DJ_ENABLE_OUTPUT_SAMPLE_TIME_FOLLOWER
        pthread_mutex_lock(&_outputTimeline.mutex);
        if (_outputTimeline.hasWritten) {
            int64_t continuousFrame = _outputTimeline.maxWrittenFrame + 1;
            int64_t delta = startFrame - continuousFrame;
            if (delta < 0) {
                delta = -delta;
            }
            if (delta <= (int64_t)kOutputSampleTimeJitterToleranceFrames) {
                startFrame = continuousFrame;
            }
        }
        pthread_mutex_unlock(&_outputTimeline.mutex);
#endif
    } else {
        pthread_mutex_lock(&_outputTimeline.mutex);
        if (_outputTimeline.hasWritten) {
            startFrame = _outputTimeline.maxWrittenFrame + 1;
        } else {
            startFrame = (int64_t)_streamDriverModePolicy.outputStartLatencyFrames;
        }
        pthread_mutex_unlock(&_outputTimeline.mutex);
    }

    uint32_t timelineResets = 0;
    uint32_t lateWriteFrames = 0;
    uint32_t dropped = OutputTimelineWrite(&_outputTimeline,
                                           inInterleaved,
                                           frames,
                                           startFrame,
                                           _streamDriverModePolicy.outputStartLatencyFrames,
                                           _streamDriverModePolicy.outputRestartLatencyFrames,
                                           _streamDriverModePolicy.outputTargetLatencyFrames,
                                           &timelineResets,
                                           &lateWriteFrames);
    if (timelineResets > 0) {
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, outputTimelineResets)
                               value:timelineResets];
    }
    if (dropped > 0) {
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, outputElasticDrops)
                               value:dropped];
    }
    if (lateWriteFrames > 0) {
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, outputLateWriteFrames)
                               value:lateWriteFrames];
        [self addStreamStatAtOffset:offsetof(OpenA8DJStreamStatsPayload, outputLateWriteBatches)
                               value:1];
    }
#if OPENA8DJ_ENABLE_DIAGNOSTIC_CAPTURE
    uint16_t diagnosticFlags = 0;
    if (sampleTimeValid) diagnosticFlags |= kDiagnosticFlagSampleTimeValid;
    if (timelineResets > 0) diagnosticFlags |= kDiagnosticFlagTimelineReset;
    if (dropped > 0) diagnosticFlags |= kDiagnosticFlagDropped;
    [self appendDiagnosticEventType:kDiagnosticEventWrite
                        frameNumber:startFrame
                              count:frames
                              flags:diagnosticFlags
                              value:dropped
                              extra:timelineResets];
#endif
#if OPENA8DJ_ENABLE_OUTPUT_WRITE_STATS
    atomic_fetch_add(&_outputFramesWrittenAtomic, frames);
#else
    (void)frames;
#endif
#if OPENA8DJ_ENABLE_TRACE
    _debugOutputFramesWritten += frames;
#endif
#if !OPENA8DJ_PLAYBACK_CAPTURE_PACED
    if (atomic_load(&_streaming) && _queue != nil) {
        __weak OpenA8DJUSBEngine *weakSelf = self;
        dispatch_async(_queue, ^{
            OpenA8DJUSBEngine *strongSelf = weakSelf;
            if (strongSelf != nil) {
                [strongSelf fillPlaybackQueue];
            }
        });
    }
#endif
}

- (void)writeOutput:(const float *)inInterleaved frames:(uint32_t)frames channels:(uint32_t)channels
{
    [self writeOutput:inInterleaved
               frames:frames
             channels:channels
           sampleTime:0.0
      sampleTimeValid:NO];
}

@end

static pthread_mutex_t gEngineMutex = PTHREAD_MUTEX_INITIALIZER;
static OpenA8DJUSBEngine *gEngine = nil;

bool OpenA8DJUSBStart(double sampleRate)
{
    pthread_mutex_lock(&gEngineMutex);
    if (gEngine == nil) {
        gEngine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:sampleRate];
    } else if (fabs(gEngine.sampleRate - sampleRate) > 0.5) {
        [gEngine close];
        gEngine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:sampleRate];
    }
    gEngine.sampleRate = sampleRate;
    BOOL ok = [gEngine start];
    pthread_mutex_unlock(&gEngineMutex);
    return ok;
}

bool OpenA8DJUSBSetSampleRate(double sampleRate)
{
    pthread_mutex_lock(&gDriverModeMutex);
    TimecodeFailOpenLocked(
        kOpenA8DJTimecodeFailConfigurationChanged, true);
    pthread_mutex_unlock(&gDriverModeMutex);
    pthread_mutex_lock(&gEngineMutex);
    if (gEngine == nil) {
        gEngine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:sampleRate];
        pthread_mutex_unlock(&gEngineMutex);
        return true;
    }

    if (fabs(gEngine.sampleRate - sampleRate) <= 0.5) {
        pthread_mutex_unlock(&gEngineMutex);
        return true;
    }

    BOOL wasStreaming = [gEngine isStreaming];
    [gEngine close];
    gEngine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:sampleRate];
    BOOL ok = !wasStreaming || [gEngine start];
    pthread_mutex_unlock(&gEngineMutex);
    return ok;
}

void OpenA8DJUSBSetCoreAudioBufferFrames(uint32_t bufferFrames)
{
    uint32_t previous = atomic_exchange(
        &gCoreAudioBufferFrames, bufferFrames);
    if (previous != bufferFrames) {
        pthread_mutex_lock(&gDriverModeMutex);
        TimecodeFailOpenLocked(
            kOpenA8DJTimecodeFailConfigurationChanged, true);
        pthread_mutex_unlock(&gDriverModeMutex);
    }
}

bool OpenA8DJUSBEnsureOpen(double sampleRate)
{
    pthread_mutex_lock(&gEngineMutex);
    if (gEngine == nil) {
        gEngine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:sampleRate];
    }
    gEngine.sampleRate = sampleRate;
    BOOL ok = [gEngine ensureOpen];
    pthread_mutex_unlock(&gEngineMutex);
    return ok;
}

bool OpenA8DJUSBDevicePresent(void)
{
    io_service_t service = FindDeviceService();
    if (service == IO_OBJECT_NULL) {
        return false;
    }
    IOObjectRelease(service);
    return true;
}

void OpenA8DJUSBSetInputDecodeActive(bool active)
{
    pthread_mutex_lock(&gEngineMutex);
    OpenA8DJUSBEngine *engine = gEngine;
    pthread_mutex_unlock(&gEngineMutex);
    [engine setInputDecodeActive:active];
}

void OpenA8DJUSBStop(void)
{
    pthread_mutex_lock(&gEngineMutex);
    [gEngine stop];
    pthread_mutex_unlock(&gEngineMutex);
}

void OpenA8DJUSBClose(void)
{
    pthread_mutex_lock(&gEngineMutex);
    [gEngine close];
    gEngine = nil;
    pthread_mutex_unlock(&gEngineMutex);
}

bool OpenA8DJUSBGetClockAnchor(OpenA8DJUSBClockAnchor *outAnchor)
{
    pthread_mutex_lock(&gEngineMutex);
    OpenA8DJUSBEngine *engine = gEngine;
    pthread_mutex_unlock(&gEngineMutex);
    if (engine == nil) {
        if (outAnchor != NULL) {
            memset(outAnchor, 0, sizeof(*outAnchor));
        }
        return false;
    }
    return [engine getClockAnchor:outAnchor];
}

uint32_t OpenA8DJUSBReadInput(float *outInterleaved, uint32_t frames, uint32_t channels)
{
    pthread_mutex_lock(&gEngineMutex);
    OpenA8DJUSBEngine *engine = gEngine;
    pthread_mutex_unlock(&gEngineMutex);
    if (engine == nil) {
        memset(outInterleaved, 0, (size_t)frames * channels * sizeof(float));
        return 0;
    }
    return [engine readInput:outInterleaved frames:frames channels:channels];
}

void OpenA8DJUSBWriteOutput(const float *inInterleaved, uint32_t frames, uint32_t channels)
{
    OpenA8DJUSBWriteOutputAtSampleTime(inInterleaved, frames, channels, 0.0, false);
}

void OpenA8DJUSBWriteOutputAtSampleTime(const float *inInterleaved,
                                        uint32_t frames,
                                        uint32_t channels,
                                        double sampleTime,
                                        bool sampleTimeValid)
{
    pthread_mutex_lock(&gEngineMutex);
    OpenA8DJUSBEngine *engine = gEngine;
    pthread_mutex_unlock(&gEngineMutex);
    if (engine != nil) {
        [engine writeOutput:inInterleaved
                     frames:frames
                   channels:channels
                 sampleTime:sampleTime
            sampleTimeValid:sampleTimeValid ? YES : NO];
    }
}

__attribute__((destructor))
static void OpenA8DJUSBDestructor(void)
{
    OpenA8DJUSBClose();
}
