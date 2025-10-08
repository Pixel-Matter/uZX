#pragma once

#include "Parameters.h"

#include <optional>


namespace MoTool {

//==============================================================================
// Type-erased base for automatable parameters with updateFromAttachedParamValue
// To be used within PluginBase::restorePluginStateFromValueTree
class BindedAutoParameterBase : public tracktion::AutomatableParameter {
public:
    using tracktion::AutomatableParameter::AutomatableParameter;

    virtual void updateFromAttachedParamValue() = 0;
};

//==============================================================================
// Base for automatable parameters with binding to ParameterValue
template <typename Type>
class BindedAutoParameter : public BindedAutoParameterBase,
                            public AsyncUpdater {
public:
    using BindedAutoParameterBase::BindedAutoParameterBase;
    using TypeTraits = ParameterConversionTraits<Type>;

    BindedAutoParameter(tracktion::AutomatableEditItem& editItem, ParameterValue<Type>& paramValue)
        : BindedAutoParameterBase(paramValue.definition.identifier, paramValue.definition.shortLabel,
                                  editItem, paramValue.definition.getFloatValueRange())
        , definition(paramValue.definition), parameterValue(paramValue)
{
        // NOTE while value is not attached, we can not restore from it
        // attachToCurrentValue(value.value);
        // updateFromAttachedValue();
        updateFromAttachedParamValue();

        parameterValue.setLiveReader(&readLiveValue, this);

        if (auto fn = parameterValue.definition.valueToStringFunction; fn) {
            valueToStringFunction = [fn](float v) { return fn(TypeTraits::from(v)); };
        }

        if (auto fn = parameterValue.definition.stringToValueFunction; fn) {
            stringToValueFunction = [fn](const String& text) { return TypeTraits::template to<float>(fn(text)); };
        }
    }

    ~BindedAutoParameter () override {
        cancelPendingUpdate();
        // detachFromCurrentValue();  // currently not attached
        parameterValue.clearLiveReader();
    }

    String getParameterName() const          override { return definition.identifier; }
    String getParameterShortName (int) const override { return definition.shortLabel; }
    String getLabel()                        override { return definition.units; }

    bool isDiscrete() const override { return definition.valueRange.interval >= 1; }

    int getNumberOfStates() const override {
        if (isDiscrete())
            return static_cast<int>(definition.valueRange.end - definition.valueRange.start + 1);
        return 0;
    }

    float getValueForState(int i) const override {
        if (!isDiscrete())
            return 0.0;

        return static_cast<float>(i);
    }

    int getStateForValue(float value) const override {
        if (!isDiscrete())
            return 0.0;

        // clamp to valid range definition.valueRange
        auto start = TypeTraits::template to<float>(definition.valueRange.start);
        auto end = TypeTraits::template to<float>(definition.valueRange.end);
        return roundToInt(jlimit(start, end, value));
    }

    float snapToState(float val) const override {
        if (isDiscrete())
            return getValueForState(getStateForValue(val));
        else
            return val;
    }

    std::optional<float> getDefaultValue() const override {
        return TypeTraits::template to<float>(definition.defaultValue);
    }

    bool hasLabels() const override {
        if constexpr (Util::EnumChoiceConcept<Type>)
            return true;
        else
            return false;
    }

    StringArray getAllLabels() const override {
        if constexpr (Util::EnumChoiceConcept<Type>) {
            StringArray labels;
            for (const auto& label : Type::getLabels())
                labels.add(String(std::string(label)));
            return labels;
        }
        return {};
    }

    String getLabelForValue(float val) const override {
        if constexpr (Util::EnumChoiceConcept<Type>) {
            int s = getStateForValue(val);
            if (isPositiveAndBelow(s, getNumberOfStates()))
                return TypeTraits::template to<String>(TypeTraits::from(s));
        }
        return {};
    }

    //--------------------------------------------------------------
    void updateFromAttachedParamValue() override {
        auto stored = parameterValue.template getStoredValueAs<float>();
        setParameter(stored, dontSendNotification);
    }

    void handleAsyncUpdate() override {
        parameterValue.setStoredValueAs(getCurrentValue());
    }

    void parameterChanged(float, bool byAutomation) override {
        if (!byAutomation) {
            triggerAsyncUpdate();
        }
    }

private:
    static auto readLiveValue(void* context) noexcept -> Type {
        auto* param = static_cast<tracktion::AutomatableParameter*>(context);
        jassert(param != nullptr);
        if (param == nullptr)
            return {};

        return TypeTraits::from(param->getCurrentValue());
    }

    const ParameterDef<Type>& definition;
    ParameterValue<Type>& parameterValue;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BindedAutoParameter)
};

//==============================================================================
// Base for plugins using ParameterValues
class PluginBase : public tracktion::Plugin {
public:
    using tracktion::Plugin::Plugin;

    template <typename Type>
    tracktion::AutomatableParameter* addParam(ParameterValue<Type>& paramValue){
        auto p = new BindedAutoParameter<Type>(*this, paramValue);
        addAutomatableParameter(*p);
        return p;
    }

    //==============================================================================
    void restorePluginStateFromValueTree(const ValueTree&) {
        for (auto p : getAutomatableParameters()) {
            if (auto bindedParam = dynamic_cast<BindedAutoParameterBase*>(p))
                bindedParam->updateFromAttachedParamValue();
            else
                p->updateFromAttachedValue();
        }
    }
};

}  // namespace MoTool
