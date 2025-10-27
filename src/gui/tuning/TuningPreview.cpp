#include "TuningPreview.h"

#include "../common/LookAndFeel.h"
#include "../../util/Helpers.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace MoTool {

static std::vector<std::pair<double, String>> makeChipClockPresets() {
    std::vector<std::pair<double, String>> choices;
    choices.reserve(static_cast<size_t>(ChipClockChoice::size()) - 1);

    auto entries = ChipClockChoice::getClockEntries();
    for (int idx = 0; idx < static_cast<int>(ChipClockChoice::size()); ++idx) {
        auto choice = ChipClockChoice(idx);
        if (entries[idx] != 0)
            choices.emplace_back(entries[idx] / MHz, choice.getLongLabel().data());
    }
    return choices;
}

//================================================================================
TuningPreviewComponent::ChipClock::ChipClock(TuningPreviewComponent& c, TuningViewModel& vm)
    : comboBinding(select, vm.selectedParams.clockFrequencyMhz, makeChipClockPresets())
{
    label.setText("Chip clock", juce::dontSendNotification);
    label.setJustificationType(Justification::centredLeft);

    c.addAndMakeVisible(label);
    c.addAndMakeVisible(select);
}

void TuningPreviewComponent::ChipClock::layout(juce::Rectangle<int>& area) {
    label.setBounds(area.removeFromTop(rowHeight));

    auto row = area.removeFromTop(rowHeight);
    select.setBounds(row.removeFromLeft(moduleWidth * 5));
}

int TuningPreviewComponent::ChipClock::getHeight() const {
    return rowHeight * 2; // 1 for label, 1 for controls
}

//================================================================================
TuningPreviewComponent::KeyScale::KeyScale(TuningPreviewComponent& c, TuningViewModel& vm)
    : keySelectBinding(keySelect, vm.selectedParams.tonic)
    , scaleSelectBinding(scaleSelect, vm.selectedParams.scaleType)
{
    label.setText("Scale", juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);

    c.addAndMakeVisible(label);
    c.addAndMakeVisible(keySelect);
    c.addAndMakeVisible(scaleSelect);
}

TuningPreviewComponent::KeyScale::ScaleSelectBinding::ScaleSelectBinding(ComboBox& comboBox, ParameterValue<Scale::ScaleType>& parameter)
    : combo(comboBox)
    , parameterValue(parameter)
{
    parameterValue.addListener(this);
    fillItems();
    refreshFromSource();
}

TuningPreviewComponent::KeyScale::ScaleSelectBinding::~ScaleSelectBinding() {
    parameterValue.removeListener(this);
}

void TuningPreviewComponent::KeyScale::ScaleSelectBinding::fillItems() {
    juce::ScopedValueSetter<bool> sv(updating, true);
    combo.clear();
    itemMap.clear();

    auto categories = Scale::getAllScaleCategories();
    const auto hasCategories = !categories.empty();
    auto lastCategory = hasCategories
        ? categories.back()
        : Scale::ScaleCategory(Scale::ScaleCategory::Enum::Diatonic);
    int itemId = 1;

    const auto userCategory = Scale::ScaleCategory(Scale::ScaleCategory::Enum::User);
    const auto userScale = Scale::ScaleType(Scale::ScaleType::Enum::User);

    for (auto category : categories) {
        if (category == userCategory)
            continue;

        combo.addSectionHeading(Scale::getNameForCategory(category));
        auto scalesInCategory = Scale::getAllScaleTypesForCategory(category);

        for (auto scaleType : scalesInCategory) {
            if (scaleType == userScale)
                continue;

            combo.addItem(Scale::getNameForType(scaleType), itemId++);
            itemMap.push_back(scaleType);
        }

        if (category != lastCategory || lastCategory == userCategory)
            combo.addSeparator();
    }

    combo.onChange = [this] {
        if (updating)
            return;

        const auto selectedId = combo.getSelectedId();
        if (selectedId <= 0 || selectedId > static_cast<int>(itemMap.size()))
            return;

        juce::ScopedValueSetter<bool> sv(updating, true);
        parameterValue.setStoredValue(itemMap[static_cast<size_t>(selectedId - 1)]);
    };
}

