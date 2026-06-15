#include "open_a8dj_rust.h"

#include <stdint.h>

int main(void) {
    OpenA8DJRustConfig config;
    OpenA8DJRustEngine *engine = 0;
    float frames[8 * 8] = {0};
    uint8_t output[352] = {0};
    uint32_t consumed = 0;
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
    if (opena8dj_rust_engine_snapshot_counters(engine, &counters) != OPENA8DJ_RUST_OK) {
        return 8;
    }
    if (counters.fill_calls != 1 || counters.bytes_packed != sizeof(output)) {
        return 9;
    }
    if (opena8dj_rust_engine_destroy(engine) != OPENA8DJ_RUST_OK) {
        return 10;
    }

    return 0;
}

