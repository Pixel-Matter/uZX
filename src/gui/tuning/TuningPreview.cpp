#include "TuningPreview.h"

#include "../common/LookAndFeel.h"
#include "../common/Utilities.h"
#include "juce_core/juce_core.h"
#include "juce_graphics/juce_graphics.h"
#include "juce_gui_extra/juce_gui_extra.h"

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

void TuningPreviewGrid::resized() {
    // Resize logic if needed
}

void TuningPreviewGrid::mouseMove(const MouseEvent& event) {
    lastMousePosition = event.getPosition();

    GridLayout layout(getLocalBounds(), viewModel);
    GridHitResult newHover = layout.hitTest(lastMousePosition);

    // Check if we're hovering over a different region
    bool regionChanged = (newHover.regionType != hoveredRegion.regionType ||
                         newHover.octave != hoveredRegion.octave ||
                         newHover.noteIndex != hoveredRegion.noteIndex ||
                         newHover.headerType != hoveredRegion.headerType);

    if (regionChanged) {
        hoveredRegion = newHover;
        // repaint(); // Optional: repaint if we want visual feedback
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
    String text = column.getHeadingText(headingType);

    // Set text color
    auto color = column.isInScale ? Colors::Theme::textPrimary : Colors::Theme::textPrimary.withAlpha(0.5f);
    if (column.isRootNote) {
        color = Colors::Theme::primary.interpolatedWith(Colors::Theme::textPrimary, 0.5f);
    }
    g.setColour(color);
    g.drawText(text, bounds, juce::Justification::centred, true);
}

void TuningPreviewGrid::paintRowHeader(juce::Graphics& g, const juce::Rectangle<int>& bounds, int octave) {
    g.setColour(Colors::Theme::textPrimary);
    g.drawText(
        String::formatted("%d", octave),
        bounds.withTrimmedRight(8),
        juce::Justification::right, true
    );
}

void TuningPreviewGrid::paintNoteCell(juce::Graphics& g, const juce::Rectangle<int>& bounds, const TuningNote& note) {
    float offtune = jlimit(-0.5f, 0.5f, (float) note.offtune / 100.0f);

    auto noteTextColor = Colors::Theme::textPrimary;
    if (offtune > 0.05f) {
        noteTextColor = noteTextColor.interpolatedWith(juce::Colours::blue, offtune * 2);
    } else if (offtune < 0.05f) {
        noteTextColor = noteTextColor.interpolatedWith(juce::Colours::red, -offtune * 2);
    }

    // Check if this note is currently playing
    bool isPlaying = note.isInMidiRange() && tuningPlayer.isNotePlaying(note.midiNote);

    auto noteBgColor = note.isInMidiRange() ? Colors::Theme::background : Colors::Theme::background.withAlpha(0.33f);

    // Use pressed button color for currently playing notes
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
        return jmap<float>(tick, -0.5f, 0.5f, (float) bounds.getX(), (float) bounds.getRight() - 2.0f);
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
    auto text = String::formatted("%d", note.period);
    int textWidth = static_cast<int>(std::ceil(TextLayout::getStringWidth(g.getCurrentFont(), text)));
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
        // Draw row header
        auto rowHeaderBounds = layout.getRowHeaderBounds(octave);
        paintRowHeader(g, rowHeaderBounds, octave);

        // Draw note cells
        auto octaveNotes = viewModel.getOctaveNotes(octave);
        for (int noteIndex = 0; noteIndex < static_cast<int>(octaveNotes.size()); ++noteIndex) {
            auto cellBounds = layout.getNoteCellBounds(octave, noteIndex);
            paintNoteCell(g, cellBounds, octaveNotes[static_cast<size_t>(noteIndex)]);
        }
    }

    // Draw vertical line at the root note column (current root)
    const int rootColumnIndex = static_cast<int>(viewModel.getCurrentRoot());
    auto firstCellBounds = layout.getNoteCellBounds(octaveRange.getStart(), rootColumnIndex);
    auto lastCellBounds = layout.getNoteCellBounds(octaveRange.getEnd() - 1, rootColumnIndex);

    int lineX = firstCellBounds.getX();
    int lineY = layout.gridBounds.getY();
    int lineHeight = lastCellBounds.getBottom() - lineY;

    g.setColour(Colors::Theme::primary.withAlpha(0.5f));
    g.fillRect(lineX, lineY, 2, lineHeight);
}

String TuningPreviewGrid::getTooltip() {
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
                String headerTypeName;
                switch (hoveredRegion.headerType) {
                    case NoteGridHeadingType::Tuning: headerTypeName = "Tuning"; break;
                    case NoteGridHeadingType::Degrees: headerTypeName = "Degree"; break;
                    case NoteGridHeadingType::Notes: headerTypeName = "Note"; break;
                }
                String headerText = column.getHeadingText(hoveredRegion.headerType);
                return String::formatted("%s: %s", headerTypeName.toUTF8(), headerText.toUTF8());
            }
            break;
        }
        case GridRegionType::None:
            break;
    }

    return "";
}

