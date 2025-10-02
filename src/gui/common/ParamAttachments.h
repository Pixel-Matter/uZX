#pragma once

#include <JuceHeader.h>
#include <utility>
#include <functional>
#include <memory>

#include "../../controllers/Parameters.h"
#include "ParameterSliderHelpers.h"
#include "MidiParameterMapping.h"
#include "MouseListener.h"

using namespace juce;
namespace te = tracktion;

namespace MoTool {


//==============================================================================
class ParamBindingBase : private te::AutomatableParameter::Listener,
                         private Value::Listener
{
public:
    ParamBindingBase(te::AutomatableParameter::Ptr p);

    template <typename Type>
    ParamBindingBase(te::AutomatableParameter::Ptr p, ParameterValue<Type>& value)
        : ParamBindingBase(std::move(p))
    {
        if (param == nullptr)
            configureStoredValueCallbacks(value);
    }

    ~ParamBindingBase() override;

    bool isAttached() const noexcept {
        return param != nullptr;
    }

protected:
    void curveHasChanged(te::AutomatableParameter&) override {}

    te::AutomatableParameter::Ptr param;
    std::function<double()> fetchValue;
    std::function<void(double)> applyValue;
    std::function<void()> beginGesture;
    std::function<void()> endGesture;
    bool updating { false };

private:
    void configureAutomationCallbacks();

    template <typename ParameterValueType>
    void configureStoredValueCallbacks(ParameterValueType& parameterValue);

    Value storedValue;
    bool listensToValue { false };
};

//==============================================================================
class SliderParamBinding : public ParamBindingBase
{
public:
    SliderParamBinding(Slider& s, te::AutomatableParameter::Ptr p);

    template <typename Type>
    SliderParamBinding(Slider& s, te::AutomatableParameter::Ptr p, ParameterValue<Type>& value)
        : SliderParamBinding(s, p)
    {
        ParameterUIHelpers::configureSliderForParameterValue(slider, value);
    }

    ~SliderParamBinding() override;

    MidiParameterMapping midiMapping;

private:
    void configureSliderHandlers();
    void configureMouseListener();
    void refreshFromSource();

    void currentValueChanged(te::AutomatableParameter&) override;
    void valueChanged(Value&) override;

    MouseListenerWithCallback mouseListener;
    Slider& slider;
};

//==============================================================================
class ButtonParamBinding : public ParamBindingBase
{
public:
    template <Util::EnumChoiceConcept Type>
    ButtonParamBinding(TextButton& button,
                              te::AutomatableParameter::Ptr p,
                              ParameterValue<Type>& value)
        : ParamBindingBase(std::move(p), value)
        , midiMapping(param)
        , textButton(button)
        , mouseListener(std::make_unique<MouseListenerWithCallback>(button))
    {
        configureFromParameterValue(value);
        configureMouseListener();
        configureButtonHandlers();
        refreshFromSource();
    }

    ~ButtonParamBinding() override;

    MidiParameterMapping midiMapping;

private:
    void configureButtonHandlers();
    void configureMouseListener();
    void refreshFromSource();

    void currentValueChanged(te::AutomatableParameter&) override;
    void valueChanged(Value&) override;

    void handleClick();
    int getCurrentIndex() const;
    int wrapIndex(int index) const;

    TextButton& textButton;
    std::unique_ptr<MouseListenerWithCallback> mouseListener;
    std::function<String(int)> indexToLabel;
    std::function<double(int)> indexToSliderValue;
    std::function<int(double)> sliderValueToIndex;

    int choiceCount { 0 };

    template <Util::EnumChoiceConcept Type>
    void configureFromParameterValue(ParameterValue<Type>& value);
};

template <Util::EnumChoiceConcept Type>
void ButtonParamBinding::configureFromParameterValue(ParameterValue<Type>& value) {
    choiceCount = static_cast<int>(Type::size());

    textButton.setTooltip(value.definition.description);

    using Traits = ParameterStorageTraits<Type>;

    indexToSliderValue = [](int index) -> double {
        auto typedValue = Type(index);
        return static_cast<double>(Traits::toFloatValue(typedValue));
    };

    sliderValueToIndex = [](double sliderValue) -> int {
        auto typedValue = Traits::fromFloatValue(sliderValue);
        return static_cast<int>(Traits::toStorage(typedValue));
    };

    if (auto valueToString = value.definition.valueToStringFunction) {
        indexToLabel = [valueToString](int index) -> String {
            using LocalTraits = ParameterStorageTraits<Type>;
            auto typedValue = LocalTraits::fromFloatValue(static_cast<typename LocalTraits::SliderValue>(index));
            return valueToString(typedValue);
        };
    } else {
        indexToLabel = [](int index) -> String {
            Type typedValue(index);
            auto label = typedValue.getLabel();
            return String(label.data(), static_cast<size_t>(label.size()));
        };
    }
}

}  // namespace MoTool
