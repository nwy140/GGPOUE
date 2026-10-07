/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "udp.h"
#include "udp_msg.h"
#include "../types.h"

SOCKET
CreateSocket(uint16 bind_port, int retries)
{
   SOCKET s;
   sockaddr_in sin;
   uint16 port;
   int optval = 1;

   s = socket(AF_INET, SOCK_DGRAM, 0);
   setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof optval);
   setsockopt(s, SOL_SOCKET, SO_DONTLINGER, (const char *)&optval, sizeof optval);

   // non-blocking...
   u_long iMode = 1;
   ioctlsocket(s, FIONBIO, &iMode);

   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   for (port = bind_port; port <= bind_port + retries; port++) {
      sin.sin_port = htons(port);
      if (bind(s, (sockaddr *)&sin, sizeof sin) != SOCKET_ERROR) {
         Log(EGGPOLogVerbosity::Info, "Udp bound to port: %d.\n", port);
         return s;
      }
   }
   closesocket(s);
   return INVALID_SOCKET;
}

// The packet protocol never sees these addresses; only this UDP backend does.
class UDPConnectionManager final : public ConnectionManager {
   SOCKET _socket;
   TArray<sockaddr_in> _peers;
public:
   explicit UDPConnectionManager(uint16 port) : _socket(CreateSocket(port, 0)) {}
   ~UDPConnectionManager() override { if (_socket != INVALID_SOCKET) closesocket(_socket); }
   bool IsReady() const override { return _socket != INVALID_SOCKET; }
   bool HasConnection(int id) const override { return _peers.IsValidIndex(id); }
   int AddUDPConnection(const char* ip, unsigned short port) override {
      sockaddr_in address = {};
      address.sin_family = AF_INET;
      address.sin_port = htons(port);
      if (!ip || port == 0 || inet_pton(AF_INET, ip, &address.sin_addr.s_addr) != 1) return -1;
      for (int i = 0; i < _peers.Num(); ++i)
         if (_peers[i].sin_addr.s_addr == address.sin_addr.s_addr && _peers[i].sin_port == address.sin_port) return i;
      if (_peers.Num() >= MAX_UDP_ENDPOINTS) return -1;
      return _peers.Add(address);
   }
   int SendTo(const char* buffer, int len, int flags, int id) override {
      if (!IsReady() || !HasConnection(id) || !buffer || len <= 0 || len > MAX_UDP_PACKET_SIZE) return -1;
      return sendto(_socket, buffer, len, flags, (sockaddr*)&_peers[id], sizeof(sockaddr_in));
   }
   int RecvFrom(char* buffer, int capacity, int flags, int* id) override {
      *id = -1;
      if (!IsReady()) return -1;
      sockaddr_in from = {};
      int address_len = sizeof(from);
      int len = recvfrom(_socket, buffer, capacity, flags, (sockaddr*)&from, &address_len);
      if (len == SOCKET_ERROR) return WSAGetLastError() == WSAEMSGSIZE ? -2 : -1;
      if (len == 0) return -2;
      for (int i = 0; i < _peers.Num(); ++i) {
         if (_peers[i].sin_addr.s_addr == from.sin_addr.s_addr && _peers[i].sin_port == from.sin_port) {
            *id = i;
            return len;
         }
      }
      return -2;
   }
};

Udp::Udp() : _callbacks(nullptr), _poll(nullptr) {}
Udp::~Udp() = default;
void Udp::Init(uint16 port, Poll* poll, Callbacks* callbacks, ConnectionManager* manager) {
   _callbacks = callbacks;
   _poll = poll;
   if (manager) _manager = manager;
   else {
      _owned_manager.reset(new UDPConnectionManager(port));
      _manager = _owned_manager.get();
   }
   _poll->RegisterLoop(this);
}
void Udp::SendTo(char* buffer, int len, int flags, int id) {
   _manager->SendTo(buffer, len, flags, id);
}
bool Udp::OnLoopPoll(void* cookie) {
   alignas(UdpMsg) uint8 recv_buf[MAX_UDP_PACKET_SIZE];
   // Bound per-poll work even if a backend or unknown sender floods the queue.
   for (int packet = 0; packet < 256; ++packet) {
      int id = -1;
      int len = _manager->RecvFrom((char*)recv_buf, sizeof(recv_buf), 0, &id);
      if (len == -1) break;
      if (len <= 0 || len > MAX_UDP_PACKET_SIZE || !HasConnection(id)) continue;
      _callbacks->OnMsg(id, (UdpMsg*)recv_buf, len);
   }
   return true;
}
