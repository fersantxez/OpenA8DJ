#import "../hal/OpenA8DJUSB.h"

#import <Foundation/Foundation.h>

#include <math.h>
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

int main(int argc, char **argv)
{
    @autoreleasepool {
        const char *path = argc > 1 ? argv[1] : "build/test-recordings/opena8dj-reference.wav";
        int selectedPair = -1;
        uint32_t leadFrames = 0;
        if (argc > 2) {
            selectedPair = ParsePair(argv[2]);
            if (selectedPair < -1) {
                fprintf(stderr, "usage: %s [wav] [A|B|C|D|all] [lead_frames]\n", argv[0]);
                return 2;
            }
        }
        if (argc > 3) {
            char *end = NULL;
            unsigned long parsed = strtoul(argv[3], &end, 10);
            if (end == argv[3] || *end != '\0' || parsed > UINT32_MAX) {
                fprintf(stderr, "usage: %s [wav] [A|B|C|D|all] [lead_frames]\n", argv[0]);
                return 2;
            }
            leadFrames = (uint32_t)parsed;
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

        if (!OpenA8DJUSBStart((double)sampleRate)) {
            fprintf(stderr, "OpenA8DJUSBStart failed\n");
            return 6;
        }

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
        enum { chunkFrames = 256 };
        float out[chunkFrames * 8];
        uint32_t frame = 0;
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
            frame += todo;
            if (frame > leadFrames) {
                SleepFrames(todo, sampleRate);
            }
        }

        SleepFrames(sampleRate / 2, sampleRate);
        OpenA8DJUSBStop();
        fprintf(stderr, "usb_play completed frames=%u\n", sourceFrames);
        fflush(stderr);
    }
    return 0;
}
