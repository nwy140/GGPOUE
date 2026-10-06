#include "input_validation.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGGPOInputEncodingTest,"RollbackCombat.GGPO.InputPacketBounds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGGPOInputEncodingTest::RunTest(const FString&) {
    uint8_t Empty[1]={0},Change[2]={3,0};
    TestTrue(TEXT("Unchanged frames accepted"),GGPOInputEncodingValid(Empty,8,16,0));
    TestTrue(TEXT("Complete button delta accepted"),GGPOInputEncodingValid(Change,12,16,0));
    TestFalse(TEXT("Truncated delta rejected"),GGPOInputEncodingValid(Change,10,16,0));
    TestFalse(TEXT("Missing frame terminator rejected"),GGPOInputEncodingValid(Change,11,16,0));
    uint8_t OutOfBounds[2]={35,0}; // button 8, input has only 8 bits.
    TestFalse(TEXT("Button outside input rejected"),GGPOInputEncodingValid(OutOfBounds,12,1,0));
    TestFalse(TEXT("Frame arithmetic overflow rejected"),GGPOInputEncodingValid(Empty,8,16,INT_MAX-1));
    TestFalse(TEXT("Declared stream beyond buffer capacity rejected"),GGPOInputEncodingValid(Empty,32769,16,0));
    return !HasAnyErrors();
}
#endif
