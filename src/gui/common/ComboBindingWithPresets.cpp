#include "ComboBindingWithPresets.h"

#include <cmath>

namespace MoTool {

ComboBindingWithPresets::ComboBindingWithPresets(
        juce::ComboBox& comboBox,
        ParameterValue<double>& valueParameter,
        const std::vector<std::pair<double, String>>& presetItems,
        bool showPresetLabels,
        bool showUnitsFlag
)
    : select(comboBox)
    , valueParam(valueParameter)
    , presets(presetItems)
    , unitsText(valueParameter.definition.units)
    , showTextForPresets(showPresetLabels)
    , showUnits(showUnitsFlag)
{
    select.addListener(this);
    valueParam.addListener(this);
    configure();
}

ComboBindingWithPresets::~ComboBindingWithPresets() {
    select.removeListener(this);
    valueParam.removeListener(this);
}

void ComboBindingWithPresets::configure() {
    select.setJustificationType(juce::Justification::centredLeft);
    select.setTooltip(valueParam.definition.description);
    select.setTextWhenNothingSelected({});

    fillPresetItems();
    refreshFromParameters();
}

void ComboBindingWithPresets::setDecimalPlaces(int digits) {
    if (digits >= 0) {
        decimalPlaces = digits;
    } else {
        decimalPlaces.reset();
    }

    refreshFromParameters();
}

void ComboBindingWithPresets::valueChanged(juce::Value& value) {
    if (value.refersToSameSourceAs(valueParam.getPropertyAsValue())) {
        valueParam.forceUpdateOfCachedValue();
        refreshFromParameters();
    }
}

void ComboBindingWithPresets::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) {
    if (comboBoxThatHasChanged != &select || updating)
        return;

    if (select.getSelectedId() > 0) {
        handleSelectionChange();
    } else {
        handleTextChange();
    }
}

void ComboBindingWithPresets::fillPresetItems() {
    juce::ScopedValueSetter<bool> guard(updating, true);

    select.clear(juce::dontSendNotification);
    int itemId = presetBaseId;
    for (const auto& preset : presets) {
        select.addItem(preset.second, itemId++);
    }
    select.addSeparator();
    select.addItem("Custom", customItemId);
}

void ComboBindingWithPresets::refreshFromParameters() {
    if (updating)
        return;

    juce::ScopedValueSetter<bool> guard(updating, true);

    const double value = valueParam.getStoredValue();

    auto preset = findPresetForValue(value);
    if (preset >= 0) {
        applyPreset(preset, false, true);
    } else {
        select.setSelectedId(0, juce::dontSendNotification);
        select.setText(formatValue(value, showUnits), juce::dontSendNotification);
        select.setEditableText(true);
    }
}

void ComboBindingWithPresets::handleSelectionChange() {
    const int selectedId = select.getSelectedId();
    if (selectedId == customItemId) {
        juce::ScopedValueSetter<bool> guard(updating, true);
        select.setText(formatValue(valueParam.getStoredValue(), showUnits), juce::dontSendNotification);
        select.setEditableText(true);
        select.showEditor();
        return;
    }

    const int presetIndex = selectedId - presetBaseId;
    if (presetIndex < 0 || presetIndex >= static_cast<int>(presets.size())) {
        return;
    }

    applyPreset(presetIndex, true, true);
}

void ComboBindingWithPresets::handleTextChange() {
    const auto& range = valueParam.definition.valueRange;
    auto text = removeUnits(select.getText().trim());

    double parsed = text.getDoubleValue();
    if (text.isEmpty() || std::isnan(parsed))
        parsed = valueParam.definition.defaultValue;

    const double clamped = jlimit(range.start, range.end, parsed);
    if (std::abs(clamped - parsed) > 1e-9) {
        juce::ScopedValueSetter<bool> guard(updating, true);
        select.setText(formatValue(clamped, showUnits), juce::dontSendNotification);
    }

    const int presetIndex = findPresetForValue(clamped);
    if (presetIndex >= 0) {
        applyPreset(presetIndex, false, false);
    }

    valueParam.setStoredValue(clamped);
}

int ComboBindingWithPresets::findPresetForValue(double freqMHz) const {
    constexpr double tolerance = 1e-6;
    for (size_t i = 0; i < presets.size(); ++i) {
        if (std::abs(presets[i].first - freqMHz) <= tolerance)
            return static_cast<int>(i);
    }
    return -1;
}

void ComboBindingWithPresets::applyPreset(int presetIndex, bool updateParameter, bool updateText) {
    if (presetIndex < 0 || presetIndex >= static_cast<int>(presets.size()))
        return;

    const auto& preset = presets[static_cast<size_t>(presetIndex)];

    juce::ScopedValueSetter<bool> guard(updating, true);

    if (updateParameter)
        valueParam.setStoredValue(preset.first);

    select.setSelectedId(presetBaseId + presetIndex, juce::dontSendNotification);
    select.setEditableText(false);

    if (updateText) {
        const juce::String textToUse = showTextForPresets ? preset.second : formatValue(preset.first, showUnits);
        select.setText(textToUse, juce::dontSendNotification);
    }
}

juce::String ComboBindingWithPresets::formatValue(double value, bool includeUnits) const {
    juce::String text;

    if (decimalPlaces.has_value()) {
        text = juce::String(value, decimalPlaces.value());
    } else {
        text = juce::String(value, 6);
        text = text.trimCharactersAtEnd("0");
        text = text.trimCharactersAtEnd(".");
        if (text.isEmpty())
            text = "0";
    }

    if (includeUnits && unitsText.isNotEmpty())
        text << " " << unitsText;
    return text;
}

juce::String ComboBindingWithPresets::removeUnits(juce::String text) const {
    if (unitsText.isEmpty())
        return text;

    auto trimmed = text.trim();
    if (trimmed.endsWithIgnoreCase(unitsText)) {
        trimmed = trimmed.dropLastCharacters(unitsText.length()).trimEnd();
    }
    return trimmed;
}

}  // namespace MoTool
