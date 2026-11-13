#include <JuceHeader.h>

#include "AYPlugin.h"
#include "AYPluginEditor.h"
#include "../../../gui/common/MidiParameterMapping.h"
#include "../../../models/EditUtilities.h"
#include "../../../models/PsgClip.h"
#include "../../../models/PsgTempoDetector.h"
#include "../../../models/TimerTestBank.h"
#include "../../../util/TestHelpers.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace MoTool::Tests {

using namespace MoTool::uZX;
namespace te = tracktion;

//==============================================================================
/** Plays a PSG clip through the engine's playback graph into the AY plugin
    and checks the registers the chip ends up with. */
class AYChipPluginPlaybackTests : public juce::UnitTest {
public:
    AYChipPluginPlaybackTests() : UnitTest("AYChipPluginPlayback", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto* track = te::getAudioTracks(*edit)[0];

        auto data = makePsgData();
        PsgClip::insertTo(*track, data,{{0_tp, te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}});
        auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
        track->pluginList.insertPlugin(plugin, 0, nullptr);
        auto& chip = dynamic_cast<AYChipPlugin&>(*plugin);

        engine.getDeviceManager().dispatchPendingUpdates();
        auto& transport = edit->getTransport();

        beginTest("Playback started mid-file sets every register to the PSG state at that point");
        {
            transport.setPosition(2_tp);
            transport.play(false);
            TestHelpers::processBlocks(engine, 10);

            auto expected = data.frames[0];
            expected.update(data.frames[50]);
            expectRegisters(chip.getChipRegisters(), expected);
        }

        beginTest("Seeking back while playing overwrites every register changed since");
        {
            transport.setPosition(0.5_tp);
            TestHelpers::processBlocks(engine, 10);

            expectRegisters(chip.getChipRegisters(), data.frames[0]);
        }

        beginTest("Changing the clock or the chip type while playing keeps every register");
        {
            const auto before = chip.getChipRegisters();
            chip.staticParams.chipClock.setStoredValue(1.75);
            expectRegisters(chip.getChipRegisters(), before);
            chip.staticParams.chipType.setStoredValue(ChipType::YM);
            expectRegisters(chip.getChipRegisters(), before);
            TestHelpers::processBlocks(engine, 2);

            chip.staticParams.chipClock.setStoredValue(1.7734);
            chip.staticParams.chipType.setStoredValue(ChipType::AY);
            expectRegisters(chip.getChipRegisters(), before);
        }

        beginTest("Stopped, the scopes keep the last frame that played");
        {
            transport.stop(false, false);
            TestHelpers::processBlocks(engine, 2);
            std::array<float, ScopeBuffer::kBufferSize> onStop {}, later {};
            expectEquals(chip.getVizChannelBuffer(0)->copyTo(onStop.data(), ScopeBuffer::kBufferSize),
                         ScopeBuffer::kBufferSize);
            TestHelpers::processBlocks(engine, 10);
            expectEquals(chip.getVizChannelBuffer(0)->copyTo(later.data(), ScopeBuffer::kBufferSize),
                         ScopeBuffer::kBufferSize);

            expect(std::any_of(onStop.begin(), onStop.end(), [](float x) { return x > 0.0f; }), "A's tone is shown");
            expect(onStop == later, "A's scope holds still");
        }
    }

private:
    void expectRegisters(const PsgRegsFrame& actual, const PsgRegsFrame& expected) {
        for (size_t reg = 0; reg < PsgRegsFrame::size(); ++reg) {
            expectEquals(int(actual.getRaw(reg)), int(expected.getRaw(reg)),
                         juce::String(PsgRegType::getLabel(reg).data()));
        }
    }

    /** Frame 0 sets every register, as a PSG file's first frame does. One second in,
        a frame changes something on each of the four MIDI channels the clip plays on. */
    static PsgData makePsgData() {
        std::vector<PsgRegsFrame> frames(151);
        frames[0] = {
            {PsgRegType::TonePeriodFineA,      0x23}, {PsgRegType::TonePeriodCoarseA, 0x01},
            {PsgRegType::TonePeriodFineB,      0x34}, {PsgRegType::TonePeriodCoarseB, 0x02},
            {PsgRegType::TonePeriodFineC,      0x45}, {PsgRegType::TonePeriodCoarseC, 0x03},
            {PsgRegType::NoisePeriod,          0x11},
            {PsgRegType::Mixer,                0x34},  // tone A, B and noise A on
            {PsgRegType::VolumeA,              0x0F},
            {PsgRegType::VolumeB,              0x10},  // envelope
            {PsgRegType::VolumeC,              0x07},
            {PsgRegType::EnvelopePeriodFine,   0x56}, {PsgRegType::EnvelopePeriodCoarse, 0x04},
            {PsgRegType::EnvelopeShape,        0x0E},
        };
        frames[50] = {
            {PsgRegType::VolumeA,              0x08},
            {PsgRegType::VolumeB,              0x0C},
            {PsgRegType::TonePeriodFineC,      0x22}, {PsgRegType::TonePeriodCoarseC, 0x02},
            {PsgRegType::NoisePeriod,          0x05},
            {PsgRegType::EnvelopePeriodFine,   0x89}, {PsgRegType::EnvelopePeriodCoarse, 0x07},
        };
        frames[150] = {{PsgRegType::VolumeA, 0x09}};  // keeps the clip running past the start point
        return {std::move(frames), {50.0, 1}};
    }

};

static AYChipPluginPlaybackTests ayChipPluginPlaybackTests;

