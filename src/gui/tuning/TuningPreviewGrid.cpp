#include "TuningPreviewGrid.h"

#include "TuningPreview.h"
#include "../common/LookAndFeel.h"

#include <cmath>
#include <memory>

namespace MoTool {

//================================================================================
TuningPreviewGrid::TuningPreviewGrid(TuningViewModel& vm, TuningPlayer& tp)
    : viewModel(vm), tuningPlayer(tp)
{
    setOpaque(true);
    tuningPlayer.addListener(this);
}

TuningPreviewGrid::~TuningPreviewGrid() {
    tuningPlayer.removeListener(this);
}

void TuningPreviewGrid::recreateTooltipWindow() {
    // Find the parent component to attach tooltip to
    auto* parent = getParentComponent();
    while (parent && !dynamic_cast<TuningPreviewComponent*>(parent)) {
        parent = parent->getParentComponent();
    }

    if (auto* tuningComponent = dynamic_cast<TuningPreviewComponent*>(parent)) {
        // Reset the tooltip window to restart the show counter
        tuningComponent->tooltipWindow = std::make_unique<MoTooltipWindow>(nullptr, 750);
        tooltipWindow = tuningComponent->tooltipWindow.get();
    }
}

void TuningPreviewGrid::resized() {
    // Resize logic if needed
}

void TuningPreviewGrid::mouseMove(const juce::MouseEvent& event) {
    lastMousePosition = event.getPosition();

    GridLayout layout(getLocalBounds(), viewModel);
    GridHitResult newHover = layout.hitTest(lastMousePosition);

    // Check if we're hovering over a different region
    bool regionChanged = (newHover.regionType != hoveredRegion.regionType ||
                         newHover.octave != hoveredRegion.octave ||
                         newHover.noteIndex != hoveredRegion.noteIndex ||
                         newHover.headerType != hoveredRegion.headerType);

    if (regionChanged) {
        // Recreate tooltip window to reset the show counter
        recreateTooltipWindow();

        hoveredRegion = newHover;
        repaint();  // to update hover effects
    }
}

void TuningPreviewGrid::paintColumnHeader(juce::Graphics& g, const juce::Rectangle<int>& bounds, const TuningNoteName& column, NoteGridHeadingType headingType) {
    // Draw center tick for Degrees and Notes headers
    if (headingType == NoteGridHeadingType::Degrees || headingType == NoteGridHeadingType::Notes) {
        g.setColour(Colors::Theme::surfaceAlt);
        const int cellCenter = bounds.getX() + (bounds.getWidth() - 2) / 2;
        g.fillRect(cellCenter, bounds.getY() - 2, 2, 4);
        g.fillRect(cellCenter, bounds.getBottom() - 2, 2, 4);
    }

    // Get text based on heading type
    juce::String text = column.getHeadingText(headingType);

    // Set text color
    auto color = column.isInScale ? Colors::Theme::textPrimary : Colors::Theme::textPrimary.withAlpha(0.5f);
    if (column.isRootNote) {
        color = Colors::Theme::primary.interpolatedWith(Colors::Theme::textPrimary, 0.5f);
    }
    g.setColour(color);
    g.drawText(text, bounds, juce::Justification::centred, true);
}

void TuningPreviewGrid::paintRowHeader(juce::Graphics& g, const juce::Rectangle<int>& bounds, int octave, bool isHovered) {
    // Add hover background for clickable row headers
    if (isHovered) {
        g.setColour(Colors::Theme::surface);
        g.fillRect(bounds.reduced(1, 1));
    }

    g.setColour(Colors::Theme::textPrimary);
    g.drawText(
        juce::String::formatted("%d", octave),
        bounds.withTrimmedRight(8),
        juce::Justification::right, true
    );
}

void TuningPreviewGrid::paintNoteCell(juce::Graphics& g, const juce::Rectangle<int>& bounds, const TuningNote& note, bool isHovered) {
    float offtune = juce::jlimit(-0.5f, 0.5f, (float) note.offtune / 100.0f);

    auto noteTextColor = Colors::Theme::textPrimary;
    if (offtune > 0.05f) {
        noteTextColor = noteTextColor.interpolatedWith(juce::Colours::blue, offtune * 2);
    } else if (offtune < 0.05f) {
        noteTextColor = noteTextColor.interpolatedWith(juce::Colours::red, -offtune * 2);
    }

    // Check if this note is currently playing
    bool isPlaying = note.isInMidiRange() && tuningPlayer.isNotePlaying(note.midiNote);

    auto noteBgColor = note.isInMidiRange() ? Colors::Theme::background : Colors::Theme::background.withAlpha(0.33f);

    // Add hover effect for clickable cells (those in MIDI range)
    if (isHovered && note.isInMidiRange()) {
        noteBgColor = Colors::Theme::surface;
    }

    // Use pressed button color for currently playing notes (overrides hover)
    if (isPlaying) {
        noteBgColor = Colors::Theme::primary;
        noteTextColor = Colors::Theme::background; // Use contrasting text color
    }

    if (note.isInScale) {
        g.setColour(noteBgColor);
        g.fillRect(bounds.reduced(2, 2));
    } else {
        noteTextColor = noteTextColor.withAlpha(0.5f);
        g.setColour(noteBgColor.withAlpha(noteBgColor.getFloatAlpha() * 0.5f));
        g.fillRect(bounds.reduced(2, 2));
    }

    // Inter-cell reference tuning ticks at the horizontal center of the cell
    g.setColour(Colors::Theme::surfaceAlt);
    const int cellCenter = bounds.getX() + (bounds.getWidth() - 2) / 2;
    g.fillRect(cellCenter, bounds.getY() - 2, 2, 4);
    g.fillRect(cellCenter, bounds.getBottom() - 2, 2, 4);

    // In-cell tuning ticks and offtune tick
    auto tickSize = 6;
    auto calcTickX = [&](const float tick) -> float {
        return juce::jmap<float>(tick, -0.5f, 0.5f, (float) bounds.getX(), (float) bounds.getRight() - 2.0f);
    };
    auto drawTick = [&](const float tickX) -> void {
        g.fillRect(tickX, (float) bounds.getY() + 2, 2.0f, (float) tickSize);
        g.fillRect(tickX, (float) bounds.getBottom() - 2.0f - (float) tickSize, 2.0f, (float) tickSize);
    };

    float offX = calcTickX(offtune);
    if (note.offtune >= -50 && note.offtune <= 50) {
        drawTick(offX);
    }

    // Drawing text
    auto textCell = bounds;
    auto text = juce::String::formatted("%d", note.period);
    int textWidth = static_cast<int>(std::ceil(juce::TextLayout::getStringWidth(g.getCurrentFont(), text)));
    textCell.setWidth(textWidth);
    textCell.setCentre((int) offX, (int) textCell.getCentreY());
    textCell = textCell.constrainedWithin(bounds.reduced(2, 0));

    g.setColour(noteTextColor);
    g.drawText(text, textCell, juce::Justification::centred, false);

    // Safe for envelope marker
    if (!viewModel.isEnvelopePeriodsShown() && note.isSafeForEnvelope()) {
        g.setColour(Colors::Theme::success);
        g.fillRect(bounds.getRight() - 6, bounds.getY() + 2, 4, 4);
    }
}

void TuningPreviewGrid::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);

    GridLayout layout(getLocalBounds(), viewModel);
    const auto columns = viewModel.getColumnNoteNames();
    const auto headerTypes = TuningViewModel::getHeaderTypes();
    const auto octaveRange = viewModel.getOctaveRange();

    // Draw column headers
    for (size_t headerTypeIndex = 0; headerTypeIndex < headerTypes.size(); ++headerTypeIndex) {
        auto headerType = headerTypes[headerTypeIndex];
        for (int noteIndex = 0; noteIndex < viewModel.getNumColumns(); ++noteIndex) {
            auto headerBounds = layout.getColumnHeaderBounds(static_cast<int>(headerTypeIndex), noteIndex);
            auto& column = columns[static_cast<size_t>(noteIndex)];
            paintColumnHeader(g, headerBounds, column, headerType);
        }
    }

    // Draw rows (row headers + note cells)
    for (auto octave = octaveRange.getStart(); octave < octaveRange.getEnd(); ++octave) {
        // Check if this row header is hovered
        bool rowHeaderHovered = (hoveredRegion.regionType == GridRegionType::RowHeader &&
                                hoveredRegion.octave == octave);

        // Draw row header
        auto rowHeaderBounds = layout.getRowHeaderBounds(octave);
        paintRowHeader(g, rowHeaderBounds, octave, rowHeaderHovered);

        // Draw note cells
        auto octaveNotes = viewModel.getOctaveNotes(octave);
        for (int noteIndex = 0; noteIndex < static_cast<int>(octaveNotes.size()); ++noteIndex) {
            // Check if this note cell is hovered
            bool noteCellHovered = (hoveredRegion.regionType == GridRegionType::NoteCell &&
                                   hoveredRegion.octave == octave &&
                                   hoveredRegion.noteIndex == noteIndex);

            auto cellBounds = layout.getNoteCellBounds(octave, noteIndex);
            paintNoteCell(g, cellBounds, octaveNotes[static_cast<size_t>(noteIndex)], noteCellHovered);
        }
    }

    // Draw vertical line at the root note column (current root)
    const int rootColumnIndex = static_cast<int>(viewModel.getCurrentTonic());
    auto firstCellBounds = layout.getNoteCellBounds(octaveRange.getStart(), rootColumnIndex);
    auto lastCellBounds = layout.getNoteCellBounds(octaveRange.getEnd() - 1, rootColumnIndex);

    int lineX = firstCellBounds.getX();
    int lineY = layout.gridBounds.getY();
    int lineHeight = lastCellBounds.getBottom() - lineY;

    g.setColour(Colors::Theme::primary.withAlpha(0.5f));
    g.fillRect(lineX, lineY, 2, lineHeight);
}

