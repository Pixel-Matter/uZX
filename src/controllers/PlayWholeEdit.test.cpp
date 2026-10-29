#include <JuceHeader.h>

#include "PlayWholeEdit.h"
#include "../util/TestHelpers.h"

namespace MoTool::Tests {

namespace te = tracktion;
using namespace tracktion::literals;

//==============================================================================
class PlayWholeEditTests : public juce::UnitTest {
public:
    PlayWholeEditTests() : UnitTest("PlayWholeEdit", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        te::getAudioTracks(*edit)[0]->insertMIDIClip({0_tp, 1_tp}, nullptr);
        engine.getDeviceManager().dispatchPendingUpdates();

        auto& transport = edit->getTransport();
        PlayWholeEdit playWholeEdit {*edit};

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
            PlayWholeEdit::rewindIfAtEnd(*edit);
            expectEquals(transport.getPosition().inSeconds(), 0.0);

            transport.setPosition(0.5_tp);
            PlayWholeEdit::rewindIfAtEnd(*edit);
            expectEquals(transport.getPosition().inSeconds(), 0.5);
        }

        beginTest("The loop range is the whole edit and follows its clips");
        {
            expect(transport.getLoopRange() == te::TimeRange(0_tp, 1_tp));

            auto* clip = te::getAudioTracks(*edit)[0]->insertMIDIClip({2_tp, 3_tp}, nullptr).get();
            TestHelpers::flushMessageQueue();
            expect(transport.getLoopRange() == te::TimeRange(0_tp, 3_tp));

            clip->removeFromParent();
            TestHelpers::flushMessageQueue();
            expect(transport.getLoopRange() == te::TimeRange(0_tp, 1_tp));
        }

        transport.stop(false, false);
    }
};

static PlayWholeEditTests playWholeEditTests;

}  // namespace MoTool::Tests
