#pragma once

#include <JuceHeader.h>

#include "TuningViewModel.h"

#include <common/Utilities.h>  // from Tracktion

namespace MoTool {

class TuningPlayer {
public:
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

private:
    TuningViewModel& viewModel;
    tracktion::Engine& engine;
    te::Edit edit { engine, te::Edit::EditRole::forEditing };
    te::TransportControl& transport { edit.getTransport() };
    te::AudioTrack& track { *EngineHelpers::getOrInsertAudioTrackAt(edit, 0) };

    // Helper methods
    void replaceNotes(const std::vector<int>& midiNotes, double noteLength = 0.5);
    void startPlayback();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPlayer)
};

}