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
    setSize(168, 160);

    addAndMakeVisible(chipTypeButton);
    addAndMakeVisible(chipClockCombo);
    chipClockBinding.setDecimalPlaces(2);

    addAndMakeVisible(volumeKnob);
    addAndMakeVisible(layoutButton);
    addAndMakeVisible(stereoKnob);
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