//==============================================================================
/** A clip opened at 50 Hz and switched to 200 Hz plays its frames four times as fast. */
class AYChipFrameRatePlaybackTests : public juce::UnitTest {
public:
    AYChipFrameRatePlaybackTests() : UnitTest("AYChipFrameRatePlayback", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto* track = te::getAudioTracks(*edit)[0];

        // Frame k sets tone A's period to k, so the period tells which frame played last.
        std::vector<PsgRegsFrame> frames(1000);
        frames[0] = {{PsgRegType::Mixer, 0x3E}, {PsgRegType::VolumeA, 0x0F}};
        for (size_t k = 0; k < frames.size(); ++k) {
            frames[k].setTonePeriod(0, static_cast<uint16_t>(k + 1));
        }
        PsgData data {std::move(frames), {50.0, 1}};
        auto clip = PsgClip::insertTo(*track, data, {{0_tp, te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}});
        // Another clip keeps the tempo as it is: only the clip's own change can rebuild playback.
        edit->ensureNumberOfAudioTracks(2);
        PsgClip::insertTo(*te::getAudioTracks(*edit)[1], data, {{0_tp, te::TimeDuration::fromSeconds(1.0)}, {}});
        auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
        track->pluginList.insertPlugin(plugin, 0, nullptr);
        auto& chip = dynamic_cast<AYChipPlugin&>(*plugin);
        Helpers::setProjectFps(*edit, 50.0);
        engine.getDeviceManager().dispatchPendingUpdates();

        // The transport keeps its playback graph, as in the app, so the change has to rebuild it.
        auto& transport = edit->getTransport();
        transport.play(false);
        TestHelpers::processBlocks(engine, 2);
        // The frame playing just after a seek; the seek follows a block, as the app's audio runs on,
        // so a tempo change has moved the playhead to its beat first.
        const auto frameAfterSeekTo = [&](te::TimePosition time) {
            TestHelpers::flushMessageQueue();
            TestHelpers::processBlocks(engine, 1);
            transport.setPosition(time);
            TestHelpers::processBlocks(engine, 2);
            return int(chip.getChipRegisters().getTonePeriod(0)) - 1;  // the frame index
        };

        beginTest("At 50 Hz, one second in plays frame 50");
        {
            const auto frame = frameAfterSeekTo(1_tp);
            expect(frame >= 50 && frame <= 53, "frame " + String(frame));  // up to 60 ms of blocks past the seek
        }

        beginTest("Switched to 200 Hz while playing, two seconds in plays frame 400");
        {
            PsgTempo::setFrameRate({clip.get()}, 200.0);
            const auto frame = frameAfterSeekTo(2_tp);
            expect(frame >= 400 && frame <= 412, "frame " + String(frame));
        }
    }
};

static AYChipFrameRatePlaybackTests ayChipFrameRatePlaybackTests;

//==============================================================================
/** Seeks landing just before a frame: Tracktion chases the clip's controllers at the seek point,
    and neither that state nor the frame right after it may be lost. */
class AYChipSeekStateTests : public juce::UnitTest {
public:
    AYChipSeekStateTests() : UnitTest("AYChipSeekState", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("A frame just after the seek point is not overwritten by the chased state");
        checkSeeksBeforeFrames(engine, 50.0, 2.0);

        beginTest("The chased state survives a frame a fraction of a sample after the seek point");
        checkSeeksBeforeFrames(engine, 48.0, 0.0);  // at 44.1 kHz every 4th 48 fps frame is 0.25 samples late
    }

private:
    /** Frames 1-39 each write VolumeA; frame 50 writes VolumeC; frame 60 keeps the clip running.
        Seeking back from past frame 50
        to `samplesEarly` before frame k must leave VolumeA from frame k and VolumeC from frame 0. */
    void checkSeeksBeforeFrames(te::Engine& engine, double fps, double samplesEarly) {
        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto* track = te::getAudioTracks(*edit)[0];

        std::vector<PsgRegsFrame> frames(61);
        frames[0] = {{PsgRegType::Mixer, 0x38}, {PsgRegType::VolumeA, 0x00}, {PsgRegType::VolumeC, 0x07}};
        for (size_t k = 1; k < 40; ++k)
            frames[k] = {{PsgRegType::VolumeA, static_cast<uint8_t>(k % 16)}};
        frames[50] = {{PsgRegType::VolumeC, 0x03}};
        frames[60] = {{PsgRegType::VolumeB, 0x01}};
        PsgData data {std::move(frames), {fps, 1}};

        PsgClip::insertTo(*track, data, {{0_tp, te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}});
        auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
        track->pluginList.insertPlugin(plugin, 0, nullptr);
        auto& chip = dynamic_cast<AYChipPlugin&>(*plugin);
        engine.getDeviceManager().dispatchPendingUpdates();

        auto& transport = edit->getTransport();
        const auto sampleRate = engine.getDeviceManager().getSampleRate();
        transport.play(false);

        for (int k = 1; k < 40; ++k) {
            transport.setPosition(te::TimePosition::fromSeconds(52.0 / fps));
            TestHelpers::processBlocks(engine, 2);
            expectEquals(int(chip.getChipRegisters().getRaw(PsgRegType::VolumeC)), 0x03, "past frame 50");

            transport.setPosition(te::TimePosition::fromSeconds(k / fps - samplesEarly / sampleRate));
            TestHelpers::processBlocks(engine, 1);  // shorter than a frame
            const auto regs = chip.getChipRegisters();
            expectEquals(int(regs.getRaw(PsgRegType::VolumeA)), k % 16, "VolumeA from frame " + juce::String(k));
            expectEquals(int(regs.getRaw(PsgRegType::VolumeC)), 0x07, "chased VolumeC, seek to frame " + juce::String(k));
        }
        transport.stop(false, false);
    }
};

static AYChipSeekStateTests ayChipSeekStateTests;

//==============================================================================
/** An envelope note across five beats, as one clip and as two, played and sought into.

    beat 0: tone and envelope on A, the shape written
    beat 1: the envelope's pitch changes, no restart
    beat 2: the same shape written again, a restart
    beat 3: the tone's pitch changes; where the second clip starts
    beat 4: the same shape written again, a restart

    The shape decays once and stays silent, so a restart is heard as the level coming back. */
