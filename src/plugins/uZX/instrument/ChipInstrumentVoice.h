#pragma once

#include <cmath>
#include "MPEEffectVoice.h"
#include "MidiEffect.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "tracktion_core/utilities/tracktion_Time.h"

namespace MoTool::uZX {

    namespace te = tracktion;

//==============================================================================
/**
    A voice implementation for chiptune-style instruments using CRTP pattern.

    This voice handles the rendering and control of individual notes for
    chiptune instruments, providing efficient callback dispatch through
    compile-time polymorphism.

    @see MPEEffectVoice, ChipInstrument
*/
class ChipInstrumentVoice
    : public MPEEffectVoice<ChipInstrumentVoice>
{
public:
    ChipInstrumentVoice() {
        // FIXME hardcoded parameters
        ampAdsr.setSampleRate(50);
        pitchAdsr.setSampleRate(50);

        ampAdsr.setParameters({ 0.0f, 0.0f, 1.0f, 0.0f });
        pitchAdsr.setParameters({ 0.0f, 0.0f, 1.0f, 0.0f });
    }

    //==============================================================================
    /** Called when a new note starts on this voice. */
    void noteStarted() {
        DBG("Note started " << currentlyPlayingNote.initialNote);
        activeNote.setCurrentAndTargetValue(currentlyPlayingNote.initialNote);
        ampAdsr.noteOn();
        pitchAdsr.noteOn();
    }

    /** Called when the currently playing note stops. */
    void noteStopped(bool allowTailOff) {
        DBG("Note stopped " << currentlyPlayingNote.initialNote);
        // if (allowTailOff)
        // {
        //     ampAdsr.noteOff();
        //     pitchAdsr.noteOff();
        // }
        // else
        // {
            ampAdsr.reset();
            pitchAdsr.reset();
            clearCurrentNote();
            // isPlaying = false;
            // isQuickStop = false;
        // }
    }

    /** Called when note pressure changes (MPE). */
    void notePressureChanged() {
        // TODO: Implement pressure change logic
        DBG("ChipInstrumentVoice Note pressure changed");
    }

//     /** Called when note pitchbend changes (MPE). */
//     void notePitchbendChangedImpl() {
//         // TODO: Implement pitchbend change logic
//     }

//     /** Called when note timbre changes (MPE). */
//     void noteTimbreChangedImpl() {
//         // TODO: Implement timbre change logic
//     }

//     /** Called when note key state changes (sustain pedal etc). */
//     void noteKeyStateChangedImpl() {
//         // TODO: Implement key state change logic
//     }

    void renderNextStep(tracktion::MidiMessageArray& midi, te::TimePosition time) {
        // TODO update parameters, adsrs, lfos, steps, mods
        // TODO output note on / off
        // output note pressure while active
        DBG("renderNextStep for note " << currentlyPlayingNote.initialNote << ", " << (isActive() ? "active " : "inactive")
            << ", time = " << time.inSeconds());
    }

    /** Renders the next block of MIDI output for this voice. */
    void renderNextBlock(MidiBufferContext& c) {
        // current block may not contain time point for midi output (block length is less than playRate)
        // I do not sure if we should chop it here but not in parent midi instrument
        // below is placeholder implementation

        const auto quantizedStart = tracktion::TimePosition::fromSeconds(
            std::ceil(c.playPosition.inSeconds() * playRate) / playRate
        );
        const auto end = c.playPosition + c.duration;
        const auto step = tracktion::TimeDuration::fromSeconds(1.0 / playRate);

        for (auto time = quantizedStart; time < end; time = time + step) {
            renderNextStep(c.buffer, time);
        }
    }

private:
    constexpr inline static double playRate = 50.0;
    juce::LinearSmoothedValue<float> activeNote;
    bool isPlaying = false;
    te::LinEnvelope ampAdsr;
    te::LinEnvelope pitchAdsr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentVoice)
};

}  // namespace MoTool::uZX