#pragma once

#include <JuceHeader.h>

#include <memory>

namespace MoTool {

class PsgClip;

/** A PSG clip's right-click menu > Tempo inspector: what the tempo detection (PsgTempo::analyse)
    read in the clip's tune and how it decided. Where a row grid holds over the tune, the row
    evidence frame by frame against the rows detected, how the evidence repeats by lag, and the
    grids and beats that were weighed. A beat cursor, set on any of them, shows the tempo it would
    give and can be made the project's, to hear it against the tune. Follows playback and closes
    with the clip. It reads the registers alone: a tracker module's own rows are not kept with
    the clip. */
void showTempoInspector(PsgClip&);

/** The inspector's content, for showTempoInspector and tests. */
std::unique_ptr<juce::Component> createTempoInspector(PsgClip&);

}  // namespace MoTool
