#pragma once

#include <JuceHeader.h>
#include <concepts>
#include <optional>
#include "../util/enumchoice.h"


namespace MoTool {

//==============================================================================
// C++20 concept for parameter sources
template<typename T>
concept ParameterSourceConcept = requires(T& t) {
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

    // Rule of Five: make it move-only
    TracktionParamSource(const TracktionParamSource&) = delete;
    TracktionParamSource& operator=(const TracktionParamSource&) = delete;

    TracktionParamSource(TracktionParamSource&&) = default;
    TracktionParamSource& operator=(TracktionParamSource&&) = default;

    ~TracktionParamSource() {
        detachFromCurrentValue();
    }

    float getValue() const {
        jassert(parameter != nullptr);
        return parameter->getCurrentValue();
    }

    template <typename Type>
    void attachToCurrentValue(CachedValue<Type>& v) {
        static_assert(std::is_same_v<Type, float> || std::is_same_v<Type, int> || std::is_same_v<Type, bool>,
                      "TracktionParamSource can only be attached to CachedValue<float>, CachedValue<int> or CachedValue<bool>");
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
static_assert(ParameterSourceConcept<TracktionParamSource>);

//==============================================================================
struct EmptyParamSource {
    float getValue() const { return 0.0; }

    template <typename Type>
    void attachToCurrentValue(juce::CachedValue<Type>&) {
       static_assert(std::is_same_v<Type, float> || std::is_same_v<Type, int> || std::is_same_v<Type, bool>,
                    "EmptyParamSource can only be attached to CachedValue<float>, CachedValue<int> or CachedValue<bool>");
}
    void detachFromCurrentValue() {}
    String getName() const { return {}; }
};
// Static assertions to verify our types satisfy the concept
static_assert(ParameterSourceConcept<EmptyParamSource>);


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

    NormalisableRange<float> getFloatValueRange() const {
        return {static_cast<float>(valueRange.start),
                static_cast<float>(valueRange.end),
                static_cast<float>(valueRange.interval),
                valueRange.skew};
    }

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

template <Util::EnumChoiceConcept E>
struct ParameterDef<E> {
    String paramID;
    Identifier propertyName;
    String shortLabel;
    String description;
    E defaultValue;

    // Auto-initialized from enum size
    NormalisableRange<E> valueRange = NormalisableRange<E>(E(0), E(E::size() - 1), 1);

    String units = {};

    // Auto-initialized label conversion functions
    std::function<String(E)> valueToStringFunction = [](E value) { return String(std::string(value.getLabel())); };

    std::function<E(const String&)> stringToValueFunction = [](const String& str) {
        return E(str.toStdString());
    };

    NormalisableRange<float> getFloatValueRange() const {
        return {static_cast<float>(valueRange.start),
                static_cast<float>(valueRange.end),
                static_cast<float>(valueRange.interval)};
    }

    // Keep the existing toString() method
    String toString() const {
        String s = paramID + ": " + description;
        s += " (choices: ";
        auto labels = E::getLabels();
        for (size_t i = 0; i < labels.size(); ++i) {
            if (i > 0)
                s += ", ";
            s += String(labels[i]);
        }
        s += ")";
        s += " (default " + String(defaultValue.getLabel()) + ")";
        return s;
    }
};

//==============================================================================
// Value with definition, CachedValue and optionally a parameter source
//==============================================================================
template <typename T, ParameterSourceConcept Source = TracktionParamSource>
struct ValueWithSource {
    using Type = T;
    using ValueType = std::conditional_t<std::is_same_v<Type, float>, float, int>;

    explicit ValueWithSource(const ParameterDef<Type>& def)
        : definition(def)
    {}

    ValueWithSource(ValueWithSource&&) = default;
    ValueWithSource& operator= (ValueWithSource&&) = default;

    // TODO variadic args passthru to ParameterDef<Type> ctor
    ValueWithSource(const ParameterDef<Type>& def, ValueTree& state, UndoManager* undoMgr = nullptr)
        : definition(def)
        , value(state, def.propertyName, undoMgr, def.defaultValue)
    {}

    ~ValueWithSource() {
        detachSource();
    }

    inline void referTo(ValueTree& v, UndoManager* um) {
        value.referTo(v, definition.propertyName, um, definition.defaultValue);
    }

    void attachSource(Source&& s) {
        source = std::move(s);
        source->attachToCurrentValue(value);
    }

    void detachSource() {
        if (isSourceAttached()) {
            source->detachFromCurrentValue();
        }
        source.reset();
    }

    Type getCurrentValue() const {
        if (isSourceAttached()) {
            return static_cast<Type>(source->getValue());
        } else {
            return value.get();
        }
    }

    bool isSourceAttached() const { return source.has_value(); }

    ParameterDef<Type> definition;
    CachedValue<ValueType> value;
    // If no source is attached, ValueWithSource instance still can be used as a static parameter
    std::optional<Source> source;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValueWithSource)
};

//==============================================================================
// CRTP base class for parameter widgets in plugins
template <typename Derived>
class ParamsBase {
public:
    // static_assert(std::is_base_of<ParamsBase, Derived>::value, "Derived must inherit from ParamsBase");

    void referTo(ValueTree& v, UndoManager* um) {
        static_cast<Derived*>(this)->visit([&v, um] (auto& value) { value.referTo(v, um); });
    }

    void restoreStateFromValueTree(const ValueTree& v) {
        static_cast<Derived*>(this)->visit([&v] (auto& value) { tracktion::copyPropertiesToCachedValues(v, value.value ); });
    }

    // Derived classes must implement
    /**

    template<typename Visitor>
    void visit(Visitor&& visitor) {
        visitor(someParam);
        visitor(anotherParam);
    }

    ValueWithSource<float> someParam;
    ValueWithSource<float> anotherParam;
    */
};


}  // namespace MoTool