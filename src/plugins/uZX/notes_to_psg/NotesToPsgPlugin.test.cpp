#include "NotesToPsgPlugin.h"

#include <JuceHeader.h>

#include "../aychip/AYPlugin.h"

namespace MoTool::Tests {

using namespace MoTool::uZX;
namespace te = tracktion;

//==============================================================================
/** The converter plays at the clock of the AY chip after it and never sets that clock. */
class NotesToPsgPluginClockTests : public juce::UnitTest
{
public:
    NotesToPsgPluginClockTests() : UnitTest("NotesToPsgPluginClock", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto* track = te::getAudioTracks(*edit)[0];

        auto converterPtr = edit->getPluginCache().createNewPlugin(NotesToPsgPlugin::xmlTypeName, {});
        auto& converter = dynamic_cast<NotesToPsgPlugin&>(*converterPtr);
        track->pluginList.insertPlugin(converterPtr, 0, nullptr);

        constexpr double zx = AYChipPlugin::defaultClockMhz * MHz;
        constexpr int a4 = 69;
        const auto a4Period = [&] {
            return converter.getTuningSystem().midiNoteToPeriod(a4, TuningSystem::PeriodMode::Tone);
        };

        beginTest("Without a chip the converter plays at the AY's default clock");
        expectEquals(converter.getChipClock(), zx);

        auto chipPtr = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
        auto& chip = dynamic_cast<AYChipPlugin&>(*chipPtr);
        track->pluginList.insertPlugin(chipPtr, 1, nullptr);

        beginTest("Equal temperament follows the chip's clock");
        {
            expectEquals(a4Period(), 252);  // 1773400 / 16 / 440
            chip.staticParams.chipClock.setStoredValue(1.75);
            expectEquals(converter.getChipClock(), 1.75 * MHz);
            expectEquals(converter.getTuningSystem().getClockFrequency(), 1.75 * MHz);
            expectEquals(a4Period(), 249);  // 1750000 / 16 / 440
        }

        beginTest("A tracker table keeps its periods and its own clock, and leaves the chip's alone");
        {
            converter.staticParams.tuningTable.setStoredValue(BuiltinTuningType::CustomPT_0_PT);
            expectEquals(chip.staticParams.chipClock.getStoredValue(), 1.75);
            expectEquals(converter.getTuningSystem().getClockFrequency(), 1625000.0);
            const auto period = a4Period();
            chip.staticParams.chipClock.setStoredValue(2.0);
            expectEquals(a4Period(), period);
            converter.staticParams.tuningTable.setStoredValue(BuiltinTuningType::EqualTemperament);
        }

        beginTest("Restoring the converter's state leaves the chip's clock alone");
        {
            converter.restorePluginStateFromValueTree(converter.state);
            expectEquals(chip.staticParams.chipClock.getStoredValue(), 2.0);
            expectEquals(converter.getChipClock(), 2.0 * MHz);
        }

        beginTest("A reloaded edit keeps the chip's clock and the converter follows it");
        {
            auto reloaded = te::Edit::createEdit({engine, edit->state.createCopy(), te::ProjectItemID::createNewID(te::ProjectID(0))});
            NotesToPsgPlugin* reloadedConverter = nullptr;
            AYChipPlugin* reloadedChip = nullptr;
            for (auto* p : te::getAudioTracks(*reloaded)[0]->pluginList) {
                if (auto* c = dynamic_cast<NotesToPsgPlugin*>(p))
                    reloadedConverter = c;
                if (auto* c = dynamic_cast<AYChipPlugin*>(p))
                    reloadedChip = c;
            }
            expect(reloadedConverter != nullptr && reloadedChip != nullptr);
            if (reloadedConverter != nullptr && reloadedChip != nullptr) {
                expectEquals(reloadedChip->staticParams.chipClock.getStoredValue(), 2.0);
                expectEquals(reloadedConverter->getChipClock(), 2.0 * MHz);
            }
        }

        beginTest("A chip before the converter is not the one it plays on");
        {
            const int converterIndex = track->state.indexOf(converter.state);
            const int chipIndex = track->state.indexOf(chip.state);
            track->state.moveChild(chipIndex, converterIndex, nullptr);
            expectEquals(converter.getChipClock(), zx);
            track->state.moveChild(converterIndex, chipIndex, nullptr);
            expectEquals(converter.getChipClock(), 2.0 * MHz);
        }

        beginTest("Removing the chip returns the converter to the default clock");
        {
            chip.deleteFromParent();
            expectEquals(converter.getChipClock(), zx);
            expectEquals(a4Period(), 252);
        }
    }
};

static NotesToPsgPluginClockTests notesToPsgPluginClockTests;

}  // namespace MoTool::Tests
