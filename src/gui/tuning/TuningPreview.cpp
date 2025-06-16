#include "TuningPreview.h"

#include "../common/LookAndFeel.h"
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
    // const int rows = viewModel.getNumRows();

    g.fillAll(Colors::Theme::backgroundAlt);
    auto bounds = getLocalBounds();
    // bounds.reduce(20, 20); // Add some padding

    bounds.setWidth(cellWidth * (cols + 2));
    // center the grid horizontally
    bounds.setX((getWidth() - bounds.getWidth()) / 2);

    // auto grid = bounds.removeFromTop(cellSize);
    {
        auto gridCell = bounds.removeFromTop(cellHeight).withSize(cellWidth, cellHeight);

        // g.setColour(juce::Colours::white);
        // g.drawText("Oct", gridCell, juce::Justification::centred, true);
        gridCell.translate(cellWidth, 0);
        for (const auto& column : viewModel.getColumnNoteNames()) {
            if (!column.isInScale) {
                // g.setColour(Colors::Theme::background.withAlpha(0.5f));
                // g.fillRect(gridCell.reduced(2, 2));
                g.setColour(Colors::Theme::textPrimary.withAlpha(0.5f));
            } else {
                // g.setColour(Colors::Theme::background);
                // g.fillRect(gridCell.reduced(2, 2));
                g.setColour(Colors::Theme::textPrimary);
            }
            g.drawText(column.name, gridCell, juce::Justification::centred, true);
            gridCell.translate(cellWidth, 0);
        }
    }

    const auto octaves = viewModel.getOctaveRange();
    for (auto octave = octaves.getStart(); octave < octaves.getEnd(); ++octave) {
        auto gridCell = bounds.removeFromTop(cellHeight).withSize(cellWidth, cellHeight);

        g.setColour(Colors::Theme::textPrimary);
        g.drawText(String::formatted("%d", octave), gridCell.withTrimmedRight(8), juce::Justification::right, true);
        gridCell.translate(cellWidth, 0);
        for (const auto& note : viewModel.getOctaveNotes(octave)) {
            float offtune = jlimit(-0.5f, 0.5f, (float) note.offtune / 100.0f); // Normalize offtune to -0.5 to 0.5 range

            auto noteTextColor = Colors::Theme::textPrimary;
            if (offtune > 0.05f) {
                noteTextColor = noteTextColor.interpolatedWith(juce::Colours::blue, offtune * 2);
            } else if (offtune < 0.05f) {
                noteTextColor = noteTextColor.interpolatedWith(juce::Colours::red, -offtune * 2);
            }

            auto noteBgColor = note.isInMidiRange() ? Colors::Theme::background : Colors::Theme::background.withAlpha(0.33f);
            if (!note.isInScale) {
                g.setColour(noteBgColor.withAlpha(noteBgColor.getFloatAlpha() * 0.5f));
                g.fillRect(gridCell.reduced(2, 2));
                noteTextColor = noteTextColor.withAlpha(0.5f);
            } else {
                g.setColour(noteBgColor);
                g.fillRect(gridCell.reduced(2, 2));
            }

            //=================================================================
            // tuner ticks

            // 1. Inter-cell reference tuning ticks at the horizontal center of the cell
            auto tickSize = 6;
            g.setColour(Colors::Theme::surfaceAlt);
            const int cellCenter = gridCell.getX() + (gridCell.getWidth() - 2) / 2;
            g.fillRect(cellCenter, gridCell.getY() - 2, 2, 4);
            g.fillRect(cellCenter, gridCell.getBottom() - 2, 2, 4);

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
            // TODO add mousover bubble with frequency, note name, offtune in cents information
            // g.drawText(String::formatted("%.2f", note.frequency), gridCell.withTrimmedRight(8), juce::Justification::right, true);

            gridCell.translate(cellWidth, 0);
        }
        // bounds.removeFromTop(1); // Add a line between rows
    }
}

