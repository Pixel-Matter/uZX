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

        beginTest("The MPE chord example starts as a cluster and plays a major seventh from two seconds on");
        {
            // scripts/make_mpe_fixture.py: C3 and B3 in a clip in MPE mode, E4 with its envelope on and G2
            // driving the envelope on channels of their own, each starting bent toward A3 and gliding into tune
            auto edit = te::loadEditFromFile(engine, juce::File(UZX_FIXTURES_DIR).getChildFile("MPE chord.uzx"));
            NotesToPsgPlugin* converter = nullptr;
            AYChipPlugin* chip = nullptr;
            for (auto* p : te::getAudioTracks(*edit)[0]->pluginList) {
                if (auto* c = dynamic_cast<NotesToPsgPlugin*>(p))
                    converter = c;
                if (auto* c = dynamic_cast<AYChipPlugin*>(p))
                    chip = c;
            }
            expect(converter != nullptr && chip != nullptr);
            if (converter == nullptr || chip == nullptr)
                return;
            // Asked for anew each time: the converter makes its tuning again when playback starts
            const auto period = [&](double note, TuningSystem::PeriodMode mode = TuningSystem::Tone) {
                return converter->getTuningSystem().midiNoteToPeriod(note, mode);
            };
            const auto tonePeriods = [&] {
                const auto regs = chip->getChipRegisters();
                juce::Array<int> periods;
                for (int n = 0; n < 3; ++n)
                    if (regs.getVolume(n) > 0)
                        periods.addUsingDefaultSort(int(regs.getTonePeriod(n)));
                return periods;
            };

            engine.getDeviceManager().dispatchPendingUpdates();
            edit->dispatchPendingUpdatesSynchronously();
            edit->getTransport().setPosition(te::TimePosition());
            edit->getTransport().play(false);

            TestHelpers::processBlocks(engine, 8);  // about a tenth of a second in
            auto periods = tonePeriods();
            expectEquals(periods.size(), 3, "three voices on the chip's channels");
            if (periods.size() == 3) {
                expect(periods[0] > period(64.0 - 5.0) && periods[0] < period(57.0), "E4 starts over five semitones flat");
                expect(periods[1] > period(59.0 - 1.0) && periods[1] < period(57.0), "B3 over one flat");
                expect(periods[2] < period(48.0 + 7.0) && periods[2] > period(57.0), "C3 over seven sharp");
            }
            expect(int(chip->getChipRegisters().getEnvelopePeriod()) < period(43.0 + 10.0, TuningSystem::Envelope),
                   "and the envelope over ten sharp: all four within a few semitones of A3");

            TestHelpers::processBlocks(engine, 252);  // past two seconds, inside the four
            periods = tonePeriods();
            expect(periods == juce::Array<int>{period(64.0), period(59.0),
                                               period(48.0)},
                   "E4, B3 and C3 in tune");
            const auto regs = chip->getChipRegisters();
            expectEquals(int(regs.getEnvelopePeriod()), period(43.0, TuningSystem::Envelope),
                         "the envelope at G2, on no chip channel of its own");
            expectEquals(int(regs.getEnvelopeShape()), 12, "as a sawtooth");
            int shaped = 0;
            for (int n = 0; n < 3; ++n)
                if (regs.getEnvMod(n)) {
                    ++shaped;
                    expectEquals(int(regs.getTonePeriod(n)), period(64.0), "E4 is the tone it shapes");
                    expect(regs.getToneOn(n), "with its tone on");
                }
            expectEquals(shaped, 1, "one tone has its envelope on");
            for (int n = 0; n < 3; ++n)
                expectEquals(int(regs.getVolume(n)), 15, "every voice at the chip's full volume");
            edit->getTransport().stop(false, false);

            // To listen to the example: UZX_RENDER_TO=<file.wav> uZXTests NotesToPsgPluginPlayback
            if (const juce::File wavFile(juce::SystemStats::getEnvironmentVariable("UZX_RENDER_TO", {}));
                wavFile != juce::File()) {
                constexpr int blockSize = 512;
                const double sampleRate = engine.getDeviceManager().getSampleRate();
                const int blocks = int(4.4 * sampleRate / blockSize);
                juce::AudioBuffer<float> all(2, blocks * blockSize), block(2, blockSize);
                juce::MidiBuffer midi;
                TestHelpers::processBlocks(engine, 4);  // the stop
                edit->getTransport().setPosition(te::TimePosition());
                edit->getTransport().play(false);
                for (int i = 0; i < blocks; ++i) {
                    block.clear();
                    engine.getDeviceManager().getHostedAudioDeviceInterface().processBlock(block, midi);
                    for (int ch = 0; ch < 2; ++ch)
                        all.copyFrom(ch, i * blockSize, block, ch, 0, blockSize);
                }
                edit->getTransport().stop(false, false);
                wavFile.deleteFile();
                juce::WavAudioFormat wav;
                if (std::unique_ptr<juce::AudioFormatWriter> writer {
                        wav.createWriterFor(new juce::FileOutputStream(wavFile), sampleRate, 2, 16, {}, 0)})
                    writer->writeFromAudioSampleBuffer(all, 0, all.getNumSamples());
            }
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
            sequence.addControllerEvent(te::BeatPosition(), 2, 36 << 7, nullptr);    // noise period 9 of 32
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