void TuningPreviewGrid::mouseDown(const MouseEvent& event) {
    GridLayout layout(getLocalBounds(), viewModel);
    GridHitResult hitResult = layout.hitTest(event.getPosition());

    if (hitResult.regionType == GridRegionType::NoteCell) {
        auto octaveNotes = viewModel.getOctaveNotes(hitResult.octave);
        auto& note = octaveNotes[static_cast<size_t>(hitResult.noteIndex)];
        if (note.isInMidiRange()) {
            if (viewModel.playChords.get()) {
                tuningPlayer.playDegreeChord(note.midiNote);
            } else {
                tuningPlayer.playNote(note.midiNote);
            }
        }
    } else if (hitResult.regionType == GridRegionType::RowHeader) {
        // Play scale as arpeggio for the clicked octave
        auto scaleNotes = viewModel.getScaleNotes(hitResult.octave, true);
        std::vector<int> notes;
        for (const auto& note : scaleNotes) {
            notes.push_back(note);
        }
        notes.push_back(scaleNotes.front() + 12); // Add octave note
        tuningPlayer.playArpeggio(notes);
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

//================================================================================
TuningPreviewComponent::TuningPreviewComponent(UndoManager* um)
    : viewModel(um)
    , tuningPlayer(viewModel, MoToolApp::getController().getEngine())
    , tuningGrid(viewModel, tuningPlayer)
    , tooltipWindow(nullptr, 750) // ms delay
{
    setOpaque(true);

    // Set up tuning table ListBox with Value binding
    tuningTableLabel.setText("Tuning Tables:", juce::dontSendNotification);
    tuningsListBox.setModel(this);
    tuningsListBox.setMultipleSelectionEnabled(false);
    tuningsListBox.selectRow(static_cast<int>(viewModel.selectedTuningTable.get()), false, false);
    // Note: ListBox doesn't have direct Value binding, so we'll use a custom approach
    viewModel.selectedTuningTable.addListener(this);

    addAndMakeVisible(tuningTableLabel);
    addAndMakeVisible(tuningsListBox);

    // Set up Key selection ComboBox
    keyScaleLabel.setText("Scale:", juce::dontSendNotification);
    keyScaleLabel.setJustificationType(juce::Justification::centredRight);
    setupScaleSelectMenu();

    addAndMakeVisible(keyScaleLabel);
    addAndMakeVisible(keySelect);
    addAndMakeVisible(scaleSelect);

    // Set up Chip Clock selection ComboBox
    chipClockLabel.setText("Chip clock:", juce::dontSendNotification);
    chipClockLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(chipClockLabel);
    addAndMakeVisible(chipClockSelect);

    // Set up frequency sliders with Value binding
    setupSliderWithValueBinding(clockFrequencySlider, clockFrequencyLabel, "Clock frequency:", clockFrequencyUnits,
                                viewModel.clockFrequencyMhz);

    setupSliderWithValueBinding(a4FrequencySlider, a4FrequencyLabel, "A4 frequency:", a4FrequencyUnits,
                                viewModel.a4Frequency);

    // Set initial controls state
    updateControlsState();

    // Register as a change listener to the view model
    viewModel.addChangeListener(this);

    tuningNameLabel.setText(viewModel.getTuningDescription(), juce::dontSendNotification);
    addAndMakeVisible(tuningNameLabel);
    // addAndMakeVisible(toneEnvSwitchLabel);

    // Set up play mode checkboxes
    playChordsCheckBox.setButtonText("Play chords");
    playChordsCheckBox.getToggleStateValue().referTo(viewModel.playChords.getValue());

    playToneCheckBox.setButtonText("Tone");
    playToneCheckBox.getToggleStateValue().referTo(viewModel.playTone.getValue());

    playEnvelopeCheckBox.setButtonText("Envelope");
    playEnvelopeCheckBox.getToggleStateValue().referTo(viewModel.playEnvelope.getValue());

    addAndMakeVisible(playChordsCheckBox);
    addAndMakeVisible(playToneCheckBox);
    addAndMakeVisible(playEnvelopeCheckBox);
    addAndMakeVisible(envelopeShapeSelect);
    addAndMakeVisible(tuningGrid);

    // Connect tooltip window to grid
    tuningGrid.setTooltipWindow(&tooltipWindow);

    // Set up export button
    exportButton.setButtonText("Export to CSV");
    exportButton.onClick = [this]() {
        String defaultFilename = viewModel.getDefaultExportFilename();

        // Get last export directory from utility function
        File lastExportDir = Helpers::getLastCsvExportDirectory();
        File defaultFile = lastExportDir.getChildFile(defaultFilename);

        FileChooser fileChooser("Save Tuning Data as CSV",
                               defaultFile,
                               "*.csv");

        if (fileChooser.browseForFileToSave(true)) {
            File selectedFile = fileChooser.getResult();
            String csvData = viewModel.exportToCSV();

            if (selectedFile.replaceWithText(csvData)) {
                // Save the directory for next time using utility function
                Helpers::setLastCsvExportDirectory(selectedFile.getParentDirectory());

                AlertWindow::showMessageBox(AlertWindow::InfoIcon,
                                            "Export Successful",
                                            "Tuning data exported to:\n" + selectedFile.getFullPathName());
            } else {
                AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                            "Export Failed",
                                            "Failed to save file:\n" + selectedFile.getFullPathName());
            }
        }
    };
    addAndMakeVisible(exportButton);
}

TuningPreviewComponent::~TuningPreviewComponent() {
    viewModel.removeChangeListener(this);
    viewModel.selectedTuningTable.removeListener(this);
}

void TuningPreviewComponent::resized() {
    auto bounds = getLocalBounds();
    auto formBounds = bounds.reduced(20, 20);

    // Create two columns layout
    auto leftColumnWidth = moduleWidth * 4;  // Width for the left column with tuning table list
    auto leftColumn = formBounds.removeFromLeft(leftColumnWidth);
    formBounds.removeFromLeft(gap); // Gap between columns
    auto rightColumn = formBounds;

    // Left column: Tuning table selection
    tuningTableLabel.setBounds(leftColumn.removeFromTop(rowHeight));
    // leftColumn.removeFromTop(gap / 2);
    tuningsListBox.setBounds(leftColumn); // Take remaining space in left column

    // Right column: Other controls and tuning grid
    // Place KeySelect and ScaleSelect on the same row with fixed widths
    auto labelsColWidth = moduleWidth * 2; // Width for labels

    {
        auto keyScaleRow = rightColumn.removeFromTop(rowHeight);
        keyScaleLabel.setBounds(keyScaleRow.removeFromLeft(labelsColWidth));
        keySelect.setBounds(keyScaleRow.removeFromLeft(moduleWidth));
        keyScaleRow.removeFromLeft(gap);
        scaleSelect.setBounds(keyScaleRow.removeFromLeft(moduleWidth * 3 - gap));
    }

    rightColumn.removeFromTop(gap);

    {
        auto chipLabelRow = rightColumn.removeFromTop(rowHeight);
        chipClockLabel.setBounds(chipLabelRow.removeFromLeft(labelsColWidth));
        chipClockSelect.setBounds(chipLabelRow.removeFromLeft(moduleWidth * 4));
    }

    rightColumn.removeFromTop(gap);

    { // Clock frequency slider on its own row
        auto clockFreqRow = rightColumn.removeFromTop(rowHeight);
        clockFrequencyLabel.setBounds(clockFreqRow.removeFromLeft(labelsColWidth));
        clockFrequencySlider.setBounds(clockFreqRow.removeFromLeft(moduleWidth * 8));
        clockFrequencyUnits.setBounds(clockFreqRow);
    }

    rightColumn.removeFromTop(gap);

    { // A4 frequency slider with label
        auto a4Row = rightColumn.removeFromTop(rowHeight);
        a4FrequencyLabel.setBounds(a4Row.removeFromLeft(labelsColWidth));
        a4FrequencySlider.setBounds(a4Row.removeFromLeft(moduleWidth * 8));
        a4FrequencyUnits.setBounds(a4Row);
    }

    rightColumn.removeFromTop(gap);

    // TuningTypeLabel.setBounds(rightColumn.removeFromTop(controlHeight));
    tuningNameLabel.setBounds(rightColumn.removeFromTop(rowHeight));
    // toneEnvSwitchLabel.setBounds(rightColumn.removeFromTop(rowHeight));

    rightColumn.removeFromTop(gap);

    {   // Play mode row with multiple controls
        auto playToneRow = rightColumn.removeFromTop(rowHeight);
        playToneRow.removeFromLeft(labelsColWidth);
        // playModeRow.removeFromLeft(labelsColWidth);
        playToneCheckBox.setBounds(playToneRow.removeFromLeft(moduleWidth * 2));
        playToneRow.removeFromLeft(gap / 2);
        playChordsCheckBox.setBounds(playToneRow.removeFromLeft(moduleWidth * 2));
    }

    rightColumn.removeFromTop(gap);

    {
        auto playEnvRow = rightColumn.removeFromTop(rowHeight);
        playEnvRow.removeFromLeft(labelsColWidth);
        playEnvelopeCheckBox.setBounds(playEnvRow.removeFromLeft(moduleWidth * 2));
        playEnvRow.removeFromLeft(gap / 2);
        envelopeShapeSelect.setBounds(playEnvRow.removeFromLeft(moduleWidth * 2));
    }

    rightColumn.removeFromTop(gap);

    // Reserve space for export button at bottom
    auto exportButtonArea = rightColumn.removeFromBottom(rowHeight);
    rightColumn.removeFromBottom(gap);

    tuningGrid.setBounds(rightColumn);

    // Export button below the grid
    exportButton.setBounds(exportButtonArea.removeFromLeft(moduleWidth * 2));
}

void TuningPreviewComponent::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::background);
    g.setColour(juce::Colours::white);
}

