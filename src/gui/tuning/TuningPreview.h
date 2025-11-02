#pragma once

#include "JuceHeader.h"

#include "../../viewmodels/tuning/TuningViewModel.h"
#include "../../viewmodels/tuning/TuningPlayer.h"

#include "../common/ParamBindings.h"
#include "../common/ComboBindingWithPresets.h"
#include "../common/ComboBoxWithOverrideId.h"
#include "../common/MoTooltipWindow.h"

#include "TuningPreviewGrid.h"

#include <vector>

namespace MoTool {

//================================================================================
class TuningPreviewComponent : public juce::Component,
                               private ChangeListener,
                               private ListBoxModel,
                               private Value::Listener {
public:
    TuningPreviewComponent(TuningViewModel& vm, TuningPlayer& tp);
    ~TuningPreviewComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    std::unique_ptr<MoTooltipWindow> tooltipWindow;

    // ListBoxModel implementation
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const MouseEvent& e) override;

private:
    void changeListenerCallback(ChangeBroadcaster* source) override;

    // UI setup helpers
    void updateControlsState();
    // Setup helpers
    void setupTuningTableControls();
    void setupTuningGrid();
    void setupExportButton();
    void handleExportButtonClick();

    // Layout helpers
    void layoutControlSections(juce::Rectangle<int>& area);

    // Value::Listener implementation for ListBox sync
    void valueChanged(Value& value) override;

    TuningViewModel& viewModel;
    TuningPlayer& tuningPlayer;

    // Tuning table selection
    Label tuningTableLabel;
    ListBox tuningsListBox;

    // Chip clock controls
    struct ChipClock {
        ChipClock(TuningPreviewComponent& c, TuningViewModel& vm);
        void layout(juce::Rectangle<int>& area);
        int getHeight() const;

    private:
        Label label;
        ComboBoxWithOverrideId select;
        ComboBindingWithPresets comboBinding;
    };
    ChipClock chipClock {*this, viewModel};

    // A4 frequency controls
    struct A4Frequency {
        A4Frequency(TuningPreviewComponent& c, TuningViewModel& vm);
        void layout(juce::Rectangle<int>& area);
        int getHeight() const;

        Label label;
        Slider slider;
        Label unitsLabel;
        ParameterValue<double>& param;
        SliderParamEndpointBinding binding;
    };
    A4Frequency a4Frequency {*this, viewModel};

    // Reference Tuning selection
    struct ReferenceTuning {
        ReferenceTuning(TuningPreviewComponent& c, TuningViewModel& vm);
        void layout(juce::Rectangle<int>& area);
        int getHeight() const;

        Label label;
        ComboBox select;
        ComboBoxParamEndpointBinding binding;
    };
    ReferenceTuning tuning {*this, viewModel};

    // Scale and Key selection
    struct KeyScale {
        KeyScale(TuningPreviewComponent& c, TuningViewModel& vm);
        void layout(juce::Rectangle<int>& area);
        int getHeight() const;

        Label label;
        ComboBox keySelect;
        ComboBox scaleSelect;
        ComboBoxParamEndpointBinding keySelectBinding;

        struct ScaleSelectBinding : private Value::Listener {
            ScaleSelectBinding(ComboBox& comboBox, ParameterValue<Scale::ScaleType>& parameter);
            ~ScaleSelectBinding() override;

            void valueChanged(Value& value) override;

        private:
            void fillItems();
            void refreshFromSource();

            ComboBox& combo;
            ParameterValue<Scale::ScaleType>& parameterValue;
            bool updating { false };
            std::vector<Scale::ScaleType> itemMap;
        };

        ScaleSelectBinding scaleSelectBinding;
    };
    KeyScale keyScale {*this, viewModel};

    // Play controls
    struct PlayControls {
        PlayControls(TuningPreviewComponent& c, TuningViewModel& vm);
        void layout(juce::Rectangle<int>& area);

        Label label;
        ToggleButton playChordsCheckBox;
        ToggleButton playToneCheckBox;
        ToggleButton retriggerToneCheckBox;
        ToggleButton playEnvelopeCheckBox;
        ComboBox envelopeShapeSelect;
        ComboBox modulationModeSelect;
        ComboBoxParamEndpointBinding envelopeShapeBinding;
        ComboBoxParamEndpointBinding envelopeModeBinding;
    };
    PlayControls playControls {*this, viewModel};

    // Label tuningNameLabel;

    TextButton exportButton;

    TuningPreviewGrid tuningGrid;

    static constexpr int rowHeight = 28;
    static constexpr int moduleWidth = 60;
    static constexpr int gap = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPreviewComponent)
};

}  // namespace MoTool
