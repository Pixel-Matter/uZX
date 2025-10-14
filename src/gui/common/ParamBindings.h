#pragma once

#include <JuceHeader.h>
#include <utility>
#include <functional>
#include <memory>

#include "../../controllers/Parameters.h"
#include "../../controllers/ParamEndpoint.h"

#include "ParameterSliderHelpers.h"
#include "MidiParameterMapping.h"
#include "MouseListener.h"

using namespace juce;

namespace MoTool {


//==============================================================================
namespace detail {
template <ParameterValueConcept ParameterValueType>
inline te::AutomatableParameter::Ptr resolveParamFromValue(ParameterValueType& value,
                                                           te::AutomatableParameter::Ptr param = {})
{
    if (param != nullptr)
        return param;

    if (auto* context = value.getLiveContext())
        if (auto* liveParam = dynamic_cast<te::AutomatableParameter*>(static_cast<te::AutomatableParameter*>(context)))
            return te::AutomatableParameter::Ptr(liveParam);

    return {};
}
} // namespace detail

//==============================================================================
class WidgetParamEndpointBinding : private ParameterEndpoint::Listener {
public:
    WidgetParamEndpointBinding(Component &c, std::unique_ptr<ParameterEndpoint> endpointIn);

    WidgetParamEndpointBinding(Component &c, te::AutomatableParameter::Ptr parameterIn);

    template <typename Type>
    WidgetParamEndpointBinding(Component &c, ParameterValue<Type>& paramValue, te::AutomatableParameter::Ptr parameterIn = {})
        : WidgetParamEndpointBinding(c, makeResolveParamEndpoint(paramValue, std::move(parameterIn)))
    {}

    ~WidgetParamEndpointBinding() override;

    MidiParameterMapping midiMapping;
    MouseListenerWithCallback mouseListener;

    ParameterEndpoint& endpoint() noexcept;
    const ParameterEndpoint& endpoint() const noexcept;

protected:
    virtual void refreshFromSource() = 0;

    bool updating { false };

private:
    void storedValueChanged(ParameterEndpoint&, float) override;
    void liveValueChanged(ParameterEndpoint&, float) override;

    std::unique_ptr<ParameterEndpoint> ownedEndpoint;

    friend class WidgetBindingTests;
};

//==============================================================================
class SliderParamEndpointBinding : public WidgetParamEndpointBinding {
public:
    SliderParamEndpointBinding(Slider& s, auto&& endpoint)
        : WidgetParamEndpointBinding(s, std::forward<decltype(endpoint)>(endpoint))
        , slider(s)
    {
        configureWidget();
        configureWidgetHandlers();
        refreshFromSource();
    }

    template <typename Type>
    SliderParamEndpointBinding(Slider &s, ParameterValue<Type>& paramValue, te::AutomatableParameter::Ptr parameterIn = {})
        : SliderParamEndpointBinding(s, makeResolveParamEndpoint(paramValue, std::move(parameterIn)))
    {}

private:
    void configureWidget();
    void configureWidgetHandlers();
    void refreshFromSource() override;

    Slider& slider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliderParamEndpointBinding)
};

//============================================================================
class ButtonParamEndpointBinding : public WidgetParamEndpointBinding {
public:
    ButtonParamEndpointBinding(Button& b, auto&& endpoint)
        : WidgetParamEndpointBinding(b, std::forward<decltype(endpoint)>(endpoint))
        , button(b)
    {
        configureWidget();
        configureWidgetHandlers();
        refreshFromSource();
    }

private:
    void configureWidget();
    void configureWidgetHandlers();
    void refreshFromSource() override;

    void handleClick();
    int getCurrentIndex() const;
    int wrapIndex(int index) const;

    Button& button;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ButtonParamEndpointBinding)
};


//==============================================================================
// Below is legacy deprecated classes, change to WidgetParamEndpointBinding
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
    WidgetParamBindingBase(Component &c, te::AutomatableParameter::Ptr p);

    template <typename Type>
    WidgetParamBindingBase(Component &c, te::AutomatableParameter::Ptr p, ParameterValue<Type>& value)
        : WidgetParamBindingBase(c, detail::resolveParamFromValue(value, std::move(p)))
    {
        configureStoredValueCallbacks(value);
        configureMouseListener();
    }

    ~WidgetParamBindingBase() override;

    bool isAttached() const noexcept;

