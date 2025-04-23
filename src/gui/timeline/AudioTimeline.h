#pragma once

#include <JuceHeader.h>
#include "EditComponent.h"

#include "../../util/base64_cx.h"
// #include "../../util/Midi.h"
// #include "../common/Utilities.h"

// #include "../../formats/psg/PsgFile.h"
// #include "../../model/EditUtilities.h"


namespace MoTool {


//==============================================================================
/** TODO
    - Move buttons to toolbar component
    - Make grid positions struct with several types of subdivisions: regions/arranger clips, bars, beats, subs, frames
    - Make grid component
*/

//==============================================================================
class AudioTimeline : public Component
                    //   private ChangeListener
                                                {
public:
    //==============================================================================
    AudioTimeline (te::Edit& ed, EditViewState& evs, te::SelectionManager& selMgr)
        : engine {ed.engine}
        , edit {ed}
        , selectionManager {selMgr}
        , editComponent {edit, evs}
    {
        evs.showFooters = true;
        evs.showMidiDevices = true;
        evs.showWaveDevices = false;
        evs.showMarkerTrack = false;
        evs.showChordTrack = false;
        evs.showGlobalTrack = false;
        evs.showMasterTrack = false;
        evs.drawWaveforms = true;

        createTracksAndAssignInputs();
        te::EditFileOperations(edit).save(true, true, false);

        setSize(600, 400);
        ::Helpers::addAndMakeVisible(*this, { &editComponent,
                                            });
    }

    ~AudioTimeline() override {
        selectionManager.deselectAll();
    }

    //==============================================================================
    void paint(Graphics& g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override {
        auto r = getLocalBounds();
        editComponent.setBounds(r);

        // TODO consider fast declarative Layout engine
        // this is Builder template actually
        // using lo = Layout;
        // lo::Vertical layout_ {
        //     32_px << topBar,
        //     display.getHeightFr() << display,
        //     32_px << transportBar,
        //     32_px << lo::Horizonal {
        //         1_fr << lo::Horizonal {
        //             lo::Options {.margin = 2_px},
        //             1_fr << insertMidiButton,
        //             1_fr << insertPSGButton,
        //             1_fr << insertAudioButton,
        //             1_fr << deleteButton,
        //         },
        //         1_fr << lo::Empty,
        //     },
        //     1_fr << editComponent,
        //     32_px << footer
        // };
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::Edit& edit;
    te::SelectionManager& selectionManager;
    EditComponent editComponent;

    //==============================================================================

    // void handleInsertAudioClip() {
    //     Helpers::browseForAudioFile(edit.engine, [this](const File& f) {
    //         if (f.existsAsFile()) {
    //             auto track = getSelectedOrInsertAudioTrack(edit, selectionManager);
    //             te::AudioFile audioFile(edit.engine, f);
    //             if (audioFile.isValid()) {
    //                 if (auto inserted = track->insertWaveClip(
    //                     f.getFileNameWithoutExtension(),
    //                     f,
    //                     {{{}, te::TimeDuration::fromSeconds(audioFile.getLength())}, {}},
    //                     false)
    //                 ) {
    //                     // DBG("Inserted clip: " << inserted->getName());
    //                 }
    //             }
    //         }
    //     });
    // }

    // void handleInsertMidiClip() {
    //     auto seq = Helpers::readMidi(MIDI_CLIP_DATA, 1);
    //     auto len = seq.getEndTime();
    //     double insertTime = edit.getTransport().getPosition().inSeconds();
    //     seq.addTimeToMessages(insertTime);
    //     auto time = te::TimeRange(te::TimePosition::fromSeconds(insertTime), te::TimeDuration::fromSeconds(len));
    //     auto track = getSelectedOrInsertAudioTrack(edit, selectionManager);

    //     if (auto clip = track->insertMIDIClip("MidiClip", time, &selectionManager)) {
    //         clip->mergeInMidiSequence(seq, te::MidiList::NoteAutomationType::none);
    //         clip->setMidiChannel(te::MidiChannel(1));
    //     }
    // }

    void createTracksAndAssignInputs() {
        for (auto& midiIn : engine.getDeviceManager().getMidiInDevices()) {
            midiIn->setMonitorMode(te::InputDevice::MonitorMode::automatic);
            midiIn->setEnabled(true);
        }

        edit.getTransport().ensureContextAllocated();
        if (te::getAudioTracks(edit).size() == 0) {
            int trackNum = 0;

            for (auto instance : edit.getAllInputDevices()) {
                if (instance->getInputDevice().getDeviceType() == te::InputDevice::physicalMidiDevice) {
                    if (auto t = EngineHelpers::getOrInsertAudioTrackAt (edit, trackNum)) {
                        [[ maybe_unused ]] auto res = instance->setTarget (t->itemID, true, &edit.getUndoManager(), 0);
                        instance->setRecordingEnabled (t->itemID, true);
                        trackNum++;
                    }
                }
            }
        }
        edit.restartPlayback();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTimeline)
};

}  // namespace MoTool
