#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenA8DJUSBClockAnchor {
    bool valid;
    double sampleRate;
    double sampleTime;
    uint64_t hostTime;
    uint64_t usbTime;
    uint64_t usbFrameNumber;
    uint64_t usbFrameHostTime;
    double hostTicksPerUSBFrame;
    uint64_t usbFrameSamples;
    uint64_t usbFrameResyncs;
    uint64_t seed;
    uint64_t framesObserved;
    uint64_t acceptedAnchors;
    uint64_t rejectedAnchors;
} OpenA8DJUSBClockAnchor;

typedef struct OpenA8DJUSBDiagnostics {
    uint32_t size;
    uint8_t running;
    uint8_t streaming;
    uint8_t playbackExplicitScheduling;
    uint8_t specDataAlignment;
    uint8_t specNumAnalogAudioIn;
    uint8_t specNumAnalogAudioOut;
    uint16_t specFwVersion;
    double sampleRate;
    uint8_t control[6];
    uint8_t lastAudioParamsResetRateCode;
    uint8_t lastAudioParamsResetDepth;
    uint8_t lastAudioParamsResetOk;
    uint8_t lastAudioParamsStreamRateCode;
    uint8_t lastAudioParamsStreamDepth;
    uint8_t lastAudioParamsStreamOk;
    uint16_t lastAudioParamsResetBytesPerPacket;
    uint16_t lastAudioParamsStreamBytesPerPacket;
    uint32_t outputByteInFrame;
    uint32_t outputRingFrames;
    uint32_t outputTargetLatencyFrames;
    uint32_t playbackLeadFrames;
    uint32_t playbackQueueTarget;
    uint32_t playbackTransfersInFlight;
    uint64_t captureTransfers;
    uint64_t playbackTransfers;
    uint64_t outputFramesWritten;
    uint64_t outputFramesRead;
    uint64_t outputStartupSilenceFrames;
    uint64_t outputUnderruns;
    uint64_t outputActiveUnderruns;
    uint64_t outputElasticDrops;
    uint64_t outputElasticReplays;
    uint64_t outputTimelineResets;
    uint64_t outputLateWriteFrames;
    uint64_t outputLateWriteBatches;
    uint64_t playbackNextFrameNumber;
    uint64_t playbackScheduleResets;
    uint64_t playbackScheduleTooOld;
    uint64_t playbackScheduleTooNew;
    uint64_t playbackScheduleOutOfWindow;
    uint64_t playbackScheduleFallbacks;
    uint64_t playbackQueueFailures;
    uint64_t playbackQueueFailureLastStatus;
    uint64_t playbackQueueFailureNoError;
    uint64_t playbackQueueFailureTooOld;
    uint64_t playbackQueueFailureTooNew;
    uint64_t playbackQueueFailureOther;
    uint64_t playbackQueueFailureExplicit;
    uint64_t playbackQueueFailureConsumedFrames;
    uint64_t playbackQueueFailureStartupSilenceFrames;
    uint64_t playbackQueueBytesMin;
    uint64_t playbackQueueBytesMax;
    uint64_t playbackQueueBytesSum;
    uint64_t playbackQueueBytesSamples;
    uint64_t playbackQueueTransactionsMin;
    uint64_t playbackQueueTransactionsMax;
    uint64_t playbackQueueTransactionsSum;
    uint64_t playbackQueueTransactionsSamples;
    uint64_t playbackRequestCountMin;
    uint64_t playbackRequestCountMax;
    uint64_t playbackRequestCountSum;
    uint64_t playbackRequestCountSamples;
    uint64_t playbackCompleteCountMin;
    uint64_t playbackCompleteCountMax;
    uint64_t playbackCompleteCountSum;
    uint64_t playbackCompleteCountSamples;
    uint64_t cadenceExpectedTransferTicks;
    uint8_t selectAlt0BeforeAlt1;
} OpenA8DJUSBDiagnostics;

bool OpenA8DJUSBStart(double sampleRate);
bool OpenA8DJUSBEnsureOpen(double sampleRate);
bool OpenA8DJUSBDevicePresent(void);
bool OpenA8DJUSBSetSampleRate(double sampleRate);
void OpenA8DJUSBStop(void);
void OpenA8DJUSBClose(void);
bool OpenA8DJUSBGetClockAnchor(OpenA8DJUSBClockAnchor *outAnchor);
bool OpenA8DJUSBGetDiagnostics(OpenA8DJUSBDiagnostics *outDiagnostics);
void OpenA8DJUSBSetInputDecodeActive(bool active);
bool OpenA8DJUSBApplyPlaybackProfile(void);
uint32_t OpenA8DJUSBReadInput(float *outInterleaved, uint32_t frames, uint32_t channels);
void OpenA8DJUSBWriteOutput(const float *inInterleaved, uint32_t frames, uint32_t channels);
void OpenA8DJUSBWriteOutputAtSampleTime(const float *inInterleaved,
                                        uint32_t frames,
                                        uint32_t channels,
                                        double sampleTime,
                                        bool sampleTimeValid);

#ifdef __cplusplus
}
#endif
