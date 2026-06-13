#import <CoreMIDI/CoreMIDI.h>
#import <CoreAudio/AudioHardware.h>
#import <Foundation/Foundation.h>
#import <mach/mach_time.h>

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static const char *kSocketPath = "/tmp/opena8dj-control.sock";

enum {
    kIPCVersion = 1,
    kIPCMagic = 0x4a443841,
    kIPCTypeHello = 1,
    kIPCTypeMidiToDevice = 2,
    kIPCTypeMidiFromDevice = 3,
    kIPCTypeControlGet = 4,
    kIPCTypeControlSet = 5,
    kIPCTypeControlState = 6,
    kIPCTypeStatus = 7
};

typedef struct OpenA8DJIPCHeader {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t length;
} __attribute__((packed)) OpenA8DJIPCHeader;

static pthread_mutex_t gSocketMutex = PTHREAD_MUTEX_INITIALIZER;
static int gSocketFd = -1;
static MIDIClientRef gMIDIClient = 0;
static MIDIEndpointRef gMIDISource = 0;
static AudioDeviceID gAudioDevice = kAudioObjectUnknown;
static AudioDeviceIOProcID gAudioIOProcID = NULL;

static bool ReadFull(int fd, void *buffer, size_t length)
{
    uint8_t *bytes = buffer;
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = read(fd, bytes + offset, length - offset);
        if (count <= 0) {
            return false;
        }
        offset += (size_t)count;
    }
    return true;
}

static bool WriteFull(int fd, const void *buffer, size_t length)
{
    const uint8_t *bytes = buffer;
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = write(fd, bytes + offset, length - offset);
        if (count <= 0) {
            return false;
        }
        offset += (size_t)count;
    }
    return true;
}

static bool SendIPCUnlocked(int fd, uint8_t type, const void *payload, uint16_t length)
{
    OpenA8DJIPCHeader header = {
        .magic = kIPCMagic,
        .version = kIPCVersion,
        .type = type,
        .length = length
    };
    return WriteFull(fd, &header, sizeof(header)) &&
           (length == 0 || WriteFull(fd, payload, length));
}

static bool SendIPC(uint8_t type, const void *payload, uint16_t length)
{
    pthread_mutex_lock(&gSocketMutex);
    int fd = gSocketFd;
    bool ok = fd >= 0 && SendIPCUnlocked(fd, type, payload, length);
    pthread_mutex_unlock(&gSocketMutex);
    return ok;
}

static int ConnectSocket(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strlcpy(address.sun_path, kSocketPath, sizeof(address.sun_path));
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static OSStatus KeepaliveIOProc(AudioObjectID deviceID,
                                const AudioTimeStamp *now,
                                const AudioBufferList *inputData,
                                const AudioTimeStamp *inputTime,
                                AudioBufferList *outputData,
                                const AudioTimeStamp *outputTime,
                                void *clientData)
{
    (void)deviceID;
    (void)now;
    (void)inputData;
    (void)inputTime;
    (void)outputTime;
    (void)clientData;
    if (outputData != NULL) {
        for (UInt32 i = 0; i < outputData->mNumberBuffers; i++) {
            if (outputData->mBuffers[i].mData != NULL) {
                memset(outputData->mBuffers[i].mData, 0, outputData->mBuffers[i].mDataByteSize);
            }
        }
    }
    return noErr;
}

static AudioDeviceID FindOpenA8DJDevice(void)
{
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                     &address,
                                                     0,
                                                     NULL,
                                                     &size);
    if (status != noErr || size == 0) {
        return kAudioObjectUnknown;
    }

    UInt32 count = size / sizeof(AudioDeviceID);
    AudioDeviceID *devices = calloc(count, sizeof(AudioDeviceID));
    if (devices == NULL) {
        return kAudioObjectUnknown;
    }

    status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &address,
                                        0,
                                        NULL,
                                        &size,
                                        devices);
    if (status != noErr) {
        free(devices);
        return kAudioObjectUnknown;
    }

    AudioDeviceID result = kAudioObjectUnknown;
    for (UInt32 i = 0; i < count; i++) {
        CFStringRef uid = NULL;
        UInt32 uidSize = sizeof(uid);
        AudioObjectPropertyAddress uidAddress = {
            kAudioDevicePropertyDeviceUID,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        status = AudioObjectGetPropertyData(devices[i],
                                            &uidAddress,
                                            0,
                                            NULL,
                                            &uidSize,
                                            &uid);
        if (status == noErr && uid != NULL) {
            if (CFStringCompare(uid, CFSTR("org.opena8dj.Audio8DJ"), 0) == kCFCompareEqualTo) {
                result = devices[i];
            }
            CFRelease(uid);
        }
        if (result != kAudioObjectUnknown) {
            break;
        }
    }

    free(devices);
    return result;
}

