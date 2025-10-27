#include "ChipClockComboBinding.h"

#include <cmath>

namespace MoTool {

ChipClockComboBinding::ChipClockComboBinding(juce::ComboBox& comboBox, ParameterValue<double>& valueParameter)
    : select(comboBox)
    , valueParam(valueParameter)
    , unitsText(valueParameter.definition.units)
{
    select.addListener(this);
    valueParam.addListener(this);
}

ChipClockComboBinding::~ChipClockComboBinding() {
    select.removeListener(this);
    valueParam.removeListener(this);
}

void ChipClockComboBinding::configure() {
    select.setEditableText(true);
    select.setJustificationType(juce::Justification::centredLeft);
    select.setTooltip("Chip clock frequency");
    select.setTextWhenNothingSelected({});

    fillPresetItems();
    refreshFromParameters();
}

void ChipClockComboBinding::valueChanged(juce::Value& value) {
    if (value.refersToSameSourceAs(valueParam.getPropertyAsValue())) {
        valueParam.forceUpdateOfCachedValue();
        refreshFromParameters();
    }
}

void ChipClockComboBinding::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) {
    if (comboBoxThatHasChanged != &select || updating)
        return;

    if (select.getSelectedId() > 0)
        handleSelectionChange();
    else
        handleTextChange();
}

void ChipClockComboBinding::fillPresetItems() {
    juce::ScopedValueSetter<bool> guard(updating, true);
    select.clear(juce::dontSendNotification);

    presetValues.clear();

    select.addItem("Custom", customItemId);
    select.addSeparator();

    auto entries = ChipClockChoice::getClockEntries();

    int itemId = presetBaseId;
    for (int idx = 0; idx < static_cast<int>(ChipClockChoice::size()); ++idx) {
        auto choice = ChipClockChoice(static_cast<ChipClockChoice::Enum>(idx));
        if (choice == ChipClockChoice::Custom)
            continue;

        const double freqHz = entries[idx];
        const double freqMHz = freqHz / 1'000'000.0;

        juce::String labelText(choice.getLongLabel().data());
        select.addItem(labelText, itemId++);

        presetValues.push_back(freqMHz);
    }
}

void ChipClockComboBinding::refreshFromParameters() {
    if (updating)
        return;

    juce::ScopedValueSetter<bool> guard(updating, true);

    const double freqMHz = valueParam.getStoredValue();

    auto preset = findPresetForValue(freqMHz);

    select.setEditableText(preset == -1);
    select.setText(formatValue(freqMHz), juce::dontSendNotification);

    if (preset >= 0)
        setPresetSelection(preset, false);
}

void ChipClockComboBinding::handleSelectionChange() {
    const int selectedId = select.getSelectedId();
    if (selectedId == customItemId) {
        {
            juce::ScopedValueSetter<bool> guard(updating, true);
            select.setText(formatValue(valueParam.getStoredValue()), juce::dontSendNotification);
        }
        select.showEditor();
        return;
    }

    const int presetIndex = selectedId - presetBaseId;
    if (presetIndex < 0 || presetIndex >= static_cast<int>(presetValues.size()))
        return;

    const double freqMHz = presetValues[static_cast<size_t>(presetIndex)];

    valueParam.setStoredValue(freqMHz);

    juce::ScopedValueSetter<bool> guard(updating, true);
    select.setText(formatValue(freqMHz), juce::dontSendNotification);
}

void ChipClockComboBinding::handleTextChange() {
    const auto& range = valueParam.definition.valueRange;
    auto text = removeUnits(select.getText().trim());

    double parsed = text.getDoubleValue();
    if (text.isEmpty() || std::isnan(parsed))
        parsed = valueParam.definition.defaultValue;

    const double clamped = jlimit(range.start, range.end, parsed);
    if (std::abs(clamped - parsed) > 1e-9) {
        juce::ScopedValueSetter<bool> guard(updating, true);
        select.setText(formatValue(clamped), juce::dontSendNotification);
    }

    const int presetIndex = findPresetForValue(clamped);
    if (presetIndex >= 0) {
        setPresetSelection(presetIndex, false);
    }

    valueParam.setStoredValue(clamped);
}

int ChipClockComboBinding::findPresetForValue(double freqMHz) const {
    constexpr double tolerance = 1e-6;
    for (size_t i = 0; i < presetValues.size(); ++i) {
        if (std::abs(presetValues[i] - freqMHz) <= tolerance)
            return static_cast<int>(i);
    }
    return -1;
}

void ChipClockComboBinding::setPresetSelection(int presetIndex, bool updateText) {
    if (presetIndex < 0 || presetIndex >= static_cast<int>(presetValues.size()))
        return;

    juce::ScopedValueSetter<bool> guard(updating, true);
    select.setSelectedId(presetBaseId + presetIndex, juce::dontSendNotification);

    if (updateText) {
        select.setText(formatValue(presetValues[static_cast<size_t>(presetIndex)]), juce::dontSendNotification);
    }
}

juce::String ChipClockComboBinding::formatValue(double value, bool includeUnits) const {
    juce::String text(value, 6);
    text = text.trimCharactersAtEnd("0");
    text = text.trimCharactersAtEnd(".");
    if (text.isEmpty())
        text = "0";
    if (includeUnits && unitsText.isNotEmpty())
        text << " " << unitsText;
    return text;
}

juce::String ChipClockComboBinding::removeUnits(juce::String text) const {
    if (unitsText.isEmpty())
        return text;

    auto trimmed = text.trim();
    if (trimmed.endsWithIgnoreCase(unitsText)) {
        trimmed = trimmed.dropLastCharacters(unitsText.length()).trimEnd();
    }
    return trimmed;
}

}  // namespace MoTool