void TuningPreviewComponent::KeyScale::ScaleSelectBinding::refreshFromSource() {
    if (updating)
        return;

    const auto current = parameterValue.getStoredValue();
    const auto it = std::find(itemMap.begin(), itemMap.end(), current);

    juce::ScopedValueSetter<bool> sv(updating, true);
    if (it != itemMap.end()) {
        const auto index = static_cast<int>(std::distance(itemMap.begin(), it));
        combo.setSelectedId(index + 1, juce::dontSendNotification);
    } else if (!itemMap.empty()) {
        combo.setSelectedId(1, juce::dontSendNotification);
    } else {
        combo.setSelectedId(0, juce::dontSendNotification);
    }
}

void TuningPreviewComponent::KeyScale::ScaleSelectBinding::valueChanged(Value& value) {
    juce::ignoreUnused(value);
    refreshFromSource();
}

void TuningPreviewComponent::KeyScale::layout(juce::Rectangle<int>& area) {
    label.setBounds(area.removeFromTop(rowHeight));

    auto row = area.removeFromTop(rowHeight);
    keySelect.setBounds(row.removeFromLeft(moduleWidth * 3 / 2));
    row.removeFromLeft(gap);
    scaleSelect.setBounds(row.removeFromLeft(moduleWidth * 7 / 2 - gap));
}

int TuningPreviewComponent::KeyScale::getHeight() const {
    return rowHeight * 2; // 1 for label, 1 for controls
}

//================================================================================
TuningPreviewComponent::ReferenceTuning::ReferenceTuning(TuningPreviewComponent& c, TuningViewModel& vm)
    : binding(select, vm.selectedParams.tuningType)
{
    label.setText(binding.endpoint().getDescription(), juce::dontSendNotification);

    c.addAndMakeVisible(label);
    c.addAndMakeVisible(select);
}

void TuningPreviewComponent::ReferenceTuning::layout(juce::Rectangle<int>& area) {
    label.setBounds(area.removeFromTop(rowHeight));
    auto row = area.removeFromTop(rowHeight);
    select.setBounds(row.removeFromLeft(moduleWidth * 5));
}

int TuningPreviewComponent::ReferenceTuning::getHeight() const {
    return rowHeight * 2; // 1 for label, 1 for controls
}

//================================================================================
TuningPreviewComponent::A4Frequency::A4Frequency(TuningPreviewComponent& c, TuningViewModel& vm)
    : param(vm.selectedParams.a4Frequency)
    , binding(slider, param)
{
    label.setText(binding.endpoint().getDescription(), juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);

    slider.setSliderStyle(Slider::LinearHorizontal);
    slider.setTextBoxStyle(Slider::TextBoxRight, false, 80, 20);

    unitsLabel.setText(binding.endpoint().getUnits(), juce::dontSendNotification);
    unitsLabel.setJustificationType(juce::Justification::centredLeft);

    c.addAndMakeVisible(label);
    c.addAndMakeVisible(slider);
    c.addAndMakeVisible(unitsLabel);
}

int TuningPreviewComponent::A4Frequency::getHeight() const {
    return rowHeight * 2; // 1 for label, 1 for controls
}

void TuningPreviewComponent::A4Frequency::layout(juce::Rectangle<int>& area) {
    label.setBounds(area.removeFromTop(rowHeight));

    auto row = area.removeFromTop(rowHeight);
    slider.setBounds(row.removeFromLeft(moduleWidth * 7));
    unitsLabel.setBounds(row);
}

