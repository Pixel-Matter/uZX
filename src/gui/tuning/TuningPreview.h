#pragma once

#include "JuceHeader.h"
#include "../../models/tuning/TuningSystemBase.h"
#include "../../viewmodels/tuning/TuningViewModel.h"
#include "../../viewmodels/tuning/TuningPlayer.h"
#include "../../controllers/App.h"
#include <map>

namespace MoTool {

class TuningPreviewGrid : public juce::Component,
                          public TooltipClient,
                          private TuningPlayer::Listener {
public:
    TuningPreviewGrid(TuningViewModel& vm, TuningPlayer& tp);
    ~TuningPreviewGrid() override;

    void setTooltipWindow(TooltipWindow* window) { tooltipWindow = window; }

    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseMove(const MouseEvent& event) override;
    void mouseDown(const MouseEvent& event) override;

    // TooltipClient implementation
    String getTooltip() override;

private:
    static constexpr int cellWidth = 56;
    static constexpr int rowHeaderWidth = cellWidth / 2;
    static constexpr int cellHeight = 32;
    static constexpr int headerRowHeight = 24;
    static constexpr int gridYOffset = static_cast<int>(headerRowHeight * 3.5);

    enum class GridRegionType {
        None,
        NoteCell,
        RowHeader,
        ColumnHeader
    };

    struct GridHitResult {
        GridRegionType regionType = GridRegionType::None;
        int octave = -1;                     // For note cells and row headers
        int noteIndex = -1;                  // For note cells and column headers
        NoteGridHeadingType headerType = NoteGridHeadingType::Tuning; // For column headers
        juce::Rectangle<int> bounds;         // Bounds of the hit region

        bool isValid() const { return regionType != GridRegionType::None; }
    };

    struct GridLayout {
        juce::Rectangle<int> totalBounds;
        juce::Rectangle<int> gridBounds;
        const TuningViewModel& viewModel;

        GridLayout(juce::Rectangle<int> bounds, const TuningViewModel& vm)
            : totalBounds(bounds), viewModel(vm) {
            gridBounds = bounds;
            gridBounds.removeFromTop(headerRowHeight / 2);
            gridBounds.setWidth(cellWidth * (viewModel.getNumColumns() + 1));
        }

        GridHitResult hitTest(juce::Point<int> position) const;
        juce::Rectangle<int> getColumnHeaderBounds(int headerTypeIndex, int noteIndex) const;
        juce::Rectangle<int> getRowHeaderBounds(int octave) const;
        juce::Rectangle<int> getNoteCellBounds(int octave, int noteIndex) const;
        int getHeaderRowsHeight() const;
    };

    TuningViewModel& viewModel;
    TuningPlayer& tuningPlayer;

    // Mouse tracking for tooltips
    Point<int> lastMousePosition;
    GridHitResult hoveredRegion;

    void playingNotesChanges() override;

    // Paint helper methods
    void paintColumnHeader(juce::Graphics& g, const juce::Rectangle<int>& bounds, const TuningNoteName& column, NoteGridHeadingType headingType);
    void paintRowHeader(juce::Graphics& g, const juce::Rectangle<int>& bounds, int octave);
    void paintNoteCell(juce::Graphics& g, const juce::Rectangle<int>& bounds, const TuningNote& note);

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
    ComboBoxBinding<Scale::Key> keySelectBinding { keySelect, viewModel.selectedRoot };
    ComboBoxBinding<Scale::ScaleType> scaleSelectBinding { scaleSelect, viewModel.selectedScale };

    static constexpr int rowHeight = 28;
    static constexpr int moduleWidth = 60;
    static constexpr int gap = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPreviewComponent)
};

}  // namespace MoTool