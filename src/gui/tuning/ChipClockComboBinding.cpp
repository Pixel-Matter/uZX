#include "ChipClockComboBinding.h"

#include <cmath>

namespace MoTool {

ChipClockComboBinding::ChipClockComboBinding(juce::ComboBox& comboBox,
                                             ParameterValue<ChipClockChoice>& chipParameter,
                                             ParameterValue<double>& clockParameter)
    : select(comboBox)
    , chipParam(chipParameter)
    , clockParam(clockParameter)
    , unitsText(clockParameter.definition.units)
{
    select.addListener(this);
    chipParam.addListener(this);
    clockParam.addListener(this);
}

ChipClockComboBinding::~ChipClockComboBinding() {
    select.removeListener(this);
    chipParam.removeListener(this);
    clockParam.removeListener(this);
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
    if (value.refersToSameSourceAs(clockParam.getPropertyAsValue())) {
        clockParam.forceUpdateOfCachedValue();
        refreshFromParameters();
    } else if (value.refersToSameSourceAs(chipParam.getPropertyAsValue())) {
        chipParam.forceUpdateOfCachedValue();
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

    presetChoices.clear();
    presetValuesMHz.clear();

    select.addItem("Custom", customItemId);
    select.addSeparator();

    auto entries = ChipClockChoice::getClockEntries();
    const int totalChoices = static_cast<int>(ChipClockChoice::size());

    int itemId = presetBaseId;
    for (int idx = 0; idx < totalChoices; ++idx) {
        auto choice = ChipClockChoice(static_cast<ChipClockChoice::Enum>(idx));
        if (choice == ChipClockChoice::Custom)
            continue;

        const double freqHz = entries[idx];
        const double freqMHz = freqHz / 1'000'000.0;

        juce::String labelText(choice.getLongLabel().data());
        select.addItem(labelText, itemId++);

        presetChoices.push_back(choice);
        presetValuesMHz.push_back(freqMHz);
    }
}

void ChipClockComboBinding::refreshFromParameters() {
    if (updating)
        return;

    juce::ScopedValueSetter<bool> guard(updating, true);

    const auto chipChoice = chipParam.getStoredValue();
    const double freqMHz = clockParam.getStoredValue();

    const bool shouldAllowEditing = (chipChoice == ChipClockChoice::Custom);
    if (select.isTextEditable() != shouldAllowEditing)
        select.setEditableText(shouldAllowEditing);

    select.setText(formatFrequency(freqMHz), juce::dontSendNotification);

    if (chipChoice != ChipClockChoice::Custom) {
        if (const int preset = findPresetForChoice(chipChoice); preset >= 0) {
            setPresetSelection(preset, false);
            return;
        }
    }

    if (const int preset = findPresetForValue(freqMHz); preset >= 0)
        setPresetSelection(preset, false);
}

void ChipClockComboBinding::handleSelectionChange() {
    const int selectedId = select.getSelectedId();
    if (selectedId == customItemId) {
        chipParam.setStoredValue(ChipClockChoice::Custom);
        {
            juce::ScopedValueSetter<bool> guard(updating, true);
            select.setText(formatFrequency(clockParam.getStoredValue()), juce::dontSendNotification);
        }
        select.showEditor();
        return;
    }

    const int presetIndex = selectedId - presetBaseId;
    if (presetIndex < 0 || presetIndex >= static_cast<int>(presetChoices.size()))
        return;

    const auto choice = presetChoices[static_cast<size_t>(presetIndex)];
    const double freqMHz = presetValuesMHz[static_cast<size_t>(presetIndex)];

    chipParam.setStoredValue(choice);
    clockParam.setStoredValue(freqMHz);

    juce::ScopedValueSetter<bool> guard(updating, true);
    select.setText(formatFrequency(freqMHz), juce::dontSendNotification);
}

void ChipClockComboBinding::handleTextChange() {
    const auto& range = clockParam.definition.valueRange;
    auto text = removeUnits(select.getText().trim());

    double parsed = text.getDoubleValue();
    if (text.isEmpty() || std::isnan(parsed))
        parsed = clockParam.definition.defaultValue;

    const double clamped = jlimit(range.start, range.end, parsed);
    if (std::abs(clamped - parsed) > 1e-9) {
        juce::ScopedValueSetter<bool> guard(updating, true);
        select.setText(formatFrequency(clamped), juce::dontSendNotification);
    }

    const int presetIndex = findPresetForValue(clamped);
    if (presetIndex >= 0) {
        chipParam.setStoredValue(presetChoices[static_cast<size_t>(presetIndex)]);
        setPresetSelection(presetIndex, false);
    } else {
        chipParam.setStoredValue(ChipClockChoice::Custom);
    }

    clockParam.setStoredValue(clamped);
}

int ChipClockComboBinding::findPresetForValue(double freqMHz) const {
    constexpr double tolerance = 1e-3;
    for (size_t i = 0; i < presetValuesMHz.size(); ++i) {
        if (std::abs(presetValuesMHz[i] - freqMHz) <= tolerance)
            return static_cast<int>(i);
    }
    return -1;
}

int ChipClockComboBinding::findPresetForChoice(ChipClockChoice choice) const {
    for (size_t i = 0; i < presetChoices.size(); ++i) {
        if (presetChoices[i] == choice)
            return static_cast<int>(i);
    }
    return -1;
}

void ChipClockComboBinding::setPresetSelection(int presetIndex, bool updateText) {
    if (presetIndex < 0 || presetIndex >= static_cast<int>(presetChoices.size()))
        return;

    juce::ScopedValueSetter<bool> guard(updating, true);
    select.setSelectedId(presetBaseId + presetIndex, juce::dontSendNotification);

    if (updateText) {
        select.setText(formatFrequency(presetValuesMHz[static_cast<size_t>(presetIndex)]), juce::dontSendNotification);
    }
}

juce::String ChipClockComboBinding::formatFrequency(double freqMHz, bool includeUnits) const {
    juce::String text(freqMHz, 6);
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
