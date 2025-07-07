#pragma once

#include <JuceHeader.h>
#include <array>

#include <common/Utilities.h>  // from Tracktion
#include "../../plugins/uZX/MidiToPsgPlugin.h"

namespace MoTool {

class MultitrackMidiPreview {
public:
    MultitrackMidiPreview(tracktion::Engine& engine);
    ~MultitrackMidiPreview();

    // Playback methods
    void playChord(const std::vector<int>& midiNotes, double noteLength = 0.5, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);
    void playSingleNote(int midiNote, double noteLength = 0.5, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);
    void playArpeggio(const std::vector<int>& midiNotes, double noteLength = 0.25, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);

    void startPlayback(double duration);
    void stopPlayback();

    void setTuningSystem(TuningSystem* ts);

    // Access to transport for state monitoring
    tracktion::TransportControl& getTransport() { return transport; }

private:
    static constexpr int NUM_CHANNELS = 4;

    // Member variables
    tracktion::Engine& engine;
    tracktion::Edit edit;
    tracktion::TransportControl& transport;
    tracktion::AudioTrack* track;
    std::array<tracktion::MidiClip::Ptr, NUM_CHANNELS> channelClips;
    uZX::MidiToPsgPlugin::Ptr midiToPsgPlugin { nullptr };

    // Private methods
    void initialize();
    void setupTracksAndPlugins();
    void setupChannelClips();
    void clearAllChannelClips();

    void placeNote(int channelIndex, int midiNote, double noteLength, double startTime = 0.0, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);

    // Test access method
    friend class MultitrackMidiPreviewTest;

    const std::array<tracktion::MidiClip::Ptr, NUM_CHANNELS>& getChannelClips() const { return channelClips; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultitrackMidiPreview)
};

}