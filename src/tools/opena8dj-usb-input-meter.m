#import "../hal/OpenA8DJUSB.h"

#import <Foundation/Foundation.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    kChannels = 8,
    kPairs = 4,
    kChunkFrames = 256
};

typedef struct PairStats {
    uint64_t frames;
    double leftSquare;
    double rightSquare;
    double cross;
    double leftPeak;
    double rightPeak;
} PairStats;

static void SleepMilliseconds(long milliseconds)
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void AddFrame(PairStats *stats, float left, float right)
{
    double l = left;
    double r = right;
    stats->leftSquare += l * l;
    stats->rightSquare += r * r;
    stats->cross += l * r;
    double leftAbs = fabs(l);
    double rightAbs = fabs(r);
    if (leftAbs > stats->leftPeak) stats->leftPeak = leftAbs;
    if (rightAbs > stats->rightPeak) stats->rightPeak = rightAbs;
    stats->frames++;
}

static void PrintPair(char name, const PairStats *stats)
{
    double rmsL = 0.0;
    double rmsR = 0.0;
    double corr = 0.0;
    if (stats->frames > 0) {
        rmsL = sqrt(stats->leftSquare / (double)stats->frames);
        rmsR = sqrt(stats->rightSquare / (double)stats->frames);
        double denom = sqrt(stats->leftSquare * stats->rightSquare);
        if (denom > 0.0) {
            corr = stats->cross / denom;
        }
    }
    printf("Input %c: frames=%llu rmsL=%.8f rmsR=%.8f peakL=%.8f peakR=%.8f corr=%.4f\n",
           name,
           stats->frames,
           rmsL,
           rmsR,
           stats->leftPeak,
           stats->rightPeak,
           corr);
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        int seconds = argc > 1 ? atoi(argv[1]) : 6;
        if (seconds <= 0 || seconds > 60) {
            seconds = 6;
        }

        double sampleRate = argc > 2 ? strtod(argv[2], NULL) : 48000.0;
        if (sampleRate < 40000.0 || sampleRate > 96000.0) {
            sampleRate = 48000.0;
        }

        if (!OpenA8DJUSBStart(sampleRate)) {
            fprintf(stderr, "OpenA8DJUSBStart failed\n");
            return 2;
        }

        PairStats stats[kPairs];
        memset(stats, 0, sizeof(stats));

        uint64_t targetFrames = (uint64_t)llround(sampleRate * (double)seconds);
        uint64_t readFrames = 0;
        uint64_t emptyReads = 0;
        float input[kChunkFrames * kChannels];
        while (readFrames < targetFrames) {
            uint32_t got = OpenA8DJUSBReadInput(input, kChunkFrames, kChannels);
            if (got == 0) {
                emptyReads++;
                SleepMilliseconds(2);
                continue;
            }
            for (uint32_t frame = 0; frame < got; frame++) {
                const float *src = &input[(size_t)frame * kChannels];
                for (uint32_t pair = 0; pair < kPairs; pair++) {
                    AddFrame(&stats[pair], src[pair * 2], src[pair * 2 + 1]);
                }
            }
            readFrames += got;
        }

        OpenA8DJUSBStop();

        printf("usb-input-meter seconds=%d rate=%.0f frames=%llu emptyReads=%llu\n",
               seconds,
               sampleRate,
               readFrames,
               emptyReads);
        PrintPair('A', &stats[0]);
        PrintPair('B', &stats[1]);
        PrintPair('C', &stats[2]);
        PrintPair('D', &stats[3]);
    }
    return 0;
}
