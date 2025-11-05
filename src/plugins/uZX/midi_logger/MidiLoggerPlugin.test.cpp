#include <JuceHeader.h>
#include <sstream>

#include "MidiLoggerPlugin.h"


namespace MoTool::Tests {

using namespace MoTool::uZX;
namespace te = tracktion;

class MidiLoggerPluginTests : public juce::UnitTest {
public:
    MidiLoggerPluginTests()
        : juce::UnitTest("MidiLoggerPlugin", "MoTool") {}

    void runTest() override {
        beginTest("Logs incoming MIDI messages to the configured stream");
        {
            auto& engine = *te::Engine::getEngines()[0];
            auto edit = te::Edit::createSingleTrackEdit(engine);

            juce::ValueTree pluginState { te::IDs::PLUGIN };
            pluginState.setProperty(te::IDs::type, MidiLoggerPlugin::xmlTypeName, nullptr);

            MidiLoggerPlugin plugin({ *edit, pluginState, true });

            te::PluginInitialisationInfo initialiseInfo {
                te::TimePosition::fromSeconds(0.0),
                48000.0,
                128
            };
            plugin.initialise(initialiseInfo);

            tracktion::MidiMessageArray midiBuffer;
            auto src = tracktion::createUniqueMPESourceID();
            midiBuffer.addMidiMessage(juce::MidiMessage::noteOn(1, 60, (uint8)100), 0.0, src);
            midiBuffer.addMidiMessage(juce::MidiMessage::noteOff(1, 60), 0.1, src);
            midiBuffer.addMidiMessage(juce::MidiMessage::controllerEvent(2, 64, 96), 0.2, src);

            plugin.setLogTag("preA");
            std::ostringstream capture;
            plugin.setOutputStream(capture);

            te::PluginRenderContext context(
                nullptr,
                juce::AudioChannelSet(),
                0,
                initialiseInfo.blockSizeSamples,
                &midiBuffer,
                0.0,
                te::TimeRange(),
                true,
                false,
                false,
                false);

            plugin.applyToBuffer(context);
            plugin.deinitialise();

            const juce::String output { capture.str() };
            expect(output.containsIgnoreCase("note on"), "Should log note on events");
            expect(output.containsIgnoreCase("note off"), "Should log note off events");
            expect(output.containsIgnoreCase("controller"), "Should log controller events");
            expect(output.contains("[ch 1"), "Should include channel information");
            expect(output.contains("[ch 2"), "Should handle multiple channels");
            expect(output.contains("[preA]"), "Should include configured tag");
        }
    }
};

static MidiLoggerPluginTests midiLoggerPluginTests;

}  // namespace MoTool::Tests