static bool StartAudioKeepalive(void)
{
    if (gAudioIOProcID != NULL) {
        return true;
    }

    AudioDeviceID device = FindOpenA8DJDevice();
    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "OpenA8DJ Core Audio device not found yet\n");
        return false;
    }
    fprintf(stderr, "OpenA8DJ Core Audio device found: %u\n", device);

    OSStatus status = AudioDeviceCreateIOProcID(device, KeepaliveIOProc, NULL, &gAudioIOProcID);
    if (status != noErr) {
        fprintf(stderr, "AudioDeviceCreateIOProcID failed: %d\n", (int)status);
        gAudioIOProcID = NULL;
        return false;
    }

    /*
     * Do not set kAudioDevicePropertyIOProcStreamUsage here. On macOS 26 the
     * proxy can report the property and then block while setting it, which
     * leaves the control bridge running but unusable.
     */
    status = AudioDeviceStart(device, gAudioIOProcID);
    if (status != noErr) {
        fprintf(stderr, "AudioDeviceStart failed: %d\n", (int)status);
        AudioDeviceDestroyIOProcID(device, gAudioIOProcID);
        gAudioIOProcID = NULL;
        return false;
    }

    gAudioDevice = device;
    fprintf(stderr, "OpenA8DJ audio keepalive started\n");
    return true;
}

static void ResetAudioKeepalive(void)
{
    if (gAudioDevice != kAudioObjectUnknown && gAudioIOProcID != NULL) {
        AudioDeviceStop(gAudioDevice, gAudioIOProcID);
        AudioDeviceDestroyIOProcID(gAudioDevice, gAudioIOProcID);
    }
    gAudioDevice = kAudioObjectUnknown;
    gAudioIOProcID = NULL;
}

static void PublishMIDIFromDevice(const uint8_t *bytes, uint16_t length)
{
    if (gMIDISource == 0 || bytes == NULL || length == 0) {
        return;
    }

    uint16_t offset = 0;
    while (offset < length) {
        uint16_t chunk = length - offset;
        if (chunk > 256) {
            chunk = 256;
        }
        Byte packetStorage[sizeof(MIDIPacketList) + 512];
        MIDIPacketList *packetList = (MIDIPacketList *)packetStorage;
        MIDIPacket *packet = MIDIPacketListInit(packetList);
        packet = MIDIPacketListAdd(packetList,
                                   sizeof(packetStorage),
                                   packet,
                                   mach_absolute_time(),
                                   chunk,
                                   bytes + offset);
        if (packet != NULL) {
            MIDIReceived(gMIDISource, packetList);
        }
        offset += chunk;
    }
}

static void MIDIDestinationRead(const MIDIPacketList *packetList, void *readProcRefCon, void *srcConnRefCon)
{
    (void)readProcRefCon;
    (void)srcConnRefCon;
    const MIDIPacket *packet = &packetList->packet[0];
    for (UInt32 index = 0; index < packetList->numPackets; index++) {
        SendIPC(kIPCTypeMidiToDevice, packet->data, packet->length);
        packet = MIDIPacketNext(packet);
    }
}

