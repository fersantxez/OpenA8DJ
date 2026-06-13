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

bool OpenA8DJUSBStart(double sampleRate);
bool OpenA8DJUSBEnsureOpen(double sampleRate);
bool OpenA8DJUSBDevicePresent(void);
bool OpenA8DJUSBSetSampleRate(double sampleRate);
void OpenA8DJUSBStop(void);
void OpenA8DJUSBClose(void);
bool OpenA8DJUSBGetClockAnchor(OpenA8DJUSBClockAnchor *outAnchor);
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
