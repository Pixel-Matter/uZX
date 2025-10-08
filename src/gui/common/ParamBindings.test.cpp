#include <JuceHeader.h>
#include <memory>

#include "../../controllers/Parameters.h"
#include "../../plugins/uZX/aychip/aychip.h"
#include "juce_core/juce_core.h"
#include "ParamBindings.h"


namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace std::literals;
namespace te = tracktion;

//==============================================================================
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
    std::function<WidgetValueType()> fetchValue;
    std::function<void(WidgetValueType)> applyValue;
    // std::function<void()> beginGesture;
    // std::function<void()> endGesture;
    bool updating { false };

private:
    template <typename Type>
    void configureStoredValueCallbacks(ParameterValue<Type>& parameterValue) {
        using Traits = ParameterConversionTraits<Type>;

        // TODO invert control?
        // fetchValue = parameterValue.getFetcher()
        // applyValue = parameterValue.getApplier()

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

        slider.onDragStart = [/*this*/] {
            // if (beginGesture)
            //     beginGesture();
        };

        slider.onDragEnd = [/*this*/] {
            // if (endGesture)
            //     endGesture();
        };

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
class WidgetBindingTests  : public UnitTest {
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
    WidgetBindingTests() : UnitTest("WidgetBindingTests", "MoTool") {}

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

            expect(!autoParam.hasLabels(), "hasLabels() false for enum parameter");
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

static WidgetBindingTests widgetBindingTests;

}  // namespace MoTool::Tests
