#pragma once

#include <cmath>
#include <utility>
#include "../midi_effects/MPEEffectVoice.h"
#include "../midi_effects/NoteFrame.h"

namespace MoTool::uZX {

namespace te = tracktion;

String toString(te::LinEnvelope adsr);

inline static String& operator<< (String& string1, const te::LinEnvelope& adsr) {
    return string1 += toString(adsr);
}

//==============================================================================
/**
    A voice implementation for chiptune-style instruments using CRTP pattern.

    This voice handles the rendering and control of individual notes for
    chiptune instruments, providing efficient callback dispatch through
    compile-time polymorphism.

    @see MPEEffectVoice, ChipInstrument
*/
template <class OwnerMidiFx>
class ChipInstrumentVoice
    : public MPEEffectVoice<ChipInstrumentVoice<OwnerMidiFx>, OwnerMidiFx>
{
public:
    ChipInstrumentVoice(OwnerMidiFx& owner)
        : MPEEffectVoice<ChipInstrumentVoice<OwnerMidiFx>, OwnerMidiFx>(owner) {
        // The envelopes advance once a frame
        ampAdsr.setSampleRate(noteFrameRate);
        pitchAdsr.setSampleRate(noteFrameRate);

        // pitchAdsr.setParameters({ 0.0f, 0.0f, 1.0f, 0.5f });
        pitchAdsr.setParameters({ 0.0f, 0.0f, 0.0f, 0.0f });
        pitchDepth = 0.0; // in semitones
        ampLevel = 1.0f;  // 100% default
    }

    //==============================================================================
    /** Called by VoiceManager when a new note starts on this voice. */
    void noteStarted() {
        // DBG("Note started " << currentlyPlayingNote.initialNote << " (" << (int) noteOnOrder << ") "
        //     << " state = " << currentlyPlayingNote.keyState
        //     << "\n---------------------------"
        // );

        // updating params only once at the start of the note
        updateParams();

        activeNote.setCurrentAndTargetValue(this->currentlyPlayingNote.initialNote);
        ampAdsr.noteOn();
        pitchAdsr.noteOn();
        // DBG("ADSR: " << ampAdsr);
        triggerNote = true;
    }

    /** Called by VoiceManager when the currently playing note stops. */
    void noteStopped(bool allowTailOff) {
        if (allowTailOff) {
            ampAdsr.noteOff();
            pitchAdsr.noteOff();
            // DBG("noteStopped+tail " << currentlyPlayingNote.initialNote << " (" << (int) noteOnOrder << ") "
            //     << " state = " << currentlyPlayingNote.keyState
            //     << "\n---------------------------"
            // );
        } else {
            ampAdsr.reset();
            pitchAdsr.reset();
            // DBG("noteStopped-tail " << currentlyPlayingNote.initialNote << " (" << (int) noteOnOrder << ") "
            //     << " state = " << currentlyPlayingNote.keyState
            //     << "\n---------------------------"
            // );
            // not clearCurrentNote() here: the note ends in renderFrame, where the encoder sees it gone
        }
        // DBG("ADSR: " << ampAdsr);
    }

    /** Called when note pressure changes (MPE). */
    void notePressureChanged() {
        // TODO: Implement pressure change logic
        // DBG("Note pressure changed " << currentlyPlayingNote.initialNote
        //     << " to " << currentlyPlayingNote.pressure.asUnsignedFloat()
        // );
    }

    /** Called when note pitchbend changes (MPE). */
    void notePitchbendChanged() {
        // TODO: Implement pitchbend change logic
        // DBG("Note pitchbend changed " << currentlyPlayingNote.initialNote
        //     << " to " << currentlyPlayingNote.pitchbend.asSignedFloat()
        // );
    }

    /** Called when note timbre changes (MPE). */
    void noteTimbreChanged() {
        // TODO: Implement timbre change logic
    }

    void updateParams() {
        auto& params = this->midiFx.oscParams;
        ampAdsr.setParameters({
            params.ampAttack.getLiveValue(),
            params.ampDecay.getLiveValue(),
            params.ampSustain.getLiveValue() / 100.0f,
            params.ampRelease.getLiveValue()
        });
        pitchAdsr.setParameters({
            params.pitchAttack.getLiveValue(),
            params.pitchDecay.getLiveValue(),
            params.pitchSustain.getLiveValue() / 100.0f,
            params.pitchRelease.getLiveValue()
        });
        pitchDepth = params.pitchDepth.getLiveValue();
        ampLevel = params.ampLevel.getLiveValue() / 100.0f;
    }

    /** Plays the next frame: adds this voice to it, or ends the note once its amp envelope has. */
    void renderFrame(NoteFrame& frame) {
        // To always reach peak value between A/D phases we should sample after advance, not before
        ampAdsr.getNextSample();
        pitchAdsr.getNextSample();

        if (shouldStop() && !triggerNote) {
            resetNote();
            return;
        }

        frame.add({
            .channel = this->currentlyPlayingNote.midiChannel,
            .key = this->currentlyPlayingNote.initialNote,
            .pitch = computeCurrentNotePitch(),
            .level = computeCurrentLevel(),
            .attack = std::exchange(triggerNote, false)
        });
    }

private:
    bool shouldStop() /* const */ {
        return ampAdsr.getState() == te::LinEnvelope::State::idle;
    }

    void resetNote() {
        ampAdsr.reset();
        pitchAdsr.reset();
        this->clearCurrentNote();
    }

    float computeCurrentLevel() const {
        auto modLevel = ampAdsr.getEnvelopeValue();
        auto velocity = this->currentlyPlayingNote.noteOnVelocity.asUnsignedFloat();
        auto pressure = this->currentlyPlayingNote.pressure.asUnsignedFloat();
        return jlimit(0.0f, 1.0f, ampLevel * (modLevel * velocity + pressure));
    }

    double computeCurrentNotePitch() const {
        // TODO inverted logic of pitchAdsr, so Sustain == 100% means base pitch, no offset
        // then after noteOff on release pitch offset will be pitchDepth
        // Should read some synth manual, for example Roland SH-201
        auto pitchOffset = pitchAdsr.getEnvelopeValue() * pitchDepth;
        return this->currentlyPlayingNote.initialNote + this->currentlyPlayingNote.totalPitchbendInSemitones + pitchOffset;
    }

    //==============================================================================

    // Parameters
    te::LinEnvelope ampAdsr;
    te::LinEnvelope pitchAdsr;
    double pitchDepth;  // for pitchAdsr, in semitones
    float ampLevel;     // overall amplitude level (0.0 to 1.0)

    // TODO keyfollow modifier for pitchBend for example will give us desired effect
    // on chord starting out of tune for every note and then going to tune

    // Note state
    juce::LinearSmoothedValue<float> activeNote;
    bool triggerNote = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentVoice)
};

}  // namespace MoTool::uZX
