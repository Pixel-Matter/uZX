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
#include <common/Utilities.h>
#include <common/Components.h>
#include <common/PluginWindow.h>

#include "../../util/base64.h"

using namespace juce;


//==============================================================================
class MidiPlaybackDemo  : public Component,
                          private ChangeListener
{
public:
    //==============================================================================
    MidiPlaybackDemo (te::Engine& e)
        : engine (e)
    {
        newEditButton.onClick = [this] { createOrLoadEdit(); };

        updatePlayButtonText();

        editNameLabel.setJustificationType (Justification::centred);

        Helpers::addAndMakeVisible (*this, { &newEditButton, &playPauseButton, &showEditButton,
                                             &recordButton, &newTrackButton, &deleteButton, &editNameLabel });

        deleteButton.setEnabled (false);

        auto d = File::getSpecialLocation (File::tempDirectory).getChildFile ("MidiPlaybackDemo");
        d.createDirectory();

        auto f = Helpers::findRecentEdit (d);
        if (f.existsAsFile())
            createOrLoadEdit (f);
        else
            createOrLoadEdit (d.getNonexistentChildFile ("Test", ".tracktionedit", false));

        selectionManager.addChangeListener (this);

        setupButtons();

        setSize (600, 400);
    }

    ~MidiPlaybackDemo() override
    {
        te::EditFileOperations (*edit).save (true, true, false);
        engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        int w = r.getWidth() / 6;
        auto topR = r.removeFromTop (30);
        newEditButton.setBounds (topR.removeFromLeft (w).reduced (2));
        playPauseButton.setBounds (topR.removeFromLeft (w).reduced (2));
        recordButton.setBounds (topR.removeFromLeft (w).reduced (2));
        showEditButton.setBounds (topR.removeFromLeft (w).reduced (2));
        newTrackButton.setBounds (topR.removeFromLeft (w).reduced (2));
        deleteButton.setBounds (topR.removeFromLeft (w).reduced (2));
        topR = r.removeFromTop (30);
        editNameLabel.setBounds (topR);

        if (editComponent != nullptr)
            editComponent->setBounds (r);
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::SelectionManager selectionManager { engine };
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<EditComponent> editComponent;

    TextButton newEditButton { "New" }, playPauseButton { "Play" },
               showEditButton { "Show Edit" }, newTrackButton { "Move to first note" }, deleteButton { "Delete" }, recordButton { "Create MIDI Clip" };
    Label editNameLabel { "No Edit Loaded" };
    ToggleButton showWaveformButton { "Show Waveforms" };

    //==============================================================================
    void setupButtons()
    {
        playPauseButton.onClick = [this]
        {
            EngineHelpers::togglePlay(*edit);
        };
        recordButton.onClick = [this]
        {
            auto track = EngineHelpers::getOrInsertAudioTrackAt (*edit, 0);

            auto data = Util::b64decode("TVRoZAAAAAYAAQACBABNVHJrAAAAKAD/WQL+AAD/WQL+AAD/WAQGAyQIAP9RAwehILAA/1gEBgMMCAD/LwBNVHJrAAAAapUwwDUAsAdkALAnKgCwCkAAsCoAAJAuZIJlgC4AAJAvZIJlgC8AAJAyWoMAgDIAAJA1boYAgDUAAJA6X4MBgDoAAJA5ZIJ4gDkAAJA4ZIJ3gDgAAJA5ZoMEgDkAAJA1aYQKgDUAhg7/LwA=");
            auto stream = juce::MemoryInputStream(&data[0], data.length(), false);
            auto midiFile = juce::MidiFile();
            midiFile.readFrom(stream);
            const MidiMessageSequence* sequence = midiFile.getTrack(1); // Assume notes on track 1
            for (int j = sequence->getNumEvents(); --j >= 0;)
            {
                auto& m = sequence->getEventPointer(j)->message;
                m.setTimeStamp (m.getTimeStamp() * 0.001);
            }

            auto len = sequence->getEndTime();

            auto time = tracktion::TimeRange(tracktion::TimePosition::fromSeconds(5.0), tracktion::TimeDuration::fromSeconds(len));
            auto valueTree = juce::ValueTree(te::IDs::MIDICLIP);
            te::MidiClip* clip = dynamic_cast<te::MidiClip*>
            (track->insertClipWithState (valueTree, "Clip", te::TrackItem::Type::midi,
                                      { time, tracktion::TimeDuration::fromSeconds(0.0) }, true, false));
            MidiMessageSequence seq = *sequence;

            seq.addTimeToMessages(5.0);

            clip->mergeInMidiSequence(seq, te::MidiList::NoteAutomationType::none);
            DBG(clip->state.toXmlString ({}));

            clip->setMidiChannel(te::MidiChannel(1));

        };
        newTrackButton.onClick = [this]
        {
            // edit->ensureNumberOfAudioTracks (getAudioTracks (*edit).size() + 1);

            auto& transport = edit->getTransport();
            transport.position = tracktion::TimePosition::fromSeconds(7.7359);
            // transport.play(false);
        };
        deleteButton.onClick = [this]
        {
            auto sel = selectionManager.getSelectedObject (0);
            if (auto clip = dynamic_cast<te::Clip*> (sel)) {
                clip->removeFromParent();
            }
            else if (auto track = dynamic_cast<te::Track*> (sel))
            {
                if (! (track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack()))
                    edit->deleteTrack (track);
            }
            else if (auto plugin = dynamic_cast<te::Plugin*> (sel))
            {
                plugin->deleteFromParent();
            }
        };
        showWaveformButton.onClick = [this]
        {
            auto& evs = editComponent->getEditViewState();
            evs.drawWaveforms = ! evs.drawWaveforms.get();
            showWaveformButton.setToggleState (evs.drawWaveforms, dontSendNotification);
        };
    }

    void updatePlayButtonText()
    {
        if (edit != nullptr)
            playPauseButton.setButtonText (edit->getTransport().isPlaying() ? "Stop" : "Play");
    }

    void createOrLoadEdit (File editFile = {})
    {
        if (editFile == File())
        {
            FileChooser fc ("New Edit", File::getSpecialLocation (File::userDocumentsDirectory), "*.tracktionedit");
            if (fc.browseForFileToSave (true))
                editFile = fc.getResult();
            else
                return;
        }

        selectionManager.deselectAll();
        editComponent = nullptr;

        if (editFile.existsAsFile())
            edit = te::loadEditFromFile (engine, editFile);
        else
            edit = te::createEmptyEdit (engine, editFile);

        edit->editFileRetriever = [editFile] { return editFile; };
        edit->playInStopEnabled = true;

        auto& transport = edit->getTransport();
        transport.addChangeListener (this);

        editNameLabel.setText (editFile.getFileNameWithoutExtension(), dontSendNotification);
        showEditButton.onClick = [this, editFile]
        {
            te::EditFileOperations (*edit).save (true, true, false);
            editFile.revealToUser();
        };

        createTracksAndAssignInputs();

        te::EditFileOperations (*edit).save (true, true, false);

        editComponent = std::make_unique<EditComponent> (*edit, selectionManager);
        editComponent->getEditViewState().showFooters = true;
        editComponent->getEditViewState().showMidiDevices = true;
        editComponent->getEditViewState().showWaveDevices = false;

        addAndMakeVisible (*editComponent);
    }

    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (edit != nullptr && source == &edit->getTransport())
        {
            updatePlayButtonText();
        }
        else if (source == &selectionManager)
        {
            auto sel = selectionManager.getSelectedObject (0);
            deleteButton.setEnabled (dynamic_cast<te::Clip*> (sel) != nullptr
                                     || dynamic_cast<te::Track*> (sel) != nullptr
                                     || dynamic_cast<te::Plugin*> (sel));
        }
    }

    void createTracksAndAssignInputs() {
        for (auto& midiIn : engine.getDeviceManager().getMidiInDevices())
        {
            midiIn->setMonitorMode (te::InputDevice::MonitorMode::automatic);
            midiIn->setEnabled (true);
        }

        edit->getTransport().ensureContextAllocated();

        if (te::getAudioTracks (*edit).size() == 0)
        {
            int trackNum = 0;

            for (auto instance : edit->getAllInputDevices())
            {
                if (instance->getInputDevice().getDeviceType() == te::InputDevice::physicalMidiDevice)
                {
                    if (auto t = EngineHelpers::getOrInsertAudioTrackAt (*edit, trackNum))
                    {
                        [[ maybe_unused ]] auto res = instance->setTarget (t->itemID, true, &edit->getUndoManager(), 0);
                        instance->setRecordingEnabled (t->itemID, true);

                        trackNum++;
                    }
                }
            }
        }

        edit->restartPlayback();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiPlaybackDemo)
};
