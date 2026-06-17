#import "../hal/OpenA8DJUSB.h"

#import <Foundation/Foundation.h>

#include <math.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint16_t ReadLE16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t ReadLE32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void SleepFrames(uint32_t frames, uint32_t sampleRate)
{
    struct timespec ts;
    double seconds = (double)frames / (double)sampleRate;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);
    nanosleep(&ts, NULL);
}

static uint64_t MonotonicNsec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ull) + (uint64_t)ts.tv_nsec;
}

static void SleepUntilNsec(uint64_t deadline)
{
    for (;;) {
        uint64_t now = MonotonicNsec();
        if (now >= deadline) {
            return;
        }
        uint64_t remaining = deadline - now;
        struct timespec ts;
        ts.tv_sec = (time_t)(remaining / 1000000000ull);
        ts.tv_nsec = (long)(remaining % 1000000000ull);
        nanosleep(&ts, NULL);
    }
}

static void PrintEvent(const char *label, uint64_t originNsec)
{
    uint64_t now = MonotonicNsec();
    double seconds = originNsec > 0 && now >= originNsec ?
        (double)(now - originNsec) / 1000000000.0 :
        0.0;
    fprintf(stderr,
            "usb_play_event label=%s monotonic_nsec=%" PRIu64 " seconds_since_process_start=%.6f\n",
            label,
            now,
            seconds);
    fflush(stderr);
}

static int ParsePair(const char *text)
{
    if (text == NULL || strcmp(text, "all") == 0 || strcmp(text, "ALL") == 0) {
        return -1;
    }
    if (strlen(text) != 1) {
        return -2;
    }
    char c = text[0];
    if (c >= 'a' && c <= 'd') {
        c = (char)(c - 'a' + 'A');
    }
    if (c < 'A' || c > 'D') {
        return -2;
    }
    return (int)(c - 'A');
}

