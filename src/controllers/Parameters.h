#pragma once

#include <JuceHeader.h>
#include "../util/enumchoice.h"

#include <memory>
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
    using LiveType = T;

    template <typename ConvType>
    static constexpr auto to(T value) noexcept -> ConvType { return static_cast<ConvType>(value); }

    template <typename ConvType>
    static constexpr auto from(ConvType value) noexcept -> T { return static_cast<T>(value); }

};

template <typename T>
struct ParameterConversionTraits<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
    using LiveType = T;

    template <typename ConvType>
    static constexpr auto to(T value) noexcept -> ConvType { return static_cast<ConvType>(value); }

    template <typename ConvType>
    static constexpr auto from(ConvType value) noexcept -> T {
        if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<ConvType>>, float>)
            return static_cast<T>(roundToInt(value));
        else
            return static_cast<T>(value);
    }

};

template <>
struct ParameterConversionTraits<bool> {
    using LiveType = bool;

    static constexpr auto toInt(bool value) noexcept -> int { return value ? 1 : 0; }
    static constexpr auto fromInt(int value) noexcept -> bool { return value != 0; }

    template <typename ConvType>
    static constexpr auto to(bool value) noexcept -> ConvType { return static_cast<ConvType>(toInt(value)); }

    template <typename ConvType>
    static constexpr auto from(ConvType value) noexcept -> bool {
        if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<ConvType>>, float>)
            return value >= 0.5f;
        else
            return fromInt(static_cast<int>(value));
    }

};

template <Util::EnumChoiceConcept E>
struct ParameterConversionTraits<E> {
    using LiveType = int;

    static constexpr auto toInt(E value) noexcept -> int {
        using EnumType = typename E::Enum;
        using Underlying = typename E::UnderlyingType;
        return static_cast<int>(static_cast<Underlying>(static_cast<EnumType>(value)));
    }

    static constexpr auto fromInt(int value) noexcept -> E { return E(value); }

    template <typename ConvType>
    static constexpr auto to(E value) noexcept -> ConvType {
        if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<ConvType>>, String>)
            return String(std::string(value.getLabel()));
        else
            return static_cast<ConvType>(toInt(value));
    }

    template <typename ConvType>
    static constexpr auto from(ConvType value) noexcept -> E {
        if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<ConvType>>, float>)
            return fromInt(roundToInt(value));
        else if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<ConvType>>, String>)
            return E(value.toStdString());
        else
            return fromInt(static_cast<int>(value));
    }

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

    The stored value typically mirrors a `ValueTree` property while the
    live accessor can be hooked up to engine state that updates in real time.
*/
template <typename T>
struct ParameterValue {
    using Type = T;
    using TypeTraits = ParameterConversionTraits<Type>;

    /**
        Lightweight functor used by UI controls to query the most up-to-date
        runtime value without touching the persistent store.
    */
    struct LiveAccessor {
        using Reader = Type(*)(void*);

        LiveAccessor(Reader r, void* ctx) noexcept
            : reader(r)
            , context(ctx)
        {}

        auto get() const -> Type {
            jassert(reader != nullptr);
            return reader(context);
        }

        auto getContext() const noexcept -> void* {
            return context;
        }

        // void set(Reader r, void* ctx) noexcept {
        //     reader = r;
        //     context = ctx;
        // }

        // void reset() noexcept {
        //     reader = nullptr;
        //     context = nullptr;
        // }

        // constexpr auto hasReader() const noexcept -> bool { return reader != nullptr; }

        // auto readOr(const Type& fallback) const -> Type {
        //     if (reader != nullptr)
        //         return reader(context);
        //     return fallback;
        // }

    private:
        Reader reader { nullptr };
        void* context { nullptr };
        // CachedValue<LiveType> liveValue;  // for attaching automation
    };

    explicit ParameterValue(const ParameterDef<Type>& def)
        : definition(def)
    {}

    ParameterValue(ParameterValue&&) = default;
    ParameterValue& operator= (ParameterValue&&) = default;

    ParameterValue(const ParameterDef<Type>& def, ValueTree& state, UndoManager* undoMgr = nullptr)
        : definition(def)
        , value(state, def.identifier, undoMgr, def.defaultValue)
    {}

    inline void referTo(ValueTree& v, UndoManager* um) {
        value.referTo(v, definition.propertyID, um, definition.defaultValue);

        if (value.isUsingDefault()) {
            // ensure the default value is written to the state tree
            value = definition.defaultValue;
        }
    }

    Type getStoredValue() const {
        return value.get();
    }

    template <typename U>
    U getStoredValueAs() const {
        using Traits = ParameterConversionTraits<Type>;
        return Traits::template to<U>(getStoredValue());
    }

    Type getLiveValue() const {
        if (liveAccessor != nullptr)
            return liveAccessor->get();
        else
            return getStoredValue();
    }

    void* getLiveContext() const noexcept {
        if (liveAccessor != nullptr)
            return liveAccessor->getContext();
        else
            return nullptr;
    }

    bool hasLiveReader() const noexcept {
        return liveAccessor != nullptr;
    }

    void setStoredValue(Type newValue) {
        value = newValue;
    }

    void setStoredValueAs(auto newValue) {
        using Traits = ParameterConversionTraits<Type>;
        value = Traits::from(newValue);
    }

    Value getPropertyAsValue() {
        return value.getPropertyAsValue();
    }

    void setLiveReader(typename LiveAccessor::Reader reader, void* context) noexcept {
        liveAccessor = std::make_unique<LiveAccessor>(reader, context);
    }

    void clearLiveReader() noexcept {
        liveAccessor.reset();
    }

    const ParameterDef<Type> definition;
    CachedValue<Type> value;

private:
    std::unique_ptr<LiveAccessor> liveAccessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterValue)
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
