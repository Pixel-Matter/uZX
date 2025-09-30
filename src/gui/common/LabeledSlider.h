#pragma once

#include <JuceHeader.h>
#include "../../controllers/Parameters.h"

#include "ParamAttachments.h"
#include "MouseListener.h"
#include "MidiParameterMapping.h"

namespace MoTool {

namespace te = tracktion;

class LabeledSlider : public Component,
                      private ChangeListener  // MidiMapping change listener
{
public:
    template <typename Type = float>
    LabeledSlider(ValueWithSource<Type>& value, Slider::SliderStyle style = Slider::RotaryVerticalDrag)
        : LabeledSlider(
            value.source ? value.source->parameter : nullptr,
            value.definition.shortLabel, value.definition.description,
            static_cast<float>(value.getCurrentValue()),
            value.definition.getFloatValueRange(),
            value.definition.units, (std::is_same_v<Type, int> ? 0 : 2),
            value.definition.valueToStringFunction,
            value.definition.stringToValueFunction,
            style
        )
    {
        // Debug assertion - this should not happen in a properly initialized plugin
        if (!value.source || !value.source->parameter) {
            DBG("Warning: LabeledSlider created with null parameter source for " + value.definition.paramID);
        }
    }

    LabeledSlider(
        te::AutomatableParameter::Ptr param,
        const String& shortLabel, const String& description,
        float initial, NormalisableRange<float> floatRange,
        const String& units, int numDecimalPlaces,
        std::function<String (double)> textFromValueFunction,
        std::function<double (const String&)> valueFromTextFunction,
        Slider::SliderStyle style
    );

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
    // TODO refactor to somewhere who owns AutomatableParameter attachment
    MidiParameterMapping midiMapping;
    MouseListenerWithCallback mouseListener;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabeledSlider)
};

}  // namespace MoTool