#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool {

//==============================================================================

juce::MidiMessageSequence createPsgPlaybackMidiSequence(const te::MidiList& list, const te::MidiClip& clip, te::MidiList::TimeBase timeBase, bool generateMPE);

}  // namespace MoTool