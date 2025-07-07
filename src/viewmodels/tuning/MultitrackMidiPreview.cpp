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
    setupClips();
}

void MultitrackMidiPreview::setupTracksAndPlugins() {
    // Setup tracks
    for (size_t i = 0; i < NUM_TRACKS; ++i) {
        tracks[i] = EngineHelpers::getOrInsertAudioTrackAt(edit, static_cast<int>(i));
    }

    // Create plugins on track 0
    auto& track = *tracks[0];

    if (auto ayPlugin = edit.getPluginCache().createNewPlugin(uZX::AYChipPlugin::xmlTypeName, {})) {
        track.pluginList.insertPlugin(*ayPlugin, 0, nullptr);
    }

    if (auto plugin = edit.getPluginCache().createNewPlugin(uZX::MidiToPsgPlugin::xmlTypeName, {})) {
        track.pluginList.insertPlugin(*plugin, 0, nullptr);
        midiToPsgPlugin = dynamic_cast<uZX::MidiToPsgPlugin*>(plugin.get());
    }
}

void MultitrackMidiPreview::setupClips() {
    const tracktion::TimeRange clipTimeRange(0s, edit.tempoSequence.toTime({ 8, {} }));

    for (size_t i = 0; i < NUM_TRACKS; ++i) {
        tracks[i]->insertNewClip(tracktion::TrackItem::Type::midi,
                                "MIDI Clip " + juce::String(static_cast<int>(i + 1)),
                                clipTimeRange,
                                nullptr);

        if (auto* midiClip = dynamic_cast<tracktion::MidiClip*>(tracks[i]->getClips()[0])) {
            clips[i] = midiClip;

            // Set MIDI channel for each clip (PSG uses channels 1-4)
            int midiChannel = static_cast<int>(i + 1);
            midiClip->setMidiChannel(tracktion::MidiChannel(midiChannel));
            DBG("Setting MIDI channel " << midiChannel << " for track " << i);
        }
    }
}


void MultitrackMidiPreview::setTuningSystem(TuningSystem* ts) {
    if (midiToPsgPlugin != nullptr) {
        midiToPsgPlugin->setTuningSystem(ts);
    }
}

void MultitrackMidiPreview::clearAllClips() {
    for (auto& clip : clips) {
        if (clip) {
            clip->getSequence().clear(nullptr);
        }
    }
}

void MultitrackMidiPreview::replaceNotesOnTrack(int trackIndex, const std::vector<int>& midiNotes, double noteLength, double startTime, bool enableTone, bool enableEnvelope, int envelopeShape, int envInterval) {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(NUM_TRACKS) || !clips[static_cast<size_t>(trackIndex)]) return;

    auto& sequence = clips[static_cast<size_t>(trackIndex)]->getSequence();
    sequence.clear(nullptr);

    const int velocity = 127;
    double currentTime = startTime;

    for (int midiNote : midiNotes) {
        // Add CC messages at the same time as the note
        // Note: Tracktion Engine uses 14-bit internal CC values that are converted to 7-bit MIDI values
        if (enableTone) {
            DBG("Adding tone switch ON for track " << trackIndex << " at beat " << currentTime);
            sequence.addControllerEvent(tracktion::BeatPosition::fromBeats(currentTime),
                                        static_cast<int>(MidiCCType::GPB1ToneSwitch), 127 << 7, nullptr);
        }

        if (enableEnvelope) {
            DBG("Adding envelope switch ON for track " << trackIndex << " at beat " << currentTime);
            sequence.addControllerEvent(tracktion::BeatPosition::fromBeats(currentTime),
                                        static_cast<int>(MidiCCType::GPB3EnvSwitch), 127 << 7, nullptr);

            // Add envelope shape on envelope channel (channel 4)
            if (trackIndex == 3) { // Envelope channel
                sequence.addControllerEvent(tracktion::BeatPosition::fromBeats(currentTime),
                                            static_cast<int>(MidiCCType::SoundVariation), envelopeShape << 7, nullptr);
            }
        }

        // Add the main note
        DBG("Adding note " << midiNote << " on track " << trackIndex << " at beat " << currentTime);
        sequence.addNote(midiNote,
                        tracktion::BeatPosition::fromBeats(currentTime),
                        tracktion::BeatDuration::fromBeats(noteLength),
                        velocity, 0, nullptr);

        // If envelope is enabled, add envelope note on a separate track
        if (enableEnvelope && trackIndex < 3) {
            // Find envelope track (track 3) and add envelope note
            if (clips[3]) {
                clips[3]->getSequence().addNote(midiNote + envInterval,
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

    // print out all notes in the track
    auto& seq = clips[static_cast<size_t>(trackIndex)]->getSequence();
    DBG("=== Notes in track " << trackIndex << " ===");
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
    clearAllClips();

    // Distribute notes across tracks (max 3 notes for chord, track 3 reserved for envelope)
    size_t noteCount = std::min(midiNotes.size(), static_cast<size_t>(3));

    for (size_t i = 0; i < noteCount; ++i) {
        std::vector<int> singleNote = {midiNotes[i]};
        bool envelopeForThisNote = enableEnvelope && (i == 0); // Only first note gets envelope
        replaceNotesOnTrack(static_cast<int>(i), singleNote, noteLength, 0.0, enableTone, envelopeForThisNote, envelopeShape, envInterval);
    }

    startPlayback();
}

void MultitrackMidiPreview::playSingleNote(int midiNote, double noteLength, bool enableTone, bool enableEnvelope, int envelopeShape, int modulationSemitones) {
    stopPlayback();
    clearAllClips();
    DBG("Playing single note: " << midiNote);

    replaceNotesOnTrack(0, {midiNote}, noteLength, 0.0, enableTone, enableEnvelope, envelopeShape, modulationSemitones);

    startPlayback();
}

void MultitrackMidiPreview::playArpeggio(const std::vector<int>& midiNotes, double noteLength, bool enableTone, bool enableEnvelope, int envelopeShape, int envInterval) {
    stopPlayback();
    clearAllClips();

    // Play arpeggio on first track
    replaceNotesOnTrack(0, midiNotes, noteLength, 0.0, enableTone, enableEnvelope, envelopeShape, envInterval);

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