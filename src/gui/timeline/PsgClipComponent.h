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

//==============================================================================
/** The header's warning when a fitted tuning's clock is not the clock its track plays at,
    e.g. "⚠ Pentagon @ ZX Spectrum +23¢": every note sounds that many cents off its name.
    Empty when the clocks agree to within a cent. */
String getClockMismatchText(double tuningClock, double trackClock);

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
    void paintRegisters(Graphics& g);
    void paintNotes(Graphics& g);
    /** Tuning-grid guide lines, drawn under the notes in the Notes view. */
    void paintNoteLanes(Graphics& g);

private:
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void changeListenerCallback(ChangeBroadcaster*) override;
    void timerCallback() override;

    /** Hover popup text for the frame under local x; empty before the first frame. */
    std::vector<std::pair<String, Colour>> describeFrameAt(int x);
    void showFrameHover();
    void hideFrameHover();

    class FrameHover;
    std::unique_ptr<FrameHover> frameHover_;

    std::vector<NoteLane> lanes_;  // rebuilt when the clip's tuning revision changes
    int lanesRevision_ = -1;

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
    PsgClipHeader(EditViewState&, TimelineGrid&, te::Clip::Ptr);
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

}  // namespace MoTool
