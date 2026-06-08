#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool OpenA8DJUSBStart(double sampleRate);
bool OpenA8DJUSBEnsureOpen(double sampleRate);
bool OpenA8DJUSBDevicePresent(void);
bool OpenA8DJUSBSetSampleRate(double sampleRate);
void OpenA8DJUSBStop(void);
void OpenA8DJUSBClose(void);
uint32_t OpenA8DJUSBReadInput(float *outInterleaved, uint32_t frames, uint32_t channels);
void OpenA8DJUSBWriteOutput(const float *inInterleaved, uint32_t frames, uint32_t channels);

#ifdef __cplusplus
}
#endif
