#pragma once

#include <JuceHeader.h>
#include "PluginDeviceUI.h"

namespace MoTool {

/**
 * LED-style stereo level meter UI for LevelMeterPlugin.
 * Displays audio levels at bottom, MIDI activity at top, like Ableton Live; in the Player, whose
 * tracks put out no MIDI, the audio levels alone, over the whole height.
 * Width: 16px, shows stereo L/R levels with LED-style visualization.
 */
class LevelMeterUI : public PluginDeviceUI,
                     private juce::Timer
{
public:
    LevelMeterUI(tracktion::Plugin::Ptr plugin);
    ~LevelMeterUI() override;

    // PluginDeviceUI overrides
    bool hasCustomDeviceUI() override { return true; }
    bool canHasPlusButtonAfter() override { return false; }
    void showForPlayer() override { showsMidi = false; }

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Timer override for level updates
    void timerCallback() override;

    void updateLevels();

    // Audio level tracking
    float leftDb = -100.0f;  // as shown: a peak, falling back
    float rightDb = -100.0f;
    float midiActivity = 0.0f;
    bool showsMidi = true;

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