class AYChipEnvelopeRestartTests : public juce::UnitTest {
public:
    AYChipEnvelopeRestartTests() : UnitTest("AYChipEnvelopeRestart", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("The tune plays the same as one clip and as two");
        const auto data = makeTune();
        for (const auto split : {Clips::one, Clips::softSplit, Clips::hardSplit}) {
            const String name = nameOf(split);
            Rig rig(engine, split);
            const auto steps = rig.play(0.0, 5.0);
            expectEquals(restartsAfterBeats(steps), String("R-R-R"), name + ": restarts on beats 0, 2 and 4 only");
            for (int k = 0; k < 5; ++k)
                expectEquals(registersAt(steps, k + 0.5), registersOf(data, k + 0.5), name + ", beat " + String(k));
        }

        beginTest("A seek restarts the envelope where the tune does, or where the chip holds another shape");
        for (const auto split : {Clips::one, Clips::softSplit, Clips::hardSplit}) {
            const String name = nameOf(split);
            Rig played(engine, split);
            played.play(0.0, 5.0);  // leaves the tune's shape in the chip, its envelope run out

            for (const double t : {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5}) {
                const String where = name + ", seek to beat " + String(t, 1);
                const bool tuneRestartsHere = t == 0.0 || t == 2.0 || t == 4.0;

                const auto warm = played.play(t, t + 0.45);
                expect(restartsIn(warm, t, t + 0.2) == tuneRestartsHere, where + ", the chip holding the tune's shape");
                expectEquals(registersAt(warm, t + 0.4), registersOf(data, t + 0.4), where);

                Rig fresh(engine, split);  // a chip after a reset holds another shape
                const auto cold = fresh.play(t, t + 0.45);
                expect(restartsIn(cold, t, t + 0.2), where + ", the chip holding another shape");
                expectEquals(registersAt(cold, t + 0.4), registersOf(data, t + 0.4), where + ", a fresh chip");
            }
        }
    }

private:
    /** One clip; that clip split, each part keeping every frame; or two clips made apart, the second
        holding only its own frames. */
    enum class Clips { one, softSplit, hardSplit };

    static String nameOf(Clips clips) {
        return clips == Clips::one ? "one clip" : clips == Clips::softSplit ? "a split clip" : "two separate clips";
    }

    struct Step {
        double beat;
        float peak;
        PsgRegsFrame regs;
    };

    static constexpr int framesPerBeat = 25;  // 50 Hz at 120 BPM

    static PsgData makeTune() {
        std::vector<PsgRegsFrame> frames(5 * framesPerBeat + 2);
        frames[0] = {
            {PsgRegType::Mixer,              0x3E},  // tone A alone
            {PsgRegType::VolumeA,            0x10},  // the envelope sets its level
            {PsgRegType::TonePeriodFineA,    0xC0}, {PsgRegType::TonePeriodCoarseA, 0x01},
            {PsgRegType::EnvelopePeriodFine, 0x58}, {PsgRegType::EnvelopePeriodCoarse, 0x02},  // 600
            {PsgRegType::EnvelopeShape,      0x09},
        };
        frames[1 * framesPerBeat] = {{PsgRegType::EnvelopePeriodFine, 0x84}, {PsgRegType::EnvelopePeriodCoarse, 0x03}};  // 900
        frames[2 * framesPerBeat] = {{PsgRegType::EnvelopeShape, 0x09}};
        frames[3 * framesPerBeat] = {{PsgRegType::TonePeriodFineA, 0x50}};
        frames[4 * framesPerBeat] = {{PsgRegType::EnvelopeShape, 0x09}};
        frames[5 * framesPerBeat + 1] = {{PsgRegType::VolumeB, 0x00}};  // keeps the clip running to the end
        return {std::move(frames), {50.0, 1}};
    }

    struct Rig {
        Rig(te::Engine& e, Clips clips) : engine(e), edit(te::Edit::createSingleTrackEdit(e)) {
            auto* track = te::getAudioTracks(*edit)[0];
            auto data = makeTune();
            const auto lengthOf = [](const PsgData& d) { return te::TimeDuration::fromSeconds(d.getLengthSeconds()); };
            if (clips == Clips::hardSplit) {
                // The second clip holds nothing from before beat 3. Its first frame states what the chip
                // holds there, the envelope shape too, and retriggers nothing
                PsgRegsFrame held;
                for (int i = 0; i <= 3 * framesPerBeat; ++i)
                    held.update(data.frames[static_cast<size_t>(i)]);
                std::vector<PsgRegsFrame> rest(data.frames.begin() + 3 * framesPerBeat, data.frames.end());
                rest[0] = {};
                rest[1 * framesPerBeat] = {};
                PsgData second {std::move(rest), {50.0, 1}};
                data.frames.resize(3 * framesPerBeat);

                PsgClip::insertTo(*track, data, {{0_tp, lengthOf(data)}, {}});
                auto clip = PsgClip::insertTo(*track, second, {{timeOf(3.0), lengthOf(second)}, {}});
                clip->getPsg().addFrameEvent(0, PsgParamFrameData {held}, nullptr);
                PsgParamFrameData restart;
                restart.set(PsgParamType::RetriggerEnvelope, 1);
                clip->getPsg().addFrameEvent(1 * framesPerBeat, restart, nullptr);
            } else {
                auto clip = PsgClip::insertTo(*track, data, {{0_tp, lengthOf(data)}, {}});
                if (clips == Clips::softSplit)
                    track->splitClip(*clip, timeOf(3.0));
            }
            auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin(plugin, 0, nullptr);
            chip = dynamic_cast<AYChipPlugin*>(plugin.get());
            engine.getDeviceManager().dispatchPendingUpdates();
        }

        ~Rig() { edit->getTransport().stop(false, false); }

        te::TimePosition timeOf(double beat) const {
            return edit->tempoSequence.toTime(te::BeatPosition::fromBeats(beat));
        }

        /** Seeks, plays to `toBeat`, and stops: the level and the chip's registers after every block. */
        std::vector<Step> play(double fromBeat, double toBeat) {
            auto& transport = edit->getTransport();
            transport.setPosition(timeOf(fromBeat));
            transport.play(false);
            auto& audioIO = engine.getDeviceManager().getHostedAudioDeviceInterface();
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            // The block after a seek starts at the seek point
            const auto blockBeats = edit->tempoSequence.toBeats(te::TimePosition::fromSeconds(
                buffer.getNumSamples() / engine.getDeviceManager().getSampleRate())).inBeats();
            std::vector<Step> steps;
            for (double beat = fromBeat; beat < toBeat; beat += blockBeats) {
                buffer.clear();
                audioIO.processBlock(buffer, midi);
                steps.push_back({beat, buffer.getMagnitude(0, buffer.getNumSamples()), chip->getChipRegisters()});
            }
            transport.stop(false, false);
            TestHelpers::processBlocks(engine, 2);
            return steps;
        }

