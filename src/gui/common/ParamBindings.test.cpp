#include <JuceHeader.h>
#include <memory>

#include "../../controllers/Parameters.h"
#include "ParamBindings.h"


namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace std::literals;
namespace te = tracktion;

//==============================================================================
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
    std::function<float()> fetchFloatValue;
    std::function<void(float)> applyFloatValue;
    std::function<void()> beginGesture;
    std::function<void()> endGesture;
    bool updating { false };

private:
    void configureAutomationCallbacks();

    template <typename Type>
    void configureStoredValueCallbacks(ParameterValue<Type>& parameterValue) {
        using Traits = ParameterConversionTraits<Type>;

        // TODO invert control?
        // fetchValue = parameterValue.getFetcher()
        // applyValue = parameterValue.getApplier()

        fetchFloatValue = [&parameterValue]() {
            parameterValue.value.forceUpdateOfCachedValue();
            return static_cast<float>(Traits::toFloat(parameterValue.getStoredValue()));
        };

        // because applier of float value can be not not templated
        applyFloatValue = [&parameterValue](float value) {
            parameterValue.setStoredValue(Traits::fromFloat(value));
        };
    }

    // To be able to listen to it
    Value storedValue;

    friend class WidgetBindingTests;
};

class SliderStaticParamBinding : public WidgetStaticParamBinding {
public:
    template <typename Type>
    SliderStaticParamBinding(Slider& s, ParameterValue<Type>& value)
        : WidgetStaticParamBinding(value)
        , slider(s)
    {
        ParameterUIHelpers::configureSliderForParameterDef(slider, value.definition);
        configureSliderHandlers();
        refreshFromSource();
    }

private:
    void configureSliderForAutomationParameter();

    void configureSliderHandlers() {
        slider.onValueChange = [this] {
            if (updating)
                return;

            juce::ScopedValueSetter<bool> svs(updating, true);
            applyFloatValue(static_cast<float>(slider.getValue()));
        };

        slider.onDragStart = [this] {
            if (beginGesture)
                beginGesture();
        };

        slider.onDragEnd = [this] {
            if (endGesture)
                endGesture();
        };

        slider.setPopupMenuEnabled(false);
    }

    void refreshFromSource() {
        if (!fetchFloatValue)
            return;

        if (updating)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        slider.setValue(fetchFloatValue(), dontSendNotification);
    }

    void valueChanged(Value&) override {
        refreshFromSource();
    }

    Slider& slider;
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

            ParameterValue<float> value {{"testParam", "testParam", "Test Param", "A parameter for testing", 0.5f, {0.f, 1.0f}}};
            value.referTo(state, nullptr);
            Slider slider;
            SliderStaticParamBinding binding(slider, value);

            slider.setValue(0.3, sendNotificationSync);

            expectWithinAbsoluteError(static_cast<double>(binding.storedValue.getValue()), 0.3, 1e-6,
                                      "Stored value updated from slider");
            expectWithinAbsoluteError((double) value.getStoredValue(), 0.3, 1e-6,
                                      "ParameterValue updated from slider");
        }

    }
};

static WidgetBindingTests widgetBindingTests;

}  // namespace MoTool::Tests
