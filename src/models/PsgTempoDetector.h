#pragma once

#include <JuceHeader.h>

#include <span>
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
    /** No row grid held through the tune (a row a frame longer now and then, a groove that varies):
        rowFrames is its pulse, the frames after which it repeats, and what lies inside is not known. */
    bool rowsArePulse = false;
    int rowsPerBeat = 0;
    int beatsPerBar = 4;      ///< 3 or 4; 4 unless bar-level repetition clearly says 3
    bool meterClear = false;  ///< bar-level repetition clearly favours beatsPerBar over the other choice
    int framesPerBeat = 0;    ///< machine frames per beat
    double bpm = 0.0;
    double gridShare = 0.0;  ///< 0..1: how much note-start energy lies on the row grid
    /** Machine frames before the first row starts; that row is taken as a downbeat.
        0 also when the first two bars don't follow the grid (an intro at another speed). */
    int firstRowFrame = 0;
    int gridPhase = 0;         ///< machine frame the row grid starts at, kept when firstRowFrame is zeroed
    bool silentLeadIn = true;  ///< nothing is audible before firstRowFrame

    /** The beat's groove for the grid: frame counts of 4 groups of rows (3 or 5 when
        the beat divides that way, else 2), so a swing or groove shows as its
        pairs, e.g. {6, 7, 6, 7} or, for 4-3-3-3 rows, {7, 6, 7, 6}. Single rows
        are left to the frame level of the grid. */
    std::vector<int> subdivisionPattern() const;
};

constexpr double minGridShare = 0.8;   ///< chance-corrected share of the energy a row grid must hold
constexpr double minBeatRepeat = 0.4;  ///< autocorrelation over 1, 2 and 4 beats; below it the tune has no beat
constexpr double beatMargin = 0.1;     ///< another grouping of rows or pulses must repeat this much better to win
constexpr double minMeterMargin = 0.1; ///< one bar length must explain repetition this much better
/** The beat's length in data frames that the BPM range allows, read at 50 Hz as trackers count rows. */
constexpr int minBeatFrames = 18, maxBeatFrames = 37;

/** How detect() came to its answer: what it measured and what it weighed, for looking at a tune
    it gets wrong (see the tempo inspector). Frames are data frames, as the dump has them. */
struct Analysis
{
    enum class RowsFrom
    {
        nothing,     ///< no tempo: too little evidence, no grid and no beat the tune repeats at
        module,      ///< the rows a tracker module states
        evidence,    ///< the grid of the learnt row evidence
        noteStarts,  ///< the grid of the cruder note starts, when the evidence shows none
        pulse        ///< no grid held: the pulse and beat the evidence repeats at
    };

    /** A row grid tried, and the chance-corrected share of the row evidence on it. */
    struct Grid
    {
        std::vector<int> rows;
        double share = 0.0;
    };

    /** A beat tried: so many rows (or pulses) to it, and what speaks for it. */
    struct Beat
    {
        int rowsPerBeat = 0;
        int frames = 0;
        juce::Optional<double> patterns;  ///< share of rows whose drums and bass repeat 2, 4 and 8 beats on
        juce::Optional<double> accents;   ///< how the rows' loudness recurs a beat on, 1 the most
        juce::Optional<double> repeat;    ///< how alike the row evidence is to itself 1, 2 and 4 beats on
    };

    juce::Optional<Result> result;
    RowsFrom rowsFrom = RowsFrom::nothing;
    std::vector<double> evidence;    ///< per frame, over all chips: PsgRowEvidence::of
    std::vector<double> noteStarts;  ///< per frame, over all chips: volume jumps that start a note
    std::vector<double> repeat;      ///< autocorrelation of the evidence by lag; empty for a tune too short
    std::vector<Grid> grids;         ///< tried on the evidence: every uniform speed, the best swing and groove
    int pulse = 0;                   ///< frames; 0 unless a pulse was looked for and found
    std::vector<Beat> beats;         ///< the beats weighed, the result's among them
    double meterMargin = 0.0;        ///< above minMeterMargin 3/4, below minus that 4/4
};

/** Chance-corrected share of a per-frame energy on the best phase of a grid of `period` frames: 1
    when all of it lies on one frame in every `period`, 0 when it is spread evenly or there is none. */
double gridShare(std::span<const double> energy, int period);

/** How alike a tune is to itself one, two and four beats of `beatFrames` on: the mean of its
    autocorrelation (Analysis::repeat) there. Nothing for a beat too long for `repeat` to reach. */
juce::Optional<double> repeatOverBeats(const std::vector<double>& repeat, int beatFrames);

/** detect() with its working shown. */
Analysis analyse(const std::vector<const uZX::PsgData*>& chips, const std::vector<size_t>& knownRows = {});

/** Returns nothing when the dump has too few note starts, or neither a row grid nor a
    beat it repeats at (speed 1, sample tunes, heavily modulated material). */
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
