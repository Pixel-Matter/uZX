#pragma once

#include <JuceHeader.h>

#include "MidiEffect.h"
#include "MPEEffectVoice.h"
#include "VoiceManager.h"


namespace MoTool::uZX {

namespace te = tracktion;

template <class Voice>
class MPEInstrumentFx : public MPEInstrument::Listener {
public:
    MPEInstrumentFx()
        : mpeInstrument(defaultMpeInstrument)
    {
        mpeInstrument.enableLegacyMode();
        mpeInstrument.addListener(this);
    }

    ~MPEInstrumentFx() override {
        mpeInstrument.removeListener(this);
    }

    void turnOffAllVoices(bool allowTailOff = false) {
        // Handle all notes off
        // 1. reset voices voiceManager.turnOffAllVoices(allowTailOff);
        mpeInstrument.releaseAllNotes();
    }

    //==========================================================================
    // Render

    void renderNextBlock(MidiBufferContext& c) {
        // Render the next block of MIDI messages
        // DBG("Render from " << c.editPos << " to " << (c.editPos + c.length));
        // TODO render all voices and merge output
    }

    void handleMidiEvent(const MidiMessage& m) {
        if (m.isController())
            handleController(m.getChannel(), m.getControllerNumber(), m.getControllerValue());
        else if (m.isProgramChange())
            handleProgramChange(m.getChannel(), m.getProgramChangeNumber());

        mpeInstrument.processNextMidiEvent(m);
    }

    void operator()(MidiBufferContext& c) {
        te::TimeDuration time {};

        if (c.isAllNotesOff()) {
            turnOffAllVoices(false);
        }

        te::MidiMessageArray midiOut;
        for (auto& m : c.buffer) {
            if (m.getTimeStamp() >= c.length.inSeconds()) {
                break;
            }
            if (m.getTimeStamp() > time.inSeconds()) {
                MidiBufferContext subCtx {midiOut, te::TimeDuration::fromSeconds(m.getTimeStamp()) - time, c.editPos + time};
                renderNextBlock(subCtx);
                time = te::TimeDuration::fromSeconds(m.getTimeStamp());
            }
            handleMidiEvent(m);
        }
        MidiBufferContext subCtx {midiOut, c.length - time, c.editPos + time};
        renderNextBlock(subCtx);
        midiOut.sortByTimestamp();
        // midiBuffer.swapWith(midiOut);
    }

protected:
    //==============================================================================
    // MPEInstrument::Listener callbacks (connect MPE events to voice management)

    /** Called when a new MPE note is added. */
    void noteAdded(MPENote newNote) override {
        DBG("Note added: " << newNote.initialNote);
        // if (auto* voice = voiceManager.findFreeVoice(newNote, voiceManager.isVoiceStealingEnabled())) {
        //     voiceManager.startVoice(voice, newNote);
        // }
    }

