#pragma once

#include <JuceHeader.h>
#include "../../controllers/ParamAttachments.h"
#include "../../controllers/Parameters.h"
#include "../../util/enumchoice.h"
#include "MidiParameterMapping.h"
#include "ParameterSliderHelpers.h"

namespace MoTool {

namespace te = tracktion;

class ChoiceButton : public TextButton,
                     private ChangeListener  // MidiMapping change listener
{
public:
    template <Util::EnumChoiceConcept Type>
    ChoiceButton(te::Plugin& plugin, ParameterValue<Type>& value)
        : attachment(slider, value, plugin.getAutomatableParameterByID(value.definition.paramID))
        , midiMapping(plugin.getAutomatableParameterByID(value.definition.paramID))
    {
        const auto& def = value.definition;

        ParameterUIHelpers::configureSliderForParameter(slider, def, value, sliderStyle);

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

    inline static constexpr Slider::SliderStyle sliderStyle = Slider::SliderStyle::LinearHorizontal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabeledSlider)
};

}  // namespace MoTool
