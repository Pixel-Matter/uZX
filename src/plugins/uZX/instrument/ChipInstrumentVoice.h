#pragma once

#include "MPEEffectVoice.h"

namespace MoTool::uZX {

//==============================================================================
/**
    A voice implementation for chiptune-style instruments using CRTP pattern.
    
    This voice handles the rendering and control of individual notes for 
    chiptune instruments, providing efficient callback dispatch through
    compile-time polymorphism.

    @see MPEEffectVoice, ChipInstrument
*/
class ChipInstrumentVoice : public MPEEffectVoice<ChipInstrumentVoice> {
public:
    //==============================================================================
    /** Called when a new note starts on this voice. */
    void noteStartedImpl() {
        // TODO: Implement note start logic
    }
    
    /** Called when the currently playing note stops. */
    void noteStoppedImpl(bool allowTailOff) {
        (void)allowTailOff;
        // TODO: Implement note stop logic
        clearCurrentNote();
    }
    
    /** Called when note pressure changes (MPE). */
    void notePressureChangedImpl() {
        // TODO: Implement pressure change logic
    }
    
    /** Called when note pitchbend changes (MPE). */
    void notePitchbendChangedImpl() {
        // TODO: Implement pitchbend change logic
    }
    
    /** Called when note timbre changes (MPE). */
    void noteTimbreChangedImpl() {
        // TODO: Implement timbre change logic
    }
    
    /** Called when note key state changes (sustain pedal etc). */
    void noteKeyStateChangedImpl() {
        // TODO: Implement key state change logic
    }
    
    /** Renders the next block of MIDI output for this voice. */
    void renderNextBlockImpl(MidiBuffer& midiBuffer, int startSample, int numSamples) {
        (void)midiBuffer;
        (void)startSample;
        (void)numSamples;
        // TODO: Implement MIDI rendering logic
    }
};

}  // namespace MoTool::uZX