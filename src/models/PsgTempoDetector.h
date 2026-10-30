#pragma once

#include <JuceHeader.h>

#include <vector>

#include "../formats/psg/PsgData.h"

namespace MoTool {

class PsgClip;

//==============================================================================
/** Tempo detection for AY register dumps.

    Trackers write every note on a row, and a row lasts a whole number of frames
    (the tracker "speed"), so the job is to find that integer, an optional
    two-value swing (6-7-6-7...), and how many rows make a beat.
*/
namespace PsgTempo {

struct Result
{
    int speedA = 0;  ///< machine frames per row (first row of a swing pair)
    int speedB = 0;  ///< machine frames per row (second row); equals speedA without swing
    int rowsPerBeat = 0;
    int framesPerBeat = 0;  ///< machine frames per beat
    double bpm = 0.0;
    double gridShare = 0.0;  ///< 0..1: how much note-start energy lies on the row grid

    /** Frame counts per row within one beat, e.g. {6, 7, 6, 7}. */
    std::vector<int> subdivisionPattern() const;
};

/** Returns nothing when the dump has too few note starts or no frame grid
    (speed 1, sample tunes, heavily modulated material). */
juce::Optional<Result> detect(const uZX::PsgData&);

/** Sets the edit tempo so a beat lasts framesPerBeat frames and stores the row
    pattern as the grid subdivision. Done only when the clip is the edit's only
    PSG clip, so importing into a project keeps the project's tempo. */
void applyToEdit(PsgClip&, const Result&);

}  // namespace PsgTempo

}  // namespace MoTool
