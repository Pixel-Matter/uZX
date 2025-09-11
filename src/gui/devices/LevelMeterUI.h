#pragma once

#include <JuceHeader.h>
#include "PluginDeviceUI.h"

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
    LevelMeterUI(tracktion::Plugin::Ptr plugin);
    ~LevelMeterUI() override;

    // PluginDeviceUI overrides
    bool hasCustomDeviceUI() override { return true; }
    bool canHasPlusButtonAfter() override { return false; }

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Timer override for level updates
    void timerCallback() override;

private:
    void updateLevels();
    void drawLevelMeter(juce::Graphics& g, juce::Rectangle<int> area, int ledHeight, float level, bool isMidi = false);
    juce::Colour getLevelColour(float level, bool isMidi = false) const;

    // Audio level tracking
    float leftLevel = 0.0f;
    float rightLevel = 0.0f;
    float midiActivity = 0.0f;

    // Visual parameters
    static constexpr int METER_WIDTH = 16;
    static constexpr int LED_HEIGHT = 2;
    static constexpr int LED_GAP = 1;
    static constexpr int MIDI_LED_HEIGHT = 2;
    static constexpr int MIDI_SECTION_HEIGHT = 64;

    tracktion::LevelMeasurer::Client measurerClient;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeterUI)
};

}  // namespace MoTool