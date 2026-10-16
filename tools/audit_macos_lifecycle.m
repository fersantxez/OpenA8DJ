// Offline fault injection into the production Objective-C transport.
// Never calls openUSB on the production class, starts audio, or connects to IPC.
#import <Foundation/Foundation.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#define OPENA8DJ_LIFECYCLE_AUDIT 1
// Suppress unlink of the live driver's IPC path. Test-only temporary socket
// fixtures are allowed to use the real unlink implementation.
static int AuditUnlink(const char *path) {
    const char *fixturePrefix = "/tmp/opena8dj-lifecycle-socket-test-";
    return strncmp(path, fixturePrefix, strlen(fixturePrefix)) == 0 ? unlink(path) : 0;
}
#define unlink AuditUnlink
#include "../src/hal/OpenA8DJUSB.m"
#undef unlink

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        abort(); \
    } \
} while (0)

@interface AuditDevice : NSObject
@property NSUInteger destroys;
- (void)destroy;
@end
@implementation AuditDevice
- (void)destroy { self.destroys++; }
@end

@interface AuditPartialOpenEngine : OpenA8DJUSBEngine
@property AuditDevice *fixtureDevice;
@end
@implementation AuditPartialOpenEngine
- (BOOL)openUSB {
    // Models configure/interface discovery failure after acquiring _device,
    // before openUSB sets _running. All other lifecycle code is production code.
    [self setValue:self.fixtureDevice forKey:@"device"];
    return NO;
}

static void AcceptStopAdmission(void) {
    OpenA8DJUSBEngine *engine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:48000];
    [engine setIPCAdmissionForAuditRunning:YES];
    int racing[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, racing) == 0);
    int acceptedPeer = racing[0];
    __block BOOL accepted = NO;
    __block uint64_t generation = 0;
    dispatch_semaphore_t gate = dispatch_semaphore_create(0);
    dispatch_group_t race = dispatch_group_create();
    dispatch_group_async(race, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        dispatch_semaphore_wait(gate, DISPATCH_TIME_FOREVER);
        accepted = [engine addAcceptedIPCClient:acceptedPeer generation:&generation];
    });
    dispatch_group_async(race, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        dispatch_semaphore_wait(gate, DISPATCH_TIME_FOREVER);
        [engine setIPCAdmissionForAuditRunning:NO];
        [engine stopIPCServer];
    });
    dispatch_semaphore_signal(gate);
    dispatch_semaphore_signal(gate);
    CHECK(dispatch_group_wait(race, dispatch_time(DISPATCH_TIME_NOW, 1000000000)) == 0);
    if (accepted) {
        [engine removeIPCClient:acceptedPeer generation:generation];
    }

    int after[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, after) == 0);
    uint64_t rejectedGeneration = 0;
    CHECK(![engine addAcceptedIPCClient:after[0] generation:&rejectedGeneration]);
    CHECK(rejectedGeneration == 0);
    close(after[1]);
    close(racing[1]);
    puts("ipc_accept_stop: racing accept linearized before or rejected after shutdown");
}

static void PartialFrameRead(void) {
    int sockets[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    struct { OpenA8DJIPCHeader header; uint8_t payload[5]; } frame = {
        .header = { .magic = kIPCMagic, .version = kIPCVersion,
                    .type = kIPCTypeControlSet, .length = 5 },
        .payload = { 1, 2, 3, 4, 5 }
    };
    int writerFD = sockets[0];
    dispatch_group_t writer = dispatch_group_create();
    dispatch_group_async(writer, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        const uint8_t *bytes = (const uint8_t *)&frame;
        for (size_t index = 0; index < sizeof(frame.header); index++) {
            CHECK(write(writerFD, bytes + index, 1) == 1);
        }
        CHECK(write(writerFD, frame.payload, 2) == 2);
        CHECK(write(writerFD, frame.payload + 2, 3) == 3);
        close(writerFD);
    });
    OpenA8DJIPCHeader header = {0};
    uint8_t payload[5] = {0};
    CHECK(ReadFull(sockets[1], &header, sizeof(header)));
    CHECK(header.magic == kIPCMagic && header.type == kIPCTypeControlSet && header.length == 5);
    CHECK(ReadFull(sockets[1], payload, sizeof(payload)));
    CHECK(memcmp(payload, frame.payload, sizeof(payload)) == 0);
    CHECK(dispatch_group_wait(writer, dispatch_time(DISPATCH_TIME_NOW, 1000000000)) == 0);
    uint8_t truncated[sizeof(header)] = {0};
    CHECK(!ReadFull(sockets[1], truncated, 1));
    close(sockets[1]);
    puts("ipc_partial_frames: fragmented header and payload reassembled; EOF handled");
}

