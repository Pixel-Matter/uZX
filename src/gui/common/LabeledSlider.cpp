#include "LabeledSlider.h"

namespace MoTool {

namespace te = tracktion;


LabeledSlider::LabeledSlider(
    te::AutomatableParameter::Ptr param,
    const String& shortLabel, const String& description,
    float initial, NormalisableRange<float> floatRange,
    const String& units, int numDecimalPlaces,
    std::function<String (double)> textFromValueFunction,
    std::function<double (const String&)> valueFromTextFunction,
    Slider::SliderStyle style
)
    : attachment(slider, param)
    , midiMapping(std::move(param))
{
    slider.setRange(static_cast<double>(floatRange.start), static_cast<double>(floatRange.end), static_cast<double>(floatRange.interval));
    slider.setSkewFactor(static_cast<double>(floatRange.skew));
    slider.setValue(static_cast<double>(initial), dontSendNotification);

    slider.setSliderStyle(style);
    slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    slider.setTooltip(description);
    slider.setPopupDisplayEnabled(true, true, nullptr);

    if (textFromValueFunction) {
        slider.textFromValueFunction = textFromValueFunction;
    };
    if (valueFromTextFunction) {
        slider.valueFromTextFunction = valueFromTextFunction;
    };

    slider.setNumDecimalPlacesToDisplay(numDecimalPlaces);

    if (units.isNotEmpty()) {
        slider.setTextValueSuffix(units);
    }

    // Disable default popup menu and add custom mouse listener for right-click
    slider.setPopupMenuEnabled(false);
    slider.addMouseListener(&mouseListener, false);

    // TODO refactor to attachment
    mouseListener.setRmbCallback([this]() {
        midiMapping.showMappingMenu();
    });

    midiMapping.addChangeListener(this);

    label.setText(shortLabel, dontSendNotification);
    label.setJustificationType(Justification::centred);
    label.setFont(label.getFont().withPointHeight((float) labelHeight));

    addAndMakeVisible(slider);
    addAndMakeVisible(label);
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
    if (midiMapping.isParameterMapped()) {
        g.setColour(Colours::orange);
        g.fillRect(0, 0, 4, 4);
    }
}


}  // namespace MoTool