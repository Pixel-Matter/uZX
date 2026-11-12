#include "NotesToPsgPlugin.h"

#include <JuceHeader.h>

#include "../aychip/AYPlugin.h"
#include "../instrument/ChipInstrumentPlugin.h"
#include "../../../util/TestHelpers.h"

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

//==============================================================================
/** Plays a held note from a MIDI clip through the converter into an AY chip
    and checks the registers the chip ends up with. */
class NotesToPsgPluginPlaybackTests : public juce::UnitTest
{
public:
    NotesToPsgPluginPlaybackTests() : UnitTest("NotesToPsgPluginPlayback", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("A chip that replaces the first one plays the next note");
        {
            Rig rig(engine);
            rig.play();
            expectNoteOnChannelA(rig, "the first chip");

            rig.edit->getTransport().stop(false, false);
            TestHelpers::processBlocks(engine, 2);
            rig.chip->deleteFromParent();
            rig.addChip();
            rig.play();
            expectNoteOnChannelA(rig, "the chip that replaced it");
        }

        beginTest("Moving the converter to other MIDI channels silences the note it holds");
        {
            Rig rig(engine);
            rig.play();
            expectNoteOnChannelA(rig, "before the move");

            rig.converter->staticParams.baseMidiChannel.setStoredValue(5);
            TestHelpers::processBlocks(engine, 4);
            expectEquals(int(rig.chip->getChipRegisters().getVolume(0)), 0, "channel A's volume after the move");
        }

        beginTest("A note panned right plays on the chip channel that the chip's layout puts on the right");
        {
            const auto volumes = [](Rig& rig) {
                const auto regs = rig.chip->getChipRegisters();
                return juce::String(regs.getVolume(0) > 0 ? "A" : "") + (regs.getVolume(1) > 0 ? "B" : "")
                       + (regs.getVolume(2) > 0 ? "C" : "");
            };
            Rig acb(engine);
            acb.clip->getSequence().addControllerEvent(te::BeatPosition(), 10, 127 << 7, nullptr);
            acb.play();
            expectEquals(volumes(acb), juce::String("B"), "the chip's own layout, ACB");

            Rig abc(engine);
            abc.clip->getSequence().addControllerEvent(te::BeatPosition(), 10, 127 << 7, nullptr);
            abc.chip->dynamicParams.layout.setStoredValue(ChannelsLayout::ABC);
            abc.play();
            expectEquals(volumes(abc), juce::String("C"), "after the chip is set to ABC");
        }

        beginTest("A chord and its controllers on one MIDI channel reach the chip through the instrument");
        {
            Rig rig(engine);
            auto instrument = rig.edit->getPluginCache().createNewPlugin(ChipInstrumentPlugin::xmlTypeName, {});
            rig.track->pluginList.insertPlugin(instrument, 0, nullptr);

            auto& sequence = rig.clip->getSequence();
            sequence.addNote(64, te::BeatPosition(), te::BeatDuration::fromBeats(8.0), 100, 0, nullptr);
            sequence.addNote(67, te::BeatPosition(), te::BeatDuration::fromBeats(8.0), 100, 0, nullptr);
            sequence.addControllerEvent(te::BeatPosition(), 81, 127 << 7, nullptr);  // noise on
            sequence.addControllerEvent(te::BeatPosition(), 2, 9 << 7, nullptr);     // noise period
            rig.play();

            const auto regs = rig.chip->getChipRegisters();
            auto& tuning = rig.converter->getTuningSystem();
            juce::Array<int> periods;
            for (int n = 0; n < 3; ++n) {
                periods.addUsingDefaultSort(int(regs.getTonePeriod(n)));
                expect(regs.getVolume(n) > 0 && regs.getToneOn(n) && regs.getNoiseOn(n),
                       "chip channel " + juce::String(n) + " plays a note with the noise on");
            }
            expect(periods == juce::Array<int>{tuning.midiNoteToPeriod(67.0), tuning.midiNoteToPeriod(64.0),
                                               tuning.midiNoteToPeriod(60.0)},
                   "the three notes, one on each chip channel");
            expectEquals(int(regs.getNoisePeriod()), 9);
        }
    }

private:
    static constexpr int note = 60;

    /** A track with a clip holding one long note on MIDI channel 1, the converter and a chip after it. */
    struct Rig {
        explicit Rig(te::Engine& e) : engine(e), edit(te::Edit::createSingleTrackEdit(e)) {
            track = te::getAudioTracks(*edit)[0];
            const te::TimeRange range {te::TimePosition(), te::TimePosition::fromSeconds(4.0)};
            clip = dynamic_cast<te::MidiClip*>(track->insertNewClip(te::TrackItem::Type::midi, "note", range, nullptr));
            clip->getSequence().addNote(note, te::BeatPosition(), te::BeatDuration::fromBeats(8.0), 100, 0, nullptr);

            auto plugin = edit->getPluginCache().createNewPlugin(NotesToPsgPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin(plugin, 0, nullptr);
            converter = dynamic_cast<NotesToPsgPlugin*>(plugin.get());
            addChip();
        }

        void addChip() {
            auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin(plugin, 1, nullptr);
            chip = dynamic_cast<AYChipPlugin*>(plugin.get());
        }

        /** Plays from the start to a point inside the note. */
        void play() {
            engine.getDeviceManager().dispatchPendingUpdates();
            edit->dispatchPendingUpdatesSynchronously();
            auto& transport = edit->getTransport();
            transport.setPosition(te::TimePosition());
            transport.play(false);
            TestHelpers::processBlocks(engine, 10);
        }

        te::Engine& engine;
        std::unique_ptr<te::Edit> edit;
        te::AudioTrack* track = nullptr;
        te::MidiClip* clip = nullptr;
        NotesToPsgPlugin::Ptr converter;
        juce::ReferenceCountedObjectPtr<AYChipPlugin> chip;
    };

    void expectNoteOnChannelA(Rig& rig, const juce::String& where) {
        const auto regs = rig.chip->getChipRegisters();
        expect(regs.getVolume(0) > 0, "volume on " + where);
        expect(regs.getToneOn(0), "tone on " + where);
        expectEquals(int(regs.getTonePeriod(0)),
                     rig.converter->getTuningSystem().midiNoteToPeriod(note, TuningSystem::PeriodMode::Tone),
                     "tone period on " + where);
    }
};

static NotesToPsgPluginPlaybackTests notesToPsgPluginPlaybackTests;

}  // namespace MoTool::Tests
