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
    , channelButtons{&channelAButton, &channelBButton, &channelCButton}
    , toneButtons{&toneAButton, &toneBButton, &toneCButton}
    , noiseButtons{&noiseAButton, &noiseBButton, &noiseCButton}
    , envelopeButtons{&envelopeAButton, &envelopeBButton, &envelopeCButton}
{
    constrainer_.setMinimumWidth(160);
    setSize(168, 180);

    addAndMakeVisible(chipTypeButton);
    addAndMakeVisible(chipClockCombo);
    chipClockBinding.setDecimalPlaces(2);

    addAndMakeVisible(volumeKnob);
    addAndMakeVisible(layoutButton);
    addAndMakeVisible(stereoKnob);

    setupToggleButtons();
}

void AYPluginUI::setupToggleButtons() {
    for (auto* btn : channelButtons) addAndMakeVisible(*btn);
    for (auto* btn : toneButtons) addAndMakeVisible(*btn);
    for (auto* btn : noiseButtons) addAndMakeVisible(*btn);
    for (auto* btn : envelopeButtons) addAndMakeVisible(*btn);
}

void AYPluginUI::layoutChannelToggles(juce::Rectangle<int>& r) {
    r.removeFromTop(itemSpacing * 2);

    // Row 1: Channel enables (A, B, C) - full width large buttons
    auto channelRow = r.removeFromTop(itemHeight);
    const int channelButtonSpacing = 4;
    const int channelButtonWidth = (channelRow.getWidth() - 2 * channelButtonSpacing) / 3;

    for (int i = 0; i < 3; ++i) {
        if (i > 0) channelRow.removeFromLeft(channelButtonSpacing);
        channelButtons[i]->setBounds(i < 2 ? channelRow.removeFromLeft(channelButtonWidth) : channelRow);
    }

    r.removeFromTop(itemSpacing);

    // Row 2: Effect toggles grouped by channel [T|N|E] [T|N|E] [T|N|E]
    auto effectsRow = r.removeFromTop(itemHeight);
    const int effectGroupSpacing = 4;
    const int effectGroupWidth = (effectsRow.getWidth() - 2 * effectGroupSpacing) / 3;
    const int effectButtonSpacing = 2;
    const int effectButtonWidth = (effectGroupWidth - 2 * effectButtonSpacing) / 3;

    for (int chan = 0; chan < 3; ++chan) {
        if (chan > 0) effectsRow.removeFromLeft(effectGroupSpacing);

        auto effectGroup = (chan < 2) ? effectsRow.removeFromLeft(effectGroupWidth) : effectsRow;

        toneButtons[chan]->setBounds(effectGroup.removeFromLeft(effectButtonWidth));
        effectGroup.removeFromLeft(effectButtonSpacing);
        noiseButtons[chan]->setBounds(effectGroup.removeFromLeft(effectButtonWidth));
        effectGroup.removeFromLeft(effectButtonSpacing);
        envelopeButtons[chan]->setBounds(effectGroup);
    }
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
    layoutChannelToggles(r);
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
