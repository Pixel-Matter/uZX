/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <JuceHeader.h>
#include "EditComponent.h"

#include "../../util/base64_cx.h"
#include "../../util/Midi.h"
#include "../common/Utilities.h"
#include "../app/Commands.h"

#include "../../formats/psg/PsgFile.h"
#include "../../model/PsgClip.h"


namespace MoTool {

namespace {

inline static const auto MIDI_CLIP_DATA = MoTool::Util::b64Decode("TVRoZAAAAAYAAQACBABNVHJrAAAAKAD/WQL+AAD/WQL+AAD/WAQGAyQIAP9RAwehILAA/1gEBgMMCAD/LwBNVHJrAAAAapUwwDUAsAdkALAnKgCwCkAAsCoAAJAuZIJlgC4AAJAvZIJlgC8AAJAyWoMAgDIAAJA1boYAgDUAAJA6X4MBgDoAAJA5ZIJ4gDkAAJA4ZIJ3gDgAAJA5ZoMEgDkAAJA1aYQKgDUAhg7/LwA=");

}  // namespace


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

        // selectionManager.addChangeListener(this);

        insertMidiButton.onClick =       [this] {
            handleInsertMidiClip();
        };
        insertPSGButton.onClick =       [this] {
            handleInsertPSGClip();
        };
        // insertAudioButton.onClick =       [this] {
        //     handleInsertAudioClip();
        // };

        newTrackButton.onClick =     [this] { handleInsertNewTrack(); };

        // ===================================================================================
        // if (auto mgr = edit.engine.getUIBehaviour().getApplicationCommandManager()) {
        //     zoomInButton.setCommandToTrigger(mgr, Commands::AppCommands::viewZoomIn, true);
        //     zoomOutButton.setCommandToTrigger(mgr, Commands::AppCommands::viewZoomOut, true);
        //     zoomFitButton.setCommandToTrigger(mgr, Commands::AppCommands::viewZoomToProject, true);
        // }

