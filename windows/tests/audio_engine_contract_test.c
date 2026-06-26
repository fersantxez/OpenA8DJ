#include "../audio/OpenA8DJAudioEngine.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define EXPECT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "FAIL:%s:%d:%s\n", __FILE__, __LINE__, #expr); \
            return 1; \
        } \
    } while (0)

static int
AlmostEqual(float a, float b)
{
    return fabsf(a - b) < 0.00001f;
}

static void
FillFrame(float *buffer, size_t frame, size_t channels, float base)
{
    size_t channel;

    for (channel = 0; channel < channels; channel++) {
        buffer[frame * channels + channel] = base + (float)channel * 0.01f;
    }
}

static int
ExpectMode2SampleByte(
    const uint8_t *mode2Out,
    const uint8_t packedSamples[OPENA8DJ_ENGINE_OUTPUT_CHANNELS][3],
    size_t byteIndex,
    size_t sampleByte)
{
    size_t stream;

    for (stream = 0; stream < OPENA8DJ_ENGINE_STREAMS; stream++) {
        size_t leftChannel = stream * OPENA8DJ_ENGINE_CHANNELS_PER_STREAM;
        size_t rightChannel = leftChannel + 1u;
        uint8_t expected;

        if (sampleByte < OPENA8DJ_ENGINE_BYTES_PER_SAMPLE) {
            expected = packedSamples[leftChannel][sampleByte];
        } else {
            expected = packedSamples[rightChannel][sampleByte - OPENA8DJ_ENGINE_BYTES_PER_SAMPLE];
        }
        if (mode2Out[byteIndex + stream] != expected) {
            fprintf(stderr,
                    "FAIL:%s:%d:stream=%zu byte=%zu expected=0x%02x actual=0x%02x\n",
                    __FILE__,
                    __LINE__,
                    stream,
                    sampleByte,
                    expected,
                    mode2Out[byteIndex + stream]);
            return 0;
        }
    }
    return 1;
}

