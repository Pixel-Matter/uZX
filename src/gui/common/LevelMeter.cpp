#include "LevelMeter.h"
#include "LookAndFeel.h"

#include <cmath>

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
    return jlimit(0.0f, 1.0f, (dB - minDb) / (maxDb - minDb));
}

int litLeds(float fill, int ledCount) {
    // A hair short of an LED's start still leaves it dark: a level of exactly 0 dBFS lights no red
    return jlimit(0, ledCount, static_cast<int>(std::ceil(fill * static_cast<float>(ledCount) - 1.0e-3f)));
}

float fallTo(float shownDb, float latestDb) {
    return jmax(latestDb, shownDb - 48.0f / static_cast<float>(levelRefreshHz));
}

static Colour ledColour(float position, Zones zones, bool isMidi) {
    if (isMidi)
        return Colors::Meter::midi;
    if (position < zones.mid)
        return Colors::Meter::low;
    if (position < zones.high)
        return Colors::Meter::mid;
    return Colors::Meter::high;
}

void draw(Graphics& g, juce::Rectangle<int> area, int ledSize, int gap, float fill, bool horizontal, Zones zones,
          bool isMidi) {
    const int step = ledSize + gap;
    const int ledCount = (horizontal ? area.getWidth() : area.getHeight()) / step;
    const int activeLeds = litLeds(fill, ledCount);

    for (int i = 0; i < ledCount; ++i) {
        using R = juce::Rectangle<int>;
        auto led = horizontal ? R(area.getX() + i * step, area.getY(), ledSize, area.getHeight())
                              : R(area.getX(), area.getBottom() - (i + 1) * step, area.getWidth(), ledSize);
        // An LED takes the colour of where it starts; the hair keeps 0.9 * 20 from landing below LED 18
        auto colour = ledColour((static_cast<float>(i) + 1.0e-3f) / static_cast<float>(ledCount), zones, isMidi);
        g.setColour(i < activeLeds ? colour : colour.withAlpha(0.1f));
        g.fillRect(led);
    }
}

}  // namespace LedMeter


MasterLevelMeter::MasterLevelMeter(tracktion::Edit& edit)
    : edit_{edit}
{
    setInterceptsMouseClicks(true, false);
    setTitle("Output level");
    setTooltip("Output level. The last LED stays red once the output has clipped: click to clear");
    startTimerHz(LedMeter::levelRefreshHz);
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

    showLevels(client_.getAndClearAudioLevel(0).dB, client_.getAndClearAudioLevel(1).dB);
}

void MasterLevelMeter::showLevels(float leftDb, float rightDb) {
    const float latest[] = {leftDb, rightDb};
    for (int ch = 0; ch < 2; ++ch) {
        shownDb_[ch] = LedMeter::fallTo(shownDb_[ch], latest[ch]);
        clipped_[ch] = clipped_[ch] || latest[ch] > 0.0f;
    }
    repaint();
}

void MasterLevelMeter::mouseDown(const MouseEvent&) {
    clipped_[0] = clipped_[1] = false;
    repaint();
}

void MasterLevelMeter::paint(Graphics& g) {
    static constexpr int ledSize = 2, gap = 1;
    auto b = getLocalBounds();
    auto top = b.removeFromTop(b.getHeight() / 2);
    const juce::Rectangle<int> rows[] = {top.withTrimmedBottom(gap), b.withTrimmedTop(gap)};
    for (int ch = 0; ch < 2; ++ch) {
        LedMeter::draw(g, rows[ch], ledSize, gap, LedMeter::dbToFill(shownDb_[ch]), true, LedMeter::levelZones);
        if (clipped_[ch]) {
            const int ledCount = rows[ch].getWidth() / (ledSize + gap);
            g.setColour(Colors::Meter::high);
            g.fillRect(rows[ch].withX(rows[ch].getX() + (ledCount - 1) * (ledSize + gap)).withWidth(ledSize));
        }
    }
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
    static constexpr int ledSize = 2, gap = 1, ledHeight = 7, captionWidth = 28;
    g.setFont(FontOptions(12.0f));
    auto b = getLocalBounds();
    const auto row = [&](juce::Rectangle<int> r, const char* caption, float load) {
        g.setColour(Colors::Theme::textSecondary);
        g.drawText(caption, r.removeFromLeft(captionWidth), Justification::centredLeft, false);
        LedMeter::draw(g, r.withSizeKeepingCentre(r.getWidth(), ledHeight), ledSize, gap, load, true,
                      LedMeter::loadZones);
    };
    row(b.removeFromTop(b.getHeight() / 2), "DSP", audio_);
    row(b, "GUI", message_);
}

}  // namespace MoTool
