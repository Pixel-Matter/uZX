#include "TuningPreview.h"

#include "../common/LookAndFeel.h"
#include "../common/Utilities.h"
#include "juce_core/juce_core.h"
#include "juce_graphics/juce_graphics.h"
#include "juce_gui_extra/juce_gui_extra.h"

namespace MoTool {


//================================================================================
TuningPreviewGrid::TuningPreviewGrid(TuningViewModel& vm)
    : viewModel(vm)
{
    setOpaque(true);
}

TuningPreviewGrid::~TuningPreviewGrid() {
}

void TuningPreviewGrid::resized() {
    // Resize logic if needed
}

void TuningPreviewGrid::mouseMove(const MouseEvent& event) {
    lastMousePosition = event.getPosition();

    TuningNote foundNote;
    if (findNoteAtPosition(lastMousePosition, foundNote)) {
        if (!hasHoveredNote || foundNote.midiNote != hoveredNote.midiNote) {
            hoveredNote = foundNote;
            hasHoveredNote = true;
            // Tooltip content will be provided by getTooltip() override
            // repaint(); // Optional: repaint if we want visual feedback
        }
    } else {
        if (hasHoveredNote) {
            hasHoveredNote = false;
            // repaint(); // Optional: repaint to clear any visual feedback
        }
    }
}

void TuningPreviewGrid::paint(juce::Graphics& g) {
    const int cols = viewModel.getNumColumns();

    auto drawCenterTick = [&](const juce::Rectangle<int>& cell) -> void {
        g.setColour(Colors::Theme::surfaceAlt);
        const int cellCenter = cell.getX() + (cell.getWidth() - 2) / 2;
        g.fillRect(cellCenter, cell.getY() - 2, 2, 4);
        g.fillRect(cellCenter, cell.getBottom() - 2, 2, 4);
    };

    auto drawColumnHeader = [&](const TuningNoteName& column, Rectangle<int>& gridCell, const String& text) {
        auto color = column.isInScale ? Colors::Theme::textPrimary : Colors::Theme::textPrimary.withAlpha(0.5f);
        if (column.isRootNote) {
            color = Colors::Theme::primary.interpolatedWith(Colors::Theme::textPrimary, 0.5f);
        }
        g.setColour(color);
        g.drawText(text, gridCell, juce::Justification::centred, true);
        gridCell.translate(cellWidth, 0);
    };

    g.fillAll(Colors::Theme::backgroundAlt);
    auto bounds = getLocalBounds();
    // bounds.setX(0);
    bounds.removeFromTop(headerRowHeight / 2);
    bounds.setWidth(cellWidth * (cols + 1));

    auto gridY = bounds.getY();
    auto gridBottom = bounds.getBottom();

    const auto columns = viewModel.getColumnNoteNames();

    {
        auto gridCell = bounds.removeFromTop(headerRowHeight).withSize(cellWidth, headerRowHeight);
        gridCell.translate(firstCellWidth, 0);
        for (const auto& column : columns) {
            // drawCenterTick(gridCell);
            drawColumnHeader(column, gridCell, column.tuning);
        }
    }

    {
        auto gridCell = bounds.removeFromTop(headerRowHeight).withSize(cellWidth, headerRowHeight);
        gridCell.translate(firstCellWidth, 0);
        for (const auto& column : columns) {
            drawCenterTick(gridCell);
            drawColumnHeader(column, gridCell, column.degree.toString());
        }
    }

    {
        auto gridCell = bounds.removeFromTop(headerRowHeight).withSize(cellWidth, headerRowHeight);
        gridCell.translate(firstCellWidth, 0);
        for (const auto& column : columns) {
            drawCenterTick(gridCell);
            drawColumnHeader(column, gridCell, column.name);
        }
    }

    const auto octaves = viewModel.getOctaveRange();
    for (auto octave = octaves.getStart(); octave < octaves.getEnd(); ++octave) {
        auto gridCell = bounds.removeFromTop(cellHeight).withSize(cellWidth, cellHeight);

        g.setColour(Colors::Theme::textPrimary);
        g.drawText(
            String::formatted("%d", octave),
            gridCell.withWidth(firstCellWidth).withTrimmedRight(8),
            juce::Justification::right, true
        );
        gridCell.translate(firstCellWidth, 0);

        for (const auto& note : viewModel.getOctaveNotes(octave)) {
            float offtune = jlimit(-0.5f, 0.5f, (float) note.offtune / 100.0f); // Normalize offtune to -0.5 to 0.5 range

            auto noteTextColor = Colors::Theme::textPrimary;
            if (offtune > 0.05f) {
                noteTextColor = noteTextColor.interpolatedWith(juce::Colours::blue, offtune * 2);
            } else if (offtune < 0.05f) {
                noteTextColor = noteTextColor.interpolatedWith(juce::Colours::red, -offtune * 2);
            }

            auto noteBgColor = note.isInMidiRange() ? Colors::Theme::background : Colors::Theme::background.withAlpha(0.33f);
            if (note.isInScale) {
                g.setColour(noteBgColor);
                g.fillRect(gridCell.reduced(2, 2));
            } else {
                noteTextColor = noteTextColor.withAlpha(0.5f);
                g.setColour(noteBgColor.withAlpha(noteBgColor.getFloatAlpha() * 0.5f));
                g.fillRect(gridCell.reduced(2, 2));
            }

            //=================================================================
            // tuner ticks

            // 1. Inter-cell reference tuning ticks at the horizontal center of the cell
            auto tickSize = 6;
            drawCenterTick(gridCell);

            // 2. In-cell tuning ticks of this specific note
            auto calcTickX = [&](const float tick) -> float {
                return jmap<float>(tick, -0.5f, 0.5f, (float) gridCell.getX(), (float) gridCell.getRight() - 2.0f);
            };
            auto drawTick = [&](const float tickX) -> void {
                g.fillRect(tickX, (float) gridCell.getY() + 2, 2.0f, (float) tickSize);
                g.fillRect(tickX, (float) gridCell.getBottom() - 2.0f - (float) tickSize, 2.0f, (float) tickSize);
            };

            // for (const auto& tick : viewModel.getTicksAroundNote(note)) {
            //     drawTick(calcTickX(tick));
            // }

            // 3. Offtune tick for this specific note
            float offX = calcTickX(offtune);
            if (note.offtune >= -50 && note.offtune <= 50) {
                drawTick(offX);
            }

            //=================================================================
            // drawing text

            auto textCell = gridCell;
            auto text = String::formatted("%d", note.period);
            int textWidth = static_cast<int>(std::ceil(TextLayout::getStringWidth(g.getCurrentFont(), text)));
            // auto textWidth = static_cast<int>(std::ceil(g.getCurrentFont().getStringWidthFloat(text)));
            textCell.setWidth(textWidth);
            textCell.setCentre((int) offX, (int) textCell.getCentreY());
            textCell = textCell.constrainedWithin(gridCell.reduced(2, 0));

            g.setColour(noteTextColor);
            g.drawText(text, textCell, juce::Justification::centred, false);

            // safe for env marker
            if (note.isSafeForEnvelope()) {
                g.setColour(Colors::Theme::success);
                // TODO tiny triangle icon
                // TODO add a legend below the grid
                g.fillRect(gridCell.getRight() - 6, gridCell.getY() + 2, 4, 4);
            }

            gridCell.translate(cellWidth, 0);
        }
        gridBottom = gridCell.getBottom();
        // bounds.removeFromTop(1); // Add a line between rows
    }

    // Draw vertical line at the root note column (current key)
    {
        const int rootColumnIndex = static_cast<int>(viewModel.getCurrentKey());
        const int rootColumnX = bounds.getX() + firstCellWidth + rootColumnIndex * cellWidth;
        g.setColour(Colors::Theme::primary.withAlpha(0.5f));
        g.fillRect(rootColumnX, gridY, 2, gridBottom - gridY);
    }
}

bool TuningPreviewGrid::findNoteAtPosition(Point<int> position, TuningNote& outNote) const {
    const int cols = viewModel.getNumColumns();

    auto bounds = getLocalBounds();
    bounds.setWidth(cellWidth * (cols + 2));
    bounds.setX((getWidth() - bounds.getWidth()) / 2);

    // Skip header row
    auto headerBounds = bounds.removeFromTop(gridYOffset);
    if (position.y < headerBounds.getBottom()) {
        return false; // Mouse is in header area
    }

    // Calculate which octave row we're in
    const auto octaves = viewModel.getOctaveRange();
    const int rowsStartY = headerBounds.getBottom();
    const int relativeY = position.y - rowsStartY;

    if (relativeY < 0) {
        return false; // Above the grid
    }

    const int rowIndex = relativeY / cellHeight;
    const int octave = octaves.getStart() + rowIndex;

    if (octave >= octaves.getEnd()) {
        return false; // Below the grid
    }

    // Calculate which note column we're in
    const int notesStartX = bounds.getX() + firstCellWidth; // Skip octave label column
    const int relativeX = position.x - notesStartX;

    if (relativeX < 0) {
        return false; // In octave label column
    }

    const int noteIndex = relativeX / cellWidth;

    if (noteIndex >= cols) {
        return false; // Past the last note column
    }

    // Verify we're within the actual cell bounds (not in padding)
    const int cellX = notesStartX + noteIndex * cellWidth;
    const int cellY = rowsStartY + rowIndex * cellHeight;

    if (position.x >= cellX && position.x < cellX + cellWidth &&
        position.y >= cellY && position.y < cellY + cellHeight) {
        // Get the note for this octave and column
        auto octaveNotes = viewModel.getOctaveNotes(octave);
        if (noteIndex < static_cast<int>(octaveNotes.size())) {
            outNote = octaveNotes[(size_t) noteIndex];
            return true;
        }
    }

    return false; // Mouse not over any note cell
}

String TuningPreviewGrid::getTooltip() {
    return hasHoveredNote ? hoveredNote.getTooltip() : "";
}


static void addItemsFromStrings(juce::ComboBox& comboBox, const StringArray& items) {
    for (int i = 0; i < items.size(); ++i) {
        comboBox.addItem(items[i], i + 1);
    }
}

//================================================================================
TuningPreviewComponent::TuningPreviewComponent(UndoManager* um)
    : viewModel(um)
    , tuningGrid(viewModel)
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
    addItemsFromStrings(keySelect, Scale::getAllKeyNames());
    setupScaleSelectMenu();
    keyScaleLabel.setText("Scale:", juce::dontSendNotification);
    keyScaleLabel.setJustificationType(juce::Justification::centredRight);
    keySelect.getSelectedIdAsValue().referTo(viewModel.keyIndex1.getValue());