juce::String TuningPreviewGrid::getTooltip() {
    if (!hoveredRegion.isValid()) {
        return "";
    }

    switch (hoveredRegion.regionType) {
        case GridRegionType::NoteCell: {
            // Get the note data and return its tooltip
            auto octaveNotes = viewModel.getOctaveNotes(hoveredRegion.octave);
            if (hoveredRegion.noteIndex < static_cast<int>(octaveNotes.size())) {
                return octaveNotes[static_cast<size_t>(hoveredRegion.noteIndex)].getTooltip();
            }
            break;
        }
        case GridRegionType::RowHeader:
            return "Click to play the scale";

        case GridRegionType::ColumnHeader: {
            // TODO display tuning for degree in cents from the root note
            // use column.getHeadingTooltip(hoveredRegion.headerType);

            auto columns = viewModel.getColumnNoteNames();
            if (hoveredRegion.noteIndex < static_cast<int>(columns.size())) {
                auto& column = columns[static_cast<size_t>(hoveredRegion.noteIndex)];
                juce::String headerTypeName;
                switch (hoveredRegion.headerType) {
                    case NoteGridHeadingType::Tuning: headerTypeName = "Tuning"; break;
                    case NoteGridHeadingType::Degrees: headerTypeName = "Degree"; break;
                    case NoteGridHeadingType::Notes: headerTypeName = "Note"; break;
                }
                juce::String headerText = column.getHeadingText(hoveredRegion.headerType);
                return headerTypeName + ": " + headerText;
            }
            break;
        }
        case GridRegionType::None:
            break;
    }

    return "";
}

