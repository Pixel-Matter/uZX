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

void TuningPlayer::sendNoteOn(int midiNote, int channel, bool isEnvelope) {
    // DBG("Sending note on: " << midiNote << " on channel " << channel
        // << ", envelope: " << (isEnvelope ? "Yes" : "No"));
    if (isEnvelope) {
        // DBG("Sending envelope note on: " << midiNote << " on channel " << channel);
        // send MidiCCType::SoundVariation 8
        // track.injectLiveMidiMessage(juce::MidiMessage::noteOn(channel, midiNote, static_cast<uint8>(127)), {});

        // env period
        track.injectLiveMidiMessage(juce::MidiMessage::noteOn(4, midiNote, static_cast<uint8>(127)), {});
        // env set shape and retriger
        track.injectLiveMidiMessage(juce::MidiMessage::controllerEvent(4,
                                    static_cast<int>(MidiCCType::SoundVariation), viewModel.getEnvelopeShape()), {});

        // tone retrigger
        // track.injectLiveMidiMessage(juce::MidiMessage::controllerEvent(channel,
        //                             static_cast<int>(MidiCCType::CC20PeriodCoarse), 0), {});
        // track.injectLiveMidiMessage(juce::MidiMessage::controllerEvent(channel,
        //                             static_cast<int>(MidiCCType::CC52PeriodFine), 0), {});

        // tone fifth higher
        // track.injectLiveMidiMessage(juce::MidiMessage::noteOn(channel, midiNote + 7, static_cast<uint8>(127)), {});
        // tone on
        track.injectLiveMidiMessage(juce::MidiMessage::controllerEvent(channel,
                                    static_cast<int>(MidiCCType::GPB1ToneSwitch), 0), {});
        // envelope on
        track.injectLiveMidiMessage(juce::MidiMessage::controllerEvent(channel,
                                    static_cast<int>(MidiCCType::GPB3EnvSwitch), 127), {});
    } else {
        track.injectLiveMidiMessage(juce::MidiMessage::noteOn(channel, midiNote, static_cast<uint8>(127)), {});
        track.injectLiveMidiMessage(juce::MidiMessage::controllerEvent(channel,
            static_cast<int>(MidiCCType::GPB1ToneSwitch), 127), {});
    }
    playingNotes_[midiNote] = channel;
}

void TuningPlayer::sendNoteOff(int midiNote, int channel, bool isEnvelope) {
    if (isEnvelope) {
        // DBG("Sending envelope note off: " << midiNote << " on channel " << channel);
        track.injectLiveMidiMessage(juce::MidiMessage::controllerEvent(channel,
            static_cast<int>(MidiCCType::Volume), 0), {});
        track.injectLiveMidiMessage(juce::MidiMessage::controllerEvent(channel,
            static_cast<int>(MidiCCType::GPB3EnvSwitch), 0), {});
        // note off not needed, but for symmetry
        track.injectLiveMidiMessage(juce::MidiMessage::noteOff(channel, midiNote), {});
        track.injectLiveMidiMessage(juce::MidiMessage::noteOff(4, midiNote), {});
    } else {
        track.injectLiveMidiMessage(juce::MidiMessage::noteOff(channel, midiNote), {});
    }
    playingNotes_.erase(midiNote);
}

void TuningPlayer::playNote(int midiNote) {
    updateTuning();
    int channel = getMonophonicChannel();
    bool env = viewModel.isEnvelopeEnabled();
    sendNoteOn(midiNote, channel, env);
    notifyPlayingNotes();

    // Clear the note after a short delay to simulate note release
    juce::Timer::callAfterDelay(200, [this, midiNote, channel, env]() {
        sendNoteOff(midiNote, channel, env);
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

    stop(/*notify=*/ false);

    // for no more than first 3 notes for chord playback
    for (size_t i = 0; i < std::min(midiNotes.size(), 3ul); ++i) {
        sendNoteOn(midiNotes[i], (int) i + 1, false);

        juce::Timer::callAfterDelay(static_cast<int>(duration), [this, note = midiNotes[i], channel = (int) i + 1]() {
            sendNoteOff(note, channel);
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
    bool env = viewModel.isEnvelopeEnabled();

    if (midiNotes.empty()) return;
    int channel = getMonophonicChannel();

    stop(/*notify=*/ false);

    // Play first note immediately
    sendNoteOn(midiNotes[0], channel, env);
    notifyPlayingNotes();

    // Add remaining notes with callAfterDelay
    for (size_t i = 1; i < midiNotes.size(); ++i) {
        juce::Timer::callAfterDelay(static_cast<int>(i * duration), [this, note = midiNotes[i], channel, env]() {
            stop(/*notify=*/ false);
            sendNoteOn(note, channel, env);
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
        sendNoteOff(note, channel); // This will erase the element from the map
        sendNoteOff(note, channel, true);
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