static void SocketPathOwnership(void) {
    char directory[] = "/tmp/opena8dj-lifecycle-socket-test-XXXXXX";
    CHECK(mkdtemp(directory) != NULL);
    char currentPath[sizeof(((struct sockaddr_un *)0)->sun_path)];
    char movedPath[sizeof(((struct sockaddr_un *)0)->sun_path)];
    CHECK(snprintf(currentPath, sizeof(currentPath), "%s/control.sock", directory) > 0);
    CHECK(snprintf(movedPath, sizeof(movedPath), "%s/old.sock", directory) > 0);

    int oldSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(oldSocket >= 0);
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    strlcpy(address.sun_path, currentPath, sizeof(address.sun_path));
    CHECK(bind(oldSocket, (struct sockaddr *)&address, sizeof(address)) == 0);
    struct stat oldIdentity;
    CHECK(lstat(currentPath, &oldIdentity) == 0 && S_ISSOCK(oldIdentity.st_mode));
    CHECK(rename(currentPath, movedPath) == 0);

    int replacementSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(replacementSocket >= 0);
    CHECK(bind(replacementSocket, (struct sockaddr *)&address, sizeof(address)) == 0);
    struct stat replacementIdentity;
    CHECK(lstat(currentPath, &replacementIdentity) == 0 && S_ISSOCK(replacementIdentity.st_mode));
    CHECK(replacementIdentity.st_ino != oldIdentity.st_ino ||
          replacementIdentity.st_dev != oldIdentity.st_dev);

    CHECK(!UnlinkOwnedIPCSocketPath(currentPath, oldIdentity.st_dev, oldIdentity.st_ino));
    CHECK(lstat(currentPath, &replacementIdentity) == 0);
    CHECK(UnlinkOwnedIPCSocketPath(currentPath, replacementIdentity.st_dev,
                                   replacementIdentity.st_ino));
    CHECK(lstat(currentPath, &replacementIdentity) != 0 && errno == ENOENT);
    close(oldSocket);
    close(replacementSocket);
    CHECK(unlink(movedPath) == 0);
    CHECK(rmdir(directory) == 0);
    puts("ipc_socket_owner: stale listener cannot unlink replacement pathname");
}
@end

static void PartialOpen(void) {
    AuditPartialOpenEngine *engine = [[AuditPartialOpenEngine alloc] initWithSampleRate:48000];
    AuditDevice *device = [AuditDevice new];
    engine.fixtureDevice = device;
    BOOL opened = [engine ensureOpen];
    BOOL retained = [engine valueForKey:@"device"] == device;
    printf("partial_open: opened=%d resource_retained=%d destroy_calls=%lu\n",
           opened, retained, (unsigned long)device.destroys);
    CHECK(!opened && !retained && device.destroys == 1);
}

static void ClientCapacity(void) {
    OpenA8DJUSBEngine *engine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:48000];
    int sockets[17][2];
    uint64_t generations[16] = {0};
    for (size_t i = 0; i < 17; ++i) {
        int status = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets[i]);
        CHECK(status == 0);
        BOOL accepted = [engine addIPCClient:sockets[i][0]
                                   generation:(i < 16 ? &generations[i] : NULL)];
        CHECK(accepted == (i < 16));
    }
    for (size_t i = 0; i < 16; ++i) {
        [engine removeIPCClient:sockets[i][0] generation:generations[i]];
        close(sockets[i][1]);
    }
    close(sockets[16][1]);
}

