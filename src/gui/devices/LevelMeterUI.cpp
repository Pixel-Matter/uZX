#include "LevelMeterUI.h"

#include "../common/LevelMeter.h"
#include "PluginUIAdapterRegistry.h"


namespace MoTool {

namespace te = tracktion;

// A note's velocity as the engine reports it, in dB: -60 and below is empty, full velocity is full
static float midiToFill(float dB) {
    return juce::jlimit(0.0f, 1.0f, (dB + 60.0f) / 60.0f);
}

LevelMeterUI::LevelMeterUI(tracktion::Plugin::Ptr p)
    : PluginDeviceUI(p)
{
    te::LevelMeterPlugin* levelMeterPlugin = dynamic_cast<te::LevelMeterPlugin*>(plugin.get());
    jassert(levelMeterPlugin != nullptr);

    setSize(METER_WIDTH, 100);  // Height will be adjusted by parent

    if (levelMeterPlugin) {
        levelMeterPlugin->showMidiActivity.setValue(true, nullptr);  // a display setting: not undoable
        levelMeterPlugin->measurer.addClient(measurerClient);
    }

    startTimerHz(LedMeter::levelRefreshHz);
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
    leftDb = LedMeter::fallTo(leftDb, measurerClient.getAndClearAudioLevel(0).dB);
    rightDb = LedMeter::fallTo(rightDb, measurerClient.getAndClearAudioLevel(1).dB);
    // MIDI activity decays faster than the audio levels
    midiActivity = juce::jmax(midiToFill(measurerClient.getAndClearMidiLevel().dB), midiActivity * 0.85f);
}

void LevelMeterUI::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    if (showsMidi) {
        // MIDI activity section at top
        auto midiArea = bounds.removeFromTop(MIDI_SECTION_HEIGHT);
        LedMeter::draw(g, midiArea, MIDI_LED_HEIGHT, LED_GAP, midiActivity, false, LedMeter::levelZones, true);

        bounds.removeFromTop(8);  // gap
    }

    // Audio level meters at bottom, left and right channels
    int halfWidth = (bounds.getWidth() - LED_GAP) / 2;
    LedMeter::draw(g, bounds.removeFromLeft(halfWidth), LED_HEIGHT, LED_GAP, LedMeter::dbToFill(leftDb), false,
                   LedMeter::levelZones);
    LedMeter::draw(g, bounds.removeFromRight(halfWidth), LED_HEIGHT, LED_GAP, LedMeter::dbToFill(rightDb), false,
                   LedMeter::levelZones);
}

// Auto-register tracktion plugin adapters
REGISTER_PLUGIN_UI_ADAPTER(te::LevelMeterPlugin, LevelMeterUI)

}  // namespace MoTool