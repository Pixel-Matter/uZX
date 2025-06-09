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
    bounds.reduce(20, 20); // Add some padding

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
            outNote = octaveNotes[noteIndex];
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
    auto chipClockLabels = viewModel.getChipClockLabels();
    for (int i = 0; i < chipClockLabels.size(); ++i) {
        ChipClockSelect.addItem(chipClockLabels[i], i + 1);
    }
    ChipClockSelect.setSelectedId(viewModel.getCurrentChipClockIndex() + 1, juce::dontSendNotification);
    ChipClockSelect.onChange = [this]() {
        int selectedId = ChipClockSelect.getSelectedId();
        if (selectedId > 0) {
            viewModel.setChipClockChoice(selectedId - 1); // Convert from 1-based ID to 0-based index
            tuningGrid.repaint(); // Refresh the tuning grid with new calculations
        }
    };

    // Set up A4 frequency slider
    a4FrequencySlider.setRange(220.0, 880.0, 0.1);
    a4FrequencySlider.setValue(viewModel.getA4Frequency(), juce::dontSendNotification);
    a4FrequencySlider.setSliderStyle(Slider::LinearHorizontal);
    a4FrequencySlider.setTextBoxStyle(Slider::TextBoxRight, false, 60, 20);
    a4FrequencySlider.onValueChange = [this]() {
        viewModel.setA4Frequency(a4FrequencySlider.getValue());
    };
    a4FrequencyLabel.setText("A4 Frequency (Hz):", juce::dontSendNotification);

    // Register as a change listener to the view model
    viewModel.addChangeListener(this);
    ScaleLabel.setText("Scale: " + viewModel.getScaleName(), juce::dontSendNotification);
    // TuningTypeLabel.setText("Tuning Type: " + viewModel.getTuningTypeName(), juce::dontSendNotification);
    TuningNameLabel.setText("Tuning Name: " + viewModel.getTuningName(), juce::dontSendNotification);
    // ToneEnvSwitchLabel.setText("Tone Env Switch: " + String(viewModel.isToneEnvSwitchEnabled()), juce::dontSendNotification);

    addAndMakeVisible(ChipClockSelect);
    addAndMakeVisible(a4FrequencySlider);
    addAndMakeVisible(a4FrequencyLabel);
    addAndMakeVisible(ScaleLabel);
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
    auto labelHeight = 20;
    auto sliderHeight = 30;

    ChipClockSelect.setBounds(bounds.removeFromTop(labelHeight));

    // A4 frequency slider with label
    auto a5Row = bounds.removeFromTop(sliderHeight);
    a4FrequencyLabel.setBounds(a5Row.removeFromLeft(120));
    a4FrequencySlider.setBounds(a5Row);

    ScaleLabel.setBounds(bounds.removeFromTop(labelHeight));
    // TuningTypeLabel.setBounds(bounds.removeFromTop(labelHeight));
    TuningNameLabel.setBounds(bounds.removeFromTop(labelHeight));
    ToneEnvSwitchLabel.setBounds(bounds.removeFromTop(labelHeight));

    tuningGrid.setBounds(bounds);
}

void TuningPreviewComponent::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::background);
    g.setColour(juce::Colours::white);
}

void TuningPreviewComponent::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &viewModel) {
        DBG("ChangeListener callback - updating UI with A4 frequency: " << viewModel.getA4Frequency() << " Hz");

        // Update the tuning name label to reflect the new tuning system
        TuningNameLabel.setText("Tuning Name: " + viewModel.getTuningName(), juce::dontSendNotification);

        // Update A4 frequency slider to reflect current value
        a4FrequencySlider.setValue(viewModel.getA4Frequency(), juce::dontSendNotification);

        // Repaint the tuning grid to show updated calculations
        tuningGrid.repaint();
    }
}

}  // namespace MoTool