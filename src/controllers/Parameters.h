#pragma once

#include <JuceHeader.h>
#include "../util/enumchoice.h"

#include <type_traits>


namespace MoTool {

//==============================================================================
// Parameter storage traits
//==============================================================================
/**
    Converts between the strongly typed values we expose to the UI layer and the
    lightweight types we persist inside state trees or JUCE dynamic values.

    Each specialisation also knows how to map the underlying type to a
    floating-point representation which the automation layer expects.
*/
template <typename T, typename Enable = void>
struct ParameterConversionTraits;

template <typename T>
struct ParameterConversionTraits<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    using StorageType = T;

    static constexpr auto toStorage(T value) noexcept -> StorageType { return value; }
    static constexpr auto fromStorage(StorageType value) noexcept -> T { return value; }

    static constexpr auto toFloat(T value) noexcept -> float { return static_cast<float>(value); }
    static constexpr auto fromFloat(float value) noexcept -> T { return static_cast<T>(value); }

    template <typename ConvType>
    static constexpr auto to(T value) noexcept -> ConvType { return static_cast<ConvType>(value); }

    template <typename ConvType>
    static constexpr auto from(ConvType value) noexcept -> T { return static_cast<T>(value); }
};

template <typename T>
struct ParameterConversionTraits<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
    using StorageType = T;

    static constexpr auto toStorage(T value) noexcept -> StorageType { return value; }
    static constexpr auto fromStorage(StorageType value) noexcept -> T { return value; }

    static constexpr auto toFloat(T value) noexcept -> float { return static_cast<float>(value); }
    static constexpr auto fromFloat(float value) noexcept -> T { return static_cast<T>(roundToInt(value)); }

    template <typename ConvType>
    static constexpr auto to(T value) noexcept -> ConvType { return static_cast<ConvType>(value); }

    template <typename ConvType>
    static constexpr auto from(ConvType value) noexcept -> T { return static_cast<T>(value); }
};

template <>
struct ParameterConversionTraits<bool> {
    using StorageType = bool;

    static constexpr auto toStorage(bool value) noexcept -> StorageType { return value; }
    static constexpr auto fromStorage(StorageType value) noexcept -> bool { return value; }

    static constexpr auto toFloat(bool value) noexcept -> float { return value ? 1.0f : 0.0f; }
    static constexpr auto fromFloat(float value) noexcept -> bool { return value >= 0.5f; }

    static constexpr auto toInt(bool value) noexcept -> int { return value ? 1 : 0; }
    static constexpr auto fromInt(int value) noexcept -> bool { return value != 0; }

    template <typename ConvType>
    static constexpr auto to(bool value) noexcept -> ConvType { return static_cast<ConvType>(toInt(value)); }

    template <typename ConvType>
    static constexpr auto from(ConvType value) noexcept -> bool { return fromInt(static_cast<int>(value)); }
};

template <Util::EnumChoiceConcept E>
struct ParameterConversionTraits<E> {
    //TODO String?
    using StorageType = int;


    static constexpr auto toInt(E value) noexcept -> int {
        using EnumType = typename E::Enum;
        using Underlying = typename E::UnderlyingType;
        return static_cast<int>(static_cast<Underlying>(static_cast<EnumType>(value)));
    }

    static constexpr auto fromInt(int value) noexcept -> E { return E(value); }

    static constexpr auto toStorage(E value) noexcept -> StorageType {
        return toInt(value);
    }

    static constexpr auto fromStorage(StorageType value) noexcept -> E { return fromInt(value); }

    static constexpr auto toFloat(E value) noexcept -> float {
        return static_cast<float>(toInt(value));
    }

    static auto fromFloat(float value) noexcept -> E {
        return fromInt(roundToInt(value));
    }

    template <typename ConvType>
    static constexpr auto to(E value) noexcept -> ConvType { return static_cast<ConvType>(toInt(value)); }

    template <typename ConvType>
    static constexpr auto from(ConvType value) noexcept -> E { return fromInt(static_cast<int>(value)); }

    // TODO from/to String?
};

//==============================================================================
// Parameter definition
//==============================================================================
/**
    Describes a parameter exposed by a controller or plugin, including metadata
    for labelling and converting values to text for the UI.
*/
template <typename Type>
struct ParameterDef {
    String identifier;
    Identifier propertyID;
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
        String s = identifier + ": " + description + " [" + String(valueRange.start)
                   + " - " + String(valueRange.end) + "]";
        if (units.isNotEmpty())
            s += " " + units;
        s += " (default " + String(defaultValue) + ")";
        return s;
    }
};

template <>
struct ParameterDef<bool> {
    String identifier;
    Identifier propertyID;
    String shortLabel;
    String description;
    bool defaultValue;

    NormalisableRange<float> getFloatValueRange() const {
        return {0.0f, 1.0f, 1.0f};
    }

