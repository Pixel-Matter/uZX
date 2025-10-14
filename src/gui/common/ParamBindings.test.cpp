#include <JuceHeader.h>
#include <cmath>
#include <limits>
#include <memory>
#include <type_traits>

#include "../../controllers/BindedAutoParameter.h"
#include "../../plugins/uZX/aychip/aychip.h"
#include "ParamBindings.h"


namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace std::literals;
namespace te = tracktion;

//==============================================================================
/**
    Maybe we should have

    WidgetHandlerAdapter <=> ParamEndpoint

    - WidgetHandlerAdapter
        - Has handlers, but different for each widget type
        - Can call
        - Has MidiParameterMapping, MouseListenerWithCallback

        - SliderHandlerAdapter: WidgetHandlerAdapter
          - Value in double (casted from/to float)
        - ButtonHandlerAdapter: WidgetHandlerAdapter
          - Value in String (casted from/to int index) via ParamEndpoint::stateToLabel

    - ParamEndpoint
        - Has listeners, but different for each parameter type
        - Unified interface for parameter metadata (range, isDiscrete, numberOfStates, stateToLabel, etc)
        - Has setters and getters
        - Can call
        - AutoParamEndpoint: ParamEndpoint
          - Value always in float
        - ParamValueEndpoint: ParamEndpoint
          - Value in templated type

    And construct WidgetParamBinding<SliderHandlerAdapter, ParamValueEndpoint>?
*/

//==============================================================================
// Interface for parameter endpoint, to be used by WidgetParamBinding
// Unifies access to parameter metadata and value conversion
// both for AutomatableParameter and ParameterValue<T>
// TODO Refactor as a concept?
class ParameterEndpoint {
public:
    virtual ~ParameterEndpoint() = default;
    virtual Range<float> getRange() const = 0;
    virtual float getLiveFloatValue() const = 0;
    virtual float getStoredFloatValue() const = 0;
    virtual void setStoredFloatValue(float value) = 0;
    virtual void beginGesture() = 0;
    virtual void endGesture() = 0;

    virtual bool isDiscrete() const noexcept = 0;
    virtual int numberOfStates() const noexcept = 0;
    virtual int floatToState(float value) const = 0;
    virtual float stateToFloat(int state) const = 0;

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

    void addListener(Listener* l) {
        listeners.add(l);
    }

