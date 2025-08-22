#include "NotesToPsgMapper.h"
#include "../../../models/PsgMidi.h"

#include <cstddef>

namespace MoTool::uZX {

// NotesToPsgMapper::NotesToPsgMapper(int baseChannel, int numChannels)
//     : baseChannel_(baseChannel)
//     , numChannels_(juce::jlimit(1, 4, numChannels))
// {}

void NotesToPsgMapper::initPSG() {
    // Initialize channel states
    DBG("Initializing PSG with base channel " << baseChannel_ << " and " << numChannels_ << " channels");
    for (int i = baseChannel_; i < baseChannel_ + numChannels_; ++i) {
        auto& state = getChannelState(i);
        state.clear();
        // emitVolumeCC(i, 0); // Ensure all volumes are off initially (by default)
        // Contrary to that, AY on reset has all flags set
        emitToneSwitchCC(i, false); // Ensure all tones are off initially
        emitNoiseSwitchCC(i, false); // Ensure all noise is off initially
        // emitEnvSwitchCC(i, false); // Ensure all envelopes are off initially  (by default)
    }
}

void NotesToPsgMapper::noteOn(int channel, int note, int velocity) {
    // DBG("Note On: Channel " << channel << ", Note " << note << ", Velocity " << velocity);
    if (!isChannelInRange(channel) || velocity == 0) return;
    jassert(tuningSystem_ != nullptr);

    auto& state = getChannelState(channel);

    state.currentNote = note;
    state.velocity = velocity;

    if (channel - baseChannel_ == 3) {
        // DBG("Note on on channel 4, using envelope period mapping");
        // Channel 4 is reserved for envelope periods
        // TODO pass divider or env flag to tuning system
        emitPeriodCC(channel, tuningSystem_->midiNoteToPeriod(note, TuningSystem::Envelope));
    } else {
        // DBG("Note on period on channel " << channel);
        emitPeriodCC(channel, tuningSystem_->midiNoteToPeriod(note, TuningSystem::Tone));

        // Only emit volume if it changed
        int currentVolume = velocityAndAftertouchToVolume(state.velocity, state.aftertouch);
        if (currentVolume != state.lastVolume) {
            emitVolumeCC(channel, currentVolume);
            state.lastVolume = currentVolume;
        }
    }
}

void NotesToPsgMapper::noteOff(int channel, int /*note*/) {
    // DBG("Note Off: Channel " << channel << ", Note " << note);
    if (!isChannelInRange(channel)) return;

    auto& state = getChannelState(channel);

    // Only turn off if this is the currently playing note
    // DBG("Current note: " << (state.currentNote.has_value() ? std::to_string(state.currentNote.value()) : "none"));
    if (channel - baseChannel_ != 3) {
        // DBG("Turning off note on channel " << channel);
        emitVolumeCC(channel, 0);
        state.clear();
    }
}

void NotesToPsgMapper::allNotesOff(int channel) {
    // DBG("All Notes Off: Channel " << channel);
    if (!isChannelInRange(channel)) return;

    auto& state = getChannelState(channel);

    if (channel - baseChannel_ == 3) {
        // Channel 4 is reserved for envelope periods, no volume to turn off
        // DBG("All notes off on envelope channel, no volume to emit");
    } else {
        emitVolumeCC(channel, 0);
        emitEnvSwitchCC(channel, false); // Ensure all envelopes are off initially  (by default)
        state.clear();
    }
}

void NotesToPsgMapper::aftertouch(int channel, int aftertouch) {
    if (!isChannelInRange(channel)) return;

    auto& state = getChannelState(channel);
    state.aftertouch = aftertouch;

    // Update output if note is playing
    if (state.currentNote.has_value()) {
        updateNoteVolume(channel);
    }
}

void NotesToPsgMapper::controlChange(int channel, int controller, int value) {
    // Pass through all CC messages unchanged
    // DBG("Control Change: Channel " << channel << ", Controller " << controller << ", Value " << value);
    if (controller == static_cast<int>(MidiCCType::AllNotesOff)) {
        allNotesOff(channel);
    } else {
        emitCC(channel, controller, value);
    }
}

std::vector<juce::MidiMessage> NotesToPsgMapper::getOutputMessages() {
    return std::move(outputBuffer_);
}

const NotesToPsgMapper::ChannelState& NotesToPsgMapper::getChannelState(int channel) const {
    return channels_[static_cast<size_t>(channel - baseChannel_)];
}

NotesToPsgMapper::ChannelState& NotesToPsgMapper::getChannelState(int channel) {
    return channels_[static_cast<size_t>(channel - baseChannel_)];
}

bool NotesToPsgMapper::isChannelInRange(int channel) const {
    return channel >= baseChannel_ && channel < baseChannel_ + numChannels_;
}

void NotesToPsgMapper::emitVolumeCC(int channel, int volume) {
    // DBG("Emitting Volume CC: Channel " << channel << ", Volume " << volume);
    emitCC(channel, static_cast<int>(MidiCCType::Volume), volume);
}

void NotesToPsgMapper::emitPeriodCC(int channel, int period) {
    // Split 12-bit period into coarse (high 5 bits) and fine (low 7 bits)
    int coarse = (period >> 7) & 0x7F;  // bits 7-11 -> 0-31 (high 5 bits)
    int fine = period & 0x7F;           // bits 0-6 -> 0-127 (low 7 bits)

    // DBG("Emitting Period CC: Channel " << channel << ", period " << period);

    auto coarseMsg = juce::MidiMessage::controllerEvent(channel, static_cast<int>(MidiCCType::CC20PeriodCoarse), coarse);
    auto fineMsg = juce::MidiMessage::controllerEvent(channel, static_cast<int>(MidiCCType::CC52PeriodFine), fine);

    outputBuffer_.push_back(coarseMsg);
    outputBuffer_.push_back(fineMsg);
}

void NotesToPsgMapper::emitToneSwitchCC(int channel, bool on) {
    emitCC(channel, static_cast<int>(MidiCCType::GPB1ToneSwitch), on ? 127 : 0);
}

void NotesToPsgMapper::emitNoiseSwitchCC(int channel, bool on) {
    emitCC(channel, static_cast<int>(MidiCCType::GPB2NoiseSwitch), on ? 127 : 0);
}

void NotesToPsgMapper::emitEnvSwitchCC(int channel, bool on) {
    emitCC(channel, static_cast<int>(MidiCCType::GPB3EnvSwitch), on ? 127 : 0);
}

void NotesToPsgMapper::emitCC(int channel, int controller, int value) {
    auto msg = juce::MidiMessage::controllerEvent(channel, controller, value);
    // DBG("Emitting CC: Channel " << channel << ", Controller " << controller << ", Value " << value);

    outputBuffer_.push_back(msg);
}

int NotesToPsgMapper::velocityAndAftertouchToVolume(int velocity, int aftertouch) const {
    // Combine velocity and aftertouch, map from 0-127 to 0-15
    int combined = juce::jlimit(0, 127, velocity + aftertouch);
    return (combined * 15) / 127;
}

void NotesToPsgMapper::updateNoteVolume(int channel) {
    auto& state = getChannelState(channel);
    // DBG("updateChannelOutput Current note: " << (state.currentNote.has_value() ? std::to_string(state.currentNote.value()) : "none"));
    if (!state.currentNote.has_value()) return;

    // Only emit volume if it changed
    int currentVolume = velocityAndAftertouchToVolume(state.velocity, state.aftertouch);
    if (currentVolume != state.lastVolume) {
        emitVolumeCC(channel, currentVolume);
        state.lastVolume = currentVolume;
    }
}


void NotesToPsgMapper::processMidiMessageWithSource(const te::MidiMessageWithSource& msg) {
    // DBG("Processing MIDI message: " << msg.getDescription());
    // converter_.debugChannelStates();
    if (msg.isNoteOn()) {
        noteOn(msg.getChannel(), msg.getNoteNumber(), msg.getVelocity());
    } else if (msg.isNoteOff()) {
        noteOff(msg.getChannel(), msg.getNoteNumber());
    } else if (msg.isAftertouch()) {
        aftertouch(msg.getChannel(), msg.getAfterTouchValue());
    } else if (msg.isController()) {
        controlChange(msg.getChannel(), msg.getControllerNumber(), msg.getControllerValue());
    }
}


void NotesToPsgMapper::operator()(MidiBufferContext& c) {
    // Process MIDI input
    for (auto& m : c.buffer) {
        DBG("in midi message " << m.getDescription());
        processMidiMessageWithSource(m);
    }

    // Get output messages from converter and add to buffer
    auto outputMessages = getOutputMessages();
    c.buffer.clear();
    for (auto& msg : outputMessages) {
        DBG("out midi message " << msg.getDescription());
        c.buffer.addMidiMessage(std::move(msg), 0);
    }
}

} // namespace MoTool::uZX