#include "LevelMeterUI.h"

#include "../common/LevelMeter.h"
#include "PluginUIAdapterRegistry.h"


namespace MoTool {

namespace te = tracktion;

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
    leftLevel = LedMeter::dbToFill(measurerClient.getAndClearAudioLevel(0).dB);
    rightLevel = LedMeter::dbToFill(measurerClient.getAndClearAudioLevel(1).dB);
    // MIDI activity decays faster than the audio levels
    midiActivity = juce::jmax(LedMeter::dbToFill(measurerClient.getAndClearMidiLevel().dB), midiActivity * 0.85f);
}

void LevelMeterUI::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    // MIDI activity section at top
    auto midiArea = bounds.removeFromTop(MIDI_SECTION_HEIGHT);
    LedMeter::draw(g, midiArea, MIDI_LED_HEIGHT, LED_GAP, midiActivity, false, true);

    bounds.removeFromTop(8);  // gap

    // Audio level meters at bottom, left and right channels
    int halfWidth = (bounds.getWidth() - LED_GAP) / 2;
    LedMeter::draw(g, bounds.removeFromLeft(halfWidth), LED_HEIGHT, LED_GAP, leftLevel, false);
    LedMeter::draw(g, bounds.removeFromRight(halfWidth), LED_HEIGHT, LED_GAP, rightLevel, false);
}

// Auto-register tracktion plugin adapters
REGISTER_PLUGIN_UI_ADAPTER(te::LevelMeterPlugin, LevelMeterUI)

}  // namespace MoTool