        te::Engine& engine;
        std::unique_ptr<te::Edit> edit;
        AYChipPlugin* chip = nullptr;
    };

    /** The level comes back within a fifth of a beat only if the envelope restarted. */
    static bool restartsIn(const std::vector<Step>& steps, double from, double to) {
        float peak = 0.0f;
        for (const auto& s : steps)
            if (s.beat >= from && s.beat < to)
                peak = jmax(peak, s.peak);
        return peak > 0.02f;
    }

    static String restartsAfterBeats(const std::vector<Step>& steps) {
        String text;
        for (int k = 0; k < 5; ++k)
            text << (restartsIn(steps, k + 0.02, k + 0.2) ? "R" : "-");
        return text;
    }

    static String hex(const PsgRegsFrame& regs) {
        String text;
        for (size_t reg = 0; reg < PsgRegsFrame::size(); ++reg)
            text << String::toHexString(int(regs.getRaw(reg))).paddedLeft('0', 2) << (reg == 5 || reg == 10 ? " " : "");
        return text;
    }

    /** The registers the tune's file has written by `beat`. */
    static String registersOf(const PsgData& data, double beat) {
        PsgRegsFrame regs;
        for (size_t i = 0; i <= static_cast<size_t>(beat * framesPerBeat); ++i)
            regs.update(data.frames[i]);
        return hex(regs);
    }

    static String registersAt(const std::vector<Step>& steps, double beat) {
        for (const auto& s : steps) {
            if (s.beat >= beat)
                return hex(s.regs);
        }
        return "(not reached)";
    }
};

static AYChipEnvelopeRestartTests ayChipEnvelopeRestartTests;

//==============================================================================
class AYChipSoloTests : public juce::UnitTest {
public:
    AYChipSoloTests() : UnitTest("AYChipSolo", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        edit->ensureNumberOfAudioTracks(2);
        auto tracks = te::getAudioTracks(*edit);
        auto addChip = [&](te::AudioTrack& track) -> AYChipPlugin& {
            auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
            track.pluginList.insertPlugin(plugin, 0, nullptr);
            return dynamic_cast<AYChipPlugin&>(*plugin);
        };
        auto& chip1 = addChip(*tracks[0]);
        addChip(*tracks[1]);

        beginTest("A chip with a soloed channel solos its track, so the other tracks go silent");
        {
            chip1.toggleSolo(1);
            chip1.toggleSolo(2);
            expect(tracks[0]->isSolo(false) && ! tracks[1]->isSolo(false));

            chip1.toggleSolo(1);
            expect(tracks[0]->isSolo(false), "C is still soloed");

            chip1.toggleSolo(2);
            expect(! edit->areAnyTracksSolo(), "no solo left");
        }

        beginTest("A channel is on, muted or soloed, and a click turns a muted or soloed one back on");
        {
            AYPluginUI ui {&chip1};
            auto button = [&](const String& text) -> Button* {
                for (auto* child : ui.getChildren())
                    if (auto* b = dynamic_cast<TextButton*>(child); b != nullptr && b->getButtonText() == text)
                        return b;
                return nullptr;
            };
            auto& muter = chip1.channelMuter;

            chip1.toggleSolo(0);  // what Alt-click on A does
            button("A")->triggerClick();
            TestHelpers::flushMessageQueue();
            expect(! muter.hasSolo() && muter.channelA.getStoredValue(), "A unsoloed, not muted");

            button("B")->triggerClick();
            TestHelpers::flushMessageQueue();
            expect(! muter.channelB.getStoredValue(), "a click on a channel that's on mutes it");

            chip1.toggleSolo(1);  // click then Alt-click
            expect(muter.soloB.getStoredValue() && muter.channelB.getStoredValue(), "the muted channel is soloed instead");

            button("B")->triggerClick();
            TestHelpers::flushMessageQueue();
            expect(! muter.hasSolo() && muter.channelB.getStoredValue(), "and a click turns it back on");
        }

        beginTest("Solo is not a step in Undo, as track solo isn't");
        {
            edit->getUndoManager().clearUndoHistory();
            chip1.toggleSolo(0);
            expect(! edit->getUndoManager().canUndo());
            chip1.toggleSolo(0);
        }
    }
};

static AYChipSoloTests ayChipSoloTests;

//==============================================================================
/** Plays bare PSG controllers from a MIDI clip into the chip: a message changes what it names
    and leaves the rest of the registers as the chip holds them. */
class AYChipPluginControllerTests : public juce::UnitTest {
public:
    AYChipPluginControllerTests() : UnitTest("AYChipPluginControllers", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto* track = te::getAudioTracks(*edit)[0];

        const te::TimeRange range {te::TimePosition(), te::TimePosition::fromSeconds(8.0)};
        auto* clip = dynamic_cast<te::MidiClip*>(track->insertNewClip(te::TrackItem::Type::midi, "cc", range, nullptr));
        const auto controller = [&](double beat, MidiCCType type, int value) {
            clip->getSequence().addControllerEvent(te::BeatPosition::fromBeats(beat), static_cast<int>(type), value << 7, nullptr);
        };
        controller(0.0, MidiCCType::Volume, 12);
        controller(0.0, MidiCCType::GPB1ToneSwitch, 1);
        controller(4.0, MidiCCType::GPB3EnvSwitch, 0);

        auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
        track->pluginList.insertPlugin(plugin, 0, nullptr);
        auto& chip = dynamic_cast<AYChipPlugin&>(*plugin);

        engine.getDeviceManager().dispatchPendingUpdates();
        edit->dispatchPendingUpdatesSynchronously();
        auto& transport = edit->getTransport();
        transport.setPosition(te::TimePosition());
        transport.play(false);
        TestHelpers::processBlocks(engine, 10);

        beginTest("Turning one channel's tone on leaves the other mixer switches off, as a reset chip has them");
        {
            const auto regs = chip.getChipRegisters();
            expectEquals(int(regs.getVolume(0)), 12, "the clip reached the chip");
            expect(regs.getToneOn(0), "tone A is on");
            expect(! regs.getNoiseOn(0), "noise A stays off");
            expect(! regs.getToneOn(1) && ! regs.getNoiseOn(1), "tone and noise B stay off");
        }

        beginTest("After a mute, a message for the envelope switch alone does not bring the volume back");
        {
            chip.midiPanic();
            expectEquals(int(chip.getChipRegisters().getVolume(0)), 0, "muted");

            int blocks = 0;
            while (transport.getPosition() < edit->tempoSequence.toTime(te::BeatPosition::fromBeats(4.5)) && ++blocks < 2000)
                TestHelpers::processBlocks(engine, 10);
            expectEquals(int(chip.getChipRegisters().getVolume(0)), 0, "still muted after the envelope switch");
        }
        transport.stop(false, false);
    }
};

