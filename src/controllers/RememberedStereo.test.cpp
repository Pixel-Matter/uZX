#include <JuceHeader.h>

#include "RememberedStereo.h"
#include "PropertyStorage.h"
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "../util/Helpers.h"

namespace MoTool::Tests {

namespace te = tracktion;

class RememberedStereoTests : public juce::UnitTest {
public:
    RememberedStereoTests() : UnitTest("RememberedStereo", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto& storage = dynamic_cast<PropertyStorage&>(engine.getPropertyStorage());
        const auto savedLayout = storage.getCustomProperty(RememberedStereo::layoutKey);
        const auto savedWidth = storage.getCustomProperty(RememberedStereo::widthKey);
        storage.setCustomProperty(RememberedStereo::layoutKey, "");
        storage.setCustomProperty(RememberedStereo::widthKey, "");

        auto makeChip = [](te::Edit& edit) -> uZX::AYChipPlugin& {
            return Helpers::ensureAYPluginOnTrack(*te::getAudioTracks(edit)[0]);
        };

        beginTest("Nothing chosen yet: a new chip keeps its defaults");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            auto& chip = makeChip(*edit);
            RememberedStereo::apply(chip);
            expect(chip.dynamicParams.layout.getStoredValue() == uZX::LayoutEnum::ACB);
            expectEquals(chip.dynamicParams.stereoWidth.getStoredValue(), 0.5f);
        }

        beginTest("The layout and width chosen in one edit apply to a chip in the next");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            auto& chip = makeChip(*edit);
            {
                RememberedStereo remembered {*edit};
                chip.dynamicParams.layout.setStoredValue(uZX::LayoutEnum::ABC);
                chip.dynamicParams.stereoWidth.setStoredValue(0.8f);
            }
            chip.dynamicParams.stereoWidth.setStoredValue(0.1f);  // no longer watched

            auto next = te::Edit::createSingleTrackEdit(engine);
            auto& nextChip = makeChip(*next);
            RememberedStereo::apply(nextChip);
            expect(nextChip.dynamicParams.layout.getLiveValue() == uZX::LayoutEnum::ABC);
            expectEquals(nextChip.dynamicParams.stereoWidth.getLiveValue(), 0.8f);
        }

        storage.setCustomProperty(RememberedStereo::layoutKey, savedLayout);
        storage.setCustomProperty(RememberedStereo::widthKey, savedWidth);
    }
};

static RememberedStereoTests rememberedStereoTests;

}  // namespace MoTool::Tests
