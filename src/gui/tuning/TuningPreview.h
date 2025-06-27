#pragma once

#include "JuceHeader.h"
#include "../../models/tuning/TuningSystemBase.h"
#include "../../viewmodels/tuning/TuningViewModel.h"
#include "../../viewmodels/tuning/TuningPlayer.h"
#include "../../controllers/App.h"
#include <map>

namespace MoTool {

class TuningPreviewGrid : public juce::Component, public TooltipClient {
public:
    TuningPreviewGrid(TuningViewModel& vm);
    ~TuningPreviewGrid() override;

    void setTooltipWindow(TooltipWindow* window) { tooltipWindow = window; }

    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseMove(const MouseEvent& event) override;

    // TooltipClient implementation
    String getTooltip() override;

private:
    static constexpr int cellWidth = 56;
    static constexpr int firstCellWidth = cellWidth / 2;
    static constexpr int cellHeight = 32;
    static constexpr int headerRowHeight = 24;
    static constexpr int gridYOffset = static_cast<int>(headerRowHeight * 3.5);

    TuningViewModel& viewModel;

    // Mouse tracking for tooltips
    Point<int> lastMousePosition;
    TuningNote hoveredNote;
    bool hasHoveredNote = false;

    // Helper method to find note at mouse position
    bool findNoteAtPosition(Point<int> position, TuningNote& outNote) const;

    TooltipWindow* tooltipWindow = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPreviewGrid)
};


class TuningPreviewComponent : public juce::Component, private ChangeListener, private ListBoxModel, private Value::Listener {
public:
    TuningPreviewComponent(UndoManager* um = nullptr);
    ~TuningPreviewComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // ListBoxModel implementation
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const MouseEvent& e) override;

private:
    void changeListenerCallback(ChangeBroadcaster* source) override;

    // UI setup helpers
    void setupSliderWithValueBinding(Slider& slider, Label& label, const String& labelText, Label& unitsLabel,
                                     RangedParamAttachment<double>& attachment);
    void updateClockControlsState();
    void setupScaleSelectMenu();
    void updateScaleSelection();

    // Value::Listener implementation for ListBox sync
    void valueChanged(Value& value) override;

    TuningViewModel viewModel;
    TuningPlayer tuningPlayer;

    // Tuning table selection
    Label tuningTableLabel;
    ListBox tuningsListBox;

    Label chipClockLabel;
    ComboBox chipClockSelect;

    Slider clockFrequencySlider;
    Label clockFrequencyLabel;
    Label clockFrequencyUnits;

    Slider a4FrequencySlider;
    Label a4FrequencyLabel;
    Label a4FrequencyUnits;

    // Scale and Key selection
    Label keyScaleLabel;
    ComboBox keySelect;
    ComboBox scaleSelect;

    Label tuningTypeLabel;
    Label tuningNameLabel;
    Label toneEnvSwitchLabel;

    TextButton exportButton;

    TuningPreviewGrid tuningGrid;
    TooltipWindow tooltipWindow;

    // bindings
    ComboBoxBinding<ChipClockChoice> chipClockBinding { chipClockSelect, viewModel.selectedChip };
    ComboBoxBinding<Scale::Key> keySelectBinding { keySelect, viewModel.selectedTonic };
    ComboBoxBinding<Scale::ScaleType> scaleSelectBinding { scaleSelect, viewModel.selectedScale };

    static constexpr int rowHeight = 28;
    static constexpr int moduleWidth = 60;
    static constexpr int gap = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPreviewComponent)
};

}  // namespace MoTool