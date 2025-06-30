#pragma once

#include <JuceHeader.h>

#include "TuningViewModel.h"

#include "../../plugins/uZX/MidiToPsgPlugin.h"

#include <common/Utilities.h>  // from Tracktion

namespace MoTool {

class TuningPlayer {
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
    }

    ~TuningPlayer() = default;

    void initialize();

    void createPlugins();

    te::MidiClip::Ptr getClip();

    te::MidiClip::Ptr createMIDIClip();

    void playNote(int midiNote);

    void playChord(const std::vector<int>& midiNotes);

    void playArpeggio(const std::vector<int>& midiNotes);

    void stop();

    void addListener(Listener* listener) { listeners_.add(listener); }
    void removeListener(Listener* listener) { listeners_.remove(listener); }

    const std::set<int>& getCurrentlyPlayingNotes() const;
    
    bool isNotePlaying(int midiNote) const;

private:
    TuningViewModel& viewModel;
    tracktion::Engine& engine;
    te::Edit edit { engine, te::Edit::EditRole::forEditing };
    te::TransportControl& transport { edit.getTransport() };
    te::AudioTrack& track { *EngineHelpers::getOrInsertAudioTrackAt(edit, 0) };
    uZX::MidiToPsgPlugin::Ptr midiToPsgPlugin { nullptr };
    juce::ListenerList<Listener> listeners_;
    std::set<int> playingNotes_;  // Currently playing MIDI notes

    // Helper methods
    void updateTuning();
    void replaceNotes(const std::vector<int>& midiNotes, double noteLength = 0.5);
    void startPlayback();
    void notifyPlayingNotes();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPlayer)
};

}