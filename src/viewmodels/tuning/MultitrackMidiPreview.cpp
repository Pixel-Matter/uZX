#include "MultitrackMidiPreview.h"
#include "tracktion_engine/tracktion_engine.h"
#include "../../plugins/uZX/aychip/AYPlugin.h"

namespace MoTool {

MultitrackMidiPreview::MultitrackMidiPreview(tracktion::Engine& e)
    : engine(e)
    , edit(engine, tracktion::Edit::EditRole::forEditing)
    , transport(edit.getTransport())
{
    initialize();
}

MultitrackMidiPreview::~MultitrackMidiPreview() {
    stopPlayback();
}

void MultitrackMidiPreview::initialize() {
    edit.playInStopEnabled = false;
    setupTracksAndPlugins();
    setupChannelClips();
}

void MultitrackMidiPreview::setupTracksAndPlugins() {
    // Setup single track
    track = EngineHelpers::getOrInsertAudioTrackAt(edit, 0);

    // Create plugins on the track
    if (auto ayPlugin = edit.getPluginCache().createNewPlugin(uZX::AYChipPlugin::xmlTypeName, {})) {
        track->pluginList.insertPlugin(*ayPlugin, 0, nullptr);
    }

    if (auto plugin = edit.getPluginCache().createNewPlugin(uZX::MidiToPsgPlugin::xmlTypeName, {})) {
        track->pluginList.insertPlugin(*plugin, 0, nullptr);
        midiToPsgPlugin = dynamic_cast<uZX::MidiToPsgPlugin*>(plugin.get());
    }
}

void MultitrackMidiPreview::setupChannelClips() {
    const tracktion::TimeRange clipTimeRange(0s, edit.tempoSequence.toTime({ 8, {} }));
    
    for (size_t i = 0; i < NUM_CHANNELS; ++i) {
        // Create overlapping clips at the same time position for parallel playback
        track->insertNewClip(tracktion::TrackItem::Type::midi,
                            "Channel " + juce::String(static_cast<int>(i + 1)),
                            clipTimeRange,
                            nullptr);

        // Get the clip we just added (it should be the last one)
        auto trackClips = track->getClips();
        if (auto* midiClip = dynamic_cast<tracktion::MidiClip*>(trackClips[trackClips.size() - 1])) {
            channelClips[i] = midiClip;

            // Set MIDI channel for each clip (PSG uses channels 1-4)
            int midiChannel = static_cast<int>(i + 1);
            midiClip->setMidiChannel(tracktion::MidiChannel(midiChannel));
            DBG("Setting MIDI channel " << midiChannel << " for channel " << i);
        }
    }
}


void MultitrackMidiPreview::setTuningSystem(TuningSystem* ts) {
    if (midiToPsgPlugin != nullptr) {
        midiToPsgPlugin->setTuningSystem(ts);
    }
}

void MultitrackMidiPreview::clearAllChannelClips() {
    for (auto& clip : channelClips) {
        if (clip) {
            clip->getSequence().clear(nullptr);
        }
    }
}

void MultitrackMidiPreview::replaceNotesOnChannel(int channelIndex, const std::vector<int>& midiNotes, double noteLength, double startTime, bool enableTone, bool enableEnvelope, int envelopeShape, int envInterval) {
    if (channelIndex < 0 || channelIndex >= static_cast<int>(NUM_CHANNELS) || !channelClips[static_cast<size_t>(channelIndex)]) return;

    auto& sequence = channelClips[static_cast<size_t>(channelIndex)]->getSequence();
    sequence.clear(nullptr);

    const int velocity = 127;
    double currentTime = startTime;

    for (int midiNote : midiNotes) {
        // Add CC messages at the same time as the note
        // Note: Tracktion Engine uses 14-bit internal CC values that are converted to 7-bit MIDI values
        if (enableTone) {
            DBG("Adding tone switch ON for channel " << channelIndex << " at beat " << currentTime);
            sequence.addControllerEvent(tracktion::BeatPosition::fromBeats(currentTime),
                                        static_cast<int>(MidiCCType::GPB1ToneSwitch), 127 << 7, nullptr);
        }

        if (enableEnvelope) {
            DBG("Adding envelope switch ON for channel " << channelIndex << " at beat " << currentTime);
            sequence.addControllerEvent(tracktion::BeatPosition::fromBeats(currentTime),
                                        static_cast<int>(MidiCCType::GPB3EnvSwitch), 127 << 7, nullptr);

            // Add envelope shape on envelope channel (channel 4)
            if (channelIndex == 3) { // Envelope channel
                sequence.addControllerEvent(tracktion::BeatPosition::fromBeats(currentTime),
                                            static_cast<int>(MidiCCType::SoundVariation), envelopeShape << 7, nullptr);
            }
        }

        // Add the main note
        DBG("Adding note " << midiNote << " on channel " << channelIndex << " at beat " << currentTime);
        sequence.addNote(midiNote,
                        tracktion::BeatPosition::fromBeats(currentTime),
                        tracktion::BeatDuration::fromBeats(noteLength),
                        velocity, 0, nullptr);

        // If envelope is enabled, add envelope note on a separate channel
        if (enableEnvelope && channelIndex < 3) {
            // Find envelope channel (channel 3) and add envelope note
            if (channelClips[3]) {
                channelClips[3]->getSequence().addNote(midiNote + envInterval,
                                               tracktion::BeatPosition::fromBeats(currentTime),
                                               tracktion::BeatDuration::fromBeats(noteLength),
                                               velocity, 0, nullptr);
            }
        }

        // For arpeggio, increment time for each note
        if (midiNotes.size() > 1 && noteLength < 0.5) {
            currentTime += noteLength;
        }
    }

    // print out all notes in the channel
    auto& seq = channelClips[static_cast<size_t>(channelIndex)]->getSequence();
    DBG("=== Notes in channel " << channelIndex << " ===");
    const auto& notes = seq.getNotes();
    for (auto noteEvent : notes) {
        DBG("Note: " << noteEvent->getNoteNumber()
            << " at beat " << noteEvent->getBeatPosition().inBeats()
            << " duration " << noteEvent->getLengthBeats().inBeats()
            << " velocity " << noteEvent->getVelocity());
    }
    DBG("=== End of notes ===");
}

void MultitrackMidiPreview::playChord(const std::vector<int>& midiNotes, double noteLength, bool enableTone, bool enableEnvelope, int envelopeShape, int envInterval) {
    stopPlayback();
    clearAllChannelClips();

    // Distribute notes across tracks (max 3 notes for chord, track 3 reserved for envelope)
    size_t noteCount = std::min(midiNotes.size(), static_cast<size_t>(3));

    for (size_t i = 0; i < noteCount; ++i) {
        std::vector<int> singleNote = {midiNotes[i]};
        bool envelopeForThisNote = enableEnvelope && (i == 0); // Only first note gets envelope
        replaceNotesOnChannel(static_cast<int>(i), singleNote, noteLength, 0.0, enableTone, envelopeForThisNote, envelopeShape, envInterval);
    }

    startPlayback();
}

void MultitrackMidiPreview::playSingleNote(int midiNote, double noteLength, bool enableTone, bool enableEnvelope, int envelopeShape, int modulationSemitones) {
    stopPlayback();
    clearAllChannelClips();
    DBG("Playing single note: " << midiNote);

    replaceNotesOnChannel(0, {midiNote}, noteLength, 0.0, enableTone, enableEnvelope, envelopeShape, modulationSemitones);

    startPlayback();
}

void MultitrackMidiPreview::playArpeggio(const std::vector<int>& midiNotes, double noteLength, bool enableTone, bool enableEnvelope, int envelopeShape, int envInterval) {
    stopPlayback();
    clearAllChannelClips();

    // Play arpeggio on first channel
    replaceNotesOnChannel(0, midiNotes, noteLength, 0.0, enableTone, enableEnvelope, envelopeShape, envInterval);

    startPlayback();
}

void MultitrackMidiPreview::startPlayback() {
    // transport.setPosition(tracktion::TimePosition::fromSeconds(0.0));
    DBG("Starting playback from start");
    transport.playFromStart(false);
}

void MultitrackMidiPreview::stopPlayback() {
    transport.stop(false, false);
    // send all notes off
    // FIXME problem with this is that this message can be injected after first note of next playback
    // for (size_t i = 0; i < NUM_TRACKS; ++i) {
    //     auto* track = tracks[i];
    //     if (!track) continue;
    //     track->injectLiveMidiMessage(juce::MidiMessage::allNotesOff((int) i + 1), {});
    // }
}

}