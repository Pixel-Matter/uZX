#include "LevelMeter.h"
#include "LookAndFeel.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>  // declares a GDI Rectangle(), so this file writes juce::Rectangle
#else
 #include <time.h>
#endif

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

void draw(Graphics& g, juce::Rectangle<int> area, int ledSize, int gap, float fill, bool horizontal, bool isMidi) {
    const int step = ledSize + gap;
    const int ledCount = (horizontal ? area.getWidth() : area.getHeight()) / step;
    const int activeLeds = static_cast<int>(jlimit(0.0f, 1.0f, fill) * static_cast<float>(ledCount));

    for (int i = 0; i < ledCount; ++i) {
        using R = juce::Rectangle<int>;
        auto led = horizontal ? R(area.getX() + i * step, area.getY(), ledSize, area.getHeight())
                              : R(area.getX(), area.getBottom() - (i + 1) * step, area.getWidth(), ledSize);
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

//==============================================================================
/** CPU time the calling thread has used so far, in milliseconds. */
static double getThreadCpuTimeMs() {
   #if JUCE_WINDOWS
    // ponytail: Windows counts thread time in scheduler ticks (~15.6 ms); fine over the meter's 250 ms.
    FILETIME created, exited, kernel, user;
    GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user);
    const auto ms = [](FILETIME t) { return double((uint64(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 1.0e4; };
    return ms(kernel) + ms(user);
   #else
    timespec t {};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return double(t.tv_sec) * 1.0e3 + double(t.tv_nsec) / 1.0e6;
   #endif
}

CpuLoadMeter::CpuLoadMeter(tracktion::Engine& engine)
    : engine_{engine}
    , lastThreadMs_{getThreadCpuTimeMs()}
    , lastWallMs_{Time::getMillisecondCounterHiRes()}
{
    setInterceptsMouseClicks(false, false);
    startTimerHz(4);
}

void CpuLoadMeter::timerCallback() {
    // The timer runs on the message thread, so its own CPU time is the message thread's.
    const auto threadMs = getThreadCpuTimeMs();
    const auto wallMs = Time::getMillisecondCounterHiRes();
    if (wallMs > lastWallMs_)
        message_ = float((threadMs - lastThreadMs_) / (wallMs - lastWallMs_));
    lastThreadMs_ = threadMs;
    lastWallMs_ = wallMs;

    audio_ = engine_.getDeviceManager().getCpuUsage();
    repaint();
}

void CpuLoadMeter::paint(Graphics& g) {
    static constexpr int ledSize = 2, gap = 1, ledHeight = 5, captionWidth = 22;
    g.setFont(FontOptions(9.0f));
    auto b = getLocalBounds();
    const auto row = [&](juce::Rectangle<int> r, const char* caption, float load) {
        g.setColour(Colors::Theme::textSecondary);
        g.drawText(caption, r.removeFromLeft(captionWidth), Justification::centredLeft, false);
        LedMeter::draw(g, r.withSizeKeepingCentre(r.getWidth(), ledHeight), ledSize, gap, load, true);
    };
    row(b.removeFromTop(b.getHeight() / 2), "DSP", audio_);
    row(b, "GUI", message_);
}

}  // namespace MoTool
