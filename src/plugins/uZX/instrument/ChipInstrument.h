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
    ChipInstrument(tracktion::Edit& e);

    ~ChipInstrument();

    void reset();
    double getTailLength() const;
    void setCurrentTempo(float newTempo);
    void applyToBuffer(tracktion::MidiMessageArray& midi, int startSample, int numSamples, double midiOffset);
    void renderFrame(tracktion::MidiMessageArray& midiBuffer, int startSample, int numSamples);

    CriticalSection& getVoiceLock();

    void restoreStateFromValueTree(const ValueTree& state);

private:
    tracktion::Edit& edit;
    CriticalSection voiceLock;
    float currentTempo;
};

}  // namespace MoTool::uZX