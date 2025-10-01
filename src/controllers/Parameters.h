#pragma once

#include <JuceHeader.h>
#include "../util/enumchoice.h"

#include <memory>
#include <vector>


namespace MoTool {

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
                static_cast<float>(valueRange.interval)};
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
    std::function<String(E)> valueToStringFunction = [](E value) {
        // DBG("valueToStringFunction called for enum with value " << static_cast<int>(value));
        return String(std::string(value.getLabel()));
    };

    std::function<E(const String&)> stringToValueFunction = [](const String& str) {
        // DBG("stringToValueFunction called for enum with string " << str);
        return E(str.toStdString());
    };

    NormalisableRange<float> getFloatValueRange() const {
        return {static_cast<float>(valueRange.start),
                static_cast<float>(valueRange.end),
                static_cast<float>(valueRange.interval)};
    }

    // TODO handle serialization of EnumChoice separately

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
// Value with definition, CachedValue and optional AutomatableParameter binding
//==============================================================================
template <typename T>
struct ParameterValue {
    using Type = T;
    using ValueType = std::conditional_t<
        std::is_same_v<Type, float>,
        float,
        std::conditional_t<
            std::is_same_v<Type, bool>,
            bool,
            int  // default to int for other types (including EnumChoice)
        >
    >;

    struct LiveAccessor {
        using Reader = Type(*)(void*);

        void set(Reader r, void* ctx) {
            reader = r;
            context = ctx;
        }

        void reset() {
            reader = nullptr;
            context = nullptr;
        }

        auto hasReader() const -> bool { return reader != nullptr; }

        auto readOr(const Type& fallback) const -> Type {
            if (reader != nullptr)
                return reader(context);
            return fallback;
        }

    private:
        Reader reader { nullptr };
        void* context { nullptr };
    };

    explicit ParameterValue(const ParameterDef<Type>& def)
        : definition(def)
    {}

    ParameterValue(ParameterValue&&) = default;
    ParameterValue& operator= (ParameterValue&&) = default;

    ParameterValue(const ParameterDef<Type>& def, ValueTree& state, UndoManager* undoMgr = nullptr)
        : definition(def)
        , value(state, def.propertyName, undoMgr, def.defaultValue)
    {}

    ~ParameterValue() {
        detachSource();
    }

    inline void referTo(ValueTree& v, UndoManager* um) {
        value.referTo(v, definition.propertyName, um, definition.defaultValue);
    }

    void attachSource(tracktion::AutomatableParameter::Ptr paramPtr) {
        detachSource();
        automatableParameter = std::move(paramPtr);
        if (automatableParameter != nullptr) {
            automatableParameter->attachToCurrentValue(value);
            liveAccessor.set([](void* ctx) -> Type {
                auto* param = static_cast<tracktion::AutomatableParameter*>(ctx);
                jassert(param != nullptr);
                if (param == nullptr)
                    return {};
                auto liveValue = static_cast<ValueType>(param->getCurrentValue());
                return static_cast<Type>(liveValue);
            }, automatableParameter.get());
        }
    }

    void detachSource() {
        if (automatableParameter != nullptr) {
            automatableParameter->detachFromCurrentValue();
            automatableParameter.reset();
        }
        liveAccessor.reset();
    }

    Type getStoredValue() const {
        return static_cast<Type>(value.get());
    }

    Type getLiveValue() const {
        return liveAccessor.readOr(getStoredValue());
    }

    bool hasLiveReader() const { return liveAccessor.hasReader(); }

    void setStoredValue(Type newValue) {
        value = static_cast<ValueType>(newValue);
    }

    juce::Value getPropertyAsValue() { return value.getPropertyAsValue(); }

    Type getCurrentValue() const { return getLiveValue(); }

    bool isSourceAttached() const { return automatableParameter != nullptr; }

    tracktion::AutomatableParameter* getAutomatableParameter() const { return automatableParameter.get(); }

    ParameterDef<Type> definition;

    // TODO If used for EnumChoice<Type>
    // EnumChoice<Type> value is for serialization, uses strings in ValueTree
    // EnumChoice<ValueType> value is for sync with AutomatableParameter, uses int in ValueTree
    CachedValue<ValueType> value;

    // If no source is attached, ParameterValue instance still can be used as a static parameter
    tracktion::AutomatableParameter::Ptr automatableParameter;
    LiveAccessor liveAccessor;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterValue)
};

struct ParameterAutomationBindingBase {
    virtual ~ParameterAutomationBindingBase() = default;
};

template <typename ParameterValueType>
struct ParameterAutomationBinding : ParameterAutomationBindingBase {
    ParameterAutomationBinding(ParameterValueType& v, tracktion::AutomatableParameter::Ptr param)
        : value(v)
        , parameter(std::move(param))
    {
        value.attachSource(parameter);
    }

    ~ParameterAutomationBinding() override {
        value.detachSource();
    }

    ParameterValueType& value;
    tracktion::AutomatableParameter::Ptr parameter;
};

//==============================================================================
// CRTP base class for parameter structs in plugins
template <typename Derived>
class ParamsBase {
public:
    void referTo(ValueTree& v, UndoManager* um) {
        static_assert(std::is_base_of<ParamsBase, Derived>::value, "Derived must inherit from ParamsBase");
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

    ParameterValue<float> someParam;
    ParameterValue<float> anotherParam;
    */
};


}  // namespace MoTool
