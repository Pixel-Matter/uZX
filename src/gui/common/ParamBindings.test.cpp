#include <JuceHeader.h>
#include <memory>

#include "../../controllers/BindedAutoParameter.h"
#include "../../plugins/uZX/aychip/aychip.h"
#include "../../util/TestHelpers.h"
#include "ParamBindings.h"


namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace std::literals;
namespace te = tracktion;

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
            TestHelpers::flushMessageQueue();

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
            TestHelpers::flushMessageQueue();

            expectEquals(button.getButtonText(), String("YM"), "Button text updated from ParameterValue");

            button.triggerClick();
            TestHelpers::flushMessageQueue();

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
            TestHelpers::flushMessageQueue();

            slider.setValue(0.5, sendNotificationAsync);
            expectWithinAbsoluteError(static_cast<double>(param->getCurrentValue()), 0.8, 1e-6,
                                      "AutomatableParameter should stay the same after async slider set");

            TestHelpers::flushMessageQueue();

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
            TestHelpers::flushMessageQueue();

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
            SliderParamEndpointBinding binding(slider, value);

            expectWithinAbsoluteError(slider.getValue(), 0.5, 1e-6,
                                      "Slider initialised from binded parameter");

            liveParam->setParameter(0.8f, sendNotification);
            TestHelpers::flushMessageQueue();
            expectWithinAbsoluteError(slider.getValue(), 0.8, 1e-6,
                                      "Slider reflects automatable parameter updates");

            slider.setValue(0.2, sendNotificationSync);
            expectWithinAbsoluteError(static_cast<double>(liveParam->getCurrentValue()), 0.2, 1e-6,
                                      "Live param value updated from slider through binded parameter");

            TestHelpers::flushMessageQueue();
            expectWithinAbsoluteError(static_cast<double>(value.getStoredValue()), 0.2, 1e-6,
                                      "ParameterValue updated from slider through binded parameter");

            value.setStoredValue(0.65f);
            TestHelpers::flushMessageQueue();
            expectWithinAbsoluteError(static_cast<double>(liveParam->getCurrentValue()), 0.2, 1e-6,
                                      "Live param value not updated from ParameterValue state change");
            expectWithinAbsoluteError(slider.getValue(), 0.2, 1e-6,
                                      "Slider not refreshed after ParameterValue state change");

            plugin.restorePluginStateFromValueTree(pluginState);
            TestHelpers::flushMessageQueue();
            expectWithinAbsoluteError(slider.getValue(), 0.65, 1e-6,
                                      "Slider refreshed after restoring ParameterValue state");

        }

        beginTest("ToggleButton - button click toggles parameter");
        {
            ValueTree state {te::IDs::PLUGIN};
            ParameterValue<bool> value{{"toggle", "toggle", "T", "Toggle test", false}};
            value.referTo(state, nullptr);

            TextButton button;
            ToggleParamEndpointBinding binding(button, value);

            // Initial state
            expect(!button.getToggleState(), "Button initially off");
            expect(!value.getStoredValue(), "Parameter initially false");

            // Click button
            button.triggerClick();
            TestHelpers::flushMessageQueue();

            // Should toggle parameter
            expect(value.getStoredValue(), "Parameter true after click");
            expect(button.getToggleState(), "Button on after click");

            // Click again
            button.triggerClick();
            TestHelpers::flushMessageQueue();

            // Should toggle back
            expect(!value.getStoredValue(), "Parameter false after second click");
            expect(!button.getToggleState(), "Button off after second click");
        }

        beginTest("ToggleButton - parameter change updates button");
        {
            ValueTree state {te::IDs::PLUGIN};
            ParameterValue<bool> value{{"toggle", "toggle", "T", "Toggle test", false}};
            value.referTo(state, nullptr);

            TextButton button;
            ToggleParamEndpointBinding binding(button, value);

            // Initial state
            expect(!button.getToggleState(), "Button initially off");

            // Change parameter
            value.setStoredValue(true);
            TestHelpers::flushMessageQueue();

            // Button should update
            expect(button.getToggleState(), "Button on after parameter changed to true");

            // Change back
            value.setStoredValue(false);
            TestHelpers::flushMessageQueue();

            // Button should update
            expect(!button.getToggleState(), "Button off after parameter changed to false");
        }

        beginTest("ToggleButton - displays shortLabel");
        {
            ValueTree state {te::IDs::PLUGIN};
            ParameterValue<bool> value{{"toggle", "toggle", "TestBtn", "Toggle test", false}};
            value.referTo(state, nullptr);

            TextButton button;
            ToggleParamEndpointBinding binding(button, value);

            expectEquals(button.getButtonText(), String("TestBtn"), "Button displays shortLabel");
        }
    }
};

static WidgetBindingTests widgetBindingTests;

}  // namespace MoTool::Tests