bool TuningPreviewGrid::findNoteAtPosition(Point<int> position, TuningNote& outNote) const {
    const int cols = viewModel.getNumColumns();

    auto bounds = getLocalBounds();
    bounds.reduce(20, 20); // Same padding as in paint
    bounds.setWidth(cellWidth * (cols + 2));
    bounds.setX((getWidth() - bounds.getWidth()) / 2);

    // Skip header row
    auto headerBounds = bounds.removeFromTop(cellHeight);
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
    const int notesStartX = bounds.getX() + cellWidth; // Skip octave label column
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

//================================================================================
TuningPreviewComponent::TuningPreviewComponent()
    : viewModel(TuningViewModel())
    , tuningGrid(viewModel)
    , tooltipWindow(nullptr, 750) // ms delay
{
    setOpaque(true);

    // Set up tuning table ListBox
    tuningTableLabel.setText("Tuning Tables:", juce::dontSendNotification);
    tuningTableListBox.setModel(this);
    tuningTableListBox.setMultipleSelectionEnabled(false);
    tuningTableListBox.selectRow(viewModel.getCurrentTuningTableIndex(), false, false);

    addAndMakeVisible(tuningTableLabel);
    addAndMakeVisible(tuningTableListBox);

    // Set up Key selection ComboBox
    addAndMakeVisible(KeyScaleLabel);
    addAndMakeVisible(KeySelect);
    addAndMakeVisible(ScaleSelect);

    KeyScaleLabel.setText("Scale:", juce::dontSendNotification);
    KeyScaleLabel.setJustificationType(juce::Justification::centredRight);

    auto keyNames = viewModel.getKeyNames();
    for (int i = 0; i < keyNames.size(); ++i) {
        KeySelect.addItem(keyNames[i], i + 1);
    }
    KeySelect.setSelectedId(static_cast<int>(viewModel.getCurrentKey()) + 1, juce::dontSendNotification);
    KeySelect.onChange = [this]() {
        int selectedId = KeySelect.getSelectedId();
        if (selectedId > 0) {
            viewModel.setCurrentKey(static_cast<Key>(selectedId - 1));
        }
    };

    // Set up Chip Clock selection ComboBox
    ChipClockLabel.setText("Chip Clock:", juce::dontSendNotification);
    ChipClockLabel.setJustificationType(juce::Justification::centredRight);

    auto chipClockLabels = viewModel.getChipClockLabels();
    for (int i = 0; i < chipClockLabels.size(); ++i) {
        ChipClockSelect.addItem(chipClockLabels[i], i + 1);
    }
    ChipClockSelect.setSelectedId(viewModel.getCurrentChipClockIndex() + 1, juce::dontSendNotification);
    ChipClockSelect.onChange = [this]() {
        int selectedId = ChipClockSelect.getSelectedId();
        if (selectedId > 0) {
            viewModel.setChipClockChoice(selectedId - 1); // Convert from 1-based ID to 0-based index
            updateClockControlsState();
            tuningGrid.repaint(); // Refresh the tuning grid with new calculations
        }
    };

    // Set up frequency sliders using helper
    setupSlider(clockFrequencySlider, clockFrequencyLabel, "Clock Frequency (MHz):", 
                1.0, 2.0, 0.001, [this]() {
        viewModel.setClockFrequency(clockFrequencySlider.getValue() * 1000000.0);
    });
    clockFrequencySlider.setValue(viewModel.getClockFrequency() / 1000000.0, juce::dontSendNotification);
    
    setupSlider(a4FrequencySlider, a4FrequencyLabel, "A4 Frequency (Hz):", 
                220.0, 880.0, 0.1, [this]() {
        viewModel.setA4Frequency(a4FrequencySlider.getValue());
    });
    a4FrequencySlider.setValue(viewModel.getA4Frequency(), juce::dontSendNotification);

    // Set initial clock controls state
    updateClockControlsState();

    // Set up Scale selection ComboBox with category grouping
    setupScaleSelectMenu();
    updateScaleSelection();
    KeyScaleLabel.setText("Scale:", juce::dontSendNotification);

    // Register as a change listener to the view model
    viewModel.addChangeListener(this);
    TuningTypeLabel.setText("Tuning Type: " + viewModel.getTuningTypeName(), juce::dontSendNotification);
    TuningNameLabel.setText("Tuning Name: " + viewModel.getTuningName(), juce::dontSendNotification);
    // ToneEnvSwitchLabel.setText("Tone Env Switch: " + String(viewModel.isToneEnvSwitchEnabled()), juce::dontSendNotification);

    addAndMakeVisible(ChipClockLabel);
    addAndMakeVisible(ChipClockSelect);


    // addAndMakeVisible(TuningTypeLabel);
    addAndMakeVisible(TuningNameLabel);
    addAndMakeVisible(ToneEnvSwitchLabel);
    addAndMakeVisible(tuningGrid);

    // Connect tooltip window to grid
    tuningGrid.setTooltipWindow(&tooltipWindow);
}

TuningPreviewComponent::~TuningPreviewComponent() {
    viewModel.removeChangeListener(this);
}

void TuningPreviewComponent::resized() {
    auto bounds = getLocalBounds();
    auto gap = 8;
    auto rowHeight = 30;
    auto widthModule = 60;

    auto formBounds = bounds.reduced(20, 20);

    // Create two columns layout
    auto leftColumnWidth = widthModule * 4;  // Width for the left column with tuning table list
    auto leftColumn = formBounds.removeFromLeft(leftColumnWidth);
    formBounds.removeFromLeft(gap); // Gap between columns
    auto rightColumn = formBounds;

    // Left column: Tuning table selection
    tuningTableLabel.setBounds(leftColumn.removeFromTop(rowHeight));
    // leftColumn.removeFromTop(gap / 2);
    tuningTableListBox.setBounds(leftColumn); // Take remaining space in left column

    // Right column: Other controls and tuning grid
    // Place KeySelect and ScaleSelect on the same row with fixed widths
    auto labelsColWidth = widthModule * 3; // Width for labels
    auto keyScaleRow = rightColumn.removeFromTop(rowHeight);
    KeyScaleLabel.setBounds(keyScaleRow.removeFromLeft(labelsColWidth));
    KeySelect.setBounds(keyScaleRow.removeFromLeft(widthModule - gap));
    keyScaleRow.removeFromLeft(gap);
    ScaleSelect.setBounds(keyScaleRow.removeFromLeft(widthModule * 3));

    rightColumn.removeFromTop(gap);

    auto chipLabelRow = rightColumn.removeFromTop(rowHeight);
    ChipClockLabel.setBounds(chipLabelRow.removeFromLeft(labelsColWidth));
    ChipClockSelect.setBounds(chipLabelRow.removeFromLeft(widthModule * 4));

    rightColumn.removeFromTop(gap);

    // Clock frequency slider on its own row
    auto clockFreqRow = rightColumn.removeFromTop(rowHeight);
    clockFrequencyLabel.setBounds(clockFreqRow.removeFromLeft(labelsColWidth));
    clockFrequencySlider.setBounds(clockFreqRow.removeFromLeft(widthModule * 8));

    rightColumn.removeFromTop(gap);

    // A4 frequency slider with label
    auto a4Row = rightColumn.removeFromTop(rowHeight);
    a4FrequencyLabel.setBounds(a4Row.removeFromLeft(labelsColWidth));
    a4FrequencySlider.setBounds(a4Row.removeFromLeft(widthModule * 8));

    rightColumn.removeFromTop(gap);

    // TuningTypeLabel.setBounds(rightColumn.removeFromTop(controlHeight));
    TuningNameLabel.setBounds(rightColumn.removeFromTop(rowHeight));
    ToneEnvSwitchLabel.setBounds(rightColumn.removeFromTop(rowHeight));

    tuningGrid.setBounds(rightColumn);
}

void TuningPreviewComponent::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::background);
    g.setColour(juce::Colours::white);
}

