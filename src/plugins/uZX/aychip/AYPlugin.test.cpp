#include <JuceHeader.h>

#include "AYPlugin.h"
#include "AYPluginEditor.h"
#include "../../../gui/common/MidiParameterMapping.h"
#include "../../../models/EditUtilities.h"
#include "../../../models/PsgClip.h"
#include "../../../models/PsgTempoDetector.h"
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
        auto plugin = edit->getPluginCache().createNewPlugin(AYChipPlugin::xmlTypeName, {});
        track->pluginList.insertPlugin(plugin, 0, nullptr);
        auto& chip = dynamic_cast<AYChipPlugin&>(*plugin);
        Helpers::setProjectFps(*edit, 50.0);
        engine.getDeviceManager().dispatchPendingUpdates();

        const auto periodAfterOneSecond = [&] {
            auto& transport = edit->getTransport();
            transport.stop(false, false);
            TestHelpers::flushMessageQueue();
            transport.setPosition(1_tp);
            transport.play(false);
            TestHelpers::processBlocks(engine, 1);
            return int(chip.getChipRegisters().getTonePeriod(0)) - 1;  // the frame index
        };

        beginTest("At 50 Hz, one second in plays frame 50");
        {
            const auto frame = periodAfterOneSecond();
            expect(frame >= 50 && frame <= 51, "frame " + String(frame));
        }

        beginTest("Switched to 200 Hz, one second in plays frame 200");
        {
            PsgTempo::setFrameRate({clip.get()}, 200.0);
            const auto frame = periodAfterOneSecond();
            expect(frame >= 200 && frame <= 203, "frame " + String(frame));
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

}  // namespace MoTool::Tests
