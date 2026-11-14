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

        beginTest("A loop range set to part of the edit stays as its clips change");
        {
            transport.setLoopRange({0.25_tp, 0.75_tp});
            auto* clip = te::getAudioTracks(*edit)[0]->insertMIDIClip({2_tp, 3_tp}, nullptr).get();
            TestHelpers::flushMessageQueue();
            expect(transport.getLoopRange() == te::TimeRange(0.25_tp, 0.75_tp));

            transport.setLoopRange({0_tp, 3_tp});  // the whole edit again: it follows
            clip->removeFromParent();
            TestHelpers::flushMessageQueue();
            expect(transport.getLoopRange() == te::TimeRange(0_tp, 1_tp));
        }

        // The audio thread moves the play head and the transport's timer reads it back,
        // 200 ms after a jump at the earliest
        const auto runAudio = [&](int blocks) {
            for (int i = 0; i < blocks; ++i) {
                TestHelpers::processBlocks(engine, 1);
                MessageManager::getInstance()->runDispatchLoopUntil(20);
            }
        };
        const auto playFrom = [&](te::TimePosition start, int blocks) {
            transport.stop(false, false);
            transport.setPosition(start);
            transport.play(false);
            runAudio(blocks);
            return transport.getPosition().inSeconds();
        };

        beginTest("Looping off while stopped at the loop end leaves the playhead there");
        {
            transport.stop(false, false);
            transport.ensureContextAllocated();
            transport.setPosition(1_tp);
            transport.looping = true;
            runAudio(15);
            transport.looping = false;
            runAudio(15);
            expectEquals(transport.getPosition().inSeconds(), 1.0);
        }

        beginTest("Looping, playback starts at the playhead and goes back only from the loop end");
        {
            transport.setLoopRange({0.25_tp, 0.75_tp});
            transport.looping = true;

            const auto before = playFrom(0.05_tp, 15);  // about 0.17 s on
            expect(before > 0.05 && before < 0.25, "plays into the loop: " + String(before));

            const auto after = playFrom(0.8_tp, 15);
            expect(after > 0.8, "plays on past the loop: " + String(after));

            const auto wrapped = playFrom(0.65_tp, 15);
            expect(wrapped >= 0.25 && wrapped < 0.5, "back from the loop end: " + String(wrapped));

            transport.setPosition(0.8_tp);  // a jump past the loop while playing
            runAudio(15);
            expect(transport.getPosition().inSeconds() > 0.8, String(transport.getPosition().inSeconds()));
        }

        beginTest("Looping, playback past the loop stops at the end");
        {
            playFrom(0.9_tp, 30);
            expect(!transport.isPlaying());
            transport.looping = false;
        }

        transport.stop(false, false);
    }
};

static PlayWholeEditTests playWholeEditTests;

}  // namespace MoTool::Tests
