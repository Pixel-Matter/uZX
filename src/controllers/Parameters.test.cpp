#include <JuceHeader.h>
#include <memory>

#include "BindedAutoParameter.h"
#include "../plugins/uZX/aychip/aychip.h"


namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace std::literals;
namespace te = tracktion;


//==============================================================================
class ParameterBindingTests  : public UnitTest {
    class TestPlugin : public te::Plugin {
    public:
        using te::Plugin::Plugin;

        String getName() const override { return "TestPlugin"; }
        String getSelectableDescription() override { return "Test Plugin for unit tests"; }
        String getPluginType() override { return "TestPlugin"; }
        void initialise(const te::PluginInitialisationInfo&) override {}
        void deinitialise() override {}
        void applyToBuffer(const te::PluginRenderContext&) noexcept override {}

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestPlugin)
    };

public:
    ParameterBindingTests() : UnitTest("ParameterBindingTests", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);

        beginTest("CachedValue conversions sharing a property");
        {
            ValueTree state {te::IDs::PLUGIN};

            CachedValue<int> intValue;
            CachedValue<ChipType> enumValue;

            intValue.referTo(state, "value", nullptr);
            enumValue.referTo(state, "value", nullptr);

            intValue = 1;
            expectEquals(intValue.get(), 1, "intValue set to 1");
            expectEquals(enumValue.get(), ChipType(ChipType::YM), "enumValue reads 1");
            expect(state.getProperty("value").isInt(), "Property stored as int after automation write");

            intValue = 0;
            expectEquals(intValue.get(), 0, "intValue set to 0");
            expectEquals(enumValue.get(), ChipType(ChipType::AY), "enumValue reads 'AY'");
            expect(state.getProperty("value").isInt(), "Property stored as int after automation reset");

            enumValue = ChipType::YM;
            expectEquals(enumValue.get(), ChipType(ChipType::YM), "enumValue reads 'YM'");
            expect(state.getProperty("value").isString(), "Enum write stores short label as string");
            expectEquals(state.getProperty("value").toString(), String("YM"),
                         "Property string matches enum label");
            expectEquals(intValue.get(), 0, "Automation CachedValue can no longer parse string label");
        }

        beginTest("ParameterValue construction and basic usage");
        {
            ValueTree state {te::IDs::PLUGIN};

            ParameterValue<float> paramValue {{"testParam", "testParam", "Test Param", "A parameter for testing", 0.5f, {0.f, 1.0f}}};
            paramValue.referTo(state, nullptr);
            Value varValue = paramValue.getPropertyAsValue();

            expectEquals((double) paramValue.getStoredValue(), 0.5, "Initial value from ParameterValue");
            expectEquals((double) varValue.getValue(), 0.5, "Initial value from storedValue");
        }

        beginTest("BindedAutoParameter no discreteness");
        {
            ValueTree pluginState {te::IDs::PLUGIN};
            pluginState.setProperty(te::IDs::type, "TestPlugin", nullptr);

            ParameterValue<float> parameter {{"volume", "volume", "Volume", "Output volume", 0.5f, {0.f, 1.0f}}};
            parameter.referTo(pluginState, nullptr);

            TestPlugin plugin({*edit, pluginState, true});

            BindedAutoParameter<float> autoParam(plugin, parameter);

            expect(autoParam.getDefaultValue().has_value(), "Default value for float param");
            expectEquals(autoParam.getDefaultValue().value_or(-1), 0.5f, "Default value for float param");

            expect(!autoParam.isDiscrete(), "isDiscrete() reflects parameter definition");
            expectEquals(autoParam.getNumberOfStates(), 0, "No states for float param");
            expectEquals(autoParam.getValueForState(1), 0.f, "Value for state is 0 for float param");
            expectEquals(autoParam.snapToState(0.3f), 0.3f, "snapToState do not snap for float param");

            expect(!autoParam.hasLabels(), "hasLabels() false for float parameter");
            expectEquals(autoParam.getLabelForValue(0.0f), String(), "No label for float param");
            expectEquals(autoParam.getAllLabels().size(), 0, "No labels for float param");
        }

        beginTest("BindedAutoParameter discreteness");
        {
            ValueTree pluginState {te::IDs::PLUGIN};
            pluginState.setProperty(te::IDs::type, "TestPlugin", nullptr);

            ParameterValue<ChipType> parameter {{"chip", "chip", "Chip", "Chip type", ChipType::YM}};
            parameter.referTo(pluginState, nullptr);

            TestPlugin plugin({*edit, pluginState, true});

            BindedAutoParameter<ChipType> autoParam(plugin, parameter);

            expect(autoParam.getDefaultValue().has_value(), "Default value from enum default");
            expectEquals(autoParam.getDefaultValue().value_or(-1), 1.0f, "Default value from enum default");

            expect(autoParam.isDiscrete(), "isDiscrete() reflects parameter definition");
            expectEquals(autoParam.getNumberOfStates(), 2, "Number of states from enum size");
            expectEquals(autoParam.getValueForState(1), 1.0f, "Value for state reflects enum index");
            expectEquals(autoParam.getStateForValue(1.0f), 1, "State for value reflects enum index");
            expectEquals(autoParam.snapToState(0.3f), 0.0f, "snapToState snaps to nearest enum index");
            expectEquals(autoParam.snapToState(1.1f), 1.0f, "snapToState clamps to valid range");

            expect(autoParam.hasLabels(), "hasLabels() true for enum parameter");
            expectEquals(autoParam.getLabelForValue(0.0f), String("AY"), "Label for value reflects enum label");
            expectEquals(autoParam.getLabelForValue(1.0f), String("YM"), "Label for value reflects enum label");
            expectEquals(autoParam.getLabelForValue(0.51f), String("YM"), "Label for val snaps to nearest enum label");

            expectEquals(autoParam.getAllLabels().size(), 2, "All labels from enum labels");
            expectEquals(autoParam.getAllLabels()[0], String("AY"), "All labels from enum labels");
            expectEquals(autoParam.getAllLabels()[1], String("YM"), "All labels from enum labels");
        }

        beginTest("BindedAutoParameter uses automation CachedValue");
        {
            ValueTree pluginState {te::IDs::PLUGIN};
            pluginState.setProperty(te::IDs::type, "TestPlugin", nullptr);

            ParameterValue<ChipType> parameter {{"chip", "chip", "Chip", "Chip type", ChipType::AY}};
            parameter.referTo(pluginState, nullptr);

            expectEquals(parameter.getStoredValue(), ChipType(ChipType::AY),
                         "Stored value initialised to default");
            expect(pluginState[parameter.definition.propertyID].isString(),
                   "Stored property holds string label");

            TestPlugin plugin({*edit, pluginState, true});

            {
                BindedAutoParameter<ChipType> autoParam(plugin, parameter);

                expectEquals(parameter.getLiveValue(), ChipType(ChipType::AY),
                             "Live reader initialises to default");

                autoParam.setParameter(1.0f, juce::dontSendNotification);

                expectEquals(parameter.getLiveValue(), ChipType(ChipType::YM),
                             "Live reader reflects automation value");
                expectEquals(parameter.getStoredValue(), ChipType(ChipType::AY),
                             "Stored value remains unchanged");
                expectEquals(pluginState[parameter.definition.propertyID].toString(), String("AY"),
                             "Stored property stays string label");

                autoParam.updateFromAttachedParamValue();
                expectEquals(parameter.getLiveValue(), ChipType(ChipType::AY),
                             "Live reader updates from ValueTree change");
            }
        }

    }
};

static ParameterBindingTests paramaterBindingTests;

}  // namespace MoTool::Tests
