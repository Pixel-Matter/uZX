#include "ParamBindings.h"
#include "gui/common/MouseListener.h"
#include "controllers/App.h"

using namespace juce;

namespace MoTool {

//==============================================================================
/**
    Widget callbacks tagged with the binding that installed them, so a binding
    can remove its own callbacks and no one else's.
*/
namespace {

template <typename Signature>
struct BoundCallback;

template <typename Result, typename... Args>
struct BoundCallback<Result(Args...)> {
    const void* owner;
    std::function<Result(Args...)> callback;

    Result operator()(Args... args) const {
        return callback(std::forward<Args>(args)...);
    }
};

template <typename Signature>
void installCallback(std::function<Signature>& slot, const void* owner,
                     std::type_identity_t<std::function<Signature>> callback) {
    slot = BoundCallback<Signature> {owner, std::move(callback)};
}

template <typename Signature>
void releaseCallback(std::function<Signature>& slot, const void* owner) {
    if (const auto* bound = slot.template target<BoundCallback<Signature>>(); bound != nullptr && bound->owner == owner)
        slot = nullptr;
}

} // namespace

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
    if (MoToolApp::hasMidiControl())
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
SliderParamEndpointBinding::~SliderParamEndpointBinding() {
    releaseCallback(slider.onValueChange, this);
    releaseCallback(slider.onDragStart, this);
    releaseCallback(slider.onDragEnd, this);
    releaseCallback(slider.textFromValueFunction, this);
    releaseCallback(slider.valueFromTextFunction, this);
}

void SliderParamEndpointBinding::configureWidget() {
    slider.setTooltip(endpoint().getDescription());
    slider.setPopupDisplayEnabled(true, true, nullptr);

    const auto range = endpoint().getRange();
    slider.setRange(static_cast<double>(range.start),
                    static_cast<double>(range.end),
                    static_cast<double>(range.interval));
    slider.setSkewFactor(static_cast<double>(range.skew));

    slider.setNumDecimalPlacesToDisplay(endpoint().getDecimalPlaces());

    installCallback(slider.textFromValueFunction, this, [this](double value) {
        return endpoint().formatValue(value);
    });

    installCallback(slider.valueFromTextFunction, this, [this](const String& text) -> double {
        double parsed {};
        if (endpoint().parseValue(text, parsed))
            return parsed;
        return slider.getValue();
    });

    const auto units = endpoint().getUnits();
    if (units.isNotEmpty())
        slider.setTextValueSuffix(units);
}

void SliderParamEndpointBinding::configureWidgetHandlers() {
    installCallback(slider.onValueChange, this, [this] {
        if (updating)
            return;

        ScopedValueSetter<bool> svs(updating, true);
        endpoint().setStoredFloatValue(static_cast<float>(slider.getValue()));
    });

    installCallback(slider.onDragStart, this, [this] {
        endpoint().beginGesture();
    });

    installCallback(slider.onDragEnd, this, [this] {
        endpoint().endGesture();
    });

    slider.setPopupMenuEnabled(false);
}

void SliderParamEndpointBinding::refreshFromSource() {
    if (updating)
        return;

    ScopedValueSetter<bool> svs(updating, true);
    slider.setValue(static_cast<double>(endpoint().getLiveFloatValue()), dontSendNotification);
}

//==============================================================================
ComboBoxParamEndpointBinding::~ComboBoxParamEndpointBinding() {
    releaseCallback(comboBox.onChange, this);
}

void ComboBoxParamEndpointBinding::configureWidget() {
    comboBox.setTooltip(endpoint().getDescription());
    fillItems();
}

void ComboBoxParamEndpointBinding::configureWidgetHandlers() {
    installCallback(comboBox.onChange, this, [this] {
        if (updating)
            return;

        ScopedValueSetter<bool> svs(updating, true);
        endpoint().setStoredFloatValue(static_cast<float>(comboBox.getSelectedId() - 1));
    });
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
    ScopedValueSetter<bool> svs(updating, true);
    const auto storedState = endpoint().floatToState(endpoint().getLiveFloatValue());
    comboBox.setSelectedId(storedState + 1, dontSendNotification);
}

//============================================================================

ButtonParamEndpointBinding::~ButtonParamEndpointBinding() {
    releaseCallback(button.onClick, this);
}

void ButtonParamEndpointBinding::configureWidget() {
    button.setTooltip(endpoint().getDescription());
    button.setEnabled(endpoint().numberOfStates() > 0);
}

void ButtonParamEndpointBinding::configureWidgetHandlers() {
    installCallback(button.onClick, this, [this] {
        handleClick();
    });
}

void ButtonParamEndpointBinding::handleClick() {
    if (endpoint().numberOfStates() <= 0)
        return;

    const auto currentIndex = getCurrentIndex();
    const auto nextIndex = wrapIndex(currentIndex + 1);

    {
        ScopedValueSetter<bool> svs(updating, true);
        endpoint().beginGesture();
        endpoint().setStoredFloatValue(endpoint().stateToFloat(nextIndex));
        endpoint().endGesture();
    }
}

void ButtonParamEndpointBinding::refreshFromSource() {
    if (updating)
        return;

    ScopedValueSetter<bool> svs(updating, true);
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

//============================================================================

ToggleParamEndpointBinding::~ToggleParamEndpointBinding() {
    releaseCallback(button.onClick, this);
}

void ToggleParamEndpointBinding::configureWidget() {
    button.setTooltip(endpoint().getDescription());
    button.setButtonText(buttonLabel);
}

void ToggleParamEndpointBinding::configureWidgetHandlers() {
    installCallback(button.onClick, this, [this] {
        if (updating)
            return;

        // Toggle the parameter value
        ScopedValueSetter<bool> svs(updating, true);
        const bool currentState = endpoint().getLiveFloatValue() >= 0.5f;
        const bool newState = !currentState;

        endpoint().beginGesture();
        endpoint().setStoredFloatValue(newState ? 1.0f : 0.0f);
        endpoint().endGesture();
    });
}

void ToggleParamEndpointBinding::refreshFromSource() {
    if (updating)
        return;

    ScopedValueSetter<bool> svs(updating, true);
    const bool state = endpoint().getLiveFloatValue() >= 0.5f;
    button.setToggleState(state, dontSendNotification);
}

} // namespace MoTool