static AYChipPluginControllerTests ayChipPluginControllerTests;

//==============================================================================
/** Follows a MIDI-mapped parameter and its live-reader slot through the plugin's deletion. */
class AYChipMappedParameterTests : public juce::UnitTest {
public:
    AYChipMappedParameterTests() : UnitTest("AYChipMappedParameter", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto* track = te::getAudioTracks(*edit)[0];
        auto& mappings = edit->getParameterControlMappings();
        const int cc7 = 0x10000 + 7;

        te::Plugin::Ptr plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
        track->pluginList.insertPlugin(plugin, 0, nullptr);
        auto* chip = dynamic_cast<AYChipPlugin*>(plugin.get());
        auto* volume = plugin->getAutomatableParameterByID("volume").get();
        te::SafeSelectable<te::Plugin> pluginRef(*plugin);
        te::SafeSelectable<te::AutomatableParameter> volumeRef(*volume);

        beginTest("A mapped parameter has one live reader and follows its controller");
        {
            MidiParameterMapping(volume).mapToMidiCC(cc7);
            expect(mappings.isParameterMapped(*volume));
            expectEquals(volume->getReferenceCount(), 2, "Held by the plugin and the mapping");
            expect(chip->dynamicParams.volume.getLiveContext() == volume, "The slot points at the plugin's parameter");

            mappings.sendChange(cc7, 1.0f, 1);
            expectWithinAbsoluteError(chip->dynamicParams.volume.getLiveValue(), 1.0f, 1e-6f);
            TestHelpers::flushMessageQueue();
            expectWithinAbsoluteError(chip->dynamicParams.volume.getStoredValue(), 1.0f, 1e-6f);
        }

        beginTest("The mapping does not keep the parameter alive after its plugin");
        {
            plugin->deleteFromParent();
            plugin = nullptr;
            chip = nullptr;
            juce::MessageManager::getInstance()->runDispatchLoopUntil(1500);  // the plugin cache drops it on a timer

            expect(pluginRef == nullptr, "Plugin destroyed");
            expect(volumeRef == nullptr, "Parameter destroyed with it");
            mappings.sendChange(cc7, 0.0f, 1);  // nothing left to call
        }
    }
};

static AYChipMappedParameterTests ayChipMappedParameterTests;

//==============================================================================
/** Drives the chip block by block with its own MIDI, so a timer's writes can be placed to the sample.

    The chip plays no tone or noise, so channel A's output is its volume: a timer that alternates R8 between
    15 and 9 every 3552 cycles (88.33 samples at 44.1 kHz) is heard as a level that steps at those samples. */
