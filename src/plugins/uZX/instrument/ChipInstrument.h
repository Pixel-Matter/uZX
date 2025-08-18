#pragma once

#include "MPEEffect.h"
#include "ChipInstrumentVoice.h"
#include "juce_core/juce_core.h"

namespace MoTool::uZX {

//==============================================================================
/**
    Represents a chiptune-style instrument that uses MPE effects.
    Used by ChipInstrumentPlugin
*/
class ChipInstrument : public MPEEffect<ChipInstrumentVoice> {
public:
    ChipInstrument();

    ~ChipInstrument();

    void reset();
    // void applyToBuffer(const tracktion::MidiMessageArray& midiIn, tracktion::MidiMessageArray& midiOut);
    double getTailLength() const;
    void setCurrentTempo(float newTempo);

    CriticalSection& getVoiceLock();

    void restoreStateFromValueTree(const ValueTree& state);

private:
    CriticalSection voiceLock;
    float currentTempo;
};

}  // namespace MoTool::uZX