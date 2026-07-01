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

}  // namespace PsgTiming

}  // namespace MoTool
