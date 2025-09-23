#pragma once

#include <cmath>
#include "../midi_effects/MPEEffectVoice.h"
#include "../midi_effects/MidiEffect.h"

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
        // FIXME hardcoded parameters
        ampAdsr.setSampleRate(playRate);
        pitchAdsr.setSampleRate(playRate);

        // pitchAdsr.setParameters({ 0.0f, 0.0f, 1.0f, 0.5f });
        pitchAdsr.setParameters({ 0.0f, 0.0f, 0.0f, 0.0f });
        pitchDepth = 0.0; // in semitones

        lastLevel = -1;
        lastNotePitch = -1.0f;
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
            // we should not call clearCurrentNote() here because we should emit NoteOff in renderNextStep after that
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
            params.ampAttack.getCurrentValue(),
            params.ampDecay.getCurrentValue(),
            params.ampSustain.getCurrentValue() / 100.0f,
            params.ampRelease.getCurrentValue()
        });
        pitchAdsr.setParameters({
            params.pitchAttack.getCurrentValue(),
            params.pitchDecay.getCurrentValue(),
            params.pitchSustain.getCurrentValue() / 100.0f,
            params.pitchRelease.getCurrentValue()
        });
        pitchDepth = params.pitchDepth.getCurrentValue();
    }

    void renderNextStep(MidiBufferContext& c, double timeOffset) {
        // DBG("renderNextStep for note " << currentlyPlayingNote.initialNote
        //     << "(" << (int) noteOnOrder << ")"
        //     << " at time = " << c.playPosition.inSeconds() + timeOffset
        //     << (firstStep ? " (first step)" : "")
        // );
        // DBG("ADSR: " << ampAdsr);

        // 1. Advance envelopes
        // To always reach peak value between A/D phases we should sample after advance, not before
        ampAdsr.getNextSample();
        pitchAdsr.getNextSample();

        // 2. Get current values
        auto level = computeCurrentLevel();
        auto notePitch = computeCurrentNotePitch();

        // 4. Render MIDI events if needed

        if (shouldStop() && !triggerNote) {
            // DBG("Emit NoteOff " << currentlyPlayingNote.initialNote
            //     << "(" << (int) noteOnOrder << ")"
            //     << ", state = " << currentlyPlayingNote.keyState
            //     << " time = " << c.playPosition.inSeconds() + timeOffset
            // );
            c.buffer.addMidiMessage(
                MidiMessage::noteOff(
                    this->currentlyPlayingNote.midiChannel,
                    this->currentlyPlayingNote.initialNote,
                    0.0f
                ),
                timeOffset, 0
            );

            resetNote();
            return;
        }

        jassert(this->isActive());

        if (!approximatelyEqual(lastNotePitch, notePitch)) {
            // DBG("Emit PitchBend " << currentlyPlayingNote.initialNote
            //     << "(" << (int) noteOnOrder << ")"
            //     << " to " << notePitch - currentlyPlayingNote.initialNote
            //     << ", time = " << c.playPosition.inSeconds() + timeOffset
            // );
            auto pitchBendValue = juce::jlimit(0, 16383,
                8192 + roundToInt(((notePitch - this->currentlyPlayingNote.initialNote) / pitchBendRange) * 8191.0f)
            );
            // TODO we SHOULD use MPE on MIDI output, so we should send pitch bend on note channel
            // so for example modify output MPENote and then send it to MPEChannelAssigner
            // See MidiClip MIDI MPE output for example tracktion_MidiList.cpp:1981
            c.buffer.addMidiMessage(
                MidiMessage::pitchWheel(
                    this->currentlyPlayingNote.midiChannel,
                    pitchBendValue
                ),
                timeOffset, 0
            );
            lastNotePitch = notePitch;
        }

        if (triggerNote) {
            triggerNote = false;
            // DBG("Emit NoteOn " << currentlyPlayingNote.initialNote << " (" << (int) noteOnOrder << ")"
            //     << " velocity = " << currentlyPlayingNote.noteOnVelocity.asUnsignedFloat()
            //     << " time = " << c.playPosition.inSeconds() + timeOffset);
            c.buffer.addMidiMessage(
                MidiMessage::noteOn(
                    this->currentlyPlayingNote.midiChannel,
                    this->currentlyPlayingNote.initialNote,
                    level
                ),
                timeOffset, 0
            );
            lastLevel = level;
        } else {
            // TODO fix comparison of floats
            if (!approximatelyEqual(level, lastLevel)) {
                // DBG("Emit Aftertouch " << currentlyPlayingNote.initialNote << " (" << (int) noteOnOrder << ")"
                //     << " level = " << level
                //     << " state = " << currentlyPlayingNote.keyState
                //     << " ampAdsr = " << ampAdsr
                //     << ", time = " << c.playPosition.inSeconds() + timeOffset
                // );
                c.buffer.addMidiMessage(
                    MidiMessage::aftertouchChange(
                        this->currentlyPlayingNote.midiChannel,
                        this->currentlyPlayingNote.initialNote,
                        roundToInt(level * 127.0f)
                    ),
                    timeOffset, 0
                );
                lastLevel = level;
            }
        }
    }

    /** Renders the next block of MIDI output for this voice. */
    void renderNextBlock(MidiBufferContext& c) {
        // current block may not contain time point for midi output (block length is less than playRate)
        // I do not sure if we should chop it here but not in parent midi instrument
        // below is placeholder implementation

        // We shouldn't quantize in floats!
        const auto step = roundToInt(c.sampleRate / playRate);
        const auto timeSamples = roundToInt(c.playPosition.inSeconds() * c.sampleRate);
        const int remainder = timeSamples % step;
        const auto sampleQuant = (remainder == 0) ? timeSamples : timeSamples + (step - remainder);
        const auto startSampleQuant = sampleQuant - timeSamples;

        // DBG("voice block " << currentlyPlayingNote.initialNote << "(" << (int) noteOnOrder << ") "
        //     << c.processStartTime() << " - " << c.processEndTime()
        //     << " / " << startSampleQuant << " - " << c.start + c.length
        // );

        for (auto s = startSampleQuant; s < c.start + c.length; s += step) {
            if (s < c.start)
                continue;
            auto timeOffset = s / c.sampleRate;
            // DBG("rendering step at sample: " << s << " timeOffset: " << timeOffset);
            renderNextStep(c, timeOffset);
        }
        // if (c.buffer.isNotEmpty()) {
        //     DBG("-->" << currentlyPlayingNote.initialNote << "(" << (int) noteOnOrder << ")");
        //     c.debugMidiBuffer();
        //     DBG("/--");
        // }
    }

