#include "ParamAttachments.h"

using namespace juce;
namespace te = tracktion;

namespace MoTool {

AutoParamAttachment::AutoParamAttachment(te::AutomatableParameter::Ptr p)
    : param(std::move(p)) {
    if (param != nullptr) {
        param->addListener(this);
        configureAutomationCallbacks();
    }
}

AutoParamAttachment::~AutoParamAttachment() {
    if (isAttached()) {
        param->removeListener(this);
    }
    if (listensToValue)
        storedValue.removeListener(this);
}

void AutoParamAttachment::configureAutomationCallbacks() {
    fetchValue = [this]() { return static_cast<double>(param->getCurrentValue()); };
    applyValue = [this](double sliderValue) {
        param->setParameter(static_cast<float>(sliderValue), juce::sendNotification);
    };
    beginGesture = [this]() { param->parameterChangeGestureBegin(); };
    endGesture   = [this]() { param->parameterChangeGestureEnd(); };
}

template <typename ParameterValueType>
void AutoParamAttachment::configureStoredValueCallbacks(ParameterValueType& parameterValue) {
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

// Explicit instantiations aren't strictly necessary because this is a private template
// used only by header templates; keep definition here for linkage separation.

SliderAutoParamAttachment::SliderAutoParamAttachment(Slider& s, te::AutomatableParameter::Ptr p)
    : AutoParamAttachment(std::move(p))
    , midiMapping(param)
    , mouseListener(s)
    , slider(s) {
    configureSliderHandlers();
    configureMouseListener();
}

SliderAutoParamAttachment::~SliderAutoParamAttachment() = default;

void SliderAutoParamAttachment::configureSliderHandlers() {
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

void SliderAutoParamAttachment::configureMouseListener() {
    mouseListener.setRmbCallback([this]() {
        midiMapping.showMappingMenu();
    });
}

void SliderAutoParamAttachment::refreshFromSource() {
    if (!fetchValue)
        return;

    juce::ScopedValueSetter<bool> svs(updating, true);
    slider.setValue(fetchValue(), dontSendNotification);
}

void SliderAutoParamAttachment::currentValueChanged(te::AutomatableParameter&) {
    if (updating)
        return;

    refreshFromSource();
}

void SliderAutoParamAttachment::valueChanged(Value&) {
    if (updating)
        return;

    refreshFromSource();
}

} // namespace MoTool