static void SlowPeerIsolation(void) {
    OpenA8DJUSBEngine *engine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:48000];
    int slow[2], healthy[2];
    uint64_t slowGeneration = 0, healthyGeneration = 0;
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, slow) == 0);
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, healthy) == 0);
    int flags = fcntl(slow[0], F_GETFL);
    CHECK(flags >= 0);
    CHECK(fcntl(slow[0], F_SETFL, flags | O_NONBLOCK) == 0);
    CHECK([engine addIPCClient:slow[0] generation:&slowGeneration]);
    CHECK([engine addIPCClient:healthy[0] generation:&healthyGeneration]);
    flags = fcntl(slow[1], F_GETFL);
    CHECK(flags >= 0);
    CHECK(fcntl(slow[1], F_SETFL, flags | O_NONBLOCK) == 0);
    char fill[4096] = {0};
    while (write(slow[0], fill, sizeof(fill)) > 0) {}
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);

    const uint8_t payload[] = {0x91, 0x3c, 0x64};
    [engine broadcastIPCType:kIPCTypeMidiFromDevice bytes:payload length:sizeof(payload)];
    OpenA8DJIPCHeader header;
    CHECK(ReadFull(healthy[1], &header, sizeof(header)));
    CHECK(header.magic == kIPCMagic && header.version == kIPCVersion);
    CHECK(header.type == kIPCTypeMidiFromDevice && header.length == sizeof(payload));
    uint8_t received[sizeof(payload)] = {0};
    CHECK(ReadFull(healthy[1], received, sizeof(received)));
    CHECK(memcmp(received, payload, sizeof(payload)) == 0);
    ssize_t drained = 0;
    do {
        drained = recv(slow[1], fill, sizeof(fill), MSG_DONTWAIT);
    } while (drained > 0);
    CHECK(drained == 0); // shutdown delivered EOF after buffered peer data was drained
    [engine removeIPCClient:slow[0] generation:slowGeneration];
    [engine removeIPCClient:healthy[0] generation:healthyGeneration];
    close(slow[1]);
    close(healthy[1]);
}

static void SixteenSlowPeers(void) {
    OpenA8DJUSBEngine *engine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:48000];
    int peers[16][2];
    uint64_t generations[16] = {0};
    char fill[4096] = {0};
    for (size_t index = 0; index < 16; index++) {
        CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, peers[index]) == 0);
        int flags = fcntl(peers[index][0], F_GETFL);
        CHECK(flags >= 0 && fcntl(peers[index][0], F_SETFL, flags | O_NONBLOCK) == 0);
        CHECK([engine addIPCClient:peers[index][0] generation:&generations[index]]);
        while (write(peers[index][0], fill, sizeof(fill)) > 0) {}
        CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    }

    uint64_t started = mach_absolute_time();
    const uint8_t payload[] = {0x90, 0x40, 0x7f};
    [engine broadcastIPCType:kIPCTypeMidiFromDevice bytes:payload length:sizeof(payload)];
    uint64_t finished = mach_absolute_time();
    mach_timebase_info_data_t timebase;
    CHECK(mach_timebase_info(&timebase) == KERN_SUCCESS && timebase.denom != 0);
    uint64_t elapsedNanos = (finished - started) * timebase.numer / timebase.denom;
    printf("ipc_slow_peers16: broadcast_ns=%llu\n", (unsigned long long)elapsedNanos);
    CHECK(elapsedNanos < 250000000ULL);

    for (size_t index = 0; index < 16; index++) {
        [engine removeIPCClient:peers[index][0] generation:generations[index]];
        close(peers[index][1]);
    }
}

