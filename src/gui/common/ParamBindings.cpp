#include "ParamBindings.h"
#include "gui/common/MouseListener.h"

using namespace juce;

namespace MoTool {

//==============================================================================
//==============================================================================
WidgetParamEndpointBinding::WidgetParamEndpointBinding(Component &c, std::unique_ptr<ParameterEndpoint> endpointIn)
    : ownedEndpoint(std::move(endpointIn))
    , mouseListener(c)
{
    jassert(ownedEndpoint != nullptr);
    ownedEndpoint->addListener(this);
    if (auto param = dynamic_cast<AutomatableParamEndpoint*>(ownedEndpoint.get())) {
        midiMapping.setParameter(param->getAutomatableParameter());
    }
    mouseListener.setRmbCallback([this]() {
        midiMapping.showMappingMenu();
    });
}

WidgetParamEndpointBinding::WidgetParamEndpointBinding(Component &c, te::AutomatableParameter::Ptr parameterIn)
    : WidgetParamEndpointBinding(c, std::make_unique<AutomatableParamEndpoint>(std::move(parameterIn)))
{}

WidgetParamEndpointBinding::~WidgetParamEndpointBinding() {
    if (ownedEndpoint != nullptr)
        ownedEndpoint->removeListener(this);
}

ParameterEndpoint& WidgetParamEndpointBinding::endpoint() noexcept {
    jassert(ownedEndpoint != nullptr);
    return *ownedEndpoint;
}
const ParameterEndpoint& WidgetParamEndpointBinding::endpoint() const noexcept {
    jassert(ownedEndpoint != nullptr);
    return *ownedEndpoint;
}

void WidgetParamEndpointBinding::storedValueChanged(ParameterEndpoint&, float) {
    refreshFromSource();
}

void WidgetParamEndpointBinding::liveValueChanged(ParameterEndpoint&, float) {
    refreshFromSource();
}

//==============================================================================
void SliderParamEndpointBinding::configureWidget() {
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

void SliderParamEndpointBinding::configureWidgetHandlers() {
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

void SliderParamEndpointBinding::refreshFromSource() {
    if (updating)
        return;

    juce::ScopedValueSetter<bool> svs(updating, true);
    slider.setValue(static_cast<double>(endpoint().getLiveFloatValue()), dontSendNotification);
}

//==============================================================================
void ComboBoxParamEndpointBinding::configureWidget() {
    comboBox.setTooltip(endpoint().getDescription());
    fillItems();
}

void ComboBoxParamEndpointBinding::configureWidgetHandlers() {
    comboBox.onChange = [this] {
        if (updating)
            return;

        juce::ScopedValueSetter<bool> svs(updating, true);
        endpoint().setStoredFloatValue(static_cast<float>(comboBox.getSelectedId() - 1));
    };
}

void ComboBoxParamEndpointBinding::fillItems() {
    comboBox.clear();
    for (int i = 0; i < endpoint().numberOfStates(); ++i) {
        auto item = endpoint().stateToLabel(i);
        comboBox.addItem(item, i + 1);
    }
}

void ComboBoxParamEndpointBinding::refreshFromSource() {
    if (updating)
        return;

    juce::ScopedValueSetter<bool> svs(updating, true);
    const auto storedState = endpoint().floatToState(endpoint().getLiveFloatValue());
    comboBox.setSelectedId(storedState + 1, dontSendNotification);
}

//============================================================================

void ButtonParamEndpointBinding::configureWidget() {
    button.setTooltip(endpoint().getDescription());
    button.setEnabled(endpoint().numberOfStates() > 0);
}

void ButtonParamEndpointBinding::configureWidgetHandlers() {
    button.onClick = [this] {
        handleClick();
    };
}

void ButtonParamEndpointBinding::handleClick() {
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

void ButtonParamEndpointBinding::refreshFromSource() {
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

int ButtonParamEndpointBinding::getCurrentIndex() const {
    if (endpoint().numberOfStates() <= 0)
        return 0;

    const auto storedState = endpoint().floatToState(endpoint().getLiveFloatValue());
    return wrapIndex(storedState);
}

int ButtonParamEndpointBinding::wrapIndex(int index) const {
    const auto choiceCount = endpoint().numberOfStates();
    if (choiceCount <= 0)
        return 0;

    index %= choiceCount;
    if (index < 0)
        index += choiceCount;
    return index;
}

} // namespace MoTool
