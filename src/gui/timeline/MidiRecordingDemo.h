/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "../../util/base64.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "tracktion_engine/tracktion_engine.h"

#include <JuceHeader.h>
#include <common/Utilities.h>
#include <common/Components.h>
#include <common/PluginWindow.h>


using namespace juce;

namespace {

inline static const auto MIDI_CLIP_DATA = Util::b64decode("TVRoZAAAAAYAAQACBABNVHJrAAAAKAD/WQL+AAD/WQL+AAD/WAQGAyQIAP9RAwehILAA/1gEBgMMCAD/LwBNVHJrAAAAapUwwDUAsAdkALAnKgCwCkAAsCoAAJAuZIJlgC4AAJAvZIJlgC8AAJAyWoMAgDIAAJA1boYAgDUAAJA6X4MBgDoAAJA5ZIJ4gDkAAJA4ZIJ3gDgAAJA5ZoMEgDkAAJA1aYQKgDUAhg7/LwA=");

MidiMessageSequence readMidi(const std::string& data, int track) {
    auto stream = juce::MemoryInputStream(&data[0], data.length(), false);
    juce::MidiFile midiFile;
    midiFile.readFrom(stream);
    MidiMessageSequence sequence = *midiFile.getTrack(track);
    for (int j = sequence.getNumEvents(); --j >= 0;) {
        auto& m = sequence.getEventPointer(j)->message;
        m.setTimeStamp(m.getTimeStamp() * 0.001);
    }
    return sequence;
}

}  // namespace