static bool CreateMIDIEndpoints(void)
{
    if (gMIDIClient != 0 && gMIDISource != 0) {
        return true;
    }

    OSStatus status = MIDIClientCreate(CFSTR("OpenA8DJ MIDI Bridge"), NULL, NULL, &gMIDIClient);
    if (status != noErr) {
        fprintf(stderr, "MIDIClientCreate failed: %d\n", (int)status);
        gMIDIClient = 0;
        return false;
    }

    status = MIDISourceCreate(gMIDIClient, CFSTR("Open Audio 8 DJ MIDI Out"), &gMIDISource);
    if (status != noErr) {
        fprintf(stderr, "MIDISourceCreate failed: %d\n", (int)status);
        MIDIClientDispose(gMIDIClient);
        gMIDIClient = 0;
        return false;
    }

    MIDIEndpointRef destination = 0;
    status = MIDIDestinationCreate(gMIDIClient,
                                   CFSTR("Open Audio 8 DJ MIDI In"),
                                   MIDIDestinationRead,
                                   NULL,
                                   &destination);
    if (status != noErr) {
        fprintf(stderr, "MIDIDestinationCreate failed: %d\n", (int)status);
        MIDIEndpointDispose(gMIDISource);
        MIDIClientDispose(gMIDIClient);
        gMIDISource = 0;
        gMIDIClient = 0;
        return false;
    }

    MIDIObjectSetStringProperty(gMIDISource,
                                kMIDIPropertyDisplayName,
                                CFSTR("Open Audio 8 DJ MIDI Out"));
    MIDIObjectSetStringProperty(destination,
                                kMIDIPropertyDisplayName,
                                CFSTR("Open Audio 8 DJ MIDI In"));
    return true;
}

static void ReadBridgeMessages(int fd)
{
    while (true) {
        OpenA8DJIPCHeader header;
        if (!ReadFull(fd, &header, sizeof(header))) {
            break;
        }
        if (header.magic != kIPCMagic || header.version != kIPCVersion || header.length > 4096) {
            break;
        }
        uint8_t payload[4096];
        if (header.length > 0 && !ReadFull(fd, payload, header.length)) {
            break;
        }
        if (header.type == kIPCTypeMidiFromDevice) {
            PublishMIDIFromDevice(payload, header.length);
        } else if (header.type == kIPCTypeStatus) {
            fprintf(stderr, "%.*s\n", header.length, payload);
        }
    }
}

int main(void)
{
    @autoreleasepool {
        while (!CreateMIDIEndpoints()) {
            sleep(2);
        }

        fprintf(stderr, "OpenA8DJ MIDI bridge running\n");
        bool keepaliveEnabled = getenv("OPENA8DJ_MIDI_KEEPALIVE") != NULL;
        int failedSocketAttempts = 0;
        while (true) {
            if (keepaliveEnabled && !StartAudioKeepalive()) {
                sleep(1);
                continue;
            }

            int fd = ConnectSocket();
            if (fd < 0) {
                failedSocketAttempts++;
                if (keepaliveEnabled && failedSocketAttempts > 5) {
                    ResetAudioKeepalive();
                    failedSocketAttempts = 0;
                }
                sleep(1);
                continue;
            }
            failedSocketAttempts = 0;

            pthread_mutex_lock(&gSocketMutex);
            gSocketFd = fd;
            SendIPCUnlocked(fd, kIPCTypeHello, NULL, 0);
            pthread_mutex_unlock(&gSocketMutex);

            fprintf(stderr, "Connected to OpenA8DJ HAL bridge\n");
            ReadBridgeMessages(fd);

            pthread_mutex_lock(&gSocketMutex);
            if (gSocketFd == fd) {
                gSocketFd = -1;
            }
            pthread_mutex_unlock(&gSocketMutex);
            close(fd);
            fprintf(stderr, "Disconnected from OpenA8DJ HAL bridge\n");
            sleep(1);
        }
    }
}
