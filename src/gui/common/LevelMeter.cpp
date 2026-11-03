#include "LevelMeter.h"
#include "LookAndFeel.h"

namespace MoTool {

namespace LedMeter {

float dbToFill(float dB) {
    constexpr float minDb = -60.0f;
    return jlimit(0.0f, 1.0f, (dB - minDb) / -minDb);
}

static Colour ledColour(float position, bool isMidi) {
    if (isMidi)
        return Colors::Meter::midi;
    if (position < 0.6f)
        return Colors::Meter::low;
    if (position < 0.85f)
        return Colors::Meter::mid;
    return Colors::Meter::high;
}

void draw(Graphics& g, Rectangle<int> area, int ledSize, int gap, float fill, bool horizontal, bool isMidi) {
    const int step = ledSize + gap;
    const int ledCount = (horizontal ? area.getWidth() : area.getHeight()) / step;
    const int activeLeds = static_cast<int>(jlimit(0.0f, 1.0f, fill) * static_cast<float>(ledCount));

    for (int i = 0; i < ledCount; ++i) {
        auto led = horizontal ? Rectangle<int>(area.getX() + i * step, area.getY(), ledSize, area.getHeight())
                              : Rectangle<int>(area.getX(), area.getBottom() - (i + 1) * step, area.getWidth(), ledSize);
        auto colour = ledColour(static_cast<float>(i) / static_cast<float>(ledCount), isMidi);
        g.setColour(i < activeLeds ? colour : colour.withAlpha(0.1f));
        g.fillRect(led);
    }
}

}  // namespace LedMeter


MasterLevelMeter::MasterLevelMeter(tracktion::Edit& edit)
    : edit_{edit}
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(30);
}

MasterLevelMeter::~MasterLevelMeter() {
    if (auto* m = measurer_.get())
        m->removeClient(client_);
}

void MasterLevelMeter::timerCallback() {
    // Device changes recreate the playback context and its measurer: follow the current one.
    auto* context = edit_.getCurrentPlaybackContext();
    auto* current = context != nullptr ? &context->masterLevels : nullptr;
    if (current != measurer_.get()) {
        if (auto* old = measurer_.get())
            old->removeClient(client_);
        measurer_ = current;
        if (current != nullptr)
            current->addClient(client_);
    }

    left_ = LedMeter::dbToFill(client_.getAndClearAudioLevel(0).dB);
    right_ = LedMeter::dbToFill(client_.getAndClearAudioLevel(1).dB);
    repaint();
}

void MasterLevelMeter::paint(Graphics& g) {
    static constexpr int ledSize = 2, gap = 1;
    auto b = getLocalBounds();
    auto top = b.removeFromTop(b.getHeight() / 2);
    LedMeter::draw(g, top.withTrimmedBottom(gap), ledSize, gap, left_, true);
    LedMeter::draw(g, b.withTrimmedTop(gap), ledSize, gap, right_, true);
}

}  // namespace MoTool
