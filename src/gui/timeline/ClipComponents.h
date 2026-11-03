#pragma once

#include <JuceHeader.h>

#include "../../controllers/EditState.h"

using namespace std::literals;

namespace MoTool {

class TimelineGrid;

/** The part of a clip's header or body that can show, in its own coordinates: inside the
    track body, which spans the view, and the viewport, whose scroll bar hides the view's
    right edge. Both are as wide as the whole clip, mostly off-screen when zoomed in. */
Rectangle<int> getShownArea(const Component&);

//==============================================================================
/** A clip's body, under its ClipHeader: the clip's contents. */
class ClipComponent : public Component {
public:
    ClipComponent(EditViewState&, TimelineGrid&, te::Clip::Ptr);

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;

    te::Clip& getClip() { return *clip; }

    /** Of the clip's outline: the header rounds the top corners, the body the bottom ones. */
    static constexpr float cornerSize = 6.0f;

    /** The body: the lane's slate, a little lighter, more when the clip is selected.
        The clip's colour is in its header and ring. */
    static Colour getFill(Colour lane, bool selected);

    /** Width and colour (a pale tint of the clip's hue) of the ring around a selected clip's
        header and body. Every clip is inset by ringWidth so the ring fits outside it. */
    static constexpr int ringWidth = 2;
    static Colour getRingColour(Colour clipColour);

protected:
    EditViewState& editViewState;
    TimelineGrid& grid;
    te::Clip::Ptr clip;
};

//==============================================================================
/** The band above a clip's body, in the clip's colour, lighter when the clip is selected. From the left: the name, warnings, info, and at the right
    edge of the view a legend, on the selected clip only, or on every clip when clips cannot
    be selected (the Player). Clicking it selects the clip. TrackBodyComponent places it,
    next to the clip's body; clip types with more to say subclass it. */
class ClipHeader : public Component {
public:
    ClipHeader(EditViewState&, TimelineGrid&, te::Clip::Ptr);

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;

    te::Clip& getClip() { return *clip; }

    static constexpr int height = 18;

    /** The clip's colour at a lightness on which textColour reads, lighter when selected. */
    static Colour getFill(Colour clipColour, bool selected);
    inline static const Colour textColour {0xff000000};  // not Colours::black, initialised in another file

    /** Which parts fit a header row of width, each item's width with the gap before it:
        the name keeps up to 48 px of its width; the legend goes first, then the items,
        last first. */
    struct Fit
    {
        size_t items;
        bool legend;
    };
    static Fit fit(int width, int nameWidth, const std::vector<int>& itemWidths, int legendWidth);

protected:
    /** Incompatibilities, drawn as warning chips after the name. */
    virtual StringArray getWarnings() { return {}; }
    /** Facts after the warnings, such as the tuning. */
    virtual String getInfo() { return {}; }
    /** Zero for no legend. */
    virtual int getLegendWidth() { return 0; }
    virtual void paintLegend(Graphics&, Rectangle<int>) {}

    EditViewState& editViewState;
    TimelineGrid& grid;
    te::Clip::Ptr clip;
};

class AudioClipComponent : public ClipComponent {
public:
    AudioClipComponent(EditViewState&, TimelineGrid&, te::Clip::Ptr);

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
    MidiClipComponent(EditViewState&, TimelineGrid&, te::Clip::Ptr);

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
