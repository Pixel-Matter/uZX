#pragma once

#include <JuceHeader.h>

#include "MPEEffectVoice.h"
#include "MPEInstrumentManager.h"
#include "VoiceManager.h"


namespace MoTool::uZX {

//==============================================================================
/**
    Complete MPE effect using composition of specialized managers.

    This class orchestrates MPE processing and voice management using a modular
    architecture. It combines:
    - MPEInstrumentManager: Pure MPE/MIDI logic
    - VoiceManager: Pure voice allocation and rendering
    - MPEInstrument::Listener: Event callbacks to connect MPE events to voices

    To create a synthesiser, you'll need to create a subclass of MPEEffectVoice
    which can play back one sound at a time.

    Then you can use the addVoice() methods to give the synthesiser a set of voices
    it can use to play notes. If you only give it one voice it will be monophonic -
    the more voices it has, the more polyphony it'll have available.

    The renderNextBlock() method processes MIDI through the MPE manager and
    renders audio through the voice manager.

    @see MPEInstrumentManager, VoiceManager, MPEEffectVoice, MPENote

    @tags{Audio}
*/
template <class Voice>
class MPEEffect : public MPEInstrument::Listener {
public:
    //==============================================================================
    /** Constructor using default MPE manager. */
    MPEEffect()
        : mpeInstrument(defaultMpeInstrument)
    {
        mpeInstrument.addListener(this);
    }

    explicit MPEEffect(MPEInstrument& manager)
        : mpeInstrument(manager)
    {
        mpeInstrument.addListener(this);
    }

    ~MPEEffect() override {
        mpeInstrument.removeListener(this);
    }

    //==============================================================================

//     /** Sets the current play rate for rendering. */
//     void setCurrentPlayRate(double newRate) {
//         voiceManager.setCurrentPlayRate(newRate);
//     }

//     /** Returns the current play rate. */
//     double getPlayRate() const noexcept {
//         return voiceManager.getPlayRate();
//     }

//     //==============================================================================
//     // Main Rendering Entry Point

//     /** Processes MIDI and renders MPE output. */
//     void renderNextBlock(const MidiBuffer& inputMidi, MidiBuffer& outputMidi, int startSample, int numSamples) {
//         // Process MIDI events through MPE manager and render voices
//         auto prevSample = startSample;
//         const auto endSample = startSample + numSamples;

//         for (auto it = inputMidi.findNextSamplePosition(startSample); it != inputMidi.cend(); ++it) {
//             const auto metadata = *it;
//             if (metadata.samplePosition >= endSample)
//                 break;

//             // Render voices for samples before this MIDI event
//             if (metadata.samplePosition > prevSample) {
//                 voiceManager.renderNextBlock(outputMidi, prevSample, metadata.samplePosition - prevSample);
//             }

//             // Process MIDI event through MPE manager (triggers listener callbacks)
//             handleMidiEvent(metadata.getMessage());
//             prevSample = metadata.samplePosition;
//         }

//         // Render remaining samples
//         if (prevSample < endSample) {
//             voiceManager.renderNextBlock(outputMidi, prevSample, endSample - prevSample);
//         }
//     }

    //==============================================================================
    // MIDI Handling

    /** Handles incoming MIDI events. */
    void handleMidiEvent(const MidiMessage& message) {
        mpeInstrument.processNextMidiEvent(message);
    }

//     /** Callback for MIDI controller messages. */
//     void handleController(int /*midiChannel*/, int /*controllerNumber*/, int /*controllerValue*/) {}

//     /** Callback for MIDI program change messages. */
//     void handleProgramChange(int /*midiChannel*/, int /*programNumber*/) {}

//     //==============================================================================
//     // Rendering Control

//     // /** Sets the minimum rendering subdivision size. */
//     // void setMinimumRenderingSubdivisionSize(int numSamples, bool shouldBeStrict = false) {
//     //     mpeManager.setMinimumRenderingSubdivisionSize(numSamples, shouldBeStrict);
//     // }

    void turnOffAllVoices(bool allowTailOff) {
        // TODO uncomment
        // voiceManager.turnOffAllVoices(allowTailOff);

        // finally make sure the MPE Instrument also doesn't have any notes anymore.
        mpeInstrument.releaseAllNotes();
    }

// protected:
//     //==============================================================================
//     // MPEInstrument::Listener callbacks (connect MPE events to voice management)

//     /** Called when a new MPE note is added. */
//     void noteAdded(MPENote newNote) override {
//         if (auto* voice = voiceManager.findFreeVoice(newNote, voiceManager.isVoiceStealingEnabled())) {
//             voiceManager.startVoice(voice, newNote);
//         }
//     }

//     /** Called when an MPE note is released. */
//     void noteReleased(MPENote finishedNote) override {
//         voiceManager.forEachVoicePlayingNote(
//             finishedNote, [this, finishedNote](Voice* voice) { voiceManager.stopVoice(voice, finishedNote, true); });
//     }

//     /** Called when an MPE note's pressure changes. */
//     void notePressureChanged(MPENote changedNote) override {
//         voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
//             voice->currentlyPlayingNote = changedNote;
//             voice->notePressureChanged();
//         });
//     }

//     /** Called when an MPE note's pitchbend changes. */
//     void notePitchbendChanged(MPENote changedNote) override {
//         voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
//             voice->currentlyPlayingNote = changedNote;
//             voice->notePitchbendChanged();
//         });
//     }

//     /** Called when an MPE note's timbre changes. */
//     void noteTimbreChanged(MPENote changedNote) override {
//         voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
//             voice->currentlyPlayingNote = changedNote;
//             voice->noteTimbreChanged();
//         });
//     }

//     /** Called when an MPE note's key state changes. */
//     void noteKeyStateChanged(MPENote changedNote) override {
//         voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
//             voice->currentlyPlayingNote = changedNote;
//             voice->noteKeyStateChanged();
//         });
//     }

protected:
    //==============================================================================
    /** @internal */
    MPEInstrument& mpeInstrument;

private:
    //==============================================================================
    MPEInstrument defaultMpeInstrument { MPEZone (MPEZone::Type::lower, 15) };
    // MPEInstrumentManager mpeManager;   // Pure MPE/MIDI logic
    // VoiceManager<Voice> voiceManager;  // Pure voice management

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MPEEffect)
};


}  // namespace MoTool::uZX
