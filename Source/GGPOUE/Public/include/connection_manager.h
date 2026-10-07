// Transport architecture adapted from erebuswolf's MIT GGPO network-abstraction fork.
// Copyright (c) 2020 Friendly Fish Games LLC. See LICENSE.
#pragma once
#include "CoreMinimal.h"

// Borrowed by GGPO: the caller must keep this alive until ggpo_close_session.
// IDs are local to a manager/session and are independent of participant slots.
class GGPOUE_API ConnectionManager {
public:
    virtual ~ConnectionManager() = default;
    virtual bool IsReady() const = 0;
    virtual bool HasConnection(int connection_id) const = 0;
    // Return bytes accepted, or -1 on failure. Preserve datagram boundaries.
    virtual int SendTo(const char* buffer, int len, int flags, int connection_id) = 0;
    // Return >0 bytes, -1 when drained/error, -2 for a consumed invalid datagram.
    // Never return a size greater than capacity. Set ID to -1 without a packet.
    // A transport disconnect stops deliveries; GGPO's timeout/protocol owns the
    // player disconnect event. An empty datagram is not a UDP disconnect.
    virtual int RecvFrom(char* buffer, int capacity, int flags, int* connection_id) = 0;
    virtual int AddUDPConnection(const char* ip, unsigned short port) { return -1; }
};
