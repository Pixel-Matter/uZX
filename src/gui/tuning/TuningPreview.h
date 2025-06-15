#pragma once

#include "JuceHeader.h"
#include "../../models/tuning/TuningSystem.h"
#include "../../viewmodels/tuning/TuningViewModel.h"

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
    static constexpr int cellHeight = 32;

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


class TuningPreviewComponent : public juce::Component, private ChangeListener, private ListBoxModel {
public:
    TuningPreviewComponent();
    ~TuningPreviewComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // ListBoxModel implementation
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const MouseEvent& e) override;

private:
    void changeListenerCallback(ChangeBroadcaster* source) override;

    TuningViewModel viewModel;

    // Tuning table selection
    Label tuningTableLabel;
    ListBox tuningTableListBox;

    Label ChipClockLabel;
    ComboBox ChipClockSelect;
    Slider a4FrequencySlider;
    Label a4FrequencyLabel;

    // Scale and Key selection
    Label KeyScaleLabel;
    ComboBox KeySelect;
    ComboBox ScaleSelect;

    Label TuningTypeLabel;
    Label TuningNameLabel;
    Label ToneEnvSwitchLabel;

    TuningPreviewGrid tuningGrid;
    TooltipWindow tooltipWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPreviewComponent)
};

}  // namespace MoTool