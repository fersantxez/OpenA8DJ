#import "../hal/OpenA8DJUSB.h"

#import <Foundation/Foundation.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void SleepFrames(uint32_t frames, double sampleRate)
{
    double seconds = (double)frames / sampleRate;
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);
    nanosleep(&ts, NULL);
}

static uint32_t ParsePair(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    if (text[0] >= 'A' && text[0] <= 'D') {
        return (uint32_t)(text[0] - 'A');
    }
    if (text[0] >= 'a' && text[0] <= 'd') {
        return (uint32_t)(text[0] - 'a');
    }
    return (uint32_t)strtoul(text, NULL, 10);
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        uint32_t pair = argc > 1 ? ParsePair(argv[1]) : 0;
        int seconds = argc > 2 ? atoi(argv[2]) : 12;
        double frequency = argc > 3 ? strtod(argv[3], NULL) : 1000.0;
        double amplitude = argc > 4 ? strtod(argv[4], NULL) : 0.12;
        double sampleRate = argc > 5 ? strtod(argv[5], NULL) : 48000.0;

        if (pair > 3 || seconds <= 0 || seconds > 120 ||
            frequency <= 0.0 || amplitude <= 0.0 || amplitude > 0.9 ||
            sampleRate < 40000.0 || sampleRate > 96000.0) {
            fprintf(stderr,
                    "usage: opena8dj-usb-pair-tone <A|B|C|D|0-3> [seconds] [frequency] [amplitude] [sample_rate]\n");
            return 2;
        }

        if (!OpenA8DJUSBStart(sampleRate)) {
            fprintf(stderr, "OpenA8DJUSBStart failed\n");
            return 3;
        }

        enum { kChannels = 8, kChunkFrames = 256 };
        float output[kChunkFrames * kChannels];
        uint64_t totalFrames = (uint64_t)llround(sampleRate * (double)seconds);
        uint64_t frame = 0;
        double phase = 0.0;
        double step = (2.0 * M_PI * frequency) / sampleRate;

        fprintf(stderr,
                "usb_pair_tone pair=%c seconds=%d frequency=%.3f amplitude=%.6f rate=%.0f\n",
                (char)('A' + pair),
                seconds,
                frequency,
                amplitude,
                sampleRate);

        while (frame < totalFrames) {
            uint32_t todo = (uint32_t)((totalFrames - frame) > kChunkFrames ?
                                      kChunkFrames :
                                      (totalFrames - frame));
            memset(output, 0, sizeof(output));
            for (uint32_t i = 0; i < todo; i++) {
                float value = (float)(amplitude * sin(phase));
                output[(size_t)i * kChannels + pair * 2] = value;
                output[(size_t)i * kChannels + pair * 2 + 1] = value;
                phase += step;
                if (phase >= 2.0 * M_PI) {
                    phase -= 2.0 * M_PI;
                }
            }
            OpenA8DJUSBWriteOutput(output, todo, kChannels);
            frame += todo;
            SleepFrames(todo, sampleRate);
        }

        SleepFrames((uint32_t)(sampleRate / 2.0), sampleRate);
        OpenA8DJUSBStop();
        fprintf(stderr, "usb_pair_tone completed frames=%llu\n", totalFrames);
    }
    return 0;
}
