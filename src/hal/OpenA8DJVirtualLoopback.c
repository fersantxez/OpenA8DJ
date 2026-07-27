#include "OpenA8DJVirtualLoopback.h"

#include <limits.h>
#include <string.h>

static uint32_t FloatBits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float BitsFloat(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void SaturatingAdd(atomic_uint_fast64_t *value, uint64_t amount)
{
    uint64_t current = atomic_load_explicit(value, memory_order_relaxed);
    while (current != UINT64_MAX) {
        uint64_t next = amount > UINT64_MAX - current ?
            UINT64_MAX : current + amount;
        if (atomic_compare_exchange_weak_explicit(value, &current, next,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed)) {
            return;
        }
    }
}

static uint64_t AdvanceGeneration(OpenA8DJVirtualLoopback *state)
{
    uint64_t previous = atomic_fetch_add_explicit(&state->generation, 1,
                                                  memory_order_acq_rel);
    if (previous == UINT64_MAX) {
        atomic_store_explicit(&state->generation, 1, memory_order_release);
        return 1;
    }
    return previous + 1;
}

static void ResetReadersAtHead(OpenA8DJVirtualLoopback *state,
                               uint64_t generation,
                               uint64_t head)
{
    for (size_t i = 0; i < kOpenA8DJLoopbackMaxClients; ++i) {
        if (atomic_load_explicit(&state->clients[i].clientID,
                                 memory_order_acquire) != 0) {
            atomic_store_explicit(&state->clients[i].cursor, head,
                                  memory_order_relaxed);
            atomic_store_explicit(&state->clients[i].generation, generation,
                                  memory_order_release);
        }
    }
}

void OpenA8DJVirtualLoopbackInitialize(OpenA8DJVirtualLoopback *state)
{
    memset(state, 0, sizeof(*state));
    atomic_init(&state->enabled, false);
    atomic_init(&state->sourcePair, kOpenA8DJLoopbackSourcePairA);
    atomic_init(&state->physicalPlaybackPublishing, false);
    atomic_init(&state->generation, 1);
    atomic_init(&state->writeHead, 0);
    atomic_init(&state->registeredReaderCount, 0);
    atomic_init(&state->sourceFramesPublished, 0);
    atomic_init(&state->framesDelivered, 0);
    atomic_init(&state->silenceFrames, 0);
    atomic_init(&state->gapFrames, 0);
    atomic_init(&state->overrunEvents, 0);
    atomic_init(&state->overrunFrames, 0);
    for (size_t i = 0; i < kOpenA8DJLoopbackMaxClients; ++i) {
        atomic_init(&state->clients[i].clientID, 0);
        atomic_init(&state->clients[i].generation, 1);
        atomic_init(&state->clients[i].cursor, 0);
    }
    for (size_t i = 0; i < kOpenA8DJLoopbackRingCapacity; ++i) {
        atomic_init(&state->ring[i].leftBits, 0);
        atomic_init(&state->ring[i].rightBits, 0);
        atomic_init(&state->ring[i].token, 0);
    }
}

bool OpenA8DJVirtualLoopbackValidateSetRequest(
    const OpenA8DJLoopbackSetRequest *request,
    size_t requestLength)
{
    return request != NULL &&
           requestLength == sizeof(*request) &&
           request->schemaVersion == kOpenA8DJLoopbackSchemaVersion &&
           request->enabled <= 1 &&
           request->sourcePair <= kOpenA8DJLoopbackSourcePairD &&
           request->reserved[0] == 0 &&
           request->reserved[1] == 0;
}

void OpenA8DJVirtualLoopbackResetContent(OpenA8DJVirtualLoopback *state)
{
    uint64_t head = atomic_load_explicit(&state->writeHead,
                                         memory_order_acquire);
    uint64_t generation = AdvanceGeneration(state);
    ResetReadersAtHead(state, generation, head);
}

bool OpenA8DJVirtualLoopbackSet(OpenA8DJVirtualLoopback *state,
                               bool enabled,
                               OpenA8DJLoopbackSourcePair sourcePair)
{
    if ((unsigned)sourcePair > kOpenA8DJLoopbackSourcePairD) {
        return false;
    }
    bool oldEnabled = atomic_load_explicit(&state->enabled,
                                           memory_order_acquire);
    unsigned char oldPair = atomic_load_explicit(&state->sourcePair,
                                                  memory_order_acquire);
    if (oldEnabled == enabled && oldPair == (unsigned char)sourcePair) {
        return true;
    }
    atomic_store_explicit(&state->enabled, false, memory_order_release);
    atomic_store_explicit(&state->sourcePair, (unsigned char)sourcePair,
                          memory_order_release);
    OpenA8DJVirtualLoopbackResetContent(state);
    atomic_store_explicit(&state->enabled, enabled, memory_order_release);
    return true;
}

void OpenA8DJVirtualLoopbackSetPhysicalPublishing(
    OpenA8DJVirtualLoopback *state,
    bool publishing)
{
    bool old = atomic_exchange_explicit(&state->physicalPlaybackPublishing,
                                        publishing, memory_order_acq_rel);
    if (old != publishing) {
        OpenA8DJVirtualLoopbackResetContent(state);
    }
}

bool OpenA8DJVirtualLoopbackRegisterClient(OpenA8DJVirtualLoopback *state,
                                          uint32_t clientID)
{
    if (clientID == 0) {
        return false;
    }
    for (size_t i = 0; i < kOpenA8DJLoopbackMaxClients; ++i) {
        if (atomic_load_explicit(&state->clients[i].clientID,
                                 memory_order_acquire) == clientID) {
            return true;
        }
    }
    for (size_t i = 0; i < kOpenA8DJLoopbackMaxClients; ++i) {
        uint_fast32_t expected = 0;
        if (atomic_compare_exchange_strong_explicit(
                &state->clients[i].clientID, &expected, UINT32_MAX,
                memory_order_acq_rel, memory_order_relaxed)) {
            uint64_t head = atomic_load_explicit(&state->writeHead,
                                                 memory_order_acquire);
            uint64_t generation = atomic_load_explicit(&state->generation,
                                                       memory_order_acquire);
            atomic_store_explicit(&state->clients[i].cursor, head,
                                  memory_order_relaxed);
            atomic_store_explicit(&state->clients[i].generation, generation,
                                  memory_order_relaxed);
            atomic_store_explicit(&state->clients[i].clientID, clientID,
                                  memory_order_release);
            atomic_fetch_add_explicit(&state->registeredReaderCount, 1,
                                      memory_order_relaxed);
            return true;
        }
    }
    return false;
}

void OpenA8DJVirtualLoopbackUnregisterClient(OpenA8DJVirtualLoopback *state,
                                             uint32_t clientID)
{
    if (clientID == 0) {
        return;
    }
    for (size_t i = 0; i < kOpenA8DJLoopbackMaxClients; ++i) {
        uint_fast32_t expected = clientID;
        if (atomic_compare_exchange_strong_explicit(
                &state->clients[i].clientID, &expected, 0,
                memory_order_acq_rel, memory_order_relaxed)) {
            atomic_fetch_sub_explicit(&state->registeredReaderCount, 1,
                                      memory_order_relaxed);
            return;
        }
    }
}

uint32_t OpenA8DJVirtualLoopbackPublish8(
    OpenA8DJVirtualLoopback *state,
    const float *interleavedEightChannels,
    uint32_t frameCount)
{
    if (!atomic_load_explicit(&state->enabled, memory_order_acquire)) {
        return 0;
    }
    if (interleavedEightChannels == NULL || frameCount == 0 ||
        !atomic_load_explicit(&state->physicalPlaybackPublishing,
                              memory_order_acquire)) {
        return 0;
    }
    uint64_t generation = atomic_load_explicit(&state->generation,
                                               memory_order_acquire);
    uint64_t head = atomic_load_explicit(&state->writeHead,
                                         memory_order_relaxed);
    if (frameCount > kOpenA8DJLoopbackRingCapacity ||
        head > UINT64_MAX - frameCount - 1) {
        OpenA8DJVirtualLoopbackResetContent(state);
        head = atomic_load_explicit(&state->writeHead, memory_order_relaxed);
        if (frameCount > kOpenA8DJLoopbackRingCapacity) {
            interleavedEightChannels +=
                (size_t)(frameCount - kOpenA8DJLoopbackRingCapacity) *
                kOpenA8DJLoopbackPhysicalChannelCount;
            frameCount = kOpenA8DJLoopbackRingCapacity;
        }
        generation = atomic_load_explicit(&state->generation,
                                          memory_order_acquire);
    }
    unsigned pair = atomic_load_explicit(&state->sourcePair,
                                         memory_order_acquire);
    unsigned channel = pair * 2;
    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        uint64_t sequence = head + frame;
        OpenA8DJLoopbackSlot *slot =
            &state->ring[sequence & (kOpenA8DJLoopbackRingCapacity - 1)];
        const float *source = interleavedEightChannels +
            (size_t)frame * kOpenA8DJLoopbackPhysicalChannelCount + channel;
        atomic_store_explicit(&slot->leftBits, FloatBits(source[0]),
                              memory_order_relaxed);
        atomic_store_explicit(&slot->rightBits, FloatBits(source[1]),
                              memory_order_relaxed);
        atomic_store_explicit(&slot->token, sequence + 1,
                              memory_order_release);
    }
    if (generation != atomic_load_explicit(&state->generation,
                                            memory_order_acquire) ||
        !atomic_load_explicit(&state->enabled, memory_order_acquire) ||
        pair != atomic_load_explicit(&state->sourcePair,
                                     memory_order_acquire) ||
        !atomic_load_explicit(&state->physicalPlaybackPublishing,
                              memory_order_acquire)) {
        return 0;
    }
    atomic_store_explicit(&state->writeHead, head + frameCount,
                          memory_order_release);
    SaturatingAdd(&state->sourceFramesPublished, frameCount);
    return frameCount;
}