    addAndMakeVisible(keyScaleLabel);
    addAndMakeVisible(keySelect);
    addAndMakeVisible(scaleSelect);

    // Set up Chip Clock selection ComboBox
    addItemsFromStrings(chipClockSelect, viewModel.getChipClockLabels());
    chipClockLabel.setText("Chip Clock:", juce::dontSendNotification);
    chipClockLabel.setJustificationType(juce::Justification::centredRight);
    chipClockSelect.getSelectedIdAsValue().referTo(viewModel.chipIndex1.getValue());

    addAndMakeVisible(chipClockLabel);
    addAndMakeVisible(chipClockSelect);

    // Set up frequency sliders with Value binding
    setupSliderWithValueBinding(clockFrequencySlider, clockFrequencyLabel, "Clock Frequency (MHz):",
                                1.0, 2.0, 0.001, viewModel.clockFrequencyMhz.getValue());

    setupSliderWithValueBinding(a4FrequencySlider, a4FrequencyLabel, "A4 Frequency (Hz):",
                                220.0, 880.0, 0.1, viewModel.a4Frequency.getValue());

    // Set initial clock controls state
    updateClockControlsState();

    // Register as a change listener to the view model
    viewModel.addChangeListener(this);

    tuningNameLabel.setText("Tuning: " + viewModel.getTuningDescription(), juce::dontSendNotification);
    addAndMakeVisible(tuningNameLabel);
    addAndMakeVisible(toneEnvSwitchLabel);
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
    auto keyScaleRow = rightColumn.removeFromTop(rowHeight);
    keyScaleLabel.setBounds(keyScaleRow.removeFromLeft(labelsColWidth));
    keySelect.setBounds(keyScaleRow.removeFromLeft(moduleWidth));
    keyScaleRow.removeFromLeft(gap);
    scaleSelect.setBounds(keyScaleRow.removeFromLeft(moduleWidth * 3 - gap));

