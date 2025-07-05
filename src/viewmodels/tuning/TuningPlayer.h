#pragma once

#include <JuceHeader.h>

#include "TuningViewModel.h"

#include "../../plugins/uZX/MidiToPsgPlugin.h"
#include "../../models/PsgMidi.h"

#include <common/Utilities.h>  // from Tracktion

namespace MoTool {

class TuningPlayer : private ChangeListener
{
public:
    // TODO maybe actually use converter from plugin for that?
    // because then we can monitor notes from external MIDI devices
    // OR introduce MidiMonitorPlugin that will just log all MIDI messages to a thread-safe buffer (just like LevelMeterPlugin)
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void playingNotesChanges() = 0;  // to repaint notes on grid
    };

    TuningPlayer(TuningViewModel& tvm, tracktion::Engine& e)
        : viewModel(tvm)
        , engine(e)
    {
        initialize();
        viewModel.addChangeListener(this);
    }

    void initialize();

    void createPlugins();

    te::MidiClip::Ptr getClip();

    te::MidiClip::Ptr createMIDIClip();

    void playSingleNote(int midiNote);

    void playDegreeChord(int midiNote);

    void playScale(int octave, bool chromatic = false);

    void playChord(const std::vector<int>& midiNotes);

    void playArpeggio(const std::vector<int>& midiNotes);

    void stopNotes(bool notify = true);

    void addListener(Listener* listener) { listeners_.add(listener); }
    void removeListener(Listener* listener) { listeners_.remove(listener); }

    bool isNotePlaying(int midiNote) const;

private:
    TuningViewModel& viewModel;
    tracktion::Engine& engine;
    te::Edit edit { engine, te::Edit::EditRole::forEditing };
    te::TransportControl& transport { edit.getTransport() };
    te::AudioTrack& track { *EngineHelpers::getOrInsertAudioTrackAt(edit, 0) };
    uZX::MidiToPsgPlugin::Ptr midiToPsgPlugin { nullptr };
    juce::ListenerList<Listener> listeners_;
    std::map<int, int> playingNotes_;  // Currently playing MIDI notes on which channels

    void changeListenerCallback (ChangeBroadcaster* source) override;

    // Helper methods
    int getMonophonicChannel() const;
    void noteOn (int midiNote, int channel, bool isTone = false, bool isEnvelope = false);
    void noteOnNoRetrigger(int midiNote, int channel, bool isTone, bool isEnvelope = false);
    void noteOff(int midiNote, int channel, bool isTone = false, bool isEnvelope = false);
    void sendCC(int channel, MidiCCType ccType, int value);
    void sendNoteOn(int channel, int midiNote, int velocity = 127);
    void sendNoteOff(int channel, int midiNote);
    void updateTuning();
    void replaceNotes(const std::vector<int>& midiNotes, double noteLength = 0.5);
    void startPlayback();
    void notifyPlayingNotes();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPlayer)
};

}