static OpenA8DJLoopbackClient *FindClient(OpenA8DJVirtualLoopback *state,
                                         uint32_t clientID)
{
    if (clientID == 0) {
        return NULL;
    }
    for (size_t i = 0; i < kOpenA8DJLoopbackMaxClients; ++i) {
        if (atomic_load_explicit(&state->clients[i].clientID,
                                 memory_order_acquire) == clientID) {
            return &state->clients[i];
        }
    }
    return NULL;
}

uint32_t OpenA8DJVirtualLoopbackRead(OpenA8DJVirtualLoopback *state,
                                    uint32_t clientID,
                                    float *outInterleavedStereo,
                                    uint32_t frameCount)
{
    if (outInterleavedStereo == NULL) {
        return 0;
    }
    memset(outInterleavedStereo, 0,
           (size_t)frameCount * kOpenA8DJLoopbackChannelCount * sizeof(float));
    OpenA8DJLoopbackClient *client = FindClient(state, clientID);
    if (client == NULL || frameCount == 0 ||
        !atomic_load_explicit(&state->enabled, memory_order_acquire) ||
        !atomic_load_explicit(&state->physicalPlaybackPublishing,
                              memory_order_acquire)) {
        SaturatingAdd(&state->silenceFrames, frameCount);
        return 0;
    }
    uint64_t generation = atomic_load_explicit(&state->generation,
                                               memory_order_acquire);
    uint64_t clientGeneration = atomic_load_explicit(&client->generation,
                                                     memory_order_acquire);
    uint64_t head = atomic_load_explicit(&state->writeHead,
                                         memory_order_acquire);
    if (clientGeneration != generation) {
        atomic_store_explicit(&client->cursor, head, memory_order_relaxed);
        atomic_store_explicit(&client->generation, generation,
                              memory_order_release);
        SaturatingAdd(&state->silenceFrames, frameCount);
        return 0;
    }

    uint64_t cursor = atomic_load_explicit(&client->cursor,
                                           memory_order_relaxed);
    if (head < cursor) {
        atomic_store_explicit(&client->cursor, head, memory_order_release);
        SaturatingAdd(&state->gapFrames, frameCount);
        SaturatingAdd(&state->silenceFrames, frameCount);
        return 0;
    }
    uint64_t available = head - cursor;
    if (available > kOpenA8DJLoopbackRingCapacity) {
        uint64_t lost = available - kOpenA8DJLoopbackRingCapacity;
        atomic_store_explicit(&client->cursor, head, memory_order_release);
        SaturatingAdd(&state->overrunEvents, 1);
        SaturatingAdd(&state->overrunFrames, lost);
        SaturatingAdd(&state->gapFrames, lost);
        SaturatingAdd(&state->silenceFrames, frameCount);
        return 0;
    }
    uint32_t reserved = available < frameCount ?
        (uint32_t)available : frameCount;
    if (reserved == 0) {
        SaturatingAdd(&state->silenceFrames, frameCount);
        return 0;
    }
    uint64_t expected = cursor;
    if (!atomic_compare_exchange_strong_explicit(
            &client->cursor, &expected, cursor + reserved,
            memory_order_acq_rel, memory_order_relaxed)) {
        SaturatingAdd(&state->silenceFrames, frameCount);
        SaturatingAdd(&state->gapFrames, reserved);
        return 0;
    }

    uint32_t delivered = 0;
    for (uint32_t frame = 0; frame < reserved; ++frame) {
        uint64_t sequence = cursor + frame;
        OpenA8DJLoopbackSlot *slot =
            &state->ring[sequence & (kOpenA8DJLoopbackRingCapacity - 1)];
        uint64_t expectedToken = sequence + 1;
        uint64_t tokenBefore = atomic_load_explicit(&slot->token,
                                                    memory_order_acquire);
        uint32_t left = atomic_load_explicit(&slot->leftBits,
                                             memory_order_relaxed);
        uint32_t right = atomic_load_explicit(&slot->rightBits,
                                              memory_order_relaxed);
        uint64_t tokenAfter = atomic_load_explicit(&slot->token,
                                                   memory_order_acquire);
        if (tokenBefore == expectedToken && tokenAfter == expectedToken &&
            generation == atomic_load_explicit(&state->generation,
                                                memory_order_acquire)) {
            outInterleavedStereo[(size_t)frame * 2] = BitsFloat(left);
            outInterleavedStereo[(size_t)frame * 2 + 1] = BitsFloat(right);
            delivered++;
        } else {
            SaturatingAdd(&state->gapFrames, 1);
        }
    }
    SaturatingAdd(&state->framesDelivered, delivered);
    SaturatingAdd(&state->silenceFrames, frameCount - delivered);
    return delivered;
}

