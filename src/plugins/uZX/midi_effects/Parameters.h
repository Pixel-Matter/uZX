#pragma once

#include <JuceHeader.h>
#include <concepts>
#include "juce_core/system/juce_PlatformDefs.h"


namespace MoTool::uZX {

//==============================================================================
// C++20 concept for parameter sources
template<typename T>
concept ParameterSource = requires(T& t) {
    { t.getValue() } -> std::convertible_to<float>;
    { t.attachToCurrentValue(std::declval<juce::CachedValue<float>&>()) } -> std::same_as<void>;
    { t.detachFromCurrentValue() } -> std::same_as<void>;
    { t.getName() } -> std::convertible_to<juce::String>;
};

//==============================================================================
// Template-based parameter sources - zero runtime overhead
//==============================================================================
struct TracktionParamSource {
    tracktion::AutomatableParameter::Ptr parameter;

    TracktionParamSource(tracktion::AutomatableParameter::Ptr p) : parameter(std::move(p)) {}

    ~TracktionParamSource() {
        detachFromCurrentValue();
    }

    float getValue() const {
        jassert(parameter != nullptr);
        return parameter->getCurrentValue();
    }

    void attachToCurrentValue(juce::CachedValue<float>& v) {
        jassert(parameter != nullptr);
        parameter->attachToCurrentValue(v);
    }

    void detachFromCurrentValue() {
        if (parameter != nullptr) {
            parameter->detachFromCurrentValue();
        }
    }

    String getName() const {
        if (parameter != nullptr)
            return parameter->getParameterName();
        return juce::String();
    }
};
// Static assertions to verify our types satisfy the concept
static_assert(ParameterSource<TracktionParamSource>);

//==============================================================================
struct JuceParamSource {
    std::atomic<float>* valuePtr_;

    JuceParamSource(std::atomic<float>& value) : valuePtr_(&value) {}

    float getValue() const { return valuePtr_->load(); }

    void attachToCurrentValue(juce::CachedValue<float>&) {}

    void detachFromCurrentValue() {}

    String getName() const { return {}; }
};
// Static assertions to verify our types satisfy the concept
static_assert(ParameterSource<JuceParamSource>);


// TODO ChoiceParameterDef

//==============================================================================
// Parameter definition
//=============================================================================
template <typename Type>
struct ParameterDef {
    String paramID;
    Identifier propertyName;
    String shortLabel;
    String description;
    Type defaultValue;
    NormalisableRange<Type> valueRange;
    String units = {};
    std::function<String(Type)> valueToStringFunction = {};
    std::function<Type(const String&)> stringToValueFunction = {};

    String toString() const {
        // output definition to string for output/debugging
        String s = paramID + ": " + description + " [" + String(valueRange.start)
                   + " - " + String(valueRange.end) + "]";
        if (units.isNotEmpty())
            s += " " + units;
        s += " (default " + String(defaultValue) + ")";
        return s;
    }

};

//==============================================================================
// Value with definition, CachedValue and optionally a parameter source
//==============================================================================
template <typename Type, typename Source = TracktionParamSource>
struct ValueWithDef {
    explicit ValueWithDef(const ParameterDef<Type>& def)
        : definition(def)
    {}

    ValueWithDef(ValueWithDef&&) = default;
    ValueWithDef& operator= (ValueWithDef&&) = default;

    // TODO variadic args passthru to ParameterDef<Type> ctor
    ValueWithDef(const ParameterDef<Type>& def, ValueTree& state, UndoManager* undoMgr = nullptr)
        : definition(def)
        , value(state, def.propertyName, undoMgr, def.defaultValue)
    {}

    ~ValueWithDef() {
        detachSource();
    }


    inline void referTo(ValueTree& v, UndoManager* um) {
        value.referTo(v, definition.propertyName, um, definition.defaultValue);
    }

    void attachSource(std::unique_ptr<Source> s) {
        jassert(s != nullptr);
        source = std::move(s);
        source->attachToCurrentValue(value);
    }

    void detachSource() {
        if (source) {
            source->detachFromCurrentValue();
        }
        source.reset();
    }

    bool isSourceAttached() const { return source != nullptr; }

    ParameterDef<Type> definition;
    CachedValue<Type> value;
    std::unique_ptr<Source> source;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValueWithDef)
};

}  // namespace MoTool::uZX