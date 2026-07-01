#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool {

//==============================================================================
/** Helpers for changing an edit's tempo while optionally keeping imported PSG
    frames pinned to their absolute (wall-clock) times.

    A plain BPM change re-maps beats to time, so PSG frames stored as beats would
    shift in time. When preserveAbsoluteFrameTimes is set, every PSG frame in the
    edit is snapshotted before the change and restored to the same wall-clock time
    afterwards, using its raw (unquantised) position so the round-trip is exact.
*/
namespace PsgTiming {

/** Sets tempo.getBpm() to the new value. When preserveAbsoluteFrameTimes is true,
    PSG frame times across the whole edit are held fixed; otherwise frames keep
    their beat positions and move in time as usual. */
void setTempoBpmPreservingFrames(te::Edit&, te::TempoSetting&, double bpm,
                                 bool preserveAbsoluteFrameTimes);

}  // namespace PsgTiming

}  // namespace MoTool
