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
        setupComponents();

        deleteButton.setEnabled(false);

        Helpers::addAndMakeVisible(*this, { &recordButton, &newTrackButton, &deleteButton,
                                            &insertButton, &showWaveformButton });
        setupButtons();
        updateRecordButtonText();
        setSize(600, 400);
    }

    ~MidiRecordingDemo() override {
        teardownComponents();
    }

    //==============================================================================
    void paint (Graphics& g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override {
        auto r = getLocalBounds();
        int w = r.getWidth() / 5;
        auto topR = r.removeFromTop (30);
        insertButton.setBounds (topR.removeFromLeft (w).reduced (2));
        recordButton.setBounds (topR.removeFromLeft (w).reduced (2));
        newTrackButton.setBounds (topR.removeFromLeft (w).reduced (2));
        deleteButton.setBounds (topR.removeFromLeft (w).reduced (2));
        showWaveformButton.setBounds (topR.removeFromLeft (w).reduced (2));

        if (editComponent != nullptr)
            editComponent->setBounds (r);
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::SelectionManager selectionManager { engine };
    te::Edit& edit;
    std::unique_ptr<EditComponent> editComponent;

    TextButton newTrackButton { "New Track" },
               deleteButton { "Delete" },
               recordButton { "Record" },
               insertButton { "Insert MIDI Clip" };
    ToggleButton showWaveformButton { "Show Waveforms" };

    //==============================================================================
    void setupComponents() {
        selectionManager.deselectAll();
        editComponent = nullptr;

        edit.getTransport().addChangeListener(this);
        selectionManager.addChangeListener(this);

        createTracksAndAssignInputs();
        te::EditFileOperations(edit).save(true, true, false);

        editComponent = std::make_unique<EditComponent>(edit, selectionManager);
        editComponent->getEditViewState().showFooters = true;
        editComponent->getEditViewState().showMidiDevices = true;
        editComponent->getEditViewState().showWaveDevices = false;
        addAndMakeVisible(*editComponent);
    }

    void teardownComponents() {
        selectionManager.deselectAll();
        edit.getTransport().removeChangeListener(this);
        selectionManager.removeChangeListener(this);
    }

    void setupButtons() {
        recordButton.onClick = [this] {
            bool wasRecording = edit.getTransport().isRecording();
            EngineHelpers::toggleRecord(edit);
            if (wasRecording)
                te::EditFileOperations(edit).save(true, true, false);
        };
        insertButton.onClick = [this] {
            auto seq = readMidi(MIDI_CLIP_DATA, 1);
            auto len = seq.getEndTime();

            auto track = EngineHelpers::getOrInsertAudioTrackAt(edit, 0);
            auto valueTree = juce::ValueTree(te::IDs::MIDICLIP);

            double insertTime = edit.getTransport().getPosition().inSeconds();
            auto time = tracktion::TimeRange(tracktion::TimePosition::fromSeconds(insertTime), tracktion::TimeDuration::fromSeconds(len));
            te::MidiClip* clip = dynamic_cast<te::MidiClip*>
                                (track->insertClipWithState (valueTree, "Clip", te::TrackItem::Type::midi,
                                                             { time, tracktion::TimeDuration::fromSeconds(0.0) }, true, false));

            seq.addTimeToMessages(insertTime);
            clip->mergeInMidiSequence(seq, te::MidiList::NoteAutomationType::none);
            clip->setMidiChannel(te::MidiChannel(1));
        };
        newTrackButton.onClick = [this] {
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

    void updateRecordButtonText() {
        recordButton.setButtonText(edit.getTransport().isRecording() ? "Abort" : "Record");
    }

    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &edit.getTransport()) {
            updateRecordButtonText();
        } else if (source == &selectionManager) {
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRecordingDemo)
};
