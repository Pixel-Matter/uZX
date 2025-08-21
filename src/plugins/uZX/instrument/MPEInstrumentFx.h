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
        // render all voices and merge output
        voices.forEachActiveVoice([&c](auto* voice) {
            voice->renderNextBlock(c);
        });
        c.buffer.sortByTimestamp();
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
            if (m.getTimeStamp() >= c.duration.inSeconds()) {
                break;
            }
            if (m.getTimeStamp() > time.inSeconds()) {
                MidiBufferContext subCtx {midiOut, te::TimeDuration::fromSeconds(m.getTimeStamp()) - time, c.playPosition + time};
                renderNextBlock(subCtx);
                time = te::TimeDuration::fromSeconds(m.getTimeStamp());
            }
            handleMidiEvent(m);
        }
        MidiBufferContext subCtx {midiOut, c.duration - time, c.playPosition + time};
        renderNextBlock(subCtx);
        midiOut.sortByTimestamp();
        // midiBuffer.swapWith(midiOut);
    }

protected:
    //==============================================================================
    // MPEInstrument::Listener callbacks (connect MPE events to voice management)

    /** Called when a new MPE note is added. */
    void noteAdded(MPENote newNote) override {
        voices.addNote(newNote);
    }

    /** Called when an MPE note is released. */
    void noteReleased(MPENote finishedNote) override {
        voices.releaseNote(finishedNote);
    }

    /** Called when an MPE note's pressure changes. */
    void notePressureChanged(MPENote changedNote) override {
        // DBG("Note pressure changed: " << changedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
        //     voice->currentlyPlayingNote = changedNote;
        //     voice->notePressureChanged();
        // });
    }

    /** Called when an MPE note's pitchbend changes. */
    void notePitchbendChanged(MPENote changedNote) override {
        // DBG("Note pitchbend changed: " << changedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
        //     voice->currentlyPlayingNote = changedNote;
        //     voice->notePitchbendChanged();
        // });
    }

    /** Called when an MPE note's timbre changes. */
    void noteTimbreChanged(MPENote changedNote) override {
        // DBG("Note timbre changed: " << changedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
        //     voice->currentlyPlayingNote = changedNote;
        //     voice->noteTimbreChanged();
        // });
    }

    /** Called when an MPE note's key state changes. */
    void noteKeyStateChanged(MPENote changedNote) override {
        // DBG("Note key state changed: " << changedNote.initialNote);
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
    VoiceManager<Voice> voices;  // Pure voice management

private:
    MPEInstrument defaultMpeInstrument;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MPEInstrumentFx)
};

}  // namespace MoTool::uZX
