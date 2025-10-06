#include "NotesToPsgMapper.h"
#include "../../../models/tuning/TuningRegistry.h"

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

void NotesToPsgMapper::ChannelVoice::reset() {
    mpeNote = MPENote();

    state.volume = 0;
    state.envOn = false;
}

bool NotesToPsgMapper::ChannelVoice::isActive() const {
    return mpeNote.isValid();
}

int NotesToPsgMapper::ChannelVoice::getEffectiveChipVolume() const {
    if (!isActive() || isEnvChannel)
        return 0;
    // Volume calculation based on velocity and aftertouch
    // Simple linear scaling for demonstration purposes
    // Combine velocity and aftertouch, map from 0.0-1.0 to 0-15
    auto vol = mpeNote.pressure.asUnsignedFloat();
    // auto vol = (double) mpeNote.noteOnVelocity.asUnsignedFloat() * mpeNote.pressure.asUnsignedFloat();
    // DBG("Resulting velocity " << velocity << " and aftertouch " << aftertouch << " to " << combined);
    return roundToInt(vol * 15.0);
}

int NotesToPsgMapper::ChannelVoice::getEffectiveChipPeriod() const {
    if (!isActive())
        return 0;
    auto notePitch = static_cast<double>(mpeNote.initialNote) + mpeNote.totalPitchbendInSemitones;
    // DBG("Resulting velocity " << velocity << " and aftertouch " << aftertouch << " to " << combined);
    jassert(tuning_ != nullptr);
    return tuning_->midiNoteToPeriod(notePitch, isEnvChannel ? TuningSystem::Envelope: TuningSystem::Tone);
}

void NotesToPsgMapper::ChannelVoice::noteOn(MPENote newNote) {
    mpeNote = newNote;
    mpeNote.pressure = mpeNote.noteOnVelocity;
    // mpeNote.pressure = MPEValue::fromUnsignedFloat(1.0f); // Ensure pressure is set to max on note on

    // called in render()
    state.toneOn = true;   // TODO use mpeNote.timbre and calc in render()
    state.noiseOn = false; // TODO use mpeNote.timbre and calc in render()
    state.envOn = false;

    // DBG("NoteOn: " << mpeNote.initialNote << " vel: " << mpeNote.noteOnVelocity.asUnsignedFloat()
    //     << " pitch: " << mpeNote.initialNote + mpeNote.totalPitchbendInSemitones
    //     << " period: " << state.period
    //     << " volume: " << state.volume
    //     << " tone: " << (state.toneOn ? "on" : "off")
    //     << " last period: " << lastState.period
    //     << " last volume: " << lastState.volume
    //     << " last tone: " << (lastState.toneOn ? "on" : "off")
    // );
}

void NotesToPsgMapper::ChannelVoice::noteOff(MPENote offNote) {
    if (!isActive() || mpeNote.initialNote != offNote.initialNote) {
        // DBG("NoteOff for non-active note " << offNote.initialNote);
        // Ignore note off for notes that are not currently playing (polyphony on chip channel not supported)
        return;
    }
    // DBG("NoteOff: " << offNote.initialNote << ", note: "
    //     << (offNote.isValid() ? std::to_string(offNote.initialNote) : "none")
    // );
    reset();
}

void NotesToPsgMapper::ChannelVoice::aftertouch(MPENote changedNote) {
    if (!isActive() || mpeNote.initialNote != changedNote.initialNote) {
        // Ignore aftertouch for notes that are not currently playing
        return;
    }
    mpeNote.pressure = changedNote.pressure;
    state.volume = getEffectiveChipVolume();
}

void NotesToPsgMapper::ChannelVoice::pitchbendChange(MPENote changedNote) {
    if (!isActive() || mpeNote.initialNote != changedNote.initialNote) {
        // Ignore pitch bend for notes that are not currently playing
        return;
    }
    mpeNote.totalPitchbendInSemitones = changedNote.totalPitchbendInSemitones;
    state.period = getEffectiveChipPeriod();

    // DBG("PitchBend: " << changedNote.initialNote
    //     << ", pitch: " << mpeNote.initialNote + mpeNote.totalPitchbendInSemitones
    //     << ", period: " << state.period
    // );
}

void NotesToPsgMapper::ChannelVoice::controllerChange(MidiCCType controller, int value) {
    if (controller == MidiCCType::AllNotesOff) {
        reset();
    } else {  // passthru: envelope shape etc
        // TODO handle tone / noise / env switches to keep state in sync
        emitControllerChange(controller, value);
    }
}

void NotesToPsgMapper::ChannelVoice::emitControllerChange(MidiCCType controller, int value) {
    auto msg = juce::MidiMessage::controllerEvent(midiChannel, static_cast<int>(controller), value);
    // DBG("Emitting: " << msg.getDescription());
    midiBuffer_.addMidiMessage(std::move(msg), mpeSourceId_);
    // TODO keep track of current CC states?
}