void OpenA8DJVirtualLoopbackSnapshot(
    const OpenA8DJVirtualLoopback *state,
    OpenA8DJLoopbackStatePayload *outPayload)
{
    memset(outPayload, 0, sizeof(*outPayload));
    outPayload->schemaVersion = kOpenA8DJLoopbackSchemaVersion;
    outPayload->enabled = atomic_load_explicit(&state->enabled,
                                               memory_order_acquire) ? 1 : 0;
    outPayload->sourcePair = atomic_load_explicit(&state->sourcePair,
                                                  memory_order_acquire);
    outPayload->sessionOnly = 1;
    outPayload->physicalPlaybackPublishing =
        atomic_load_explicit(&state->physicalPlaybackPublishing,
                             memory_order_acquire) ? 1 : 0;
    outPayload->ringCapacity = kOpenA8DJLoopbackRingCapacity;
    outPayload->registeredReaderCount =
        atomic_load_explicit(&state->registeredReaderCount,
                             memory_order_relaxed);
    outPayload->generation = atomic_load_explicit(&state->generation,
                                                  memory_order_acquire);
    outPayload->sourceFramesPublished =
        atomic_load_explicit(&state->sourceFramesPublished,
                             memory_order_relaxed);
    outPayload->framesDelivered =
        atomic_load_explicit(&state->framesDelivered, memory_order_relaxed);
    outPayload->silenceFrames =
        atomic_load_explicit(&state->silenceFrames, memory_order_relaxed);
    outPayload->gapFrames =
        atomic_load_explicit(&state->gapFrames, memory_order_relaxed);
    outPayload->overrunEvents =
        atomic_load_explicit(&state->overrunEvents, memory_order_relaxed);
    outPayload->overrunFrames =
        atomic_load_explicit(&state->overrunFrames, memory_order_relaxed);
}
