#pragma once

#include <JuceHeader.h>
#include "../../controllers/Parameters.h"

#include "ParamBindings.h"
#include "MidiParameterMapping.h"

namespace MoTool {

namespace te = tracktion;

/**
    Compound control that displays a `Slider` with a caption and keeps it bound
    to a plugin parameter, repainting when MIDI learn mappings change.
*/
class LabeledSlider : public Component,
                      private ChangeListener  // MidiMapping change listener
{
public:
    LabeledSlider(std::unique_ptr<ParameterEndpoint> endpoint, Slider::SliderStyle style = Slider::RotaryVerticalDrag);

    template <typename Type>
    LabeledSlider(ParameterValue<Type>& value, Slider::SliderStyle style = Slider::RotaryVerticalDrag)
        : LabeledSlider(makeResolveParamEndpoint(value), style)
    {}

    template <typename Type>
    LabeledSlider(te::AutomatableEditItem& editItem, ParameterValue<Type>& value, Slider::SliderStyle style = Slider::RotaryVerticalDrag)
        : LabeledSlider(makeResolveParamEndpoint(editItem, value), style)
    {}

    ~LabeledSlider() override;

    void resized() override;
    void paint(Graphics& g) override;

    inline static constexpr int labelHeight = 10;
    inline static constexpr int labelOverlap = 1;

    int getLabelHeight() const { return labelHeight - labelOverlap; }

private:
    // listener to update on mapping changes
    void changeListenerCallback(ChangeBroadcaster* source) override;

    Slider slider;
    Label label;

    SliderParamEndpointBinding binding;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabeledSlider)
};

}  // namespace MoTool