static void WriterSerialization(void) {
    OpenA8DJUSBEngine *engine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:48000];
    int sockets[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    int clientFD = sockets[0];
    uint64_t generation = 0;
    CHECK([engine addIPCClient:clientFD generation:&generation]);

    dispatch_semaphore_t ready = dispatch_semaphore_create(0);
    dispatch_semaphore_t start = dispatch_semaphore_create(0);
    dispatch_group_t writers = dispatch_group_create();
    const uint32_t messagesPerWriter = 64;
    for (uint32_t writer = 0; writer < 2; ++writer) {
        dispatch_group_async(writers, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
            dispatch_semaphore_signal(ready);
            dispatch_semaphore_wait(start, DISPATCH_TIME_FOREVER);
            uint8_t type = writer == 0 ? kIPCTypeMidiFromDevice : kIPCTypeMidiToDevice;
            for (uint32_t sequence = 0; sequence < messagesPerWriter; ++sequence) {
                struct { uint32_t writer; uint32_t sequence; } payload = { writer, sequence };
                CHECK([engine sendIPCType:type bytes:&payload length:(uint16_t)sizeof(payload)
                                  clientFD:clientFD]);
            }
        });
    }
    CHECK(dispatch_semaphore_wait(ready, dispatch_time(DISPATCH_TIME_NOW, 1000000000)) == 0);
    CHECK(dispatch_semaphore_wait(ready, dispatch_time(DISPATCH_TIME_NOW, 1000000000)) == 0);
    dispatch_semaphore_signal(start);
    dispatch_semaphore_signal(start);
    for (uint32_t index = 0; index < messagesPerWriter * 2; ++index) {
        OpenA8DJIPCHeader header;
        CHECK(ReadFull(sockets[1], &header, sizeof(header)));
        CHECK(header.magic == kIPCMagic && header.version == kIPCVersion);
        CHECK(header.length == sizeof(uint32_t) * 2);
        CHECK(header.type == kIPCTypeMidiFromDevice || header.type == kIPCTypeMidiToDevice);
        struct { uint32_t writer; uint32_t sequence; } payload;
        CHECK(ReadFull(sockets[1], &payload, sizeof(payload)));
        CHECK(payload.writer < 2 && payload.sequence < messagesPerWriter);
        CHECK((payload.writer == 0 && header.type == kIPCTypeMidiFromDevice) ||
              (payload.writer == 1 && header.type == kIPCTypeMidiToDevice));
    }
    CHECK(dispatch_group_wait(writers, dispatch_time(DISPATCH_TIME_NOW, 2000000000)) == 0);
    [engine removeIPCClient:clientFD generation:generation];
    close(sockets[1]);
}

static void DoubleClose(void) {
    OpenA8DJUSBEngine *engine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:48000];
    int sockets[2];
    int socketStatus = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    CHECK(socketStatus == 0);
    int stale = sockets[0];
    uint64_t staleGeneration = 0;
    CHECK([engine addIPCClient:stale generation:&staleGeneration]);
    [engine stopIPCServer];
    [engine removeIPCClient:stale generation:staleGeneration]; // old owner closes its generation
    int replacementSockets[2];
    socketStatus = socketpair(AF_UNIX, SOCK_STREAM, 0, replacementSockets);
    CHECK(socketStatus == 0);
    if (replacementSockets[0] != stale) {
        int duplicateStatus = dup2(replacementSockets[0], stale);
        CHECK(duplicateStatus == stale);
        close(replacementSockets[0]);
    }
    replacementSockets[0] = stale;
    uint64_t replacementGeneration = 0;
    CHECK([engine addIPCClient:stale generation:&replacementGeneration]);
    [engine removeIPCClient:stale generation:staleGeneration]; // stale cleanup cannot remove the new owner
    BOOL replacementAlive = fcntl(stale, F_GETFD) >= 0;
    printf("ipc_generation_cleanup: reused_descriptor_still_owned=%d\n", replacementAlive);
    CHECK(replacementAlive && replacementGeneration != staleGeneration);
    [engine removeIPCClient:stale generation:replacementGeneration];
    BOOL descriptorClosed = fcntl(stale, F_GETFD) == -1 && errno == EBADF;
    CHECK(descriptorClosed);
    close(sockets[1]);
    close(replacementSockets[1]);
}

