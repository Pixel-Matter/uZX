#include <JuceHeader.h>
#include <memory>
#include <type_traits>
#include <utility>
#include <cmath>

#include "../../controllers/BindedAutoParameter.h"
#include "../../controllers/ParamEndpoint.h"
#include "../../plugins/uZX/aychip/aychip.h"
#include "ParamBindings.h"


namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace std::literals;
namespace te = tracktion;

//==============================================================================
class WidgetParamEndpointBinding : private ParameterEndpoint::Listener {
public:
    explicit WidgetParamEndpointBinding(std::unique_ptr<ParameterEndpoint> endpointIn)
        : ownedEndpoint(std::move(endpointIn))
    {
        jassert(ownedEndpoint != nullptr);
        ownedEndpoint->addListener(this);
    }

    template <typename Type>
    explicit WidgetParamEndpointBinding(ParameterValue<Type>& paramValue)
        : WidgetParamEndpointBinding(std::make_unique<StaticParamEndpoint<Type>>(paramValue))
    {}

    explicit WidgetParamEndpointBinding(te::AutomatableParameter::Ptr parameterIn)
        : WidgetParamEndpointBinding(std::make_unique<AutomatedParamEndpoint>(std::move(parameterIn)))
    {}

    ~WidgetParamEndpointBinding() override {
        if (ownedEndpoint != nullptr)
            ownedEndpoint->removeListener(this);
    }

protected:
    virtual void refreshFromSource() = 0;

    bool updating { false };

    ParameterEndpoint& endpoint() noexcept {
        jassert(ownedEndpoint != nullptr);
        return *ownedEndpoint;
    }
    const ParameterEndpoint& endpoint() const noexcept {
        jassert(ownedEndpoint != nullptr);
        return *ownedEndpoint;
    }

private:
    void storedValueChanged(ParameterEndpoint&, float) override {
        refreshFromSource();
    }

    void liveValueChanged(ParameterEndpoint&, float) override {
        refreshFromSource();
    }

    std::unique_ptr<ParameterEndpoint> ownedEndpoint;

    friend class WidgetBindingTests;
};

//==============================================================================
class SliderParamEndpointBinding : public WidgetParamEndpointBinding {
public:
    SliderParamEndpointBinding(Slider& s, auto&& endpoint)
        : WidgetParamEndpointBinding(std::forward<decltype(endpoint)>(endpoint))
        , slider(s)
    {
        configureWidget();
        configureWidgetHandlers();
        refreshFromSource();
    }

private:
    void configureWidget() {
        slider.setTooltip(endpoint().getDescription());
        slider.setPopupDisplayEnabled(true, true, nullptr);

        const auto range = endpoint().getRange();
        slider.setRange(static_cast<double>(range.start),
                        static_cast<double>(range.end),
                        static_cast<double>(range.interval));
        slider.setSkewFactor(static_cast<double>(range.skew));

        slider.setNumDecimalPlacesToDisplay(endpoint().getDecimalPlaces());

        slider.textFromValueFunction = [this](double value) {
            return endpoint().formatValue(value);
        };

        slider.valueFromTextFunction = [this](const String& text) -> double {
            double parsed {};
            if (endpoint().parseValue(text, parsed))
                return parsed;
            return slider.getValue();
        };

        const auto units = endpoint().getUnits();
        if (units.isNotEmpty())
            slider.setTextValueSuffix(units);
    }

    void configureWidgetHandlers() {
        slider.onValueChange = [this] {
            if (updating)
                return;

            juce::ScopedValueSetter<bool> svs(updating, true);
            endpoint().setStoredFloatValue(static_cast<float>(slider.getValue()));
        };

        slider.onDragStart = [this] {
            endpoint().beginGesture();
        };

        slider.onDragEnd = [this] {
            endpoint().endGesture();
        };

        slider.setPopupMenuEnabled(false);
    }

    void refreshFromSource() override {
        if (updating)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        slider.setValue(static_cast<double>(endpoint().getLiveFloatValue()), dontSendNotification);
    }

    Slider& slider;
};

//============================================================================
class ButtonParamEndpointBinding : public WidgetParamEndpointBinding {
public:
    ButtonParamEndpointBinding(Button& b, auto&& endpoint)
        : WidgetParamEndpointBinding(std::forward<decltype(endpoint)>(endpoint))
        , button(b)
    {
        configureWidget();
        configureWidgetHandlers();
        refreshFromSource();
    }

private:
    void configureWidget() {
        button.setTooltip(endpoint().getDescription());
        button.setEnabled(endpoint().numberOfStates() > 0);
    }

    void configureWidgetHandlers() {
        button.onClick = [this] {
            handleClick();
        };
    }

    void handleClick() {
        if (endpoint().numberOfStates() <= 0)
            return;

        const auto currentIndex = getCurrentIndex();
        const auto nextIndex = wrapIndex(currentIndex + 1);

        {
            juce::ScopedValueSetter<bool> svs(updating, true);
            endpoint().beginGesture();
            endpoint().setStoredFloatValue(endpoint().stateToFloat(nextIndex));
            endpoint().endGesture();
        }
    }

    void refreshFromSource() override {
        if (updating)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        if (endpoint().numberOfStates() <= 0) {
            button.setButtonText(endpoint().formatValue(endpoint().getLiveFloatValue()));
            return;
        }

        const auto storedState = endpoint().floatToState(endpoint().getLiveFloatValue());
        const auto index = wrapIndex(storedState);
        button.setButtonText(endpoint().stateToLabel(index));
    }

    int getCurrentIndex() const {
        if (endpoint().numberOfStates() <= 0)
            return 0;

        const auto storedState = endpoint().floatToState(endpoint().getLiveFloatValue());
        return wrapIndex(storedState);
    }

    int wrapIndex(int index) const {
        const auto choiceCount = endpoint().numberOfStates();
        if (choiceCount <= 0)
            return 0;

        index %= choiceCount;
        if (index < 0)
            index += choiceCount;
        return index;
    }

    Button& button;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ButtonParamEndpointBinding)
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
            SliderParamEndpointBinding binding(slider, value);

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
            SliderParamEndpointBinding binding(slider, value);

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
            ButtonParamEndpointBinding binding(button, value);

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
            SliderParamEndpointBinding binding(slider, param);

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
            ButtonParamEndpointBinding binding(button, param);

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
