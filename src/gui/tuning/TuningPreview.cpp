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

void TuningPreviewGrid::paint(juce::Graphics& g) {
    const int cols = viewModel.getNumColumns();
    // const int rows = viewModel.getNumRows();
    const int cellWidth = 56;
    const int cellHeight = 32;

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
            auto offtune = jlimit(-0.5, 0.5, note.offtune / 100.0); // Normalize offtune to -0.5 to 0.5 range

            auto noteTextColor = Colors::Theme::textPrimary;
            if (offtune > 0.05) {
                noteTextColor = noteTextColor.interpolatedWith(juce::Colours::blue, offtune * 2);
            } else if (offtune < 0.05) {
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
            // tuner ticks
            auto tickSize = 6;
            g.setColour(Colors::Theme::surfaceAlt);
            const int cellCenter = gridCell.getX() + (gridCell.getWidth() - 2) / 2;
            g.fillRect(cellCenter, gridCell.getY() - 2, 2, 4);
            g.fillRect(cellCenter, gridCell.getBottom() - 2, 2, 4);

            g.setColour(Colors::Theme::surfaceAlt);
            float offX = jmap<float>((float) offtune, -0.5f, 0.5f, (float) gridCell.getX(), (float) gridCell.getRight() - 2.0f);
            if (note.offtune >= -50 && note.offtune <= 50) {
                g.fillRect(offX, (float) gridCell.getY() + 2, 2.0f, (float) tickSize);
                g.fillRect(offX, (float) gridCell.getBottom() - 2.0f - (float) tickSize, 2.0f, (float) tickSize);
            }

            auto textCell = gridCell;
            auto text = String::formatted("%d", note.period);
            auto textWidth = static_cast<int>(std::ceil(g.getCurrentFont().getStringWidthFloat(text)));
            textCell.setWidth(textWidth);
            textCell.setCentre(offX, textCell.getCentreY());
            textCell = textCell.constrainedWithin(gridCell.reduced(2, 0));

            g.setColour(noteTextColor);
            g.drawText(text, textCell, juce::Justification::centred, false);
            // g.drawText(String::formatted("%.2f", note.frequency), gridCell.withTrimmedRight(8), juce::Justification::right, true);

            gridCell.translate(cellWidth, 0);
        }
        // bounds.removeFromTop(1); // Add a line between rows
    }
}

//================================================================================
TuningPreviewComponent::TuningPreviewComponent()
    : viewModel(TuningViewModel())
    , tuningGrid(viewModel)
{
    setOpaque(true);
    addAndMakeVisible(tuningGrid);

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
    
    // Set up callback to update UI when tuning system changes
    viewModel.onTuningSystemChanged = [this]() {
        DBG("UI callback triggered - updating tuning name and repainting grid");
        // Update the tuning name label to reflect the new tuning system
        TuningNameLabel.setText("Tuning Name: " + viewModel.getTuningName(), juce::dontSendNotification);
        
        // Repaint the tuning grid to show updated calculations
        tuningGrid.repaint();
    };
    ScaleLabel.setText("Scale: " + viewModel.getScaleName(), juce::dontSendNotification);
    // TuningTypeLabel.setText("Tuning Type: " + viewModel.getTuningTypeName(), juce::dontSendNotification);
    TuningNameLabel.setText("Tuning Name: " + viewModel.getTuningName(), juce::dontSendNotification);
    // ToneEnvSwitchLabel.setText("Tone Env Switch: " + String(viewModel.isToneEnvSwitchEnabled()), juce::dontSendNotification);

    addAndMakeVisible(ChipClockSelect);
    addAndMakeVisible(ScaleLabel);
    // addAndMakeVisible(TuningTypeLabel);
    addAndMakeVisible(TuningNameLabel);
    addAndMakeVisible(ToneEnvSwitchLabel);
}

TuningPreviewComponent::~TuningPreviewComponent() {
}

void TuningPreviewComponent::resized() {
    auto bounds = getLocalBounds();
    auto labelHeight = 20;

    ChipClockSelect.setBounds(bounds.removeFromTop(labelHeight));
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

}  // namespace MoTool