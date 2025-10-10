#include <JuceHeader.h>
#include <memory>

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
        - ButtonHandlerAdapter: WidgetHandlerAdapter

    - ParamEndpoint
        - Has listeners, but different for each parameter type
        - Has setters and getters
        - Can call
        - AutoParamEndpoint: ParamEndpoint
        - ParamValueEndpoint: ParamEndpoint

    And construct WidgetParamBinding<SliderHandlerAdapter, ParamValueEndpoint>?
*/

//==============================================================================
// For binding vanilla tracktion AutomatableParameters to widgets
template <typename WidgetValueType>
class WidgetStaticParamBinding : private Value::Listener {
public:
    template <typename Type>
    WidgetStaticParamBinding(ParameterValue<Type>& value) {
        storedValue = value.getPropertyAsValue();
        storedValue.addListener(this);
        configureStoredValueCallbacks(value);
    }

    ~WidgetStaticParamBinding() override {
        storedValue.removeListener(this);
    }

protected:
    // Called by Value listener to refresh widget from ParameterValue
    std::function<WidgetValueType()> fetchValue;
    // Called by widget handlers to apply value to ParameterValue
    std::function<void(WidgetValueType)> applyValue;

    // std::function<void()> beginGesture;
    // std::function<void()> endGesture;
    bool updating { false };

private:
    template <typename Type>
    void configureStoredValueCallbacks(ParameterValue<Type>& parameterValue) {
        using Traits = ParameterConversionTraits<Type>;

        // Called by widget handlers
        fetchValue = [&parameterValue]() {
            parameterValue.value.forceUpdateOfCachedValue();
            return Traits::template to<WidgetValueType>(parameterValue.getStoredValue());
        };

        // because applier of float value can be not not templated
        applyValue = [&parameterValue](WidgetValueType value) {
            parameterValue.setStoredValue(Traits::template from<WidgetValueType>(value));
        };
    }

    // To be able to listen to it
    Value storedValue;

    friend class WidgetBindingTests;
};

class SliderStaticParamBinding : public WidgetStaticParamBinding<float> {
public:
    template <typename Type>
    SliderStaticParamBinding(Slider& s, ParameterValue<Type>& value)
        : WidgetStaticParamBinding(value)
        , slider(s)
    {
        ParameterUIHelpers::configureSliderForParameterDef(slider, value.definition);
        configureWidgetHandlers();
        refreshFromSource();
    }

private:
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

    void refreshFromSource() {
        if (!fetchValue)
            return;

        if (updating)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        slider.setValue(fetchValue(), dontSendNotification);
    }

    void valueChanged(Value&) override {
        refreshFromSource();
    }

    Slider& slider;
};

//============================================================================
class ButtonStaticParamBinding : public WidgetStaticParamBinding<int> {
public:
    template <typename Type>
    ButtonStaticParamBinding(Button& b, ParameterValue<Type>& value)
        : WidgetStaticParamBinding(value)
        , button(b)
    {
        configureForParameterDef(value.definition);
        configureWidgetHandlers();
        refreshFromSource();
    }

private:
    template <Util::EnumChoiceConcept Type>
    void configureForParameterDef(const ParameterDef<Type>& def) {
        choiceCount = static_cast<int>(Type::size());

        button.setTooltip(def.description);

        using Traits = ParameterConversionTraits<Type>;

        // valueToStringFunction overrides getLabel()
        if (auto valueToString = def.valueToStringFunction) {
            indexToLabel = [valueToString](int index) -> String {
                return valueToString(Traits::from(index));
            };
        } else {
            indexToLabel = [](int index) -> String {
                return Traits::template to<String>(Traits::from(index+1));
            };
        }
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

    void refreshFromSource() {
        if (updating)
            return;

        if (!fetchValue || !indexToLabel || choiceCount <= 0)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        const auto intValue = fetchValue();
        const auto index = wrapIndex(intValue);
        button.setButtonText(indexToLabel(index));
    }

    void valueChanged(Value&) override {
        refreshFromSource();
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
class WidgetAutoParamBinding : private te::AutomatableParameter::Listener
{
public:
    WidgetAutoParamBinding(te::AutomatableParameter::Ptr p)
        : parameter(std::move(p))
    {
        if (isAttached()) {
            configureParameterCallbacks();
            parameter->addListener(this);
        }
    }

    ~WidgetAutoParamBinding() override {
        if (isAttached()) {
            parameter->removeListener(this);
        }
    }

    bool isAttached() const noexcept {
        return parameter != nullptr;
    }

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
        jassert(isAttached());
        if (!isAttached())
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
        jassert(isAttached());

        if (!isAttached() || !parameter->isDiscrete() || parameter->getNumberOfStates() <= 0)
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
        if (!isAttached() || !fetchValue || choiceCount <= 0)
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
        if (!isAttached() || !applyValue || choiceCount <= 0)
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
        if (!isAttached() || !fetchValue || choiceCount <= 0)
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

            expectWithinAbsoluteError(static_cast<double>(binding.storedValue.getValue()), 0.8, 1e-6,
                                      "Stored value propagated to binding");
            expectWithinAbsoluteError(slider.getValue(), 0.8, 1e-6,
                                      "Slider updated from ParameterValue");
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

            expectWithinAbsoluteError(static_cast<double>(binding.storedValue.getValue()), 0.3, 1e-6,
                                      "Stored value updated from slider");
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
