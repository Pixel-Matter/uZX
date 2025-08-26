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
    {
        mpeInstrument.enableLegacyMode();
        mpeInstrument.addListener(this);
    }

    ~MPEInstrumentFx() override {
        mpeInstrument.removeListener(this);
    }

    // Handle all notes off
    void turnOffAllVoices(bool allowTailOff = false) {
        voices.turnOffAllVoices(allowTailOff);
        mpeInstrument.releaseAllNotes();
    }

    //==========================================================================
    // Render

    // render all voices and merge output
    void renderNextSubBlock(MidiBufferContext& c) {
        // if (c.buffer.isNotEmpty()) {
        //     DBG("Rendering sub-block " << c.processStartTime() << " - " << c.processEndTime());
        // }
        voices.forEachActiveVoice([&c](auto* voice) {
            voice->renderNextBlock(c);
        });
    }

    void handleMidiEvent(const MidiMessage& m) {
        if (m.isController())
            handleController(m.getChannel(), m.getControllerNumber(), m.getControllerValue());
        else if (m.isProgramChange())
            handleProgramChange(m.getChannel(), m.getProgramChangeNumber());

        mpeInstrument.processNextMidiEvent(m);
    }

    void operator()(MidiBufferContext& c) {
        int currentSample = c.start;

        if (c.isAllNotesOff()) {
            turnOffAllVoices(false);
            // but do not return, there may be messages after all notes off
        }

        // if (c.buffer.isNotEmpty()) {
        //     DBG("\n> --- " << c.processStartTime() << " - " << c.processEndTime() << " --- (" << c.duration() << " duration) ---");
        // }

        te::MidiMessageArray midiOut;
        MidiBufferContext outBlock {
            midiOut,
            c.start,
            c.length,
            c.playPosition,
            c.sampleRate
        };

        for (auto& m : c.buffer) {
            auto eventSample = c.getSampleForTimeRel(m.getTimeStamp());
            if (eventSample < currentSample) {
                continue;
            }
            if (eventSample >= c.length) {
                break;
            }
            // if (m.getTimeStamp() > time.inSeconds()) {
            if (eventSample > currentSample) {
                // render block from last time to current event
                auto subBlock = outBlock.sliced(currentSample, eventSample - currentSample);
                renderNextSubBlock(subBlock);
                currentSample = eventSample;
            }
            handleMidiEvent(m);
            // DBG("> " << m.getDescription()
            //     << " at " << m.getTimeStamp() + c.playPosition.inSeconds()
            //     << " local " << m.getTimeStamp());
        }
        // render block from last time to end of buffer
        auto subBlock = outBlock.sliced(currentSample, c.length - currentSample);
        renderNextSubBlock(subBlock);

        midiOut.sortByTimestamp();

        c.buffer.swapWith(midiOut);

        // if (c.buffer.isNotEmpty()) {
        //     DBG("\n--- " << c.processStartTime() << " - " << c.processEndTime() << " --- (" << c.duration() << " duration) --- >");
        //     c.debugMidiBuffer();
        // }
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
        ignoreUnused(changedNote);
        // DBG("Note pressure changed: " << changedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
        //     voice->currentlyPlayingNote = changedNote;
        //     voice->notePressureChanged();
        // });
    }

    /** Called when an MPE note's pitchbend changes. */
    void notePitchbendChanged(MPENote changedNote) override {
        ignoreUnused(changedNote);
        // DBG("Note pitchbend changed: " << changedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
        //     voice->currentlyPlayingNote = changedNote;
        //     voice->notePitchbendChanged();
        // });
    }

    /** Called when an MPE note's timbre changes. */
    void noteTimbreChanged(MPENote changedNote) override {
        ignoreUnused(changedNote);
        // DBG("Note timbre changed: " << changedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
        //     voice->currentlyPlayingNote = changedNote;
        //     voice->noteTimbreChanged();
        // });
    }

    /** Called when an MPE note's key state changes. */
    void noteKeyStateChanged(MPENote changedNote) override {
        ignoreUnused(changedNote);
        // DBG("Note key state changed: " << changedNote.initialNote);
        // voiceManager.forEachVoicePlayingNote(changedNote, [changedNote](Voice* voice) {
        //     voice->currentlyPlayingNote = changedNote;
        //     voice->noteKeyStateChanged();
        // });
    }

    /** Called when a MIDI controller changes. */
    void handleController(int midiChannel, int controllerNumber, int controllerValue) {
        ignoreUnused(midiChannel);
        DBG("Controller changed: " << controllerNumber << " = " << controllerValue);
        // Implement controller handling logic here
    }

    void handleProgramChange(int midiChannel, int programNumber) {
        ignoreUnused(midiChannel);
        DBG("Program change: " << programNumber);
        // Implement program change handling logic here
    }

    VoiceManager<Voice> voices;  // Pure voice management

private:
    MPEInstrument mpeInstrument;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MPEInstrumentFx)
};

}  // namespace MoTool::uZX
