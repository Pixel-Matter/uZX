#pragma once

#include <JuceHeader.h>

#include "TuningViewModel.h"
#include "MultitrackMidiPreview.h"

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
        , midiPreview(e)
    {
        initialize();
        viewModel.addChangeListener(this);
    }
    
    ~TuningPlayer() override {
        auto& transport = midiPreview.getTransport();
        transport.removeChangeListener(this);
        viewModel.removeChangeListener(this);
    }

    void initialize();

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
    MultitrackMidiPreview midiPreview;
    juce::ListenerList<Listener> listeners_;
    std::map<int, int> playingNotes_;  // Currently playing MIDI notes on which channels

    void changeListenerCallback (ChangeBroadcaster* source) override;

    // Helper methods
    void updateTuning();
    void notifyPlayingNotes();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPlayer)
};

}