        setSize(600, 400);
        ::Helpers::addAndMakeVisible(*this, { &editComponent,
                                              &newTrackButton,
                                              &insertMidiButton, &insertPSGButton,
                                            //   &deleteButton,
                                            //    &insertAudioButton,
                                            //   &zoomInButton, &zoomOutButton, &zoomFitButton
                                            });
    }

    ~AudioTimeline() override {
        selectionManager.deselectAll();
        // selectionManager.removeChangeListener(this);
    }

    //==============================================================================
    void paint(Graphics& g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override {
        auto r = getLocalBounds();
        int w = r.getWidth() / 6;
        auto topR = r.removeFromTop(30);
        insertMidiButton.setBounds(topR.removeFromLeft(w).reduced(2));
        insertPSGButton.setBounds(topR.removeFromLeft(w).reduced(2));
        // insertAudioButton.setBounds(topR.removeFromLeft(w).reduced(2));
        newTrackButton.setBounds(topR.removeFromLeft(w).reduced(2));
        // deleteButton.setBounds(topR.removeFromLeft(w).reduced(2));
        // zoomInButton.setBounds(topR.removeFromLeft(w).reduced(2));
        // zoomOutButton.setBounds(topR.removeFromLeft(w).reduced(2));
        // zoomFitButton.setBounds(topR.removeFromLeft(w).reduced(2));
        editComponent.setBounds(r);
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::Edit& edit;
    te::SelectionManager& selectionManager;
    EditComponent editComponent;

    TextButton
               newTrackButton { "New Track" },
            //    deleteButton { "Delete" },
               insertMidiButton { "Insert MIDI" },
               insertPSGButton { "Insert PSG" }
            //    insertAudioButton { "Insert Audio" },
            //    zoomInButton { "Zoom In" },
            //    zoomOutButton { "Zoom Out" },
            //    zoomFitButton { "Zoom Fit" },
               ;

    //==============================================================================

    te::AudioTrack* getSelectedOrInsertAudioTrack() {
        auto sel = selectionManager.getSelectedObject(0);
        auto track = dynamic_cast<te::AudioTrack*>(sel);
        if (track == nullptr) {
            track = EngineHelpers::getOrInsertAudioTrackAt(edit, 0);
        }
        return track;
    }

    // PsgTrack* getSelectedOrInsertPsgTrack() {
    //     auto sel = selectionManager.getSelectedObject(0);
    //     auto track = dynamic_cast<PsgTrack*>(sel);
    //     if (track == nullptr) {
    //         track = addNewPsgTrack();
    //     }
    //     return track;
    // }

    // PsgTrack* addNewPsgTrack() {
    //     edit.getTransport().stopIfRecording();
    //     if (auto newTrack = edit.insertNewTrack(te::TrackInsertPoint::getEndOfTracks(edit), IDs::PSGTRACK, &selectionManager)) {
    //         DBG("Added new PSG track");
    //         return dynamic_cast<PsgTrack*>(newTrack.get());
    //     }
    //     DBG("Failed to add new PSG track");
    //     return {};
    // }

    // void handleInsertPsgTrack() {
    //     auto track = addNewPsgTrack();
    //     if (track != nullptr) {
    //         selectionManager.select({track});
    //     }
    // }

    void handleInsertNewTrack() {
        edit.ensureNumberOfAudioTracks(getAudioTracks(edit).size() + 1);
        // select the new track
        auto tracks = getAudioTracks(edit);
        if (tracks.size() > 0) {
            selectionManager.select({tracks.getLast()});
        }
    }

    void handleInsertAudioClip() {
        Helpers::browseForAudioFile(edit.engine, [this](const File& f) {
            if (f.existsAsFile()) {
                auto track = getSelectedOrInsertAudioTrack();
                te::AudioFile audioFile(edit.engine, f);
                if (audioFile.isValid()) {
                    if (auto inserted = track->insertWaveClip(
                        f.getFileNameWithoutExtension(),
                        f,
                        {{{}, te::TimeDuration::fromSeconds(audioFile.getLength())}, {}},
                        false)
                    ) {
                        // DBG("Inserted clip: " << inserted->getName());
                    }
                }
            }
        });
    }

    void handleInsertMidiClip() {
        auto seq = Helpers::readMidi(MIDI_CLIP_DATA, 1);
        auto len = seq.getEndTime();
        double insertTime = edit.getTransport().getPosition().inSeconds();
        seq.addTimeToMessages(insertTime);
        auto time = te::TimeRange(te::TimePosition::fromSeconds(insertTime), te::TimeDuration::fromSeconds(len));
        auto track = getSelectedOrInsertAudioTrack();

        if (auto clip = track->insertMIDIClip("MidiClip", time, &selectionManager)) {
            clip->mergeInMidiSequence(seq, te::MidiList::NoteAutomationType::none);
            clip->setMidiChannel(te::MidiChannel(1));
        }
    }

    void handleInsertPSGClip() {
        Helpers::browseForPSGFile(edit.engine, [this](const File& f) {
            if (f.existsAsFile()) {
                double insertTime = edit.getTransport().getPosition().inSeconds();
                auto track = getSelectedOrInsertAudioTrack();
                auto psgFile = uZX::PsgFile(f);
                psgFile.ensureRead();
                te::ClipPosition pos = {{te::TimePosition::fromSeconds(insertTime), te::TimeDuration::fromSeconds(psgFile.getLengthSeconds())}, {}};
                if (auto inserted = PsgClip::insertTo(*track, psgFile, pos)) {
                    DBG("Inserted clip: " << inserted->getName());
                }
            }
        });
    }

    // void changeListenerCallback(ChangeBroadcaster* /*source*/) override {
    //     // if (source == &selectionManager) {
    //     //     auto sel = selectionManager.getSelectedObject(0);
    //     //     deleteButton.setEnabled(dynamic_cast<te::Clip*> (sel) != nullptr
    //     //                             || dynamic_cast<te::Track*> (sel) != nullptr
    //     //                             || dynamic_cast<te::Plugin*> (sel));
    //     // }
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
