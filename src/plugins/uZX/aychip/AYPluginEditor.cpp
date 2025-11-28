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
    updateTNEState();
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

void AYPluginUI::ChannelGroup::setupButtonColours(ToggleButton& button) {
    button .setColour(TextButton::buttonOnColourId, Colors::Theme::soloed);

    // TODO if logic is inverted, use Theme::surface as on color
    // button.setColour(TextButton::buttonOnColourId, Colors::Theme::surface);
    // button.setColour(TextButton::buttonColourId,   Colors::Theme::muted);
    // button.setColour(TextButton::textColourOffId,  Colors::Theme::background);
    // button.setColour(TextButton::textColourOnId,   Colors::Theme::textPrimary);
}

void AYPluginUI::ChannelGroup::setupWidgets() {
    setupButtonColours(channelOn);
    setupButtonColours(toneOn);
    setupButtonColours(noiseOn);
    setupButtonColours(envelopeOn);

    // Set connected edges for buttons
    channelOn.setConnectedEdges(Button::ConnectedOnRight);
    toneOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnBottom);
    noiseOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnTop | Button::ConnectedOnBottom);
    envelopeOn.setConnectedEdges(Button::ConnectedOnLeft | Button::ConnectedOnTop);
}

void AYPluginUI::ChannelGroup::layoutButtons(Rectangle<int> bounds) {
    const int buttonSize = bounds.getHeight() / 2;
    const int w = bounds.getWidth();

    channelOn.setBounds(bounds.removeFromLeft(buttonSize));
    auto column = bounds.removeFromLeft(buttonSize);
    toneOn.setBounds(column.removeFromTop(column.getHeight() / 3));
    noiseOn.setBounds(column.removeFromTop(column.getHeight() / 2));
    envelopeOn.setBounds(column);
}

void AYPluginUI::ChannelGroup::valueChanged(Value&) {
    updateTNEState();
}

void AYPluginUI::ChannelGroup::updateTNEState() {
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
        group->setupWidgets();
    }
}

void AYPluginUI::paint(Graphics& g) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void AYPluginUI::resized() {
    auto r = getLocalBounds().reduced(8, 8);

    // static
    auto staticRow = r.removeFromTop(itemHeight);
    auto buttonWidth = staticRow.getWidth() / 3 - 8;
    chipTypeButton.setBounds(staticRow.removeFromLeft(buttonWidth).withSizeKeepingCentre(buttonWidth, 20));
    staticRow.removeFromLeft(8);
    chipClockCombo.setBounds(staticRow.withSizeKeepingCentre(staticRow.getWidth(), 20));

    // automatable
    r.removeFromTop(itemSpacing * 2);
    auto knobsRow = r.removeFromTop(itemHeight * 2);

    layoutButton.setBounds(knobsRow.removeFromLeft(buttonWidth).withSizeKeepingCentre(buttonWidth, 20));

    knobsRow.removeFromLeft(8);
    stereoKnob.setBounds(knobsRow.removeFromLeft(knobsRow.getWidth() / 2));
    volumeKnob.setBounds(knobsRow);

    // Channel and effect toggles
    r.removeFromTop(itemSpacing * 2);
    auto col = r.removeFromLeft(buttonWidth);
    const float h = ((float) col.getHeight() - itemSpacing * 2.0f) / 3.0f;
    for (int i = 0; i < 3; ++i) {
        channelGroups[i]->layoutButtons(col.removeFromTop(roundToInt(h)));
        col.removeFromTop(itemSpacing);
    }}

ComponentBoundsConstrainer* AYPluginUI::getBoundsConstrainer() {
    return &constrainer_;
}

bool AYPluginUI::hasDeviceMenu() const {
    return true;
}

void AYPluginUI::populateDeviceMenu(juce::PopupMenu& menu) {
    addMidiRangeMenu(menu, plugin_.staticParams.baseMidiChannel, plugin_.staticParams.baseMidiChannel.definition.description, 4);
    // addDiscreteIntegerParameterMenu(menu, plugin_.staticParams.numOutputChannels, plugin_.staticParams.numOutputChannels.definition.description);
}

REGISTER_PLUGIN_UI_ADAPTER(AYChipPlugin, AYPluginUI)

}
