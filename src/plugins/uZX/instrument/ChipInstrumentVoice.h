#pragma once

#include <cmath>
#include "../midi_effects/MPEEffectVoice.h"
#include "../midi_effects/MidiEffect.h"

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
        ampAdsr.setSampleRate(playRate);
        pitchAdsr.setSampleRate(playRate);

        // ampAdsr.setParameters({ 0.0f, 0.0f, 1.0f, 0.0f });
        // pitchAdsr.setParameters({ 0.0f, 0.0f, 1.0f, 0.0f });
        ampAdsr.setParameters({ 0.0f, 0.0f, 1.0f, 1.0f });
        pitchAdsr.setParameters({ 0.0f, 0.0f, 1.0f, 1.0f });

        lastAftertouch = -1;
    }

    //==============================================================================
    /** Called when a new note starts on this voice. */
    void noteStarted() {
        // DBG("Note started " << currentlyPlayingNote.initialNote);
        activeNote.setCurrentAndTargetValue(currentlyPlayingNote.initialNote);
        ampAdsr.noteOn();
        pitchAdsr.noteOn();
        firstStep = true;
    }

    /** Called when the currently playing note stops. */
    void noteStopped(bool allowTailOff) {
        // DBG("Note stopped " << currentlyPlayingNote.initialNote);
        if (allowTailOff) {
            ampAdsr.noteOff();
            pitchAdsr.noteOff();
        } else {
            ampAdsr.reset();
            pitchAdsr.reset();
            // we can not call clearCurrentNote() here because we should emit NoteOff in renderNextStep after that
            // clearCurrentNote();
        }
    }

    /** Called when note pressure changes (MPE). */
    void notePressureChanged() {
        // TODO: Implement pressure change logic
        DBG("ChipInstrumentVoice Note pressure changed");
    }

//     /** Called when note pitchbend changes (MPE). */
//     void notePitchbendChangedl() {
//         // TODO: Implement pitchbend change logic
//     }

//     /** Called when note timbre changes (MPE). */
//     void noteTimbreChanged() {
//         // TODO: Implement timbre change logic
//     }

//     /** Called when note key state changes (sustain pedal etc). */
//     void noteKeyStateChanged() {
//         // TODO: Implement key state change logic
//     }

    void renderNextStep(MidiBufferContext& c, double timeOffset) {
        // 1. First check for noteOff state
        if (ampAdsr.getState() == te::LinEnvelope::State::idle && !firstStep) {
            DBG("NoteOff " << currentlyPlayingNote.initialNote << " time = " << c.playPosition.inSeconds() + timeOffset);
            c.buffer.addMidiMessage(
                MidiMessage::noteOff(
                    currentlyPlayingNote.midiChannel,
                    currentlyPlayingNote.initialNote,
                    0.0f
                ),
                timeOffset, 0
            );

            ampAdsr.reset();
            pitchAdsr.reset();
            clearCurrentNote();
            return;
        }

        jassert(isActive());

        // 2. Not on if firstStep
        if (firstStep) {
            firstStep = false;
            DBG("NoteOn " << currentlyPlayingNote.initialNote << " time = " << c.playPosition.inSeconds() + timeOffset);
            c.buffer.addMidiMessage(
                MidiMessage::noteOn(
                    currentlyPlayingNote.midiChannel,
                    currentlyPlayingNote.initialNote,
                    currentlyPlayingNote.noteOnVelocity.asUnsignedFloat()
                ),
                timeOffset, 0
            );
        }

        // 3. get state and advance parameters
        // TODO update parameters, adsrs, lfos, steps, mods
        auto aftertouch = roundToInt(ampAdsr.getNextSample() * 127.0f);
        auto pitchOffset = pitchAdsr.getNextSample();
        ignoreUnused(pitchOffset);

        // 4. TODO output note MPE params if they are changed from the last step
        // DBG("Note " << currentlyPlayingNote.initialNote
        //     << ", AT = " << aftertouch
        //     << ", time = " << c.playPosition.inSeconds() + timeOffset
        // );
        if (aftertouch != lastAftertouch) {
            lastAftertouch = aftertouch;
            c.buffer.addMidiMessage(
                MidiMessage::aftertouchChange(
                    currentlyPlayingNote.midiChannel,
                    currentlyPlayingNote.initialNote,
                    aftertouch
                ),
                timeOffset, 0
            );
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

        // DBG("voice block " << currentlyPlayingNote.initialNote << "(" << (int) noteOnOrder << ")"
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
        //     DBG("-->");
        //     c.debugMidiBuffer();
        //     DBG("/--");
        // }
    }

private:
    constexpr inline static double playRate = 50.0;
    juce::LinearSmoothedValue<float> activeNote;
    bool firstStep = false;
    te::LinEnvelope ampAdsr;
    te::LinEnvelope pitchAdsr;

    int lastAftertouch = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentVoice)
};

}  // namespace MoTool::uZX