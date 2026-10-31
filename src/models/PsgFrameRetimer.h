#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool {

//==============================================================================
/** Helper for changing an edit's tempo with optional PSG timing preservation.

    PSG frames carry integer frame indices. Beat positions are derived from each
    list's framesPerBeat metadata, so preserving timing only scales metadata and
    never rewrites frame indices.
*/
namespace PsgTiming {

/** Sets tempo.getBpm() to the new value. If preserveAbsoluteFrameTimes is true,
    PSG lists scale framesPerBeat by the beat-length ratio so frame-index playback
    remains fixed in wall-clock time.

    Assumes a single tempo setting: the ratio comes from the changed TempoSetting
    but is applied to every PSG clip in the edit, so clips under other tempo
    sections of a multi-tempo edit would be rescaled incorrectly. */
void setTempoBpmRetimingFrames(te::Edit&, te::TempoSetting&, double bpm, bool preserveAbsoluteFrameTimes);

/** Sets the given tempo so a beat lasts framesPerBeat frames at the project fps
    (clamped to the tempo limits). */
void setFramesPerBeat(te::Edit&, te::TempoSetting&, int framesPerBeat, bool preserveAbsoluteFrameTimes);

/** Sets a time signature without changing when PSG frames sound. The
    denominator sets how long a beat lasts (6/8 beats are half as long as 6/4
    ones), so PSG lists scale framesPerBeat by the same ratio; frame indices are
    never touched. A signature is notation, so unlike a tempo change this does
    not depend on the preserve-timing option. Same single-tempo assumption as
    setTempoBpmRetimingFrames. */
void setTimeSig(te::Edit&, te::TimeSigSetting&, int numerator, int denominator);

// The functions below work on the tempo at the transport position, at the project fps.

/** How many frames a beat lasts. */
double getFramesPerBeat(te::Edit&);

/** Sets the tempo so a beat lasts framesPerBeat frames (clamped to the tempo limits). */
void setFramesPerBeat(te::Edit&, int framesPerBeat, bool preserveAbsoluteFrameTimes);

/** Sets the tempo to the bpm nearest to the given one whose beat is a whole number of frames, and returns it. */
double setBpmSnappedToFrames(te::Edit&, double bpm, bool preserveAbsoluteFrameTimes);

}  // namespace PsgTiming

}  // namespace MoTool
