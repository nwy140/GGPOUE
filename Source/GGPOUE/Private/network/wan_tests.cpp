#include "udp_proto.h"
#include "udp.h"
#include "sync.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace {
class FWANTransport : public ConnectionManager {
public:
    TArray<uint8> Packet;
    bool IsReady()const override{return true;}
    bool HasConnection(int Id)const override{return Id==7;}
    int SendTo(const char* Data,int Len,int,int)override {Packet.SetNum(Len);FMemory::Memcpy(Packet.GetData(),Data,Len);return Len;}
    int RecvFrom(char*,int,int,int* Id)override{*Id=-1;return -1;}
};
class FWANReceiver : public Udp::Callbacks {public:void OnMsg(int,UdpMsg*,int)override{}};
class FWANProtocol : public UdpProtocol {
public:
    void RetryExpired(){_last_sync_request_time-=501;OnLoopPoll(nullptr);}
    void PrepareRunning(){_current_state=Running;_remote_magic_number=_magic_number;}
    void AddUnsent(const GameInput& Input){_pending_output.push(Input);}
    void Flush(){SendPendingOutput();}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGGPOWANProtocolTest,"RollbackCombat.GGPO.WANProtocol",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGGPOWANProtocolTest::RunTest(const FString&)
{
    sf4e::netplay::InputRepair Repair;Repair.SetEnabled(true);Repair.OnInputSent(100,12);
    TestFalse(TEXT("Repair never races normal 60 Hz sends"),Repair.TryRepair(116,true));
    TestTrue(TEXT("Stalled history repaired at 33 ms"),Repair.TryRepair(133,true));
    TestFalse(TEXT("Repair waits for idle transport"),Repair.TryRepair(200,false));
    TestTrue(TEXT("Repair backs off to 66 ms"),Repair.TryRepair(200,true));
    TestTrue(TEXT("Third repair at 132 ms"),Repair.TryRepair(332,true));
    TestTrue(TEXT("Fourth repair at 200 ms"),Repair.TryRepair(532,true));
    Repair.OnAck(11);
    TestFalse(TEXT("ACK progress cannot refill rolling repair budget"),Repair.TryRepair(732,true));
    TestTrue(TEXT("Repair budget recovers after one second"),Repair.TryRepair(1133,true));
    Repair.OnAck(12);TestFalse(TEXT("Acknowledged history needs no repair"),Repair.TryRepair(1400,true));
    FWANTransport Transport;FWANReceiver Receiver;Poll Poller;Udp UDP;
    UDP.Init(0,&Poller,&Receiver,&Transport);
    FWANProtocol Protocol;Protocol.Init(&UDP,Poller,0,7,nullptr,0,16);
    Protocol.Synchronize();
    UdpMsg First(UdpMsg::Invalid);FMemory::Memcpy(&First,Transport.Packet.GetData(),Transport.Packet.Num());
    Protocol.RetryExpired();
    UdpMsg Retry(UdpMsg::Invalid);FMemory::Memcpy(&Retry,Transport.Packet.GetData(),Transport.Packet.Num());
    TestEqual(TEXT("Retry keeps challenge valid for a delayed reply"),Retry.u.sync_request.random_request,First.u.sync_request.random_request);
    UdpMsg Reply(UdpMsg::SyncReply);Reply.hdr.magic=First.hdr.magic;Reply.hdr.sequence_number=0;
    Reply.u.sync_reply.compatibility_version=First.u.sync_request.compatibility_version;
    Reply.u.sync_reply.compatibility_token=0;Reply.u.sync_reply.random_reply=First.u.sync_request.random_request;
    Protocol.OnMsg(&Reply,Reply.PacketSize());
    UdpProtocol::Event Event;bool Progress=false;
    while(Protocol.GetEvent(Event))Progress|=Event.type==UdpProtocol::Event::Synchronizing;
    TestTrue(TEXT("Reply after retry advances handshake"),Progress);
    UdpMsg Next(UdpMsg::Invalid);FMemory::Memcpy(&Next,Transport.Packet.GetData(),Transport.Packet.Num());
    TestNotEqual(TEXT("New handshake round uses a different challenge"),Next.u.sync_request.random_request,First.u.sync_request.random_request);
    Protocol.PrepareRunning();
    for(int F=0;F<64;++F){uint8 Bits[16];FMemory::Memset(Bits,F%2?0xff:0,16);GameInput Input;Input.init(F,reinterpret_cast<char*>(Bits),16);Protocol.AddUnsent(Input);}
    Protocol.Flush();
    TestTrue(TEXT("High entropy history fits transport capacity"),Transport.Packet.Num()<=4096);
    UdpMsg History(UdpMsg::Invalid);FMemory::Memcpy(&History,Transport.Packet.GetData(),Transport.Packet.Num());
    TestTrue(TEXT("Expanding delta history selects raw encoding"),(History.u.input.input_size&0x80)!=0);
    Protocol.OnMsg(&History,Transport.Packet.Num());
    int Count=0;
    while(Protocol.GetEvent(Event))if(Event.type==UdpProtocol::Event::Input){TestEqual(TEXT("Every historical frame survives"),Event.u.input.input.frame,Count);for(int I=0;I<16;++I)TestEqual(TEXT("Raw input preserved"),uint8(Event.u.input.input.bits[I]),uint8(Count%2?0xff:0));++Count;}
    TestEqual(TEXT("Complete history delivered"),Count,64);
    Protocol.OnMsg(&History,Transport.Packet.Num());
    TestFalse(TEXT("Duplicate history produces no duplicate inputs"),Protocol.GetEvent(Event));
    History.u.input.num_bits-=1;
    Protocol.OnMsg(&History,History.PacketSize());
    TestFalse(TEXT("Truncated raw frame rejected"),Protocol.GetEvent(Event));
    FWANProtocol Large;Large.Init(&UDP,Poller,0,7,nullptr,0,16);Large.PrepareRunning();
    for(int F=0;F<300;++F){char Bits[16];FMemory::Memset(Bits,F%2?0xff:0,16);GameInput Input;Input.init(F,Bits,16);Large.AddUnsent(Input);}
    Large.Flush();
    TestTrue(TEXT("Oversized history is split at the datagram bound"),Transport.Packet.Num()<=4096);
    UdpMsg Prefix(UdpMsg::Invalid);FMemory::Memcpy(&Prefix,Transport.Packet.GetData(),Transport.Packet.Num());
    Large.OnMsg(&Prefix,Transport.Packet.Num());int Delivered=0;
    while(Large.GetEvent(Event))if(Event.type==UdpProtocol::Event::Input){TestEqual(TEXT("Prefix sequential"),Event.u.input.input.frame,Delivered);++Delivered;}
    TestTrue(TEXT("Only complete frames fit first packet"),Delivered>64 && Delivered<300);
    UdpMsg Ack(UdpMsg::InputAck);Ack.hdr.magic=Prefix.hdr.magic;Ack.hdr.sequence_number=1;Ack.u.input_ack.ack_frame=Delivered-1;
    Large.OnMsg(&Ack,Ack.PacketSize());Large.Flush();
    UdpMsg Suffix(UdpMsg::Invalid);FMemory::Memcpy(&Suffix,Transport.Packet.GetData(),Transport.Packet.Num());
    Large.OnMsg(&Suffix,Transport.Packet.Num());
    while(Large.GetEvent(Event))if(Event.type==UdpProtocol::Event::Input){TestEqual(TEXT("Unsent suffix retained"),Event.u.input.input.frame,Delivered);for(int I=0;I<16;++I)TestEqual(TEXT("Suffix bits preserved"),uint8(Event.u.input.input.bits[I]),uint8(Delivered%2?0xff:0));++Delivered;}
    TestEqual(TEXT("All frames delivered after acknowledgement"),Delivered,300);
    return !HasAnyErrors();
}
#endif
