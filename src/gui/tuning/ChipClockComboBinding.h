#pragma once

#include "JuceHeader.h"

#include "../common/ParamBindings.h"
#include "../../plugins/uZX/aychip/aychip.h"

#include <vector>

namespace MoTool {

class ChipClockComboBinding : private juce::Value::Listener,
                              private juce::ComboBox::Listener {
public:
    ChipClockComboBinding(juce::ComboBox& comboBox,
                          ParameterValue<ChipClockChoice>& chipParameter,
                          ParameterValue<double>& clockParameter);
    ~ChipClockComboBinding() override;

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
    int findPresetForChoice(ChipClockChoice choice) const;
    void setPresetSelection(int presetIndex, bool updateText);
    juce::String formatFrequency(double freqMHz, bool includeUnits = true) const;
    juce::String removeUnits(juce::String text) const;

    juce::ComboBox& select;
    ParameterValue<ChipClockChoice>& chipParam;
    ParameterValue<double>& clockParam;
    juce::String unitsText;

    std::vector<ChipClockChoice> presetChoices;
    std::vector<double> presetValuesMHz;
    bool updating = false;
};

}  // namespace MoTool