void TuningPreviewComponent::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &viewModel) {
        // DBG("TuningPreviewComponent::changeListenerCallback");
        tuningNameLabel.setText("Tuning Name: " + viewModel.getTuningDescription(), juce::dontSendNotification);
        updateControlsState();
        tuningGrid.repaint();
    }
}

// ListBoxModel implementation
int TuningPreviewComponent::getNumRows() {
    return viewModel.getTuningTableNames().size();
}

void TuningPreviewComponent::paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) {
    if (rowIsSelected) {
        g.setColour(Colors::Theme::primary);
        g.fillRect(0, 0, width, height);
    }

    g.setColour(Colors::Theme::textPrimary);
    // g.setColour(rowIsSelected ? Colors::Theme::textPrimary : Colors::Theme::textSecondary);

    auto tableNames = viewModel.getTuningTableNames();
    if (rowNumber >= 0 && rowNumber < tableNames.size()) {
        g.drawText(tableNames[rowNumber], 4, 0, width - 8, height, juce::Justification::centredLeft, true);
    }
}

void TuningPreviewComponent::listBoxItemClicked(int row, const MouseEvent& e) {
    juce::ignoreUnused(e);
    viewModel.selectedTuningTable = BuiltinTuningType(row); // Update the tuning table index in the view model
}

