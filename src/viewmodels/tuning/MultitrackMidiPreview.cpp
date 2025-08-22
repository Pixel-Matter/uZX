#include "MultitrackMidiPreview.h"
#include "../../plugins/uZX/aychip/AYPlugin.h"

namespace MoTool {

MultitrackMidiPreview::MultitrackMidiPreview(te::Edit& ed)
    : edit(ed)
    , transport(edit.getTransport())
{
    initialize();
    transport.addChangeListener(this);
}

MultitrackMidiPreview::~MultitrackMidiPreview() {
    transport.removeChangeListener(this);
    stopPlayback();
}

void MultitrackMidiPreview::initialize() {
    setupTracksAndPlugins();
    setupChannelClips();
}

void MultitrackMidiPreview::setupTracksAndPlugins() {
    // DBG("MultitrackMidiPreview::setupTracksAndPlugins");
    // Setup single track
    track = EngineHelpers::getOrInsertAudioTrackAt(edit, 0);

    // Create plugins on the track
    if (auto ayPlugin = edit.getPluginCache().createNewPlugin(uZX::AYChipPlugin::xmlTypeName, {})) {
        track->pluginList.insertPlugin(*ayPlugin, 0, nullptr);
        auto AYPlugin = dynamic_cast<uZX::AYChipPlugin*>(ayPlugin.get());
        jassert(AYPlugin != nullptr);
        // set CAB to first channel be the center
        AYPlugin->staticParams.channelsLayoutValue = uZX::ChannelsLayout::CAB;
    }

    if (auto plugin = edit.getPluginCache().createNewPlugin(uZX::NotesToPsgPlugin::xmlTypeName, {})) {
        track->pluginList.insertPlugin(*plugin, 0, nullptr);
        NotesToPsgPlugin = dynamic_cast<uZX::NotesToPsgPlugin*>(plugin.get());
    }

    // Set up MIDI device assignments
    // reassignInputs();
}

void MultitrackMidiPreview::setupChannelClips() {
    const tracktion::TimeRange clipTimeRange(0s, edit.tempoSequence.toTime({ 8, {} }));

    for (size_t i = 0; i < NUM_CHANNELS; ++i) {
        // Create overlapping clips at the same time position for parallel playback
        track->insertNewClip(tracktion::TrackItem::Type::midi,
                            "Channel " + juce::String(static_cast<int>(i + 1)),
                            clipTimeRange, nullptr);

        // Get the clip we just added (it should be the last one)
        auto trackClips = track->getClips();
        if (auto* midiClip = dynamic_cast<tracktion::MidiClip*>(trackClips[trackClips.size() - 1])) {
            channelClips[i] = midiClip;

            // Set MIDI channel for each clip (PSG uses channels 1-4)
            int midiChannel = static_cast<int>(i + 1);
            midiClip->setMidiChannel(tracktion::MidiChannel(midiChannel));
            // DBG("Setting MIDI channel " << midiChannel << " for channel " << i);
        }
    }
}


void MultitrackMidiPreview::setTuningSystem(TuningSystem* ts) {
    if (NotesToPsgPlugin != nullptr) {
        NotesToPsgPlugin->setTuningSystem(ts);
    }
}

void MultitrackMidiPreview::clearAllChannelClips() {
    for (auto& clip : channelClips) {
        if (clip) {
            clip->getSequence().clear(nullptr);
        }
    }
}

void MultitrackMidiPreview::placeNote(int channelIndex, int midiNote, double noteLength, double startTime, bool enableTone, bool enableEnvelope, int envelopeShape, int envInterval) {
    if (channelIndex < 0 || channelIndex >= static_cast<int>(NUM_CHANNELS) || !channelClips[static_cast<size_t>(channelIndex)]) return;

    auto& sequence = channelClips[static_cast<size_t>(channelIndex)]->getSequence();
    auto& envSequence = channelClips[3]->getSequence();

    const int velocity = 127;
    double currentTime = startTime;

    // Add CC messages at the same time as the note
    // Note: Tracktion Engine uses 14-bit internal CC values that are converted to 7-bit MIDI values
    if (enableTone) {
        // DBG("Adding tone switch ON for channel " << channelIndex << " at beat " << currentTime);
        sequence.addControllerEvent(tracktion::BeatPosition::fromBeats(currentTime),
                                    static_cast<int>(MidiCCType::GPB1ToneSwitch), 127 << 7, nullptr);
    } else {
        // DBG("Adding tone switch OFF for channel " << channelIndex << " at beat " << currentTime);
        sequence.addControllerEvent(tracktion::BeatPosition::fromBeats(currentTime),
                                    static_cast<int>(MidiCCType::GPB1ToneSwitch), 0, nullptr);
    }

    if (enableEnvelope) {
        // DBG("Adding envelope switch ON for channel " << channelIndex << " at beat " << currentTime);
        sequence.addControllerEvent(tracktion::BeatPosition::fromBeats(currentTime),
        static_cast<int>(MidiCCType::GPB3EnvSwitch), 127 << 7, nullptr);

        // DBG("Adding envelope switch OFF for channel " << channelIndex << " at beat " << currentTime + noteLength);
        sequence.addControllerEvent(tracktion::BeatPosition::fromBeats(currentTime + noteLength),
        static_cast<int>(MidiCCType::GPB3EnvSwitch), 0, nullptr);

        // TODO only when env retriggered or if shape changed.
        // I.e to support legato env notes we need to not add this CC
        // DBG("Adding envelope shape for channel " << 3 << " at beat " << currentTime);
        envSequence.addControllerEvent(tracktion::BeatPosition::fromBeats(currentTime),
                                        static_cast<int>(MidiCCType::SoundVariation), envelopeShape << 7, nullptr);

        // DBG("Adding envelope note " << (midiNote + envInterval) << " on channel 3 at beat " << currentTime);
        envSequence.addNote(midiNote + envInterval,
                            tracktion::BeatPosition::fromBeats(currentTime),
                            tracktion::BeatDuration::fromBeats(noteLength),
                            velocity, 0, nullptr);
    }

    // Add the main note
    // DBG("Adding note " << midiNote << " on channel " << channelIndex << " at beat " << currentTime);
    sequence.addNote(midiNote,
                    tracktion::BeatPosition::fromBeats(currentTime),
                    tracktion::BeatDuration::fromBeats(noteLength),
                    velocity, 0, nullptr);

    // // print out all notes in the channel
    // auto& seq = channelClips[static_cast<size_t>(channelIndex)]->getSequence();
    // DBG("=== Notes in channel " << channelIndex << " ===");
    // const auto& notes = seq.getNotes();
    // for (auto noteEvent : notes) {
    //     DBG("Note: " << noteEvent->getNoteNumber()
    //         << " at beat " << noteEvent->getBeatPosition().inBeats()
    //         << " duration " << noteEvent->getLengthBeats().inBeats()
    //         << " velocity " << noteEvent->getVelocity());
    // }
    // DBG("=== End of notes ===");
}

void MultitrackMidiPreview::playSingleNote(int midiNote, double noteLength, bool enableTone, bool enableEnvelope, int envelopeShape, int envInterval) {
    stopPlayback();
    clearAllChannelClips();
    // DBG("Playing single note: " << midiNote
    //     << " tone " << (enableTone ? "enabled" : "disabled")
    //     << ", envelope " << (enableEnvelope ? "enabled" : "disabled")
    //     << ", shape " << envelopeShape
    //     << ", interval " << envInterval);

    placeNote(0, midiNote, noteLength, 0.0, enableTone, enableEnvelope, envelopeShape, envInterval);

    startPlayback(noteLength);
}

void MultitrackMidiPreview::playChord(const std::vector<int>& midiNotes, double noteLength, bool enableTone, bool enableEnvelope, int envelopeShape, int envInterval) {
    stopPlayback();
    clearAllChannelClips();

    // Distribute notes across channels (max 3 notes for chord, channel 3 reserved for envelope)
    size_t noteCount = std::min(midiNotes.size(), static_cast<size_t>(NUM_CHANNELS - 1)); // Reserve channel 3 for envelope

    for (size_t i = 0; i < noteCount; ++i) {
        bool envelopeForThisNote = enableEnvelope && (i == 0); // Only first note gets envelope
        placeNote(static_cast<int>(i), midiNotes[i], noteLength, 0.0, enableTone, envelopeForThisNote, envelopeShape, envInterval);
    }

    startPlayback(noteLength);
}

void MultitrackMidiPreview::playArpeggio(const std::vector<int>& midiNotes, double noteLength, bool enableTone, bool enableEnvelope, int envelopeShape, int envInterval) {
    stopPlayback();
    clearAllChannelClips();

    double currentTime = 0.0;
    for (int midiNote : midiNotes) {
        placeNote(0, midiNote, noteLength, currentTime, enableTone, enableEnvelope, envelopeShape, envInterval);
        currentTime += noteLength;
    }
    auto duration = currentTime; // Total duration of the arpeggio

    startPlayback(duration);
}

void MultitrackMidiPreview::startPlayback(double duration) {
    auto beatLastPos = tracktion::BeatPosition::fromBeats(duration);
    auto timeDuration = edit.tempoSequence.toTime(beatLastPos);
    // DBG("Starting playback from start to " << timeDuration << "s");
    transport.playSectionAndReset(tracktion::TimeRange(tracktion::TimePosition(), timeDuration));
}

void MultitrackMidiPreview::stopPlayback() {
    transport.stop(false, false);
}

void MultitrackMidiPreview::changeListenerCallback(juce::ChangeBroadcaster* /*source*/) {
    // if (source == &transport) {
    //     // Handle transport changes if needed
    //     // DBG("Transport state changed: "
    //     //     << (transport.isPlaying() ? "Playing" : "Stopped")
    //     //     << (transport.isRecording() ? ", Recording" : "")
    //     //     << (transport.isStopping() ? ", Stopping" : "")
    //     // );
    // }
}


}