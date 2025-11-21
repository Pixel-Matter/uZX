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
    SliderParamEndpointBinding(Slider& s, ParameterValue<Type>& paramValue, te::AutomatableParameter::Ptr parameterIn = {})
        : SliderParamEndpointBinding(s, makeResolveParamEndpoint(paramValue, std::move(parameterIn)))
    {}

private:
    void configureWidget();
    void configureWidgetHandlers();
    void refreshFromSource() override;

    Slider& slider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliderParamEndpointBinding)
};

//==============================================================================
class ComboBoxParamEndpointBinding : public WidgetParamEndpointBinding {
public:
    ComboBoxParamEndpointBinding(ComboBox& c, auto&& endpoint)
        : WidgetParamEndpointBinding(c, std::forward<decltype(endpoint)>(endpoint))
        , comboBox(c)
    {
        configureWidget();
        configureWidgetHandlers();
        refreshFromSource();
    }

    template <typename Type>
    ComboBoxParamEndpointBinding(ComboBox& s, ParameterValue<Type>& paramValue, te::AutomatableParameter::Ptr parameterIn = {})
        : ComboBoxParamEndpointBinding(s, makeResolveParamEndpoint(paramValue, std::move(parameterIn)))
    {}

private:
    void configureWidget();
    void configureWidgetHandlers();
    void refreshFromSource() override;

    void fillItems();

    ComboBox& comboBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ComboBoxParamEndpointBinding)
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

    template <typename Type>
    ButtonParamEndpointBinding(Button& b, ParameterValue<Type>& paramValue, te::AutomatableParameter::Ptr parameterIn = {})
        : ButtonParamEndpointBinding(b, makeResolveParamEndpoint(paramValue, std::move(parameterIn)))
    {}

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

//============================================================================
class ToggleParamEndpointBinding : public WidgetParamEndpointBinding {
public:
    ToggleParamEndpointBinding(Button& b, auto&& endpoint, const String& label)
        : WidgetParamEndpointBinding(b, std::forward<decltype(endpoint)>(endpoint))
        , button(b)
        , buttonLabel(label)
    {
        configureWidget();
        configureWidgetHandlers();
        refreshFromSource();
    }

    template <typename Type>
    ToggleParamEndpointBinding(Button& b, ParameterValue<Type>& paramValue, te::AutomatableParameter::Ptr parameterIn = {})
        : ToggleParamEndpointBinding(b, makeResolveParamEndpoint(paramValue, std::move(parameterIn)), paramValue.definition.shortLabel)
    {}

private:
    void configureWidget();
    void configureWidgetHandlers();
    void refreshFromSource() override;

    Button& button;
    String buttonLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToggleParamEndpointBinding)
};


}  // namespace MoTool