void TuningPreviewComponent::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &viewModel) {

        // Update the tuning name label to reflect the new tuning system
        TuningNameLabel.setText("Tuning Name: " + viewModel.getTuningName(), juce::dontSendNotification);

        // Update A4 frequency slider to reflect current value
        a4FrequencySlider.setValue(viewModel.getA4Frequency(), juce::dontSendNotification);

        // Update clock frequency slider (convert Hz to MHz)
        clockFrequencySlider.setValue(viewModel.getClockFrequency() / 1000000.0, juce::dontSendNotification);

        // Update chip clock selection to reflect current choice
        ChipClockSelect.setSelectedId(viewModel.getCurrentChipClockIndex() + 1, juce::dontSendNotification);

        // Update enabled state of clock frequency controls
        updateClockControlsState();

        // Update tuning table selection
        tuningTableListBox.selectRow(viewModel.getCurrentTuningTableIndex(), false, false);

        // Update scale selection
        updateScaleSelection();

        // Repaint the tuning grid to show updated calculations
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
    viewModel.setCurrentTuningTable(row);
}

// UI setup helpers
void TuningPreviewComponent::setupSlider(Slider& slider, Label& label, const String& labelText,
                                        double min, double max, double step, std::function<void()> callback) {
    slider.setRange(min, max, step);
    slider.setValue(min, juce::dontSendNotification);
    slider.setSliderStyle(Slider::LinearHorizontal);
    slider.setTextBoxStyle(Slider::TextBoxRight, false, 80, 20);
    slider.onValueChange = callback;
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
    ScaleSelect.clear();
    scaleMenuMapping.clear();
    
    // Get all scale categories
    auto categories = Scale::getAllScaleCategories();
    int menuItemId = 1;
    
    // Build the grouped menu structure
    PopupMenu rootMenu;
    
    for (auto category : categories) {
        if (category == Scale::ScaleCategory::UserDefined) {
            continue; // Skip user defined for now
        }
        
        // Add category header (non-selectable)
        rootMenu.addSectionHeader(Scale::getNameForCategory(category));
        
        // Add scales in this category
        auto scalesInCategory = Scale::getAllScaleTypesForCategory(category);
        for (auto scaleType : scalesInCategory) {
            String scaleName = Scale::getNameForType(scaleType);
            rootMenu.addItem(menuItemId, scaleName);
            scaleMenuMapping[menuItemId] = scaleType;
            
            // Also add to ComboBox for text display purposes
            ScaleSelect.addItem(scaleName, menuItemId);
            menuItemId++;
        }
        
        // Add separator after each category (except the last one)
        if (category != categories.back() || categories.back() == Scale::ScaleCategory::UserDefined) {
            rootMenu.addSeparator();
        }
    }
    
    // Replace the ComboBox's root menu with our grouped menu
    *ScaleSelect.getRootMenu() = rootMenu;
    
    // Set up the onChange callback to handle selection
    ScaleSelect.onChange = [this]() {
        int selectedId = ScaleSelect.getSelectedId();
        if (selectedId > 0 && scaleMenuMapping.find(selectedId) != scaleMenuMapping.end()) {
            Scale::ScaleType selectedScale = scaleMenuMapping[selectedId];
            viewModel.setCurrentScale(selectedScale);
        }
    };
}

void TuningPreviewComponent::updateScaleSelection() {
    // Update the ComboBox text to show the current scale
    Scale::ScaleType currentScale = viewModel.getCurrentScale();
    String currentScaleName = Scale::getNameForType(currentScale);
    
    // Find the corresponding menu item ID
    for (const auto& [itemId, scaleType] : scaleMenuMapping) {
        if (scaleType == currentScale) {
            ScaleSelect.setSelectedId(itemId, juce::dontSendNotification);
            break;
        }
    }
    
    // If we can't find it in the mapping, set the text directly
    if (ScaleSelect.getSelectedId() == 0) {
        ScaleSelect.setText(currentScaleName, juce::dontSendNotification);
    }
}

}  // namespace MoTool