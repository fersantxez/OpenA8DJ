#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENA8DJ_ENGINE_INPUT_CHANNELS 8u
#define OPENA8DJ_ENGINE_OUTPUT_CHANNELS 8u
#define OPENA8DJ_ENGINE_STEREO_PAIRS 4u
#define OPENA8DJ_ENGINE_STREAMS 4u
#define OPENA8DJ_ENGINE_CHANNELS_PER_STREAM 2u
#define OPENA8DJ_ENGINE_BYTES_PER_SAMPLE 3u
#define OPENA8DJ_ENGINE_BYTES_PER_SAMPLE_USB 4u
#define OPENA8DJ_ENGINE_MODE2_GROUP_BYTES \
    (OPENA8DJ_ENGINE_STREAMS * OPENA8DJ_ENGINE_BYTES_PER_SAMPLE_USB)
#define OPENA8DJ_ENGINE_MODE2_CHECK_OFFSET \
    (OPENA8DJ_ENGINE_STREAMS * OPENA8DJ_ENGINE_CHANNELS_PER_STREAM)
#define OPENA8DJ_ENGINE_OUTPUT_START_BYTE (OPENA8DJ_ENGINE_BYTES_PER_SAMPLE + 1u)
#define OPENA8DJ_ENGINE_STABLE_RATE_44100 44100u
#define OPENA8DJ_ENGINE_STABLE_RATE_48000 48000u
#define OPENA8DJ_ENGINE_DEFAULT_RATE OPENA8DJ_ENGINE_STABLE_RATE_48000
#define OPENA8DJ_ENGINE_MIN_BUFFER_FRAMES 15u
#define OPENA8DJ_ENGINE_DEFAULT_BUFFER_FRAMES 512u
#define OPENA8DJ_ENGINE_MAX_BUFFER_FRAMES 4096u

typedef enum OPENA8DJ_ENGINE_STATUS {
    OPENA8DJ_ENGINE_OK = 0,
    OPENA8DJ_ENGINE_INVALID_ARGUMENT = 1,
    OPENA8DJ_ENGINE_UNSUPPORTED_FORMAT = 2,
    OPENA8DJ_ENGINE_INSUFFICIENT_STORAGE = 3
} OPENA8DJ_ENGINE_STATUS;

typedef struct OPENA8DJ_ENGINE_CONFIG {
    uint32_t sampleRate;
    uint32_t bufferFrames;
    uint32_t inputChannels;
    uint32_t outputChannels;
} OPENA8DJ_ENGINE_CONFIG;

typedef struct OPENA8DJ_ENGINE_COUNTERS {
    uint64_t renderFramesWritten;
    uint64_t renderFramesRead;
    uint64_t renderUnderruns;
    uint64_t captureFramesWritten;
    uint64_t captureFramesRead;
    uint64_t captureOverruns;
} OPENA8DJ_ENGINE_COUNTERS;

typedef struct OPENA8DJ_AUDIO_ENGINE {
    OPENA8DJ_ENGINE_CONFIG config;
    OPENA8DJ_ENGINE_COUNTERS counters;
    float *renderBuffer;
    size_t renderCapacityFrames;
    size_t renderReadFrame;
    size_t renderWriteFrame;
    size_t renderAvailableFrames;
    float *captureBuffer;
    size_t captureCapacityFrames;
    size_t captureReadFrame;
    size_t captureWriteFrame;
    size_t captureAvailableFrames;
} OPENA8DJ_AUDIO_ENGINE;

typedef struct OPENA8DJ_MODE2_OUTPUT_PACKER {
    uint8_t outputFrameBytes[OPENA8DJ_ENGINE_STREAMS]
                            [OPENA8DJ_ENGINE_CHANNELS_PER_STREAM * OPENA8DJ_ENGINE_BYTES_PER_SAMPLE];
    size_t nextFrame;
    uint8_t outputByteInFrame;
    bool outputFrameLoaded;
} OPENA8DJ_MODE2_OUTPUT_PACKER;

bool OpenA8DJEngineIsStableSampleRate(uint32_t sampleRate);

OPENA8DJ_ENGINE_CONFIG OpenA8DJEngineDefaultConfig(void);

OPENA8DJ_ENGINE_STATUS OpenA8DJEngineValidateConfig(
    const OPENA8DJ_ENGINE_CONFIG *config);

OPENA8DJ_ENGINE_STATUS OpenA8DJEngineInit(
    OPENA8DJ_AUDIO_ENGINE *engine,
    const OPENA8DJ_ENGINE_CONFIG *config,
    float *renderStorage,
    size_t renderStorageFrames,
    float *captureStorage,
    size_t captureStorageFrames);

void OpenA8DJEngineReset(OPENA8DJ_AUDIO_ENGINE *engine);

size_t OpenA8DJEngineWriteRender(
    OPENA8DJ_AUDIO_ENGINE *engine,
    const float *interleaved,
    size_t frames);

size_t OpenA8DJEngineReadRender(
    OPENA8DJ_AUDIO_ENGINE *engine,
    float *interleaved,
    size_t frames);

size_t OpenA8DJEngineWriteCapture(
    OPENA8DJ_AUDIO_ENGINE *engine,
    const float *interleaved,
    size_t frames);

size_t OpenA8DJEngineReadCapture(
    OPENA8DJ_AUDIO_ENGINE *engine,
    float *interleaved,
    size_t frames);

void OpenA8DJEnginePackS24BE(float sample, uint8_t out[3]);

float OpenA8DJEngineUnpackS24BE(const uint8_t in[3]);

void OpenA8DJEngineMode2PackerInit(OPENA8DJ_MODE2_OUTPUT_PACKER *packer);

size_t OpenA8DJEnginePackMode2Output(
    OPENA8DJ_MODE2_OUTPUT_PACKER *packer,
    const float *interleavedFrames,
    size_t frameCount,
    uint8_t *out,
    size_t outBytes);

#ifdef __cplusplus
}
#endif