static void PrintDiagnostics(const char *label)
{
    OpenA8DJUSBDiagnostics diagnostics;
    if (!OpenA8DJUSBGetDiagnostics(&diagnostics)) {
        fprintf(stderr, "direct_diag label=%s available=0\n", label);
        fflush(stderr);
        return;
    }

    fprintf(stderr,
            "direct_diag label=%s available=1 size=%u running=%u streaming=%u rate=%.0f "
            "fw=%u align=%u analog_in=%u analog_out=%u "
            "control=%02x:%02x:%02x:%02x:%02x:%02x "
            "audio_reset_rate=0x%02x audio_reset_depth=%u audio_reset_bpp=%u audio_reset_ok=%u "
            "audio_stream_rate=0x%02x audio_stream_depth=%u audio_stream_bpp=%u audio_stream_ok=%u "
            "output_byte=%u ring_frames=%u target_latency=%u lead=%u queue_target=%u in_flight=%u "
            "select_alt0_before_alt1=%u "
            "capture_transfers=%" PRIu64 " playback_transfers=%" PRIu64 " "
            "frames_written=%" PRIu64 " frames_read=%" PRIu64 " startup_silence=%" PRIu64 " "
            "underruns=%" PRIu64 " active_underruns=%" PRIu64 " elastic_drops=%" PRIu64 " "
            "elastic_replays=%" PRIu64 " timeline_resets=%" PRIu64 " late_frames=%" PRIu64 " "
            "late_batches=%" PRIu64 " next_frame=%" PRIu64 " sched_resets=%" PRIu64 " "
            "sched_too_old=%" PRIu64 " sched_too_new=%" PRIu64 " sched_out_of_window=%" PRIu64 " "
            "sched_fallbacks=%" PRIu64 " queue_failures=%" PRIu64 " qfail_last=0x%08" PRIx64 " "
            "qfail_no_error=%" PRIu64 " qfail_too_old=%" PRIu64 " qfail_too_new=%" PRIu64 " "
            "qfail_other=%" PRIu64 " qfail_explicit=%" PRIu64 " qfail_consumed=%" PRIu64 " "
            "qfail_startup_silence=%" PRIu64 " "
            "queue_bytes_min=%" PRIu64 " queue_bytes_max=%" PRIu64 " queue_bytes_sum=%" PRIu64 " "
            "queue_bytes_samples=%" PRIu64 " queue_tx_min=%" PRIu64 " queue_tx_max=%" PRIu64 " "
            "queue_tx_sum=%" PRIu64 " queue_tx_samples=%" PRIu64 " "
            "request_count_min=%" PRIu64 " request_count_max=%" PRIu64 " request_count_sum=%" PRIu64 " "
            "request_count_samples=%" PRIu64 " complete_count_min=%" PRIu64 " complete_count_max=%" PRIu64 " "
            "complete_count_sum=%" PRIu64 " complete_count_samples=%" PRIu64 " expected_ticks=%" PRIu64 "\n",
            label,
            diagnostics.size,
            diagnostics.running,
            diagnostics.streaming,
            diagnostics.sampleRate,
            diagnostics.specFwVersion,
            diagnostics.specDataAlignment,
            diagnostics.specNumAnalogAudioIn,
            diagnostics.specNumAnalogAudioOut,
            diagnostics.control[0],
            diagnostics.control[1],
            diagnostics.control[2],
            diagnostics.control[3],
            diagnostics.control[4],
            diagnostics.control[5],
            diagnostics.lastAudioParamsResetRateCode,
            diagnostics.lastAudioParamsResetDepth,
            diagnostics.lastAudioParamsResetBytesPerPacket,
            diagnostics.lastAudioParamsResetOk,
            diagnostics.lastAudioParamsStreamRateCode,
            diagnostics.lastAudioParamsStreamDepth,
            diagnostics.lastAudioParamsStreamBytesPerPacket,
            diagnostics.lastAudioParamsStreamOk,
            diagnostics.outputByteInFrame,
            diagnostics.outputRingFrames,
            diagnostics.outputTargetLatencyFrames,
            diagnostics.playbackLeadFrames,
            diagnostics.playbackQueueTarget,
            diagnostics.playbackTransfersInFlight,
            diagnostics.selectAlt0BeforeAlt1,
            diagnostics.captureTransfers,
            diagnostics.playbackTransfers,
            diagnostics.outputFramesWritten,
            diagnostics.outputFramesRead,
            diagnostics.outputStartupSilenceFrames,
            diagnostics.outputUnderruns,
            diagnostics.outputActiveUnderruns,
            diagnostics.outputElasticDrops,
            diagnostics.outputElasticReplays,
            diagnostics.outputTimelineResets,
            diagnostics.outputLateWriteFrames,
            diagnostics.outputLateWriteBatches,
            diagnostics.playbackNextFrameNumber,
            diagnostics.playbackScheduleResets,
            diagnostics.playbackScheduleTooOld,
            diagnostics.playbackScheduleTooNew,
            diagnostics.playbackScheduleOutOfWindow,
            diagnostics.playbackScheduleFallbacks,
            diagnostics.playbackQueueFailures,
            diagnostics.playbackQueueFailureLastStatus,
            diagnostics.playbackQueueFailureNoError,
            diagnostics.playbackQueueFailureTooOld,
            diagnostics.playbackQueueFailureTooNew,
            diagnostics.playbackQueueFailureOther,
            diagnostics.playbackQueueFailureExplicit,
            diagnostics.playbackQueueFailureConsumedFrames,
            diagnostics.playbackQueueFailureStartupSilenceFrames,
            diagnostics.playbackQueueBytesMin,
            diagnostics.playbackQueueBytesMax,
            diagnostics.playbackQueueBytesSum,
            diagnostics.playbackQueueBytesSamples,
            diagnostics.playbackQueueTransactionsMin,
            diagnostics.playbackQueueTransactionsMax,
            diagnostics.playbackQueueTransactionsSum,
            diagnostics.playbackQueueTransactionsSamples,
            diagnostics.playbackRequestCountMin,
            diagnostics.playbackRequestCountMax,
            diagnostics.playbackRequestCountSum,
            diagnostics.playbackRequestCountSamples,
            diagnostics.playbackCompleteCountMin,
            diagnostics.playbackCompleteCountMax,
            diagnostics.playbackCompleteCountSum,
            diagnostics.playbackCompleteCountSamples,
            diagnostics.cadenceExpectedTransferTicks);
    fflush(stderr);
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        uint64_t processStartNsec = MonotonicNsec();
        PrintEvent("process-start", processStartNsec);
        const char *path = argc > 1 ? argv[1] : "build/test-recordings/opena8dj-reference.wav";
        int selectedPair = -1;
        uint32_t leadFrames = 0;
        bool applyPlaybackProfile = false;
        if (argc > 2) {
            selectedPair = ParsePair(argv[2]);
            if (selectedPair < -1) {
                fprintf(stderr, "usage: %s [wav] [A|B|C|D|all] [lead_frames] [--playback-profile]\n", argv[0]);
                return 2;
            }
        }
        if (argc > 3) {
            char *end = NULL;
            unsigned long parsed = strtoul(argv[3], &end, 10);
            if (end == argv[3] || *end != '\0' || parsed > UINT32_MAX) {
                fprintf(stderr, "usage: %s [wav] [A|B|C|D|all] [lead_frames] [--playback-profile]\n", argv[0]);
                return 2;
            }
            leadFrames = (uint32_t)parsed;
        }
        if (argc > 4) {
            if (argc != 5 || strcmp(argv[4], "--playback-profile") != 0) {
                fprintf(stderr, "usage: %s [wav] [A|B|C|D|all] [lead_frames] [--playback-profile]\n", argv[0]);
                return 2;
            }
            applyPlaybackProfile = true;
        }
        NSData *data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:path]];
        if (data == nil || data.length < 44) {
            fprintf(stderr, "could not read wav: %s\n", path);
            return 2;
        }

        const uint8_t *bytes = data.bytes;
        if (memcmp(bytes, "RIFF", 4) != 0 || memcmp(bytes + 8, "WAVE", 4) != 0) {
            fprintf(stderr, "not a RIFF/WAVE file\n");
            return 3;
        }

        uint16_t channels = 0;
        uint32_t sampleRate = 0;
        uint16_t bitsPerSample = 0;
        const uint8_t *audio = NULL;
        uint32_t audioBytes = 0;
        size_t offset = 12;
        while (offset + 8 <= data.length) {
            const uint8_t *chunk = bytes + offset;
            uint32_t chunkSize = ReadLE32(chunk + 4);
            offset += 8;
            if (offset + chunkSize > data.length) {
                break;
            }
            if (memcmp(chunk, "fmt ", 4) == 0 && chunkSize >= 16) {
                uint16_t format = ReadLE16(bytes + offset);
                channels = ReadLE16(bytes + offset + 2);
                sampleRate = ReadLE32(bytes + offset + 4);
                bitsPerSample = ReadLE16(bytes + offset + 14);
                if (format != 1) {
                    fprintf(stderr, "only PCM wav is supported\n");
                    return 4;
                }
            } else if (memcmp(chunk, "data", 4) == 0) {
                audio = bytes + offset;
                audioBytes = chunkSize;
            }
            offset += (chunkSize + 1u) & ~1u;
        }

        if (audio == NULL || channels < 1 || channels > 2 || sampleRate == 0 || bitsPerSample != 16) {
            fprintf(stderr, "unsupported wav format channels=%u rate=%u bits=%u\n",
                    channels,
                    sampleRate,
                    bitsPerSample);
            return 5;
        }

        if (applyPlaybackProfile) {
            if (!OpenA8DJUSBEnsureOpen((double)sampleRate)) {
                fprintf(stderr, "OpenA8DJUSBEnsureOpen failed\n");
                return 6;
            }
            PrintEvent("after-ensure-open", processStartNsec);
            if (!OpenA8DJUSBApplyPlaybackProfile()) {
                fprintf(stderr, "OpenA8DJUSBApplyPlaybackProfile failed\n");
                return 6;
            }
            PrintEvent("after-playback-profile", processStartNsec);
            PrintDiagnostics("after-playback-profile");
        }
        PrintEvent("before-start", processStartNsec);
        if (!OpenA8DJUSBStart((double)sampleRate)) {
            fprintf(stderr, "OpenA8DJUSBStart failed\n");
            return 6;
        }
        uint64_t startDoneNsec = MonotonicNsec();
        PrintEvent("after-start", processStartNsec);
        PrintDiagnostics("after-start");

        const uint32_t sourceFrames = audioBytes / ((uint32_t)channels * sizeof(int16_t));
        fprintf(stderr,
                "usb_play path=%s pair=%s lead_frames=%u rate=%u channels=%u source_frames=%u duration=%.3f\n",
                path,
                selectedPair < 0 ? "all" : (const char *[]){"A", "B", "C", "D"}[selectedPair],
                leadFrames,
                sampleRate,
                channels,
                sourceFrames,
                (double)sourceFrames / (double)sampleRate);
        fflush(stderr);
        PrintDiagnostics("before-play");
        enum { chunkFrames = 256 };
        float out[chunkFrames * 8];
        uint32_t frame = 0;
        uint64_t playbackStartNsec = MonotonicNsec();
        PrintEvent("before-first-write", processStartNsec);
        bool printedFirstWrite = false;
        while (frame < sourceFrames) {
            uint32_t todo = sourceFrames - frame;
            if (todo > chunkFrames) {
                todo = chunkFrames;
            }
            memset(out, 0, sizeof(out));
            for (uint32_t i = 0; i < todo; i++) {
                const int16_t *src = (const int16_t *)(audio + ((frame + i) * channels * sizeof(int16_t)));
                float left = (float)src[0] / 32768.0f;
                float right = channels > 1 ? (float)src[1] / 32768.0f : left;
                for (uint32_t pair = 0; pair < 4; pair++) {
                    if (selectedPair >= 0 && (uint32_t)selectedPair != pair) {
                        continue;
                    }
                    out[i * 8 + pair * 2] = left;
                    out[i * 8 + pair * 2 + 1] = right;
                }
            }
            OpenA8DJUSBWriteOutput(out, todo, 8);
            if (!printedFirstWrite) {
                PrintEvent("after-first-write", processStartNsec);
                printedFirstWrite = true;
            }
            frame += todo;
            if (frame > leadFrames) {
                double elapsedFrames = (double)(frame - leadFrames);
                uint64_t elapsedNsec = (uint64_t)((elapsedFrames * 1000000000.0) / (double)sampleRate);
                SleepUntilNsec(playbackStartNsec + elapsedNsec);
            }
        }

        PrintDiagnostics("after-play");
        uint64_t afterPlayNsec = MonotonicNsec();
        PrintEvent("after-play", processStartNsec);
        SleepFrames(sampleRate / 2, sampleRate);
        PrintDiagnostics("before-stop");
        PrintEvent("before-stop", processStartNsec);
        OpenA8DJUSBStop();
        PrintEvent("after-stop", processStartNsec);
        PrintDiagnostics("after-stop");
        fprintf(stderr,
                "usb_play completed frames=%u start_to_before_play_seconds=%.6f play_loop_seconds=%.6f\n",
                sourceFrames,
                (double)(playbackStartNsec - startDoneNsec) / 1000000000.0,
                (double)(afterPlayNsec - playbackStartNsec) / 1000000000.0);
        fflush(stderr);
    }
    return 0;
}
