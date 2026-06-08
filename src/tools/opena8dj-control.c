#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/select.h>
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
    kIPCTypeStatus = 7,
    kIPCTypeInputStatsGet = 8,
    kIPCTypeInputStats = 9
};

enum {
    kInputPairs = 4
};

enum {
    kInputTransformPairMask = 0x0f
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
    uint8_t inputSwapMask;
    uint8_t inputInvertLeftMask;
    uint8_t inputInvertRightMask;
    uint8_t inputSource[kInputPairs];
} __attribute__((packed)) OpenA8DJControlPayload;

typedef struct OpenA8DJInputStatsPayload {
    uint64_t frames[kInputPairs];
    double leftSquare[kInputPairs];
    double rightSquare[kInputPairs];
    double cross[kInputPairs];
    double leftPeak[kInputPairs];
    double rightPeak[kInputPairs];
} __attribute__((packed)) OpenA8DJInputStatsPayload;

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

static int ParseInputPair(const char *text)
{
    if (text == NULL || text[0] == '\0' || text[1] != '\0') {
        return -1;
    }
    if (text[0] >= 'A' && text[0] <= 'D') {
        return text[0] - 'A';
    }
    if (text[0] >= 'a' && text[0] <= 'd') {
        return text[0] - 'a';
    }
    return -1;
}

static uint8_t InputTransformForPair(const OpenA8DJControlPayload *state, int pair)
{
    uint8_t pairBit = (uint8_t)(1u << pair);
    uint8_t transform = 0;
    if ((state->inputSwapMask & pairBit) != 0) {
        transform |= 1u << 0;
    }
    if ((state->inputInvertLeftMask & pairBit) != 0) {
        transform |= 1u << 1;
    }
    if ((state->inputInvertRightMask & pairBit) != 0) {
        transform |= 1u << 2;
    }
    return transform;
}

static const char *InputTransformName(uint8_t transform)
{
    switch (transform) {
        case 0:
            return "normal";
        case 1:
            return "swap";
        case 2:
            return "invert-left";
        case 4:
            return "invert-right";
        case 6:
            return "invert-both";
        case 3:
            return "swap-invert-left";
        case 5:
            return "swap-invert-right";
        case 7:
            return "swap-invert-both";
        default:
            return "unknown";
    }
}

static bool ParseInputTransform(const char *text, uint8_t *outTransform)
{
    if (strcmp(text, "normal") == 0 || strcmp(text, "none") == 0 || strcmp(text, "off") == 0) {
        *outTransform = 0;
        return true;
    }
    if (strcmp(text, "swap") == 0 || strcmp(text, "swap-lr") == 0) {
        *outTransform = 1;
        return true;
    }
    if (strcmp(text, "invert-left") == 0 || strcmp(text, "invert-l") == 0) {
        *outTransform = 2;
        return true;
    }
    if (strcmp(text, "invert-right") == 0 || strcmp(text, "invert-r") == 0) {
        *outTransform = 4;
        return true;
    }
    if (strcmp(text, "invert-both") == 0 || strcmp(text, "invert") == 0 || strcmp(text, "phase") == 0) {
        *outTransform = 6;
        return true;
    }
    if (strcmp(text, "swap-invert-left") == 0 || strcmp(text, "swap-invert-l") == 0) {
        *outTransform = 3;
        return true;
    }
    if (strcmp(text, "swap-invert-right") == 0 || strcmp(text, "swap-invert-r") == 0) {
        *outTransform = 5;
        return true;
    }
    if (strcmp(text, "swap-invert-both") == 0 || strcmp(text, "swap-invert") == 0) {
        *outTransform = 7;
        return true;
    }
    return false;
}

static void SetInputTransform(OpenA8DJControlPayload *state, int pair, uint8_t transform)
{
    uint8_t pairBit = (uint8_t)(1u << pair);
    state->inputSwapMask &= (uint8_t)~pairBit;
    state->inputInvertLeftMask &= (uint8_t)~pairBit;
    state->inputInvertRightMask &= (uint8_t)~pairBit;
    if ((transform & (1u << 0)) != 0) {
        state->inputSwapMask |= pairBit;
    }
    if ((transform & (1u << 1)) != 0) {
        state->inputInvertLeftMask |= pairBit;
    }
    if ((transform & (1u << 2)) != 0) {
        state->inputInvertRightMask |= pairBit;
    }
    state->inputSwapMask &= kInputTransformPairMask;
    state->inputInvertLeftMask &= kInputTransformPairMask;
    state->inputInvertRightMask &= kInputTransformPairMask;
}

static void ResetInputTransforms(OpenA8DJControlPayload *state)
{
    state->inputSwapMask = 0;
    state->inputInvertLeftMask = 0;
    state->inputInvertRightMask = 0;
    for (uint8_t pair = 0; pair < kInputPairs; pair++) {
        state->inputSource[pair] = pair;
    }
}

