#pragma once

#include <JuceHeader.h>
// #include <common/Utilities.h>


using namespace juce;
namespace te = tracktion;

namespace MoTool {


class ParameterSliderAttachment : private te::AutomatableParameter::Listener {
public:
    ParameterSliderAttachment (Slider& s, te::AutomatableParameter& p)
        : slider (s)
        , param (p)
    {
        slider.setRange(p.getValueRange().getStart(), p.getValueRange().getEnd(), 0.0);
        slider.setValue(p.getCurrentValue(), dontSendNotification);

        slider.onValueChange = [this] {
            juce::ScopedValueSetter<bool> svs(updatingSlider, true);
            param.setParameter((float)slider.getValue(), juce::sendNotification);
        };
        slider.setPopupDisplayEnabled(true, false, nullptr);
        slider.onDragStart = [&]{ param.parameterChangeGestureBegin(); };
        slider.onDragEnd   = [&]{ param.parameterChangeGestureEnd(); };

        param.addListener(this);
    }

    ~ParameterSliderAttachment() override {
        param.removeListener(this);
    }

private:
    // called from MessageThread (see AsyncCaller)
    void currentValueChanged(te::AutomatableParameter& p) override {
        if (updatingSlider)
            return;  // don't update the parameter if we're already updating it
        slider.setValue(p.getCurrentValue(), dontSendNotification);
    }

    void curveHasChanged(te::AutomatableParameter&) override {}

    Slider& slider;
    te::AutomatableParameter& param;
    bool updatingSlider { false };
    // bool updatingFromParam { false };
};


}  // namespace MoTool
