#include "TuningPlayer.h"

#include "../../plugins/uZX/aychip/AYPlugin.h"

namespace MoTool {

void TuningPlayer::initialize() {
    // Create edit with 1 track using the main engine
    edit.playInStopEnabled = true;

    createPlugins();
    createMIDIClip();
}

void TuningPlayer::createPlugins() {
    if (auto ayPlugin = edit.getPluginCache().createNewPlugin(uZX::AYChipPlugin::xmlTypeName, {})) {
        track.pluginList.insertPlugin(*ayPlugin, 0, nullptr);
    }

    if (auto plugin = edit.getPluginCache().createNewPlugin(uZX::MidiToPsgPlugin::xmlTypeName, {})) {
        track.pluginList.insertPlugin(*plugin, 0, nullptr);
        midiToPsgPlugin = dynamic_cast<uZX::MidiToPsgPlugin*>(plugin.get());
        updateTuning();
    }
}

te::MidiClip::Ptr TuningPlayer::getClip() {
    return dynamic_cast<te::MidiClip*>(track.getClips()[0]);
}


void TuningPlayer::updateTuning() {
    if (midiToPsgPlugin != nullptr) {
        midiToPsgPlugin->setTuningSystem(viewModel.getTuningSystem());
    }
}

te::MidiClip::Ptr TuningPlayer::createMIDIClip() {
    // Find length of 8 bars
    const tracktion::TimeRange editTimeRange(0s, edit.tempoSequence.toTime({ 8, {} }));
    track.insertNewClip(te::TrackItem::Type::midi, "MIDI Clip", editTimeRange, nullptr);

    if (auto midiClip = getClip()) {
        // auto& pg = *midiClip->getPatternGenerator();
        // pg.setChordProgressionFromChordNames ({"I", "V", "VI", "III", "IV", "I", "IV", "V"});
        // pg.mode = te::PatternGenerator::Mode::arpeggio;
        // pg.scaleRoot = 0;
        // pg.octave = 7;
        // pg.velocity = 30;
        // pg.generatePattern();

        return EngineHelpers::loopAroundClip(*midiClip);
    }
    return {};
}

void TuningPlayer::playNote(int midiNote) {
    // Use playGuideNote for immediate preview playback
    updateTuning();
    track.playGuideNote(midiNote, te::MidiChannel(1), 127, true, false, true);
    playingNotes_[midiNote] = 1; // Store note with its channel
    notifyPlayingNotes();

    // Clear the note after a short delay to simulate note release
    juce::Timer::callAfterDelay(100, [this, midiNote]() {
        playingNotes_.erase(midiNote);
        notifyPlayingNotes();
    });

    // Previous MIDI clip approach (commented out):
    // replaceNotes({midiNote});
    // startPlayback();
}

bool TuningPlayer::isNotePlaying(int midiNote) const {
    return playingNotes_.count(midiNote) > 0;
}

void TuningPlayer::playScale(int octave, bool chromatic) {
    // Build scale notes
    std::vector<int> notes;
    if (chromatic) {
        for (int i = 0; i < 12; ++i) {
            notes.push_back((octave + 1) * 12 + i);
        }
    } else {
        auto scaleNotes = viewModel.getScaleNotes(octave, true);
        for (const auto& note : scaleNotes) {
            notes.push_back(note);
        }
    }

    playArpeggio(notes);
}

void TuningPlayer::playDegreeChord(int midiNote) {
    // Build chord notes based on the degree
    std::vector<int> notes;
    auto scaleNotes = viewModel.getScaleNotesFrom(midiNote);
    if (scaleNotes.empty()) {
        return;
    }

    String notesStr;
    for (const auto& note : scaleNotes) {
        notesStr += String(note) + " ";
    }

    jassert(scaleNotes.size() >= 5 && "Scale must have at least 5 notes for degree chord");
    for (size_t i = 0; i < 5; i += 2) {
        // take every second note starting from the given note
        notes.push_back(scaleNotes[i]);
    }

    playChord(notes);
}

void TuningPlayer::playChord(const std::vector<int>& midiNotes) {
    constexpr int duration = 600;
    updateTuning();

    playingNotes_.clear();
    // for no more than first 3 notes for chord playback
    for (size_t i = 0; i < std::min(midiNotes.size(), 3ul); ++i) {
        track.playGuideNote(midiNotes[i], te::MidiChannel((int) i + 1), 127, false);
        playingNotes_[midiNotes[i]] = (int) i + 1; // Store note with its channel
    }
    notifyPlayingNotes();

    // Clear all notes after chord finishes
    juce::Timer::callAfterDelay(duration, [this]() {
        for (auto& [note, channel] : playingNotes_) {
            track.injectLiveMidiMessage(juce::MidiMessage::noteOff(channel, note), {});
        }
        // do not work properly (clears guiding notes array on forst midi channel)
        track.turnOffGuideNotes();
        playingNotes_.clear();
        notifyPlayingNotes();
    });

    // Previous MIDI clip approach (commented out):
    // replaceNotes(midiNotes);
    // startPlayback();
}

void TuningPlayer::playArpeggio(const std::vector<int>& midiNotes) {
    constexpr int duration = 200;
    updateTuning();

    if (midiNotes.empty()) return;

    // Play first note immediately
    track.playGuideNote(midiNotes[0], te::MidiChannel(1), 127, false);
    playingNotes_.clear();
    playingNotes_[midiNotes[0]] = 1; // Store note with its channel
    notifyPlayingNotes();

    // Add remaining notes with callAfterDelay
    for (size_t i = 1; i < midiNotes.size(); ++i) {
        juce::Timer::callAfterDelay(static_cast<int>(i * duration), [this, note = midiNotes[i]]() {
            track.turnOffGuideNotes();
            track.playGuideNote(note, te::MidiChannel(1), 127, true, true, false);
            playingNotes_.clear();
            playingNotes_[note] = 1; // Store note with its channel
            notifyPlayingNotes();
        });
    }

    // Clear all notes after arpeggio finishes
    juce::Timer::callAfterDelay(duration * static_cast<int>(midiNotes.size()), [this]() {
        track.turnOffGuideNotes();  // works for channel 1 only
        playingNotes_.clear();
        notifyPlayingNotes();
    });
}

void TuningPlayer::stop() {
    // Use turnOffGuideNotes for immediate stop
    track.turnOffGuideNotes();
    playingNotes_.clear();
    notifyPlayingNotes();

    // Previous transport approach (commented out):
    // transport.stop(false, false);
}

void TuningPlayer::replaceNotes(const std::vector<int>& midiNotes, double noteLength) {
    getClip()->getSequence().clear(nullptr);

    const int velocity = 80;
    double startTime = 0.0;

    for (int midiNote : midiNotes) {
        getClip()->getSequence().addNote(midiNote,
                                         te::BeatPosition::fromBeats(startTime),
                                         te::BeatDuration::fromBeats(noteLength),
                                         velocity, 0, nullptr);

        // For arpeggio, increment start time for each note
        if (midiNotes.size() > 1 && noteLength < 0.5) {
            startTime += noteLength;
        }
    }
}

void TuningPlayer::startPlayback() {
    updateTuning();
    transport.setPosition(te::TimePosition::fromSeconds(0.0));
    transport.play(false);
}

void TuningPlayer::notifyPlayingNotes() {
    listeners_.call([&](Listener& listener) {
        listener.playingNotesChanges();
    });
}

}