    void removeListener(Listener* l) {
        listeners.remove(l);
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
// For binding ParameterValue<Type> to widgets
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

    Range<float> getRange() const override {
        const auto floatRange = parameterValue.definition.getFloatValueRange();
        return {floatRange.start, floatRange.end};
    }

    float getLiveFloatValue() const override {
        using Traits = ParameterConversionTraits<Type>;
        return Traits::template to<float>(parameterValue.getLiveValue());
    }

    float getStoredFloatValue() const override {
        using Traits = ParameterConversionTraits<Type>;
        parameterValue.value.forceUpdateOfCachedValue();
        return Traits::template to<float>(parameterValue.getStoredValue());
    }

    void setStoredFloatValue(float value) override {
        parameterValue.setStoredValueAs(value);
        notifyStoredValueChanged(getStoredFloatValue());
    }

    void beginGesture() override {}
    void endGesture() override {}

    bool isDiscrete() const noexcept override {
        if constexpr (std::is_floating_point_v<Type>)
            return parameterValue.definition.getFloatValueRange().interval != 0.0f;
        else
            return true;
    }

    int numberOfStates() const noexcept override {
        if (!isDiscrete())
            return 0;

        if constexpr (std::is_same_v<Type, bool>) {
            return 2;
        } else if constexpr (Util::EnumChoiceConcept<Type>) {
            return static_cast<int>(Type::size());
        } else {
            const auto floatRange = parameterValue.definition.getFloatValueRange();
            if (floatRange.interval <= 0.0f)
                return 0;
            const auto steps = (floatRange.end - floatRange.start) / floatRange.interval;
            return jmax(0, static_cast<int>(std::floor(steps + 0.5f)) + 1);
        }
    }

    int floatToState(float value) const override {
        const auto stateCount = numberOfStates();
        if (stateCount <= 0)
            return 0;

        if constexpr (std::is_same_v<Type, bool>) {
            return value >= 0.5f ? 1 : 0;
        } else if constexpr (Util::EnumChoiceConcept<Type>) {
            return jlimit(0, stateCount - 1, roundToInt(value));
        } else {
            const auto floatRange = parameterValue.definition.getFloatValueRange();
            auto interval = floatRange.interval;
            if (interval == 0.0f)
                interval = 1.0f;
            const auto index = roundToInt((value - floatRange.start) / interval);
            return jlimit(0, stateCount - 1, index);
        }
    }

    float stateToFloat(int state) const override {
        const auto stateCount = numberOfStates();
        if (stateCount <= 0)
            return getStoredFloatValue();

        state = jlimit(0, stateCount - 1, state);

        if constexpr (std::is_same_v<Type, bool>) {
            return state != 0 ? 1.0f : 0.0f;
        } else if constexpr (Util::EnumChoiceConcept<Type>) {
            auto value = ParameterConversionTraits<Type>::fromInt(state);
            return ParameterConversionTraits<Type>::template to<float>(value);
        } else {
            const auto floatRange = parameterValue.definition.getFloatValueRange();
            auto interval = floatRange.interval;
            if (interval == 0.0f)
                interval = 1.0f;
            return floatRange.start + interval * static_cast<float>(state);
        }
    }

    String stateToLabel(int index) const override {
        const auto stateCount = numberOfStates();
        if (stateCount <= 0)
            return {};

        index = jlimit(0, stateCount - 1, index);

        if constexpr (std::is_same_v<Type, bool>) {
            return index == 0 ? "Off" : "On";
        } else if constexpr (Util::EnumChoiceConcept<Type>) {
            auto value = ParameterConversionTraits<Type>::fromInt(index);
            if constexpr (requires { parameterValue.definition.valueToStringFunction; }) {
                if (parameterValue.definition.valueToStringFunction)
                    return parameterValue.definition.valueToStringFunction(value);
            }
            return ParameterConversionTraits<Type>::template to<String>(value);
        } else {
            const auto typedValue = valueFromState(index);
            if constexpr (requires { parameterValue.definition.valueToStringFunction; }) {
                if (parameterValue.definition.valueToStringFunction)
                    return parameterValue.definition.valueToStringFunction(typedValue);
            }
            return String(typedValue);
        }
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
    Type valueFromState(int state) const {
        if constexpr (std::is_same_v<Type, bool>) {
            return state != 0;
        } else if constexpr (Util::EnumChoiceConcept<Type>) {
            return ParameterConversionTraits<Type>::fromInt(state);
        } else {
            const auto& range = parameterValue.definition.valueRange;
            auto interval = range.interval;

            if constexpr (std::is_floating_point_v<Type>) {
                if (std::abs(interval) <= std::numeric_limits<Type>::epsilon())
                    interval = static_cast<Type>(1);
            } else {
                if (interval == static_cast<Type>(0))
                    interval = static_cast<Type>(1);
            }

            return static_cast<Type>(range.start + interval * static_cast<Type>(state));
        }
    }

    void valueChanged(Value&) override {
        notifyStoredValueChanged(getStoredFloatValue());
    }

    ParameterValue<Type>& parameterValue;
    Value storedValue;

    friend class WidgetBindingTests;
};

//==============================================================================
template <typename WidgetValueType>
class WidgetStaticParamBinding : private ParameterEndpoint::Listener {
public:
    explicit WidgetStaticParamBinding(std::unique_ptr<ParameterEndpoint> endpointIn)
        : ownedEndpoint(std::move(endpointIn))
    {
        jassert(ownedEndpoint != nullptr);
        endpointPtr = ownedEndpoint.get();
        jassert(endpointPtr != nullptr);
        configureEndpointCallbacks();
        endpointPtr->addListener(this);
    }

    ~WidgetStaticParamBinding() override {
        if (endpointPtr != nullptr)
            endpointPtr->removeListener(this);
    }

protected:
    virtual void refreshFromSource() = 0;

    std::function<WidgetValueType()> fetchValue;
    std::function<void(WidgetValueType)> applyValue;
    bool updating { false };

    ParameterEndpoint& endpoint() noexcept { return *endpointPtr; }
    const ParameterEndpoint& endpoint() const noexcept { return *endpointPtr; }

private:
    void configureEndpointCallbacks() {
        fetchValue = [this]() -> WidgetValueType {
            if constexpr (std::is_same_v<WidgetValueType, int>)
                return endpointPtr->floatToState(endpointPtr->getStoredFloatValue());
            else
                return static_cast<WidgetValueType>(endpointPtr->getStoredFloatValue());
        };

        applyValue = [this](WidgetValueType value) {
            if constexpr (std::is_same_v<WidgetValueType, int>)
                endpointPtr->setStoredFloatValue(endpointPtr->stateToFloat(value));
            else
                endpointPtr->setStoredFloatValue(static_cast<float>(value));
        };
    }

    void storedValueChanged(ParameterEndpoint&, float) override {
        refreshFromSource();
    }

    void liveValueChanged(ParameterEndpoint&, float newValue) override {
        juce::ignoreUnused(newValue);
    }

    std::unique_ptr<ParameterEndpoint> ownedEndpoint;
    ParameterEndpoint* endpointPtr { nullptr };

    friend class WidgetBindingTests;
};

//==============================================================================
class SliderStaticParamBinding : public WidgetStaticParamBinding<float> {
public:
    template <typename Type>
    SliderStaticParamBinding(Slider& s, ParameterValue<Type>& value)
        : WidgetStaticParamBinding<float>(std::make_unique<StaticParamEndpoint<Type>>(value))
        , slider(s)
    {
        configureWidgetForParameter(value.definition);
        configureWidgetHandlers();
        refreshFromSource();
    }

private:
    template <typename Type>
    inline void configureWidgetForParameter(const ParameterDef<Type>& def) {
        slider.setTooltip(def.description);
        slider.setPopupDisplayEnabled(true, true, nullptr);

        const auto floatRange = def.getFloatValueRange();
        slider.setRange(static_cast<double>(floatRange.start),
                        static_cast<double>(floatRange.end),
                        static_cast<double>(floatRange.interval));
        slider.setSkewFactor(static_cast<double>(floatRange.skew));

        slider.setNumDecimalPlacesToDisplay(ParameterUIHelpers::decimalPlacesFor<Type>());

        if (def.valueToStringFunction)
            slider.textFromValueFunction = ParameterUIHelpers::wrapValueToString<Type>(def.valueToStringFunction);

        if (def.stringToValueFunction)
            slider.valueFromTextFunction = ParameterUIHelpers::wrapStringToValue<Type>(def.stringToValueFunction);

        if (def.units.isNotEmpty())
            slider.setTextValueSuffix(def.units);
    }

    void configureWidgetHandlers() {
        slider.onValueChange = [this] {
            if (updating)
                return;

            juce::ScopedValueSetter<bool> svs(updating, true);
            applyValue(static_cast<float>(slider.getValue()));
        };

        // slider.onDragStart = [/*this*/] {
        //     // if (beginGesture)
        //     //     beginGesture();
        // };

        // slider.onDragEnd = [/*this*/] {
        //     // if (endGesture)
        //     //     endGesture();
        // };

        slider.setPopupMenuEnabled(false);
    }

    void refreshFromSource() override {
        if (!fetchValue)
            return;

        if (updating)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        slider.setValue(fetchValue(), dontSendNotification);
    }

    Slider& slider;
};

//============================================================================
class ButtonStaticParamBinding : public WidgetStaticParamBinding<int> {
public:
    template <typename Type>
    ButtonStaticParamBinding(Button& b, ParameterValue<Type>& value)
        : WidgetStaticParamBinding<int>(std::make_unique<StaticParamEndpoint<Type>>(value))
        , button(b)
    {
        configureForParameterDef(value.definition);
        configureWidgetHandlers();
        refreshFromSource();
    }

private:
    template <typename DefType>
    void configureForParameterDef(const DefType& def) {
        button.setTooltip(def.description);

        choiceCount = endpoint().numberOfStates();
        indexToLabel = [this](int index) -> String {
            return endpoint().stateToLabel(index);
        };
    }

    void configureWidgetHandlers() {
        button.onClick = [this] {
            handleClick();
        };
    }

    void handleClick() {
        if (!applyValue || choiceCount <= 0)
            return;

        const auto currentIndex = getCurrentIndex();
        const auto nextIndex = wrapIndex(currentIndex + 1);

        // if (beginGesture)
        //     beginGesture();

        {
            juce::ScopedValueSetter<bool> svs(updating, true);
            applyValue(nextIndex);
        }

        // if (endGesture)
        //     endGesture();
    }

    void refreshFromSource() override {
        if (updating)
            return;

        if (!fetchValue || !indexToLabel || choiceCount <= 0)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        const auto intValue = fetchValue();
        const auto index = wrapIndex(intValue);
        button.setButtonText(indexToLabel(index));
    }

    int getCurrentIndex() const {
        if (choiceCount <= 0)
            return 0;

        return wrapIndex(fetchValue());
    }

    int wrapIndex(int index) const {
        if (choiceCount <= 0)
            return 0;

        index %= choiceCount;
        if (index < 0)
            index += choiceCount;
        return index;
    }

    std::function<String(int)> indexToLabel;

    int choiceCount { 0 };

    Button& button;
};

//==============================================================================
// For parameters without ParameterValue<T>, for example vanilla tracktion AutomatableParameters
class WidgetAutoParamBinding : private te::AutomatableParameter::Listener {
public:
    WidgetAutoParamBinding(te::AutomatableParameter::Ptr p)
        : parameter(std::move(p))
    {
        if (!ensureAttached())
            return;

        configureParameterCallbacks();
        parameter->addListener(this);
    }

    ~WidgetAutoParamBinding() override {
        if (!ensureAttached())
            return;
        parameter->removeListener(this);
    }

    bool ensureAttached() const {
        jassert(parameter != nullptr);
        return parameter != nullptr;
    }

    // bool isAttached() const noexcept {
    //     return parameter != nullptr;
    // }

protected:
    te::AutomatableParameter::Ptr parameter;

    // Called by AutomatableParameter listener to refresh widget from ParameterValue
    std::function<float()> fetchValue;
    // Called by widget handlers to apply value to AutomatableParameter
    std::function<void(float)> applyValue;

    std::function<void()> beginGesture;
    std::function<void()> endGesture;
    bool updating { false };

private:
    void curveHasChanged(te::AutomatableParameter&) override {}

    void configureParameterCallbacks() {
        fetchValue = [this] {
            return static_cast<double>(parameter->getCurrentValue());
        };
        applyValue = [this](double widgetValue) {
            // TODO what if widgetValue is not float? String, int...
            // Notification needed for attachedValue, for example
            parameter->setParameter(static_cast<float>(widgetValue), juce::sendNotification);
        };
        beginGesture = [this] { parameter->parameterChangeGestureBegin(); };
        endGesture   = [this] { parameter->parameterChangeGestureEnd(); };
    }

    friend class WidgetBindingTests;
};

//==============================================================================
class SliderAutoParamBinding : public WidgetAutoParamBinding {
public:
    SliderAutoParamBinding(Slider& s, te::AutomatableParameter::Ptr p)
        : WidgetAutoParamBinding(std::move(p))
        , slider(s)
    {
        configureWidget();
        configureWidgetHandlers();
        refreshFromSource();
    }

private:
    void configureWidget() {
        // depends on param
        if (!ensureAttached())
            return;

        slider.setTooltip(parameter->getParameterName());
        slider.setPopupDisplayEnabled(true, true, nullptr);
        slider.setPopupMenuEnabled(false);

        slider.setRange(parameter->getValueRange().getStart(),
                        parameter->getValueRange().getEnd(),
                        parameter->valueRange.interval);
        slider.setSkewFactor(parameter->valueRange.skew);

        // TODO deduce from interval
        slider.setNumDecimalPlacesToDisplay(2);
        slider.textFromValueFunction = parameter->valueToStringFunction;
        slider.valueFromTextFunction = parameter->stringToValueFunction;

        slider.setValue(parameter->getCurrentValue(), juce::dontSendNotification);
    }

    void configureWidgetHandlers() {
        slider.onValueChange = [this] {
            if (updating || !applyValue)
                return;

            juce::ScopedValueSetter<bool> svs(updating, true);
            applyValue(static_cast<float>(slider.getValue()));
        };

        slider.onDragStart = [this] {
            if (beginGesture)
                beginGesture();
        };

        slider.onDragEnd = [this] {
            if (endGesture)
                endGesture();
        };
    }

    void refreshFromSource() {
        if (!fetchValue)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        slider.setValue(fetchValue(), dontSendNotification);
    }

    void currentValueChanged(te::AutomatableParameter&) override {
        if (updating)
            return;
        refreshFromSource();
    }

    void parameterChanged(te::AutomatableParameter&, float newValue) override {
        if (updating)
            return;
        juce::ScopedValueSetter<bool> svs(updating, true);
        slider.setValue(newValue, dontSendNotification);
    }

    Slider& slider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliderAutoParamBinding)
};

//==============================================================================
class ButtonAutoParamBinding : public WidgetAutoParamBinding {
public:
    ButtonAutoParamBinding(TextButton& button, te::AutomatableParameter::Ptr p)
        : WidgetAutoParamBinding(std::move(p))
        , textButton(button)
    {
        configureWidgetHandlers();
        configureLabelCallbacks();
        refreshFromSource();
    }

