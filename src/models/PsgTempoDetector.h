#pragma once

#include <JuceHeader.h>

#include <vector>

#include "../formats/psg/PsgData.h"

namespace MoTool {

class PsgClip;

//==============================================================================
/** Tempo detection for AY register dumps.

    Trackers write every note on a row, and a row lasts a whole number of frames
    (the tracker "speed"), so the job is to find that integer, or a repeating
    swing (6-7) or groove (4-4-3-3) of row lengths, and how many rows make a beat
    (3, 4 or 5, times a power of two).
*/
namespace PsgTempo {

struct Result
{
    std::vector<int> rowFrames;  ///< machine frames per row, repeating: {6}, a swing {6, 7}, a groove {4, 4, 3, 3}
    int rowsPerBeat = 0;
    int beatsPerBar = 4;      ///< 3 or 4; 4 unless bar-level repetition clearly says 3
    bool meterClear = false;  ///< bar-level repetition clearly favours beatsPerBar over the other choice
    int framesPerBeat = 0;    ///< machine frames per beat
    double bpm = 0.0;
    double gridShare = 0.0;  ///< 0..1: how much note-start energy lies on the row grid
    /** Machine frames before the first row starts; that row is taken as a downbeat.
        0 also when the first two bars don't follow the grid (an intro at another speed). */
    int firstRowFrame = 0;
    bool silentLeadIn = true;  ///< nothing is audible before firstRowFrame

    /** The beat's groove for the grid: frame counts of 4 groups of rows (3 or 5 when
        the beat divides that way, else 2), so a swing or groove shows as its
        pairs, e.g. {6, 7, 6, 7} or, for 4-3-3-3 rows, {7, 6, 7, 6}. Single rows
        are left to the frame level of the grid. */
    std::vector<int> subdivisionPattern() const;
};

/** Returns nothing when the dump has too few note starts or no frame grid
    (speed 1, sample tunes, heavily modulated material). */
juce::Optional<Result> detect(const uZX::PsgData&);
/** The chips of one tune detected as one: a Turbo Sound tune spreads its notes and
    drums over them. They share the first chip's frame rate. */
juce::Optional<Result> detect(const std::vector<const uZX::PsgData*>& chips);
/** Same, on the row grid a tracker module knows (the frame every row starts at, see
    uZX::TrackerModule): its rows replace the grid detection, the first row is the downbeat,
    and how many rows make a beat and the meter are still detected. Too few rows to make a
    grid (fewer than 2) mean detection, as above. */
juce::Optional<Result> detect(const std::vector<const uZX::PsgData*>& chips, const std::vector<size_t>& knownRows);

/** When the clips (the chips of one tune, each loaded from its dump in `chips`) are the
    only clips in their edit, detects the tempo of the dumps together and
    sets the tempo at the clip start so a beat lasts framesPerBeat frames, stores
    the row pattern as the grid subdivision, sets the time signature to
    beatsPerBar/4 when the meter is clear, and puts the first row on a bar line:
    a silent lead-in is trimmed with the clip offsets, an audible one (a dump that
    starts mid-row) moves the clips right so the first row lands on bar 2.
    Importing into a project that already has clips keeps its tempo and meter.

    Musical pickups (notes before the first downbeat on the row grid) are not
    detected: in tracker dumps the first row is the downbeat in about 97% of
    tunes, and no register cue tried could tell the rest apart without moving
    correct ones. With `knownRows`, a module's row grid, the tempo follows its rows instead. */
void applyToEdit(const juce::Array<PsgClip*>& clips,
                 const std::vector<const uZX::PsgData*>& chips,
                 const std::vector<size_t>& knownRows = {});

/** Plays the clips at another frame rate (see PsgTiming::setClipFrameRate). When they are the
    only clips in their edit, the project's fps follows and its tempo scales with the rate, so a
    beat keeps its frames and the grid its groove; past Tracktion's tempo limits a beat takes
    twice (or half) the frames. The clips stay where they are. */
void setFrameRate(const juce::Array<PsgClip*>& clips, double fps);

/** Sets the project's fps; the PSG clips that played at the old one follow it, as in setFrameRate. */
void setProjectFrameRate(tracktion::Edit&, double fps);

}  // namespace PsgTempo

}  // namespace MoTool
