#pragma once

#include <JuceHeader.h>

#include <optional>
#include <vector>

#include "../../models/tuning/Scales.h"
#include "TimelineGrid.h"

namespace MoTool {

//==============================================================================
/**
    The pitch axis of a notes view: piano-roll lanes under a clip's notes, with their names and a short
    piano keyboard at the left edge of the view. A clip says which lanes there are and where its pitch
    range sits; everything here draws them.
*/

/** One piano-roll lane: a note's band in normalized pitch, bounded by the midpoints to its neighbours. */
struct NoteLane
{
    float lo, hi;
    int note;
    String label;
};

/** How much of the lanes a notes view draws at a lane height (one semitone, in px). */
struct LaneDetail
{
    enum Level
    {
        None,
        Tonic,
        All
    };
    Level lines = None;
    Level labels = None;
};

LaneDetail getLaneDetail(float noteHeight);

/** Whether notes views draw note lanes (View > Note Lanes). An app setting rather than
    an edit property: the Player opens every file into a fresh edit, where an edit property
    would reset. The settings file broadcasts a change. */
bool areNoteLanesShown(tracktion::Engine&);
void setNoteLanesShown(tracktion::Engine&, bool shouldShow);

/** Whether the note lanes get a short piano keyboard over their left edge in the view
    (View > Piano Keys). An app setting, like areNoteLanesShown(). */
bool arePianoKeysShown(tracktion::Engine&);
void setPianoKeysShown(tracktion::Engine&, bool shouldShow);

/** Vertical mapping of a notes view: the clip's pitch range over the component
    height, the height of one semitone, and the visible slice of the clip in x. */
struct NotesLayout
{
    Range<float> pitchRange;  // normalized pitch, as NoteLane's
    float noteHeight;
    Range<float> visibleX;  // the view's part of the clip, in the clip component's coordinates
    float height;

    float normToY(float norm) const { return (pitchRange.getEnd() - norm) / pitchRange.getLength() * height; }
};

/** The note lanes as a notes view draws them now. */
struct LaneView
{
    NotesLayout layout;
    LaneDetail detail;
    bool pianoKeys;  // keys too short to tell apart leave just the grid

    /** Where the names start: right of the black keys when the keys are shown, but a black
        key's own name starts at its left end. */
    float getLabelX() const;
    /** Where the keys and names end: right of it is clear of them. */
    float getGutterRight() const;
    /** The whole pixels of the view's part of the clip, and where the grid starts in them, right of
        the keys and names. */
    Range<int> getViewX() const;
    int getGridX() const;
};

/** None when the lanes are off, or too short to draw. */
std::optional<LaneView> makeLaneView(tracktion::Engine&, const NotesLayout&);

/** The key the lanes are drawn for: its tonic's lanes are filled, and its notes are named on the keys. */
struct LaneKey
{
    int tonic;  // pitch class, 0 = C
    Scale::ScaleType scale;
};

/** The lanes that intersect the repaint area, on `background`: a line between neighbours, a brighter
    one under each C, names, and the piano keys. Without a key no lane is filled and the keys name C major. */
void paintNoteLanes(Graphics&,
                    const LaneView&,
                    const std::vector<NoteLane>&,
                    const std::optional<LaneKey>&,
                    Colour background);

/** The timeline grid's beat and bar ticks over the lanes' grid, opaque, so where one crosses a lane
    line the brighter of the two shows, not their blend. The finest ticks stay under the lanes, the
    colour of their lines; none cross the keys and names. `clipX` is the clip component's x in the
    track body, where the ticks are. */
void paintTicksOverLanes(Graphics&, const LaneView&, TimelineGrid&, int clipX, Colour background);

}  // namespace MoTool
