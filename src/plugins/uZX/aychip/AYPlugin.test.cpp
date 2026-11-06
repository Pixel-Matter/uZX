#include <JuceHeader.h>

#include "AYPlugin.h"
#include "AYPluginEditor.h"
#include "../../../models/PsgClip.h"
#include "../../../util/TestHelpers.h"

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

        beginTest("Stopped, the scopes keep the last frame that played");
        {
            transport.stop(false, false);
            TestHelpers::processBlocks(engine, 2);
            std::array<float, ScopeBuffer::kBufferSize> onStop {}, later {};
            chip.getVizChannelBuffer(0)->copyTo(onStop.data(), ScopeBuffer::kBufferSize);
            TestHelpers::processBlocks(engine, 10);
            chip.getVizChannelBuffer(0)->copyTo(later.data(), ScopeBuffer::kBufferSize);

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

}  // namespace MoTool::Tests
