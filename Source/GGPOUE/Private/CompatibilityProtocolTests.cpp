#include "network/udp_proto.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGGPOCompatibilityWireTest,"RollbackCombat.GGPO.CompatibilityWireRejection",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGGPOCompatibilityWireTest::RunTest(const FString&)
{
    for(int32 Case=0;Case<3;++Case)
    {
        UdpProtocol Protocol;UdpMsg Message(UdpMsg::SyncReply);
        FMemory::Memzero(&Message,sizeof(Message));Message.hdr.type=UdpMsg::SyncReply;
        Message.u.sync_reply.compatibility_version=Case==1?0x52424302:0x52424301;
        Message.u.sync_reply.compatibility_token=Case==2?123:0;
        const int32 Length=Case==0?int32(sizeof(Message.hdr)+sizeof(uint32)):Message.PacketSize();
        Protocol.OnMsg(&Message,Length);UdpProtocol::Event Event;
        if(TestTrue(TEXT("Legacy/version/token mismatch emits event"),Protocol.GetEvent(Event)))
        {
            TestEqual(TEXT("Wire mismatch is explicit incompatibility"),int32(Event.type),int32(UdpProtocol::Event::Incompatible));
            TestFalse(TEXT("Rejected protocol is never synchronized"),Protocol.IsSynchronized());
        }
        Protocol.OnMsg(&Message,Length);
        TestFalse(TEXT("Repeated incompatible packet emits no duplicate event"),Protocol.GetEvent(Event));
    }
    return !HasAnyErrors();
}
#endif
