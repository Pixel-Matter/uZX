#include <JuceHeader.h>

#include "TaymFile.h"
#include "../../models/PsgClip.h"
#include "../../models/TimerCC.h"
#include "../../models/TimerTestBank.h"
#include "../../plugins/uZX/aychip/AYPlugin.h"
#include "../../plugins/uZX/aychip/ChipTimers.h"
#include "../../util/Helpers.h"
#include "../../util/TestHelpers.h"

#include <map>
#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <vector>

namespace MoTool::Tests {

using namespace MoTool::uZX;
namespace te = tracktion;

namespace {

constexpr uint64_t frameCycles = 1773400 / 50;

/** A chip that is only its registers. */
struct RegisterChip {
    std::array<uint8_t, 14> regs {};
    uint8_t getRegister(size_t index) const noexcept { return regs[index]; }
    void setRegister(size_t index, uint8_t value) noexcept { regs[index] = value; }
};

using CommandsByFrame = std::map<int, ChipTimers::Commands>;

/** Every register write the timers make over `frames` frames, given each frame's commands. */
std::vector<TaymTimerWrite> runTimers(const TimerBank::Ptr& bank, const CommandsByFrame& commands, int frames) {
    ChipTimers timers;
    timers.setClock(1773400);
    timers.setBank(bank);
    RegisterChip chip;
    std::vector<TaymTimerWrite> all;
    for (int frame = 0; frame < frames; ++frame) {
        if (const auto found = commands.find(frame); found != commands.end())
            timers.applyFrame(found->second, {}, static_cast<uint64_t>(frame) * frameCycles, chip);
        for (bool more = true; more;) {
            std::span<const TaymTimerWrite> writes;
            more = timers.collect(static_cast<uint64_t>(frame + 1) * frameCycles, false, writes);
            all.insert(all.end(), writes.begin(), writes.end());
        }
    }
    return all;
}

bool sameWrites(const std::vector<TaymTimerWrite>& a, const std::vector<TaymTimerWrite>& b) {
    return a.size() == b.size()
        && std::equal(a.begin(), a.end(), b.begin(), [](const TaymTimerWrite& x, const TaymTimerWrite& y) {
               return x.cycle == y.cycle && x.target == y.target && x.value == y.value;
           });
}

/** The timer commands a clip's playback MIDI carries, by frame, read as the chip reads them.
    `firstFrame` is the frame at the sequence's time 0. */
CommandsByFrame commandsOnTheWire(PsgClip& clip, int firstFrame, int& bankSeen) {
    const auto sequence = clip.createPlaybackMidiSequence(te::MidiList::TimeBase::seconds);
    const int baseChannel = clip.getPsg().getMidiChannel().getChannelNumber();
    TimerDecoder decoder;
    CommandsByFrame byFrame;
    double frameTime = -1.0;

    const auto endFrame = [&] {
        if (decoder.isEmpty())
            return;
        if (const auto bank = decoder.takeBank())
            bankSeen = *bank;
        const auto commands = decoder.takeCommands();
        if (std::any_of(commands.begin(), commands.end(), [](const TaymMods& m) { return m.command != TAYM_CMD_EMPTY; }))
            byFrame[firstFrame + juce::roundToInt(frameTime * 50.0)] = commands;
    };
    for (const auto* event : sequence) {
        const auto& m = event->message;
        if (m.getTimeStamp() > frameTime) {  // the sequence is in time order
            endFrame();
            frameTime = m.getTimeStamp();
        }
        if (m.isController())
            decoder.decode(m.getChannel() - baseChannel, m.getControllerNumber(), m.getControllerValue());
    }
    endFrame();
    return byFrame;
}

juce::File exampleTune() {
    return juce::File(UZX_TAYM_EXAMPLES_DIR).getChildFile("atarized.taym");
}

}  // namespace

//==============================================================================
class TaymFileTests : public juce::UnitTest {
public:
    TaymFileTests() : UnitTest("TaymFile", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        juce::MemoryBlock bytes;
        const bool loaded = exampleTune().loadFileAsData(bytes);
        const auto tune = readTaymModule(bytes);

        beginTest("A TAYM reads as a tune: its chip's frames, clock and title, and its timers' commands");
        {
            expect(loaded, "the TAYM repository's example tune is there");
            expect(tune.has_value());
            if (tune) {
                expectEquals(tune->chips.size(), size_t {1});
                expectEquals(tune->chips[0].frames.size(), size_t {9477});
                expectEquals(tune->chips[0].getFrameRate(), 50.0);
                expectEquals(tune->title, juce::String("atarized"));
                expect(tune->chipClock == std::optional<double> {1773400.0});
                expect(tune->chipType.has_value() && *tune->chipType == ChipType {TypeEnum::AY});
                expectEquals(tune->timers.size(), size_t {1});
                // 396 starts, 5401 modulates and 26 stops, as `python -m taym stats` counts them
                expectEquals(tune->timers[0].events.size(), size_t {396 + 5401 + 26});
                expect(! tune->timers[0].unplayable);
                const auto bank = TimerBank::fromTaym(tune->timers[0].bank.getData(), tune->timers[0].bank.getSize(), 0);
                expect(bank != nullptr && bank->getNumTimers() == 3, "the bank reads back with the chip's three timers");
                expect(tune->timers[0].bank.getSize() < bytes.getSize() / 4, "without the file's frames and commands");
            }
        }

        beginTest("A PSG's repeated writes are kept, as a PSG file's are");
        {
            // The tune's frame data writes the envelope shape 109 times, twice with the shape it had
            int shapeWrites = 0;
            if (tune)
                for (const auto& frame : tune->chips[0].frames)
                    shapeWrites += frame.hasEnvelopeShapeSet() ? 1 : 0;
            expectEquals(shapeWrites, 109);
        }

        beginTest("A pause in the frame data counts in fours or in ones, whichever fills the track");
        {
            // A frame, a pause of 3, a frame: 14 frames if a pause counts in fours, 5 if in ones
            const std::vector<uint8_t> psg {'P', 'S', 'G', 0x1A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                            0xFF, 8, 15, 0xFE, 3, 0xFF, 8, 7, 0xFD};
            const auto lastVolume = [&](uint32_t frames) {
                const auto read = readTaymModule(makeTestTaym(frames, {}, psg));
                expect(read.has_value() && read->chips[0].frames.size() == frames);
                int at = -1;
                if (read)
                    for (size_t i = 0; i < read->chips[0].frames.size(); ++i)
                        if (read->chips[0].frames[i].hasVolumeOrEnvModSet(0) && read->chips[0].frames[i].getVolume(0) == 7)
                            at = static_cast<int>(i);
                return at;
            };
            expectEquals(lastVolume(14), 13, "in fours, as a PSG file has it");
            expectEquals(lastVolume(5), 4, "in ones, as TAYM's own tools write it");
            expectEquals(lastVolume(9), -1, "neither fits: fours, and the frame past the track's end is cut off");
        }

        beginTest("Other bytes are not a TAYM");
        {
            expect(! readTaymModule(juce::MemoryBlock("PSG\x1a and so on", 14)).has_value());
            expect(! readTaymModule(juce::MemoryBlock("TAYM", 4)).has_value(), "the magic alone");
            juce::MemoryBlock cut(bytes.getData(), bytes.getSize() / 2);
            expect(! readTaymModule(cut).has_value(), "half a file");
            expect(! looksLikeTaym({}));
        }

        if (! tune)
            return;
        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto* track = te::getAudioTracks(*edit)[0];
        auto data = tune->chips[0];
        const int frames = static_cast<int>(data.frames.size());
        const auto clip = PsgClip::insertTo(*track, data, {{te::TimePosition(), te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}});
        clip->getTimers().set(tune->timers[0].bank, tune->timers[0].chipIndex, tune->timers[0].events, nullptr);
        const auto bank = clip->getTimers().getBank();

        CommandsByFrame inFile;
        for (const auto& event : tune->timers[0].events)
            inFile[event.frameIndex][static_cast<size_t>(event.timer)] = event.command;

        beginTest("A clip's playback MIDI runs the timers exactly as the file's commands do");
        {
            expect(bank != nullptr);
            expect(clip->getTimers().getBankNumber() > 0);
            int bankSeen = 0;
            const auto onTheWire = commandsOnTheWire(*clip, 0, bankSeen);
            expectEquals(bankSeen, clip->getTimers().getBankNumber(), "the clip's bank is chosen");

            const auto expected = runTimers(bank, inFile, frames);
            const auto played = runTimers(bank, onTheWire, frames);
            expect(expected.size() > 150000, "the tune's timers write all through it: " + juce::String(expected.size()));
            expectEquals(played.size(), expected.size());
            expect(sameWrites(played, expected), "every write, at its cycle");
        }

        beginTest("The tune's timers come out as spans with the pitch and duty of what they write");
        {
            // Frame 798: timer 0 is modulated to timer lane 34 (166, 428) with R8 from lane 7 (12, 0), and its
            // timers count the clock in eights: a repeat of 8 * (166 + 428) cycles, 12 for 166 of each 594
            const auto* span = clip->getTimers().findSpan(0, 798);
            expect(span != nullptr);
            if (span != nullptr) {
                expectEquals(span->startFrame, 798);
                expectEquals(span->endFrame, 799);
                expect(! span->starts && span->channels == 1 && span->timerLane == 34);
                expectWithinAbsoluteError(span->loopCycles, 8.0 * 594.0, 1.0e-9);
                expectWithinAbsoluteError(double(span->duty), 166.0 / 594.0, 1.0e-6);
            }
            size_t spans = 0;
            for (int timer = 0; timer < clip->getTimers().getNumTimers(); ++timer)
                spans += clip->getTimers().getSpans(timer).size();
            expectEquals(spans, size_t {396 + 5401}, "one for every start and modulate");
        }

        beginTest("The timers are in the clip's state: they survive a save and a copy");
        {
            const auto xml = clip->state.toXmlString();
            const auto restored = juce::ValueTree::fromXml(xml);
            ClipTimers timers {restored};
            timers.reload();
            expectEquals(timers.getEvents().size(), tune->timers[0].events.size());
            expect(timers.getBank() != nullptr);
            expect(timers.getBankNumber() != clip->getTimers().getBankNumber(), "a copy has a bank number of its own");
            bool same = timers.getEvents().size() == tune->timers[0].events.size();
            for (size_t i = 0; same && i < timers.getEvents().size(); ++i) {
                const auto& a = timers.getEvents()[i];
                const auto& b = tune->timers[0].events[i];
                same = a.frameIndex == b.frameIndex && a.timer == b.timer && a.command.command == b.command.command
                    && a.command.base_timer_value == b.command.base_timer_value
                    && a.command.timer_lane_ref == b.command.timer_lane_ref
                    && a.command.first_action == b.command.first_action && a.command.action_count == b.command.action_count;
            }
            expect(same, "every command as it was");
        }

        beginTest("A clip trimmed at its start plays the commands from there, each timer from its next start");
        {
            const int cut = 1000;  // 20 seconds in
            clip->setStart(te::TimePosition::fromSeconds(cut / 50.0), true, false);

            CommandsByFrame fromTheCut;
            std::array<bool, TimerCC::maxTimers> started {};
            for (const auto& event : tune->timers[0].events) {
                if (event.frameIndex < cut)
                    continue;
                const auto timer = static_cast<size_t>(event.timer);
                if (event.command.command == TAYM_CMD_START)
                    started[timer] = true;
                if (event.command.command == TAYM_CMD_MODULATE && ! started[timer])
                    continue;  // its timer was started before the cut
                fromTheCut[event.frameIndex][timer] = event.command;
            }

            int bankSeen = 0;
            const auto onTheWire = commandsOnTheWire(*clip, cut, bankSeen);
            expectEquals(bankSeen, clip->getTimers().getBankNumber());
            expect(! onTheWire.empty() && onTheWire.begin()->first >= cut, "nothing from before the cut");
            const auto expected = runTimers(bank, fromTheCut, frames);
            expect(expected.size() > 100000);
            expect(sameWrites(runTimers(bank, onTheWire, frames), expected));
        }

        beginTest("A clip that loses its timers is a plain PSG clip again");
        {
            clip->getTimers().set({}, 0, {}, nullptr);
            expect(clip->getTimers().isEmpty() && clip->getTimers().getBankNumber() == 0);
            expect(! clip->state.getChildWithName(MoTool::IDs::TIMERS).isValid());
            int bankSeen = 0;
            expect(commandsOnTheWire(*clip, 0, bankSeen).empty());
            expectEquals(bankSeen, 0);
        }
    }
};

static TaymFileTests taymFileTests;

//==============================================================================
/** A TAYM opened as any tune is, and played through the engine into the chip. */
class TaymImportTests : public juce::UnitTest {
public:
    TaymImportTests() : UnitTest("TaymImport", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("A TAYM file is a tune file, and opens on a track with its timers, clock and chip type");
        {
            expect(Helpers::isTuneFile(exampleTune()));
            expect(Helpers::getTuneFileWildcard().contains("*.taym"));

            auto edit = te::Edit::createSingleTrackEdit(engine);
            const auto clips = Helpers::importPsgFiles(*edit, {exampleTune()});
            expectEquals(clips.size(), 1);
            if (clips.size() == 1) {
                expectEquals(clips[0]->getTimers().getEvents().size(), size_t {396 + 5401 + 26});
                expect(clips[0]->getTimers().getBank() != nullptr);
                expectEquals(clips[0]->getName(), juce::String("atarized"));
                auto* chip = dynamic_cast<te::AudioTrack*>(clips[0]->getTrack())->pluginList.findFirstPluginOfType<AYChipPlugin>();
                expect(chip != nullptr);
                if (chip != nullptr) {
                    expectWithinAbsoluteError(chip->staticParams.chipClock.getStoredValue(), 1.7734, 1.0e-6);
                    expect(chip->staticParams.chipType.getStoredValue() == ChipType {TypeEnum::AY});
                }
            }
        }

        beginTest("A project saved with a TAYM's clip opens with its timers");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            expectEquals(Helpers::importPsgFiles(*edit, {exampleTune()}).size(), 1);
            edit->flushState();
            const auto file = juce::File::createTempFile(".uzx");
            expect(file.replaceWithText(edit->state.toXmlString()));

            const auto loaded = te::loadEditFromFile(engine, file);
            PsgClip* found = nullptr;
            for (auto* track : te::getAudioTracks(*loaded))
                for (auto* clip : track->getClips())
                    if (auto* psg = dynamic_cast<PsgClip*>(clip))
                        found = psg;
            expect(found != nullptr);
            if (found != nullptr) {
                expectEquals(found->getTimers().getEvents().size(), size_t {396 + 5401 + 26});
                expect(found->getTimers().getBank() != nullptr && found->getTimers().getBank()->getNumTimers() == 3);
                expect(found->getTimers().getBankNumber() > 0);
                int bankSeen = 0;
                expect(! commandsOnTheWire(*found, 0, bankSeen).empty(), "and plays them");
                expectEquals(bankSeen, found->getTimers().getBankNumber());
            }
            file.deleteFile();
        }

