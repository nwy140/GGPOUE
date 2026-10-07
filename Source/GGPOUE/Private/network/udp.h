/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _UDP_H
#define _UDP_H

#include "../poll.h"
#include "include/connection_manager.h"
#include <memory>

// Forward declarations
struct UdpMsg;

#define MAX_UDP_ENDPOINTS     36 // Four participants plus 32 spectators.

static const int MAX_UDP_PACKET_SIZE = 4096;

class Udp : public IPollSink
{
public:
   struct Stats {
      int      bytes_sent;
      int      packets_sent;
      float    kbps_sent;
   };

   struct Callbacks {
      virtual ~Callbacks() { }
      virtual void OnMsg(int from, UdpMsg *msg, int len) = 0;
   };


protected:
   void Log(EGGPOLogVerbosity Verbosity, const char *fmt, ...);

public:
   Udp();

   void Init(uint16 port, Poll *p, Callbacks *callbacks, ConnectionManager* manager = nullptr);
   int AddConnection(const char* ip, uint16 port) { return _manager->AddUDPConnection(ip, port); }
   bool HasConnection(int id) const { return _manager && _manager->HasConnection(id); }
   bool IsReady() const { return _manager && _manager->IsReady(); }
   
   void SendTo(char *buffer, int len, int flags, int connection_id);

   virtual bool OnLoopPoll(void *cookie);

public:
   ~Udp(void);

protected:
   // Network transmission information
   std::unique_ptr<ConnectionManager> _owned_manager;
   ConnectionManager* _manager = nullptr;

   // state management
   Callbacks      *_callbacks;
   Poll           *_poll;
};

#endif
