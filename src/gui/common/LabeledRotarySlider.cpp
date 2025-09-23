#include "LabeledRotarySlider.h"

namespace MoTool {

namespace te = tracktion;

LabeledRotarySlider::LabeledRotarySlider(te::AutomatableParameter::Ptr param, const String& labelText, const String& tooltip, const String& valueSuffix)
    : attachment(slider, *param), parameter(param), mouseListener(*this), midiMapping(param)
{
    slider.setSliderStyle(Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    slider.setTooltip(tooltip);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    slider.setNumDecimalPlacesToDisplay(2);
    slider.setTextValueSuffix(valueSuffix);

    // Disable default popup menu and add custom mouse listener for right-click
    slider.setPopupMenuEnabled(false);
    slider.addMouseListener(&mouseListener, false);

    addAndMakeVisible(slider);

    label.setText(labelText, dontSendNotification);
    label.setJustificationType(Justification::centred);
    label.setFont(label.getFont().withPointHeight((float) labelHeight));
    addAndMakeVisible(label);
}

LabeledRotarySlider::LabeledRotarySlider(te::AutomatableParameter::Ptr param, const uZX::ValueWithSource<float>& value)
    : LabeledRotarySlider(param, value.definition.shortLabel, value.definition.description, value.definition.units)
{}

LabeledRotarySlider::LabeledRotarySlider(const uZX::ValueWithSource<float>& value)
    : LabeledRotarySlider(*value.source->parameter, value.definition.shortLabel, value.definition.description, value.definition.units)
{
    jassert(value.source != nullptr);
}

void LabeledRotarySlider::resized() {
    auto bounds = getLocalBounds();
    auto sliderHeight = bounds.getHeight() - labelHeight;

    slider.setBounds(bounds.removeFromTop(sliderHeight));
    bounds.translate(0, -labelOverlap);
    label.setBounds(bounds.expanded(4, 0));
}

void LabeledRotarySlider::paint(Graphics& g) {
    // Draw a small indicator in top-left corner if parameter is mapped to MIDI CC
    if (midiMapping.isParameterMapped()) {
        g.setColour(Colours::orange);
        g.fillRect(0, 0, 4, 4);
    }
}

void LabeledRotarySlider::showParameterMenu() {
    midiMapping.showMappingMenu([this]() {
        repaint(); // Update visual feedback when mapping changes
    });
}


}  // namespace MoTool