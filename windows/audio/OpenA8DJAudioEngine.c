#include "OpenA8DJAudioEngine.h"

#include <string.h>

static size_t
OpenA8DJ_MinSize(size_t a, size_t b)
{
    return a < b ? a : b;
}

static float
OpenA8DJ_ClampFloat(float sample)
{
    if (sample > 1.0f) {
        return 1.0f;
    }
    if (sample < -1.0f) {
        return -1.0f;
    }
    return sample;
}

bool
OpenA8DJEngineIsStableSampleRate(uint32_t sampleRate)
{
    return sampleRate == OPENA8DJ_ENGINE_STABLE_RATE_44100 ||
           sampleRate == OPENA8DJ_ENGINE_STABLE_RATE_48000;
}

OPENA8DJ_ENGINE_CONFIG
OpenA8DJEngineDefaultConfig(void)
{
    OPENA8DJ_ENGINE_CONFIG config;
    config.sampleRate = OPENA8DJ_ENGINE_DEFAULT_RATE;
    config.bufferFrames = OPENA8DJ_ENGINE_DEFAULT_BUFFER_FRAMES;
    config.inputChannels = OPENA8DJ_ENGINE_INPUT_CHANNELS;
    config.outputChannels = OPENA8DJ_ENGINE_OUTPUT_CHANNELS;
    return config;
}

OPENA8DJ_ENGINE_STATUS
OpenA8DJEngineValidateConfig(const OPENA8DJ_ENGINE_CONFIG *config)
{
    if (config == NULL) {
        return OPENA8DJ_ENGINE_INVALID_ARGUMENT;
    }
    if (!OpenA8DJEngineIsStableSampleRate(config->sampleRate)) {
        return OPENA8DJ_ENGINE_UNSUPPORTED_FORMAT;
    }
    if (config->bufferFrames < OPENA8DJ_ENGINE_MIN_BUFFER_FRAMES ||
        config->bufferFrames > OPENA8DJ_ENGINE_MAX_BUFFER_FRAMES) {
        return OPENA8DJ_ENGINE_UNSUPPORTED_FORMAT;
    }
    if (config->inputChannels != OPENA8DJ_ENGINE_INPUT_CHANNELS ||
        config->outputChannels != OPENA8DJ_ENGINE_OUTPUT_CHANNELS) {
        return OPENA8DJ_ENGINE_UNSUPPORTED_FORMAT;
    }
    return OPENA8DJ_ENGINE_OK;
}

OPENA8DJ_ENGINE_STATUS
OpenA8DJEngineInit(
    OPENA8DJ_AUDIO_ENGINE *engine,
    const OPENA8DJ_ENGINE_CONFIG *config,
    float *renderStorage,
    size_t renderStorageFrames,
    float *captureStorage,
    size_t captureStorageFrames)
{
    OPENA8DJ_ENGINE_STATUS status;

    if (engine == NULL || renderStorage == NULL || captureStorage == NULL) {
        return OPENA8DJ_ENGINE_INVALID_ARGUMENT;
    }

    status = OpenA8DJEngineValidateConfig(config);
    if (status != OPENA8DJ_ENGINE_OK) {
        return status;
    }

    if (renderStorageFrames < config->bufferFrames ||
        captureStorageFrames < config->bufferFrames) {
        return OPENA8DJ_ENGINE_INSUFFICIENT_STORAGE;
    }

    memset(engine, 0, sizeof(*engine));
    engine->config = *config;
    engine->renderBuffer = renderStorage;
    engine->renderCapacityFrames = renderStorageFrames;
    engine->captureBuffer = captureStorage;
    engine->captureCapacityFrames = captureStorageFrames;
    memset(renderStorage,
           0,
           renderStorageFrames * config->outputChannels * sizeof(renderStorage[0]));
    memset(captureStorage,
           0,
           captureStorageFrames * config->inputChannels * sizeof(captureStorage[0]));
    return OPENA8DJ_ENGINE_OK;
}

void
OpenA8DJEngineReset(OPENA8DJ_AUDIO_ENGINE *engine)
{
    if (engine == NULL) {
        return;
    }
    engine->counters.renderFramesWritten = 0;
    engine->counters.renderFramesRead = 0;
    engine->counters.renderUnderruns = 0;
    engine->counters.captureFramesWritten = 0;
    engine->counters.captureFramesRead = 0;
    engine->counters.captureOverruns = 0;
    engine->renderReadFrame = 0;
    engine->renderWriteFrame = 0;
    engine->renderAvailableFrames = 0;
    engine->captureReadFrame = 0;
    engine->captureWriteFrame = 0;
    engine->captureAvailableFrames = 0;
}