static bool ApplyProfile(const char *name, OpenA8DJControlPayload *state)
{
    if (strcmp(name, "timecode-vinyl") == 0 || strcmp(name, "tc-vinyl") == 0) {
        state->inputMode = 0;
        state->gndLiftTCVinyl = 1;
        state->gndLiftTCCDLine = 0;
        state->gndLiftPhono = 0;
        state->softwareLock = 1;
        ResetInputTransforms(state);
        return true;
    }
    if (strcmp(name, "timecode-cd-line") == 0 ||
        strcmp(name, "timecode-cd") == 0 ||
        strcmp(name, "cd-line") == 0 ||
        strcmp(name, "line") == 0) {
        state->inputMode = 1;
        state->gndLiftTCVinyl = 0;
        state->gndLiftTCCDLine = 1;
        state->gndLiftPhono = 0;
        state->softwareLock = 1;
        ResetInputTransforms(state);
        return true;
    }
    if (strcmp(name, "phono") == 0) {
        state->inputMode = 2;
        state->gndLiftTCVinyl = 0;
        state->gndLiftTCCDLine = 0;
        state->gndLiftPhono = 1;
        state->softwareLock = 1;
        ResetInputTransforms(state);
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

static bool ReadOneState(int fd, OpenA8DJControlPayload *state)
{
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

static bool ReadState(int fd, OpenA8DJControlPayload *state)
{
    if (!SendIPC(fd, kIPCTypeControlGet, NULL, 0)) {
        return false;
    }
    if (!ReadOneState(fd, state)) {
        return false;
    }

    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(fd, &readSet);
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 20000
        };
        int ready = select(fd + 1, &readSet, NULL, NULL, &timeout);
        if (ready <= 0 || !FD_ISSET(fd, &readSet)) {
            break;
        }
        OpenA8DJControlPayload latest;
        if (!ReadOneState(fd, &latest)) {
            return false;
        }
        *state = latest;
    }
    return true;
}

static bool ReadStats(int fd, OpenA8DJInputStatsPayload *stats)
{
    if (!SendIPC(fd, kIPCTypeInputStatsGet, NULL, 0)) {
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
        if (header.type == kIPCTypeInputStats && header.length >= sizeof(*stats)) {
            memcpy(stats, payload, sizeof(*stats));
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
    printf("  input-transform:   A=%s B=%s C=%s D=%s\n",
           InputTransformName(InputTransformForPair(state, 0)),
           InputTransformName(InputTransformForPair(state, 1)),
           InputTransformName(InputTransformForPair(state, 2)),
           InputTransformName(InputTransformForPair(state, 3)));
    printf("  input-source:      A=%c B=%c C=%c D=%c\n",
           'A' + (state->inputSource[0] < kInputPairs ? state->inputSource[0] : 0),
           'A' + (state->inputSource[1] < kInputPairs ? state->inputSource[1] : 1),
           'A' + (state->inputSource[2] < kInputPairs ? state->inputSource[2] : 2),
           'A' + (state->inputSource[3] < kInputPairs ? state->inputSource[3] : 3));
}

static void PrintInputStats(const OpenA8DJInputStatsPayload *stats)
{
    printf("Audio 8 DJ input stats since last read\n");
    for (int i = 0; i < kInputPairs; i++) {
        double rmsL = 0.0;
        double rmsR = 0.0;
        double corr = 0.0;
        if (stats->frames[i] > 0) {
            rmsL = sqrt(stats->leftSquare[i] / (double)stats->frames[i]);
            rmsR = sqrt(stats->rightSquare[i] / (double)stats->frames[i]);
            double denom = sqrt(stats->leftSquare[i] * stats->rightSquare[i]);
            if (denom > 0.0) {
                corr = stats->cross[i] / denom;
            }
        }
        printf("  Input %c: frames=%llu rmsL=%.8f rmsR=%.8f peakL=%.8f peakR=%.8f corr=%.4f\n",
               'A' + i,
               (unsigned long long)stats->frames[i],
               rmsL,
               rmsR,
               stats->leftPeak[i],
               stats->rightPeak[i],
               corr);
    }
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
    fprintf(stderr, "  %s input-stats\n", argv0);
    fprintf(stderr, "  %s profile timecode-vinyl|timecode-cd-line|phono|unlock\n", argv0);
    fprintf(stderr, "  %s input-mode 0|1|2|timecode-vinyl|timecode-cd-line|phono\n", argv0);
    fprintf(stderr, "  %s gnd-vinyl on|off\n", argv0);
    fprintf(stderr, "  %s gnd-cd-line on|off\n", argv0);
    fprintf(stderr, "  %s gnd-phono on|off\n", argv0);
    fprintf(stderr, "  %s software-lock on|off\n", argv0);
    fprintf(stderr, "  %s input-transform A|B|C|D normal|swap|invert-left|invert-right|invert-both|swap-invert-left|swap-invert-right|swap-invert-both\n", argv0);
    fprintf(stderr, "  %s input-source A|B|C|D A|B|C|D\n", argv0);
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

    if (argc == 2 && strcmp(argv[1], "input-stats") == 0) {
        OpenA8DJInputStatsPayload stats;
        if (!ReadStats(fd, &stats)) {
            fprintf(stderr, "Could not read Audio 8 DJ input stats\n");
            close(fd);
            return 1;
        }
        PrintInputStats(&stats);
        close(fd);
        return 0;
    }

    if (argc == 4 && strcmp(argv[1], "input-transform") == 0) {
        int pair = ParseInputPair(argv[2]);
        uint8_t transform = 0;
        if (pair < 0 || !ParseInputTransform(argv[3], &transform)) {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
        SetInputTransform(&state, pair, transform);
    } else if (argc == 4 && strcmp(argv[1], "input-source") == 0) {
        int pair = ParseInputPair(argv[2]);
        int source = ParseInputPair(argv[3]);
        if (pair < 0 || source < 0) {
            Usage(argv[0]);
            close(fd);
            return 2;
        }
        state.inputSource[pair] = (uint8_t)source;
    } else if (argc != 3) {
        Usage(argv[0]);
        close(fd);
        return 2;
    } else if (strcmp(argv[1], "input-mode") == 0) {
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
