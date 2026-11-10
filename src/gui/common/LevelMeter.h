#pragma once

#include <JuceHeader.h>

namespace MoTool {

//==============================================================================
/** LED meter drawing shared by the device-panel meter and the transport meter. */
namespace LedMeter {

/** Maps a measured level to meter fill: -60 dB and below is empty, 0 dB is full. */
float dbToFill(float dB);

/** An LED's colour at a position along the meter, 0 at the bottom: audio ramps green, yellow, red; MIDI is cyan. */
Colour ledColour(float position, bool isMidi = false);

/** Draws LEDs lit from the bottom, or from the left when horizontal. Audio LEDs ramp
    green, yellow, red along the meter; MIDI LEDs are cyan. */
void draw(Graphics& g, Rectangle<int> area, int ledSize, int gap, float fill, bool horizontal, bool isMidi = false);

}  // namespace LedMeter


//==============================================================================
/** Horizontal stereo meter of the edit's master output, after the master volume. */
class MasterLevelMeter : public Component,
                         private Timer
{
public:
    explicit MasterLevelMeter(tracktion::Edit& edit);
    ~MasterLevelMeter() override;

    void paint(Graphics& g) override;

private:
    void timerCallback() override;

    tracktion::Edit& edit_;
    tracktion::LevelMeasurer::Client client_;
    WeakReference<tracktion::LevelMeasurer> measurer_;  // owned by the playback context, which can be recreated
    float left_ = 0.0f, right_ = 0.0f;

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