    // Keep the existing toString() method
    String toString() const {
        String s = identifier + ": " + description;
        s += " (default " + String(defaultValue ? "true" : "false") + ")";
        return s;
    }
};


template <Util::EnumChoiceConcept E>
struct ParameterDef<E> {
    String identifier;
    Identifier propertyID;
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
        String s = identifier + ": " + description;
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
    typename ParameterConversionTraits<typename T::Type>;
    { t.getStoredValue() } -> std::same_as<typename T::Type>;
    { t.setStoredValue(std::declval<typename T::Type>()) } -> std::same_as<void>;
    { t.getPropertyAsValue() } -> std::same_as<Value>;
};

//==============================================================================
// Value with definition, CachedValue and LiveAccessor
//==============================================================================
/**
    Wraps a parameter definition with accessors to the underlying storage and
    optional runtime readers.

    The stored value typically mirrors a `juce::ValueTree` property while the
    live accessor can be hooked up to engine state that updates in real time.
*/
template <typename T>
struct ParameterValue {
    using Type = T;
    using Traits = ParameterConversionTraits<Type>;
    using StorageType = typename Traits::StorageType;

    /**
        Lightweight functor used by UI controls to query the most up-to-date
        runtime value without touching the persistent store.
    */
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
        , value(state, def.identifier, undoMgr, Traits::toStorage(def.defaultValue))
    {}

    ~ParameterValue() {
        liveAccessor.reset();
    }

    inline void referTo(ValueTree& v, UndoManager* um) {
        value.referTo(v, definition.propertyID, um, Traits::toStorage(definition.defaultValue));

        if (value.isUsingDefault())
            value = Traits::toStorage(definition.defaultValue);
    }

    Type getStoredValue() const {
        return Traits::fromStorage(value.get());
    }

    Type getLiveValue() const {
        return liveAccessor.readOr(getStoredValue());
    }

    bool hasLiveReader() const noexcept {
        return liveAccessor.hasReader();
    }

    void setStoredValue(Type newValue) {
        value = Traits::toStorage(newValue);
    }

    juce::Value getPropertyAsValue() {
        return value.getPropertyAsValue();
    }

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
// Base for automatable parameters with binding to ParameterValue
template <typename Type>
class BindedAutoParameter : public tracktion::AutomatableParameter {
public:
    using tracktion::AutomatableParameter::AutomatableParameter;
    using TypeTraits = ParameterConversionTraits<Type>;

    BindedAutoParameter(tracktion::AutomatableEditItem& editItem, ParameterValue<Type>& paramValue)
        : tracktion::AutomatableParameter(paramValue.definition.identifier, paramValue.definition.shortLabel, editItem, paramValue.definition.getFloatValueRange())
        , definition(paramValue.definition)
        , value(paramValue)
    {
        attachToCurrentValue(value.value);
        value.setLiveReader(&readLiveValue, this);

        if (auto fn = value.definition.valueToStringFunction; fn) {
            valueToStringFunction = [fn](float v) {
                return fn(TypeTraits::fromFloat(v));
            };
        }

        if (auto fn = value.definition.stringToValueFunction; fn) {
            stringToValueFunction = [fn](const juce::String& text) {
                auto typedValue = fn(text);
                return TypeTraits::toFloat(typedValue);
            };
        }
    }

    ~BindedAutoParameter () override {
        detachFromCurrentValue();
        value.clearLiveReader();
    }

    // TODO
    String getParameterName() const          override { return definition.identifier; }
    String getParameterShortName (int) const override { return definition.shortLabel; }
    String getLabel()                        override { return definition.units; }


private:
    static auto readLiveValue(void* context) noexcept -> Type {
        auto* param = static_cast<tracktion::AutomatableParameter*>(context);
        jassert(param != nullptr);
        if (param == nullptr)
            return {};

        return TypeTraits::fromFloat(param->getCurrentValue());
    }

    const ParameterDef<Type>& definition;
    ParameterValue<Type>& value;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BindedAutoParameter)
};

//==============================================================================
// Base for plugins using ParameterValues
class PluginBase: public tracktion::Plugin {
public:
    using tracktion::Plugin::Plugin;

    template <typename Type>
    tracktion::AutomatableParameter* addParam(ParameterValue<Type>& paramValue){
        auto p = new BindedAutoParameter<Type>(*this, paramValue);
        addAutomatableParameter(*p);
        return p;
    }

    // //==============================================================================
    // void restorePluginStateFromValueTree(const ValueTree&) {
    //     for (auto p : getAutomatableParameters()) {
    //         p->updateFromAttachedValue();
    //     }
    // }

};


//==============================================================================
// CRTP base class for parameter structs in plugins
template <typename Derived>
class ParamsBase {
public:
    void referTo(ValueTree& v, UndoManager* um) {
        static_assert(std::is_base_of<ParamsBase, Derived>::value, "Derived must inherit from ParamsBase");
        static_cast<Derived*>(this)->visit([&v, um] (auto& value) {
            value.referTo(v, um);
        });
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