        beginTest("Giving a clip timers is one undo step");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            juce::MemoryBlock bytes;
            exampleTune().loadFileAsData(bytes);
            const auto tune = readTaymModule(bytes);
            expect(tune.has_value());
            if (tune) {
                auto data = tune->chips[0];
                const auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                                    {{te::TimePosition(), te::TimeDuration::fromSeconds(10.0)}, {}});
                auto& um = edit->getUndoManager();
                um.beginNewTransaction();
                clip->getTimers().set(tune->timers[0].bank, 0, tune->timers[0].events, &um);
                expect(! clip->getTimers().isEmpty());
                um.undo();
                expect(clip->getTimers().isEmpty() && clip->getTimers().getBankNumber() == 0, "undone");
                um.redo();
                expectEquals(clip->getTimers().getEvents().size(), tune->timers[0].events.size(), "redone");
                expect(clip->getTimers().getBankNumber() > 0);
            }
        }

        beginTest("Played through the engine, a timer writes its register between the clip's frames and gives it back");
        {
            // Twenty frames of a timer on R8, started at frame 5 and stopped at frame 25, over frames that
            // set the volume to 3 once
            constexpr int frames = 50;
            std::vector<TaymMods> commands(static_cast<size_t>(frames) * 2);
            commands[5 * 2] = timerStart(222, 0);
            commands[25 * 2] = timerStop();
            const auto taym = makeTestTaym(frames, commands);
            const auto tune = readTaymModule(taym);
            expect(tune.has_value() && tune->timers.size() == 1 && tune->timers[0].events.size() == 2);
            if (! tune)
                return;

            auto edit = te::Edit::createSingleTrackEdit(engine);
            auto* track = te::getAudioTracks(*edit)[0];
            auto data = tune->chips[0];
            data.frames[0] = {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 3}};
            data.frames[frames - 1] = {{PsgRegType::VolumeB, 1}};  // keeps the clip running to its end
            const auto clip = PsgClip::insertTo(*track, data, {{te::TimePosition(), te::TimeDuration::fromSeconds(1.0)}, {}});
            clip->getTimers().set(tune->timers[0].bank, 0, tune->timers[0].events, nullptr);
            auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin(plugin, 0, nullptr);
            auto& chip = dynamic_cast<AYChipPlugin&>(*plugin);
            engine.getDeviceManager().dispatchPendingUpdates();
            edit->dispatchPendingUpdatesSynchronously();

            // The transport's position is not read here: it follows the audio only as the message thread
            // lets it. Blocks are counted from where play starts
            auto& transport = edit->getTransport();
            const double sampleRate = engine.getDeviceManager().getSampleRate();
            const auto playFor = [&](double seconds) {
                TestHelpers::processBlocks(engine, juce::roundToInt(std::ceil(seconds * sampleRate / 512.0)));
                return int(chip.getChipRegisters().getRaw(PsgRegType::VolumeA));
            };

            transport.setPosition(te::TimePosition());
            transport.play(false);
            expectEquals(playFor(0.04), 3, "before the timer, the frame's volume");
            const int under = playFor(0.26);  // 0.3 seconds in: the timer runs from 0.1 to 0.5
            expect(under == 15 || under == 9, "under the timer, one of its two values: " + juce::String(under));
            expectEquals(playFor(0.35), 3, "once it stops, the frame's volume again");

            // Play that lands inside the timer's run: the timer stays silent, and the chip has the frame's volume
            TestHelpers::flushMessageQueue();
            TestHelpers::processBlocks(engine, 1);
            transport.setPosition(te::TimePosition::fromSeconds(0.2));
            expectEquals(playFor(0.1), 3, "landed inside the timer's run, the timer is silent");
            transport.stop(false, false);
        }

        beginTest("The example tune plays, and plays on after a jump, with nothing for the chip to warn of");
        {
            juce::MemoryBlock bytes;
            exampleTune().loadFileAsData(bytes);
            const auto tune = readTaymModule(bytes);
            expect(tune.has_value());
            if (tune) {
                auto edit = te::Edit::createSingleTrackEdit(engine);
                auto* track = te::getAudioTracks(*edit)[0];
                auto data = tune->chips[0];
                const auto clip = PsgClip::insertTo(*track, data, {{te::TimePosition(), te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}});
                clip->getTimers().set(tune->timers[0].bank, tune->timers[0].chipIndex, tune->timers[0].events, nullptr);
                auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
                track->pluginList.insertPlugin(plugin, 0, nullptr);
                auto& chip = dynamic_cast<AYChipPlugin&>(*plugin);
                engine.getDeviceManager().dispatchPendingUpdates();
                edit->dispatchPendingUpdatesSynchronously();

                auto& transport = edit->getTransport();
                transport.setPosition(te::TimePosition());
                transport.play(false);
                TestHelpers::processBlocks(engine, 300);  // three and a half seconds
                expectEquals(int(chip.getTimerWarnings()), 0, "from the start");
                TestHelpers::flushMessageQueue();
                transport.setPosition(te::TimePosition::fromSeconds(20.0));  // inside running timers
                TestHelpers::processBlocks(engine, 300);
                expectEquals(int(chip.getTimerWarnings()), 0, "after a jump");
                transport.stop(false, false);
            }
        }

        // To listen to the example tune as the engine plays it:
        //     UZX_RENDER_TAYM_TO=<file.wav> uZXTests TaymImport
        // UZX_RENDER_TAYM_SECONDS=<n> sets how much of it (30); UZX_RENDER_TAYM_TIMERS=0 leaves the timers out.
        if (const juce::File wavFile(juce::SystemStats::getEnvironmentVariable("UZX_RENDER_TAYM_TO", {})); wavFile != juce::File())
            renderExample(engine, wavFile);
    }

