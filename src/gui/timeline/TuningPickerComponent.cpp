#include "TuningPickerComponent.h"

#include <algorithm>
#include <array>

#include "../../models/Ids.h"
#include "../common/LookAndFeel.h"
#include "../common/Utilities.h"

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
TuningPickerComponent::TuningPickerComponent(PsgClip& clip)
    : clip_(clip), clipState_(clip.state), histogramView_(std::make_unique<HistogramView>()) {
    for (size_t i = 0; i < std::size(tuningClocks); ++i)
        clockBox_.addItem(String(tuningClocks[i].name) + "  " + String(tuningClocks[i].hz / 1.0e6, 4) + " MHz",
                          static_cast<int>(i) + 1);
    clockBox_.onChange = [this] {
        auto* c = clip_.get();
        const int selected = clockBox_.getSelectedId();
        if (c != nullptr && c->getTuning().fit && selected > 0 && selected <= static_cast<int>(std::size(tuningClocks)))
            c->chooseFit(atClock(*c->getTuning().fit, tuningClocks[static_cast<size_t>(selected - 1)].hz));
    };
    tableBox_.onChange = [this] {
        auto* c = clip_.get();
        const int selected = tableBox_.getSelectedId();
        if (c == nullptr)
            return;
        if (selected >= firstFitItemId && selected < firstFitItemId + static_cast<int>(listedFits_.size()))
            c->chooseFit(listedFits_[static_cast<size_t>(selected - firstFitItemId)]);
        else if (selected > 0 && selected <= static_cast<int>(listedTables_.size()))
            c->chooseTuningTable(listedTables_[static_cast<size_t>(selected - 1)]);
    };
    keyBox_.onChange = [this] {
        auto* c = clip_.get();
        const int selected = keyBox_.getSelectedId();
        if (c != nullptr && selected > 0 && selected <= static_cast<int>(listedKeys_.size()))
            c->chooseKey(listedKeys_[static_cast<size_t>(selected - 1)]);
    };
    detectedButton_.onClick = [this] {
        if (auto* c = clip_.get())
            c->clearTuningChoice();
    };

    for (auto* c : std::initializer_list<Component*>{
             &tableLabel_, &tableBox_, &detectedButton_, &clockLabel_, &clockBox_, &a4Label_, &keyLabel_, &keyBox_,
             histogramView_.get()})
        addAndMakeVisible(c);

    clip.addSelectableListener(this);
    clipState_.addListener(this);
    handleAsyncUpdate();
}

TuningPickerComponent::~TuningPickerComponent() {
    cancelPendingUpdate();
    clipState_.removeListener(this);
    if (auto* c = clip_.get())
        c->removeSelectableListener(this);
}

void TuningPickerComponent::show(PsgClip& clip) {
    DialogWindow::LaunchOptions o;
    o.dialogTitle = "Tuning and key: " + clip.getName();
    o.dialogBackgroundColour = Colors::Theme::background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.useBottomRightCornerResizer = true;
    auto* content = new TuningPickerComponent(clip);
    content->setSize(560, 370);
    o.content.setOwned(content);
    Helpers::launchDialogAsync(o);
}

void TuningPickerComponent::selectableObjectAboutToBeDeleted(te::Selectable*) {
    // The clip is going (its Edit may be going too): drop everything that refers to it
    // now, and close the window once this call has returned.
    cancelPendingUpdate();
    clipState_.removeListener(this);
    clip_ = nullptr;
    MessageManager::callAsync([safeThis = Component::SafePointer<TuningPickerComponent>(this)] {
        if (safeThis != nullptr)
            safeThis->close();
    });
}

void TuningPickerComponent::close() {
    if (auto* window = findParentComponentOfClass<DialogWindow>())
        window->exitModalState(0);  // launchAsync windows delete themselves on exit
}

void TuningPickerComponent::resized() {
    auto area = getLocalBounds().reduced(10);
    auto row = area.removeFromTop(24);
    tableLabel_.setBounds(row.removeFromLeft(90));
    detectedButton_.setBounds(row.removeFromRight(110));
    tableBox_.setBounds(row.reduced(4, 0));
    area.removeFromTop(6);
    row = area.removeFromTop(24);
    clockLabel_.setBounds(row.removeFromLeft(90));
    clockBox_.setBounds(row.removeFromLeft(220).reduced(4, 0));
    a4Label_.setBounds(row);
    area.removeFromTop(6);
    row = area.removeFromTop(24);
    keyLabel_.setBounds(row.removeFromLeft(90));
    keyBox_.setBounds(row.reduced(4, 0));
    area.removeFromTop(10);
    histogramView_->setBounds(area);
}

void TuningPickerComponent::handleAsyncUpdate() {
    auto* c = clip_.get();
    if (c == nullptr)
        return;
    if (listedDetectionRevision_ != c->getTuningDetectionRevision())
        rebuildLists(*c);
    refresh();
}