    ~ButtonAutoParamBinding() override {
        textButton.onClick = nullptr;
    }

private:
    void configureLabelCallbacks() {
        if (!ensureAttached() || !parameter->isDiscrete() || parameter->getNumberOfStates() <= 0)
            return;

        choiceCount = parameter->getNumberOfStates();

        // indexToLabel = [](int index) -> String {
        //     return getLabelForValue(static_cast)
        // };
    }

    void configureWidgetHandlers() {
        textButton.onClick = [this]() { handleClick(); };
    }

    void refreshFromSource() {
        if (!ensureAttached() || !fetchValue || choiceCount <= 0)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        textButton.setButtonText(parameter->getLabelForValue(fetchValue()));
    }

    void currentValueChanged(te::AutomatableParameter&) override {
        if (updating)
            return;
        refreshFromSource();
    }

    void parameterChanged(te::AutomatableParameter&, float) override {
        if (updating)
            return;
        refreshFromSource();
    }

    void handleClick() {
        if (!ensureAttached() || !applyValue || choiceCount <= 0)
            return;

        const auto currentIndex = getCurrentIndex();
        const auto nextIndex = wrapIndex(currentIndex + 1);

        if (beginGesture)
            beginGesture();

        {
            juce::ScopedValueSetter<bool> svs(updating, true);
            // apply to parameter source or value
            applyValue(parameter->getValueForState(nextIndex));
        }

        if (endGesture)
            endGesture();
    }