private:
    void renderExample(te::Engine& engine, const juce::File& wavFile) {
        juce::MemoryBlock bytes;
        exampleTune().loadFileAsData(bytes);
        const auto tune = readTaymModule(bytes);
        if (! tune)
            return;
        const double seconds = juce::SystemStats::getEnvironmentVariable("UZX_RENDER_TAYM_SECONDS", "30").getDoubleValue();
        const bool withTimers = juce::SystemStats::getEnvironmentVariable("UZX_RENDER_TAYM_TIMERS", "1").getIntValue() != 0;

        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto* track = te::getAudioTracks(*edit)[0];
        auto data = tune->chips[0];
        const auto clip = PsgClip::insertTo(*track, data, {{te::TimePosition(), te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}});
        if (withTimers)
            clip->getTimers().set(tune->timers[0].bank, tune->timers[0].chipIndex, tune->timers[0].events, nullptr);
        auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
        track->pluginList.insertPlugin(plugin, 0, nullptr);
        engine.getDeviceManager().dispatchPendingUpdates();
        edit->dispatchPendingUpdatesSynchronously();

        constexpr int blockSize = 512;
        const double sampleRate = engine.getDeviceManager().getSampleRate();
        const int blocks = int(seconds * sampleRate / blockSize);
        juce::AudioBuffer<float> all(2, blocks * blockSize), block(2, blockSize);
        juce::MidiBuffer midi;
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
};

static TaymImportTests taymImportTests;

}  // namespace MoTool::Tests
