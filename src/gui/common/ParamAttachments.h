#pragma once

#include <JuceHeader.h>
#include "../../util/convert.h"
#include "../../util/enumchoice.h"
#include "../../controllers/Parameters.h"

using namespace juce;
namespace te = tracktion;

namespace MoTool {

//==============================================================================
class AutoParamAttachment : private te::AutomatableParameter::Listener {
public:
    AutoParamAttachment(te::AutomatableParameter::Ptr p)
        : param(std::move(p))
    {
        if (param != nullptr) {
            param->addListener(this);
        }
    }

    ~AutoParamAttachment() override {
        if (isAttached()) {
            param->removeListener(this);
        }
    }

    bool isAttached() const noexcept {
        return param != nullptr;
    }

protected:
    void curveHasChanged(te::AutomatableParameter&) override {}

    te::AutomatableParameter::Ptr param;
};

//==============================================================================
class SliderAutoParamAttachment : public AutoParamAttachment {
public:
    // TODO maybe pass TracktionParamSource instead of ParameterValue<Type>?
    // template <typename Type>
    // SliderAutoParamAttachment(Slider& s, ParameterValue<Type>& value)
    //     : SliderAutoParamAttachment(s, value.isSourceAttached() ? value.source->parameter : nullptr)
    // {}

    SliderAutoParamAttachment(Slider& s, te::AutomatableParameter::Ptr p)
        : AutoParamAttachment(std::move(p))
        , slider(s)
    {
        if (param == nullptr) {
            // slider.getValueObject().referTo(value.value);
            return;
        }

        slider.onValueChange = [this] {
            juce::ScopedValueSetter<bool> svs(updating, true);
            // TODO value.setParameter((Type)slider.getValue());
            param->setParameter((float)slider.getValue(), juce::sendNotification);
        };
        slider.onDragStart = [&]{
            // TODO value.changeGestureBegin();
            param->parameterChangeGestureBegin();
        };
        slider.onDragEnd   = [&]{ param->parameterChangeGestureEnd(); };

        slider.setValue((double)param->getCurrentValue(), juce::dontSendNotification);
    }

private:
    // called from MessageThread (see AsyncCaller)
    void currentValueChanged(te::AutomatableParameter& p) override {
        if (updating)
            return;  // don't update the parameter if we're already updating it
        slider.setValue(static_cast<double>(p.getCurrentValue()), dontSendNotification);
    }

    Slider& slider;
    bool updating { false };
};

// //==============================================================================
// class SliderValueAttachment {
// public:
//     template <typename Type>
//     SliderValueAttachment(Slider& slider, ParameterValue<Type>& value)
//         : slider(s)
//     {
//         slider.getValueObject().referTo(value.getPropertyAsValue());
//     }

// private:
//     // Slider& slider;
// };

// //==============================================================================
// class ButtonAutoParamAttachment : public AutoParamAttachment {
// public:
//     template <Util::EnumChoiceConcept Type>
//     ButtonAutoParamAttachment(TextButton& b, ParameterValue<Type>& value)
//         : ButtonAutoParamAttachment(b, value.isSourceAttached() ? value.source->parameter : nullptr)
//     {}

//     ButtonAutoParamAttachment(TextButton& b, te::AutomatableParameter::Ptr p)
//         : AutoParamAttachment(std::move(p))
//         , button(b)
//     {
//         if (param == nullptr) {
//             return;
//         }

//         button.onStateChange = [this] {
//             juce::ScopedValueSetter<bool> svs(updating, true);
//             param->setParameter((float)slider.getValue(), juce::sendNotification);
//         };

//         slider.setValue((double)param->getCurrentValue(), juce::dontSendNotification);
//     }

// private:
//     // called from MessageThread (see AsyncCaller)
//     void currentValueChanged(te::AutomatableParameter& p) override {
//         if (updating)
//             return;  // don't update the parameter if we're already updating it
//         int index = roundToInt(p.getCurrentValue());
//         button.setValue(static_cast<double>(p.getCurrentValue()), dontSendNotification);
//     }

//     // text - value functions
//     std::function<String(float)> valueToStringFunction;
//     std::function<float(const String&)> stringToValueFunction;

//     TextButton& button;
//     bool updating { false };
// };


}  // namespace MoTool
