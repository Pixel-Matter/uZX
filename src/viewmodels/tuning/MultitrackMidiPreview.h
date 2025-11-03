#pragma once

#include <JuceHeader.h>
#include <array>

#include <common/Utilities.h>  // from Tracktion
#include "../../plugins/uZX/notes_to_psg/NotesToPsgPlugin.h"

namespace MoTool {

class MultitrackMidiPreview : private juce::ChangeListener {
public:
    MultitrackMidiPreview(te::Edit& ed);
    ~MultitrackMidiPreview() override;

    // Playback methods
    void playChord(const std::vector<int>& midiNotes, double noteLength = 0.5, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);
    void playSingleNote(int midiNote, double noteLength = 0.5, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);
    void playArpeggio(const std::vector<int>& midiNotes, double noteLength = 0.25, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);

    void startPlayback(double duration);
    void stopPlayback();

    void setTuningSystem(std::shared_ptr<TuningSystem> ts);

    // Access to transport for state monitoring
    tracktion::TransportControl& getTransport() { return transport; }

    // // MIDI device management
    // void reassignInputs();

private:
    static constexpr int NUM_CHANNELS = 4;
    static constexpr int SINGLE_NOTE_CHANNEL = 1;
    static constexpr bool USE_MIDI_LOGGER = false;

    // Member variables
    tracktion::Edit& edit;
    tracktion::TransportControl& transport;
    tracktion::AudioTrack* track;
    std::array<tracktion::MidiClip::Ptr, NUM_CHANNELS> channelClips;
    uZX::NotesToPsgPlugin::Ptr NotesToPsgPlugin { nullptr };

    // ChangeListener overrides
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Private methods
    void initialize();
    void setupTracksAndPlugins();
    void setupChannelClips();
    void clearAllChannelClips();

    void placeNote(int channelIndex, int midiNote, double noteLength, double startTime = 0.0, bool enableTone = true, bool enableEnvelope = false, int envelopeShape = 0, int modulationSemitones = 0);

    // Test access method
    friend class MultitrackMidiPreviewTest;

    const std::array<tracktion::MidiClip::Ptr, NUM_CHANNELS>& getChannelClips() const { return channelClips; }

    JUCE_DECLARE_WEAK_REFERENCEABLE(MultitrackMidiPreview)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultitrackMidiPreview)
};

}
