#include "TuningPickerComponent.h"

#include <array>

#include "../../models/Ids.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

//==============================================================================
/** Bars of the folded pitch histogram with a line per grid note and note names below. */
class TuningPickerComponent::HistogramView : public Component
{
public:
    static constexpr int binsPerSemitone = 5;

    /** inKey marks the pitch classes of the chosen key's scale; they get the brighter
        grid lines and labels, so out-of-key peaks stand out at a glance. */
    void set(std::vector<float> newBins, int newTonic, std::array<bool, 12> inKey) {
        bins_ = std::move(newBins);
        tonic_ = newTonic;
        inKey_ = inKey;
        repaint();
    }

    void paint(Graphics& g) override {
        auto area = getLocalBounds().toFloat();
        auto labels = area.removeFromBottom(14.0f);
        g.setColour(Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(area, 3.0f);
        if (bins_.empty())
            return;

        const float binWidth = area.getWidth() / static_cast<float>(bins_.size());
        g.setFont(Font(FontOptions(11.0f)));
        for (int pc = 0; pc < 12; ++pc) {
            const float x = area.getX() + static_cast<float>(pc * binsPerSemitone) * binWidth + binWidth * 0.5f;
            const bool isTonic = pc == tonic_;
            const bool inKey = inKey_[static_cast<size_t>(pc)];
            g.setColour(Colours::white.withAlpha(isTonic ? 0.7f : inKey ? 0.4f : 0.1f));
            g.drawVerticalLine(roundToInt(x), area.getY(), area.getBottom());
            g.setColour(Colours::white.withAlpha(isTonic ? 0.95f : inKey ? 0.75f : 0.3f));
            g.drawText(String::fromUTF8(Scale::Tonic(pc).getLongLabel().data()),
                       Rectangle<float>(x - 12.0f, labels.getY(), 24.0f, labels.getHeight()),
                       Justification::centred,
                       false);
        }
        for (size_t i = 0; i < bins_.size(); ++i) {
            const float h = bins_[i] * (area.getHeight() - 4.0f);
            if (h <= 0.0f)
                continue;
            const float x = area.getX() + static_cast<float>(i) * binWidth;
            const bool onGrid = i % binsPerSemitone == 0;
            g.setColour((onGrid ? Colours::white : Colors::Theme::warning).withAlpha(0.85f));
            g.fillRect(x + 0.5f, area.getBottom() - 2.0f - h, jmax(1.0f, binWidth - 1.0f), h);
        }
    }

private:
    std::vector<float> bins_;
    int tonic_ = -1;
    std::array<bool, 12> inKey_{};
};

//==============================================================================
TuningPickerComponent::TuningPickerComponent(PsgClip::Ptr clip)
    : clip_(std::move(clip)), histogram_(clip_->buildPitchHistogram()),
      histogramView_(std::make_unique<HistogramView>()) {
    const auto& detection = clip_->getTuningDetection();

    int id = 1;
    for (const auto& candidate : detection.tables)
        tableBox_.addItem(String(candidate.table.getShortLabel().data()) + "  "
                              + String(roundToInt(candidate.share * 100)) + "%",
                          id++);
    tableBox_.addItem(getEqualTemperamentLabel(detection.equalTemperament.a4Frequency) + "  "
                          + String(roundToInt(detection.equalTemperament.share * 100)) + "%",
                      equalTemperamentItemId);
    tableBox_.onChange = [this] {
        const int selected = tableBox_.getSelectedId();
        if (selected == equalTemperamentItemId)
            clip_->chooseEqualTemperament(clip_->getTuningDetection().equalTemperament.a4Frequency);
        else if (selected > 0)
            clip_->chooseTuningTable(clip_->getTuningDetection().tables[static_cast<size_t>(selected - 1)].table);
    };

    id = 1;
    for (const auto& key : detection.keys)
        keyBox_.addItem(key.getName() + "  " + String(key.correlation, 2), id++);
    keyBox_.onChange = [this] {
        const int selected = keyBox_.getSelectedId();
        if (selected > 0)
            clip_->chooseKey(clip_->getTuningDetection().keys[static_cast<size_t>(selected - 1)]);
    };

    detectedButton_.onClick = [this] { clip_->clearTuningChoice(); };

    for (auto* c : std::initializer_list<Component*>{
             &tableLabel_, &tableBox_, &keyLabel_, &keyBox_, &detectedButton_, histogramView_.get()})
        addAndMakeVisible(c);

    clip_->state.addListener(this);
    refresh();
}

TuningPickerComponent::~TuningPickerComponent() {
    clip_->state.removeListener(this);
}

void TuningPickerComponent::show(PsgClip& clip) {
    DialogWindow::LaunchOptions o;
    o.dialogTitle = "Tuning and key: " + clip.getName();
    o.dialogBackgroundColour = Colors::Theme::background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.useBottomRightCornerResizer = true;
    auto* content = new TuningPickerComponent(PsgClip::Ptr(&clip));
    content->setSize(560, 340);
    o.content.setOwned(content);
    o.launchAsync();
}

void TuningPickerComponent::resized() {
    auto area = getLocalBounds().reduced(10);
    auto row = area.removeFromTop(24);
    tableLabel_.setBounds(row.removeFromLeft(90));
    detectedButton_.setBounds(row.removeFromRight(110));
    tableBox_.setBounds(row.reduced(4, 0));
    area.removeFromTop(6);
    row = area.removeFromTop(24);
    keyLabel_.setBounds(row.removeFromLeft(90));
    keyBox_.setBounds(row.reduced(4, 0));
    area.removeFromTop(10);
    histogramView_->setBounds(area);
}

void TuningPickerComponent::refresh() {
    const auto& detection = clip_->getTuningDetection();
    const auto tuning = clip_->getTuning();

    // Reflect the state without re-triggering the onChange handlers.
    if (clip_->state.hasProperty(IDs::clipTuningA4) || (!tuning.tableChosen && detection.usesEqualTemperament)) {
        tableBox_.setSelectedId(equalTemperamentItemId, dontSendNotification);
    } else {
        const auto chosen = tuning.tableChosen
                                ? juce::VariantConverter<BuiltinTuningType>::fromVar(clip_->state[IDs::clipTuningTable])
                                : detection.tables.front().table;
        for (size_t i = 0; i < detection.tables.size(); ++i)
            if (detection.tables[i].table == chosen)
                tableBox_.setSelectedId(static_cast<int>(i) + 1, dontSendNotification);
    }
    for (size_t i = 0; i < detection.keys.size(); ++i)
        if (detection.keys[i].tonic == tuning.key.tonic && detection.keys[i].minor == tuning.key.minor)
            keyBox_.setSelectedId(static_cast<int>(i) + 1, dontSendNotification);

    detectedButton_.setEnabled(tuning.tableChosen || tuning.keyChosen);
    const Scale scale(tuning.key.getScaleType());
    const int tonic = static_cast<int>(tuning.key.tonic);
    std::array<bool, 12> inKey{};
    for (int pc = 0; pc < 12; ++pc)
        inKey[static_cast<size_t>(pc)] = scale.isIntervalInScale((pc - tonic + 12) % 12);
    histogramView_->set(
        makeFoldedPitchHistogram(histogram_, tuning.grid, HistogramView::binsPerSemitone), tonic, inKey);
}

void TuningPickerComponent::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) {
    if (property == IDs::clipTuningTable || property == IDs::clipTuningA4 || property == IDs::clipKeyTonic
        || property == IDs::clipKeyMinor)
        refresh();
}

}  // namespace MoTool
