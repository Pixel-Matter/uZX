#include "ParamEndpoint.h"

#include <cmath>

namespace MoTool {

AutomatedParamEndpoint::AutomatedParamEndpoint(te::AutomatableParameter::Ptr parameterIn)
    : parameter(std::move(parameterIn))
{
    if (!ensureIsValid())
        return;

    parameter->addListener(this);
}

AutomatedParamEndpoint::~AutomatedParamEndpoint() {
    if (!ensureIsValid())
        return;

    parameter->removeListener(this);
}

bool AutomatedParamEndpoint::ensureIsValid() const {
    jassert(parameter != nullptr);
    return parameter != nullptr;
}

NormalisableRange<float> AutomatedParamEndpoint::getRange() const {
    if (!ensureIsValid())
        return {};

    return parameter->valueRange;
}

float AutomatedParamEndpoint::getLiveFloatValue() const {
    if (!ensureIsValid())
        return 0.0f;

    return parameter->getCurrentValue();
}

float AutomatedParamEndpoint::getStoredFloatValue() const {
    if (!ensureIsValid())
        return 0.0f;

    return parameter->getCurrentExplicitValue();
}

void AutomatedParamEndpoint::setStoredFloatValue(float value) {
    if (ensureIsValid())
        parameter->setParameter(value, juce::sendNotification);
}

void AutomatedParamEndpoint::beginGesture() {
    if (ensureIsValid())
        parameter->parameterChangeGestureBegin();
}

void AutomatedParamEndpoint::endGesture() {
    if (ensureIsValid())
        parameter->parameterChangeGestureEnd();
}

bool AutomatedParamEndpoint::isDiscrete() const noexcept {
    return parameter != nullptr && parameter->isDiscrete();
}

int AutomatedParamEndpoint::numberOfStates() const noexcept {
    return parameter != nullptr ? parameter->getNumberOfStates() : 0;
}

int AutomatedParamEndpoint::floatToState(float value) const {
    if (!ensureIsValid())
        return 0;

    return parameter->getStateForValue(value);
}

float AutomatedParamEndpoint::stateToFloat(int state) const {
    if (!ensureIsValid())
        return 0.0f;

    return parameter->getValueForState(state);
}

int AutomatedParamEndpoint::getDecimalPlaces() const noexcept {
    if (!ensureIsValid())
        return 2;

    const auto interval = parameter->valueRange.interval;

    if (interval <= 0.0f)
        return 2;

    int decimals = 0;
    auto scaled = interval;

    while (decimals < 6) {
        const auto rounded = std::round(scaled);
        if (std::abs(rounded - scaled) < 1.0e-5f)
            return decimals;

        scaled *= 10.0f;
        ++decimals;
    }

    return 6;
}

String AutomatedParamEndpoint::formatValue(double value) const {
    if (!ensureIsValid())
        return {};

    return parameter->valueToString(static_cast<float>(value));
}

bool AutomatedParamEndpoint::parseValue(const String& text, double& outValue) const {
    if (!ensureIsValid())
        return false;

    const auto parsed = parameter->stringToValue(text);
    outValue = static_cast<double>(parsed);
    return true;
}

String AutomatedParamEndpoint::stateToLabel(int index) const {
    if (!ensureIsValid())
        return {};

    const auto stateValue = stateToFloat(index);

    if (parameter->hasLabels())
        return parameter->getLabelForValue(stateValue);

    return parameter->valueToString(stateValue);
}

String AutomatedParamEndpoint::getId() const {
    if (!ensureIsValid())
        return {};

    return parameter->paramID;
}

String AutomatedParamEndpoint::getName() const {
    if (!ensureIsValid())
        return {};

    return parameter->getParameterName();
}

String AutomatedParamEndpoint::getDescription() const {
    if (!ensureIsValid())
        return {};

    return parameter->getPluginAndParamName();
}

String AutomatedParamEndpoint::getUnits() const {
    if (!ensureIsValid())
        return {};

    return parameter->getLabel();
}

void AutomatedParamEndpoint::curveHasChanged(te::AutomatableParameter& p) {
    notifyLiveValueChanged(p.getCurrentValue());
}

void AutomatedParamEndpoint::currentValueChanged(te::AutomatableParameter& p) {
    notifyLiveValueChanged(p.getCurrentValue());
}

void AutomatedParamEndpoint::parameterChanged(te::AutomatableParameter& p, float) {
    notifyStoredValueChanged(p.getCurrentExplicitValue());
}

} // namespace MoTool