void TuningPickerComponent::rebuildLists(const PsgClip& clip) {
    const auto& detection = clip.getTuningDetection();
    listedDetectionRevision_ = clip.getTuningDetectionRevision();

    // Detection ranks one tracker table per group of near-identical grids. The tables it
    // merged away (PT #0, a semitone from PT #3) are listed after, so a tune written for
    // them can still be named right. The fitted tunings follow, each at its guessed clock.
    std::vector<TuningCandidate> candidates = detection.tables;
    for (int i = 0; i < static_cast<int>(BuiltinTuningType::size()); ++i) {
        const BuiltinTuningType table(i);
        if (isTrackerTable(table)
            && std::none_of(candidates.begin(), candidates.end(), [&](const auto& c) { return c.table == table; }))
            candidates.push_back({table, detection.histogram.getShareOnGrid(getBuiltinPeriodGrid(table))});
    }
    listedTables_.clear();
    tableBox_.clear(dontSendNotification);
    for (const auto& candidate : candidates) {
        listedTables_.push_back(candidate.table);
        tableBox_.addItem(String(candidate.table.getShortLabel().data()) + "  "
                              + String(roundToInt(candidate.share * 100)) + "%",
                          static_cast<int>(listedTables_.size()));
    }
    tableBox_.addSeparator();
    listedFits_ = detection.fits;
    for (size_t i = 0; i < listedFits_.size(); ++i)
        tableBox_.addItem(getListedFitText(i), firstFitItemId + static_cast<int>(i));

    listedKeys_.clear();  // scores changed with the data: refresh() lists them again
}

String TuningPickerComponent::getListedFitText(size_t index) const {
    const auto& fit = listedFits_[index];
    return getFitLabel(fit) + "  " + String(roundToInt(fit.share * 100)) + "%";
}

void TuningPickerComponent::rebuildKeyList(const std::vector<DetectedKey>& keys) {
    listedKeys_ = keys;
    keyBox_.clear(dontSendNotification);
    for (size_t i = 0; i < listedKeys_.size(); ++i)
        keyBox_.addItem(listedKeys_[i].getName() + "  " + String(listedKeys_[i].correlation, 2),
                        static_cast<int>(i) + 1);
}

void TuningPickerComponent::refresh() {
    auto* c = clip_.get();
    if (c == nullptr)
        return;
    const auto& detection = c->getTuningDetection();
    const auto& tuning = c->getTuning();

    // Select the tuning in effect. A chosen fit keeps its own A4 and clock, which may differ
    // from the detected fit of its system, so that item shows them.
    for (size_t i = 0; i < listedFits_.size(); ++i) {
        const int id = firstFitItemId + static_cast<int>(i);
        const bool inEffect = tuning.fit && tuning.fit->system == listedFits_[i].system;
        tableBox_.changeItemText(id, inEffect && tuning.tableChosen ? tuning.tableLabel : getListedFitText(i));
        if (inEffect)
            tableBox_.setSelectedId(id, dontSendNotification);
    }
    if (tuning.table) {
        for (size_t i = 0; i < listedTables_.size(); ++i)
            if (listedTables_[i] == *tuning.table)
                tableBox_.setSelectedId(static_cast<int>(i) + 1, dontSendNotification);
    }

    // The clock only names a fitted tuning's notes; a tracker table is the same at any clock.
    clockBox_.setEnabled(tuning.fit.has_value());
    clockBox_.setSelectedId(0, dontSendNotification);
    a4Label_.setText({}, dontSendNotification);
    if (tuning.fit) {
        for (size_t i = 0; i < std::size(tuningClocks); ++i)
            if (exactlyEqual(tuningClocks[i].hz, tuning.fit->chipClock))
                clockBox_.setSelectedId(static_cast<int>(i) + 1, dontSendNotification);
        a4Label_.setText("A4 = " + String(tuning.fit->a4Frequency, 1) + " Hz", dontSendNotification);
    }

    // Keys are ranked on the grid in effect, so choosing a table can reorder them.
    const bool sameRanking =
        listedKeys_.size() == tuning.keys.size()
        && std::equal(listedKeys_.begin(), listedKeys_.end(), tuning.keys.begin(), [](const auto& a, const auto& b) {
               return a.tonic == b.tonic && a.minor == b.minor;
           });
    if (!sameRanking)
        rebuildKeyList(tuning.keys);
    for (size_t i = 0; i < listedKeys_.size(); ++i)
        if (tuning.hasKey && listedKeys_[i].tonic == tuning.key.tonic && listedKeys_[i].minor == tuning.key.minor)
            keyBox_.setSelectedId(static_cast<int>(i) + 1, dontSendNotification);

    detectedButton_.setEnabled(tuning.tableChosen || tuning.keyChosen);
    const Scale scale(tuning.key.getScaleType());
    const int tonic = tuning.hasKey ? static_cast<int>(tuning.key.tonic) : -1;
    std::array<bool, 12> inKey{};
    for (int pc = 0; pc < 12 && tonic >= 0; ++pc)
        inKey[static_cast<size_t>(pc)] = scale.isIntervalInScale((pc - tonic + 12) % 12);
    histogramView_->set(
        makeFoldedPitchHistogram(detection.histogram, tuning.grid, HistogramView::binsPerSemitone), tonic, inKey);
}

}  // namespace MoTool
