#include "LabeledKnob.h"

namespace MoTool {

namespace te = tracktion;

LabeledKnob::LabeledKnob(const uZX::ValueWithSource<float>& value)
    : attachment(slider, value.source->parameter)
    , midiMapping(value.source->parameter)
    , mouseListener(*this)
{
    const auto& def = value.definition;

    slider.setRange(static_cast<double>(def.valueRange.start), static_cast<double>(def.valueRange.end), static_cast<double>(def.valueRange.interval));
    slider.setSkewFactor(static_cast<double>(def.valueRange.skew));
    slider.setValue(static_cast<double>(value.getCurrentValue()), dontSendNotification);

    // slider.setSliderStyle(Slider::LinearHorizontal);
    slider.setSliderStyle(Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    slider.setTooltip(def.description);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    slider.setNumDecimalPlacesToDisplay(2);

    if (def.units.isNotEmpty()) {
        slider.setTextValueSuffix(def.units);
    }

    // Disable default popup menu and add custom mouse listener for right-click
    slider.setPopupMenuEnabled(false);
    slider.addMouseListener(&mouseListener, false);

    label.setText(def.shortLabel, dontSendNotification);
    label.setJustificationType(Justification::centred);
    label.setFont(label.getFont().withPointHeight((float) labelHeight));

    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void LabeledKnob::resized() {
    auto bounds = getLocalBounds();
    auto sliderHeight = bounds.getHeight() - labelHeight;

    slider.setBounds(bounds.removeFromTop(sliderHeight));
    bounds.translate(0, -labelOverlap);
    label.setBounds(bounds.expanded(4, 0));
}

void LabeledKnob::paint(Graphics& g) {
    // Draw a small indicator in top-left corner if parameter is mapped to MIDI CC
    if (midiMapping.isParameterMapped()) {
        g.setColour(Colours::orange);
        g.fillRect(0, 0, 4, 4);
    }
}

void LabeledKnob::showParameterMenu() {
    midiMapping.showMappingMenu([this]() {
        repaint(); // Update visual feedback when mapping changes
    });
}


}  // namespace MoTool