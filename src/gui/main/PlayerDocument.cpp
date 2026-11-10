#include "PlayerDocument.h"

#include "../../controllers/App.h"
#include "../../controllers/PlayerController.h"
#include "../../plugins/uZX/analyzer/SpectrumPanel.h"
#include "../../util/Helpers.h"

#include <algorithm>

namespace MoTool {

//==============================================================================
/** What Viz mode can show, a line per key with the choice shown lit, and how to leave: on top for
    a moment, then fading. The scopes' line is dim while there are no scopes to arrange. */
class VizHint : public Component,
                private Timer
{
public:
    explicit VizHint(const VizOptions& shown)
        : lines_ {{{VizOptions::layoutNames, (int) shown.layout, "V", true},
                   {VizOptions::scopesNames, (int) shown.scopes, "S", shown.showsScopes()}}} {
        setInterceptsMouseClicks(false, false);
        int width = GlyphArrangement::getStringWidthInt(smallFont(), leave);
        for (auto& line : lines_)
            width = jmax(width, namesWidth(line.names) + gap + keyWidth);
        setSize(width + pad * 4, pad * 2 + lineHeight * (int) lines_.size() + leaveHeight);
        startTimer(shownMs);
    }

    void paint(Graphics& g) override {
        g.setColour(Colours::black.withAlpha(0.8f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), (float) pad);

        auto r = getLocalBounds().reduced(pad * 2, pad);
        for (auto& line : lines_) {
            auto row = r.removeFromTop(lineHeight);
            const auto dim = Colors::Theme::textDisabled.withMultipliedAlpha(line.active ? 1.0f : 0.5f);
            g.setFont(nameFont());
            g.setColour(line.active ? Colors::Theme::textSecondary : dim);
            g.drawText(line.key, row.removeFromRight(keyWidth), Justification::centredRight);
            for (int i = 0; i < line.names.size(); ++i) {
                const int width = GlyphArrangement::getStringWidthInt(nameFont(), line.names[i]);
                g.setColour(i == line.shown && line.active ? Colors::Theme::textPrimary : dim);
                g.drawText(line.names[i], row.removeFromLeft(width), Justification::centredLeft);
                row.removeFromLeft(gap);
            }
        }
        g.setFont(smallFont());
        g.setColour(Colors::Theme::textSecondary);
        g.drawText(leave, r, Justification::centred);
    }

private:
    struct Line
    {
        StringArray names;
        int shown;
        String key;
        bool active;
    };

    void timerCallback() override {
        stopTimer();
        Desktop::getInstance().getAnimator().fadeOut(this, 400);
    }

    static Font nameFont() { return Font(FontOptions(15.0f)); }
    static Font smallFont() { return Font(FontOptions(12.0f)); }
    static int namesWidth(const StringArray& names) {
        int width = gap * (names.size() - 1);
        for (auto& name : names)
            width += GlyphArrangement::getStringWidthInt(nameFont(), name);
        return width;
    }

    static constexpr int pad = 8, gap = 24, keyWidth = 16, lineHeight = 24, leaveHeight = 20, shownMs = 2500;
    inline static const String leave = "Esc leaves";
    const std::array<Line, 2> lines_;
};

//==============================================================================
PlayerDocumentComponent::PlayerDocumentComponent(te::Edit& edit, EditViewState& evs)
    : editViewState_ {evs}
    , transportBar_ {evs, {.showRecord = false, .showAutomation = false}}
    , editComponent_ {edit, evs, {.showDetailsPanel = false, .autoFitTrackHeights = true, .transparentClips = true, .disableClipSelection = true}}
{
    if (evs.showsVizSpectrum()) {
        spectrum_ = std::make_unique<uZX::SpectrumPanel>(edit, evs);
        addAndMakeVisible(*spectrum_);
        evs.state.addListener(this);  // its size, as its edge is dragged
    }
    if (! evs.vizMode)
        addAndMakeVisible(transportBar_);
    addAndMakeVisible(editComponent_);
}

PlayerDocumentComponent::~PlayerDocumentComponent() {
    editViewState_.state.removeListener(this);
}

void PlayerDocumentComponent::valueTreePropertyChanged(ValueTree&, const Identifier& id) {
    if (id == IDs::vizSpectrumWidth || id == IDs::vizSpectrumHeight)
        resized();
}

void PlayerDocumentComponent::showVizHint() {
    if (! editViewState_.vizMode)
        return;
    vizHint_ = std::make_unique<VizHint>(editViewState_.vizOptions);
    addAndMakeVisible(*vizHint_);
    resized();
}

void PlayerDocumentComponent::paint(Graphics& g) {
    if (editViewState_.isVizClassic())  // the margin at the left, black like the rest
        g.fillAll(Colors::VizClassic::background);
}

void PlayerDocumentComponent::resized() {
    auto r = getLocalBounds();
    static constexpr int transportBarHeight = 40;

    if (transportBar_.isVisible())
        transportBar_.setBounds(r.removeFromTop(transportBarHeight));
    // The spectrum takes at most half the window
    const bool spectrumAtLeft =
        spectrum_ != nullptr && editViewState_.vizOptions.spectrum == VizOptions::Spectrum::left;
    if (spectrumAtLeft) {
        constexpr int narrowest = EditViewState::minVizSpectrumWidth;
        spectrum_->setBounds(r.removeFromLeft(
            jlimit(narrowest, jmax(narrowest, getWidth() / 2), editViewState_.vizSpectrumWidth.get())));
    } else if (spectrum_ != nullptr) {
        constexpr int lowest = EditViewState::minVizSpectrumHeight;
        spectrum_->setBounds(r.removeFromBottom(
            jlimit(lowest, jmax(lowest, getHeight() / 2), editViewState_.vizSpectrumHeight.get())));
    }
    editComponent_.setBounds(spectrumAtLeft ? r : r.withTrimmedLeft(8));
    if (vizHint_ != nullptr)
        vizHint_->setTopLeftPosition((getWidth() - vizHint_->getWidth()) / 2, 24);
}

bool PlayerDocumentComponent::isInterestedInFileDrag(const StringArray& files) {
    return std::ranges::any_of(files, [](const String& path) {
        return Helpers::isTuneFile(File(path)) || File(path).hasFileExtension("uzx");
    });
}

void PlayerDocumentComponent::filesDropped(const StringArray& paths, int /*x*/, int /*y*/) {
    Array<File> files;
    for (auto& path : paths)
        files.add(File(path));
    MoToolApp::getPlayerController().handleOpenFiles(files);
}

}  // namespace MoTool
