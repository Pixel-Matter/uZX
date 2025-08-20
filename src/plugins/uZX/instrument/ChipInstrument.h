#pragma once

#include <JuceHeader.h>

#include "MidiEffect.h"
#include "MPEInstrumentFx.h"
#include "ChipInstrumentVoice.h"

namespace MoTool::uZX {

//==============================================================================
/**
    Represents a chiptune-style instrument that uses MPE.
    Used by ChipInstrumentPlugin
*/

class ChipInstrumentFx : public MPEInstrumentFx<ChipInstrumentVoice> {
public:
    ChipInstrumentFx() = default;

    // TODO parameters?

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentFx)
};

// class ChipInstrument : public MPEEffect<ChipInstrumentVoice, ChipInstrument> {
// public:
//     ChipInstrument(tracktion::Edit& e);

//     ~ChipInstrument();

//     void reset();
//     double getTailLength() const;
//     void setCurrentTempo(float newTempo);
//     void setPlayRate(double newRate);
//     void renderNextBlock(tracktion::MidiMessageArray& midi, double time, double len, double editPos = 0.0);

//     CriticalSection& getVoiceLock();

//     void restoreStateFromValueTree(const ValueTree& state);

// private:
//     tracktion::Edit& edit;
//     CriticalSection voiceLock;
//     float currentTempo;
//     double playRate { 50.0 };
// };

}  // namespace MoTool::uZX