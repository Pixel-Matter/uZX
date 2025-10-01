#pragma once

#include <JuceHeader.h>
#include "../util/enumchoice.h"

#include <cmath>
#include <memory>
#include <type_traits>
#include <vector>


namespace MoTool {

//==============================================================================
// Parameter storage traits
//==============================================================================
template <typename T, typename Enable = void>
struct ParameterStorageTraits;

template <typename T>
struct ParameterStorageTraits<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    using StorageType = T;
    using SliderValue = double;

    static auto toStorage(T value) -> StorageType { return value; }
    static auto fromStorage(StorageType value) -> T { return value; }

    static auto toSliderValue(T value) -> SliderValue { return static_cast<SliderValue>(value); }
    static auto fromSliderValue(SliderValue value) -> T { return static_cast<T>(value); }
};

template <typename T>
struct ParameterStorageTraits<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
    using StorageType = T;
    using SliderValue = double;

    static auto toStorage(T value) -> StorageType { return value; }
    static auto fromStorage(StorageType value) -> T { return value; }

    static auto toSliderValue(T value) -> SliderValue { return static_cast<SliderValue>(value); }
    static auto fromSliderValue(SliderValue value) -> T { return static_cast<T>(std::llround(value)); }
};

template <>
struct ParameterStorageTraits<bool> {
    using StorageType = bool;
    using SliderValue = double;

    static auto toStorage(bool value) -> StorageType { return value; }
    static auto fromStorage(StorageType value) -> bool { return value; }

    static auto toSliderValue(bool value) -> SliderValue { return value ? 1.0 : 0.0; }
    static auto fromSliderValue(SliderValue value) -> bool { return value >= 0.5; }
};

template <Util::EnumChoiceConcept E>
struct ParameterStorageTraits<E> {
    using StorageType = int;
    using SliderValue = double;

    static auto toStorage(E value) -> StorageType {
        using EnumType = typename E::Enum;
        using Underlying = typename E::UnderlyingType;
        return static_cast<StorageType>(static_cast<Underlying>(static_cast<EnumType>(value)));
    }

    static auto fromStorage(StorageType value) -> E { return E(value); }

    static auto toSliderValue(E value) -> SliderValue {
        using EnumType = typename E::Enum;
        using Underlying = typename E::UnderlyingType;
        return static_cast<SliderValue>(static_cast<Underlying>(static_cast<EnumType>(value)));
    }

    static auto fromSliderValue(SliderValue value) -> E {
        return E(static_cast<int>(std::llround(value)));
    }
};

//==============================================================================
// Parameter definition
//==============================================================================
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
    using Traits = ParameterStorageTraits<Type>;
    using StorageType = typename Traits::StorageType;
    using SliderValue = typename Traits::SliderValue;

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
        , value(state, def.propertyName, undoMgr, Traits::toStorage(def.defaultValue))
    {}

    ~ParameterValue() {
        liveAccessor.reset();
    }

    inline void referTo(ValueTree& v, UndoManager* um) {
        value.referTo(v, definition.propertyName, um, Traits::toStorage(definition.defaultValue));
    }

    Type getStoredValue() const {
        return Traits::fromStorage(value.get());
    }

    Type getLiveValue() const {
        return liveAccessor.readOr(getStoredValue());
    }

    bool hasLiveReader() const { return liveAccessor.hasReader(); }

    void setStoredValue(Type newValue) {
        value = Traits::toStorage(newValue);
    }

    juce::Value getPropertyAsValue() { return value.getPropertyAsValue(); }

    Type getCurrentValue() const { return getLiveValue(); }

    void setLiveReader(typename LiveAccessor::Reader reader, void* context) {
        liveAccessor.set(reader, context);
    }

    void clearLiveReader() {
        liveAccessor.reset();
    }

    ParameterDef<Type> definition;

    // TODO If used for EnumChoice<Type>
    // EnumChoice<Type> value is for serialization, uses strings in ValueTree
    // EnumChoice<ValueType> value is for sync with AutomatableParameter, uses int in ValueTree
    CachedValue<StorageType> value;

private:
    LiveAccessor liveAccessor;

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
        if (parameter != nullptr) {
            parameter->attachToCurrentValue(value.value);
            value.setLiveReader(&ParameterAutomationBinding::readLiveValue, parameter.get());

            if (value.definition.valueToStringFunction) {
                auto fn = value.definition.valueToStringFunction;
                parameter->valueToStringFunction = [fn](float sliderValue) {
                    using Traits = ParameterStorageTraits<typename ParameterValueType::Type>;
                    return fn(Traits::fromSliderValue(static_cast<typename Traits::SliderValue>(sliderValue)));
                };
            }

            if (value.definition.stringToValueFunction) {
                auto fn = value.definition.stringToValueFunction;
                parameter->stringToValueFunction = [fn](const juce::String& text) {
                    using Traits = ParameterStorageTraits<typename ParameterValueType::Type>;
                    auto typedValue = fn(text);
                    return static_cast<float>(Traits::toSliderValue(typedValue));
                };
            }
        }
    }

    ~ParameterAutomationBinding() override {
        if (parameter != nullptr)
            parameter->detachFromCurrentValue();
        value.clearLiveReader();
    }

    ParameterValueType& value;
    tracktion::AutomatableParameter::Ptr parameter;

private:
    static auto readLiveValue(void* context) -> typename ParameterValueType::Type {
        auto* param = static_cast<tracktion::AutomatableParameter*>(context);
        jassert(param != nullptr);
        if (param == nullptr)
            return {};

        using Traits = ParameterStorageTraits<typename ParameterValueType::Type>;
        return Traits::fromSliderValue(static_cast<typename Traits::SliderValue>(param->getCurrentValue()));
    }
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
