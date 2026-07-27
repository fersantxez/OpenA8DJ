#ifndef OPENA8DJ_IPC_AUTH_H
#define OPENA8DJ_IPC_AUTH_H

#include <stdbool.h>
#include <sys/types.h>

static inline bool OpenA8DJIPCPeerUIDIsAuthorized(uid_t peerUID,
                                                  uid_t hostUID,
                                                  uid_t consoleUID)
{
    return peerUID == 0 ||
           peerUID == hostUID ||
           (consoleUID != (uid_t)-1 && peerUID == consoleUID);
}

#endif
