#include "PlayerDocument.h"

#include "../../controllers/App.h"
#include "../../controllers/PlayerController.h"

namespace MoTool {

//==============================================================================
/** The layouts Viz mode can show, the one it shows lit, and the keys: on top for a moment, then fading. */
class VizHint : public Component,
                private Timer
{
public:
    explicit VizHint(VizOptions::Layout shown) : shown_((int) shown) {
        setInterceptsMouseClicks(false, false);
        const auto& names = VizOptions::layoutNames;
        int namesWidth = gap * (names.size() - 1);
        for (auto& name : names)
            namesWidth += GlyphArrangement::getStringWidthInt(nameFont(), name);
        setSize(jmax(namesWidth, GlyphArrangement::getStringWidthInt(keysFont(), keys)) + pad * 4,
                pad * 2 + nameHeight + keysHeight);
        startTimer(shownMs);
    }

    void paint(Graphics& g) override {
        g.setColour(Colours::black.withAlpha(0.8f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), (float) pad);

        auto r = getLocalBounds().reduced(pad * 2, pad);
        const auto& names = VizOptions::layoutNames;
        int x = r.getX();
        g.setFont(nameFont());
        for (int i = 0; i < names.size(); ++i) {
            const int width = GlyphArrangement::getStringWidthInt(nameFont(), names[i]);
            g.setColour(i == shown_ ? Colors::Theme::textPrimary : Colors::Theme::textDisabled);
            g.drawText(names[i], x, r.getY(), width, nameHeight, Justification::centred);
            x += width + gap;
        }
        g.setFont(keysFont());
        g.setColour(Colors::Theme::textSecondary);
        g.drawText(keys, r.withTrimmedTop(nameHeight), Justification::centred);
    }

private:
    void timerCallback() override {
        stopTimer();
        Desktop::getInstance().getAnimator().fadeOut(this, 400);
    }

    static Font nameFont() { return Font(FontOptions(15.0f)); }
    static Font keysFont() { return Font(FontOptions(12.0f)); }

    static constexpr int pad = 8, gap = 24, nameHeight = 22, keysHeight = 18, shownMs = 2500;
    inline static const String keys = String::fromUTF8("V switches · Esc leaves");
    const int shown_;
};

//==============================================================================
PlayerDocumentComponent::PlayerDocumentComponent(te::Edit& edit, EditViewState& evs)
    : editViewState_ {evs}
    , transportBar_ {evs, {.showRecord = false, .showAutomation = false}}
    , editComponent_ {edit, evs, {.showDetailsPanel = false, .autoFitTrackHeights = true, .transparentClips = true, .disableClipSelection = true}}
{
    if (! evs.vizMode)
        addAndMakeVisible(transportBar_);
    addAndMakeVisible(editComponent_);
}

PlayerDocumentComponent::~PlayerDocumentComponent() {}

void PlayerDocumentComponent::showVizHint() {
    if (! editViewState_.vizMode)
        return;
    vizHint_ = std::make_unique<VizHint>(editViewState_.vizOptions.layout);
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
    editComponent_.setBounds(r.withTrimmedLeft(8));
    if (vizHint_ != nullptr)
        vizHint_->setTopLeftPosition((getWidth() - vizHint_->getWidth()) / 2, 24);
}

bool PlayerDocumentComponent::isInterestedInFileDrag(const StringArray& files) {
    for (auto& path : files) {
        auto ext = File(path).getFileExtension().toLowerCase();
        if (ext == ".psg" || ext == ".uzx")
            return true;
    }
    return false;
}

void PlayerDocumentComponent::filesDropped(const StringArray& paths, int /*x*/, int /*y*/) {
    Array<File> files;
    for (auto& path : paths)
        files.add(File(path));
    MoToolApp::getPlayerController().handleOpenFiles(files);
}

}  // namespace MoTool
