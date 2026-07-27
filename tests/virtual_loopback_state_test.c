#include "OpenA8DJVirtualLoopback.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
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

typedef struct StressContext {
    OpenA8DJVirtualLoopback *state;
    atomic_bool failed;
    atomic_uint_fast64_t expectedPublished;
} StressContext;

typedef struct ReaderContext {
    StressContext *stress;
    uint32_t clientID;
} ReaderContext;

static void *StressWriter(void *opaque)
{
    StressContext *context = opaque;
    float source[16 * 8];
    for (uint32_t frame = 0; frame < 16; ++frame) {
        for (uint32_t channel = 0; channel < 8; ++channel) {
            source[frame * 8 + channel] = (float)(channel + 1);
        }
    }
    for (unsigned iteration = 0; iteration < 5000; ++iteration) {
        uint32_t published = OpenA8DJVirtualLoopbackPublish8(
            context->state, source, 16);
        atomic_fetch_add(&context->expectedPublished, published);
    }
    return NULL;
}

static void *StressMutator(void *opaque)
{
    StressContext *context = opaque;
    for (unsigned iteration = 0; iteration < 1000; ++iteration) {
        (void)OpenA8DJVirtualLoopbackSet(
            context->state, true,
            (OpenA8DJLoopbackSourcePair)(iteration & 3));
        if ((iteration % 17) == 0) {
            OpenA8DJVirtualLoopbackResetContent(context->state);
        }
    }
    return NULL;
}

static void *StressReader(void *opaque)
{
    ReaderContext *reader = opaque;
    float output[8 * 2];
    for (unsigned iteration = 0; iteration < 5000; ++iteration) {
        (void)OpenA8DJVirtualLoopbackRead(
            reader->stress->state, reader->clientID, output, 8);
        for (unsigned frame = 0; frame < 8; ++frame) {
            float left = output[frame * 2];
            float right = output[frame * 2 + 1];
            bool zero = left == 0.0f && right == 0.0f;
            bool validPair =
                (left == 1.0f && right == 2.0f) ||
                (left == 3.0f && right == 4.0f) ||
                (left == 5.0f && right == 6.0f) ||
                (left == 7.0f && right == 8.0f);
            if (!zero && !validPair) {
                atomic_store(&reader->stress->failed, true);
            }
        }
    }
    return NULL;
}

