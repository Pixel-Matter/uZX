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

#include "../../util/base64_cx.h"
#include "../../util/Midi.h"
#include "../../plugins/uZX/aychip/AYPlugin.h"
#include "EditComponent.h"

namespace MoTool {

namespace {

inline static const auto MIDI_CLIP_DATA = MoTool::Util::b64Decode("TVRoZAAAAAYAAQACBABNVHJrAAAAKAD/WQL+AAD/WQL+AAD/WAQGAyQIAP9RAwehILAA/1gEBgMMCAD/LwBNVHJrAAAAapUwwDUAsAdkALAnKgCwCkAAsCoAAJAuZIJlgC4AAJAvZIJlgC8AAJAyWoMAgDIAAJA1boYAgDUAAJA6X4MBgDoAAJA5ZIJ4gDkAAJA4ZIJ3gDgAAJA5ZoMEgDkAAJA1aYQKgDUAhg7/LwA=");

}  // namespace


//==============================================================================
/** TODO
    - Move buttons to toolbar component
    - Make zoom struct
    - Make grid positions struct with several types of subdivisions: regions, bars, beats, subs, frames
    - Make ruler component
    - Make grid component
*/

//==============================================================================
class MidiTimeline  : public Component,
                      private ChangeListener
{
public:
    //==============================================================================
    MidiTimeline (te::Edit& ed)
        : engine {ed.engine}
        , edit {ed}
        , editComponent {edit, selectionManager}
    {
        auto& evs = editComponent.getEditViewState();
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

        selectionManager.addChangeListener(this);

        insertMidiButton.onClick =       [this] {
            handleInsertMidiClip();
        };
        insertAudioButton.onClick =       [this] {
            handleInsertAudioClip();
        };
        newTrackButton.onClick =     [this] { edit.ensureNumberOfAudioTracks(getAudioTracks(edit).size() + 1); };
        deleteButton.onClick =       [this] { handleDelete(); };

        deleteButton.setEnabled(false);

        setSize(600, 400);
        Helpers::addAndMakeVisible(*this, { &editComponent,
                                            &newTrackButton, &deleteButton,
                                            &insertMidiButton, &insertAudioButton
                                          });
    }

    ~MidiTimeline() override {
        selectionManager.deselectAll();
        selectionManager.removeChangeListener(this);
    }

    //==============================================================================
    void paint(Graphics& g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override {
        auto r = getLocalBounds();
        int w = r.getWidth() / 8;
        auto topR = r.removeFromTop(30);
        insertMidiButton.setBounds(topR.removeFromLeft (w).reduced (2));
        insertAudioButton.setBounds(topR.removeFromLeft (w).reduced (2));
        newTrackButton.setBounds(topR.removeFromLeft (w).reduced (2));
        deleteButton.setBounds(topR.removeFromLeft (w).reduced (2));
        editComponent.setBounds(r);
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::SelectionManager selectionManager { engine };
    te::Edit& edit;
    EditComponent editComponent;

    TextButton newTrackButton { "New Track" },
               deleteButton { "Delete" },
               insertMidiButton { "Insert MIDI" },
               insertAudioButton { "Insert Audio" }
               ;
    // ToggleButton showWaveformButton { "Show Waveforms" };

    //==============================================================================

    void handleInsertAudioClip() {
        using namespace te;
        EngineHelpers::browseForAudioFile(edit.engine, [this](const File& f) {
            if (f.existsAsFile()) {
                auto sel = selectionManager.getSelectedObject(0);
                auto track = dynamic_cast<te::AudioTrack*>(sel);
                if (track == nullptr) {
                    track = EngineHelpers::getOrInsertAudioTrackAt(edit, 0);
                }
                te::AudioFile audioFile (edit.engine, f);
                if (audioFile.isValid()) {
                    if (auto inserted = track->insertWaveClip({}, f, {{{}, te::TimeDuration::fromSeconds(audioFile.getLength())}, {}}, false)) {
                        DBG("Inserted clip: " << inserted->getName());
                    }
                }
            }
        });
    }

    void handleInsertMidiClip() {
        auto seq = Util::readMidi(MIDI_CLIP_DATA, 1);
        auto len = seq.getEndTime();

        auto sel = selectionManager.getSelectedObject(0);
        auto track = dynamic_cast<te::AudioTrack*>(sel);
        if (track == nullptr) {
            track = EngineHelpers::getOrInsertAudioTrackAt(edit, 0);
        }
        auto valueTree = juce::ValueTree(te::IDs::MIDICLIP);

        double insertTime = edit.getTransport().getPosition().inSeconds();
        auto time = tracktion::TimeRange(tracktion::TimePosition::fromSeconds(insertTime),
                                            tracktion::TimeDuration::fromSeconds(len));
        te::MidiClip* clip = dynamic_cast<te::MidiClip*>
                            (track->insertClipWithState(valueTree, "Clip", te::TrackItem::Type::midi,
                                                        { time, tracktion::TimeDuration::fromSeconds(0.0) }, true, false));

        seq.addTimeToMessages(insertTime);
        clip->mergeInMidiSequence(seq, te::MidiList::NoteAutomationType::none);
        clip->setMidiChannel(te::MidiChannel(1));
    }

    void handleDelete() {
        auto sel = selectionManager.getSelectedObject (0);
        if (auto clip = dynamic_cast<te::Clip*>(sel)) {
            clip->removeFromParent();
        } else if (auto track = dynamic_cast<te::Track*>(sel)) {
            if (! (track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack()))
                edit.deleteTrack(track);
        } else if (auto plugin = dynamic_cast<te::Plugin*>(sel)) {
            plugin->deleteFromParent();
        }
    }

    // void handleShowWaveform() {
    //     auto& evs = editComponent.getEditViewState();
    //     evs.drawWaveforms = ! evs.drawWaveforms.get();
    //     showWaveformButton.setToggleState(evs.drawWaveforms, dontSendNotification);
    // }

    void changeListenerCallback(ChangeBroadcaster* source) override {
        if (source == &selectionManager) {
            auto sel = selectionManager.getSelectedObject(0);
            deleteButton.setEnabled(dynamic_cast<te::Clip*> (sel) != nullptr
                                    || dynamic_cast<te::Track*> (sel) != nullptr
                                    || dynamic_cast<te::Plugin*> (sel));
        }
    }

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiTimeline)
};

}  // namespace MoTool
