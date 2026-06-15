#include "open_a8dj_rust.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    kStreams = 4,
    kChannelsPerStream = 2,
    kChannels = kStreams * kChannelsPerStream,
    kBytesPerSample = 3,
    kBytesPerSampleUSB = 4,
    kFrameBytesPerStream = kChannelsPerStream * kBytesPerSample,
    kGroupBytes = kStreams * kBytesPerSampleUSB,
    kCheckOffset = kStreams * kChannelsPerStream,
    kDefaultStartByte = kBytesPerSample + 1,
    kDefaultTransferBytes = 352,
    kDefaultFrames = 64,
    kS24Max = 8388607
};

typedef struct LegacyPacker {
    const float *frames;
    uint32_t frameCount;
    uint32_t frameIndex;
    uint32_t outputByteInFrame;
    bool outputFrameLoaded;
    uint8_t outputFrameBytes[kStreams][kFrameBytesPerStream];
    float gain;
    uint32_t byteOrder;
} LegacyPacker;

typedef struct RustCallbackContext {
    const float *frames;
    uint32_t frameCount;
    uint32_t frameIndex;
} RustCallbackContext;

static uint8_t Mode2CheckByte(uint32_t stream, uint32_t byteIndex)
{
    uint32_t group = byteIndex / kGroupBytes;
    return (uint8_t)((stream << 1) | ((~group) & 1u));
}

static int32_t FloatToOutputI24(float sample, float gain)
{
    sample *= gain;
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;

    int32_t value;
    if (sample >= 1.0f) {
        value = INT32_MAX;
    } else if (sample <= -1.0f) {
        value = INT32_MIN;
    } else {
        value = (int32_t)lrintf(sample * 2147483647.0f);
    }
    return value >> 8;
}

static void EncodeI24(int32_t value, uint32_t byteOrder, uint8_t *bytes)
{
    uint32_t raw = (uint32_t)value & 0x00ffffffu;
    if (byteOrder == OPENA8DJ_RUST_BYTE_ORDER_NATIVE_LITTLE_ENDIAN) {
        bytes[0] = (uint8_t)(raw & 0xffu);
        bytes[1] = (uint8_t)((raw >> 8) & 0xffu);
        bytes[2] = (uint8_t)((raw >> 16) & 0xffu);
    } else {
        bytes[0] = (uint8_t)((raw >> 16) & 0xffu);
        bytes[1] = (uint8_t)((raw >> 8) & 0xffu);
        bytes[2] = (uint8_t)(raw & 0xffu);
    }
}

static int32_t SyntheticS24Value(uint32_t frameIndex, uint32_t channel)
{
    uint32_t stream = channel / kChannelsPerStream;
    uint32_t side = channel % kChannelsPerStream;
    uint32_t magnitude = ((stream + 1u) * 1000000u) + (side * 250000u);
    magnitude += (frameIndex % 8192u) * 257u;
    return side == 0 ? (int32_t)magnitude : -(int32_t)magnitude;
}

static void GenerateSyntheticFrames(float *frames, uint32_t frameCount)
{
    for (uint32_t frame = 0; frame < frameCount; frame++) {
        for (uint32_t channel = 0; channel < kChannels; channel++) {
            frames[(size_t)frame * kChannels + channel] =
                (float)SyntheticS24Value(frame, channel) / (float)kS24Max;
        }
    }
}

static void LegacyLoadNextFrameIfNeeded(LegacyPacker *packer)
{
    if (packer->outputFrameLoaded && packer->outputByteInFrame != 0) {
        return;
    }

    if (packer->frameIndex < packer->frameCount) {
        const float *frame = &packer->frames[(size_t)packer->frameIndex * kChannels];
        for (uint32_t stream = 0; stream < kStreams; stream++) {
            int32_t left = FloatToOutputI24(frame[stream * 2], packer->gain);
            int32_t right = FloatToOutputI24(frame[stream * 2 + 1], packer->gain);
            EncodeI24(left, packer->byteOrder, &packer->outputFrameBytes[stream][0]);
            EncodeI24(right, packer->byteOrder, &packer->outputFrameBytes[stream][3]);
        }
        packer->frameIndex++;
    } else {
        memset(packer->outputFrameBytes, 0, sizeof(packer->outputFrameBytes));
    }
    packer->outputFrameLoaded = true;
}