    int getCurrentIndex() const {
        if (!ensureAttached() || !fetchValue || choiceCount <= 0)
            return 0;

        return wrapIndex(parameter->getStateForValue(fetchValue()));
    }

    int wrapIndex(int index) const {
        if (choiceCount <= 0)
            return 0;

        index %= choiceCount;
        if (index < 0)
            index += choiceCount;
        return index;
    }

    // std::function<String(int)> indexToLabel;
    int choiceCount { 0 };

    TextButton& textButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ButtonAutoParamBinding)
};

//==============================================================================
class WidgetBindingTests  : public UnitTest {
    class TestPlugin : public PluginBase {
    public:
        using PluginBase::PluginBase;

        String getName() const override { return "TestPlugin"; }
        String getSelectableDescription() override { return "Test Plugin for unit tests"; }
        String getPluginType() override { return "TestPlugin"; }
        void initialise(const te::PluginInitialisationInfo&) override {}
        void deinitialise() override {}
        void applyToBuffer(const te::PluginRenderContext&) noexcept override {}

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestPlugin)
    };

    class TestBindedPlugin : public TestPlugin {
    public:
        TestBindedPlugin(ParameterValue<float>& value, te::PluginCreationInfo info)
            : TestPlugin(info)
            , parameterValue(value)
        {
            addParam(parameterValue);
        }

    private:
        ParameterValue<float>& parameterValue;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestBindedPlugin)
    };

