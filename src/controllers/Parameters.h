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
        : BindedAutoParameterBase(paramValue.definition.identifier, paramValue.definition.shortLabel, editItem, paramValue.definition.getFloatValueRange())
        , definition(paramValue.definition)
        , parameterValue(paramValue)
    {
        // NOTE while value is not attached, we can not restore from it
        // attachToCurrentValue(value.value);
        // updateFromAttachedValue();
        updateFromAttachedParamValue();

        parameterValue.setLiveReader(&readLiveValue, this);

        if (auto fn = parameterValue.definition.valueToStringFunction; fn) {
            valueToStringFunction = [fn](float v) {
                return fn(TypeTraits::from(v));
            };
        }

        if (auto fn = parameterValue.definition.stringToValueFunction; fn) {
            stringToValueFunction = [fn](const String& text) {
                return TypeTraits::template to<float>(fn(text));
            };
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
        return getValueForState(getStateForValue(val));
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
class PluginBase: public tracktion::Plugin {
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
