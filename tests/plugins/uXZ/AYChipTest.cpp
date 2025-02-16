#include <JuceHeader.h>

#include <plugins/uZX/aychip/aychip.h>

// #include "TestUtils.h"

using namespace uZX::Chip;

//==============================================================================
class AyChipPluginTests : public juce::UnitTest {
public:
    AyChipPluginTests() : UnitTest("AYChipPlugin", "MoToolPlugins") {}

    void runTest() override {
        beginTest("AYEmulator Creation");
        {
            auto emulator = AyumiEmulator {44100, 2000000, AyumiEmulator::TypeEnum::YM};
            expect(false);
        }
    }
};

static AyChipPluginTests ayChipPluginTests;