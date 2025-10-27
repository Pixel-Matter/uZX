#include "ChipClockComboBinding.h"

#include <cmath>
#include "juce_core/juce_core.h"

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
    select.setJustificationType(juce::Justification::centredLeft);
    select.setTooltip(valueParam.definition.description);
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

    if (select.getSelectedId() > 0) {
        DBG("ComboBox selection changed: ID=" << select.getSelectedId());
        handleSelectionChange();
    } else {
        DBG("ComboBox text changed: Text=\"" << select.getText() << "\"");
        handleTextChange();
    }
}

static std::vector<std::pair<double, String>> makeChipClockPresets() {
    std::vector<std::pair<double, String>> choices;
    choices.reserve(static_cast<size_t>(ChipClockChoice::size()) - 1);

    auto entries = ChipClockChoice::getClockEntries();
    for (int idx = 0; idx < static_cast<int>(ChipClockChoice::size()); ++idx) {
        auto choice = ChipClockChoice(idx);
        if (entries[idx] != 0)
            choices.emplace_back(entries[idx] / 1'000'000.0, choice.getLongLabel().data());
    }
    return choices;
}

void ChipClockComboBinding::fillPresetItems() {
    auto presets = makeChipClockPresets();
    juce::ScopedValueSetter<bool> guard(updating, true);

    select.clear(juce::dontSendNotification);
    select.addItem("Custom", customItemId);
    select.addSeparator();
    presetValues.clear();

    int itemId = presetBaseId;
    for (auto [value, label] : presets) {
        select.addItem(label, itemId++);
        presetValues.push_back(value);
    }
}

void ChipClockComboBinding::refreshFromParameters() {
    if (updating)
        return;

    juce::ScopedValueSetter<bool> guard(updating, true);

    const double value = valueParam.getStoredValue();

    auto preset = findPresetForValue(value);
    DBG("Refreshing ComboBox from parameter: Value=" << value << ", Preset=" << preset);
    if (preset >= 0) {
        DBG(" - matched preset index " << preset);
        setPresetSelection(preset, !showTextForPresets);
    } else {
        DBG(" - custom value");
        select.setSelectedId(0, juce::dontSendNotification);
        select.setText(formatValue(value), juce::dontSendNotification);
        select.setEditableText(true);
    }
}

void ChipClockComboBinding::handleSelectionChange() {
    DBG("Handling ComboBox selection change");
    const int selectedId = select.getSelectedId();
    if (selectedId == customItemId) {
        juce::ScopedValueSetter<bool> guard(updating, true);
        select.setText(formatValue(valueParam.getStoredValue()), juce::dontSendNotification);
        select.setEditableText(true);
        select.showEditor();
        return;
    }

    const int presetIndex = selectedId - presetBaseId;
    if (presetIndex < 0 || presetIndex >= static_cast<int>(presetValues.size()))
        return;

    const double value = presetValues[static_cast<size_t>(presetIndex)];

    valueParam.setStoredValue(value);
    DBG(" - setting text to " << formatValue(value));
    juce::ScopedValueSetter<bool> guard(updating, true);
    select.setText(formatValue(value), juce::dontSendNotification);
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
    select.setEditableText(false);

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