    /** Called when an MPE note is released. */
    void noteReleased(MPENote finishedNote) override {
        DBG("Note released: " << finishedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(
        //     finishedNote, [this, finishedNote](Voice* voice) { voiceManager.stopVoice(voice, finishedNote, true); });
    }

    /** Called when an MPE note's pressure changes. */
    void notePressureChanged(MPENote changedNote) override {
        DBG("Note pressure changed: " << changedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
        //     voice->currentlyPlayingNote = changedNote;
        //     voice->notePressureChanged();
        // });
    }

    /** Called when an MPE note's pitchbend changes. */
    void notePitchbendChanged(MPENote changedNote) override {
        DBG("Note pitchbend changed: " << changedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
        //     voice->currentlyPlayingNote = changedNote;
        //     voice->notePitchbendChanged();
        // });
    }

    /** Called when an MPE note's timbre changes. */
    void noteTimbreChanged(MPENote changedNote) override {
        DBG("Note timbre changed: " << changedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
        //     voice->currentlyPlayingNote = changedNote;
        //     voice->noteTimbreChanged();
        // });
    }

    /** Called when an MPE note's key state changes. */
    void noteKeyStateChanged(MPENote changedNote) override {
        DBG("Note key state changed: " << changedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
        //     voice->currentlyPlayingNote = changedNote;
        //     voice->noteKeyStateChanged();
        // });
    }

    /** Called when a MIDI controller changes. */
    void handleController(int midiChannel, int controllerNumber, int controllerValue) {
        DBG("Controller changed: " << controllerNumber << " = " << controllerValue);
        // Implement controller handling logic here
    }

    void handleProgramChange(int midiChannel, int programNumber) {
        DBG("Program change: " << programNumber);
        // Implement program change handling logic here
    }

    MPEInstrument& mpeInstrument;
    VoiceManager<Voice> voiceManager;  // Pure voice management

private:
    MPEInstrument defaultMpeInstrument;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MPEInstrumentFx)
};


// // EVERYTHING BELOW IS WRONG !!!

// //==============================================================================
// /**
//     Complete MIDI MPE Instrument using composition of specialized managers.

//     This class orchestrates MPE processing and voice management using a modular
//     architecture. It combines:
//     - MPEInstrumentManager: Pure MPE/MIDI logic
//     - VoiceManager: Pure voice allocation and rendering
//     - MPEInstrument::Listener: Event callbacks to connect MPE events to voices

//     To create a synthesiser, you'll need to create a subclass of MPEEffectVoice
//     which can play back one sound at a time.

//     Then you can use the addVoice() methods to give the synthesiser a set of voices
//     it can use to play notes. If you only give it one voice it will be monophonic -
//     the more voices it has, the more polyphony it'll have available.

//     The renderNextBlock() method processes MIDI through the MPE manager and
//     renders audio through the voice manager.

//     @see MPEInstrumentManager, VoiceManager, MPEEffectVoice, MPENote

//     @tags{Audio}
// */
// template <class Voice, class Derived>
// class MPEEffect : public MPEInstrument::Listener {
// public:
//     //==============================================================================
//     /** Constructor using default MPE manager. */
//     MPEEffect()
//         : mpeInstrument(defaultMpeInstrument)
//     {
//         mpeInstrument.addListener(this);
//     }

//     explicit MPEEffect(MPEInstrument& manager)
//         : mpeInstrument(manager)
//     {
//         mpeInstrument.addListener(this);
//     }

//     ~MPEEffect() override {
//         mpeInstrument.removeListener(this);
//     }

//     Derived& self() {
//         return static_cast<Derived&>(*this);
//     }

//     //==============================================================================

// //     /** Sets the current play rate for rendering. */
// //     void setCurrentPlayRate(double newRate) {
// //         voiceManager.setCurrentPlayRate(newRate);
// //     }

// //     /** Returns the current play rate. */
// //     double getPlayRate() const noexcept {
// //         return voiceManager.getPlayRate();
// //     }

// //     //==============================================================================
// //     // Main Rendering Entry Point

// //     /** Processes MIDI and renders MPE output. */
// //     void renderNextBlock(const MidiBuffer& inputMidi, MidiBuffer& outputMidi, int startSample, int numSamples) {
// //         // Process MIDI events through MPE manager and render voices
// //         auto prevSample = startSample;
// //         const auto endSample = startSample + numSamples;

// //         for (auto it = inputMidi.findNextSamplePosition(startSample); it != inputMidi.cend(); ++it) {
// //             const auto metadata = *it;
// //             if (metadata.samplePosition >= endSample)
// //                 break;

// //             // Render voices for samples before this MIDI event
// //             if (metadata.samplePosition > prevSample) {
// //                 voiceManager.renderNextBlock(outputMidi, prevSample, metadata.samplePosition - prevSample);
// //             }

// //             // Process MIDI event through MPE manager (triggers listener callbacks)
// //             handleMidiEvent(metadata.getMessage());
// //             prevSample = metadata.samplePosition;
// //         }

// //         // Render remaining samples
// //         if (prevSample < endSample) {
// //             voiceManager.renderNextBlock(outputMidi, prevSample, endSample - prevSample);
// //         }
// //     }

//     //==============================================================================
//     // MIDI Handling

//     /** Handles incoming MIDI events. */
//     void handleMidiEvent(const tracktion::MidiMessageWithSource& message) {
//         DBG("MPEEffect handling MIDI event: " << message.getDescription() << " at " << message.getTimeStamp());
//         mpeInstrument.processNextMidiEvent(message);
//     }

// //     /** Callback for MIDI controller messages. */
// //     void handleController(int /*midiChannel*/, int /*controllerNumber*/, int /*controllerValue*/) {}

// //     /** Callback for MIDI program change messages. */
// //     void handleProgramChange(int /*midiChannel*/, int /*programNumber*/) {}

// //     //==============================================================================
// //     // Rendering Control

// //     // /** Sets the minimum rendering subdivision size. */
// //     // void setMinimumRenderingSubdivisionSize(int numSamples, bool shouldBeStrict = false) {
// //     //     mpeManager.setMinimumRenderingSubdivisionSize(numSamples, shouldBeStrict);
// //     // }

//     void turnOffAllVoices(bool allowTailOff) {
//         // TODO uncomment
//         // voiceManager.turnOffAllVoices(allowTailOff);

//         // finally make sure the MPE Instrument also doesn't have any notes anymore.
//         mpeInstrument.releaseAllNotes();
//     }

//     void applyToBuffer(tracktion::MidiMessageArray& midiBuffer, double len, double editPos) {
//         double time = 0.0;

//         // EVERYTHING IS WRONG !!!
//         // MIDI FX IS JUST A FUNCTOR

//         tracktion::MidiMessageArray midiOut;
//         for (auto& m : midiBuffer) {
//             if (m.getTimeStamp() >= len) {
//                 break;
//             }
//             if (m.getTimeStamp() > time) {
//                 self().renderNextBlock(midiOut, time, m.getTimeStamp() - time, editPos + time);
//                 time = m.getTimeStamp();
//             }
//             handleMidiEvent(m);
//         }
//         self().renderNextBlock(midiOut, time, len - time, editPos + time);
//         midiOut.sortByTimestamp();
//         // midiBuffer.swapWith(midiOut);
//     }

// protected:
// //     //==============================================================================
// //     // MPEInstrument::Listener callbacks (connect MPE events to voice management)

// //     /** Called when a new MPE note is added. */
// //     void noteAdded(MPENote newNote) override {
// //         if (auto* voice = voiceManager.findFreeVoice(newNote, voiceManager.isVoiceStealingEnabled())) {
// //             voiceManager.startVoice(voice, newNote);
// //         }
// //     }

// //     /** Called when an MPE note is released. */
// //     void noteReleased(MPENote finishedNote) override {
// //         voiceManager.forEachVoicePlayingNote(
// //             finishedNote, [this, finishedNote](Voice* voice) { voiceManager.stopVoice(voice, finishedNote, true); });
// //     }

// //     /** Called when an MPE note's pressure changes. */
// //     void notePressureChanged(MPENote changedNote) override {
// //         voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
// //             voice->currentlyPlayingNote = changedNote;
// //             voice->notePressureChanged();
// //         });
// //     }

// //     /** Called when an MPE note's pitchbend changes. */
// //     void notePitchbendChanged(MPENote changedNote) override {
// //         voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
// //             voice->currentlyPlayingNote = changedNote;
// //             voice->notePitchbendChanged();
// //         });
// //     }

// //     /** Called when an MPE note's timbre changes. */
// //     void noteTimbreChanged(MPENote changedNote) override {
// //         voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
// //             voice->currentlyPlayingNote = changedNote;
// //             voice->noteTimbreChanged();
// //         });
// //     }

// //     /** Called when an MPE note's key state changes. */
// //     void noteKeyStateChanged(MPENote changedNote) override {
// //         voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
// //             voice->currentlyPlayingNote = changedNote;
// //             voice->noteKeyStateChanged();
// //         });
// //     }

// protected:
//     //==============================================================================
//     /** @internal */
//     MPEInstrument& mpeInstrument;

// private:
//     //==============================================================================
//     MPEInstrument defaultMpeInstrument { MPEZone (MPEZone::Type::lower, 15) };
//     // MPEInstrumentManager mpeManager;   // Pure MPE/MIDI logic
//     // VoiceManager<Voice> voiceManager;  // Pure voice management

//     JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MPEEffect)
// };


}  // namespace MoTool::uZX
