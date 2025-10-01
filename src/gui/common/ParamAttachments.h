#pragma once

#include <JuceHeader.h>
#include <concepts>
#include <utility>
#include <functional>
#include "../../util/convert.h"
#include "../../util/enumchoice.h"
#include "../../controllers/Parameters.h"

using namespace juce;
namespace te = tracktion;

namespace MoTool {

template <typename T>
concept ParameterValueLike = requires(T& t) {
    typename T::Type;
    typename ParameterStorageTraits<typename T::Type>;
    { t.getStoredValue() };
    { t.setStoredValue(std::declval<typename T::Type>()) };
    { t.getPropertyAsValue() };
};

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
class SliderAutoParamAttachment : public AutoParamAttachment, private Value::Listener {
public:
    SliderAutoParamAttachment(Slider& s, te::AutomatableParameter::Ptr p)
        : AutoParamAttachment(std::move(p))
        , slider(s)
    {
        if (param != nullptr)
            configureAutomationCallbacks();

        configureSliderHandlers();
    }

    SliderAutoParamAttachment(Slider& s, te::AutomatableParameter& p)
        : SliderAutoParamAttachment(s, te::AutomatableParameter::Ptr(&p))
    {}

    template <typename ParameterValueType>
        requires ParameterValueLike<ParameterValueType>
    SliderAutoParamAttachment(Slider& s, ParameterValueType& parameterValue, te::AutomatableParameter::Ptr automationParam = {})
        : AutoParamAttachment(std::move(automationParam))
        , slider(s)
    {
        if (param != nullptr)
            configureAutomationCallbacks();
        else
            configureStoredValueCallbacks(parameterValue);

        configureSliderHandlers();
    }

    ~SliderAutoParamAttachment() override {
        if (listensToValue)
            storedValue.removeListener(this);
    }

private:
    void configureAutomationCallbacks() {
        fetchSliderValue = [this]() { return static_cast<double>(param->getCurrentValue()); };
        applySliderValue = [this](double sliderValue) {
            param->setParameter(static_cast<float>(sliderValue), juce::sendNotification);
        };
        beginGesture = [this]() { param->parameterChangeGestureBegin(); };
        endGesture   = [this]() { param->parameterChangeGestureEnd(); };
    }

    template <typename ParameterValueType>
    void configureStoredValueCallbacks(ParameterValueType& parameterValue) {
        listensToValue = true;
        storedValue = parameterValue.getPropertyAsValue();
        storedValue.addListener(this);

        fetchSliderValue = [&parameterValue]() {
            using LocalTraits = ParameterStorageTraits<typename ParameterValueType::Type>;
            return static_cast<double>(LocalTraits::toSliderValue(parameterValue.getStoredValue()));
        };

        applySliderValue = [&parameterValue](double sliderValue) {
            using LocalTraits = ParameterStorageTraits<typename ParameterValueType::Type>;
            parameterValue.setStoredValue(LocalTraits::fromSliderValue(sliderValue));
        };
    }

    void configureSliderHandlers() {
        refreshFromSource();

        slider.onValueChange = [this]() {
            if (updating || !applySliderValue)
                return;

            juce::ScopedValueSetter<bool> svs(updating, true);
            applySliderValue(slider.getValue());
        };

        slider.onDragStart = [this]() {
            if (beginGesture)
                beginGesture();
        };

        slider.onDragEnd = [this]() {
            if (endGesture)
                endGesture();
        };
    }

    void refreshFromSource() {
        if (!fetchSliderValue)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        slider.setValue(fetchSliderValue(), dontSendNotification);
    }

    void currentValueChanged(te::AutomatableParameter&) override {
        if (updating)
            return;

        refreshFromSource();
    }

    void valueChanged(Value&) override {
        if (updating)
            return;

        refreshFromSource();
    }

    Slider& slider;
    bool updating { false };
    Value storedValue;
    bool listensToValue { false };

    std::function<double()> fetchSliderValue;
    std::function<void(double)> applySliderValue;
    std::function<void()> beginGesture;
    std::function<void()> endGesture;
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
