#include "ChipInstrumentUI.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"

namespace te = tracktion;

namespace MoTool::uZX {

LabeledRotarySlider::LabeledRotarySlider(te::AutomatableParameter::Ptr parameter, const String& labelText, const String& tooltip, const String& valueSuffix)
    : attachment(slider, *parameter)
{
    slider.setSliderStyle(Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    slider.setTooltip(tooltip);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    slider.setNumDecimalPlacesToDisplay(2);
    slider.setTextValueSuffix(valueSuffix);
    addAndMakeVisible(slider);

    label.setText(labelText, dontSendNotification);
    label.setJustificationType(Justification::centred);
    label.setFont(label.getFont().withPointHeight((float) labelHeight));
    addAndMakeVisible(label);
}

LabeledRotarySlider::LabeledRotarySlider(te::AutomatableParameter::Ptr parameter, const ValueWithSource<float>& value)
    : LabeledRotarySlider(parameter, value.definition.shortLabel, value.definition.description, value.definition.units)
{}

LabeledRotarySlider::LabeledRotarySlider(const ValueWithSource<float>& value)
    : LabeledRotarySlider(*value.source->parameter, value.definition.shortLabel, value.definition.description, value.definition.units)
{
    jassert(value.source != nullptr);
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
    , instrument(instrumentPlugin()->instrument)
    , adsrAttackSlider  (instrument.oscParams.ampAttack)
    , adsrDecaySlider   (instrument.oscParams.ampDecay)
    , adsrSustainSlider (instrument.oscParams.ampSustain)
    , adsrReleaseSlider (instrument.oscParams.ampRelease)
    // , adsrVelocitySlider(instrument.oscParams.ampVelocity)
    , pitchAttackSlider  (instrument.oscParams.pitchAttack)
    , pitchDecaySlider   (instrument.oscParams.pitchDecay)
    , pitchSustainSlider (instrument.oscParams.pitchSustain)
    , pitchReleaseSlider (instrument.oscParams.pitchRelease)
    , pitchDepthSlider   (instrument.oscParams.pitchDepth)
{
    jassert(pluginPtr != nullptr);

    setSize(168, 320);

    addAndMakeVisible(adsrAttackSlider);
    addAndMakeVisible(adsrDecaySlider);
    addAndMakeVisible(adsrSustainSlider);
    addAndMakeVisible(adsrReleaseSlider);

    addAndMakeVisible(pitchAttackSlider);
    addAndMakeVisible(pitchDecaySlider);
    addAndMakeVisible(pitchSustainSlider);
    addAndMakeVisible(pitchReleaseSlider);
    addAndMakeVisible(pitchDepthSlider);
}

ChipInstrumentPlugin* ChipInstrumentUI::instrumentPlugin() {
    return dynamic_cast<ChipInstrumentPlugin*>(plugin.get());
}

void ChipInstrumentUI::paint(Graphics&) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void ChipInstrumentUI::resized() {
    auto r = getLocalBounds().reduced(8);
    static constexpr int knobSize = 32;
    static constexpr int spacing = 8;

    // Amp ADSR row
    auto ampRow = r.removeFromTop(knobSize + adsrAttackSlider.getLabelHeight());
    adsrAttackSlider.setBounds(ampRow.removeFromLeft(knobSize));
    ampRow.removeFromLeft(spacing);
    adsrDecaySlider.setBounds(ampRow.removeFromLeft(knobSize));
    ampRow.removeFromLeft(spacing);
    adsrSustainSlider.setBounds(ampRow.removeFromLeft(knobSize));
    ampRow.removeFromLeft(spacing);
    adsrReleaseSlider.setBounds(ampRow.removeFromLeft(knobSize));

    r.removeFromTop(spacing); // Space between rows

    // Pitch ADSR row
    auto pitchRow = r.removeFromTop(knobSize + pitchAttackSlider.getLabelHeight());
    pitchAttackSlider.setBounds(pitchRow.removeFromLeft(knobSize));
    pitchRow.removeFromLeft(spacing);
    pitchDecaySlider.setBounds(pitchRow.removeFromLeft(knobSize));
    pitchRow.removeFromLeft(spacing);
    pitchSustainSlider.setBounds(pitchRow.removeFromLeft(knobSize));
    pitchRow.removeFromLeft(spacing);
    pitchReleaseSlider.setBounds(pitchRow.removeFromLeft(knobSize));

    r.removeFromTop(spacing); // Space between rows

    // Pitch Depth knob (separate row)
    auto depthRow = r.removeFromTop(knobSize + pitchDepthSlider.getLabelHeight());
    pitchDepthSlider.setBounds(depthRow.removeFromLeft(knobSize));
}

REGISTER_PLUGIN_UI_ADAPTER(ChipInstrumentPlugin, ChipInstrumentUI)

}  // namespace MoTool::uZX
