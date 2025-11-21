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
    setSize(168, 240);

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
    const int toggleButtonWidth = 40;
    const int toggleButtonSpacing = 4;
    const int togglesWidth = 3 * toggleButtonWidth + 2 * toggleButtonSpacing;

    // Row 1: Channel enables (A, B, C)
    auto channelRow = r.removeFromTop(itemHeight);
    auto channelRowCentered = channelRow.withSizeKeepingCentre(togglesWidth, itemHeight);
    channelAButton.setBounds(channelRowCentered.removeFromLeft(toggleButtonWidth));
    channelRowCentered.removeFromLeft(toggleButtonSpacing);
    channelBButton.setBounds(channelRowCentered.removeFromLeft(toggleButtonWidth));
    channelRowCentered.removeFromLeft(toggleButtonSpacing);
    channelCButton.setBounds(channelRowCentered);

    r.removeFromTop(itemSpacing);

    // Row 2: Tone enables (T, T, T)
    auto toneRow = r.removeFromTop(itemHeight);
    auto toneRowCentered = toneRow.withSizeKeepingCentre(togglesWidth, itemHeight);
    toneAButton.setBounds(toneRowCentered.removeFromLeft(toggleButtonWidth));
    toneRowCentered.removeFromLeft(toggleButtonSpacing);
    toneBButton.setBounds(toneRowCentered.removeFromLeft(toggleButtonWidth));
    toneRowCentered.removeFromLeft(toggleButtonSpacing);
    toneCButton.setBounds(toneRowCentered);

    r.removeFromTop(itemSpacing);

    // Row 3: Noise enables (N, N, N)
    auto noiseRow = r.removeFromTop(itemHeight);
    auto noiseRowCentered = noiseRow.withSizeKeepingCentre(togglesWidth, itemHeight);
    noiseAButton.setBounds(noiseRowCentered.removeFromLeft(toggleButtonWidth));
    noiseRowCentered.removeFromLeft(toggleButtonSpacing);
    noiseBButton.setBounds(noiseRowCentered.removeFromLeft(toggleButtonWidth));
    noiseRowCentered.removeFromLeft(toggleButtonSpacing);
    noiseCButton.setBounds(noiseRowCentered);

    r.removeFromTop(itemSpacing);

    // Row 4: Envelope enables (E, E, E)
    auto envelopeRow = r.removeFromTop(itemHeight);
    auto envelopeRowCentered = envelopeRow.withSizeKeepingCentre(togglesWidth, itemHeight);
    envelopeAButton.setBounds(envelopeRowCentered.removeFromLeft(toggleButtonWidth));
    envelopeRowCentered.removeFromLeft(toggleButtonSpacing);
    envelopeBButton.setBounds(envelopeRowCentered.removeFromLeft(toggleButtonWidth));
    envelopeRowCentered.removeFromLeft(toggleButtonSpacing);
    envelopeCButton.setBounds(envelopeRowCentered);
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
