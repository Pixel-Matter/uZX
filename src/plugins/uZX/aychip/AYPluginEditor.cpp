#include "AYPluginEditor.h"
#include "ChipClockPresets.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"


namespace MoTool::uZX {

namespace te = tracktion;

//==============================================================================
// ChannelGroup implementation
//==============================================================================
AYPluginUI::ChannelGroup::ChannelGroup(AYChipPlugin& plugin,
                                        ParameterValue<bool>& channelParam,
                                        ParameterValue<bool>& toneParam,
                                        ParameterValue<bool>& noiseParam,
                                        ParameterValue<bool>& envelopeParam)
    : channelOn   { plugin, channelParam }
    , toneOn      { plugin, toneParam }
    , noiseOn     { plugin, noiseParam }
    , envelopeOn  { plugin, envelopeParam }
    , channelParam_(channelParam)
{
    channelParam_.addListener(this);
    updateTNEButtonsEnabledState();
}

AYPluginUI::ChannelGroup::~ChannelGroup() {
    channelParam_.removeListener(this);
}

void AYPluginUI::ChannelGroup::addToComponent(Component& parent) {
    parent.addAndMakeVisible(channelOn);
    parent.addAndMakeVisible(toneOn);
    parent.addAndMakeVisible(noiseOn);
    parent.addAndMakeVisible(envelopeOn);
}

void AYPluginUI::ChannelGroup::valueChanged(Value&) {
    updateTNEButtonsEnabledState();
}

void AYPluginUI::ChannelGroup::updateTNEButtonsEnabledState() {
    bool enabled = channelParam_.getStoredValue();
    toneOn.setEnabled(enabled);
    noiseOn.setEnabled(enabled);
    envelopeOn.setEnabled(enabled);
}

//==============================================================================
// Editor for AYChipPlugin
//==============================================================================
AYPluginUI::AYPluginUI(te::Plugin::Ptr pluginPtr)
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

    setupToggleButtons();
}

void AYPluginUI::setupToggleButtons() {
    for (auto* group : channelGroups) {
        group->addToComponent(*this);

        // Set connected edges for [A|T|N|E] style buttons
        group->channelOn.setConnectedEdges(Button::ConnectedOnRight);
        group->toneOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnRight);
        group->noiseOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnRight);
        group->envelopeOn.setConnectedEdges(Button::ConnectedOnLeft);
    }
}

void AYPluginUI::layoutChannelToggles(juce::Rectangle<int>& r) {
    r.removeFromTop(itemSpacing * 3);

    // Layout: [A|T|N|E]
    //         [B|T|N|E]
    //         [C|T|N|E]
    // Each channel gets its own row with all buttons connected and square (w=h)

    const int buttonSize = itemHeight;  // Square buttons

    for (int i = 0; i < 3; ++i) {
        if (i > 0) r.removeFromTop(itemSpacing);

        auto row = r.removeFromTop(buttonSize);
        auto& group = *channelGroups[i];

        // All buttons connected with no spacing, all square
        group.channelOn.setBounds(row.removeFromLeft(buttonSize));
        group.toneOn.setBounds(row.removeFromLeft(buttonSize));
        group.noiseOn.setBounds(row.removeFromLeft(buttonSize));
        group.envelopeOn.setBounds(row.removeFromLeft(buttonSize));
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
