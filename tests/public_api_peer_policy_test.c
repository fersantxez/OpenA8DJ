#include "OpenA8DJIPCAuth.h"

#include <stdio.h>

int main(void)
{
    const uid_t hostUID = 202;
    const uid_t consoleUID = 501;

    if (!OpenA8DJIPCPeerUIDIsAuthorized(0, hostUID, consoleUID) ||
        !OpenA8DJIPCPeerUIDIsAuthorized(hostUID, hostUID, consoleUID) ||
        !OpenA8DJIPCPeerUIDIsAuthorized(consoleUID, hostUID, consoleUID) ||
        OpenA8DJIPCPeerUIDIsAuthorized(777, hostUID, consoleUID) ||
        OpenA8DJIPCPeerUIDIsAuthorized(777, hostUID, (uid_t)-1)) {
        fprintf(stderr, "public API peer authorization policy failed\n");
        return 1;
    }

    puts("public API peer authorization policy: PASS");
    return 0;
}
