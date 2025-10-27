#pragma once

#include "JuceHeader.h"

#include "../common/ParamBindings.h"
#include "../../plugins/uZX/aychip/aychip.h"

#include <vector>

namespace MoTool {

class ComboBindingWithPresets : private juce::Value::Listener,
                              private juce::ComboBox::Listener {
public:
    ComboBindingWithPresets(juce::ComboBox& comboBox, ParameterValue<double>& valueParameter, const std::vector<std::pair<double, String>>& presetItems);
    ~ComboBindingWithPresets() override;

    void configure();

private:
    static constexpr int customItemId = 1;
    static constexpr int presetBaseId = 100;

    void valueChanged(juce::Value& value) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    void fillPresetItems();
    void refreshFromParameters();
    void handleSelectionChange();
    void handleTextChange();

    int findPresetForValue(double freqMHz) const;
    void setPresetSelection(int presetIndex, bool updateText);
    juce::String formatValue(double value, bool includeUnits = true) const;
    juce::String removeUnits(juce::String text) const;

    juce::ComboBox& select;
    ParameterValue<double>& valueParam;
    std::vector<std::pair<double, String>> presets;
    std::vector<double> presetValues;
    juce::String unitsText;
    bool showTextForPresets = true;

    bool updating = false;
};

}  // namespace MoTool
