#pragma once

#include <JuceHeader.h>
#include "../util/enumchoice.h"

#include <type_traits>


namespace MoTool {

//==============================================================================
// Parameter storage traits
//==============================================================================
template <typename T, typename Enable = void>
struct ParameterStorageTraits;

template <typename T>
struct ParameterStorageTraits<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    using StorageType = T;

    static constexpr auto toStorage(T value) noexcept -> StorageType { return value; }
    static constexpr auto fromStorage(StorageType value) noexcept -> T { return value; }

    static constexpr auto toFloatValue(T value) noexcept -> float { return static_cast<float>(value); }
    static constexpr auto fromFloatValue(float value) noexcept -> T { return static_cast<T>(value); }
};

template <typename T>
struct ParameterStorageTraits<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
    using StorageType = T;

    static constexpr auto toStorage(T value) noexcept -> StorageType { return value; }
    static constexpr auto fromStorage(StorageType value) noexcept -> T { return value; }

    static constexpr auto toFloatValue(T value) noexcept -> float { return static_cast<float>(value); }
    static constexpr auto fromFloatValue(float value) noexcept -> T { return static_cast<T>(roundToInt(value)); }
};

template <>
struct ParameterStorageTraits<bool> {
    using StorageType = bool;

    static constexpr auto toStorage(bool value) noexcept -> StorageType { return value; }
    static constexpr auto fromStorage(StorageType value) noexcept -> bool { return value; }

    static constexpr auto toFloatValue(bool value) noexcept -> float { return value ? 1.0f : 0.0f; }
    static constexpr auto fromFloatValue(float value) noexcept -> bool { return value >= 0.5f; }
};

template <Util::EnumChoiceConcept E>
struct ParameterStorageTraits<E> {
    //TODO String?
    using StorageType = int;

    static constexpr auto toStorage(E value) noexcept -> StorageType {
        using EnumType = typename E::Enum;
        using Underlying = typename E::UnderlyingType;
        return static_cast<StorageType>(static_cast<Underlying>(static_cast<EnumType>(value)));
    }

    static constexpr auto fromStorage(StorageType value) noexcept -> E { return E(value); }

    static constexpr auto toFloatValue(E value) noexcept -> float {
        using EnumType = typename E::Enum;
        using Underlying = typename E::UnderlyingType;
        return static_cast<float>(static_cast<Underlying>(static_cast<EnumType>(value)));
    }

    static auto fromFloatValue(float value) noexcept -> E {
        return E(roundToInt(value));
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

template <>
struct ParameterDef<bool> {
    String paramID;
    Identifier propertyName;
    String shortLabel;
    String description;
    bool defaultValue;

    NormalisableRange<float> getFloatValueRange() const {
        return {0.0f, 1.0f, 1.0f};
    }

    // Keep the existing toString() method
    String toString() const {
        String s = paramID + ": " + description;
        s += " (default " + String(defaultValue ? "true" : "false") + ")";
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
template <typename T>
concept ParameterValueConcept = requires(T& t) {
    typename T::Type;
    typename ParameterStorageTraits<typename T::Type>;
    { t.getStoredValue() } -> std::same_as<typename T::Type>;
    { t.setStoredValue(std::declval<typename T::Type>()) } -> std::same_as<void>;
    { t.getPropertyAsValue() } -> std::same_as<Value>;
};

//==============================================================================
// Value with definition, CachedValue and LiveAccessor
//==============================================================================
template <typename T>
struct ParameterValue {
    using Type = T;
    using Traits = ParameterStorageTraits<Type>;
    using StorageType = typename Traits::StorageType;

    struct LiveAccessor {
        using Reader = Type(*)(void*);

        void set(Reader r, void* ctx) noexcept {
            reader = r;
            context = ctx;
        }

        void reset() noexcept {
            reader = nullptr;
            context = nullptr;
        }

        constexpr auto hasReader() const noexcept -> bool { return reader != nullptr; }

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

    bool hasLiveReader() const noexcept { return liveAccessor.hasReader(); }

    void setStoredValue(Type newValue) {
        value = Traits::toStorage(newValue);
    }

    juce::Value getPropertyAsValue() { return value.getPropertyAsValue(); }

    Type getCurrentValue() const { return getLiveValue(); }

    void setLiveReader(typename LiveAccessor::Reader reader, void* context) noexcept {
        liveAccessor.set(reader, context);
    }

    void clearLiveReader() noexcept {
        liveAccessor.reset();
    }

    const ParameterDef<Type> definition;
    CachedValue<StorageType> value;

private:
    LiveAccessor liveAccessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterValue)
};

//==============================================================================
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
                parameter->valueToStringFunction = [fn](float paramValue) {
                    using Traits = ParameterStorageTraits<typename ParameterValueType::Type>;
                    return fn(Traits::fromFloatValue(paramValue));
                };
            }

            if (value.definition.stringToValueFunction) {
                auto fn = value.definition.stringToValueFunction;
                parameter->stringToValueFunction = [fn](const juce::String& text) {
                    using Traits = ParameterStorageTraits<typename ParameterValueType::Type>;
                    auto typedValue = fn(text);
                    return Traits::toFloatValue(typedValue);
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
    static auto readLiveValue(void* context) noexcept -> typename ParameterValueType::Type {
        auto* param = static_cast<tracktion::AutomatableParameter*>(context);
        jassert(param != nullptr);
        if (param == nullptr)
            return {};

        using Traits = ParameterStorageTraits<typename ParameterValueType::Type>;
        return Traits::fromFloatValue(param->getCurrentValue());
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
        static_cast<Derived*>(this)->visit([&v] (auto& value) {
            tracktion::copyPropertiesToCachedValues(v, value.value );
        });
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
