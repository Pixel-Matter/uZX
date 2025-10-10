#include "ParamBindings.h"
#include "gui/common/MouseListener.h"

using namespace juce;

namespace MoTool {

WidgetParamBindingBase::WidgetParamBindingBase(Component &c, te::AutomatableParameter::Ptr p)
    : mouseListener(c)
{
    attachParameter(std::move(p));
}

WidgetParamBindingBase::~WidgetParamBindingBase() {
    detachParameter();
    if (listensToValue)
        storedValue.removeListener(this);
}

bool WidgetParamBindingBase::isAttached() const noexcept {
    return param != nullptr;
}

void WidgetParamBindingBase::configureAutomationCallbacks() {
    fetchValue = [this] {
        return static_cast<double>(param->getCurrentValue());
    };
    applyValue = [this](double widgetValue) {
        // TODO what if widgetValue is not float? String, int...
        param->setParameter(static_cast<float>(widgetValue), juce::sendNotification);
    };
    beginGesture = [this] { param->parameterChangeGestureBegin(); };
    endGesture   = [this] { param->parameterChangeGestureEnd(); };
}

void WidgetParamBindingBase::configureMouseListener() {
    mouseListener.setRmbCallback([this]() {
        midiMapping.showMappingMenu();
    });
}

void WidgetParamBindingBase::attachParameter(te::AutomatableParameter::Ptr newParam) {
    if (param == newParam)
        return;

    if (param != nullptr)
        param->removeListener(this);

    param = std::move(newParam);
    midiMapping.setParameter(param);

    if (param != nullptr) {
        param->addListener(this);
        configureAutomationCallbacks();
    } else {
        fetchValue = {};
        applyValue = {};
        beginGesture = {};
        endGesture = {};
    }
}

void WidgetParamBindingBase::detachParameter() {
    if (param == nullptr)
        return;

    param->removeListener(this);
    midiMapping.reset();
    param.reset();
    fetchValue = {};
    applyValue = {};
    beginGesture = {};
    endGesture = {};
}

//==============================================================================
void SliderParamBinding::configureSliderForAutomationParameter() {
    jassert(param != nullptr);
    if (param == nullptr)
        return;

    slider.setTooltip(param->getParameterName());
    slider.setPopupDisplayEnabled(true, true, nullptr);

    slider.setRange(param->getValueRange().getStart(),
                    param->getValueRange().getEnd(),
                    param->valueRange.interval);
    slider.setSkewFactor(param->valueRange.skew);

    slider.setNumDecimalPlacesToDisplay(2);
    slider.textFromValueFunction = param->valueToStringFunction;
    slider.valueFromTextFunction = param->stringToValueFunction;

    slider.setValue(param->getCurrentValue(), juce::dontSendNotification);
}

SliderParamBinding::~SliderParamBinding() {
    slider.onValueChange = nullptr;
    slider.onDragStart = nullptr;
    slider.onDragEnd = nullptr;
}

void SliderParamBinding::configureSliderHandlers() {
    slider.onValueChange = [this] {
        if (updating || !applyValue)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        applyValue(static_cast<float>(slider.getValue()));
    };

    slider.onDragStart = [this] {
        if (beginGesture)
            beginGesture();
    };

    slider.onDragEnd = [this] {
        if (endGesture)
            endGesture();
    };

    slider.setPopupMenuEnabled(false);
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

void ButtonParamBinding::refreshFromSource() {
    if (!fetchValue || !indexToLabel || choiceCount <= 0)
        return;

    juce::ScopedValueSetter<bool> svs(updating, true);
    const auto index = wrapIndex(roundToInt(fetchValue()));
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
    if (!applyValue || choiceCount <= 0)
        return;

    const auto currentIndex = getCurrentIndex();
    const auto nextIndex = wrapIndex(currentIndex + 1);

    if (beginGesture)
        beginGesture();

    {
        juce::ScopedValueSetter<bool> svs(updating, true);
        // apply to parameter source or value
        applyValue(static_cast<float>(nextIndex));
    }

    if (endGesture)
        endGesture();
}

int ButtonParamBinding::getCurrentIndex() const {
    if (!fetchValue || choiceCount <= 0)
        return 0;

    return wrapIndex(roundToInt(fetchValue()));
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