static void RunStressTest(void)
{
    OpenA8DJVirtualLoopback *state = calloc(1, sizeof(*state));
    assert(state != NULL);
    OpenA8DJVirtualLoopbackInitialize(state);
    assert(OpenA8DJVirtualLoopbackSet(
        state, true, kOpenA8DJLoopbackSourcePairA));
    OpenA8DJVirtualLoopbackSetPhysicalPublishing(state, true);
    assert(OpenA8DJVirtualLoopbackRegisterClient(state, 101));
    assert(OpenA8DJVirtualLoopbackRegisterClient(state, 102));
    StressContext stress = {
        .state = state,
        .failed = ATOMIC_VAR_INIT(false),
        .expectedPublished = ATOMIC_VAR_INIT(0)
    };
    ReaderContext readers[] = {
        {.stress = &stress, .clientID = 101},
        {.stress = &stress, .clientID = 102}
    };
    pthread_t writer;
    pthread_t mutator;
    pthread_t readerThreads[2];
    assert(pthread_create(&writer, NULL, StressWriter, &stress) == 0);
    assert(pthread_create(&mutator, NULL, StressMutator, &stress) == 0);
    assert(pthread_create(&readerThreads[0], NULL, StressReader,
                          &readers[0]) == 0);
    assert(pthread_create(&readerThreads[1], NULL, StressReader,
                          &readers[1]) == 0);
    assert(pthread_join(writer, NULL) == 0);
    assert(pthread_join(mutator, NULL) == 0);
    assert(pthread_join(readerThreads[0], NULL) == 0);
    assert(pthread_join(readerThreads[1], NULL) == 0);
    assert(!atomic_load(&stress.failed));
    OpenA8DJLoopbackStatePayload snapshot;
    OpenA8DJVirtualLoopbackSnapshot(state, &snapshot);
    assert(snapshot.sourceFramesPublished ==
           atomic_load(&stress.expectedPublished));
    assert(snapshot.framesDelivered + snapshot.silenceFrames ==
           2ull * 5000ull * 8ull);
    free(state);
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
    uint64_t generation = snapshot.generation;

    float source[8 * 8];
    float output[8 * 2];
    Fill8(source, 8, 100.0f);
    memset(output, 1, sizeof(output));
    assert(OpenA8DJVirtualLoopbackPublish8(state, source, 8) == 0);
    OpenA8DJVirtualLoopbackSnapshot(state, &snapshot);
    assert(snapshot.sourceFramesPublished == 0);
    assert(OpenA8DJVirtualLoopbackRegisterClient(state, 41));
    assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 8) == 0);
    ExpectZero(output, 8);

    assert(OpenA8DJVirtualLoopbackSet(state, true,
                                     kOpenA8DJLoopbackSourcePairA));
    OpenA8DJVirtualLoopbackSnapshot(state, &snapshot);
    assert(snapshot.generation == generation + 1);
    generation = snapshot.generation;
    OpenA8DJVirtualLoopbackSetPhysicalPublishing(state, true);
    OpenA8DJVirtualLoopbackSnapshot(state, &snapshot);
    assert(snapshot.generation == generation + 1);
    generation = snapshot.generation;
    assert(OpenA8DJVirtualLoopbackPublish8(state, source, 8) == 8);
    assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 8) == 8);
    for (uint32_t f = 0; f < 8; ++f) {
        assert(output[f * 2] == source[f * 8]);
        assert(output[f * 2 + 1] == source[f * 8 + 1]);
    }

    assert(OpenA8DJVirtualLoopbackRegisterClient(state, 42));
    assert(OpenA8DJVirtualLoopbackRegisterClient(state, 42));
    OpenA8DJVirtualLoopbackSnapshot(state, &snapshot);
    assert(snapshot.registeredReaderCount == 2);
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

    Fill8(source, 2, 1500.0f);
    assert(OpenA8DJVirtualLoopbackPublish8(state, source, 2) == 2);
    memset(output, 1, sizeof(output));
    assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 4) == 2);
    assert(output[0] == source[6] && output[1] == source[7]);
    assert(output[4] == 0.0f && output[7] == 0.0f);

    atomic_store(&state->writeHead, kOpenA8DJLoopbackRingCapacity - 2);
    for (size_t i = 0; i < kOpenA8DJLoopbackMaxClients; ++i) {
        if (atomic_load(&state->clients[i].clientID) == 41) {
            atomic_store(&state->clients[i].cursor,
                         kOpenA8DJLoopbackRingCapacity - 2);
        }
    }
    Fill8(source, 4, 1700.0f);
    assert(OpenA8DJVirtualLoopbackPublish8(state, source, 4) == 4);
    assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 4) == 4);
    assert(output[0] == source[6] && output[6] == source[30]);

    atomic_store(&state->writeHead, UINT64_MAX - 2);
    for (size_t i = 0; i < kOpenA8DJLoopbackMaxClients; ++i) {
        if (atomic_load(&state->clients[i].clientID) != 0) {
            atomic_store(&state->clients[i].cursor, UINT64_MAX - 2);
        }
    }
    OpenA8DJVirtualLoopbackSnapshot(state, &snapshot);
    generation = snapshot.generation;
    Fill8(source, 4, 1800.0f);
    assert(OpenA8DJVirtualLoopbackPublish8(state, source, 4) == 4);
    OpenA8DJVirtualLoopbackSnapshot(state, &snapshot);
    assert(snapshot.generation == generation + 1);
    assert(atomic_load(&state->writeHead) == 4);
    assert(OpenA8DJVirtualLoopbackRead(state, 41, output, 4) == 4);
    assert(output[0] == source[6] && output[6] == source[30]);

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
    assert(snapshot.registeredReaderCount == 1);
    OpenA8DJVirtualLoopbackUnregisterClient(state, 42);
    memset(output, 1, sizeof(output));
    assert(OpenA8DJVirtualLoopbackRead(state, 9999, output, 2) == 0);
    ExpectZero(output, 2);
    assert(!OpenA8DJVirtualLoopbackRegisterClient(state, 0));
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
    request.reserved[0] = 0;
    request.sourcePair = 4;
    assert(!OpenA8DJVirtualLoopbackValidateSetRequest(&request, sizeof(request)));
    request.sourcePair = 3;
    request.schemaVersion = 2;
    assert(!OpenA8DJVirtualLoopbackValidateSetRequest(&request, sizeof(request)));
    request.schemaVersion = 1;
    assert(!OpenA8DJVirtualLoopbackValidateSetRequest(
        &request, sizeof(request) - 1));

    free(large);
    free(state);
    RunStressTest();
    puts("virtual loopback state tests: PASS");
    return 0;
}
