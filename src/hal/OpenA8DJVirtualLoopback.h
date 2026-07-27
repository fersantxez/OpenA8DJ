#ifndef OPENA8DJ_VIRTUAL_LOOPBACK_H
#define OPENA8DJ_VIRTUAL_LOOPBACK_H

#include <stdbool.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

enum {
    kOpenA8DJLoopbackSchemaVersion = 1,
    kOpenA8DJLoopbackChannelCount = 2,
    kOpenA8DJLoopbackPhysicalChannelCount = 8,
    kOpenA8DJLoopbackRingCapacity = 32768,
    kOpenA8DJLoopbackMaxClients = 32
};

typedef enum OpenA8DJLoopbackSourcePair {
    kOpenA8DJLoopbackSourcePairA = 0,
    kOpenA8DJLoopbackSourcePairB = 1,
    kOpenA8DJLoopbackSourcePairC = 2,
    kOpenA8DJLoopbackSourcePairD = 3
} OpenA8DJLoopbackSourcePair;

typedef struct OpenA8DJLoopbackSetRequest {
    uint32_t schemaVersion;
    uint8_t enabled;
    uint8_t sourcePair;
    uint8_t reserved[2];
} __attribute__((packed)) OpenA8DJLoopbackSetRequest;

typedef struct OpenA8DJLoopbackStatePayload {
    uint32_t schemaVersion;
    uint8_t enabled;
    uint8_t sourcePair;
    uint8_t sessionOnly;
    uint8_t physicalPlaybackPublishing;
    uint32_t ringCapacity;
    uint32_t registeredReaderCount;
    uint64_t generation;
    uint64_t sourceFramesPublished;
    uint64_t framesDelivered;
    uint64_t silenceFrames;
    uint64_t gapFrames;
    uint64_t overrunEvents;
    uint64_t overrunFrames;
} __attribute__((packed)) OpenA8DJLoopbackStatePayload;

typedef struct OpenA8DJLoopbackSlot {
    atomic_uint_least32_t leftBits;
    atomic_uint_least32_t rightBits;
    atomic_uint_fast64_t token;
} OpenA8DJLoopbackSlot;

typedef struct OpenA8DJLoopbackClient {
    atomic_uint_fast32_t clientID;
    atomic_uint_fast64_t generation;
    atomic_uint_fast64_t cursor;
} OpenA8DJLoopbackClient;

typedef struct OpenA8DJVirtualLoopback {
    atomic_bool enabled;
    atomic_uchar sourcePair;
    atomic_bool physicalPlaybackPublishing;
    atomic_uint_fast64_t generation;
    atomic_uint_fast64_t writeHead;
    atomic_uint registeredReaderCount;
    atomic_uint_fast64_t sourceFramesPublished;
    atomic_uint_fast64_t framesDelivered;
    atomic_uint_fast64_t silenceFrames;
    atomic_uint_fast64_t gapFrames;
    atomic_uint_fast64_t overrunEvents;
    atomic_uint_fast64_t overrunFrames;
    OpenA8DJLoopbackClient clients[kOpenA8DJLoopbackMaxClients];
    OpenA8DJLoopbackSlot ring[kOpenA8DJLoopbackRingCapacity];
} OpenA8DJVirtualLoopback;

extern OpenA8DJVirtualLoopback gOpenA8DJVirtualLoopback;

/*
 * Reader lag rule: if a client has fallen behind by more than ring capacity,
 * that entire callback is silence. Its cursor advances to the current head;
 * the exact lost range is charged to gap/overrun counters. The next callback
 * resumes with newly published audio. Partial availability is delivered first
 * and the remainder of the requested buffer is silence.
 */
void OpenA8DJVirtualLoopbackInitialize(OpenA8DJVirtualLoopback *state);
bool OpenA8DJVirtualLoopbackValidateSetRequest(
    const OpenA8DJLoopbackSetRequest *request,
    size_t requestLength);
bool OpenA8DJVirtualLoopbackSet(OpenA8DJVirtualLoopback *state,
                               bool enabled,
                               OpenA8DJLoopbackSourcePair sourcePair);
void OpenA8DJVirtualLoopbackResetContent(OpenA8DJVirtualLoopback *state);
void OpenA8DJVirtualLoopbackSetPhysicalPublishing(
    OpenA8DJVirtualLoopback *state,
    bool publishing);
bool OpenA8DJVirtualLoopbackRegisterClient(OpenA8DJVirtualLoopback *state,
                                          uint32_t clientID);
void OpenA8DJVirtualLoopbackUnregisterClient(OpenA8DJVirtualLoopback *state,
                                             uint32_t clientID);
uint32_t OpenA8DJVirtualLoopbackPublish8(
    OpenA8DJVirtualLoopback *state,
    const float *interleavedEightChannels,
    uint32_t frameCount);
uint32_t OpenA8DJVirtualLoopbackRead(OpenA8DJVirtualLoopback *state,
                                    uint32_t clientID,
                                    float *outInterleavedStereo,
                                    uint32_t frameCount);
void OpenA8DJVirtualLoopbackSnapshot(
    const OpenA8DJVirtualLoopback *state,
    OpenA8DJLoopbackStatePayload *outPayload);

/* HAL-owned session singleton accessors used by the authenticated IPC server. */
bool OpenA8DJHALVirtualLoopbackApply(
    const OpenA8DJLoopbackSetRequest *request,
    size_t requestLength,
    OpenA8DJLoopbackStatePayload *outPayload);
void OpenA8DJHALVirtualLoopbackSnapshot(
    OpenA8DJLoopbackStatePayload *outPayload);

#endif
