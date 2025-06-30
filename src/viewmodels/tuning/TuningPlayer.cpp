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
    playingNotes_.insert(midiNote);
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


const std::set<int>& TuningPlayer::getCurrentlyPlayingNotes() const {
    return playingNotes_;
}

void TuningPlayer::playChord(const std::vector<int>& midiNotes) {
    // updateTuning();
    // notifyPlayingNotes();
    // // Use playGuideNotes for immediate chord preview
    // juce::Array<int> notes(midiNotes.data(), midiNotes.size());
    // juce::Array<int> velocities;
    // velocities.resize(notes.size());
    // velocities.fill(80);
    // track.playGuideNotes(notes, te::MidiChannel(1), velocities, true);

    // // Previous MIDI clip approach (commented out):
    // // replaceNotes(midiNotes);
    // // startPlayback();
}

void TuningPlayer::playArpeggio(const std::vector<int>& midiNotes) {
    // updateTuning();
    // notifyPlayingNotes();
    // // For arpeggio, play notes sequentially using guide notes
    // track.turnOffGuideNotes(); // Stop any currently playing notes

    // // Play notes with 250ms delay between each
    // for (size_t i = 0; i < midiNotes.size(); ++i) {
    //     // TODO: Implement proper timing for arpeggio - this plays all at once
    //     // Consider using a Timer or async approach for sequential playback
    //     track.playGuideNote(midiNotes[i], te::MidiChannel(1), 80, false, false, true);
    // }

    // // Previous MIDI clip approach (commented out):
    // // replaceNotes(midiNotes, 0.25);
    // // startPlayback();
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