void TuningPreviewGrid::mouseDown(const juce::MouseEvent& event) {
    GridLayout layout(getLocalBounds(), viewModel);
    GridHitResult hitResult = layout.hitTest(event.getPosition());

    if (hitResult.regionType == GridRegionType::NoteCell) {
        auto octaveNotes = viewModel.getOctaveNotes(hitResult.octave);
        auto& note = octaveNotes[static_cast<size_t>(hitResult.noteIndex)];
        if (note.isInMidiRange()) {
            if (viewModel.selectedParams.playChords.getStoredValue()) {
                tuningPlayer.playDegreeChord(note.midiNote);
            } else {
                tuningPlayer.playSingleNote(note.midiNote);
            }
        }
    } else if (hitResult.regionType == GridRegionType::RowHeader) {
        tuningPlayer.playScale(hitResult.octave);
    }
    // Could add functionality for clicking on column headers here
}

void TuningPreviewGrid::playingNotesChanges() {
    repaint();
}

//================================================================================
// GridLayout Implementation

int TuningPreviewGrid::GridLayout::getHeaderRowsHeight() const {
    return headerRowHeight * static_cast<int>(TuningViewModel::getHeaderTypes().size());
}

juce::Rectangle<int> TuningPreviewGrid::GridLayout::getColumnHeaderBounds(int headerTypeIndex, int noteIndex) const {
    int x = gridBounds.getX() + rowHeaderWidth + noteIndex * cellWidth;
    int y = gridBounds.getY() + headerTypeIndex * headerRowHeight;
    return juce::Rectangle<int>(x, y, cellWidth, headerRowHeight);
}

