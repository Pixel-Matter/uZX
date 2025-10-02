#pragma once

#include <JuceHeader.h>
#include "../../controllers/Parameters.h"

#include "ParamAttachments.h"
#include "MouseListener.h"
#include "MidiParameterMapping.h"
#include "ParameterSliderHelpers.h"

namespace MoTool {

namespace te = tracktion;

class LabeledSlider : public Component,
                      private ChangeListener  // MidiMapping change listener
{
public:
    template <typename Type = float>
    LabeledSlider(te::Plugin& plugin, ParameterValue<Type>& value, Slider::SliderStyle style = Slider::RotaryVerticalDrag)
        : attachment(slider, plugin.getAutomatableParameterByID(value.definition.paramID), value)
    {
        slider.setSliderStyle(style);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        // TODO pass label into ctor?
        label.setText(value.definition.shortLabel, dontSendNotification);
        label.setJustificationType(Justification::centred);
        label.setFont(label.getFont().withPointHeight((float) labelHeight));

        addAndMakeVisible(slider);
        addAndMakeVisible(label);

        attachment.midiMapping.addChangeListener(this);
    }

    ~LabeledSlider() override {
        attachment.midiMapping.removeChangeListener(this);
    }

    void resized() override;
    void paint(Graphics& g) override;

    inline static constexpr int labelHeight = 10;
    inline static constexpr int labelOverlap = 2;

    int getLabelHeight() const { return labelHeight - labelOverlap; }

private:
    // listener to update on mapping changes
    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &attachment.midiMapping) {
            repaint();
        }
    }

    Slider slider;
    Label label;

    SliderAutoParamAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabeledSlider)
};

}  // namespace MoTool
