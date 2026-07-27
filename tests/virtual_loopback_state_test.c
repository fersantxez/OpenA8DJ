#include "OpenA8DJVirtualLoopback.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Fill8(float *buffer, uint32_t frames, float base)
{
    for (uint32_t f = 0; f < frames; ++f) {
        for (uint32_t c = 0; c < 8; ++c) {
            buffer[(size_t)f * 8 + c] = base + (float)(f * 10 + c);
        }
    }
}

static void ExpectZero(const float *buffer, uint32_t frames)
{
    for (uint32_t i = 0; i < frames * 2; ++i) assert(buffer[i] == 0.0f);
}

int main(void)
{
    OpenA8DJVirtualLoopback *state = calloc(1, sizeof(*state));
    assert(state != NULL);
    OpenA8DJVirtualLoopbackInitialize(state);
    OpenA8DJLoopbackStatePayload snapshot;
    OpenA8DJVirtualLoopbackSnapshot(state, &snapshot);
    assert(snapshot.enabled == 0 && snapshot.sourcePair == 0);
    assert(snapshot.sessionOnly == 1 && snapshot.generation != 0);

    float source[8 * 8];
    float output[8 * 2];
    Fill8(source, 8, 100.0f);
    memset(output, 1, sizeof(output));
    assert(OpenA8DJVirtualLoopbackPublish8(state, source, 8) == 0);
    assert(OpenA8DJVirtualLoopbackRegisterClient(state, 41));
    assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 8) == 0);
    ExpectZero(output, 8);

    assert(OpenA8DJVirtualLoopbackSet(state, true,
                                     kOpenA8DJLoopbackSourcePairA));
    OpenA8DJVirtualLoopbackSetPhysicalPublishing(state, true);
    assert(OpenA8DJVirtualLoopbackPublish8(state, source, 8) == 8);
    assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 8) == 8);
    for (uint32_t f = 0; f < 8; ++f) {
        assert(output[f * 2] == source[f * 8]);
        assert(output[f * 2 + 1] == source[f * 8 + 1]);
    }

    assert(OpenA8DJVirtualLoopbackRegisterClient(state, 42));
    Fill8(source, 8, 500.0f);
    assert(OpenA8DJVirtualLoopbackPublish8(state, source, 8) == 8);
    assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 3) == 3);
    assert(output[0] == 500.0f && output[4] == 520.0f);
    assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 5) == 5);
    assert(output[0] == 530.0f);
    assert(OpenA8DJVirtualLoopbackRead(state, 42, output, 8) == 8);
    assert(output[0] == 500.0f);

    for (unsigned pair = 0; pair < 4; ++pair) {
        assert(OpenA8DJVirtualLoopbackSet(
            state, true, (OpenA8DJLoopbackSourcePair)pair));
        Fill8(source, 2, 1000.0f + (float)pair * 100.0f);
        assert(OpenA8DJVirtualLoopbackPublish8(state, source, 2) == 2);
        assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 2) == 2);
        assert(output[0] == source[pair * 2]);
        assert(output[1] == source[pair * 2 + 1]);
    }

    Fill8(source, 4, 2000.0f);
    assert(OpenA8DJVirtualLoopbackPublish8(state, source, 4) == 4);
    assert(OpenA8DJVirtualLoopbackSet(state, false,
                                     kOpenA8DJLoopbackSourcePairD));
    assert(OpenA8DJVirtualLoopbackSet(state, true,
                                     kOpenA8DJLoopbackSourcePairD));
    memset(output, 1, sizeof(output));
    assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 4) == 0);
    ExpectZero(output, 4);

    float *large = malloc((size_t)kOpenA8DJLoopbackRingCapacity * 8 *
                          sizeof(float));
    assert(large != NULL);
    Fill8(large, kOpenA8DJLoopbackRingCapacity, 3000.0f);
    assert(OpenA8DJVirtualLoopbackPublish8(
        state, large, kOpenA8DJLoopbackRingCapacity) ==
        kOpenA8DJLoopbackRingCapacity);
    Fill8(source, 1, 4000.0f);
    assert(OpenA8DJVirtualLoopbackPublish8(state, source, 1) == 1);
    memset(output, 1, sizeof(output));
    assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 8) == 0);
    ExpectZero(output, 8);
    OpenA8DJVirtualLoopbackSnapshot(state, &snapshot);
    assert(snapshot.overrunEvents == 1);
    assert(snapshot.overrunFrames == 1);

    OpenA8DJVirtualLoopbackSetPhysicalPublishing(state, false);
    memset(output, 1, sizeof(output));
    assert(OpenA8DJVirtualLoopbackRead(state, 42, output, 8) == 0);
    ExpectZero(output, 8);
    OpenA8DJVirtualLoopbackUnregisterClient(state, 41);
    OpenA8DJVirtualLoopbackUnregisterClient(state, 42);
    OpenA8DJVirtualLoopbackSnapshot(state, &snapshot);
    assert(snapshot.registeredReaderCount == 0);

    OpenA8DJLoopbackSetRequest request = {
        .schemaVersion = 1, .enabled = 1, .sourcePair = 3, .reserved = {0, 0}
    };
    assert(OpenA8DJVirtualLoopbackValidateSetRequest(&request, sizeof(request)));
    request.enabled = 2;
    assert(!OpenA8DJVirtualLoopbackValidateSetRequest(&request, sizeof(request)));
    request.enabled = 1;
    request.reserved[0] = 1;
    assert(!OpenA8DJVirtualLoopbackValidateSetRequest(&request, sizeof(request)));

    free(large);
    free(state);
    puts("virtual loopback state tests: PASS");
    return 0;
}
