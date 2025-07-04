#include "TuningPlayer.h"

#include "../../plugins/uZX/aychip/AYPlugin.h"
#include "juce_events/juce_events.h"

namespace MoTool {

void TuningPlayer::initialize() {
    // Create edit with 1 track using the main engine
    edit.playInStopEnabled = true;

    createPlugins();
    createMIDIClip();
}

void TuningPlayer::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &viewModel) {
        updateTuning();
        stop();
    }
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

void TuningPlayer::sendCC(int channel, MidiCCType ccType, int value) {
    track.injectLiveMidiMessage(juce::MidiMessage::controllerEvent(channel, static_cast<int>(ccType), value), {});
}

void TuningPlayer::sendNoteOn(int channel, int midiNote, int velocity) {
    track.injectLiveMidiMessage(juce::MidiMessage::noteOn(channel, midiNote, static_cast<uint8>(velocity)), {});
}

void TuningPlayer::sendNoteOff(int channel, int midiNote) {
    track.injectLiveMidiMessage(juce::MidiMessage::noteOff(channel, midiNote), {});
}

void TuningPlayer::noteOn(int midiNote, int channel, bool isTone, bool isEnvelope) {
    // DBG("Sending note on: " << midiNote << " on channel " << channel
        // << ", envelope: " << (isEnvelope ? "Yes" : "No"));
    auto retrigger = viewModel.retriggerTone.get() && isTone;
    if (retrigger) {
        sendCC(channel, MidiCCType::CC20PeriodCoarse, 0);
        sendCC(channel, MidiCCType::CC52PeriodFine, 0);
        // we should delay everything to allow the note retrigger reset to be sent first
        Timer::callAfterDelay(5, [this, channel, midiNote, isTone, isEnvelope]() {
            noteOnNoRetrigger(midiNote, channel, isTone, isEnvelope);
        });
    } else {
        noteOnNoRetrigger(midiNote, channel, isTone, isEnvelope);
    }

    playingNotes_[midiNote] = channel;
}

void TuningPlayer::noteOnNoRetrigger(int midiNote, int channel, bool isTone, bool isEnvelope) {
    // DBG("Sending note on: " << midiNote << " on channel " << channel
        // << ", envelope: " << (isEnvelope ? "Yes" : "No"));
    if (isTone) {
        sendNoteOn(channel, midiNote, 127);
        sendCC(channel, MidiCCType::GPB1ToneSwitch, 127);
    }
    if (isEnvelope) {
        auto semitones = viewModel.getModulationSemitones();
        // DBG("Sending envelope note on: " << midiNote << " on channel " << channel);
        // env period
        sendNoteOn(4, midiNote + semitones, 127);
        // env set shape and retriger
        sendCC(4, MidiCCType::SoundVariation, viewModel.getEnvelopeShape());
        // envelope on
        sendCC(channel, MidiCCType::GPB3EnvSwitch, 127);
    }
}

void TuningPlayer::noteOff(int midiNote, int channel, bool isTone, bool isEnvelope) {
    if (isEnvelope) {
        // DBG("Sending envelope note off: " << midiNote << " on channel " << channel);
        sendCC(channel, MidiCCType::GPB3EnvSwitch, 0);
        sendNoteOff(4, midiNote);
    }
    if (isTone) {
        sendCC(channel, MidiCCType::GPB1ToneSwitch, 0);
        sendNoteOff(channel, midiNote);
    }
    playingNotes_.erase(midiNote);
}

void TuningPlayer::playSingleNote(int midiNote) {
    updateTuning();
    int channel = getMonophonicChannel();
    bool tone = viewModel.isToneEnabled();
    bool env = viewModel.isEnvelopeEnabled();
    noteOn(midiNote, channel, tone, env);
    notifyPlayingNotes();

    // Clear the note after a short delay to simulate note release
    juce::Timer::callAfterDelay(400, [this, midiNote, channel, tone, env]() {
        noteOff(midiNote, channel, tone, env);
        notifyPlayingNotes();
    });
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
    auto tone = viewModel.isToneEnabled();
    auto env = viewModel.isEnvelopeEnabled();

    stop(/*notify=*/ false);

    // for no more than first 3 notes for chord playback
    // TODO first note can be envelope enabled
    for (size_t i = 0; i < std::min(midiNotes.size(), 3ul); ++i) {
        noteOn(midiNotes[i], (int) i + 1, tone, i == 0 ? env : false);

        juce::Timer::callAfterDelay(static_cast<int>(duration), [this, note = midiNotes[i], channel = (int) i + 1, tone, env, i]() {
            noteOff(note, channel, tone, i == 0 ? env : false); // Only turn off tone, not envelope
            notifyPlayingNotes();
        });
    }
    notifyPlayingNotes();

    // // Clear all notes after chord finishes
    // juce::Timer::callAfterDelay(duration, [this, notesToPlay = playingNotes_]() {
    //     // TODO stop only initially intended to play notes
    //     stop();
    // });

    // Previous MIDI clip approach (commented out):
    // replaceNotes(midiNotes);
    // startPlayback();
}

void TuningPlayer::playArpeggio(const std::vector<int>& midiNotes) {
    constexpr int duration = 200;
    updateTuning();
    auto tone = viewModel.isToneEnabled();
    auto env = viewModel.isEnvelopeEnabled();

    if (midiNotes.empty()) return;
    int channel = getMonophonicChannel();

    stop(/*notify=*/ false);

    // Play first note immediately
    noteOn(midiNotes[0], channel, tone, env);
    notifyPlayingNotes();

    // Add remaining notes with callAfterDelay
    for (size_t i = 1; i < midiNotes.size(); ++i) {
        juce::Timer::callAfterDelay(static_cast<int>(i * duration), [this, note = midiNotes[i], channel, tone, env]() {
            stop(/*notify=*/ false);
        });
        juce::Timer::callAfterDelay(static_cast<int>(i * duration), [this, note = midiNotes[i], channel, tone, env]() {
            noteOn(note, channel, tone, env);
            notifyPlayingNotes();
        });
    }

    // Clear all notes after arpeggio finishes
    juce::Timer::callAfterDelay(duration * static_cast<int>(midiNotes.size()), [this]() {
        stop();
    });
}

void TuningPlayer::stop(bool notify) {
    while (!playingNotes_.empty()) {
        auto [note, channel] = *playingNotes_.begin();
        noteOff(note, channel, true, true); // This will erase the element from the map
    }
    if (notify) {
        notifyPlayingNotes();
    }
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

int TuningPlayer::getMonophonicChannel() const {
    return 3; // middle channel for monophonic playback
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