int
main(void)
{
    OPENA8DJ_ENGINE_CONFIG config = OpenA8DJEngineDefaultConfig();
    OPENA8DJ_AUDIO_ENGINE engine;
    float renderStorage[OPENA8DJ_ENGINE_MIN_BUFFER_FRAMES * OPENA8DJ_ENGINE_OUTPUT_CHANNELS];
    float captureStorage[OPENA8DJ_ENGINE_MIN_BUFFER_FRAMES * OPENA8DJ_ENGINE_INPUT_CHANNELS];
    float renderIn[3 * OPENA8DJ_ENGINE_OUTPUT_CHANNELS];
    float renderOut[4 * OPENA8DJ_ENGINE_OUTPUT_CHANNELS];
    float captureIn[5 * OPENA8DJ_ENGINE_INPUT_CHANNELS];
    float captureOut[4 * OPENA8DJ_ENGINE_INPUT_CHANNELS];
    uint8_t packed[3];
    uint8_t packedChannels[2][OPENA8DJ_ENGINE_OUTPUT_CHANNELS][3];
    OPENA8DJ_MODE2_OUTPUT_PACKER mode2;
    uint8_t mode2Out[32];
    uint8_t mode2FrameOut[44];
    float mode2Frames[2][OPENA8DJ_ENGINE_OUTPUT_CHANNELS] = {
        {
            0.125f, -0.125f, 0.250f, -0.250f,
            0.375f, -0.375f, 0.500f, -0.500f
        },
        {
            -0.0625f, 0.0625f, -0.1875f, 0.1875f,
            -0.3125f, 0.3125f, -0.4375f, 0.4375f
        }
    };
    size_t frame;
    size_t channel;

    EXPECT_TRUE(OpenA8DJEngineIsStableSampleRate(44100));
    EXPECT_TRUE(OpenA8DJEngineIsStableSampleRate(48000));
    EXPECT_TRUE(!OpenA8DJEngineIsStableSampleRate(88200));
    EXPECT_TRUE(!OpenA8DJEngineIsStableSampleRate(96000));

    config.bufferFrames = 4;
    EXPECT_TRUE(OpenA8DJEngineValidateConfig(&config) == OPENA8DJ_ENGINE_UNSUPPORTED_FORMAT);
    config.bufferFrames = OPENA8DJ_ENGINE_MIN_BUFFER_FRAMES;
    EXPECT_TRUE(OpenA8DJEngineValidateConfig(&config) == OPENA8DJ_ENGINE_OK);
    config.sampleRate = 96000;
    EXPECT_TRUE(OpenA8DJEngineValidateConfig(&config) == OPENA8DJ_ENGINE_UNSUPPORTED_FORMAT);

    config = OpenA8DJEngineDefaultConfig();
    config.bufferFrames = 4;
    EXPECT_TRUE(OpenA8DJEngineInit(&engine,
                                   &config,
                                   renderStorage,
                                   4,
                                   captureStorage,
                                   4) == OPENA8DJ_ENGINE_UNSUPPORTED_FORMAT);

    config.bufferFrames = OPENA8DJ_ENGINE_MIN_BUFFER_FRAMES;
    EXPECT_TRUE(OpenA8DJEngineInit(&engine,
                                   &config,
                                   renderStorage,
                                   4,
                                   captureStorage,
                                   4) == OPENA8DJ_ENGINE_INSUFFICIENT_STORAGE);

    config.bufferFrames = 4;
    config.sampleRate = OPENA8DJ_ENGINE_STABLE_RATE_48000;
    config.inputChannels = OPENA8DJ_ENGINE_INPUT_CHANNELS;
    config.outputChannels = OPENA8DJ_ENGINE_OUTPUT_CHANNELS;
    config.bufferFrames = OPENA8DJ_ENGINE_MIN_BUFFER_FRAMES;
    EXPECT_TRUE(OpenA8DJEngineInit(&engine,
                                   &config,
                                   renderStorage,
                                   OPENA8DJ_ENGINE_MIN_BUFFER_FRAMES,
                                   captureStorage,
                                   OPENA8DJ_ENGINE_MIN_BUFFER_FRAMES) == OPENA8DJ_ENGINE_OK);

    memset(renderIn, 0, sizeof(renderIn));
    memset(renderOut, 0xff, sizeof(renderOut));
    for (frame = 0; frame < 3; frame++) {
        FillFrame(renderIn, frame, OPENA8DJ_ENGINE_OUTPUT_CHANNELS, (float)frame);
    }
    EXPECT_TRUE(OpenA8DJEngineWriteRender(&engine, renderIn, 3) == 3);
    EXPECT_TRUE(OpenA8DJEngineReadRender(&engine, renderOut, 4) == 4);
    for (frame = 0; frame < 3; frame++) {
        for (channel = 0; channel < OPENA8DJ_ENGINE_OUTPUT_CHANNELS; channel++) {
            EXPECT_TRUE(AlmostEqual(renderOut[frame * OPENA8DJ_ENGINE_OUTPUT_CHANNELS + channel],
                                    renderIn[frame * OPENA8DJ_ENGINE_OUTPUT_CHANNELS + channel]));
        }
    }
    for (channel = 0; channel < OPENA8DJ_ENGINE_OUTPUT_CHANNELS; channel++) {
        EXPECT_TRUE(renderOut[3 * OPENA8DJ_ENGINE_OUTPUT_CHANNELS + channel] == 0.0f);
    }
    EXPECT_TRUE(engine.counters.renderUnderruns == 1);

    OpenA8DJEngineReset(&engine);
    for (frame = 0; frame < 5; frame++) {
        FillFrame(captureIn, frame, OPENA8DJ_ENGINE_INPUT_CHANNELS, (float)(10 + frame));
    }
    EXPECT_TRUE(OpenA8DJEngineWriteCapture(&engine, captureIn, 5) == 5);
    EXPECT_TRUE(engine.counters.captureOverruns == 0);
    EXPECT_TRUE(OpenA8DJEngineReadCapture(&engine, captureOut, 4) == 4);
    for (frame = 0; frame < 4; frame++) {
        for (channel = 0; channel < OPENA8DJ_ENGINE_INPUT_CHANNELS; channel++) {
            EXPECT_TRUE(AlmostEqual(captureOut[frame * OPENA8DJ_ENGINE_INPUT_CHANNELS + channel],
                                    captureIn[frame * OPENA8DJ_ENGINE_INPUT_CHANNELS + channel]));
        }
    }

    OpenA8DJEnginePackS24BE(1.0f, packed);
    EXPECT_TRUE(packed[0] == 0x7f && packed[1] == 0xff && packed[2] == 0xff);
    OpenA8DJEnginePackS24BE(-1.0f, packed);
    EXPECT_TRUE(packed[0] == 0x80 && packed[1] == 0x00 && packed[2] == 0x00);
    OpenA8DJEnginePackS24BE(0.5f, packed);
    EXPECT_TRUE(OpenA8DJEngineUnpackS24BE(packed) > 0.49f);
    EXPECT_TRUE(OpenA8DJEngineUnpackS24BE(packed) < 0.51f);

    memset(mode2Out, 0xff, sizeof(mode2Out));
    OpenA8DJEngineMode2PackerInit(&mode2);
    EXPECT_TRUE(OpenA8DJEnginePackMode2Output(&mode2, NULL, 0, mode2Out, sizeof(mode2Out)) == sizeof(mode2Out));
    for (channel = 0; channel < sizeof(mode2Out); channel++) {
        size_t groupOffset = channel % OPENA8DJ_ENGINE_MODE2_GROUP_BYTES;
        if (groupOffset >= OPENA8DJ_ENGINE_MODE2_CHECK_OFFSET &&
            groupOffset < OPENA8DJ_ENGINE_MODE2_CHECK_OFFSET + OPENA8DJ_ENGINE_STREAMS) {
            size_t group = channel / OPENA8DJ_ENGINE_MODE2_GROUP_BYTES;
            size_t stream = groupOffset - OPENA8DJ_ENGINE_MODE2_CHECK_OFFSET;
            uint8_t expected = (uint8_t)((stream << 1u) | ((~group) & 1u));
            EXPECT_TRUE(mode2Out[channel] == expected);
        } else {
            EXPECT_TRUE(mode2Out[channel] == 0);
        }
    }

    memset(mode2FrameOut, 0, sizeof(mode2FrameOut));
    OpenA8DJEngineMode2PackerInit(&mode2);
    EXPECT_TRUE(OpenA8DJEnginePackMode2Output(&mode2,
                                             &mode2Frames[0][0],
                                             2,
                                             mode2FrameOut,
                                             sizeof(mode2FrameOut)) == sizeof(mode2FrameOut));
    for (frame = 0; frame < 2; frame++) {
        for (channel = 0; channel < OPENA8DJ_ENGINE_OUTPUT_CHANNELS; channel++) {
            OpenA8DJEnginePackS24BE(mode2Frames[frame][channel], packedChannels[frame][channel]);
        }
    }
    EXPECT_TRUE(ExpectMode2SampleByte(mode2FrameOut, packedChannels[0], 0, 4));
    EXPECT_TRUE(ExpectMode2SampleByte(mode2FrameOut, packedChannels[0], 4, 5));
    EXPECT_TRUE(ExpectMode2SampleByte(mode2FrameOut, packedChannels[1], 12, 0));
    EXPECT_TRUE(ExpectMode2SampleByte(mode2FrameOut, packedChannels[1], 16, 1));
    EXPECT_TRUE(ExpectMode2SampleByte(mode2FrameOut, packedChannels[1], 20, 2));
    EXPECT_TRUE(ExpectMode2SampleByte(mode2FrameOut, packedChannels[1], 28, 3));
    EXPECT_TRUE(ExpectMode2SampleByte(mode2FrameOut, packedChannels[1], 32, 4));
    EXPECT_TRUE(ExpectMode2SampleByte(mode2FrameOut, packedChannels[1], 36, 5));
    for (channel = 0; channel < OPENA8DJ_ENGINE_STREAMS; channel++) {
        size_t byteIndex = 40u + channel;
        size_t group = byteIndex / OPENA8DJ_ENGINE_MODE2_GROUP_BYTES;
        uint8_t expected = (uint8_t)((channel << 1u) | ((~group) & 1u));

        EXPECT_TRUE(mode2FrameOut[byteIndex] == expected);
    }

    printf("PASS: OpenA8DJ offline audio engine contract\n");
    return 0;
}
