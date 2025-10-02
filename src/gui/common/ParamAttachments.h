#pragma once

#include <JuceHeader.h>
#include <utility>
#include <functional>

#include "../../controllers/Parameters.h"
#include "ParameterSliderHelpers.h"
#include "MidiParameterMapping.h"
#include "MouseListener.h"

using namespace juce;
namespace te = tracktion;

namespace MoTool {


//==============================================================================
class AutoParamAttachment : private te::AutomatableParameter::Listener,
                            private Value::Listener
{
public:
    AutoParamAttachment(te::AutomatableParameter::Ptr p)
        : param(std::move(p))
    {
        if (param != nullptr) {
            param->addListener(this);
            configureAutomationCallbacks();
        }
    }

    template <typename Type>
    AutoParamAttachment(te::AutomatableParameter::Ptr p, ParameterValue<Type>& value)
        : AutoParamAttachment(std::move(p))
    {
        if (param == nullptr)
            configureStoredValueCallbacks(value);
    }

    ~AutoParamAttachment() override {
        if (isAttached()) {
            param->removeListener(this);
        }
        if (listensToValue)
            storedValue.removeListener(this);
    }

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
    void configureAutomationCallbacks() {
        fetchValue = [this]() { return static_cast<double>(param->getCurrentValue()); };
        applyValue = [this](double sliderValue) {
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

        fetchValue = [&parameterValue]() {
            using LocalTraits = ParameterStorageTraits<typename ParameterValueType::Type>;
            return static_cast<double>(LocalTraits::toSliderValue(parameterValue.getStoredValue()));
        };

        applyValue = [&parameterValue](double sliderValue) {
            using LocalTraits = ParameterStorageTraits<typename ParameterValueType::Type>;
            parameterValue.setStoredValue(LocalTraits::fromSliderValue(sliderValue));
        };
    }

    Value storedValue;
    bool listensToValue { false };
};

//==============================================================================
class SliderAutoParamAttachment : public AutoParamAttachment
{
public:
    SliderAutoParamAttachment(Slider& s, te::AutomatableParameter::Ptr p)
        : AutoParamAttachment(std::move(p))
        , midiMapping(param)
        , mouseListener(s)
        , slider(s)
    {
        configureSliderHandlers();
        configureMouseListener();
    }

    template <typename Type>
    SliderAutoParamAttachment(Slider& s, te::AutomatableParameter::Ptr p, ParameterValue<Type>& value)
        : SliderAutoParamAttachment(s, p)
    {
        ParameterUIHelpers::configureSliderForParameterValue(slider, value);
    }

    ~SliderAutoParamAttachment() override {
    }

    MidiParameterMapping midiMapping;

private:
    void configureSliderHandlers() {
        refreshFromSource();

        slider.onValueChange = [this]() {
            if (updating || !applyValue)
                return;

            juce::ScopedValueSetter<bool> svs(updating, true);
            applyValue(slider.getValue());
        };

        slider.onDragStart = [this]() {
            if (beginGesture)
                beginGesture();
        };

        slider.onDragEnd = [this]() {
            if (endGesture)
                endGesture();
        };

        slider.setPopupMenuEnabled(false);
    }

    void configureMouseListener() {
        mouseListener.setRmbCallback([this]() {
            midiMapping.showMappingMenu();
        });
    }

    void refreshFromSource() {
        if (!fetchValue)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        slider.setValue(fetchValue(), dontSendNotification);
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

    MouseListenerWithCallback mouseListener;
    Slider& slider;
};

}  // namespace MoTool