//================================================================================
TuningPreviewComponent::PlayControls::PlayControls(TuningPreviewComponent& c, TuningViewModel& vm)
    : envelopeShapeBinding(envelopeShapeSelect, vm.selectedParams.envelopeShape)
    , envelopeModeBinding(modulationModeSelect, vm.selectedParams.envInterval)
{
    label.setText("Play mode", juce::dontSendNotification);

    playChordsCheckBox.setButtonText("Chords");
    playChordsCheckBox.getToggleStateValue().referTo(vm.selectedParams.playChords.getPropertyAsValue());

    playToneCheckBox.setButtonText("Tone");
    playToneCheckBox.getToggleStateValue().referTo(vm.selectedParams.playTone.getPropertyAsValue());

    retriggerToneCheckBox.setButtonText("Retrigger");
    retriggerToneCheckBox.getToggleStateValue().referTo(vm.selectedParams.retriggerTone.getPropertyAsValue());

    playEnvelopeCheckBox.setButtonText("Envelope");
    playEnvelopeCheckBox.getToggleStateValue().referTo(vm.selectedParams.playEnvelope.getPropertyAsValue());

    c.addAndMakeVisible(label);
    c.addAndMakeVisible(playChordsCheckBox);
    c.addAndMakeVisible(playToneCheckBox);
    c.addAndMakeVisible(retriggerToneCheckBox);
    c.addAndMakeVisible(playEnvelopeCheckBox);
    c.addAndMakeVisible(envelopeShapeSelect);
    c.addAndMakeVisible(modulationModeSelect);
}

void TuningPreviewComponent::PlayControls::layout(juce::Rectangle<int>& area) {
    // Play controls row
    label.setBounds(area.removeFromTop(rowHeight));

    auto row1 = area.removeFromTop(rowHeight);
    playToneCheckBox.setBounds(row1.removeFromLeft(moduleWidth * 2 - gap));
    row1.removeFromLeft(gap);
    playChordsCheckBox.setBounds(row1.removeFromLeft(moduleWidth * 2));

    // Envelope controls row
    area.removeFromTop(gap);
    playEnvelopeCheckBox.setBounds(area.removeFromTop(rowHeight));

    auto row3 = area.removeFromTop(rowHeight);
    envelopeShapeSelect.setBounds(row3.removeFromLeft(moduleWidth * 2));
    row3.removeFromLeft(gap);
    modulationModeSelect.setBounds(row3.removeFromLeft(moduleWidth * 3));
}


//================================================================================
TuningPreviewComponent::TuningPreviewComponent(TuningViewModel& vm, TuningPlayer& tp)
    : viewModel(vm)
    , tuningPlayer(tp)
    , tuningGrid(viewModel, tuningPlayer)
{
    setOpaque(true);

    setupTuningTableControls();
    setupTuningGrid();
    setupExportButton();

    updateControlsState();
    viewModel.addChangeListener(this);
    // tuningNameLabel.setText(viewModel.getTuningDescription(), juce::dontSendNotification);
    // addAndMakeVisible(tuningNameLabel);
}

TuningPreviewComponent::~TuningPreviewComponent() {
    viewModel.removeChangeListener(this);
    viewModel.selectedParams.tuningTable.removeListener(this);
}


void TuningPreviewComponent::resized() {
    auto bounds = getLocalBounds().reduced(20, 20);
    bounds.removeFromTop(-8);

    // Left column: Tuning table selection
    auto leftColumn = bounds.removeFromLeft(moduleWidth * 4);
    bounds.removeFromLeft(20);

    tuningTableLabel.setBounds(leftColumn.removeFromTop(rowHeight));
    tuningsListBox.setBounds(leftColumn);

    // Right column: Controls and grid
    layoutControlSections(bounds);

    // Reserve space for export button
    auto exportArea = bounds.removeFromBottom(rowHeight);
    bounds.removeFromBottom(gap);

    tuningGrid.setBounds(bounds);
    exportButton.setBounds(exportArea.removeFromLeft(moduleWidth * 2));
}