// UI setup helpers
void TuningPreviewComponent::setupSliderWithValueBinding(Slider& slider, Label& label, const String& labelText, Label& unitsLabel,
                                                         RangedParamAttachment<double>& attachment) {
    const auto range = attachment.getRange();
    slider.setRange(range.start, range.end, range.interval);
    slider.setSliderStyle(Slider::LinearHorizontal);
    slider.setTextBoxStyle(Slider::TextBoxRight, false, 80, 20);

    // Use Value binding for automatic bidirectional sync
    slider.getValueObject().referTo(attachment.getValue());

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredRight);

    unitsLabel.setText(attachment.getUnits(), juce::dontSendNotification);
    unitsLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(slider);
    addAndMakeVisible(label);
    addAndMakeVisible(unitsLabel);
}

void TuningPreviewComponent::updateControlsState() {
    // Update clock controls
    bool isCustom = viewModel.isCustomClockEnabled();
    clockFrequencySlider.setEnabled(isCustom);
    clockFrequencyLabel.setEnabled(isCustom);

    // Update envelope controls
    playChordsCheckBox.setEnabled(!viewModel.isEnvelopePeriodsShown());
    playEnvelopeCheckBox.setEnabled(!viewModel.playChords.get());
    envelopeShapeSelect.setEnabled(viewModel.isEnvelopeEnabled());
}

void TuningPreviewComponent::setupScaleSelectMenu() {
    // Clear any existing items and mapping
    scaleSelect.clear();

    // Get all scale categories
    auto categories = Scale::getAllScaleCategories();
    int menuItemId = 1;

    for (auto category : categories) {
        if (category == Scale::ScaleCategory::User) {
            continue; // Skip user defined for now
        }

        scaleSelect.addSectionHeading(Scale::getNameForCategory(category));

        // Add scales in this category
        auto scalesInCategory = Scale::getAllScaleTypesForCategory(category);
        for (auto scaleType : scalesInCategory) {
            String scaleName = Scale::getNameForType(scaleType);
            scaleSelect.addItem(scaleName, menuItemId);
            menuItemId++;
        }

        // Add separator after each category (except the last one)
        if (category != categories.back() || categories.back() == Scale::ScaleCategory::User) {
            scaleSelect.addSeparator();
        }
    }
}

// Value::Listener implementation for ListBox and other custom sync
void TuningPreviewComponent::valueChanged(Value& value) {
    // DBG("TuningPreviewComponent::valueChanged");
    if (value.refersToSameSourceAs(viewModel.selectedTuningTable.getValue())) {
        int newIndex = static_cast<int>(viewModel.selectedTuningTable.get());
        tuningsListBox.selectRow(newIndex, false, false);
    }
}

}  // namespace MoTool