static void Backpressure(void) {
    OpenA8DJUSBEngine *engine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:48000];
    int sockets[2];
    int socketStatus = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    CHECK(socketStatus == 0);
    uint64_t generation = 0;
    CHECK([engine addIPCClient:sockets[0] generation:&generation]);
    int flags = fcntl(sockets[0], F_GETFL);
    CHECK(flags >= 0);
    int setStatus = fcntl(sockets[0], F_SETFL, flags | O_NONBLOCK);
    CHECK(setStatus == 0);
    char bytes[4096] = {0};
    while (write(sockets[0], bytes, sizeof(bytes)) > 0) {}
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    setStatus = fcntl(sockets[0], F_SETFL, flags);
    CHECK(setStatus == 0);
    dispatch_semaphore_t ready = dispatch_semaphore_create(0);
    dispatch_semaphore_t start = dispatch_semaphore_create(0);
    dispatch_semaphore_t writerDone = dispatch_semaphore_create(0);
    dispatch_semaphore_t stopDone = dispatch_semaphore_create(0);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        dispatch_semaphore_signal(ready);
        dispatch_semaphore_wait(start, DISPATCH_TIME_FOREVER);
        [engine broadcastIPCType:kIPCTypeMidiFromDevice bytes:(const uint8_t *)"x" length:1];
        dispatch_semaphore_signal(writerDone);
    });
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        dispatch_semaphore_signal(ready);
        dispatch_semaphore_wait(start, DISPATCH_TIME_FOREVER);
        [engine stopIPCServer];
        dispatch_semaphore_signal(stopDone);
    });
    long readyOne = dispatch_semaphore_wait(ready, dispatch_time(DISPATCH_TIME_NOW, 1000000000));
    long readyTwo = dispatch_semaphore_wait(ready, dispatch_time(DISPATCH_TIME_NOW, 1000000000));
    CHECK(readyOne == 0 && readyTwo == 0);
    dispatch_semaphore_signal(start);
    dispatch_semaphore_signal(start);
    long writerFinished = dispatch_semaphore_wait(writerDone, dispatch_time(DISPATCH_TIME_NOW, 500000000));
    long stopFinished = dispatch_semaphore_wait(stopDone, dispatch_time(DISPATCH_TIME_NOW, 500000000));
    printf("ipc_backpressure: concurrent_writer_completed=%d stop_completed=%d\n",
           writerFinished == 0, stopFinished == 0);
    CHECK(writerFinished == 0 && stopFinished == 0);
    [engine removeIPCClient:sockets[0] generation:generation];
    close(sockets[1]);
}

static void SlowReply(void) {
    OpenA8DJUSBEngine *engine = [[OpenA8DJUSBEngine alloc] initWithSampleRate:48000];
    int sockets[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    uint64_t generation = 0;
    CHECK([engine addIPCClient:sockets[0] generation:&generation]);
    int flags = fcntl(sockets[0], F_GETFL);
    CHECK(flags >= 0 && fcntl(sockets[0], F_SETFL, flags | O_NONBLOCK) == 0);
    uint8_t bytes[4096] = {0};
    while (write(sockets[0], bytes, sizeof(bytes)) > 0) {}
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    CHECK(fcntl(sockets[0], F_SETFL, flags) == 0);

    uint64_t started = mach_absolute_time();
    uint32_t response = 0x12345678;
    BOOL sent = [engine sendIPCType:kIPCTypeControlState bytes:&response
                             length:(uint16_t)sizeof(response) clientFD:sockets[0]];
    uint64_t finished = mach_absolute_time();
    mach_timebase_info_data_t timebase;
    CHECK(mach_timebase_info(&timebase) == KERN_SUCCESS && timebase.denom != 0);
    uint64_t elapsedNanos = (finished - started) * timebase.numer / timebase.denom;
    printf("ipc_slow_reply: sent=%d elapsed_ns=%llu\n", sent,
           (unsigned long long)elapsedNanos);
    CHECK(!sent && elapsedNanos < 250000000ULL);
    [engine removeIPCClient:sockets[0] generation:generation];
    close(sockets[1]);
}

int main(int argc, char **argv) {
    @autoreleasepool {
        if (argc > 1 && strcmp(argv[1], "--ipc") == 0) {
            AcceptStopAdmission();
            PartialFrameRead();
            SocketPathOwnership();
            ClientCapacity();
            DoubleClose();
            SlowPeerIsolation();
            SixteenSlowPeers();
            WriterSerialization();
            Backpressure();
            SlowReply();
            puts("PASS: isolated IPC lifecycle checks; no hardware or live IPC accessed");
            return 0;
        }
        if (argc > 1 && strcmp(argv[1], "--slow-reply") == 0) {
            SlowReply();
            puts("PASS: IPC reply returns promptly for a saturated peer");
            return 0;
        }
        PartialOpen();
        AcceptStopAdmission();
        PartialFrameRead();
        SocketPathOwnership();
        ClientCapacity();
        DoubleClose();
        SlowPeerIsolation();
        WriterSerialization();
        Backpressure();
        puts("PASS: 9 production lifecycle checks; no hardware or live IPC accessed");
    }
    return 0;
}