protected:
    void curveHasChanged(te::AutomatableParameter&) override {}
    void configureMouseListener();

    te::AutomatableParameter::Ptr param;
    std::function<float()> fetchValue;
    std::function<void(float)> applyValue;
    std::function<void()> beginGesture;
    std::function<void()> endGesture;
    bool updating { false };

public:
    MidiParameterMapping midiMapping;
    MouseListenerWithCallback mouseListener;

private:
    void configureAutomationCallbacks();

    template <typename ParameterValueType>
    void configureStoredValueCallbacks(ParameterValueType& parameterValue);

    // To be able to listen to it
    Value storedValue;
    bool listensToValue { false };

    void attachParameter(te::AutomatableParameter::Ptr newParam);

    void detachParameter();
};

template <typename ParameterValueType>
void WidgetParamBindingBase::configureStoredValueCallbacks(ParameterValueType& parameterValue) {
    listensToValue = true;
    storedValue = parameterValue.getPropertyAsValue();
    storedValue.addListener(this);
    using Traits = ParameterConversionTraits<typename ParameterValueType::Type>;

    if (!isAttached()) {
        fetchValue = [&parameterValue]() {
            return static_cast<float>(Traits::template to<float>(parameterValue.getStoredValue()));
        };

        applyValue = [&parameterValue](float value) {
            parameterValue.setStoredValue(Traits::from(value));
        };
    }
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
        : WidgetParamBindingBase(s, std::move(p))
        , slider(s)
    {
        configureSliderForAutomationParameter();
        configureSliderHandlers();
        refreshFromSource();
    }

    template <typename Type>
    SliderParamBinding(Slider& s, te::AutomatableParameter::Ptr p, ParameterValue<Type>& value)
        : WidgetParamBindingBase(s, std::move(p), value)
        , slider(s)
    {
        ParameterUIHelpers::configureSliderForParameterDef(slider, value.definition);
        configureSliderHandlers();
        refreshFromSource();
    }

    ~SliderParamBinding() override;

private:
    void configureSliderForAutomationParameter();
    void configureSliderHandlers();
    void refreshFromSource();

    void currentValueChanged(te::AutomatableParameter&) override;
    void valueChanged(Value&) override;

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
        : WidgetParamBindingBase(button, std::move(p))
        , textButton(button)
    {
        configureButtonHandlers();
        refreshFromSource();
    }

    template <Util::EnumChoiceConcept Type>
    ButtonParamBinding(TextButton& button,
                       te::AutomatableParameter::Ptr p,
                       ParameterValue<Type>& value)
        : WidgetParamBindingBase(button, std::move(p), value)
        , textButton(button)
    {
        configureFromParameterValue(value);
        configureButtonHandlers();
        refreshFromSource();
    }

    ~ButtonParamBinding() override;

private:
    void configureButtonHandlers();
    void refreshFromSource();

    void currentValueChanged(te::AutomatableParameter&) override;
    void valueChanged(Value&) override;

    void handleClick();
    int getCurrentIndex() const;
    int wrapIndex(int index) const;

    TextButton& textButton;
    std::function<String(int)> indexToLabel;

    int choiceCount { 0 };

    template <Util::EnumChoiceConcept Type>
    void configureFromParameterValue(ParameterValue<Type>& value);
};

template <Util::EnumChoiceConcept Type>
void ButtonParamBinding::configureFromParameterValue(ParameterValue<Type>& value) {
    choiceCount = static_cast<int>(Type::size());

    textButton.setTooltip(value.definition.description);

    using Traits = ParameterConversionTraits<Type>;

    if (auto valueToString = value.definition.valueToStringFunction) {
        indexToLabel = [valueToString](int index) -> String {
            auto typedValue = Traits::from(index);
            return valueToString(typedValue);
        };
    } else {
        indexToLabel = [](int index) -> String {
            auto label = Type(index).getLabel();
            return String(label.data());
        };
    }
}

}  // namespace MoTool
