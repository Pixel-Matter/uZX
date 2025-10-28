#include "AYPluginEditor.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"


namespace MoTool::uZX {

//==============================================================================
// Editor for AYChipPlugin
//==============================================================================
AYPluginUI::AYPluginUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , plugin_(*dynamic_cast<AYChipPlugin*>(pluginPtr.get()))
{
    constrainer_.setMinimumWidth(160);
    setSize(160, 160);

    addAndMakeVisible(chipTypeButton);
    addAndMakeVisible(clockKnob);

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
    auto width = staticRow.getWidth() / 3;
    chipTypeButton.setBounds(staticRow.removeFromLeft(width).withSizeKeepingCentre(width, 20));
    clockKnob.setBounds(staticRow.removeFromLeft(width));

    // automatable
    r.removeFromTop(itemSpacing * 2);
    auto knobsRow = r.removeFromTop(itemHeight * 2);

    width = knobsRow.getWidth() / 3;
    layoutButton.setBounds(knobsRow.removeFromLeft(width).withSizeKeepingCentre(width, 20));
    stereoKnob.setBounds(knobsRow.removeFromLeft(width));
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
