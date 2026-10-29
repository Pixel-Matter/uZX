#include <JuceHeader.h>

#include "StopAtEditEnd.h"
#include "../util/TestHelpers.h"

namespace MoTool::Tests {

namespace te = tracktion;
using namespace tracktion::literals;

//==============================================================================
class StopAtEditEndTests : public juce::UnitTest {
public:
    StopAtEditEndTests() : UnitTest("StopAtEditEnd", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        te::getAudioTracks(*edit)[0]->insertMIDIClip({0_tp, 1_tp}, nullptr);
        engine.getDeviceManager().dispatchPendingUpdates();

        auto& transport = edit->getTransport();
        StopAtEditEnd stopAtEnd {*edit};

        beginTest("Playback keeps going until the end has been heard, then stops");
        {
            transport.setPosition(0.5_tp);
            transport.play(false);
            TestHelpers::processBlocks(engine, 2);

            transport.setPosition(1.5_tp);  // audible time still lags behind the end
            expect(transport.isPlaying());

            TestHelpers::processBlocks(engine, 2);
            transport.setPosition(1.6_tp);
            expect(!transport.isPlaying());
        }

        beginTest("Play from the end starts over from the beginning");
        {
            StopAtEditEnd::rewindIfAtEnd(*edit);
            expectEquals(transport.getPosition().inSeconds(), 0.0);

            transport.setPosition(0.5_tp);
            StopAtEditEnd::rewindIfAtEnd(*edit);
            expectEquals(transport.getPosition().inSeconds(), 0.5);
        }

        transport.stop(false, false);
    }
};

static StopAtEditEndTests stopAtEditEndTests;

}  // namespace MoTool::Tests
