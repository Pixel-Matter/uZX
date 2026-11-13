#include <JuceHeader.h>

#include "ClipComponents.h"
#include "PsgParamEditorComponent.h"
#include "../../models/PsgClip.h"
#include "../common/GUIPaintMeasurer.h"

namespace MoTool {

//==============================================================================
/** Frame hover popup text as coloured runs: A/B/C, Env and Noise as a borderless
    table with right-aligned numbers, in legend colours, half-alpha unless this frame
    changed the value. With hex, periods and volumes are zero-padded hex. */
std::vector<std::pair<String, Colour>> formatPsgFrame(const PsgParamFrameData&, double chipClockHz, bool hex);

/** The frame hover's lines for a clip's timers at a frame, as coloured runs; empty when none runs there.
    A line for each running timer: what began this stretch of it if it began at this frame (start or mod),
    the pitch and duty of what it writes, its base value and timer lane; then a line for each register it
    holds, with the lane or the one value it writes. In the colour of what the timer drives, half-alpha
    unless its command is at this frame. A timer started before frame `firstPlayed` is left out: a clip
    trimmed at its start does not play it. */
std::vector<std::pair<String, Colour>> formatTimerFrame(const ClipTimers&, int frameIndex, int firstPlayed,
                                                        double chipClockHz, bool hex);

//==============================================================================
/** A PSG clip's warnings: its fps against the project's, and its tuning's clock against its track's. */
StringArray getPsgClipWarnings(PsgClip&);
/** A PSG clip's tuning table and key, "?" after a detection tie nobody settled. */
String getPsgClipInfo(const PsgClip&);
/** The A B C E N channel legend: its width, then the swatches from area's left edge, area's height tall. */
int getPsgLegendWidth();
void paintPsgLegend(Graphics& g, Rectangle<int> area);
/** The tuning table and key of an edit's PSG clips: one clip's own, or several detected
    together, as the chips of a Turbo Sound tune. Empty without PSG clips or pitches. */
String getPsgSetInfo(te::Edit&);

/** One piano-roll lane of the Notes view: a grid note's band in normalized pitch,
    bounded by the midpoints to its neighbours. */
struct NoteLane
{
    float lo, hi;
    int note;
    String label;
};

/** Lanes for every note of a grid, lowest first, continued by octaves past the grid's
    ends to the limits of the tone range. */
std::vector<NoteLane> makeNoteLanes(const PeriodGrid& grid);

/** How much of the lanes the Notes view draws at a lane height (one semitone, in px). */
struct LaneDetail
{
    enum Level { None, Tonic, All };
    Level lines = None;
    Level labels = None;
};

LaneDetail getLaneDetail(float noteHeight);

/** Whether the Notes view draws note lanes (View > Note Lanes). An app setting rather than
    an edit property: the Player opens every file into a fresh edit, where an edit property
    would reset. Every PsgClipComponent repaints when the settings change. */
bool areNoteLanesShown(te::Engine&);
void setNoteLanesShown(te::Engine&, bool shouldShow);

/** Whether the note lanes get a short piano keyboard over their left edge in the view
    (View > Piano Keys). An app setting, like areNoteLanesShown(). */
bool arePianoKeysShown(te::Engine&);
void setPianoKeysShown(te::Engine&, bool shouldShow);

//==============================================================================
class PsgClipComponent : public MidiClipComponent,
                         private juce::ValueTree::Listener,
                         private juce::ChangeListener,
                         private juce::Timer {
public:
    PsgClipComponent(EditViewState& evs, TimelineGrid& g, te::Clip::Ptr c);
    ~PsgClipComponent() override;

    PsgClip* getPsgClip();

    void paint(Graphics& g) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseMove(const MouseEvent& e) override;
    void mouseExit(const MouseEvent& e) override;
    /** The Notes view's notes, as paint() draws them for the view. */
    void paintNotes(Graphics& g);
    /** Tuning-grid guide lines, drawn under the notes in the Notes view, with the lane names and
        piano keys at the view's left edge. */
    void paintNoteLanes(Graphics& g);

    /** Where the painters draw: see PsgClipComponent.cpp. */
    struct PaintView;

protected:
    /** Whether paint() blits the content from cached tiles: while the view follows playback,
        when it scrolls every frame by whole pixels. */
    virtual bool shouldCacheContent() const;

private:
    /** The view's part of the clip, in this component's coordinates. */
    PaintView getViewPaintView() const;
    /** Note lanes and notes, or registers. */
    void paintContent(Graphics& g, const PaintView& view);
    void paintRegisters(Graphics& g, const PaintView& view);
    void paintNotes(Graphics& g, const PaintView& view);
    void paintNoteLanes(Graphics& g, const PaintView& view);
    void paintTicksOverLanes(Graphics& g, const PaintView& view);
    /** paintContent() from cached images: the note lanes, and the notes or registers over them. */
    void paintCachedContent(Graphics& g, const PaintView& view);

    /** The notes or registers, cached as images in fixed-width tiles along x, and the note lanes
        as an image of the gutter and a one-pixel column of the grid. Scrolling blits them; the
        tile about to scroll in is painted ahead, a slice each frame. */
    class ContentTiles;
    std::unique_ptr<ContentTiles> tiles_;
    int contentRevision_ = 0;  // changes of the content other than the clip's geometry and the zoom

    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void changeListenerCallback(ChangeBroadcaster*) override;
    void timerCallback() override;
    /** Without clip headers (the Player), the warnings go over the notes' top-left corner. */
    void paintWarnings(Graphics& g);

    /** Hover popup text for the frame under local x; empty before the first frame. */
    std::vector<std::pair<String, Colour>> describeFrameAt(int x);
    void showFrameHover();
    void hideFrameHover();

    class FrameHover;
    std::unique_ptr<FrameHover> frameHover_;

    std::vector<NoteLane> lanes_;  // rebuilt when the clip's tuning revision changes
    int lanesRevision_ = -1;

    /** The noise band, pre-rendered once per height at full opacity and one pixel wide: each frame's
        band is a stretched blit of it, not a gradient evaluated per pixel. */
    Image noiseBand_;

    // void onVBlank() {
    //     // Throttle to ~30fps (skip every other vblank on 60Hz display)
    //     if (++vblankCounter_ < vblankDivider_)
    //         return;
    //     vblankCounter_ = 0;

    //     if (needsRepaint_) {
    //         needsRepaint_ = false;
    //         repaint();
    //     }
    // }

    // // Mark for repaint on next VBlank instead of immediate
    // void visibilityChanged() override {
    //     MidiClipComponent::visibilityChanged();
    //     needsRepaint_ = true;
    // }

    // void moved() override {
    //     MidiClipComponent::moved();
    //     needsRepaint_ = true;
    // }

    // void resized() override {
    //     MidiClipComponent::resized();
    //     needsRepaint_ = true;
    // }

    GUIPaintMeasurer paintMeasurer_;
    // VBlankAttachment vblankAttachment_;
    // bool needsRepaint_ = true;
    // int vblankCounter_ = 0;
    // int vblankDivider_ = 2;  // 60Hz / 2 = 30fps
};

/** A PSG clip's header: fps and clock warnings, tuning and key, and the channel legend. */
class PsgClipHeader : public ClipHeader,
                      private juce::ValueTree::Listener {
public:
    PsgClipHeader(EditViewState&, te::Clip::Ptr);
    ~PsgClipHeader() override;

    void mouseDown(const MouseEvent& e) override;

    /** A B C E N swatches from area's left edge, each area's height tall. */
    void paintLegend(Graphics& g, Rectangle<int> area) override;

protected:
    StringArray getWarnings() override;
    String getInfo() override;
    int getLegendWidth() override;

private:
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
};

//==============================================================================
/** The channel legend on its backing, for views without clip headers (the Player). */
class PsgLegendComponent : public Component {
public:
    PsgLegendComponent();
    void paint(Graphics& g) override;
};

/** The tuning table and key of all PSG clips (getPsgSetInfo), for views without clip
    headers (the Player); follows clips coming and going and their tuning choices. */
class PsgSetInfoComponent : public Component,
                            private juce::ValueTree::Listener,
                            private juce::AsyncUpdater {
public:
    explicit PsgSetInfoComponent(te::Edit&);
    ~PsgSetInfoComponent() override;

    void paint(Graphics& g) override;
    const String& getText() const noexcept { return text_; }

private:
    void handleAsyncUpdate() override;
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;

    te::Edit& edit_;
    String text_;
};

}  // namespace MoTool