class AYChipTimerTests : public juce::UnitTest {
public:
    AYChipTimerTests() : UnitTest("AYChipTimers", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        const auto bank = makeTestBank();
        const auto registration = TimerBankRegistry::getInstance().add(bank);

        beginTest("A started timer steps the level at whole intervals from its frame, to the sample");
        {
            Rig rig(engine);
            rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
            rig.bank(0.0, registration->getNumber());
            rig.timer(0.0, 0, timerStart(222, 0));
            const auto out = rig.render({4096});

            // 15 from the frame, 9 from 3552 cycles on, 15 again from 7104, ...
            const ChipTime time {1773400, 44100};
            // The level alternates between volume 15 and 9, an interval each. (The chip's output filter delays
            // every write alike by a dozen samples, so only the middle of an interval is looked at here.)
            int wrongMiddles = 0;
            for (uint64_t k = 0; k < 40; ++k) {
                const auto from = static_cast<size_t>(time.sampleAt(k * 3552));
                const auto to = static_cast<size_t>(time.sampleAt((k + 1) * 3552));
                if (std::abs(out[(from + to) / 2] - rig.levelOf(k % 2 == 0 ? 15 : 9)) > 1.0e-3f)
                    ++wrongMiddles;
            }
            expectEquals(wrongMiddles, 0);
            expect(rig.levelOf(15) > rig.levelOf(9) + 0.1f, "the two levels are far apart");

            // Where each step is: the same writes as ordinary frames at the samples the timer's expiries
            // fall on sound exactly the same
            Rig frames(engine);
            frames.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
            for (uint64_t k = 0; time.sampleAt(k * 3552) < 4096; ++k)
                frames.frame(static_cast<double>(time.sampleAt(k * 3552)) / Rig::sampleRate,
                             {{PsgRegType::VolumeA, static_cast<uint8_t>(k % 2 == 0 ? 15 : 9)}});
            expect(frames.render({4096}) == out, "a timer's write sounds from the first sample at or after its cycle");
            const int held = rig.chip().getChipRegisters().getRaw(PsgRegType::VolumeA);
            expect(held == 15 || held == 9, "the chip holds one of the timer's values");
        }

        beginTest("An expiry in a frame's sample but before its cycle comes first: it keeps the interval it had");
        {
            // The timer's fifth expiry is at cycle 17760, in sample 442, whose frame is at cycle 17774. That
            // frame changes the base to 100: the expiry at 17760 was before it, so the next one is a whole old
            // interval on, at 21312, and only the ones after that are 1600 cycles apart
            const ChipTime time {1773400, 44100};
            expect(time.sampleAt(17760) == 442 && time.cycleAt(442) > 17760, "the case this test is about");
            TaymMods faster {};
            faster.command = TAYM_CMD_MODULATE;
            faster.base_timer_value = 100;
            faster.timer_lane_ref = TAYM_TLAN_UNCHANGED;

            Rig rig(engine);
            rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
            rig.bank(0.0, registration->getNumber());
            rig.timer(0.0, 0, timerStart(222, 0));
            rig.timer(442.0 / Rig::sampleRate, 0, faster);
            const auto out = rig.render({2048});

            std::vector<uint64_t> expiries;
            for (uint64_t cycle = 0; cycle <= 21312; cycle += 3552)
                expiries.push_back(cycle);
            for (uint64_t cycle = 21312 + 1600; time.sampleAt(cycle) < 2048; cycle += 1600)
                expiries.push_back(cycle);
            Rig frames(engine);
            frames.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
            for (size_t k = 0; k < expiries.size(); ++k)
                frames.frame(static_cast<double>(time.sampleAt(expiries[k])) / Rig::sampleRate,
                             {{PsgRegType::VolumeA, static_cast<uint8_t>(k % 2 == 0 ? 15 : 9)}});
            expect(frames.render({2048}) == out);
        }

        beginTest("The audio is the same however the time is cut into blocks");
        {
            const auto play = [&](std::vector<int> blocks) {
                Rig rig(engine);
                rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
                rig.bank(0.0, registration->getNumber());
                rig.timer(0.0, 0, timerStart(222, 0));
                rig.timer(0.05, 0, timerStart(100, 0));   // a faster one, started mid-way
                rig.timer(0.07, 0, timerStop());
                return rig.render(std::move(blocks));
            };
            const auto whole = play({4096});
            const auto ragged = play({64, 100, 37, 1, 512, 3, 882, 441, 1024, 1032});
            const auto small = play(std::vector<int>(4096 / 32, 32));
            expectEquals(ragged.size(), whole.size());
            expect(ragged == whole, "ragged blocks");
            expect(small == whole, "32-sample blocks");
        }

        beginTest("A stopped timer gives its register back to the frames");
        {
            Rig rig(engine);
            rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
            rig.bank(0.0, registration->getNumber());
            rig.timer(0.0, 0, timerStart(222, 0));
            rig.frame(0.02, {{PsgRegType::VolumeA, 6}});   // while the timer has R8
            rig.timer(0.04, 0, timerStop());
            const auto out = rig.render({882, 882, 882});

            expect(std::abs(out[882 + 441] - rig.levelOf(6)) > 0.01f, "the frame's volume does not sound under the timer");
            expect(std::abs(out[2 * 882 + 441] - rig.levelOf(6)) < 1.0e-3f, "it sounds once the timer stops");
            expectEquals(int(rig.chip().getChipRegisters().getRaw(PsgRegType::VolumeA)), 6);
        }

        beginTest("A muted channel stays silent under a timer that writes its volume");
        {
            Rig rig(engine);
            rig.chip().channelMuter.getChannelParam(0).setStoredValue(false);
            rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
            rig.bank(0.0, registration->getNumber());
            rig.timer(0.0, 0, timerStart(222, 0));
            const auto out = rig.render({2048});
            float loudest = 0.0f;
            for (size_t i = 200; i < out.size(); ++i)
                loudest = juce::jmax(loudest, std::abs(out[i] - rig.levelOf(0)));
            expect(loudest < 1.0e-3f, "nothing but silence");
        }

        beginTest("A channel's timers switch mutes what timers do to its volume, and the timer comes back with it");
        {
            Rig rig(engine);
            rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 5}});
            rig.bank(0.0, registration->getNumber());
            rig.timer(0.0, 0, timerStart(222, 0));
            rig.render({1024});
            const auto volume = [&] { return int(rig.chip().getChipRegisters().getRaw(PsgRegType::VolumeA)); };
            expect(volume() == 15 || volume() == 9, "the timer has channel A's volume");

            rig.chip().channelMuter.getTimersParam(0).setStoredValue(false);
            const auto muted = rig.render({2048});
            expectEquals(volume(), 5, "switched off: the frames' volume");
            bool steady = true;
            for (size_t i = 600; i < muted.size(); ++i)
                steady = steady && std::abs(muted[i] - rig.levelOf(5)) < 1.0e-3f;
            expect(steady, "and nothing of the timer is heard");

            rig.frame(0.0, {{PsgRegType::VolumeA, 6}});
            rig.render({512});
            expectEquals(volume(), 6, "a frame's write reaches the chip while the timer is muted");

            rig.chip().channelMuter.getTimersParam(0).setStoredValue(true);
            rig.render({1024});
            expect(volume() == 15 || volume() == 9, "switched on again, the timer has it, without a new start");
        }

        beginTest("The chip says which channels a timer drives");
        {
            Rig rig(engine);
            rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 5}, {PsgRegType::VolumeB, 0x10}});  // B on the envelope
            rig.render({512});
            expectEquals(int(rig.chip().getTimerChannels()), 0, "no timer");

            rig.bank(0.0, registration->getNumber());
            rig.timer(0.0, 0, timerStart(222, 0));   // A's volume
            rig.render({512});
            expectEquals(int(rig.chip().getTimerChannels()), 0b001);

            rig.timer(0.0, 1, timerStart(222, 4));   // the envelope's shape, which B uses
            rig.render({512});
            expectEquals(int(rig.chip().getTimerChannels()), 0b011);

            rig.timer(0.0, 0, timerStop());
            rig.render({512});
            expectEquals(int(rig.chip().getTimerChannels()), 0b010);

            rig.allNotesOff();
            rig.render({512});
            expectEquals(int(rig.chip().getTimerChannels()), 0, "play jumped: the timers stopped");
        }

        beginTest("When play jumps or stops, a timer stops and stays silent until its next start");
        {
            Rig rig(engine);
            rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
            rig.bank(0.0, registration->getNumber());
            rig.timer(0.0, 0, timerStart(222, 0));
            rig.render({882});

            // The jump, and the values a host states again where play lands: everything but the start
            rig.allNotesOff();
            TimerEncoder encoder;
            for (const auto& m : encoder.encode(0, timerStart(222, 0)))
                if (static_cast<int>(m.controller) != 83)
                    rig.controller(0.0, m);
            rig.frame(0.0, {{PsgRegType::VolumeA, 5}});
            const auto out = rig.render({2048});
            bool steady = true;
            for (size_t i = 400; i < out.size(); ++i)
                steady = steady && std::abs(out[i] - rig.levelOf(5)) < 1.0e-3f;
            expect(steady, "the level is the frame's, with no timer over it");

            rig.timer(0.0, 0, timerStart(222, 0));
            const auto again = rig.render({2048});
            expect(std::abs(again[44] - rig.levelOf(15)) < 1.0e-3f && std::abs(again[132] - rig.levelOf(9)) < 1.0e-3f,
                   "the next start runs it again");
        }

        beginTest("A timer with no bank chosen, or a bank that is gone, writes nothing, and the chip says so");
        {
            Rig rig(engine);
            rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
            rig.render({512});
            expectEquals(int(rig.chip().getTimerWarnings()), 0, "a chip with no timers has nothing to warn of");

            rig.timer(0.0, 0, timerStart(222, 0));
            rig.render({1024});
            expectEquals(int(rig.chip().getChipRegisters().getRaw(PsgRegType::VolumeA)), 4, "no bank");
            expectEquals(int(rig.chip().getTimerWarnings()), int(AYChipPlugin::timerCommandNoBank));

            rig.bank(0.0, 16000);  // a number nothing is registered under
            rig.timer(0.0, 0, timerStart(222, 0));
            rig.render({1024});
            expectEquals(int(rig.chip().getChipRegisters().getRaw(PsgRegType::VolumeA)), 4, "a bank that is gone");
            expectEquals(int(rig.chip().getTimerWarnings()), int(AYChipPlugin::timerBankNotFound),
                         "the bank is what is wrong, not each command after it");
            expect(AYChipPlugin::describeTimerWarnings(rig.chip().getTimerWarnings()).isNotEmpty());

            rig.bank(0.0, registration->getNumber());
            rig.timer(0.0, 0, timerStart(222, 0));
            rig.render({1024});
            expectEquals(int(rig.chip().getTimerWarnings()), 0, "a bank that is found ends the warning");
            const int held = rig.chip().getChipRegisters().getRaw(PsgRegType::VolumeA);
            expect(held == 15 || held == 9, "and the timer runs");
        }

        beginTest("A command its bank cannot play is ignored, the rest plays, and the warning stays");
        {
            Rig rig(engine);
            rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
            rig.bank(0.0, registration->getNumber());
            rig.timer(0.0, 0, timerStart(222, 99));  // an action the bank does not have
            rig.timer(0.0, 5, timerStart(222, 0));   // a timer the chip's bank does not have
            rig.render({1024});
            expectEquals(int(rig.chip().getChipRegisters().getRaw(PsgRegType::VolumeA)), 4, "nothing started");
            expectEquals(int(rig.chip().getTimerWarnings()), int(AYChipPlugin::timerCommandIgnored));

            rig.timer(0.0, 1, timerStart(222, 0));   // a good one
            rig.render({1024});
            const int held = rig.chip().getChipRegisters().getRaw(PsgRegType::VolumeA);
            expect(held == 15 || held == 9, "a command that can be played is");
            expectEquals(int(rig.chip().getTimerWarnings()), int(AYChipPlugin::timerCommandIgnored), "the warning stays to be seen");

            rig.chip().reset();
            expectEquals(int(rig.chip().getTimerWarnings()), 0, "until the chip is reset");
        }

        beginTest("The chip's panel shows a warning sign while there is something to warn of, with the reason");
        {
            Rig rig(engine);
            AYPluginUI panel {te::Plugin::Ptr(&rig.chip())};
            panel.setSize(260, 360);
            const auto findSign = [&]() -> juce::Label* {
                for (auto* child : panel.getChildren())
                    if (auto* label = dynamic_cast<juce::Label*>(child); label != nullptr && label->getText() == juce::String::fromUTF8("\xE2\x9A\xA0"))
                        return label;
                return nullptr;
            };
            auto* sign = findSign();
            expect(sign != nullptr);
            if (sign != nullptr) {
                juce::MessageManager::getInstance()->runDispatchLoopUntil(400);
                expect(! sign->isVisible(), "nothing to warn of");

                rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
                rig.bank(0.0, 16000);
                rig.timer(0.0, 0, timerStart(222, 0));
                rig.render({1024});
                juce::MessageManager::getInstance()->runDispatchLoopUntil(400);
                expect(sign->isVisible(), "a bank that is not there");
                expect(sign->getTooltip().isNotEmpty());
                expect(sign->getRight() <= panel.getWidth() - 8 && sign->getY() == 8, "at the right of the chip row");

                rig.bank(0.0, registration->getNumber());
                rig.render({512});
                juce::MessageManager::getInstance()->runDispatchLoopUntil(400);
                expect(! sign->isVisible(), "the bank is found");
            }
        }

        beginTest("The panel has a timers switch per channel while the track has a clip with timers, and a stopwatch on the scope of a channel a timer drives");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            auto* track = te::getAudioTracks(*edit)[0];
            PsgData data {std::vector<PsgRegsFrame>(50), {50.0, 1}};
            data.frames[0] = {{PsgRegType::Mixer, 0x3F}};
            data.frames[49] = {{PsgRegType::VolumeB, 0}};
            const auto clip = PsgClip::insertTo(*track, data, {{te::TimePosition(), te::TimeDuration::fromSeconds(1.0)}, {}});
            auto chipPlugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin(chipPlugin, 0, nullptr);
            Rig rig(engine, chipPlugin);

            AYPluginUI panel {chipPlugin};
            panel.setSize(260, 420);
            const auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil(400); };
            const auto switchesShown = [&] {
                int shown = 0;
                for (auto* child : panel.getChildren())
                    if (child->getComponentID() == "timers" && child->isVisible())
                        ++shown;
                return shown;
            };
            AYScopes* scopes = nullptr;
            for (auto* child : panel.getChildren())
                if (auto* found = dynamic_cast<AYScopes*>(child))
                    scopes = found;
            expect(scopes != nullptr);
            // True if the sign's place on a channel's scope has pixels of the channel's colour that bright
            const auto signShown = [&] (size_t chan, Colour colour, float minBrightness) {
                const auto image = panel.createComponentSnapshot(panel.getLocalBounds(), true, 1.0f);
                const auto area = scopes->getTimerSignBounds(chan) + scopes->getPosition();
                for (int y = area.getY(); y < area.getBottom(); ++y)
                    for (int x = area.getX(); x < area.getRight(); ++x)
                        if (const auto pixel = image.getPixelAt(x, y);
                            std::abs(pixel.getHue() - colour.getHue()) < 0.03f && pixel.getSaturation() > 0.3f
                            && pixel.getBrightness() >= minBrightness)
                            return true;
                return false;
            };

            pump();
            expectEquals(switchesShown(), 0, "a track of plain PSG clips has none");
            clip->getTimers().set(makeTestTaym(), 0, {{10, 0, timerStart(222, 0)}}, nullptr);
            pump();
            expectEquals(switchesShown(), 3, "one under each channel's T, N and E");
            if (scopes != nullptr) {
                expect(! signShown(0, Colors::PSG::A, 0.5f), "no timer runs: no sign");

                rig.frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, 4}});
                rig.bank(0.0, registration->getNumber());
                rig.timer(0.0, 0, timerStart(222, 0));
                rig.render({512});
                pump();
                expect(signShown(0, Colors::PSG::A, 0.6f), "a stopwatch on A's scope, in A's colour");
                expect(! signShown(1, Colors::PSG::B, 0.3f), "none on B's");

                rig.chip().channelMuter.getTimersParam(0).setStoredValue(false);
                rig.render({512});
                pump();
                expect(! signShown(0, Colors::PSG::A, 0.6f) && signShown(0, Colors::PSG::A, 0.1f), "dimmed while A's timers are switched off");
            }
            clip->getTimers().set({}, 0, {}, nullptr);
            pump();
            expectEquals(switchesShown(), 0, "gone with the clip's timers");
        }
    }

