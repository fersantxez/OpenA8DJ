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

typedef struct OpenA8DJControlPayload {
    uint8_t inputMode;
    uint8_t gndLiftTCVinyl;
    uint8_t gndLiftTCCDLine;
    uint8_t gndLiftPhono;
    uint8_t softwareLock;
} __attribute__((packed)) OpenA8DJControlPayload;

static const char *InputModeName(uint8_t mode)
{
    switch (mode) {
        case 0:
            return "timecode-vinyl";
        case 1:
            return "timecode-cd-line";
        case 2:
            return "phono";
        default:
            return "unknown";
    }
}

static bool ParseInputMode(const char *text, uint8_t *outMode)
{
    if (strcmp(text, "0") == 0 ||
        strcmp(text, "timecode-vinyl") == 0 ||
        strcmp(text, "tc-vinyl") == 0) {
        *outMode = 0;
        return true;
    }
    if (strcmp(text, "1") == 0 ||
        strcmp(text, "timecode-cd-line") == 0 ||
        strcmp(text, "timecode-cd") == 0 ||
        strcmp(text, "cd-line") == 0 ||
        strcmp(text, "line") == 0) {
        *outMode = 1;
        return true;
    }
    if (strcmp(text, "2") == 0 || strcmp(text, "phono") == 0) {
        *outMode = 2;
        return true;
    }
    return false;
}

static bool ApplyProfile(const char *name, OpenA8DJControlPayload *state)
{
    if (strcmp(name, "timecode-vinyl") == 0 || strcmp(name, "tc-vinyl") == 0) {
        state->inputMode = 0;
        state->gndLiftTCVinyl = 1;
        state->softwareLock = 1;
        return true;
    }
    if (strcmp(name, "timecode-cd-line") == 0 ||
        strcmp(name, "timecode-cd") == 0 ||
        strcmp(name, "cd-line") == 0 ||
        strcmp(name, "line") == 0) {
        state->inputMode = 1;
        state->gndLiftTCCDLine = 1;
        state->softwareLock = 1;
        return true;
    }
    if (strcmp(name, "phono") == 0) {
        state->inputMode = 2;
        state->gndLiftPhono = 1;
        state->softwareLock = 1;
        return true;
    }
    if (strcmp(name, "unlock") == 0) {
        state->softwareLock = 0;
        return true;
    }
    return false;
}

static bool ReadFull(int fd, void *buffer, size_t length)
{
    uint8_t *bytes = buffer;
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = read(fd, bytes + offset, length - offset);
        if (count <= 0) return false;
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
        if (count <= 0) return false;
        offset += (size_t)count;
    }
    return true;
}

static bool SendIPC(int fd, uint8_t type, const void *payload, uint16_t length)
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

static int ConnectSocket(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

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

static bool ReadState(int fd, OpenA8DJControlPayload *state)
{
    if (!SendIPC(fd, kIPCTypeControlGet, NULL, 0)) {
        return false;
    }
    while (true) {
        OpenA8DJIPCHeader header;
        if (!ReadFull(fd, &header, sizeof(header))) {
            return false;
        }
        if (header.magic != kIPCMagic || header.version != kIPCVersion || header.length > 4096) {
            return false;
        }
        uint8_t payload[4096];
        if (header.length > 0 && !ReadFull(fd, payload, header.length)) {
            return false;
        }
        if (header.type == kIPCTypeControlState && header.length >= sizeof(*state)) {
            memcpy(state, payload, sizeof(*state));
            return true;
        }
    }
}

static void PrintState(const OpenA8DJControlPayload *state)
{
    printf("Audio 8 DJ controls\n");
    printf("  input-mode:        %u (%s)\n", state->inputMode, InputModeName(state->inputMode));
    printf("  gnd-vinyl:         %s\n", state->gndLiftTCVinyl ? "on" : "off");
    printf("  gnd-cd-line:       %s\n", state->gndLiftTCCDLine ? "on" : "off");
    printf("  gnd-phono:         %s\n", state->gndLiftPhono ? "on" : "off");
    printf("  software-lock:     %s\n", state->softwareLock ? "on" : "off");
}

static bool ParseBool(const char *text, uint8_t *outValue)
{
    if (strcmp(text, "on") == 0 || strcmp(text, "1") == 0 || strcmp(text, "true") == 0) {
        *outValue = 1;
        return true;
    }
    if (strcmp(text, "off") == 0 || strcmp(text, "0") == 0 || strcmp(text, "false") == 0) {
        *outValue = 0;
        return true;
    }
    return false;
}

static void Usage(const char *argv0)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s\n", argv0);
    fprintf(stderr, "  %s profile timecode-vinyl|timecode-cd-line|phono|unlock\n", argv0);
    fprintf(stderr, "  %s input-mode 0|1|2|timecode-vinyl|timecode-cd-line|phono\n", argv0);
    fprintf(stderr, "  %s gnd-vinyl on|off\n", argv0);
    fprintf(stderr, "  %s gnd-cd-line on|off\n", argv0);
    fprintf(stderr, "  %s gnd-phono on|off\n", argv0);
    fprintf(stderr, "  %s software-lock on|off\n", argv0);
}

int main(int argc, char **argv)
{
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        Usage(argv[0]);
        return 0;
    }

    int fd = ConnectSocket();
    if (fd < 0) {
        fprintf(stderr, "OpenA8DJ HAL bridge is not available at %s\n", kSocketPath);
        return 1;
    }

    OpenA8DJControlPayload state;
    if (!ReadState(fd, &state)) {
        fprintf(stderr, "Could not read Audio 8 DJ controls\n");
        close(fd);
        return 1;
    }

    if (argc == 1) {
        PrintState(&state);
        close(fd);
        return 0;
    }

    if (argc != 3) {
        Usage(argv[0]);
        close(fd);
        return 2;
    }

    if (strcmp(argv[1], "input-mode") == 0) {
        uint8_t mode = 0;
        if (!ParseInputMode(argv[2], &mode)) {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
        state.inputMode = mode;
    } else if (strcmp(argv[1], "profile") == 0) {
        if (!ApplyProfile(argv[2], &state)) {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
    } else {
        uint8_t value = 0;
        if (!ParseBool(argv[2], &value)) {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
        if (strcmp(argv[1], "gnd-vinyl") == 0) {
            state.gndLiftTCVinyl = value;
        } else if (strcmp(argv[1], "gnd-cd-line") == 0) {
            state.gndLiftTCCDLine = value;
        } else if (strcmp(argv[1], "gnd-phono") == 0) {
            state.gndLiftPhono = value;
        } else if (strcmp(argv[1], "software-lock") == 0) {
            state.softwareLock = value;
        } else {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
    }

    if (!SendIPC(fd, kIPCTypeControlSet, &state, sizeof(state)) || !ReadState(fd, &state)) {
        fprintf(stderr, "Could not write Audio 8 DJ controls\n");
        close(fd);
        return 1;
    }
    PrintState(&state);
    close(fd);
    return 0;
}