void TuningPreviewComponent::layoutControlSections(juce::Rectangle<int>& area) {
    auto row1 = area.removeFromTop(jmax(a4Frequency.getHeight(), tuning.getHeight()));
    auto leftColumn = row1.removeFromLeft(moduleWidth * 6 - gap);
    tuning.layout(leftColumn);
    // row1.removeFromLeft(gap);
    a4Frequency.layout(row1);

    area.removeFromTop(gap);
    auto row2 = area.removeFromTop(keyScale.getHeight() + chipClock.getHeight() + gap);
    leftColumn = row2.removeFromLeft(moduleWidth * 6 - gap);

    keyScale.layout(leftColumn);
    leftColumn.removeFromTop(gap);
    chipClock.layout(leftColumn);

    // formArea.removeFromTop(row1.getHeight() + gap);
    playControls.layout(row2);

    area.removeFromTop(gap * 2);
}

void TuningPreviewComponent::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::background);
}

void TuningPreviewComponent::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &viewModel) {
        // DBG("TuningPreviewComponent::changeListenerCallback");
        // tuningNameLabel.setText("Tuning Name: " + viewModel.getTuningDescription(), juce::dontSendNotification);
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
    viewModel.selectedParams.tuningTable.setStoredValue(BuiltinTuningType(row));
}

void TuningPreviewComponent::updateControlsState() {
    // Update envelope controls
    playControls.playChordsCheckBox.setEnabled(viewModel.isToneEnabled());
    playControls.retriggerToneCheckBox.setEnabled(viewModel.isToneEnabled());
    playControls.envelopeShapeSelect.setEnabled(viewModel.isEnvelopeEnabled());
    playControls.modulationModeSelect.setEnabled(viewModel.isModulationEnabled());
}

void TuningPreviewComponent::setupTuningTableControls() {
    tuningTableLabel.setText("Tuning Tables:", juce::dontSendNotification);
    tuningsListBox.setModel(this);
    tuningsListBox.setMultipleSelectionEnabled(false);
    tuningsListBox.selectRow(viewModel.selectedParams.tuningTable.getStoredValueAs<int>(), false, false);
    viewModel.selectedParams.tuningTable.addListener(this);

    addAndMakeVisible(tuningTableLabel);
    addAndMakeVisible(tuningsListBox);
}


void TuningPreviewComponent::setupTuningGrid() {
    tooltipWindow = std::make_unique<MoTooltipWindow>(nullptr, 750);
    tuningGrid.tooltipWindow = tooltipWindow.get();
    addAndMakeVisible(tuningGrid);
}

void TuningPreviewComponent::setupExportButton() {
    exportButton.setButtonText("Export to CSV");
    exportButton.onClick = [this]() { handleExportButtonClick(); };
    addAndMakeVisible(exportButton);
}

void TuningPreviewComponent::handleExportButtonClick() {
    String defaultFilename = viewModel.getDefaultExportFilename();
    File lastExportDir = Helpers::getLastCsvExportDirectory();
    File defaultFile = lastExportDir.getChildFile(defaultFilename);

    FileChooser fileChooser("Save Tuning Data as CSV", defaultFile, "*.csv");

    if (fileChooser.browseForFileToSave(true)) {
        File selectedFile = fileChooser.getResult();
        String csvData = viewModel.exportToCSV();

        if (selectedFile.replaceWithText(csvData)) {
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
}

// Value::Listener implementation for ListBox and other custom sync
void TuningPreviewComponent::valueChanged(Value& value) {
    // DBG("TuningPreviewComponent::valueChanged");
    if (value.refersToSameSourceAs(viewModel.selectedParams.tuningTable.getPropertyAsValue())) {
        auto newIndex = viewModel.selectedParams.tuningTable.getStoredValueAs<int>();
        tuningsListBox.selectRow(newIndex, false, false);
    }
}

}  // namespace MoTool
