#include "TuningPlayer.h"

#include "juce_events/juce_events.h"

namespace MoTool {

void TuningPlayer::initialize() {
    updateTuning();

    midiPreview.getTransport().addChangeListener(this);
}

void TuningPlayer::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &viewModel) {
        updateTuning();
        stopNotes();
    } else if (source == &midiPreview.getTransport()) {
        // Transport state changed
        auto& transport = midiPreview.getTransport();
        if (!transport.isPlaying()) {
            // Transport stopped - clear playing notes
            playingNotes_.clear();
            notifyPlayingNotes();
        }
    }
}

void TuningPlayer::updateTuning() {
    midiPreview.setTuningSystem(viewModel.getTuningSystem());
}


void TuningPlayer::playSingleNote(int midiNote) {
    updateTuning();
    playingNotes_.clear();

    bool tone = viewModel.isToneEnabled();
    bool env = viewModel.isEnvelopeEnabled();
    int envelopeShape = viewModel.getEnvelopeShape();
    int envelopeInterval = viewModel.getEnvelopeInterval();

    // DBG("========== playSingleNote " << midiNote << " ==========");
    // DBG("Stopping notes");
    stopNotes(/*notify=*/ false);
    // DBG("Playing single note " << midiNote);
    playingNotes_[midiNote] = 1;
    midiPreview.playSingleNote(midiNote, 0.5, tone, env, envelopeShape, envelopeInterval);
    notifyPlayingNotes();

    // // Clear the note after a short delay to simulate note release
    // juce::Timer::callAfterDelay(400, [this, midiNote]() {
    //     midiPreview.stopPlayback();
    //     playingNotes_.erase(midiNote);
    //     notifyPlayingNotes();
    // });
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

    // jassert(scaleNotes.size() >= 5 && "Scale must have at least 5 notes for degree chord");
    for (size_t i = 0; i < scaleNotes.size(); i += 2) {
        // take every second note starting from the given note
        notes.push_back(scaleNotes[i]);
    }

    playChord(notes);
}

void TuningPlayer::playChord(const std::vector<int>& midiNotes) {
    constexpr double duration = 0.5;
    updateTuning();
    auto tone = viewModel.isToneEnabled();
    auto env = viewModel.isEnvelopeEnabled();
    int envelopeShape = viewModel.getEnvelopeShape();
    int modulationSemitones = viewModel.getEnvelopeInterval();

    stopNotes(/*notify=*/ false);

    midiPreview.playChord(midiNotes, duration, tone, env, envelopeShape, modulationSemitones);

    // Track playing notes
    for (size_t i = 0; i < std::min(midiNotes.size(), 3ul); ++i) {
        playingNotes_[midiNotes[i]] = static_cast<int>(i + 1);
    }
    notifyPlayingNotes();

    // Clear all notes after chord finishes
    // juce::Timer::callAfterDelay(duration, [this]() {
    //     midiPreview.stopPlayback();
    //     playingNotes_.clear();
    //     notifyPlayingNotes();
    // });
}

void TuningPlayer::playArpeggio(const std::vector<int>& midiNotes) {
    constexpr double duration = 0.25;
    updateTuning();
    auto tone = viewModel.isToneEnabled();
    auto env = viewModel.isEnvelopeEnabled();
    int envelopeShape = viewModel.getEnvelopeShape();
    int envelopeInterval = viewModel.getEnvelopeInterval();

    if (midiNotes.empty()) return;

    stopNotes(/*notify=*/ false);

    midiPreview.playArpeggio(midiNotes, duration, tone, env, envelopeShape, envelopeInterval);

    // Track playing notes (they will be playing sequentially)
    for (const auto& note : midiNotes) {
        playingNotes_[note] = 1; // Using channel 1 for arpeggio
    }
    notifyPlayingNotes();

    // // Clear all notes after arpeggio finishes
    // juce::Timer::callAfterDelay(duration * static_cast<int>(midiNotes.size()), [this]() {
    //     midiPreview.stopPlayback();
    //     playingNotes_.clear();
    //     notifyPlayingNotes();
    // });
}

void TuningPlayer::stopNotes(bool notify) {
    midiPreview.stopPlayback();
    playingNotes_.clear();
    if (notify) {
        notifyPlayingNotes();
    }
}


void TuningPlayer::notifyPlayingNotes() {
    listeners_.call([&](Listener& listener) {
        listener.playingNotesChanges();
    });
}

}