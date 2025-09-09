#include "LevelMeterUI.h"

#include "../common/LookAndFeel.h"
#include "PluginUIAdapterRegistry.h"


namespace MoTool {

LevelMeterUI::LevelMeterUI(EditViewState& evs, tracktion::Plugin::Ptr p)
    : PluginDeviceUI(evs, p)
{
    te::LevelMeterPlugin* levelMeterPlugin = dynamic_cast<te::LevelMeterPlugin*>(plugin.get());
    jassert(levelMeterPlugin != nullptr);

    setSize(METER_WIDTH, 100);  // Height will be adjusted by parent

    if (levelMeterPlugin) {
        levelMeterPlugin->showMidiActivity = true;
        levelMeterPlugin->measurer.addClient(measurerClient);
    }

    startTimerHz(30);
}

LevelMeterUI::~LevelMeterUI() {
    stopTimer();

    te::LevelMeterPlugin* levelMeterPlugin = dynamic_cast<te::LevelMeterPlugin*>(plugin.get());
    if (levelMeterPlugin)
        levelMeterPlugin->measurer.removeClient(measurerClient);
}

void LevelMeterUI::resized() {
    // Fixed width, height determined by parent
}

void LevelMeterUI::timerCallback() {
    updateLevels();
    repaint();
}

void LevelMeterUI::updateLevels() {
    // Get real audio levels from the level measurer using the client API
    auto leftDbTime = measurerClient.getAndClearAudioLevel(0);
    auto rightDbTime = measurerClient.getAndClearAudioLevel(1);
    auto midiDbTime = measurerClient.getAndClearMidiLevel();

    // Convert dB to linear (0.0 to 1.0 range)
    // -60 dB is considered "silence", 0 dB is full scale
    constexpr float minDb = -60.0f;
    constexpr float maxDb = 0.0f;

    leftLevel = juce::jlimit(0.0f, 1.0f, (leftDbTime.dB - minDb) / (maxDb - minDb));
    rightLevel = juce::jlimit(0.0f, 1.0f, (rightDbTime.dB - minDb) / (maxDb - minDb));

    // MIDI activity - convert dB to linear with faster decay
    float midiLevelLinear = juce::jlimit(0.0f, 1.0f, (midiDbTime.dB - minDb) / (maxDb - minDb));
    midiActivity = juce::jmax(midiLevelLinear, midiActivity * 0.85f);  // Fast decay for MIDI
}

void LevelMeterUI::paint(juce::Graphics& g) {
    // g.fillAll(juce::Colours::black);

    auto bounds = getLocalBounds();

    // MIDI activity section at top
    auto midiArea = bounds.removeFromTop(MIDI_SECTION_HEIGHT);
    drawLevelMeter(g, midiArea, MIDI_LED_HEIGHT, midiActivity, true);

    bounds.removeFromTop(8);  // gap

    // Audio level meters at bottom
    int halfWidth = (bounds.getWidth() - LED_GAP) / 2;

    // Left channel
    auto leftArea = bounds.removeFromLeft(halfWidth);
    drawLevelMeter(g, leftArea, LED_HEIGHT, leftLevel, false);

    // Right channel
    auto rightArea = bounds.removeFromRight(halfWidth);
    drawLevelMeter(g, rightArea, LED_HEIGHT, rightLevel, false);

    // Draw center separator line
    // g.setColour(juce::Colours::darkgrey);
    // g.drawVerticalLine(halfWidth, 0.0f, static_cast<float>(getHeight()));
}

void LevelMeterUI::drawLevelMeter(juce::Graphics& g, juce::Rectangle<int> area, int ledHeight, float level, bool isMidi) {
    if (area.isEmpty())
        return;

    level = juce::jlimit(0.0f, 1.0f, level);

    int meterHeight = area.getHeight();
    int ledCount = meterHeight / (ledHeight + LED_GAP);
    int activeLeds = static_cast<int>(level * static_cast<float>(ledCount));

    for (int i = 0; i < ledCount; ++i) {
        int y = area.getBottom() - (i + 1) * (ledHeight + LED_GAP);
        juce::Rectangle<int> ledRect(area.getX(), y, area.getWidth(), ledHeight);

        bool isActive = i < activeLeds;
        float ledLevel = static_cast<float>(i) / static_cast<float>(ledCount);

        if (isActive) {
            g.setColour(getLevelColour(ledLevel, isMidi));
            g.fillRect(ledRect);
        } else {
            // Dim background LED
            g.setColour(getLevelColour(ledLevel, isMidi).withAlpha(0.1f));
            g.fillRect(ledRect);
        }
    }
}

juce::Colour LevelMeterUI::getLevelColour(float level, bool isMidi) const {
    if (isMidi) {
        // MIDI activity: blue/cyan
        return juce::Colours::cyan.withSaturation(0.8f);
    } else {
        // Audio levels: green -> yellow -> red (like Ableton)
        if (level < 0.6f) {
            return juce::Colours::green;
        } else if (level < 0.85f) {
            return juce::Colours::yellow;
        } else {
            return juce::Colours::red;
        }
    }
}

// Auto-register tracktion plugin adapters
REGISTER_PLUGIN_UI_ADAPTER(te::LevelMeterPlugin, LevelMeterUI)

}  // namespace MoTool