private:
    /** A chip outside any playback graph, fed blocks of MIDI. */
    struct Rig {
        explicit Rig(te::Engine& engine)
            : Rig(engine, {})
        {}

        /** With `existing`, drives that chip: one on a track of an edit of the caller's. */
        Rig(te::Engine& engine, te::Plugin::Ptr existing)
            : edit(existing == nullptr ? te::Edit::createSingleTrackEdit(engine) : nullptr)
            , plugin(existing != nullptr ? existing : edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {}))
        {
            chip().staticParams.removeDC.setStoredValue(false);
            chip().staticParams.chipClock.setStoredValue(1.7734);
            chip().dynamicParams.layout.setStoredValue(LayoutEnum::ABC);  // A alone on the left
            chip().dynamicParams.stereoWidth.setStoredValue(1.0f);
            plugin->baseClassInitialise({te::TimePosition(), sampleRate, 4096});
            // The levels a volume gives, by playing each alone
            for (int volume : {0, 5, 6, 9, 15}) {
                frame(0.0, {{PsgRegType::Mixer, 0x3F}, {PsgRegType::VolumeA, static_cast<uint8_t>(volume)}});
                levels[static_cast<size_t>(volume)] = render({2048}).back();
            }
            plugin->baseClassDeinitialise();
            plugin->baseClassInitialise({te::TimePosition(), sampleRate, 4096});
        }

        ~Rig() { plugin->baseClassDeinitialise(); }

        AYChipPlugin& chip() { return dynamic_cast<AYChipPlugin&>(*plugin); }
        float levelOf(int volume) const { return levels[static_cast<size_t>(volume)]; }

        /** A message at `seconds` from where the next render starts, on a channel counted from the chip's. */
        void controller(double seconds, const PsgCC::Message& m) {
            pending.push_back({seconds, juce::MidiMessage::controllerEvent(1 + m.channel, static_cast<int>(m.controller), m.value)});
        }

        void frame(double seconds, const PsgRegsFrame& regs) {
            const PsgParamFrameData params {regs};
            PsgParamsMidiWriter writer {1};
            writer.write(seconds, params);
            for (const auto* event : writer.getSequence())
                pending.push_back({seconds, event->message});
        }

        void bank(double seconds, int number) {
            for (const auto& m : TimerCC::encodeBank(number))
                controller(seconds, m);
        }

        void timer(double seconds, int index, const TaymMods& command) {
            for (const auto& m : encoder.encode(index, command))
                controller(seconds, m);
        }

        void allNotesOff() { jump = true; }

        /** Renders blocks of these sizes and returns the left channel. Pending messages go into the block
            their time falls in. */
        std::vector<float> render(std::vector<int> blocks) {
            std::stable_sort(pending.begin(), pending.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            std::vector<float> out;
            double blockStart = 0.0;
            size_t next = 0;
            for (int size : blocks) {
                const double blockEnd = blockStart + size / sampleRate;
                te::MidiMessageArray midi;
                midi.isAllNotesOff = std::exchange(jump, false);
                for (; next < pending.size() && pending[next].first < blockEnd - 1.0e-9; ++next)
                    midi.addMidiMessage(pending[next].second, juce::jmax(0.0, pending[next].first - blockStart), 0);
                juce::AudioBuffer<float> buffer(2, size);
                buffer.clear();
                const te::PluginRenderContext fc(&buffer, 0, size, &midi, 0.0, {}, true, false, false, false);
                plugin->applyToBuffer(fc);
                out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + size);
                blockStart = blockEnd;
            }
            pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(next));
            for (auto& [time, message] : pending)
                time -= blockStart;
            return out;
        }

        static constexpr double sampleRate = 44100.0;
        std::unique_ptr<te::Edit> edit;
        te::Plugin::Ptr plugin;
        TimerEncoder encoder;
        std::vector<std::pair<double, juce::MidiMessage>> pending;
        std::array<float, 16> levels {};
        bool jump = false;
    };
};

static AYChipTimerTests ayChipTimerTests;

}  // namespace MoTool::Tests
