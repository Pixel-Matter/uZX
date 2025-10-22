#pragma once

#include "JuceHeader.h"

#include "../../viewmodels/tuning/TuningViewModel.h"
#include "../../viewmodels/tuning/TuningPlayer.h"

#include "../common/MoTooltipWindow.h"

namespace MoTool {

class TuningPreviewComponent;

class TuningPreviewGrid : public juce::Component,
                          public juce::TooltipClient,
                          private TuningPlayer::Listener {
public:
    TuningPreviewGrid(TuningViewModel& vm, TuningPlayer& tp);
    ~TuningPreviewGrid() override;

    void recreateTooltipWindow();

    MoTooltipWindow* tooltipWindow = nullptr;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;

    // TooltipClient implementation
    juce::String getTooltip() override;

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

    // Mouse tracking for tooltips and hover effects
    juce::Point<int> lastMousePosition;
    GridHitResult hoveredRegion;

    void playingNotesChanges() override;

    // Paint helper methods
    void paintColumnHeader(juce::Graphics& g, const juce::Rectangle<int>& bounds, const TuningNoteName& column, NoteGridHeadingType headingType);
    void paintRowHeader(juce::Graphics& g, const juce::Rectangle<int>& bounds, int octave, bool isHovered = false);
    void paintNoteCell(juce::Graphics& g, const juce::Rectangle<int>& bounds, const TuningNote& note, bool isHovered = false);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPreviewGrid)
};

} // namespace MoTool