static void LegacyFill(LegacyPacker *packer, uint8_t *out, uint32_t length)
{
    uint32_t i = 0;
    while (i < length) {
        if ((i % kGroupBytes) == kCheckOffset) {
            for (uint32_t stream = 0; stream < kStreams && i < length; stream++, i++) {
                out[i] = Mode2CheckByte(stream, i);
            }
            continue;
        }

        LegacyLoadNextFrameIfNeeded(packer);
        for (uint32_t stream = 0; stream < kStreams && i < length; stream++, i++) {
            out[i] = packer->outputFrameBytes[stream][packer->outputByteInFrame];
        }
        packer->outputByteInFrame++;
        if (packer->outputByteInFrame >= kFrameBytesPerStream) {
            packer->outputByteInFrame = 0;
        }
    }
}

static uint32_t RustNextFrame(void *context, float *outFrame, uint32_t channels)
{
    RustCallbackContext *callback = (RustCallbackContext *)context;
    if (callback == NULL || outFrame == NULL || channels != kChannels) {
        return 0;
    }
    if (callback->frameIndex < callback->frameCount) {
        memcpy(outFrame,
               &callback->frames[(size_t)callback->frameIndex * kChannels],
               sizeof(float) * kChannels);
    } else {
        memset(outFrame, 0, sizeof(float) * kChannels);
    }
    callback->frameIndex++;
    return 1;
}

