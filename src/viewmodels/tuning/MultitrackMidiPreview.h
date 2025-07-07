#pragma once

#include <JuceHeader.h>
#include <array>

#include <common/Utilities.h>  // from Tracktion
#include "../../models/PsgMidi.h"

namespace MoTool {

class MultitrackMidiPreview {
public:
    MultitrackMidiPreview(tracktion::Engine& engine);
    ~MultitrackMidiPreview();
    
    void initialize();
    void setupTracks();
    void setupClips();
    
    // Playback methods
    void playChord(const std::vector<int>& midiNotes, double noteLength = 0.5, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);
    void playSingleNote(int midiNote, double noteLength = 0.5, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);
    void playArpeggio(const std::vector<int>& midiNotes, double noteLength = 0.25, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);
    
    void startPlayback();
    void stopPlayback();
    
    // Access to the edit for plugin management
    tracktion::Edit& getEdit() { return edit; }
    
    static constexpr int NUM_TRACKS = 4;
    
    // Test access
    const std::array<tracktion::MidiClip::Ptr, NUM_TRACKS>& getClips() const { return clips; }
    void replaceNotesOnTrack(int trackIndex, const std::vector<int>& midiNotes, double noteLength, double startTime = 0.0, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);
    
private:
    tracktion::Engine& engine;
    tracktion::Edit edit;
    tracktion::TransportControl& transport;
    std::array<tracktion::AudioTrack*, NUM_TRACKS> tracks;
    std::array<tracktion::MidiClip::Ptr, NUM_TRACKS> clips;
    
    void clearAllClips();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultitrackMidiPreview)
};

}