size_t
OpenA8DJEngineWriteRender(
    OPENA8DJ_AUDIO_ENGINE *engine,
    const float *interleaved,
    size_t frames)
{
    size_t written = 0;
    size_t channels;

    if (engine == NULL || interleaved == NULL) {
        return 0;
    }

    channels = engine->config.outputChannels;
    while (written < frames &&
           engine->renderAvailableFrames < engine->renderCapacityFrames) {
        size_t dst = engine->renderWriteFrame * channels;
        size_t src = written * channels;
        memcpy(&engine->renderBuffer[dst], &interleaved[src], channels * sizeof(float));
        engine->renderWriteFrame = (engine->renderWriteFrame + 1u) % engine->renderCapacityFrames;
        engine->renderAvailableFrames++;
        written++;
    }

    engine->counters.renderFramesWritten += written;
    return written;
}

size_t
OpenA8DJEngineReadRender(
    OPENA8DJ_AUDIO_ENGINE *engine,
    float *interleaved,
    size_t frames)
{
    size_t read = 0;
    size_t channels;

    if (engine == NULL || interleaved == NULL) {
        return 0;
    }

    channels = engine->config.outputChannels;
    while (read < frames && engine->renderAvailableFrames > 0) {
        size_t src = engine->renderReadFrame * channels;
        size_t dst = read * channels;
        memcpy(&interleaved[dst], &engine->renderBuffer[src], channels * sizeof(float));
        engine->renderReadFrame = (engine->renderReadFrame + 1u) % engine->renderCapacityFrames;
        engine->renderAvailableFrames--;
        read++;
    }

    if (read < frames) {
        size_t missing = frames - read;
        memset(&interleaved[read * channels], 0, missing * channels * sizeof(float));
        engine->counters.renderUnderruns++;
    }

    engine->counters.renderFramesRead += frames;
    return frames;
}

size_t
OpenA8DJEngineWriteCapture(
    OPENA8DJ_AUDIO_ENGINE *engine,
    const float *interleaved,
    size_t frames)
{
    size_t written = 0;
    size_t channels;

    if (engine == NULL || interleaved == NULL) {
        return 0;
    }

    channels = engine->config.inputChannels;
    while (written < frames) {
        size_t dst = engine->captureWriteFrame * channels;
        size_t src = written * channels;
        if (engine->captureAvailableFrames == engine->captureCapacityFrames) {
            engine->captureReadFrame = (engine->captureReadFrame + 1u) % engine->captureCapacityFrames;
            engine->captureAvailableFrames--;
            engine->counters.captureOverruns++;
        }
        memcpy(&engine->captureBuffer[dst], &interleaved[src], channels * sizeof(float));
        engine->captureWriteFrame = (engine->captureWriteFrame + 1u) % engine->captureCapacityFrames;
        engine->captureAvailableFrames++;
        written++;
    }

    engine->counters.captureFramesWritten += written;
    return written;
}

size_t
OpenA8DJEngineReadCapture(
    OPENA8DJ_AUDIO_ENGINE *engine,
    float *interleaved,
    size_t frames)
{
    size_t read;
    size_t index;
    size_t channels;

    if (engine == NULL || interleaved == NULL) {
        return 0;
    }

    channels = engine->config.inputChannels;
    read = OpenA8DJ_MinSize(frames, engine->captureAvailableFrames);
    for (index = 0; index < read; index++) {
        size_t src = engine->captureReadFrame * channels;
        size_t dst = index * channels;
        memcpy(&interleaved[dst], &engine->captureBuffer[src], channels * sizeof(float));
        engine->captureReadFrame = (engine->captureReadFrame + 1u) % engine->captureCapacityFrames;
        engine->captureAvailableFrames--;
    }

    if (read < frames) {
        memset(&interleaved[read * channels], 0, (frames - read) * channels * sizeof(float));
    }

    engine->counters.captureFramesRead += read;
    return read;
}

void
OpenA8DJEnginePackS24BE(float sample, uint8_t out[3])
{
    int32_t packed;

    if (out == NULL) {
        return;
    }

    sample = OpenA8DJ_ClampFloat(sample);
    if (sample >= 1.0f) {
        packed = 0x7fffff;
    } else if (sample <= -1.0f) {
        packed = -0x800000;
    } else {
        packed = (int32_t)(sample * 8388607.0f);
    }

    out[0] = (uint8_t)((uint32_t)packed >> 16);
    out[1] = (uint8_t)((uint32_t)packed >> 8);
    out[2] = (uint8_t)((uint32_t)packed);
}

float
OpenA8DJEngineUnpackS24BE(const uint8_t in[3])
{
    int32_t sample;

    if (in == NULL) {
        return 0.0f;
    }

    sample = ((int32_t)in[0] << 16) | ((int32_t)in[1] << 8) | (int32_t)in[2];
    if ((sample & 0x00800000) != 0) {
        sample |= (int32_t)0xff000000;
    }
    return (float)sample / 8388608.0f;
}