//==============================================================================
class MidiRecordingDemo  : public Component,
                           private ChangeListener
{
public:
    //==============================================================================
    MidiRecordingDemo (te::Engine& e, te::Edit& ed)
        : engine (e)
        , edit (ed)
    {
        updatePlayButtonText();
        updateRecordButtonText();

        editNameLabel.setJustificationType(Justification::centred);
        deleteButton.setEnabled(false);

        Helpers::addAndMakeVisible(*this, { &newEditButton, &playPauseButton, &showEditButton,
                                            &recordButton, &newTrackButton, &deleteButton, &editNameLabel,
                                            &insertButton, &showWaveformButton });

        setupComponents();

        // auto d = File::getSpecialLocation(File::tempDirectory).getChildFile("MoTool");
        // d.createDirectory();

        // auto f = Helpers::findRecentEdit(d);
        // if (f.existsAsFile())
        //     createOrLoadEdit(f);
        // else
        //     createOrLoadEdit(d.getNonexistentChildFile ("Unnamed", ".motool", false));

        setupButtons();
        setSize (600, 400);
    }

    // ~MidiRecordingDemo() override {
    //     te::EditFileOperations (*edit).save (true, true, false);
    //     engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
    // }

    //==============================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        int w = r.getWidth() / 8;
        auto topR = r.removeFromTop (30);
        newEditButton.setBounds (topR.removeFromLeft (w).reduced (2));
        playPauseButton.setBounds (topR.removeFromLeft (w).reduced (2));
        insertButton.setBounds (topR.removeFromLeft (w).reduced (2));
        recordButton.setBounds (topR.removeFromLeft (w).reduced (2));
        showEditButton.setBounds (topR.removeFromLeft (w).reduced (2));
        newTrackButton.setBounds (topR.removeFromLeft (w).reduced (2));
        deleteButton.setBounds (topR.removeFromLeft (w).reduced (2));
        showWaveformButton.setBounds (topR.removeFromLeft (w).reduced (2));
        topR = r.removeFromTop (30);
        editNameLabel.setBounds (topR);

        if (editComponent != nullptr)
            editComponent->setBounds (r);
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::SelectionManager selectionManager { engine };
    te::Edit& edit;
    std::unique_ptr<EditComponent> editComponent;

    TextButton newEditButton { "New" },
               playPauseButton { "Play" },
               showEditButton { "Show Edit" },
               newTrackButton { "New Track" },
               deleteButton { "Delete" },
               recordButton { "Record" },
               insertButton { "Insert MIDI Clip" };
    Label editNameLabel { "No Edit Loaded" };
    ToggleButton showWaveformButton { "Show Waveforms" };

    //==============================================================================
    void setupComponents() {
        selectionManager.deselectAll();
        editComponent = nullptr;

        edit.getTransport().addChangeListener(this);
        selectionManager.addChangeListener(this);

        editNameLabel.setText(edit.getName(), dontSendNotification);
        // editNameLabel.setText(te::EditFileOperations(edit).getEditFile().getFileNameWithoutExtension(), dontSendNotification);
        showEditButton.onClick = [this] {
            te::EditFileOperations(edit).save(true, true, false);
            te::EditFileOperations(edit).getEditFile().revealToUser();
        };

        createTracksAndAssignInputs();
        te::EditFileOperations(edit).save(true, true, false);

        editComponent = std::make_unique<EditComponent>(edit, selectionManager);
        editComponent->getEditViewState().showFooters = true;
        editComponent->getEditViewState().showMidiDevices = true;
        editComponent->getEditViewState().showWaveDevices = false;
        addAndMakeVisible(*editComponent);
    }

    void setupButtons() {
        // TODO Implement in DocumentWindow
        // newEditButton.onClick = [this] {
        //     // createOrLoadEdit();
        // };
        playPauseButton.onClick = [this] {
            EngineHelpers::togglePlay(edit);
        };
        recordButton.onClick = [this] {
            bool wasRecording = edit.getTransport().isRecording();
            EngineHelpers::toggleRecord(edit);
            if (wasRecording)
                te::EditFileOperations(edit).save(true, true, false);
        };
        insertButton.onClick = [this] {
            double insertTime = 5.0;  // TODO use current time
            auto track = EngineHelpers::getOrInsertAudioTrackAt(edit, 0);

            auto seq = readMidi(MIDI_CLIP_DATA, 1);
            auto len = seq.getEndTime();

            auto time = tracktion::TimeRange(tracktion::TimePosition::fromSeconds(insertTime), tracktion::TimeDuration::fromSeconds(len));

            auto valueTree = juce::ValueTree(te::IDs::MIDICLIP);
            te::MidiClip* clip = dynamic_cast<te::MidiClip*>
                                (track->insertClipWithState (valueTree, "Clip", te::TrackItem::Type::midi,
                                                             { time, tracktion::TimeDuration::fromSeconds(0.0) }, true, false));

            seq.addTimeToMessages(insertTime);
            clip->mergeInMidiSequence(seq, te::MidiList::NoteAutomationType::none);
            clip->setMidiChannel(te::MidiChannel(1));
        };
        newTrackButton.onClick = [this] {
            DBG("Edit size " << getAudioTracks(edit).size());
            edit.ensureNumberOfAudioTracks(getAudioTracks(edit).size() + 1);
        };
        deleteButton.onClick = [this]{
            auto sel = selectionManager.getSelectedObject (0);
            if (auto clip = dynamic_cast<te::Clip*>(sel)) {
                clip->removeFromParent();
            } else if (auto track = dynamic_cast<te::Track*>(sel)) {
                if (! (track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack()))
                    edit.deleteTrack (track);
            } else if (auto plugin = dynamic_cast<te::Plugin*>(sel)) {
                plugin->deleteFromParent();
            }
        };
        showWaveformButton.onClick = [this] {
            auto& evs = editComponent->getEditViewState();
            evs.drawWaveforms = ! evs.drawWaveforms.get();
            showWaveformButton.setToggleState (evs.drawWaveforms, dontSendNotification);
        };
    }

    void updatePlayButtonText() {
        playPauseButton.setButtonText(edit.getTransport().isPlaying() ? "Stop" : "Play");
    }

    void updateRecordButtonText() {
        recordButton.setButtonText(edit.getTransport().isRecording() ? "Abort" : "Record");
    }

    // void createOrLoadEdit (File editFile = {}) {
    //     if (editFile == File()) {
    //         FileChooser fc("New Edit", File::getSpecialLocation (File::userDocumentsDirectory), "*.tracktionedit");
    //         if (fc.browseForFileToSave(true))
    //             editFile = fc.getResult();
    //         else
    //             return;
    //     }

    //     selectionManager.deselectAll();
    //     editComponent = nullptr;

    //     if (editFile.existsAsFile())
    //         edit = te::loadEditFromFile (engine, editFile);
    //     else
    //         edit = te::createEmptyEdit (engine, editFile);

    //     edit->editFileRetriever = [editFile] { return editFile; };
    //     edit->playInStopEnabled = true;

    //     auto& transport = edit->getTransport();
    //     transport.addChangeListener (this);

    //     editNameLabel.setText (editFile.getFileNameWithoutExtension(), dontSendNotification);
    //     showEditButton.onClick = [this, editFile]
    //     {
    //         te::EditFileOperations (*edit).save (true, true, false);
    //         editFile.revealToUser();
    //     };

    //     createTracksAndAssignInputs();

    //     te::EditFileOperations (*edit).save (true, true, false);

    //     editComponent = std::make_unique<EditComponent> (*edit, selectionManager);
    //     editComponent->getEditViewState().showFooters = true;
    //     editComponent->getEditViewState().showMidiDevices = true;
    //     editComponent->getEditViewState().showWaveDevices = false;

    //     addAndMakeVisible (*editComponent);
    // }

    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &edit.getTransport()) {
            updatePlayButtonText();
            updateRecordButtonText();
        } else if (source == &selectionManager) {
            auto sel = selectionManager.getSelectedObject (0);
            deleteButton.setEnabled (dynamic_cast<te::Clip*> (sel) != nullptr
                                     || dynamic_cast<te::Track*> (sel) != nullptr
                                     || dynamic_cast<te::Plugin*> (sel));
        }
    }

    void createTracksAndAssignInputs() {
        for (auto& midiIn : engine.getDeviceManager().getMidiInDevices()) {
            midiIn->setMonitorMode (te::InputDevice::MonitorMode::automatic);
            midiIn->setEnabled (true);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRecordingDemo)
};
