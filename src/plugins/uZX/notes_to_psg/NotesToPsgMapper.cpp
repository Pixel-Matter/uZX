#include "NotesToPsgMapper.h"
#include "../../../models/tuning/TuningRegistry.h"
#include "juce_core/system/juce_PlatformDefs.h"

#include <cstddef>
#include <memory>

namespace MoTool::uZX {

//==============================================================================
NotesToPsgMapper::ChannelVoice::ChannelVoice(tracktion::MidiMessageArray& buffer, int chan, bool isEnv)
        : midiBuffer_(buffer)
        , midiChannel(chan)
        , isEnvChannel(isEnv) // Channel 4 is env channel
    {
        reset();
    }

// TODO a way to switch note pitch when a chip channel do not stop a note: glissando or portamento or deep bends
// for this to work we should separate midi event callbacks and midi rendering
// midi callbacks modify the state
// midi rendering reads the state and emits midi messages ONLY for changed parameters
// so voice should keep two states: current and last emitted

// TODO if called always after ChannelVoice::reset(), render() should do the thing
void NotesToPsgMapper::ChannelVoice::emitSoundStop() {
    if (!isEnvChannel) {
        emitVolume(0);
        emitEnvSwitch(false); // Ensure envelope are off initially  (by default)
        emitToneSwitch(true); // Ensure tone are on initially
        emitNoiseSwitch(false); // Ensure noise is off initially
    }
}

void NotesToPsgMapper::ChannelVoice::reset() {
    mpeNote = MPENote();
    notePitch = -1.0f;
    aftertouchValue = -1;

    state.volume = 0;
    state.envOn = false;
}

bool NotesToPsgMapper::ChannelVoice::isActive() const {
    return mpeNote.isValid();
}

int NotesToPsgMapper::ChannelVoice::getEffectiveChipVolume() {
    if (!isActive() || isEnvChannel)
        return 0;
    // Volume calculation based on velocity and aftertouch
    // Simple linear scaling for demonstration purposes
    // Combine velocity and aftertouch, map from 0-127 to 0-15
    auto vol = (double) mpeNote.noteOnVelocity.asUnsignedFloat() * aftertouchValue / 127.0;
    // DBG("Resulting velocity " << velocity << " and aftertouch " << aftertouch << " to " << combined);
    return roundToInt(vol * 15.0);
}

// TODO pass MPENote here
void NotesToPsgMapper::ChannelVoice::noteOn(MPENote newNote, const TuningSystem& tuning) {
    mpeNote = newNote;
    mpeNote.pressure = MPEValue::fromUnsignedFloat(1.0f); // Ensure pressure is set to max on note on
    notePitch = static_cast<double>(mpeNote.initialNote) + mpeNote.totalPitchbendInSemitones;
    aftertouchValue = 127; // Default to max aftertouch on note on

    state.volume = getEffectiveChipVolume();
    state.period = tuning.midiNoteToPeriod(notePitch, isEnvChannel ? TuningSystem::Envelope: TuningSystem::Tone);
    // DBG("NoteOn: " << mpeNote.initialNote << " vel: " << mpeNote.noteOnVelocity.asUnsignedFloat() << " pitch: " << notePitch
    //     << " period: " << state.period
    //     << " volume: " << state.volume
    //     << " last period: " << lastState.period
    // );
}

void NotesToPsgMapper::ChannelVoice::noteOff(MPENote offNote) {
    // TODO Note off means set volume to 0 and env to off (if set) ONLY?
    DBG("NoteOff: " << offNote.initialNote << ", current note: "
        << (mpeNote.isValid() ? std::to_string(mpeNote.initialNote) : "none")
    );
    if (!isActive() || mpeNote.initialNote != offNote.initialNote) {
        DBG("NoteOff for non-active note " << offNote.initialNote);
        // Ignore note off for notes that are not currently playing (polyphony on chip channel not supported)
        return;
    }
    reset();
}

void NotesToPsgMapper::ChannelVoice::aftertouch(int note, int value) {
    if (!isActive() || mpeNote.initialNote != note) {
        // Ignore aftertouch for notes that are not currently playing
        return;
    }
    aftertouchValue = value;
}

void NotesToPsgMapper::ChannelVoice::pitchbendChange(MPENote changedNote, const TuningSystem& tuning) {
    if (!isActive() || mpeNote.initialNote != changedNote.initialNote) {
        // Ignore pitch bend for notes that are not currently playing
        return;
    }
    mpeNote.totalPitchbendInSemitones = changedNote.totalPitchbendInSemitones;
    // TODO updateNotePitchState()
    notePitch = static_cast<double>(mpeNote.initialNote) + mpeNote.totalPitchbendInSemitones;
    state.period = tuning.midiNoteToPeriod(notePitch, isEnvChannel ? TuningSystem::Envelope: TuningSystem::Tone);

    DBG("PitchBend: " << changedNote.initialNote
        << ", pitch: " << notePitch
        << ", period: " << state.period
    );
}

void NotesToPsgMapper::ChannelVoice::controllerChange(MidiCCType controller, int value) {
    if (controller == MidiCCType::AllNotesOff) {
        reset();
    } else {  // passthru
        // TODO handle envelope shape,
        emitControllerChange(controller, value);
    }
}

// TODO this emit methods have nothing to do with channel itself, move them out to unitily functions
void NotesToPsgMapper::ChannelVoice::emitControllerChange(MidiCCType controller, int value) {
    auto msg = juce::MidiMessage::controllerEvent(midiChannel, static_cast<int>(controller), value);
    // DBG("Emitting: " << msg.getDescription());
    midiBuffer_.addMidiMessage(std::move(msg), mpeSourceId_);
    // TODO keep track of current CC states?
}

void NotesToPsgMapper::ChannelVoice::emitVolume(int volume) {
    // DBG("Emitting Volume CC: Channel " << channel << ", Volume " << volume);
    emitControllerChange(MidiCCType::Volume, volume);
    lastState.volume = volume;
}

void NotesToPsgMapper::ChannelVoice::emitPeriod(int p) {
    // DBG("Emitting Period: " << midiChannel << ", period " << p);

    // Split 12-bit period into coarse (high 5 bits) and fine (low 7 bits)
    int coarse = (p >> 7) & 0x7F;  // bits 7-11 -> 0-31 (high 5 bits)
    int fine = p & 0x7F;           // bits 0-6 -> 0-127 (low 7 bits)

    emitControllerChange(MidiCCType::CC20PeriodCoarse, coarse);
    emitControllerChange(MidiCCType::CC52PeriodFine, fine);
    lastState.period = p;
}

void NotesToPsgMapper::ChannelVoice::emitToneSwitch(bool on) {
    emitControllerChange(MidiCCType::GPB1ToneSwitch, on ? 127 : 0);
    lastState.toneOn = on;
}

void NotesToPsgMapper::ChannelVoice::emitNoiseSwitch(bool on) {
    emitControllerChange(MidiCCType::GPB2NoiseSwitch, on ? 127 : 0);
    lastState.noiseOn = on;
}

void NotesToPsgMapper::ChannelVoice::emitEnvSwitch(bool on) {
    emitControllerChange(MidiCCType::GPB3EnvSwitch, on ? 127 : 0);
    lastState.envOn = on;
}

// Emit volume if it was changed
void NotesToPsgMapper::ChannelVoice::updateVolume() {
    state.volume = getEffectiveChipVolume();

    if (state.volume != lastState.volume) {
        emitVolume(state.volume);
    }
}

void NotesToPsgMapper::ChannelVoice::updatePeriod() {
    if (lastState.period != state.period) {
        emitPeriod(state.period);
    }
}

void NotesToPsgMapper::ChannelVoice::render() {
    updatePeriod();
    updateVolume();
    // updateModulation();
}


void NotesToPsgMapper::ChannelVoice::debug() const {
    DBG("ChannelVoice {"
        << " midiChannel: " << midiChannel
        << ", initialNote: " << (mpeNote.isValid() ? String(mpeNote.initialNote) : "none")
        << ", velocity: " << (mpeNote.isValid() ? String(mpeNote.noteOnVelocity.asUnsignedFloat()) : "none")
        << ", actualNote: " << notePitch
        << ", aftertouch: " << aftertouchValue
        << ", volume: " << state.volume
        // << ", toneOn: " << (toneOn ? "true" : "false")
        // << ", noiseOn: " << (noiseOn ? "true" : "false")
        // << ", envOn: " << (envOn ? "true" : "false")
        << " }");
}


//==============================================================================
NotesToPsgMapper::NotesToPsgMapper()
    : voices_ {
        ChannelVoice(midiBuffer_, baseChannel_ + 0),
        ChannelVoice(midiBuffer_, baseChannel_ + 1),
        ChannelVoice(midiBuffer_, baseChannel_ + 2),
        ChannelVoice(midiBuffer_, baseChannel_ + 3, true)
    }
{
    // Default to standard 12-TET tuning
    setTuningSystem(defaultTuningSystem_.get());

    mpeInstrument_.enableLegacyMode(2, {baseChannel_, baseChannel_ + 4});
    mpeInstrument_.addListener(this);
}

NotesToPsgMapper::~NotesToPsgMapper() {
    mpeInstrument_.removeListener(this);
    setTuningSystem(nullptr);
}

void NotesToPsgMapper::setBaseChannel(int channel) {
    baseChannel_ = juce::jlimit(1, 16, channel);
    // Update midiChannel in voices
    for (size_t i = 0; i < voices_.size(); ++i) {
        voices_[i].midiChannel = baseChannel_ + static_cast<int>(i);
    }
    mpeInstrument_.setLegacyModeChannelRange({baseChannel_, baseChannel_ + 4});
}

void NotesToPsgMapper::reset() {
    for (auto& voice : voices_) {
        voice.reset();
        voice.emitSoundStop();
    }
    mpeInstrument_.releaseAllNotes();
}

te::MidiMessageArray NotesToPsgMapper::takeOutputMessages() {
    return std::move(midiBuffer_);
}

const NotesToPsgMapper::ChannelVoice& NotesToPsgMapper::getVoice(int channel) const {
    return voices_[static_cast<size_t>(channel - baseChannel_)];
}

NotesToPsgMapper::ChannelVoice& NotesToPsgMapper::getVoice(int channel) {
    jassert(isChannelInRange(channel));
    return voices_[static_cast<size_t>(channel - baseChannel_)];
}

bool NotesToPsgMapper::isChannelInRange(int channel) const {
    return channel >= baseChannel_ && channel < baseChannel_ + numChannels_;
}

void NotesToPsgMapper::handleMidiMessage(const te::MidiMessageWithSource& msg) {
    auto channel = msg.getChannel();
    if (!isChannelInRange(channel)) {
        if (passthruOutsideChannels_) {
            midiBuffer_.addMidiMessage(msg, msg.mpeSourceID);
        } else {
            return;
        }
    }
    mpeInstrument_.processNextMidiEvent(msg);

    // Process note on/off and other messages via mpeInstrument_
    auto& voice = getVoice(channel);
    // TODO use callbacks from MPEInstrument::Listener instead of handling here?
    if (msg.isAftertouch()) {
        // workaround to MPEInstrument not handling poly aftertouch on key channels
        mpeInstrument_.polyAftertouch(msg.getChannel(), msg.getNoteNumber(), MPEValue::from7BitInt(msg.getAfterTouchValue()));
        // TODO use callback?
        voice.aftertouch(msg.getNoteNumber(), msg.getAfterTouchValue());
    } else if (msg.isController()) {
        voice.controllerChange(static_cast<MidiCCType>(msg.getControllerNumber()), msg.getControllerValue());
    } else if (passthruUnprocessedMIDI_) {
        midiBuffer_.addMidiMessage(msg, msg.mpeSourceID);
    }
    // TODO other mesages: pitchBend, tuning, channel aftertouch...
}

te::MidiMessageArray NotesToPsgMapper::renderVoices() {
    for (auto& voice : voices_) {
        voice.render();
    }
    return takeOutputMessages();
}

// Process MIDI input
void NotesToPsgMapper::operator()(MidiBufferContext& c) {
    if (c.isAllNotesOff()) {
        reset();
    }

    if (c.buffer.isEmpty()) {
        return;
    }

    // DBG("\n--- " << c.processStartTime() << " - " << c.processEndTime() << " --- (" << c.duration() << " duration) ---");
    // DBG(">---");
    // c.debugMidiBuffer();
    te::MidiMessageArray tempBuffer;

    double lastTimestamp = 0.0f;
    for (const auto& m : c.buffer) {
        auto timestamp = m.getTimeStamp();
        if (timestamp > lastTimestamp) {
            tempBuffer.mergeFromWithOffset(renderVoices(), timestamp);
        }
        handleMidiMessage(m);
        lastTimestamp = timestamp;
    }

    tempBuffer.mergeFromWithOffset(renderVoices(), lastTimestamp);

    c.buffer.swapWith(tempBuffer);

    // DBG("--->");
    // c.debugMidiBuffer();
}

//==============================================================================
// MPEInstrument::Listener callbacks (connect MPE events to voice management)

/** Called when a new MPE note is added. */
void NotesToPsgMapper::noteAdded(MPENote newNote) {
    auto& voice = getVoice(newNote.midiChannel);
    voice.noteOn(newNote, *tuningSystem_);
}

/** Called when an MPE note is released. */
void NotesToPsgMapper::noteReleased(MPENote finishedNote) {
    auto& voice = getVoice(finishedNote.midiChannel);
    voice.noteOff(finishedNote);
}

/** Called when an MPE note's pressure changes. */
void NotesToPsgMapper::notePressureChanged(MPENote changedNote) {
    // TODO
    ignoreUnused(changedNote);
    DBG("Note pressure changed: " << changedNote.initialNote << " to " << changedNote.pressure.as7BitInt());
}

/** Called when an MPE note's pitchbend changes. */
void NotesToPsgMapper::notePitchbendChanged(MPENote changedNote) {
    // DBG("Note pitchbend changed: " << changedNote.initialNote << " to " << changedNote.pitchbend.asSignedFloat());

    jassert(tuningSystem_ != nullptr);
    auto& voice = getVoice(changedNote.midiChannel);
    voice.pitchbendChange(changedNote, *tuningSystem_);
}

/** Called when an MPE note's timbre changes. */
void NotesToPsgMapper::noteTimbreChanged(MPENote changedNote) {
    // TODO
    ignoreUnused(changedNote);
    // DBG("Note timbre changed: " << changedNote.initialNote);
}

void NotesToPsgMapper::debugChannelVoices() const {
    for (const auto& voice : voices_) {
        voice.debug();
    }
}


} // namespace MoTool::uZX