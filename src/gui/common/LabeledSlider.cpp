#include "LabeledSlider.h"

namespace MoTool {

namespace te = tracktion;

LabeledSlider::LabeledSlider(std::unique_ptr<ParameterEndpoint> endpoint, Slider::SliderStyle style)
    : binding(slider, std::move(endpoint))
{
    slider.setSliderStyle(style);
    slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);

    label.setText(binding.endpoint().getName(), dontSendNotification);
    label.setJustificationType(Justification::centred);
    label.setFont(label.getFont().withPointHeight((float) labelHeight));

    addAndMakeVisible(slider);
    addAndMakeVisible(label);

    binding.midiMapping.addChangeListener(this);
}


LabeledSlider::~LabeledSlider() {
    binding.midiMapping.removeChangeListener(this);
}


void LabeledSlider::resized() {
    auto bounds = getLocalBounds();
    auto sliderHeight = bounds.getHeight() - labelHeight;

    slider.setBounds(bounds.removeFromTop(sliderHeight));
    bounds.translate(0, -labelOverlap);
    label.setBounds(bounds.expanded(4, 0));
}

void LabeledSlider::paint(Graphics& g) {
    // Draw a small indicator in top-left corner if parameter is mapped to MIDI CC
    // TODO indicator for automation as well?
    if (binding.midiMapping.isParameterMapped()) {
        g.setColour(Colours::orange);
        g.fillRect(0, 0, 4, 4);
    }
}

void LabeledSlider::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &binding.midiMapping) {
        repaint();
    }
}


}  // namespace MoTool