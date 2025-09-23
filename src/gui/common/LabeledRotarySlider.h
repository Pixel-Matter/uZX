#pragma once

#include <JuceHeader.h>
#include "../../controllers/ParamAttachments.h"
#include "../../plugins/uZX/midi_effects/Parameters.h"
#include "MidiParameterMapping.h"

namespace MoTool {

namespace te = tracktion;

class LabeledRotarySlider : public Component {
public:
    LabeledRotarySlider(te::AutomatableParameter::Ptr param, const String& labelText = {}, const String& tooltip = {}, const String& valueSuffix = {});

    LabeledRotarySlider(te::AutomatableParameter::Ptr param, const uZX::ValueWithSource<float>& value);

    LabeledRotarySlider(const uZX::ValueWithSource<float>& value);

    void resized() override;
    void paint(Graphics& g) override;

    Slider& getSlider() { return slider; }
    Label& getLabel() { return label; }

    inline static constexpr int labelHeight = 10;
    inline static constexpr int labelOverlap = 2;

    int getLabelHeight() const { return labelHeight - labelOverlap; }

private:
    void showParameterMenu();

    class SliderMouseListener : public MouseListener {
    public:
        SliderMouseListener(LabeledRotarySlider& ownerRef) : owner(ownerRef) {}
        void mouseDown(const MouseEvent& e) override {
            if (e.mods.isRightButtonDown()) {
                owner.showParameterMenu();
            }
        }
    private:
        LabeledRotarySlider& owner;
    };

    Slider slider;
    Label label;
    SliderAttachment attachment;
    te::AutomatableParameter::Ptr parameter;
    SliderMouseListener mouseListener;
    MidiParameterMapping midiMapping;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabeledRotarySlider)
};

}  // namespace MoTool