#pragma once

#include <JuceHeader.h>
#include <utility>

#include "Parameters.h"

using namespace juce;
namespace te = tracktion;

namespace MoTool {

//==============================================================================
/**
    Interface for unifying access to parameter metadata and value conversion.

    Widgets can bind to this abstraction without caring whether they are backed
    by a stored `ParameterValue` or a live `AutomatableParameter`.
*/
class ParameterEndpoint {
public:
    virtual ~ParameterEndpoint() = default;

    virtual NormalisableRange<float> getRange() const = 0;
    virtual float getLiveFloatValue() const = 0;
    virtual float getStoredFloatValue() const = 0;
    virtual void setStoredFloatValue(float value) = 0;
    virtual void beginGesture() = 0;
    virtual void endGesture() = 0;

    virtual bool isDiscrete() const noexcept = 0;
    virtual int numberOfStates() const noexcept = 0;
    virtual int floatToState(float value) const = 0;
    virtual float stateToFloat(int state) const = 0;
    virtual int getDecimalPlaces() const noexcept = 0;
    virtual String formatValue(double value) const = 0;
    virtual bool parseValue(const String& text, double& outValue) const = 0;

    virtual String stateToLabel(int index) const = 0;
    virtual String getId() const = 0;
    virtual String getName() const = 0;
    virtual String getDescription() const = 0;
    virtual String getUnits() const = 0;

    // Listener interface, designed after AutomatableParameter::Listener
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void storedValueChanged(ParameterEndpoint&, float newValue) = 0;
        virtual void liveValueChanged(ParameterEndpoint&, float newValue) = 0;
    };

    void addListener(Listener* listener) {
        listeners.add(listener);
    }

    void removeListener(Listener* listener) {
        listeners.remove(listener);
    }

protected:
    ParameterEndpoint() = default;

    void notifyStoredValueChanged(float newValue) {
        listeners.call([&](Listener& l) {
            l.storedValueChanged(*this, newValue);
        });
    }

    void notifyLiveValueChanged(float newValue) {
        listeners.call([&](Listener& l) {
            l.liveValueChanged(*this, newValue);
        });
    }

private:
    ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterEndpoint)
};

//==============================================================================
// For binding ParameterValue<Type> to widgets.
template <typename Type>
class StaticParamEndpoint : public ParameterEndpoint,
                            private Value::Listener {
public:
    explicit StaticParamEndpoint(ParameterValue<Type>& value)
        : parameterValue(value)
    {
        storedValue = value.getPropertyAsValue();
        storedValue.addListener(this);
    }

    ~StaticParamEndpoint() override {
        storedValue.removeListener(this);
    }

    NormalisableRange<float> getRange() const override {
        return parameterValue.definition.getFloatValueRange();
    }

    float getLiveFloatValue() const override {
        return parameterValue.template getLiveValueAs<float>();
    }

    float getStoredFloatValue() const override {
        return parameterValue.template getStoredValueAs<float>();
    }

    void setStoredFloatValue(float value) override {
        parameterValue.setStoredValueAs(value);
        notifyStoredValueChanged(getStoredFloatValue());
    }

    void beginGesture() override {}
    void endGesture() override {}

    bool isDiscrete() const noexcept override {
        return parameterValue.definition.isDiscrete();
    }

    int numberOfStates() const noexcept override {
        return parameterValue.definition.numberOfStates();
    }

    int floatToState(float value) const override {
        return parameterValue.definition.floatToState(value);
    }

    float stateToFloat(int state) const override {
        return parameterValue.definition.stateToFloat(state);
    }

    int getDecimalPlaces() const noexcept override {
        return parameterValue.definition.decimalPlaces();
    }

    String formatValue(double value) const override {
        using Traits = ParameterConversionTraits<Type>;
        auto typedValue = Traits::template from<double>(value);
        return parameterValue.definition.valueToText(typedValue);
    }

    bool parseValue(const String& text, double& outValue) const override {
        if (auto parsed = parameterValue.definition.textToValue(text)) {
            using Traits = ParameterConversionTraits<Type>;
            outValue = Traits::template to<double>(*parsed);
            return true;
        }
        return false;
    }

    String stateToLabel(int index) const override {
        return parameterValue.definition.stateToLabel(index);
    }

    String getId() const override {
        return parameterValue.definition.identifier;
    }

    String getName() const override {
        return parameterValue.definition.shortLabel;
    }

    String getDescription() const override {
        return parameterValue.definition.description;
    }

    String getUnits() const override {
        if constexpr (requires { parameterValue.definition.units; })
            return parameterValue.definition.units;
        else
            return {};
    }

private:
    void valueChanged(Value&) override {
        notifyStoredValueChanged(getStoredFloatValue());
    }

    ParameterValue<Type>& parameterValue;
    Value storedValue;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StaticParamEndpoint)
};

//==============================================================================
class AutomatedParamEndpoint : public ParameterEndpoint,
                               private te::AutomatableParameter::Listener {
public:
    explicit AutomatedParamEndpoint(te::AutomatableParameter::Ptr parameterIn);
    ~AutomatedParamEndpoint() override;

    bool ensureIsValid() const;

    NormalisableRange<float> getRange() const override;
    float getLiveFloatValue() const override;
    float getStoredFloatValue() const override;
    void setStoredFloatValue(float value) override;
    void beginGesture() override;
    void endGesture() override;

    bool isDiscrete() const noexcept override;
    int numberOfStates() const noexcept override;
    int floatToState(float value) const override;
    float stateToFloat(int state) const override;
    int getDecimalPlaces() const noexcept override;
    String formatValue(double value) const override;
    bool parseValue(const String& text, double& outValue) const override;
    String stateToLabel(int index) const override;
    String getId() const override;
    String getName() const override;
    String getDescription() const override;
    String getUnits() const override;

private:
    void curveHasChanged(te::AutomatableParameter& parameter) override;
    void currentValueChanged(te::AutomatableParameter& parameter) override;
    void parameterChanged(te::AutomatableParameter& parameter, float newValue) override;

    te::AutomatableParameter::Ptr parameter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomatedParamEndpoint)
};

} // namespace MoTool

