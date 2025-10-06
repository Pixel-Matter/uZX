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

namespace MoTool {


//==============================================================================
/**
    Base bridge between a UI control and a tracktion `AutomatableParameter`.

    The binding keeps the control and automation parameter in sync, dispatching
    gesture callbacks and optionally falling back to a stored `ParameterValue`
    when the plugin doesn't expose a live parameter.
*/
class WidgetParamBindingBase : private te::AutomatableParameter::Listener,
                               private Value::Listener
{
public:
    WidgetParamBindingBase(te::AutomatableParameter::Ptr p);

    template <typename Type>
    WidgetParamBindingBase(te::AutomatableParameter::Ptr p, ParameterValue<Type>& value)
        : WidgetParamBindingBase(std::move(p))
    {
        if (param == nullptr)
            configureStoredValueCallbacks(value);
    }

    ~WidgetParamBindingBase() override;

    bool isAttached() const noexcept {
        return param != nullptr;
    }

protected:
    void curveHasChanged(te::AutomatableParameter&) override {}

    te::AutomatableParameter::Ptr param;
    std::function<float()> fetchValue;
    std::function<void(float)> applyValue;
    std::function<void()> beginGesture;
    std::function<void()> endGesture;
    bool updating { false };

private:
    void configureAutomationCallbacks();

    template <typename ParameterValueType>
    void configureStoredValueCallbacks(ParameterValueType& parameterValue);

    // To be able to listen to it
    Value storedValue;
    bool listensToValue { false };
};

template <typename ParameterValueType>
void WidgetParamBindingBase::configureStoredValueCallbacks(ParameterValueType& parameterValue) {
    listensToValue = true;
    storedValue = parameterValue.getPropertyAsValue();
    storedValue.addListener(this);
    using Traits = ParameterConversionTraits<typename ParameterValueType::Type>;

    // TODO invert control?
    // fetchValue = parameterValue.getFetcher()
    // applyValue = parameterValue.getApplier()

    fetchValue = [&parameterValue]() {
        return static_cast<float>(Traits::toFloat(parameterValue.getStoredValue()));
    };

    applyValue = [&parameterValue](float value) {
        parameterValue.setStoredValue(Traits::fromFloat(value));
    };
}


//==============================================================================
/**
    Attaches a `juce::Slider` to either an automation parameter or a
    `ParameterValue`, keeping MIDI learn and mouse gesture support wired up.
*/
class SliderParamBinding : public WidgetParamBindingBase
{
public:
    SliderParamBinding(Slider& s, te::AutomatableParameter::Ptr p)
        : WidgetParamBindingBase(std::move(p))
        , midiMapping(param)
        , mouseListener(s)
        , slider(s)
    {
        configureSliderForAutomationParameter();
        configureSliderHandlers();
        configureMouseListener();
        refreshFromSource();
    }

    template <typename Type>
    SliderParamBinding(Slider& s, te::AutomatableParameter::Ptr p, ParameterValue<Type>& value)
        : WidgetParamBindingBase(std::move(p), value)
        , midiMapping(param)
        , mouseListener(s)
        , slider(s)
    {
        ParameterUIHelpers::configureSliderForParameterDef(slider, value.definition);
        configureSliderHandlers();
        configureMouseListener();
        refreshFromSource();
    }

    // TODO only for values attached to automatable parameters?
    MidiParameterMapping midiMapping;

private:
    void configureSliderForAutomationParameter();
    void configureSliderHandlers();
    void configureMouseListener();
    void refreshFromSource();

    void currentValueChanged(te::AutomatableParameter&) override;
    void valueChanged(Value&) override;

    MouseListenerWithCallback mouseListener;
    Slider& slider;
};

//==============================================================================
/**
    Attaches a `TextButton` to a parameter. When used with enum parameters the
    binding cycles through the available choices while honouring optional custom
    string converters from the parameter definition.
*/
class ButtonParamBinding : public WidgetParamBindingBase
{
public:
    ButtonParamBinding(TextButton& button,
                       te::AutomatableParameter::Ptr p)
        : WidgetParamBindingBase(std::move(p))
        , midiMapping(param)
        , textButton(button)
        , mouseListener(std::make_unique<MouseListenerWithCallback>(button))
    {
        configureMouseListener();
        configureButtonHandlers();
        refreshFromSource();
    }

    template <Util::EnumChoiceConcept Type>
    ButtonParamBinding(TextButton& button,
                       te::AutomatableParameter::Ptr p,
                       ParameterValue<Type>& value)
        : WidgetParamBindingBase(std::move(p), value)
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
    std::function<float(int)> indexToFloatValue;
    std::function<int(float)> floatValueToIndex;

    int choiceCount { 0 };

    template <Util::EnumChoiceConcept Type>
    void configureFromParameterValue(ParameterValue<Type>& value);
};

template <Util::EnumChoiceConcept Type>
void ButtonParamBinding::configureFromParameterValue(ParameterValue<Type>& value) {
    DBG("ButtonParamBinding::configureFromParameterValue for " << value.definition.identifier);
    choiceCount = static_cast<int>(Type::size());

    textButton.setTooltip(value.definition.description);

    using Traits = ParameterConversionTraits<Type>;

    indexToFloatValue = [](int index) -> float {
        auto typedValue = Type(index);
        return static_cast<float>(Traits::toFloat(typedValue));
    };

    floatValueToIndex = [](float sliderValue) -> int {
        auto typedValue = Traits::fromFloat(sliderValue);
        return static_cast<int>(Traits::toStorage(typedValue));
    };

    if (auto valueToString = value.definition.valueToStringFunction) {
        indexToLabel = [valueToString](int index) -> String {
            using LocalTraits = ParameterConversionTraits<Type>;
            auto typedValue = LocalTraits::fromFloat(static_cast<float>(index));
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
