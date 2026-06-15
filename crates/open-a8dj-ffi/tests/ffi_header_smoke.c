#include "open_a8dj_rust.h"

#include <stdint.h>

static uint32_t next_silent_frame(void *context, float *out_frame, uint32_t channels) {
    uint32_t *counter = (uint32_t *)context;
    if (!counter || !out_frame || channels != 8) {
        return 0;
    }
    for (uint32_t channel = 0; channel < channels; channel++) {
        out_frame[channel] = 0.0f;
    }
    *counter += 1;
    return 1;
}

int main(void) {
    OpenA8DJRustConfig config;
    OpenA8DJRustEngine *engine = 0;
    float frames[8 * 8] = {0};
    uint8_t output[352] = {0};
    uint8_t stream_frame[24] = {0};
    uint32_t consumed = 0;
    uint32_t callback_count = 0;
    OpenA8DJRustCounters counters;

    if (opena8dj_rust_config_default(&config) != OPENA8DJ_RUST_OK) {
        return 1;
    }
    if (opena8dj_rust_channels() != 8) {
        return 2;
    }
    if (opena8dj_rust_default_start_byte() != 4) {
        return 3;
    }
    if (opena8dj_rust_default_transfer_bytes() != sizeof(output)) {
        return 4;
    }
    frames[1] = 1.0f;
    if (opena8dj_rust_stream_frame_bytes(frames,
                                         8,
                                         stream_frame,
                                         sizeof(stream_frame),
                                         1.0f,
                                         OPENA8DJ_RUST_BYTE_ORDER_BIG_ENDIAN) != OPENA8DJ_RUST_OK) {
        return 11;
    }
    if (stream_frame[3] != 0x7f || stream_frame[4] != 0xff || stream_frame[5] != 0xff) {
        return 12;
    }
    if (opena8dj_rust_engine_create(&config, &engine) != OPENA8DJ_RUST_OK || !engine) {
        return 5;
    }
    if (opena8dj_rust_engine_fill_playback_bytes(engine,
                                                 frames,
                                                 8,
                                                 8,
                                                 output,
                                                 sizeof(output),
                                                 &consumed) != OPENA8DJ_RUST_OK) {
        return 6;
    }
    if (consumed == 0) {
        return 7;
    }
    if (opena8dj_rust_engine_reset(engine, &config) != OPENA8DJ_RUST_OK) {
        return 13;
    }
    if (opena8dj_rust_engine_fill_playback_bytes_with_callback(engine,
                                                               next_silent_frame,
                                                               &callback_count,
                                                               output,
                                                               sizeof(output),
                                                               &consumed) != OPENA8DJ_RUST_OK) {
        return 14;
    }
    if (callback_count == 0 || consumed != callback_count) {
        return 15;
    }
    if (opena8dj_rust_engine_snapshot_counters(engine, &counters) != OPENA8DJ_RUST_OK) {
        return 8;
    }
    if (counters.fill_calls != 2 || counters.bytes_packed != sizeof(output) * 2) {
        return 9;
    }
    if (opena8dj_rust_engine_destroy(engine) != OPENA8DJ_RUST_OK) {
        return 10;
    }

    return 0;
}
