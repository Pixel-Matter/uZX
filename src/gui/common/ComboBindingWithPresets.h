#pragma once

#include "JuceHeader.h"

#include "../common/ParamBindings.h"
#include "../../plugins/uZX/aychip/aychip.h"

#include <vector>
#include <optional>

namespace MoTool {

class ComboBindingWithPresets : private juce::Value::Listener,
                              private juce::ComboBox::Listener {
public:
    ComboBindingWithPresets(juce::ComboBox& comboBox,
                            ParameterValue<double>& valueParameter,
                            const std::vector<std::pair<double, String>>& presetItems,
                            bool showPresetLabels = true,
                            bool showUnits = true);
    ~ComboBindingWithPresets() override;

    void configure();
    void setDecimalPlaces(int digits);

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
    void applyPreset(int presetIndex, bool updateParameter, bool updateText);
    juce::String formatValue(double value, bool includeUnits = true) const;
    juce::String removeUnits(juce::String text) const;

    juce::ComboBox& select;
    ParameterValue<double>& valueParam;
    std::vector<std::pair<double, String>> presets;
    juce::String unitsText;
    bool showTextForPresets;
    bool showUnits;
    std::optional<int> decimalPlaces;

    bool updating = false;
};

}  // namespace MoTool
