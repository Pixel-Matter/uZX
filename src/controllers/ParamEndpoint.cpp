#include "ParamEndpoint.h"

#include <cmath>

namespace MoTool {

AutomatableParamEndpoint::AutomatableParamEndpoint(te::AutomatableParameter::Ptr parameterIn)
    : parameter(std::move(parameterIn))
{
    if (!ensureIsValid())
        return;

    parameter->addListener(this);
}

AutomatableParamEndpoint::~AutomatableParamEndpoint() {
    if (!ensureIsValid())
        return;

    parameter->removeListener(this);
}

bool AutomatableParamEndpoint::ensureIsValid() const {
    jassert(parameter != nullptr);
    return parameter != nullptr;
}

NormalisableRange<float> AutomatableParamEndpoint::getRange() const {
    if (!ensureIsValid())
        return {};

    return parameter->valueRange;
}

float AutomatableParamEndpoint::getLiveFloatValue() const {
    if (!ensureIsValid())
        return 0.0f;

    return parameter->getCurrentValue();
}

float AutomatableParamEndpoint::getStoredFloatValue() const {
    if (!ensureIsValid())
        return 0.0f;

    return parameter->getCurrentExplicitValue();
}

void AutomatableParamEndpoint::setStoredFloatValue(float value) {
    if (ensureIsValid())
        parameter->setParameter(value, juce::sendNotification);
}

void AutomatableParamEndpoint::beginGesture() {
    if (ensureIsValid())
        parameter->parameterChangeGestureBegin();
}

void AutomatableParamEndpoint::endGesture() {
    if (ensureIsValid())
        parameter->parameterChangeGestureEnd();
}

bool AutomatableParamEndpoint::isDiscrete() const noexcept {
    return parameter != nullptr && parameter->isDiscrete();
}

int AutomatableParamEndpoint::numberOfStates() const noexcept {
    return parameter != nullptr ? parameter->getNumberOfStates() : 0;
}

int AutomatableParamEndpoint::floatToState(float value) const {
    if (!ensureIsValid())
        return 0;

    return parameter->getStateForValue(value);
}

float AutomatableParamEndpoint::stateToFloat(int state) const {
    if (!ensureIsValid())
        return 0.0f;

    return parameter->getValueForState(state);
}

int AutomatableParamEndpoint::getDecimalPlaces() const noexcept {
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

String AutomatableParamEndpoint::formatValue(double value) const {
    if (!ensureIsValid())
        return {};

    return parameter->valueToString(static_cast<float>(value));
}

bool AutomatableParamEndpoint::parseValue(const String& text, double& outValue) const {
    if (!ensureIsValid())
        return false;

    const auto parsed = parameter->stringToValue(text);
    outValue = static_cast<double>(parsed);
    return true;
}

String AutomatableParamEndpoint::stateToLabel(int index) const {
    if (!ensureIsValid())
        return {};

    const auto stateValue = stateToFloat(index);

    if (parameter->hasLabels())
        return parameter->getLabelForValue(stateValue);

    return parameter->valueToString(stateValue);
}

String AutomatableParamEndpoint::getId() const {
    if (!ensureIsValid())
        return {};

    return parameter->paramID;
}

String AutomatableParamEndpoint::getName() const {
    if (!ensureIsValid())
        return {};

    return parameter->getParameterShortName(16);
}

String AutomatableParamEndpoint::getDescription() const {
    if (!ensureIsValid())
        return {};

    return parameter->getPluginAndParamName();
}

String AutomatableParamEndpoint::getUnits() const {
    if (!ensureIsValid())
        return {};

    return parameter->getLabel();
}

void AutomatableParamEndpoint::curveHasChanged(te::AutomatableParameter& p) {
    notifyLiveValueChanged(p.getCurrentValue());
}

void AutomatableParamEndpoint::currentValueChanged(te::AutomatableParameter& p) {
    notifyLiveValueChanged(p.getCurrentValue());
}

void AutomatableParamEndpoint::parameterChanged(te::AutomatableParameter& p, float) {
    notifyStoredValueChanged(p.getCurrentExplicitValue());
}

} // namespace MoTool
