#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"

using namespace std::literals;

namespace MoTool {

//==============================================================================
class ClipComponent : public Component {
public:
    ClipComponent(EditViewState&, te::Clip::Ptr);

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;

    te::Clip& getClip() { return *clip; }

protected:
    EditViewState& editViewState;
    te::Clip::Ptr clip;
};

//==============================================================================
class AudioClipComponent : public ClipComponent {
public:
    AudioClipComponent(EditViewState&, te::Clip::Ptr);

    te::WaveAudioClip* getWaveAudioClip() {
        return dynamic_cast<te::WaveAudioClip*>(clip.get());
    }

    void paint(Graphics& g) override;

private:
    void updateThumbnail();
    void drawWaveform(Graphics& g, te::AudioClipBase& c, te::SmartThumbnail& thumb, Colour colour,
                      int left, int right, int y, int h, int xOffset);
    void drawChannels(Graphics& g, te::SmartThumbnail& thumb, Rectangle<int> area,
                      te::TimeRange time, bool useLeft, bool useRight,
                      float leftGain, float rightGain);

    std::unique_ptr<te::SmartThumbnail> thumbnail;
};

//==============================================================================
class MidiClipComponent : public ClipComponent {
public:
    MidiClipComponent(EditViewState&, te::Clip::Ptr);

    te::MidiClip* getMidiClip() {
        return dynamic_cast<te::MidiClip*>(clip.get());
    }

    void paint(Graphics& g) override;
};

//==============================================================================
class RecordingClipComponent : public Component,
                               private Timer {
public:
    RecordingClipComponent (te::Track::Ptr t, EditViewState&);

    void paint(Graphics& g) override;

private:
    void timerCallback() override;
    void updatePosition();
    void initialiseThumbnailAndPunchTime();
    void drawThumbnail(Graphics& g, Colour waveformColour) const;
    bool getBoundsAndTime(juce::Rectangle<int>& bounds, tracktion::TimeRange& times) const;

    te::Track::Ptr track;
    EditViewState& editViewState;

    te::RecordingThumbnailManager::Thumbnail::Ptr thumbnail;
    te::TimePosition punchInTime = -1.0s;
};


}  // namespace MoTool
