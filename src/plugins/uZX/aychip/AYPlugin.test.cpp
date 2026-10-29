#include <JuceHeader.h>

#include "AYPlugin.h"
#include "../../../models/PsgClip.h"

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
            processBlocks(engine, 10);

            auto expected = data.frames[0];
            expected.update(data.frames[50]);
            expectRegisters(chip.getChipRegisters(), expected);
        }

        beginTest("Seeking back while playing overwrites every register changed since");
        {
            transport.setPosition(0.5_tp);
            processBlocks(engine, 10);

            expectRegisters(chip.getChipRegisters(), data.frames[0]);
        }

        transport.stop(false, false);
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

    static void processBlocks(te::Engine& engine, int numBlocks) {
        auto& audioIO = engine.getDeviceManager().getHostedAudioDeviceInterface();
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        for (int i = 0; i < numBlocks; ++i) {
            buffer.clear();
            audioIO.processBlock(buffer, midi);
        }
    }
};

static AYChipPluginPlaybackTests ayChipPluginPlaybackTests;

}  // namespace MoTool::Tests
