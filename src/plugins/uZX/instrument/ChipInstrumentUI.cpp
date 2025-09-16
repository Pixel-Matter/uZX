#include "ChipInstrumentUI.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"

namespace te = tracktion;

namespace MoTool::uZX {

LabeledRotarySlider::LabeledRotarySlider(te::AutomatableParameter::Ptr parameter, const String& labelText, const String& tooltip)
    : attachment(slider, *parameter)
{
    slider.setSliderStyle(Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    slider.setTooltip(tooltip);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    slider.setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(slider);

    label.setText(labelText, dontSendNotification);
    label.setJustificationType(Justification::centred);
    label.setFont(label.getFont().withPointHeight((float) labelHeight));
    addAndMakeVisible(label);
}

void LabeledRotarySlider::resized() {
    auto bounds = getLocalBounds();
    auto sliderHeight = bounds.getHeight() - labelHeight;

    slider.setBounds(bounds.removeFromTop(sliderHeight));
    bounds.translate(0, -labelOverlap);
    label.setBounds(bounds);
}

//==============================================================================
// ChipInstrumentUI
//==============================================================================
ChipInstrumentUI::ChipInstrumentUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , adsrAttackSlider  (dynamic_cast<ChipInstrumentPlugin*>(pluginPtr.get())->ampAttack,   "A", "Amp Attack Time")
    , adsrDecaySlider   (dynamic_cast<ChipInstrumentPlugin*>(pluginPtr.get())->ampDecay,    "D", "Amp Decay Time")
    , adsrSustainSlider (dynamic_cast<ChipInstrumentPlugin*>(pluginPtr.get())->ampSustain,  "S", "Amp Sustain Level")
    , adsrReleaseSlider (dynamic_cast<ChipInstrumentPlugin*>(pluginPtr.get())->ampRelease,  "R", "Amp Release Time")
    , adsrVelocitySlider(dynamic_cast<ChipInstrumentPlugin*>(pluginPtr.get())->ampVelocity, "V", "Amp Velocity Sensitivity")

{
    jassert(pluginPtr != nullptr);

    setSize(168, 320);

    addAndMakeVisible(adsrAttackSlider);
    addAndMakeVisible(adsrDecaySlider);
    addAndMakeVisible(adsrSustainSlider);
    addAndMakeVisible(adsrReleaseSlider);
}

void ChipInstrumentUI::paint(Graphics&) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void ChipInstrumentUI::resized() {
    auto r = getLocalBounds().reduced(8);
    static constexpr int knobSize = 32;
    static constexpr int spacing = 8;

    auto row = r.removeFromTop(knobSize + adsrAttackSlider.getLabelHeight());

    adsrAttackSlider.setBounds(row.removeFromLeft(knobSize));
    row.removeFromLeft(spacing);
    adsrDecaySlider.setBounds(row.removeFromLeft(knobSize));
    row.removeFromLeft(spacing);
    adsrSustainSlider.setBounds(row.removeFromLeft(knobSize));
    row.removeFromLeft(spacing);
    adsrReleaseSlider.setBounds(row.removeFromLeft(knobSize));
}

REGISTER_PLUGIN_UI_ADAPTER(ChipInstrumentPlugin, ChipInstrumentUI)

}  // namespace MoTool::uZX
