#include "TuningPreview.h"

#include "../common/LookAndFeel.h"

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
    const int cellSize = 40;

    g.fillAll(Colors::Theme::backgroundAlt);
    g.setColour(juce::Colours::white);
    auto bounds = getLocalBounds();
    bounds.reduce(20, 20); // Add some padding

    bounds.setWidth(cellSize * (cols + 2));
    // center the grid horizontally
    bounds.setX((getWidth() - bounds.getWidth()) / 2);

    // auto grid = bounds.removeFromTop(cellSize);
    {
        auto gridCell = bounds.removeFromTop(cellSize).withSize(cellSize, cellSize);

        g.drawText("Oct", gridCell, juce::Justification::centred, true);
        gridCell.translate(cellSize, 0);
        for (const auto& column : viewModel.getColumnNoteNames()) {
            g.drawText(column.name, gridCell, juce::Justification::centred, true);
            gridCell.translate(cellSize, 0);
        }
    }

    const auto octaves = viewModel.getOctaveRange();
    for (auto octave = octaves.getStart(); octave < octaves.getEnd(); ++octave) {
        auto gridCell = bounds.removeFromTop(cellSize).withSize(cellSize, cellSize);

        g.setColour(juce::Colours::white);
        g.drawText(String::formatted("%d", octave), gridCell, juce::Justification::centred, true);
        gridCell.translate(cellSize, 0);
        for (const auto& note : viewModel.getOctaveNotes(octave)) {
            g.setColour(Colors::Theme::background);
            g.fillRect(gridCell.reduced(2, 2));
            g.setColour(juce::Colours::white);
            g.drawText(note.name, gridCell, juce::Justification::centred, true);
            gridCell.translate(cellSize, 0);
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

    ChipClockLabel.setText("Chip Clock: " + String(viewModel.getChipClock()), juce::dontSendNotification);
    // ScaleLabel.setText("Scale: " + viewModel.getScaleName(), juce::dontSendNotification);
    // TuningTypeLabel.setText("Tuning Type: " + viewModel.getTuningTypeName(), juce::dontSendNotification);
    // TuningNameLabel.setText("Tuning Name: " + viewModel.getTuningName(), juce::dontSendNotification);
    // ToneEnvSwitchLabel.setText("Tone Env Switch: " + String(viewModel.isToneEnvSwitchEnabled()), juce::dontSendNotification);

    addAndMakeVisible(ChipClockLabel);
    addAndMakeVisible(ScaleLabel);
    addAndMakeVisible(TuningTypeLabel);
    addAndMakeVisible(TuningNameLabel);
    addAndMakeVisible(ToneEnvSwitchLabel);
}

TuningPreviewComponent::~TuningPreviewComponent() {
}

void TuningPreviewComponent::resized() {
    auto bounds = getLocalBounds();
    auto labelHeight = 20;

    ChipClockLabel.setBounds(bounds.removeFromTop(labelHeight));
    ScaleLabel.setBounds(bounds.removeFromTop(labelHeight));
    TuningTypeLabel.setBounds(bounds.removeFromTop(labelHeight));
    TuningNameLabel.setBounds(bounds.removeFromTop(labelHeight));
    ToneEnvSwitchLabel.setBounds(bounds.removeFromTop(labelHeight));

    tuningGrid.setBounds(bounds);
}

void TuningPreviewComponent::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::background);
    g.setColour(juce::Colours::white);
}

}  // namespace MoTool