public:
    WidgetBindingTests() : UnitTest("WidgetBindingTests", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);

        beginTest("WidgetParamBinding from stored value");
        {
            ValueTree state {te::IDs::PLUGIN};

            ParameterValue<float> value {{"testParam", "testParam", "Test Param", "A parameter for testing", 0.5f, {0.f, 1.0f}}};
            value.referTo(state, nullptr);
            Slider slider;
            SliderStaticParamBinding binding(slider, value);

            value.setStoredValue(0.8f);

            // Stored value listeners fire asynchronously on the message thread.
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

            expectWithinAbsoluteError(slider.getValue(), 0.8, 1e-6,
                                      "Slider updated from ParameterValue");
            expectWithinAbsoluteError(static_cast<double>(value.getStoredValue()), 0.8, 1e-6,
                                      "ParameterValue holds updated value");
        }

        beginTest("WidgetParamBinding from slider");
        {
            ValueTree state {te::IDs::PLUGIN};

            ParameterValue<float> value{
                {"testParam", "testParam", "Test Param", "A parameter for testing", 0.5f, {0.f, 1.0f}}};
            value.referTo(state, nullptr);
            Slider slider;
            SliderStaticParamBinding binding(slider, value);

            slider.setValue(0.3, sendNotificationSync);

            expectWithinAbsoluteError((double) value.getStoredValue(), 0.3, 1e-6,
                                      "ParameterValue updated from slider");
        }

        beginTest("ParameterValue<ChipType> interaction");
        {
            ValueTree state {te::IDs::PLUGIN};

            ParameterValue<ChipType> value {{"chip", "chip", "Chip", "Chip type", ChipType::AY}};
            value.referTo(state, nullptr);

            expectEquals(value.getStoredValue(), ChipType(ChipType::AY), "Initial value from ParameterValue");

            TextButton button;
            ButtonStaticParamBinding binding(button, value);

            expectEquals(button.getButtonText(), String("AY"), "Initial button text");

            value.setStoredValue(ChipType::YM);

            // Stored value listeners fire asynchronously on the message thread.
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

            expectEquals(button.getButtonText(), String("YM"), "Button text updated from ParameterValue");

            button.triggerClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

            expectEquals(value.getStoredValue(), ChipType(ChipType::AY),
                         "ParameterValue updated from button click");
        }

        beginTest("WidgetAutoBinding from slider");
        {
            ValueTree pluginState {te::IDs::PLUGIN};
            pluginState.setProperty(te::IDs::type, "TestPlugin", nullptr);
            TestPlugin plugin({*edit, pluginState, true});

            auto param = te::AutomatableParameter::Ptr(
                new te::AutomatableParameter("volume", "Volume", plugin, {0.f, 1.0f}));

            Slider slider;
            SliderAutoParamBinding binding(slider, param);

            param->setParameter(0.3f, sendNotification);

            expectWithinAbsoluteError(static_cast<double>(slider.getValue()), 0.3, 1e-6,
                                      "Slider updated from AutomatableParameter");

            slider.setValue(0.8, sendNotificationSync);

            expectWithinAbsoluteError(static_cast<double>(param->getCurrentValue()), 0.8, 1e-6,
                                      "AutomatableParameter updated from slider sync");

            // should wait to async backsync to slider
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

            slider.setValue(0.5, sendNotificationAsync);
            expectWithinAbsoluteError(static_cast<double>(param->getCurrentValue()), 0.8, 1e-6,
                                      "AutomatableParameter should stay the same after async slider set");

            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

            expectWithinAbsoluteError(static_cast<double>(param->getCurrentValue()), 0.5, 1e-6,
                                      "AutomatableParameter should stay the same after async slider set");

        }
        beginTest("WidgetAutoBinding from button");
        {
            ValueTree pluginState {te::IDs::PLUGIN};
            pluginState.setProperty(te::IDs::type, "TestPlugin", nullptr);
            TestPlugin plugin({*edit, pluginState, true});

            auto param = te::AutomatableParameter::Ptr(
                new te::DiscreteLabelledParameter("mode", "Mode", plugin,
                                                  {0.0f, 2.0f}, 3, {"One", "Two", "Three"}));

            TextButton button;
            ButtonAutoParamBinding binding(button, param);

            expect(param->isDiscrete(), "Parameter is discrete");
            expectEquals(param->getNumberOfStates(), 3, "Parameter has 3 states");
            expectEquals(param->getValueForState(0), 0.f, "Value to state 0");
            expectEquals(param->getValueForState(1), 1.f, "Value to state 1");
            expectEquals(param->getValueForState(2), 2.f, "Value to state 2");

            expectEquals(param->getStateForValue(1.f), 1, "State for value 1");
            expectEquals(param->snapToState(1.1f), 1.f, "To state 1");

            expectEquals(button.getButtonText(), String("One"), "Button initial updated from AutomatableParameter");

            param->setParameter(2.f, sendNotification);

            expectEquals(button.getButtonText(), String("Three"), "Button updated from AutomatableParameter");

            button.triggerClick();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

            expectEquals(param->getCurrentValue(), 0.f, "AutomatableParameter updated from button click");
            expectEquals(button.getButtonText(), String("One"), "Button updated from AutomatableParameter");
        }

        beginTest("Slider binding with BindedAutoParameter from ParameterValue");
        {
            ValueTree pluginState {te::IDs::PLUGIN};
            pluginState.setProperty(te::IDs::type, "TestBindedPlugin", nullptr);

            ParameterDef<float> def {
                "bindedParam",
                "bindedParam",
                "Binded",
                "Binded parameter",
                0.5f,
                {0.0f, 1.0f}
            };

            ParameterValue<float> value { def };
            value.referTo(pluginState, nullptr);

            TestBindedPlugin plugin(value, {*edit, pluginState, true});

            auto liveParam = detail::resolveParamFromValue(value);
            expect(liveParam != nullptr, "Live automatable parameter is attached");

            Slider slider;
            SliderParamBinding binding(slider, te::AutomatableParameter::Ptr(), value);

            expectWithinAbsoluteError(slider.getValue(), 0.5, 1e-6,
                                      "Slider initialised from binded parameter");

            liveParam->setParameter(0.8f, sendNotification);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            expectWithinAbsoluteError(slider.getValue(), 0.8, 1e-6,
                                      "Slider reflects automatable parameter updates");

            slider.setValue(0.2, sendNotificationSync);
            expectWithinAbsoluteError(static_cast<double>(liveParam->getCurrentValue()), 0.2, 1e-6,
                                      "Live param value updated from slider through binded parameter");

            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            expectWithinAbsoluteError(static_cast<double>(value.getStoredValue()), 0.2, 1e-6,
                                      "ParameterValue updated from slider through binded parameter");

            value.setStoredValue(0.65f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            expectWithinAbsoluteError(static_cast<double>(liveParam->getCurrentValue()), 0.2, 1e-6,
                                      "Live param value not updated from ParameterValue state change");
            expectWithinAbsoluteError(slider.getValue(), 0.2, 1e-6,
                                      "Slider not refreshed after ParameterValue state change");

            plugin.restorePluginStateFromValueTree(pluginState);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            expectWithinAbsoluteError(slider.getValue(), 0.65, 1e-6,
                                      "Slider refreshed after restoring ParameterValue state");
        }
    }
};

static WidgetBindingTests widgetBindingTests;

}  // namespace MoTool::Tests
