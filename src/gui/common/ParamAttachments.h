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
    AutoParamAttachment(te::AutomatableParameter::Ptr p);

    template <typename Type>
    AutoParamAttachment(te::AutomatableParameter::Ptr p, ParameterValue<Type>& value)
        : AutoParamAttachment(std::move(p))
    {
        if (param == nullptr)
            configureStoredValueCallbacks(value);
    }

    ~AutoParamAttachment() override;

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
class SliderAutoParamAttachment : public AutoParamAttachment
{
public:
    SliderAutoParamAttachment(Slider& s, te::AutomatableParameter::Ptr p);

    template <typename Type>
    SliderAutoParamAttachment(Slider& s, te::AutomatableParameter::Ptr p, ParameterValue<Type>& value)
        : SliderAutoParamAttachment(s, p)
    {
        ParameterUIHelpers::configureSliderForParameterValue(slider, value);
    }

    ~SliderAutoParamAttachment() override;

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

}  // namespace MoTool
