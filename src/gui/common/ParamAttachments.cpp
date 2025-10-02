#include "ParamAttachments.h"

using namespace juce;

namespace MoTool {

ParamBindingBase::ParamBindingBase(te::AutomatableParameter::Ptr p)
    : param(std::move(p)) {
    if (param != nullptr) {
        param->addListener(this);
        configureAutomationCallbacks();
    }
}

ParamBindingBase::~ParamBindingBase() {
    if (isAttached()) {
        param->removeListener(this);
    }
    if (listensToValue)
        storedValue.removeListener(this);
}

void ParamBindingBase::configureAutomationCallbacks() {
    fetchValue = [this]() { return static_cast<double>(param->getCurrentValue()); };
    applyValue = [this](double sliderValue) {
        param->setParameter(static_cast<float>(sliderValue), juce::sendNotification);
    };
    beginGesture = [this]() { param->parameterChangeGestureBegin(); };
    endGesture   = [this]() { param->parameterChangeGestureEnd(); };
}

// Explicit instantiations aren't strictly necessary because this is a private template
// used only by header templates; keep definition here for linkage separation.

SliderParamBinding::SliderParamBinding(Slider& s, te::AutomatableParameter::Ptr p)
    : ParamBindingBase(std::move(p))
    , midiMapping(param)
    , mouseListener(s)
    , slider(s) {
    configureSliderHandlers();
    configureMouseListener();
}

SliderParamBinding::~SliderParamBinding() = default;

void SliderParamBinding::configureSliderHandlers() {
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

void SliderParamBinding::configureMouseListener() {
    mouseListener.setRmbCallback([this]() {
        midiMapping.showMappingMenu();
    });
}

void SliderParamBinding::refreshFromSource() {
    if (!fetchValue)
        return;

    juce::ScopedValueSetter<bool> svs(updating, true);
    slider.setValue(fetchValue(), dontSendNotification);
}

void SliderParamBinding::currentValueChanged(te::AutomatableParameter&) {
    if (updating)
        return;

    refreshFromSource();
}

void SliderParamBinding::valueChanged(Value&) {
    if (updating)
        return;

    refreshFromSource();
}

//==============================================================================
ButtonParamBinding::~ButtonParamBinding() {
    textButton.onClick = nullptr;
}

void ButtonParamBinding::configureButtonHandlers() {
    textButton.onClick = [this]() { handleClick(); };
}

void ButtonParamBinding::configureMouseListener() {
    if (mouseListener != nullptr) {
        mouseListener->setRmbCallback([this]() {
            midiMapping.showMappingMenu();
        });
    }
}

void ButtonParamBinding::refreshFromSource() {
    if (!fetchValue || !floatValueToIndex || !indexToLabel || choiceCount <= 0)
        return;

    juce::ScopedValueSetter<bool> svs(updating, true);
    const auto sliderValue = fetchValue();
    const auto index = wrapIndex(floatValueToIndex(static_cast<float>(sliderValue)));
    textButton.setButtonText(indexToLabel(index));
}

void ButtonParamBinding::currentValueChanged(te::AutomatableParameter&) {
    if (updating)
        return;

    refreshFromSource();
}

void ButtonParamBinding::valueChanged(Value&) {
    if (updating)
        return;

    refreshFromSource();
}

void ButtonParamBinding::handleClick() {
    if (!applyValue || !indexToFloatValue || choiceCount <= 0)
        return;

    const auto currentIndex = getCurrentIndex();
    const auto nextIndex = wrapIndex(currentIndex + 1);
    const auto sliderValue = indexToFloatValue(nextIndex);

    if (beginGesture)
        beginGesture();

    {
        juce::ScopedValueSetter<bool> svs(updating, true);
        applyValue(sliderValue);
        refreshFromSource();
    }

    if (endGesture)
        endGesture();
}

int ButtonParamBinding::getCurrentIndex() const {
    if (!fetchValue || !floatValueToIndex || choiceCount <= 0)
        return 0;

    return wrapIndex(floatValueToIndex(static_cast<float>(fetchValue())));
}

int ButtonParamBinding::wrapIndex(int index) const {
    if (choiceCount <= 0)
        return 0;

    index %= choiceCount;
    if (index < 0)
        index += choiceCount;
    return index;
}

} // namespace MoTool
