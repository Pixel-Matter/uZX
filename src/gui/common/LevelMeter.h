#pragma once

#include <JuceHeader.h>

namespace MoTool {

//==============================================================================
/** LED meter drawing shared by the device-panel meter and the transport meter. */
namespace LedMeter {

/** The level scale, in dBFS: 0 is the most the output carries, and a meter shows a little past it. */
inline constexpr float minDb = -54.0f, maxDb = 6.0f;

/** How many times a second a level meter reads its level. */
inline constexpr int levelRefreshHz = 30;

/** Maps a measured level to meter fill: minDb and below is empty, maxDb is full. */
float dbToFill(float dB);

/** Where along a meter, 0 to 1, its LEDs turn yellow and then red. */
struct Zones
{
    float mid, high;
};

/** A load: yellow from 60 %, red from 85 %. */
inline constexpr Zones loadZones{0.6f, 0.85f};
/** A level: yellow for the last 6 dB below 0 dBFS, red only above it, where the output clips. */
inline constexpr Zones levelZones{(-6.0f - minDb) / (maxDb - minDb), (0.0f - minDb) / (maxDb - minDb)};

/** How many of ledCount LEDs a fill lights: each one as soon as the fill goes past its start. */
int litLeds(float fill, int ledCount);

/** The level a meter shows next, in dB: a louder reading at once, a quieter one falling 48 dB a second. */
float fallTo(float shownDb, float latestDb);

/** Draws LEDs lit from the bottom, or from the left when horizontal. Audio LEDs ramp
    green, yellow, red along the meter at its zones; MIDI LEDs are cyan. */
void draw(Graphics& g, Rectangle<int> area, int ledSize, int gap, float fill, bool horizontal, Zones zones,
          bool isMidi = false);

}  // namespace LedMeter


//==============================================================================
/** Horizontal stereo meter of the edit's master output, after the master volume. Once a channel goes
    over 0 dBFS, where the output clips, its last LED stays red until the meter is clicked. */
class MasterLevelMeter : public Component,
                         public SettableTooltipClient,
                         private Timer
{
public:
    explicit MasterLevelMeter(tracktion::Edit& edit);
    ~MasterLevelMeter() override;

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent&) override;

    /** Shows a reading of each channel, in dB, as the timer does 30 times a second. */
    void showLevels(float leftDb, float rightDb);
    bool hasClipped() const { return clipped_[0] || clipped_[1]; }

private:
    void timerCallback() override;

    tracktion::Edit& edit_;
    tracktion::LevelMeasurer::Client client_;
    WeakReference<tracktion::LevelMeasurer> measurer_;  // owned by the playback context, which can be recreated
    float shownDb_[2] = {-100.0f, -100.0f};
    bool clipped_[2] = {false, false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterLevelMeter)
};


//==============================================================================
/** CPU load as two captioned LED rows: DSP is the audio callback's share of its time budget,
    GUI is the message thread's CPU time per second of wall time. */
class CpuLoadMeter : public Component,
                     private Timer
{
public:
    explicit CpuLoadMeter(tracktion::Engine& engine);

    void paint(Graphics& g) override;

private:
    void timerCallback() override;

    tracktion::Engine& engine_;
    double lastThreadMs_, lastWallMs_;
    float audio_ = 0.0f, message_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CpuLoadMeter)
};

}  // namespace MoTool
