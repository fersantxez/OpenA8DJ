#include <stdio.h>
#include <string.h>

#include "OpenA8DJTimecodeOptimized.h"

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    OpenA8DJDriverModeState driverMode;
    OpenA8DJDriverModeStateInit(&driverMode);
    OpenA8DJTimecodeStatePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.schemaVersion = kOpenA8DJTimecodeSchemaVersion;
    payload.evidenceKind = 1;
    OpenA8DJDriverModeMakeStatePayload(
        &driverMode, false, &payload.driverMode);
    OpenA8DJTimecodeState timecode;
    OpenA8DJTimecodeStateInit(&timecode);
    if (strcmp(argv[1], "qualifying") == 0) {
        (void)OpenA8DJTimecodeArm(
            &timecode, kOpenA8DJDriverModeBalanced,
            kOpenA8DJTimecodeProfileVinyl, 48000.0, 512);
    } else if (strcmp(argv[1], "waiting") == 0) {
        (void)OpenA8DJTimecodeArm(
            &timecode, kOpenA8DJDriverModeBalanced,
            kOpenA8DJTimecodeProfileUnavailable, 48000.0, 512);
    } else if (strcmp(argv[1], "invalid-enum") == 0) {
        payload.driverMode.requestedMode = 99;
    } else if (strcmp(argv[1], "disarmed") != 0) {
        return 2;
    }
    memcpy(&payload.timecode, &timecode, sizeof(timecode));
    return fwrite(&payload, sizeof(payload), 1, stdout) == 1 ? 0 : 1;
}