static int RunCase(uint32_t startByte,
                   uint32_t transferBytes,
                   uint32_t transfers,
                   float gain,
                   uint32_t byteOrder,
                   uint32_t frameCount)
{
    size_t frameSamples = (size_t)frameCount * kChannels;
    size_t byteCount = (size_t)transferBytes * transfers;
    float *frames = calloc(frameSamples, sizeof(float));
    uint8_t *legacy = calloc(byteCount, sizeof(uint8_t));
    uint8_t *rust = calloc(byteCount, sizeof(uint8_t));
    if (frames == NULL || legacy == NULL || rust == NULL) {
        fprintf(stderr, "allocation failed\n");
        free(frames);
        free(legacy);
        free(rust);
        return 2;
    }

    GenerateSyntheticFrames(frames, frameCount);

    LegacyPacker legacyPacker = {
        .frames = frames,
        .frameCount = frameCount,
        .frameIndex = 0,
        .outputByteInFrame = startByte,
        .outputFrameLoaded = false,
        .gain = gain,
        .byteOrder = byteOrder,
    };
    OpenA8DJRustConfig config;
    OpenA8DJRustEngine *engine = NULL;
    OpenA8DJRustStatus status = opena8dj_rust_config_default(&config);
    if (status != OPENA8DJ_RUST_OK) {
        fprintf(stderr, "rust config default failed status=%d\n", status);
        free(frames);
        free(legacy);
        free(rust);
        return 3;
    }
    config.start_byte = startByte;
    config.transfer_bytes = transferBytes;
    config.output_gain = gain;
    config.byte_order = byteOrder;
    status = opena8dj_rust_engine_create(&config, &engine);
    if (status != OPENA8DJ_RUST_OK || engine == NULL) {
        fprintf(stderr, "rust engine create failed status=%d\n", status);
        free(frames);
        free(legacy);
        free(rust);
        return 4;
    }

    RustCallbackContext callback = {
        .frames = frames,
        .frameCount = frameCount,
        .frameIndex = 0,
    };

    for (uint32_t transfer = 0; transfer < transfers; transfer++) {
        uint8_t *legacyTransfer = legacy + (size_t)transfer * transferBytes;
        uint8_t *rustTransfer = rust + (size_t)transfer * transferBytes;
        uint32_t consumed = 0;
        LegacyFill(&legacyPacker, legacyTransfer, transferBytes);
        status = opena8dj_rust_engine_fill_playback_bytes_with_callback(engine,
                                                                        RustNextFrame,
                                                                        &callback,
                                                                        rustTransfer,
                                                                        transferBytes,
                                                                        &consumed);
        if (status != OPENA8DJ_RUST_OK) {
            fprintf(stderr,
                    "rust fill failed status=%d start=%u transfer=%u gain=%.3f byte_order=%u\n",
                    status,
                    startByte,
                    transfer,
                    gain,
                    byteOrder);
            opena8dj_rust_engine_destroy(engine);
            free(frames);
            free(legacy);
            free(rust);
            return 5;
        }
    }

    int rc = 0;
    for (size_t index = 0; index < byteCount; index++) {
        if (legacy[index] != rust[index]) {
            fprintf(stderr,
                    "FAIL packet_parity start=%u transfer_bytes=%u transfers=%u gain=%.3f byte_order=%u index=%zu legacy=0x%02x rust=0x%02x\n",
                    startByte,
                    transferBytes,
                    transfers,
                    gain,
                    byteOrder,
                    index,
                    legacy[index],
                    rust[index]);
            rc = 1;
            break;
        }
    }

    uint32_t rustOutputByte = UINT32_MAX;
    status = opena8dj_rust_engine_output_byte_in_frame(engine, &rustOutputByte);
    if (status != OPENA8DJ_RUST_OK || rustOutputByte != legacyPacker.outputByteInFrame) {
        fprintf(stderr,
                "FAIL cursor_parity start=%u legacy=%u rust=%u status=%d\n",
                startByte,
                legacyPacker.outputByteInFrame,
                rustOutputByte,
                status);
        rc = 1;
    }

    OpenA8DJRustCounters counters;
    status = opena8dj_rust_engine_snapshot_counters(engine, &counters);
    if (status != OPENA8DJ_RUST_OK ||
        counters.fill_calls != transfers ||
        counters.bytes_packed != byteCount) {
        fprintf(stderr,
                "FAIL counter_parity status=%d fill_calls=%llu bytes_packed=%llu expected_calls=%u expected_bytes=%zu\n",
                status,
                (unsigned long long)counters.fill_calls,
                (unsigned long long)counters.bytes_packed,
                transfers,
                byteCount);
        rc = 1;
    }

    opena8dj_rust_engine_destroy(engine);
    free(frames);
    free(legacy);
    free(rust);
    if (rc == 0) {
        printf("PASS case start_byte=%u transfer_bytes=%u transfers=%u gain=%.3f byte_order=%s bytes=%zu\n",
               startByte,
               transferBytes,
               transfers,
               gain,
               byteOrder == OPENA8DJ_RUST_BYTE_ORDER_NATIVE_LITTLE_ENDIAN ? "native" : "big",
               byteCount);
    }
    return rc;
}

int main(void)
{
    const uint32_t transferBytes[] = { kDefaultTransferBytes, kGroupBytes * 3u, kGroupBytes * 5u };
    const float gains[] = { 1.0f, 0.5f };
    const uint32_t byteOrders[] = {
        OPENA8DJ_RUST_BYTE_ORDER_BIG_ENDIAN,
        OPENA8DJ_RUST_BYTE_ORDER_NATIVE_LITTLE_ENDIAN
    };

    for (uint32_t startByte = 0; startByte < kFrameBytesPerStream; startByte++) {
        for (size_t transferIndex = 0; transferIndex < sizeof(transferBytes) / sizeof(transferBytes[0]); transferIndex++) {
            for (size_t gainIndex = 0; gainIndex < sizeof(gains) / sizeof(gains[0]); gainIndex++) {
                for (size_t orderIndex = 0; orderIndex < sizeof(byteOrders) / sizeof(byteOrders[0]); orderIndex++) {
                    int rc = RunCase(startByte,
                                     transferBytes[transferIndex],
                                     6,
                                     gains[gainIndex],
                                     byteOrders[orderIndex],
                                     kDefaultFrames);
                    if (rc != 0) {
                        return rc;
                    }
                }
            }
        }
    }

    printf("PASS rust_packet_parity cases=%u\n", (unsigned)(kFrameBytesPerStream * 3u * 2u * 2u));
    return 0;
}