void NotesToPsgMapper::ChannelVoice::emitVolume(int volume) {
    // DBG("Emitting Volume CC: Channel " << midiChannel << ", Volume " << volume);
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
    auto period = getEffectiveChipPeriod();
    if (period != 0 && lastState.period != period) {
        state.period = period;
        // DBG("UpdatePeriod: Channel " << midiChannel
        //     << ", note: " << (isActive() ? std::to_string(mpeNote.initialNote) : "none")
        //     << ", pitch: " << (isActive() ? std::to_string(mpeNote.initialNote + mpeNote.totalPitchbendInSemitones) : "none")
        //     << ", last period: " << lastState.period
        //     << ", current period: " << state.period
        // );
        emitPeriod(state.period);
    }
}

void  NotesToPsgMapper::ChannelVoice::updateModulation() {
    if (isEnvChannel) {
        return;  // No modulation for env channel
    }
    if (state.toneOn != lastState.toneOn) {
        emitToneSwitch(state.toneOn);
    }
    if (state.noiseOn != lastState.noiseOn) {
        emitNoiseSwitch(state.noiseOn);
    }
    if (state.envOn != lastState.envOn) {
        emitEnvSwitch(state.envOn);
    }
}

void NotesToPsgMapper::ChannelVoice::render() {
    updatePeriod();
    updateVolume();
    updateModulation();
}


void NotesToPsgMapper::ChannelVoice::debug() const {
    if (!isActive()) {
        DBG("ChannelVoice { inactive }");
        return;
    } else {
        DBG("ChannelVoice {"
            << " midiChannel: " << midiChannel
            << ", initialNote: " << mpeNote.initialNote
            << ", actualNote: " << (float) mpeNote.initialNote + mpeNote.totalPitchbendInSemitones
            << ", velocity: " << mpeNote.noteOnVelocity.asUnsignedFloat()
            << ", aftertouch: " << mpeNote.pressure.asUnsignedFloat()
            << " }");
    }
    DBG("ChannelSate {"
        << ", period: " << state.period
        << ", volume: " << state.volume
        << ", tone:   " << (state.toneOn  ? "on" : "off")
        << ", noise:  " << (state.noiseOn ? "on" : "off")
        << ", env:    " << (state.envOn   ? "on" : "off")
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

void NotesToPsgMapper::setTuningSystem(const TuningSystem* tuning) {
    // TODO locking for thread safety
    tuningSystem_ = tuning;
    for (auto& voice : voices_) {
        voice.setTuningSystem(tuningSystem_);
    }
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
        voice.render();
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
        }
        return;
    }
    mpeInstrument_.processNextMidiEvent(msg);

    // Process note on/off and other messages via mpeInstrument_
    if (msg.isAftertouch()) {
        // workaround to MPEInstrument not handling poly aftertouch on key channels
        mpeInstrument_.polyAftertouch(msg.getChannel(), msg.getNoteNumber(), MPEValue::from7BitInt(msg.getAfterTouchValue()));
    } else if (msg.isController()) {
        auto& voice = getVoice(channel);
        voice.controllerChange(static_cast<MidiCCType>(msg.getControllerNumber()), msg.getControllerValue());
    } else if (passthruUnprocessedMIDI_) {
        midiBuffer_.addMidiMessage(msg, msg.mpeSourceID);
    }
    // Other mesages can be handled here if needed
}

[[nodiscard]] te::MidiMessageArray NotesToPsgMapper::renderVoices() {
    for (auto& voice : voices_) {
        voice.render();
    }
    return takeOutputMessages();
}

// Process MIDI input
void NotesToPsgMapper::operator()(MidiBufferContext& c) {
    te::MidiMessageArray tempBuffer;

    if (c.isAllNotesOff()) {
        reset();
        // tempBuffer.mergeFromWithOffset(renderVoices(), 0.0f);
    }

    if (c.buffer.isEmpty()) {
        return;
    }

    // DBG("\n--- " << c.processStartTime() << " - " << c.processEndTime() << " --- (" << c.duration() << " duration) ---");
    // DBG(">---");
    // c.debugMidiBuffer();

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
    voice.noteOn(newNote);
}

/** Called when an MPE note is released. */
void NotesToPsgMapper::noteReleased(MPENote finishedNote) {
    auto& voice = getVoice(finishedNote.midiChannel);
    voice.noteOff(finishedNote);
}

/** Called when an MPE note's pressure changes. */
void NotesToPsgMapper::notePressureChanged(MPENote changedNote) {
    auto& voice = getVoice(changedNote.midiChannel);
    voice.aftertouch(changedNote);

    // DBG("Note pressure changed: " << changedNote.initialNote << " to " << changedNote.pressure.as7BitInt());
}

/** Called when an MPE note's pitchbend changes. */
void NotesToPsgMapper::notePitchbendChanged(MPENote changedNote) {
    // DBG("Note pitchbend changed: " << changedNote.initialNote << " to " << changedNote.pitchbend.asSignedFloat());

    jassert(tuningSystem_ != nullptr);
    auto& voice = getVoice(changedNote.midiChannel);
    voice.pitchbendChange(changedNote);
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