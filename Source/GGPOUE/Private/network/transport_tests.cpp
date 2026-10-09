#include "Misc/AutomationTest.h"
#include "udp.h"
#include "udp_msg.h"
#include "include/ggponet.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace {
class TestTransport final : public ConnectionManager {
public:
   int Receives = 0;
   int Sends = 0;
   bool Ready = true;
   bool IsReady() const override { return Ready; }
   bool HasConnection(int id) const override { return id == 7 || id == 42; }
   int SendTo(const char*, int len, int, int id) override {
      if (!HasConnection(id)) return -1;
      ++Sends;
      return len;
   }
   int RecvFrom(char* buffer, int capacity, int, int* id) override {
      ++Receives;
      *id = -1;
      if (Receives == 1) return -2; // Consumed unknown-source datagram.
      if (Receives == 2) { *id = 7; return capacity + 1; } // Backend contract violation.
      if (Receives == 3) { *id = 42; buffer[0] = 17; return 1; }
      return -1;
   }
};
class TestReceiver final : public Udp::Callbacks {
public:
   int Count = 0;
   int Peer = -1;
   int Length = 0;
   void OnMsg(int id, UdpMsg*, int len) override { ++Count; Peer = id; Length = len; }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGGPOTransportRoutingTest,
   "RollbackCombat.GGPO.TransportRouting",
   EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGGPOTransportRoutingTest::RunTest(const FString&) {
   TestTransport transport;
   TestReceiver receiver;
   Poll poll;
   {
      Udp pump;
      pump.Init(0, &poll, &receiver, &transport);
      pump.OnLoopPoll(nullptr);
      TestEqual(TEXT("Unknown/oversized packets do not block subsequent valid packet"), receiver.Count, 1);
      TestEqual(TEXT("Opaque ID remains independent of participant slot"), receiver.Peer, 42);
      TestEqual(TEXT("Datagram size preserved"), receiver.Length, 1);
      char byte = 9;
      pump.SendTo(&byte, 1, 0, 7);
      TestEqual(TEXT("Send delegated to borrowed manager"), transport.Sends, 1);
   }
   TestTrue(TEXT("Session pump does not destroy borrowed manager"), transport.IsReady());
   GGPOSessionCallbacks callbacks = {};
   callbacks.begin_game = [](const char*) { return true; };
   callbacks.on_event = [](GGPOEvent*) { return true; };
   callbacks.free_buffer = [](void* buffer) { FMemory::Free(buffer); };
   GGPOSession* session = nullptr;
   TestEqual(TEXT("Four-slot external transport startup"),
      GGPONet::ggpo_start_session(&session, &callbacks, &transport, "TransportTest", 4, 1), GGPO_OK);
   if (session) {
      TestEqual(TEXT("Zero prediction window rejected"),GGPONet::ggpo_set_prediction_window(session,0),GGPO_ERRORCODE_INVALID_REQUEST);
      TestEqual(TEXT("Oversized prediction window rejected"),GGPONet::ggpo_set_prediction_window(session,33),GGPO_ERRORCODE_INVALID_REQUEST);
      TestEqual(TEXT("Maximum bounded window accepted before play"),GGPONet::ggpo_set_prediction_window(session,32),GGPO_OK);
      GGPOPlayer player = {};
      player.size = sizeof(player);
      player.type = EGGPOPlayerType::REMOTE;
      player.player_num = 1;
      player.connection_id = 99;
      GGPOPlayerHandle handle;
      TestEqual(TEXT("Unregistered remote ID rejected"), GGPONet::ggpo_add_player(session, &player, &handle), GGPO_ERRORCODE_INVALID_REQUEST);
      player.connection_id = 42;
      TestEqual(TEXT("Registered non-slot ID accepted"), GGPONet::ggpo_add_player(session, &player, &handle), GGPO_OK);
      GGPONet::ggpo_close_session(session);
   }
   transport.Ready = false;
   session = nullptr;
   TestEqual(TEXT("Unavailable transport rejected before session creation"),
      GGPONet::ggpo_start_session(&session, &callbacks, &transport, "TransportTest", 2, 1), GGPO_ERRORCODE_INVALID_REQUEST);
   TestNull(TEXT("Failed startup leaves no session"), session);
   return !HasAnyErrors();
}
#endif
