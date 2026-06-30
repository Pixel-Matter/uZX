#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool {

//==============================================================================
/** Helper for changing an edit's tempo while keeping imported PSG frames pinned
    to their absolute (wall-clock) times.

    PSG frames carry a tempo-independent machine-frame index (frameIndex/frameRate
    seconds). A BPM change re-maps beats to time, so after setting the new tempo we
    re-derive each frame's beat from its frame index — keeping PSG playback fixed in
    time by construction, with no snapshot/restore needed.
*/
namespace PsgTiming {

/** Sets tempo.getBpm() to the new value, then retimes every PSG frame in the edit
    from its frame index so PSG timing stays fixed in wall-clock time. */
void setTempoBpmRetimingFrames(te::Edit&, te::TempoSetting&, double bpm);

}  // namespace PsgTiming

}  // namespace MoTool
