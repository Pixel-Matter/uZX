#include <JuceHeader.h>
#include "formats/psg/PsgData.h"
#include "PsgList.h"
#include "PsgParameter.h"

namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace tracktion;
using namespace juce;
using namespace MoTool::uZX;

//==============================================================================
class PsgListAccumulatedStateTests : public UnitTest {
public:
    PsgListAccumulatedStateTests() : UnitTest("PsgListAcc", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("Each frame stores accumulated state in values array");
        {
            // Create PSG data with sparse changes
            PsgData data {
                {
                    {{PsgRegType::VolumeA, 10}, {PsgRegType::TonePeriodFineA, 100}},  // F0
                    {},                                                               // F1: Empty - no changes
                    {{PsgRegType::VolumeA, 5},  {PsgRegType::VolumeB, 8}},            // F2: Change Volumes
                    {{PsgRegType::TonePeriodFineA, 200}},                             // F3: Change tone period
                },
                {50.0, 1}
            };

            auto edit = Edit::createSingleTrackEdit(engine);
            PsgList psgList;
            psgList.loadFrom(data, *edit, nullptr);

            auto& frames = psgList.getFrames();
            expectEquals(int(frames.size()), 3, "Should have 3 non-empty frames");

            {   // Frame 0: VolumeA=10, TonePeriodA=100
                auto& f = frames[0]->getData();
                // Check masks (what changed)
                expect(f.isSet(PsgParamType::VolumeA), "Frame 0: VolumeA should be set (changed)");
                expect(f.isSet(PsgParamType::TonePeriodA), "Frame 0: TonePeriodA should be set (changed)");
                expect(!f.isSet(PsgParamType::VolumeB), "Frame 0: VolumeB should NOT be set (not changed)");

                // Check accumulated values
                expectEquals(int(f.getRaw(PsgParamType::VolumeA)), 10, "Frame 0: VolumeA value=10");
                expectEquals(int(f.getRaw(PsgParamType::TonePeriodA)), 100, "Frame 0: TonePeriodA value=100");
            }

            {   // Frame 1: VolumeA=5, VolumeB=8, TonePeriodA still 100
                auto& f = frames[1]->getData();
                // Check masks
                expect(f.isSet(PsgParamType::VolumeA), "Frame 1: VolumeA should be set (changed)");
                expect(f.isSet(PsgParamType::VolumeB), "Frame 1: VolumeB should be set (changed)");
                expect(!f.isSet(PsgParamType::TonePeriodA), "Frame 1: TonePeriodA should NOT be set (not changed)");

                // Check accumulated values - TonePeriodA should persist from frame 0
                expectEquals(int(f.getRaw(PsgParamType::VolumeA)), 5, "Frame 1: VolumeA value=5");
                expectEquals(int(f.getRaw(PsgParamType::VolumeB)), 8, "Frame 1: VolumeB value=8");
                expectEquals(int(f.getRaw(PsgParamType::TonePeriodA)), 100, "Frame 1: TonePeriodA accumulated=100");
            }

            {   // Frame 2: TonePeriodA=200, VolumeA still 5, VolumeB still 8
                auto& f = frames[2]->getData();
                // Check masks
                expect(f.isSet(PsgParamType::TonePeriodA), "Frame 2: TonePeriodA should be set (changed)");
                expect(!f.isSet(PsgParamType::VolumeA), "Frame 2: VolumeA should NOT be set (not changed)");
                expect(!f.isSet(PsgParamType::VolumeB), "Frame 2: VolumeB should NOT be set (not changed)");

                // Check accumulated values
                expectEquals(int(f.getRaw(PsgParamType::TonePeriodA)), 200, "Frame 2: TonePeriodA value=200");
                expectEquals(int(f.getRaw(PsgParamType::VolumeA)), 5, "Frame 2: VolumeA accumulated=5");
                expectEquals(int(f.getRaw(PsgParamType::VolumeB)), 8, "Frame 2: VolumeB accumulated=8");
            }
        }

        beginTest("Mixer flags accumulate correctly");
        {
            PsgData data {
                {
                    // Frame 0: Mixer set (tone A on, noise B on)
                    {{PsgRegType::Mixer, 0b00101110}},  // ~(ToneA=0, NoiseB=0) = bits 0,4 clear
                    // Frame 1: Change volume only
                    {{PsgRegType::VolumeA, 10}},
                },
                {50.0, 1}
            };

            auto edit = Edit::createSingleTrackEdit(engine);
            PsgList psgList;
            psgList.loadFrom(data, *edit, nullptr);

            auto& frames = psgList.getFrames();
            expectEquals(int(frames.size()), 2, "Should have 2 frames");

            // Frame 1 should have mixer values accumulated from frame 0
            auto& f1 = frames[1]->getData();
            expect(!f1.isSet(PsgParamType::ToneIsOnA), "Frame 1: Mixer flags should NOT be set (not changed)");

            // But values should be accumulated
            expectEquals(int(f1.getRaw(PsgParamType::ToneIsOnA)), 1, "Frame 1: ToneIsOnA accumulated=1");
            expectEquals(int(f1.getRaw(PsgParamType::NoiseIsOnB)), 1, "Frame 1: NoiseIsOnB accumulated=1");
        }

        beginTest("getFrameAt returns full accumulated state");
        {
            PsgData data {
                {
                    {{PsgRegType::VolumeA, 10}},  // beat ~0
                    {},
                    {{PsgRegType::VolumeB, 8}},   // beat ~0.04
                    {},
                    {{PsgRegType::VolumeC, 6}},   // beat ~0.08
                },
                {50.0, 1}
            };

            auto edit = Edit::createSingleTrackEdit(engine);
            PsgList psgList;
            psgList.loadFrom(data, *edit, nullptr);

            // Get state at various positions
            auto state0 = psgList.getFrameAt(0_bp)->getData();
            expectEquals(int(state0.getRaw(PsgParamType::VolumeA)), 10, "State at 0: VolumeA=10");
            expectEquals(int(state0.getRaw(PsgParamType::VolumeB)), 0, "State at 0: VolumeB=0 (not yet set)");

            auto state1 = psgList.getFrameAt(1_bp)->getData();
            expectEquals(int(state1.getRaw(PsgParamType::VolumeA)), 10, "State at 1: VolumeA=10");
            expectEquals(int(state1.getRaw(PsgParamType::VolumeB)), 8, "State at 1: VolumeB=8");
            expectEquals(int(state1.getRaw(PsgParamType::VolumeC)), 6, "State at 1: VolumeC=6");
        }

        // TODO test accumulated state after loading the edit from a file or constructing the list in other ways, not just from PsgData
    }
};

static PsgListAccumulatedStateTests psgListAccumulatedStateTests;

}  // namespace MoTool::Tests
