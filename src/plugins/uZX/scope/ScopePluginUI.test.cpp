#include <JuceHeader.h>
#include "ScopePluginUI.h"

namespace MoTool::uZX {

class ScopeMinMaxColumnsTest : public juce::UnitTest {
public:
    ScopeMinMaxColumnsTest() : UnitTest("ScopeMinMaxColumns", "MoTool") {}

    void runTest() override {
        beginTest("One span per column, also when the samples don't divide evenly");
        {
            std::vector<float> samples(1024, 0.0f);
            expectEquals(static_cast<int>(minMaxColumns(samples.data(), 1024, 312).size()), 312);
        }

        beginTest("A one-sample peak that skipping samples would miss is kept");
        {
            std::vector<float> samples(1000, 0.0f);
            samples[503] = 1.0f;
            samples[777] = -0.5f;
            const auto spans = minMaxColumns(samples.data(), 1000, 100);
            const auto has = [&](float v) {
                return std::any_of(spans.begin(), spans.end(), [v](auto s) {
                    return juce::exactlyEqual(s.first, v) || juce::exactlyEqual(s.second, v);
                });
            };
            expect(has(1.0f));
            expect(has(-0.5f));
        }

        beginTest("Spans join up: a rise runs min to max from where the column before ended");
        {
            std::vector<float> samples(1000);
            for (size_t i = 0; i < samples.size(); ++i)
                samples[i] = static_cast<float>(i) / 1000.0f;
            const auto spans = minMaxColumns(samples.data(), 1000, 100);
            for (size_t c = 1; c < spans.size(); ++c) {
                expect(spans[c].first < spans[c].second);
                expect(juce::exactlyEqual(spans[c].first, spans[c - 1].second));
            }
        }

        beginTest("A fall runs max to min");
        {
            std::vector<float> samples(1000);
            for (size_t i = 0; i < samples.size(); ++i)
                samples[i] = -static_cast<float>(i) / 1000.0f;
            const auto spans = minMaxColumns(samples.data(), 1000, 100);
            for (size_t c = 1; c < spans.size(); ++c) {
                expect(spans[c].first > spans[c].second);
                expect(juce::exactlyEqual(spans[c].first, spans[c - 1].second));
            }
        }
    }
};

static ScopeMinMaxColumnsTest scopeMinMaxColumnsTest;

class ScopeTriggerModesTest : public juce::UnitTest {
public:
    ScopeTriggerModesTest() : UnitTest("ScopeTriggerModes", "MoTool") {}

    void runTest() override {
        beginTest("A chip channel's scope syncs to the chip by default; audio's modes stop before Chip Sync");
        {
            const ScopeSettings audio, chip {ScopeSettings::Signal::ChipLevels};
            expect(audio.triggerMode.definition.defaultValue == TriggerMode::RisingEdge);
            expectEquals(audio.triggerMode.definition.numberOfStates(), int(TriggerMode::ChipSync));
            expect(chip.triggerMode.definition.defaultValue == TriggerMode::ChipSync);
            expectEquals(chip.triggerMode.definition.numberOfStates(), int(TriggerMode::ChipSync) + 1);
        }

        beginTest("A chip scope from an edit saved before Chip Sync syncs to the chip, not to the stored Rising Edge");
        {
            const auto loaded = [](ScopeSettings::Signal signal, const char* storedMode) {
                juce::ValueTree state {"PLUGIN"};
                state.setProperty(ScopeIDs::triggerMode, storedMode, nullptr);
                ScopeSettings settings {signal};
                settings.referTo(state, nullptr);
                return settings.triggerMode.getStoredValue();
            };
            expect(loaded(ScopeSettings::Signal::ChipLevels, "RisingEdge") == TriggerMode::ChipSync);
            expect(loaded(ScopeSettings::Signal::Audio, "FallingEdge") == TriggerMode::FallingEdge);
        }
    }
};

static ScopeTriggerModesTest scopeTriggerModesTest;

class ScopePluginBufferTest : public juce::UnitTest
{
public:
    ScopePluginBufferTest() : UnitTest("ScopePluginBuffer", "MoTool") {}

    void runTest() override {
        beginTest("Stereo scopes receive their own audio and drop old history on initialisation");

        auto edit = tracktion::Edit::createSingleTrackEdit(*tracktion::Engine::getEngines()[0]);
        auto plugin = edit->getPluginCache().createNewPlugin(ScopePlugin::xmlTypeName, {});
        auto& scope = dynamic_cast<ScopePlugin&>(*plugin);
        const tracktion::PluginInitialisationInfo info{{}, 44100.0, 4};
        scope.initialise(info);

        juce::AudioBuffer<float> audio(2, 4);
        audio.clear();
        audio.setSample(0, 3, 0.25f);
        audio.setSample(1, 3, 0.75f);
        scope.applyToBuffer(
            {&audio, juce::AudioChannelSet::stereo(), 0, 4, nullptr, 0.0, {}, true, false, false, false});

        float sample = 0.0f;
        expectEquals(scope.getBuffer(0)->copyTo(&sample, 1), 1);
        expect(juce::exactlyEqual(sample, 0.25f));
        expectEquals(scope.getBuffer(1)->copyTo(&sample, 1), 1);
        expect(juce::exactlyEqual(sample, 0.75f));

        scope.initialise(info);
        expectEquals(scope.getBuffer(1)->copyTo(&sample, 1), 1);
        expect(juce::exactlyEqual(sample, 0.0f));
    }
};

static ScopePluginBufferTest scopePluginBufferTest;

}  // namespace MoTool::uZX
