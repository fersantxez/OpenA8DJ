#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENA8DJ_RUST_CONFIG_VERSION 1u
#define OPENA8DJ_RUST_COUNTERS_VERSION 1u
#define OPENA8DJ_RUST_BYTE_ORDER_BIG_ENDIAN 0u
#define OPENA8DJ_RUST_BYTE_ORDER_NATIVE_LITTLE_ENDIAN 1u

typedef enum OpenA8DJRustStatus {
    OPENA8DJ_RUST_OK = 0,
    OPENA8DJ_RUST_NULL_POINTER = -1,
    OPENA8DJ_RUST_INVALID_CONFIG = -2,
    OPENA8DJ_RUST_INVALID_HANDLE = -3,
    OPENA8DJ_RUST_INVALID_CHANNELS = -4,
    OPENA8DJ_RUST_INVALID_BUFFER = -5,
    OPENA8DJ_RUST_PANIC = -100,
} OpenA8DJRustStatus;

typedef struct OpenA8DJRustConfig {
    uint32_t size;
    uint32_t version;
    uint32_t start_byte;
    uint32_t transfer_bytes;
    float output_gain;
    uint32_t byte_order;
} OpenA8DJRustConfig;

typedef struct OpenA8DJRustCounters {
    uint32_t size;
    uint32_t version;
    uint64_t frames_submitted;
    uint64_t frames_consumed;
    uint64_t bytes_packed;
    uint64_t fill_calls;
    uint64_t invalid_calls;
    int32_t last_status;
} OpenA8DJRustCounters;

typedef struct OpenA8DJRustEngine OpenA8DJRustEngine;

OpenA8DJRustStatus opena8dj_rust_config_default(OpenA8DJRustConfig *out_config);
uint32_t opena8dj_rust_channels(void);
uint32_t opena8dj_rust_default_start_byte(void);
uint32_t opena8dj_rust_default_transfer_bytes(void);

OpenA8DJRustStatus opena8dj_rust_stream_frame_bytes(
    const float *input_frame,
    uint32_t channels,
    uint8_t *output_bytes,
    size_t output_len,
    float output_gain,
    uint32_t byte_order);

OpenA8DJRustStatus opena8dj_rust_engine_create(
    const OpenA8DJRustConfig *config,
    OpenA8DJRustEngine **out_engine);

OpenA8DJRustStatus opena8dj_rust_engine_destroy(OpenA8DJRustEngine *engine);

OpenA8DJRustStatus opena8dj_rust_engine_fill_playback_bytes(
    OpenA8DJRustEngine *engine,
    const float *input_frames,
    uint32_t frame_count,
    uint32_t channels,
    uint8_t *output_bytes,
    size_t output_len,
    uint32_t *out_frames_consumed);

OpenA8DJRustStatus opena8dj_rust_engine_snapshot_counters(
    const OpenA8DJRustEngine *engine,
    OpenA8DJRustCounters *out_counters);

OpenA8DJRustStatus opena8dj_rust_engine_reset(
    OpenA8DJRustEngine *engine,
    const OpenA8DJRustConfig *config);

#ifdef __cplusplus
}
#endif
