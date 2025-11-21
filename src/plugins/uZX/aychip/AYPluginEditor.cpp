#include "AYPluginEditor.h"
#include "ChipClockPresets.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"


namespace MoTool::uZX {

//==============================================================================
// Editor for AYChipPlugin
//==============================================================================
AYPluginUI::AYPluginUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , plugin_(*dynamic_cast<AYChipPlugin*>(pluginPtr.get()))
    , chipClockBinding(chipClockCombo, plugin_.staticParams.chipClock, makeChipClockPresets(), false, true)
{
    constrainer_.setMinimumWidth(160);
    setSize(168, 180);

    addAndMakeVisible(chipTypeButton);
    addAndMakeVisible(chipClockCombo);
    chipClockBinding.setDecimalPlaces(2);

    addAndMakeVisible(volumeKnob);
    addAndMakeVisible(layoutButton);
    addAndMakeVisible(stereoKnob);

    // Add channel and effect toggle buttons
    addAndMakeVisible(channelAButton);
    addAndMakeVisible(channelBButton);
    addAndMakeVisible(channelCButton);

    addAndMakeVisible(toneAButton);
    addAndMakeVisible(toneBButton);
    addAndMakeVisible(toneCButton);

    addAndMakeVisible(noiseAButton);
    addAndMakeVisible(noiseBButton);
    addAndMakeVisible(noiseCButton);

    addAndMakeVisible(envelopeAButton);
    addAndMakeVisible(envelopeBButton);
    addAndMakeVisible(envelopeCButton);
}

void AYPluginUI::paint(Graphics& g) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void AYPluginUI::resized() {
    auto r = getLocalBounds().reduced(8, 8);

    // static
    auto staticRow = r.removeFromTop(itemHeight * 2);
    auto buttonWidth = staticRow.getWidth() / 3 - 8;
    auto knobWidth = (staticRow.getWidth() - buttonWidth) / 2;
    chipTypeButton.setBounds(staticRow.removeFromLeft(buttonWidth).withSizeKeepingCentre(buttonWidth, 20));
    staticRow.removeFromLeft(8);
    auto comboArea = staticRow;
    chipClockCombo.setBounds(comboArea.withSizeKeepingCentre(comboArea.getWidth(), 20));

    // automatable
    r.removeFromTop(itemSpacing * 2);
    auto knobsRow = r.removeFromTop(itemHeight * 2);

    layoutButton.setBounds(knobsRow.removeFromLeft(buttonWidth).withSizeKeepingCentre(buttonWidth, 20));
    stereoKnob.setBounds(knobsRow.removeFromLeft(knobWidth));
    volumeKnob.setBounds(knobsRow);

    // Channel and effect toggles
    r.removeFromTop(itemSpacing * 2);

    // Row 1: Channel enables (A, B, C) - full width large buttons
    auto channelRow = r.removeFromTop(itemHeight);
    const int channelButtonSpacing = 4;
    const int channelButtonWidth = (channelRow.getWidth() - 2 * channelButtonSpacing) / 3;

    channelAButton.setBounds(channelRow.removeFromLeft(channelButtonWidth));
    channelRow.removeFromLeft(channelButtonSpacing);
    channelBButton.setBounds(channelRow.removeFromLeft(channelButtonWidth));
    channelRow.removeFromLeft(channelButtonSpacing);
    channelCButton.setBounds(channelRow);

    r.removeFromTop(itemSpacing);

    // Row 2: Effect toggles grouped by channel [T|N|E] [T|N|E] [T|N|E]
    auto effectsRow = r.removeFromTop(itemHeight);
    const int effectGroupSpacing = 4;
    const int effectGroupWidth = (effectsRow.getWidth() - 2 * effectGroupSpacing) / 3;
    const int effectButtonSpacing = 2;
    const int effectButtonWidth = (effectGroupWidth - 2 * effectButtonSpacing) / 3;

    // Channel A effects: T|N|E
    auto effectGroupA = effectsRow.removeFromLeft(effectGroupWidth);
    toneAButton.setBounds(effectGroupA.removeFromLeft(effectButtonWidth));
    effectGroupA.removeFromLeft(effectButtonSpacing);
    noiseAButton.setBounds(effectGroupA.removeFromLeft(effectButtonWidth));
    effectGroupA.removeFromLeft(effectButtonSpacing);
    envelopeAButton.setBounds(effectGroupA);

    effectsRow.removeFromLeft(effectGroupSpacing);

    // Channel B effects: T|N|E
    auto effectGroupB = effectsRow.removeFromLeft(effectGroupWidth);
    toneBButton.setBounds(effectGroupB.removeFromLeft(effectButtonWidth));
    effectGroupB.removeFromLeft(effectButtonSpacing);
    noiseBButton.setBounds(effectGroupB.removeFromLeft(effectButtonWidth));
    effectGroupB.removeFromLeft(effectButtonSpacing);
    envelopeBButton.setBounds(effectGroupB);

    effectsRow.removeFromLeft(effectGroupSpacing);

    // Channel C effects: T|N|E
    auto effectGroupC = effectsRow;
    toneCButton.setBounds(effectGroupC.removeFromLeft(effectButtonWidth));
    effectGroupC.removeFromLeft(effectButtonSpacing);
    noiseCButton.setBounds(effectGroupC.removeFromLeft(effectButtonWidth));
    effectGroupC.removeFromLeft(effectButtonSpacing);
    envelopeCButton.setBounds(effectGroupC);
}

ComponentBoundsConstrainer* AYPluginUI::getBoundsConstrainer() {
    return &constrainer_;
}

bool AYPluginUI::hasDeviceMenu() const {
    return true;
}

void AYPluginUI::populateDeviceMenu(juce::PopupMenu& menu) {
    addMidiRangeMenu(menu, plugin_.staticParams.baseMidiChannel, plugin_.staticParams.baseMidiChannel.definition.description, 4);
}

REGISTER_PLUGIN_UI_ADAPTER(AYChipPlugin, AYPluginUI)

}