    rightColumn.removeFromTop(gap);

    auto chipLabelRow = rightColumn.removeFromTop(rowHeight);
    chipClockLabel.setBounds(chipLabelRow.removeFromLeft(labelsColWidth));
    chipClockSelect.setBounds(chipLabelRow.removeFromLeft(moduleWidth * 4));

    rightColumn.removeFromTop(gap);

    // Clock frequency slider on its own row
    auto clockFreqRow = rightColumn.removeFromTop(rowHeight);
    clockFrequencyLabel.setBounds(clockFreqRow.removeFromLeft(labelsColWidth));
    clockFrequencySlider.setBounds(clockFreqRow.removeFromLeft(moduleWidth * 8));

    rightColumn.removeFromTop(gap);

    // A4 frequency slider with label
    auto a4Row = rightColumn.removeFromTop(rowHeight);
    a4FrequencyLabel.setBounds(a4Row.removeFromLeft(labelsColWidth));
    a4FrequencySlider.setBounds(a4Row.removeFromLeft(moduleWidth * 8));

    rightColumn.removeFromTop(gap);

    // TuningTypeLabel.setBounds(rightColumn.removeFromTop(controlHeight));
    tuningNameLabel.setBounds(rightColumn.removeFromTop(rowHeight));
    toneEnvSwitchLabel.setBounds(rightColumn.removeFromTop(rowHeight));

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
        updateClockControlsState();
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
void TuningPreviewComponent::setupSliderWithValueBinding(Slider& slider, Label& label, const String& labelText,
                                                        double min, double max, double step, Value& valueToReference) {
    slider.setRange(min, max, step);
    slider.setSliderStyle(Slider::LinearHorizontal);
    slider.setTextBoxStyle(Slider::TextBoxRight, false, 80, 20);

    // Use Value binding for automatic bidirectional sync
    slider.getValueObject().referTo(valueToReference);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void TuningPreviewComponent::updateClockControlsState() {
    bool isCustom = viewModel.isCustomClockEnabled();
    clockFrequencySlider.setEnabled(isCustom);
    clockFrequencyLabel.setEnabled(isCustom);
}

void TuningPreviewComponent::setupScaleSelectMenu() {
    // Clear any existing items and mapping
    scaleSelect.clear();

    // Get all scale categories
    auto categories = Scale::getAllScaleCategories();
    int menuItemId = 1;

    // Build the grouped menu structure
    PopupMenu rootMenu;

    for (auto category : categories) {
        if (category == Scale::ScaleCategory::User) {
            continue; // Skip user defined for now
        }

        // Add category header (non-selectable)
        rootMenu.addSectionHeader(Scale::getNameForCategory(category));

        // Add scales in this category
        auto scalesInCategory = Scale::getAllScaleTypesForCategory(category);
        for (auto scaleType : scalesInCategory) {
            String scaleName = Scale::getNameForType(scaleType);
            rootMenu.addItem(menuItemId, scaleName);

            // Also add to ComboBox for text display purposes
            scaleSelect.addItem(scaleName, menuItemId);
            menuItemId++;
        }

        // Add separator after each category (except the last one)
        if (category != categories.back() || categories.back() == Scale::ScaleCategory::User) {
            rootMenu.addSeparator();
        }
    }

    // Replace the ComboBox's root menu with our grouped menu
    *scaleSelect.getRootMenu() = rootMenu;

    scaleSelect.getSelectedIdAsValue().referTo(viewModel.scaleIndex1.getValue());
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