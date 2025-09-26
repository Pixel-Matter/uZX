#pragma once

#include <JuceHeader.h>
#include "../../controllers/ParamAttachments.h"
#include "../../plugins/uZX/midi_effects/Parameters.h"
#include "MidiParameterMapping.h"

namespace MoTool {

namespace te = tracktion;

class LabeledKnob : public Component {
public:
    LabeledKnob(const uZX::ValueWithSource<float>& value);

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
        SliderMouseListener(LabeledKnob& ownerRef) : owner(ownerRef) {}
        void mouseDown(const MouseEvent& e) override {
            if (e.mods.isRightButtonDown()) {
                owner.showParameterMenu();
            }
        }
    private:
        LabeledKnob& owner;
    };

    Slider slider;
    Label label;

    SliderAttachment attachment;
    MidiParameterMapping midiMapping;
    SliderMouseListener mouseListener;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabeledKnob)
};

}  // namespace MoTool