private:
    bool shouldStop() /* const */ {
        return ampAdsr.getState() == te::LinEnvelope::State::idle;
    }

    void resetNote() {
        ampAdsr.reset();
        pitchAdsr.reset();
        lastLevel = -1;
        lastNotePitch = -1.0f;
        this->clearCurrentNote();
    }

    float computeCurrentLevel() const {
        auto modLevel = ampAdsr.getEnvelopeValue();
        auto velocity = this->currentlyPlayingNote.noteOnVelocity.asUnsignedFloat();
        auto pressure = this->currentlyPlayingNote.pressure.asUnsignedFloat();
        return jlimit(0.0f, 1.0f, modLevel * velocity + pressure);
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
    constexpr inline static double playRate = 50.0;
    te::LinEnvelope ampAdsr;
    te::LinEnvelope pitchAdsr;
    double pitchDepth;  // for pitchAdsr, in semitones, can not be more than pitchBendRange

    // TODO as a parameter, for deeper pitch bend range
    // TODO emit MIDI systems messages for pitch bend range on start/reset
    double pitchBendRange = 2.0; // in semitones
    // TODO keyfollow modifier for pitchBendRange for example will give us desired effect
    // on chord starting out of tune for every note and then going to tune

    // Note state
    juce::LinearSmoothedValue<float> activeNote;
    bool triggerNote = false;

    // Last state
    float lastLevel = -1;
    double lastNotePitch = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentVoice)
};

}  // namespace MoTool::uZX