juce::Rectangle<int> TuningPreviewGrid::GridLayout::getRowHeaderBounds(int octave) const {
    auto octaveRange = viewModel.getOctaveRange();
    int rowIndex = octave - octaveRange.getStart();
    int y = gridBounds.getY() + getHeaderRowsHeight() + rowIndex * cellHeight;
    return juce::Rectangle<int>(gridBounds.getX(), y, rowHeaderWidth, cellHeight);
}

juce::Rectangle<int> TuningPreviewGrid::GridLayout::getNoteCellBounds(int octave, int noteIndex) const {
    auto octaveRange = viewModel.getOctaveRange();
    int rowIndex = octave - octaveRange.getStart();
    int x = gridBounds.getX() + rowHeaderWidth + noteIndex * cellWidth;
    int y = gridBounds.getY() + getHeaderRowsHeight() + rowIndex * cellHeight;
    return juce::Rectangle<int>(x, y, cellWidth, cellHeight);
}

TuningPreviewGrid::GridHitResult TuningPreviewGrid::GridLayout::hitTest(juce::Point<int> position) const {
    GridHitResult result;

    auto octaveRange = viewModel.getOctaveRange();
    auto headerTypes = TuningViewModel::getHeaderTypes();
    int headerRowsHeight = getHeaderRowsHeight();

    // Test column headers
    if (position.y >= gridBounds.getY() && position.y < gridBounds.getY() + headerRowsHeight) {
        int headerTypeIndex = (position.y - gridBounds.getY()) / headerRowHeight;
        if (headerTypeIndex >= 0 && headerTypeIndex < static_cast<int>(headerTypes.size())) {
            int notesStartX = gridBounds.getX() + rowHeaderWidth;
            if (position.x >= notesStartX) {
                int noteIndex = (position.x - notesStartX) / cellWidth;
                if (noteIndex >= 0 && noteIndex < viewModel.getNumColumns()) {
                    result.regionType = GridRegionType::ColumnHeader;
                    result.noteIndex = noteIndex;
                    result.headerType = headerTypes[static_cast<size_t>(headerTypeIndex)];
                    result.bounds = getColumnHeaderBounds(headerTypeIndex, noteIndex);
                    return result;
                }
            }
        }
    }

    // Test row headers and note cells
    int rowsStartY = gridBounds.getY() + headerRowsHeight;
    if (position.y >= rowsStartY) {
        int rowIndex = (position.y - rowsStartY) / cellHeight;
        int octave = octaveRange.getStart() + rowIndex;

        if (octave >= octaveRange.getStart() && octave < octaveRange.getEnd()) {
            // Test row header
            if (position.x >= gridBounds.getX() && position.x < gridBounds.getX() + rowHeaderWidth) {
                result.regionType = GridRegionType::RowHeader;
                result.octave = octave;
                result.bounds = getRowHeaderBounds(octave);
                return result;
            }

            // Test note cells
            int notesStartX = gridBounds.getX() + rowHeaderWidth;
            if (position.x >= notesStartX) {
                int noteIndex = (position.x - notesStartX) / cellWidth;
                if (noteIndex >= 0 && noteIndex < viewModel.getNumColumns()) {
                    result.regionType = GridRegionType::NoteCell;
                    result.octave = octave;
                    result.noteIndex = noteIndex;
                    result.bounds = getNoteCellBounds(octave, noteIndex);
                    return result;
                }
            }
        }
    }

    return result; // GridRegionType::None
}

} // namespace MoTool

