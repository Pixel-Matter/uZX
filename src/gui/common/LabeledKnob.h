#pragma once

#include <JuceHeader.h>
#include "../../controllers/ParamAttachments.h"
#include "../../controllers/Parameters.h"
#include "MidiParameterMapping.h"
#include "juce_events/juce_events.h"

namespace MoTool {

namespace te = tracktion;

class LabeledKnob : public Component,
                    private ChangeListener  // MidiMapping change listener
{
public:
    template <typename Type = float>
    LabeledKnob(ValueWithSource<Type>& value)
        : attachment(slider, value)
        , midiMapping(value.source ? value.source->parameter : nullptr)
    {
        // Debug assertion - this should not happen in a properly initialized plugin
        if (!value.source || !value.source->parameter) {
            DBG("Warning: LabeledKnob created with null parameter source for " + value.definition.paramID);
        }

        const auto& def = value.definition;

        slider.setRange(static_cast<double>(def.valueRange.start), static_cast<double>(def.valueRange.end), static_cast<double>(def.valueRange.interval));
        slider.setSkewFactor(static_cast<double>(def.valueRange.skew));
        slider.setValue(static_cast<double>(value.getCurrentValue()), dontSendNotification);

        // slider.setSliderStyle(Slider::LinearHorizontal);
        slider.setSliderStyle(Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        slider.setTooltip(def.description);
        slider.setPopupDisplayEnabled(true, true, nullptr);

        if constexpr (std::is_same_v<Type, int>)
            slider.setNumDecimalPlacesToDisplay(0);
        else
            slider.setNumDecimalPlacesToDisplay(2);

        if (def.units.isNotEmpty()) {
            slider.setTextValueSuffix(def.units);
        }

        // Disable default popup menu and add custom mouse listener for right-click
        slider.setPopupMenuEnabled(false);
        slider.addMouseListener(&mouseListener, false);

        // TODO refactor to attachment
        mouseListener.setRmbCallback([this]() {
            midiMapping.showMappingMenu();
        });

        midiMapping.addChangeListener(this);

        label.setText(def.shortLabel, dontSendNotification);
        label.setJustificationType(Justification::centred);
        label.setFont(label.getFont().withPointHeight((float) labelHeight));

        addAndMakeVisible(slider);
        addAndMakeVisible(label);
    }

    Slider& getSlider() { return slider; }
    Label& getLabel() { return label; }

    void resized() override;
    void paint(Graphics& g) override;

    inline static constexpr int labelHeight = 10;
    inline static constexpr int labelOverlap = 2;

    int getLabelHeight() const { return labelHeight - labelOverlap; }

private:
    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &midiMapping) {
            repaint();
        }
    }

    Slider slider;
    Label label;

    SliderAutoParamAttachment attachment;
    // TODO refactor to SliderAutoParamAttachment
    MidiParameterMapping midiMapping;
    MouseListenerWithCallback mouseListener;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabeledKnob)
};

}  // namespace MoTool