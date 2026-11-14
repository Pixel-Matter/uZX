#include <JuceHeader.h>

#include "TaymFile.h"
#include "../../models/PsgClip.h"
#include "../../models/TimerCC.h"
#include "../../models/TimerTestBank.h"
#include "../../plugins/uZX/aychip/AYPlugin.h"
#include "../../plugins/uZX/aychip/ChipTimers.h"
#include "../../util/Helpers.h"
#include "../../util/TestHelpers.h"

#include <cstring>
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
    return juce::File(UZX_FIXTURES_DIR).getChildFile("taym/atarized.taym");
}

/** A TAYM of the test bank's chip and a second, a YM at 2 MHz with no timers, on the same frames. */
juce::MemoryBlock makeTwoChipTaym() {
    const std::vector<uint8_t> psg {'P', 'S', 'G', 0x1A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                    0xFF, 7, 0x3E, 8, 15, 0xFF, 0xFF, 0xFF, 0xFF, 0xFD};
    const auto one = makeTestTaym(5, {}, psg);
    juce::MemoryBlock two;
    Taym taym;
    taym_init(&taym);
    if (taym_read_bytes(static_cast<const uint8_t*>(one.getData()), one.getSize(), &taym) == TAYM_OK) {
        // Each chip's frames in a chunk of its own: the format has no two chips share one
        TaymChunk chunks[2] {*taym_find_chunk(&taym, taym.chips[0].frame_data_tag), {}};
        chunks[1] = chunks[0];
        std::memcpy(chunks[1].tag, "PSG1", 4);
        TaymChip chips[2] {taym.chips[0], taym.chips[0]};
        std::memcpy(chips[1].frame_data_tag, "PSG1", 4);
        chips[1].clock_hz = 2000000;
        chips[1].variant = TAYM_AY_VARIANT_YM;
        auto both = taym;
        both.chips = chips;
        both.chip_count = 2;
        both.chunks = chunks;
        both.chunk_count = 2;
        uint8_t* bytes = nullptr;
        size_t size = 0;
        if (taym_write_bytes(&both, &bytes, &size) == TAYM_OK) {
            two.append(bytes, size);
            taym_free_bytes(bytes);
        }
    }
    taym_free(&taym);
    return two;
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
            expect(loaded, "the example tune is there");
            expect(tune.has_value());
            if (tune) {
                expectEquals(tune->chips.size(), size_t {1});
                expectEquals(tune->chips[0].psg.frames.size(), size_t {9477});
                expectEquals(tune->chips[0].psg.getFrameRate(), 50.0);
                expectEquals(tune->title, juce::String("atarized"));
                expect(tune->chips[0].clock == std::optional<double> {1773400.0});
                expect(tune->chips[0].type.has_value() && *tune->chips[0].type == ChipType {TypeEnum::AY});
                // 396 starts, 5401 modulates and 26 stops, as `python -m taym stats` counts them
                expectEquals(tune->chips[0].timers.events.size(), size_t {396 + 5401 + 26});
                expect(! tune->chips[0].timers.unplayable);
                const auto bank = TimerBank::fromTaym(tune->chips[0].timers.bank.getData(), tune->chips[0].timers.bank.getSize(), 0);
                expect(bank != nullptr && bank->getNumTimers() == 3, "the bank reads back with the chip's three timers");
                expect(tune->chips[0].timers.bank.getSize() < bytes.getSize() / 4, "without the file's frames and commands");
            }
        }

        beginTest("A PSG's repeated writes are kept, as a PSG file's are");
        {
            // The tune's frame data writes the envelope shape 109 times, twice with the shape it had
            int shapeWrites = 0;
            if (tune)
                for (const auto& frame : tune->chips[0].psg.frames)
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
                expect(read.has_value() && read->chips[0].psg.frames.size() == frames);
                int at = -1;
                if (read)
                    for (size_t i = 0; i < read->chips[0].psg.frames.size(); ++i)
                        if (read->chips[0].psg.frames[i].hasVolumeOrEnvModSet(0) && read->chips[0].psg.frames[i].getVolume(0) == 7)
                            at = static_cast<int>(i);
                return at;
            };
            expectEquals(lastVolume(14), 13, "in fours, as a PSG file has it");
            expectEquals(lastVolume(5), 4, "in ones, as TAYM's own tools write it");
            expectEquals(lastVolume(9), -1, "neither fits: fours, and the frame past the track's end is cut off");
        }

        beginTest("Each chip has its own clock and type");
        {
            const auto tune = readTaymModule(makeTwoChipTaym());
            expect(tune.has_value() && tune->chips.size() == 2);
            if (tune && tune->chips.size() == 2) {
                expect(tune->chips[0].clock == std::optional<double> {1773400.0});
                expect(tune->chips[0].type == std::optional<ChipType> {ChipType {TypeEnum::AY}});
                expect(tune->chips[1].clock == std::optional<double> {2000000.0}, "the second chip's, not the first's");
                expect(tune->chips[1].type == std::optional<ChipType> {ChipType {TypeEnum::YM}});
                expect(tune->chips[1].timers.events.empty(), "the timers are the first chip's");
            }
        }

        beginTest("A modulate that names some of its timer's targets is given a slice that names all of them");
        {
            // Timer 0 on A's volume from lane 1 and B's from lane 0; then A's alone made 7, twice
            constexpr uint32_t frames = 10;
            std::vector<TaymMods> commands(frames * 2);
            commands[1 * 2] = timerStart(222, 2, 2);
            TaymMods aAlone {};
            aAlone.command = TAYM_CMD_MODULATE;
            aAlone.timer_lane_ref = TAYM_TLAN_UNCHANGED;
            aAlone.first_action = 1;  // A's volume, always 7
            aAlone.action_count = 1;
            commands[3 * 2] = aAlone;
            commands[5 * 2] = aAlone;
            const auto tune = readTaymModule(makeTestTaym(frames, commands));
            expect(tune.has_value() && tune->chips[0].timers.events.size() == 3);
            if (tune && tune->chips[0].timers.events.size() == 3) {
                const auto& timers = tune->chips[0].timers;
                const auto& modulate = timers.events[1].command;
                expectEquals(int(modulate.action_count), 2, "both targets");
                expect(modulate.first_action >= 6, "in actions added after the file's");
                expect(timers.events[2].command.first_action == modulate.first_action, "added once for the same slice");

                // Stated again where play resumes, with what the wire holds of it, the timer has both targets:
                // A's from the modulate, B's from the start
                TimerCC::Values held {};
                for (const auto& event : timers.events)
                    held = TimerCC::apply(held, event.command);
                const auto bank = TimerBank::fromTaym(timers.bank.getData(), timers.bank.getSize(), 0);
                expect(bank != nullptr);
                if (bank != nullptr) {
                    TaymTimerBank engine {};
                    taym_timers_init(&engine, &bank->getTables(), bank->getTimerClocks(), bank->getNumTimers(), bank->getClockHz());
                    const auto resumed = timerStart(held.base, static_cast<uint32_t>(held.actionFirst),
                                                    static_cast<uint8_t>(held.actionCount));
                    expect(taym_timers_command(&engine, 0, &resumed, 0) == TAYM_OK);
                    const auto& targets = engine.timers[0].targets;
                    expect(engine.timers[0].target_count == 2
                           && targets[0].target_id == 8 && ! targets[0].has_lane && targets[0].inline_value == 7
                           && targets[1].target_id == 9 && targets[1].has_lane && targets[1].lane_index == 0);
                }
            }
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
        auto data = tune->chips[0].psg;
        const int frames = static_cast<int>(data.frames.size());
        const auto clip = PsgClip::insertTo(*track, data, {{te::TimePosition(), te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}});
        clip->getTimers().set(tune->chips[0].timers.bank, tune->chips[0].timers.chipIndex, tune->chips[0].timers.events, nullptr);
        const auto bank = clip->getTimers().getBank();

        CommandsByFrame inFile;
        for (const auto& event : tune->chips[0].timers.events)
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
            expectEquals(timers.getEvents().size(), tune->chips[0].timers.events.size());
            expect(timers.getBank() != nullptr);
            expect(timers.getBankNumber() != clip->getTimers().getBankNumber(), "a copy has a bank number of its own");
            bool same = timers.getEvents().size() == tune->chips[0].timers.events.size();
            for (size_t i = 0; same && i < timers.getEvents().size(); ++i) {
                const auto& a = timers.getEvents()[i];
                const auto& b = tune->chips[0].timers.events[i];
                same = a.frameIndex == b.frameIndex && a.timer == b.timer && a.command.command == b.command.command
                    && a.command.base_timer_value == b.command.base_timer_value
                    && a.command.timer_lane_ref == b.command.timer_lane_ref
                    && a.command.first_action == b.command.first_action && a.command.action_count == b.command.action_count;
            }
            expect(same, "every command as it was");
        }

        beginTest("A clip trimmed at its start plays the commands from there, and starts the timers running there");
        {
            const int cut = 1000;  // 20 seconds in
            clip->setStart(te::TimePosition::fromSeconds(cut / 50.0), true, false);

            // A timer running at the cut starts there with what it holds, from its lanes' first elements, as
            // it does where play resumes inside its run
            CommandsByFrame fromTheCut;
            std::array<TimerCC::Values, TimerCC::maxTimers> atTheCut {};
            for (const auto& event : tune->chips[0].timers.events) {
                const auto timer = static_cast<size_t>(event.timer);
                if (event.frameIndex <= cut)
                    atTheCut[timer] = TimerCC::apply(atTheCut[timer], event.command);
                else
                    fromTheCut[event.frameIndex][timer] = event.command;
            }
            int running = 0;
            for (size_t timer = 0; timer < atTheCut.size(); ++timer) {
                if (const auto& held = atTheCut[timer]; held.on) {
                    fromTheCut[cut][timer] = timerStart(held.base, static_cast<uint32_t>(held.actionFirst),
                                                        static_cast<uint8_t>(held.actionCount),
                                                        held.lane == 0 ? TAYM_TLAN_NONE : static_cast<uint32_t>(held.lane - 1));
                    ++running;
                }
            }
            expect(running > 0, "the case this test is about");

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

        beginTest("A TAYM's chips open on tracks of their own, each with its chip's clock and type");
        {
            const auto file = juce::File::createTempFile(".taym");
            const auto bytes = makeTwoChipTaym();
            expect(file.replaceWithData(bytes.getData(), bytes.getSize()));
            auto edit = te::Edit::createSingleTrackEdit(engine);
            const auto clips = Helpers::importPsgFiles(*edit, {file});
            expectEquals(clips.size(), 2);
            if (clips.size() == 2) {
                const auto chipOf = [](PsgClip* clip) {
                    return dynamic_cast<te::AudioTrack*>(clip->getTrack())->pluginList.findFirstPluginOfType<AYChipPlugin>();
                };
                auto* first = chipOf(clips[0]);
                auto* second = chipOf(clips[1]);
                expect(first != nullptr && second != nullptr && first != second);
                if (first != nullptr && second != nullptr) {
                    expectWithinAbsoluteError(first->staticParams.chipClock.getStoredValue(), 1.7734, 1.0e-6);
                    expect(first->staticParams.chipType.getStoredValue() == ChipType {TypeEnum::AY});
                    expectWithinAbsoluteError(second->staticParams.chipClock.getStoredValue(), 2.0, 1.0e-6);
                    expect(second->staticParams.chipType.getStoredValue() == ChipType {TypeEnum::YM});
                }
            }
            file.deleteFile();
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
                auto data = tune->chips[0].psg;
                const auto clip = PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data,
                                                    {{te::TimePosition(), te::TimeDuration::fromSeconds(10.0)}, {}});
                auto& um = edit->getUndoManager();
                um.beginNewTransaction();
                clip->getTimers().set(tune->chips[0].timers.bank, 0, tune->chips[0].timers.events, &um);
                expect(! clip->getTimers().isEmpty());
                um.undo();
                expect(clip->getTimers().isEmpty() && clip->getTimers().getBankNumber() == 0, "undone");
                um.redo();
                expectEquals(clip->getTimers().getEvents().size(), tune->chips[0].timers.events.size(), "redone");
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
            expect(tune.has_value() && tune->chips.size() == 1 && tune->chips[0].timers.events.size() == 2);
            if (! tune)
                return;

            auto edit = te::Edit::createSingleTrackEdit(engine);
            auto* track = te::getAudioTracks(*edit)[0];
            auto data = tune->chips[0].psg;
            data.frames[0] = {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 3}};
            data.frames[frames - 1] = {{PsgRegType::VolumeB, 1}};  // keeps the clip running to its end
            const auto clip = PsgClip::insertTo(*track, data, {{te::TimePosition(), te::TimeDuration::fromSeconds(1.0)}, {}});
            clip->getTimers().set(tune->chips[0].timers.bank, 0, tune->chips[0].timers.events, nullptr);
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

            // Play that lands inside the timer's run: the timer starts there
            TestHelpers::flushMessageQueue();
            TestHelpers::processBlocks(engine, 1);
            transport.setPosition(te::TimePosition::fromSeconds(0.2));
            const int landed = playFor(0.1);
            expect(landed == 15 || landed == 9, "landed inside the timer's run, the timer plays: " + juce::String(landed));
            expectEquals(playFor(0.3), 3, "and stops where the tune stops it");

            // Paused, and resumed inside the run. The transport's position does not follow the audio here, so
            // the place play stopped at is set
            transport.stop(false, false);
            TestHelpers::flushMessageQueue();
            TestHelpers::processBlocks(engine, 4);
            transport.setPosition(te::TimePosition::fromSeconds(0.3));
            TestHelpers::flushMessageQueue();
            transport.play(false);
            const int resumed = playFor(0.05);
            expect(resumed == 15 || resumed == 9, "resumed inside the timer's run, the timer plays: " + juce::String(resumed));
            transport.stop(false, false);
        }

        beginTest("A plain PSG clip after a clip with timers stops them where it begins");
        {
            constexpr int frames = 50;
            std::vector<TaymMods> commands(static_cast<size_t>(frames) * 2);
            commands[5 * 2] = timerStart(222, 0);  // never stopped
            const auto tune = readTaymModule(makeTestTaym(frames, commands));
            expect(tune.has_value());
            if (! tune)
                return;

            auto edit = te::Edit::createSingleTrackEdit(engine);
            auto* track = te::getAudioTracks(*edit)[0];
            const auto second = te::TimeDuration::fromSeconds(1.0);
            auto data = tune->chips[0].psg;
            data.frames[0] = {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 3}};
            data.frames[frames - 1] = {{PsgRegType::VolumeB, 1}};  // keeps the clip running to its end
            PsgClip::insertTo(*track, data, {{te::TimePosition(), second}, {}})
                ->getTimers().set(tune->chips[0].timers.bank, 0, tune->chips[0].timers.events, nullptr);
            auto plain = data;
            plain.frames[0] = {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 5}};
            PsgClip::insertTo(*track, plain, {{te::TimePosition() + second, second}, {}});
            auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin(plugin, 0, nullptr);
            auto& chip = dynamic_cast<AYChipPlugin&>(*plugin);
            engine.getDeviceManager().dispatchPendingUpdates();
            edit->dispatchPendingUpdatesSynchronously();

            auto& transport = edit->getTransport();
            const double sampleRate = engine.getDeviceManager().getSampleRate();
            const auto playFor = [&](double seconds) {
                TestHelpers::processBlocks(engine, juce::roundToInt(std::ceil(seconds * sampleRate / 512.0)));
                return int(chip.getChipRegisters().getRaw(PsgRegType::VolumeA));
            };
            transport.setPosition(te::TimePosition::fromSeconds(0.8));
            transport.play(false);
            const int under = playFor(0.1);
            expect(under == 15 || under == 9, "the timer runs in its clip: " + juce::String(under));
            expectEquals(playFor(0.3), 5, "the plain clip's volume, with no timer over it");
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
                auto data = tune->chips[0].psg;
                const auto clip = PsgClip::insertTo(*track, data, {{te::TimePosition(), te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}});
                clip->getTimers().set(tune->chips[0].timers.bank, tune->chips[0].timers.chipIndex, tune->chips[0].timers.events, nullptr);
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
        auto data = tune->chips[0].psg;
        const auto clip = PsgClip::insertTo(*track, data, {{te::TimePosition(), te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}});
        if (withTimers)
            clip->getTimers().set(tune->chips[0].timers.bank, tune->chips[0].timers.chipIndex, tune->chips[0].timers.events, nullptr);
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
