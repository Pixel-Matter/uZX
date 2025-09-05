#pragma once

#include "PluginDeviceUI.h"
#include <JuceHeader.h>

namespace MoTool {

/**
 * LED-style stereo level meter UI for LevelMeterPlugin.
 * Displays audio levels at bottom, MIDI activity at top, like Ableton Live.
 * Width: 16px, shows stereo L/R levels with LED-style visualization.
 */
class LevelMeterUI : public PluginDeviceUI,
                     public juce::Timer
{
public:
    LevelMeterUI(EditViewState& evs, tracktion::Plugin::Ptr plugin);
    ~LevelMeterUI() override;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Timer override for level updates
    void timerCallback() override;

private:
    void updateLevels();
    void drawLevelMeter(juce::Graphics& g, juce::Rectangle<int> area, float level, bool isMidi = false);
    juce::Colour getLevelColour(float level, bool isMidi = false) const;

    // Audio level tracking
    float leftLevel = 0.0f;
    float rightLevel = 0.0f;
    float midiActivity = 0.0f;

    // Visual parameters
    static constexpr int METER_WIDTH = 16;
    static constexpr int LED_HEIGHT = 2;
    static constexpr int LED_GAP = 1;
    static constexpr int MIDI_SECTION_HEIGHT = 32;

    tracktion::LevelMeterPlugin* levelMeterPlugin = nullptr;
    tracktion::LevelMeasurer::Client measurerClient;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeterUI)
};

}  // namespace MoTool