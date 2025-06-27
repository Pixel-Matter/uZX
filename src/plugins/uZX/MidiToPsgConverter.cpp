#include "MidiToPsgConverter.h"
#include "../../models/PsgMidi.h"

namespace MoTool::uZX {

MidiToPsgConverter::MidiToPsgConverter(int baseChannel, int numChannels)
    : baseChannel_(baseChannel)
    , numChannels_(juce::jlimit(1, 4, numChannels))
{
}

void MidiToPsgConverter::noteOn(int channel, int note, int velocity) {
    if (!isChannelInRange(channel) || velocity == 0) return;

    auto& state = getChannelState(channel);

    state.currentNote = note;
    state.velocity = velocity;
    updateChannelOutput(channel);
}

void MidiToPsgConverter::noteOff(int channel, int note) {
    if (!isChannelInRange(channel)) return;

    auto& state = getChannelState(channel);

    // Only turn off if this is the currently playing note
    if (state.currentNote == note) {
        if (state.toneOn) {
            emitToneSwitchCC(channel, false);
        }
        state.clear();
    }
}

void MidiToPsgConverter::aftertouch(int channel, int aftertouch) {
    if (!isChannelInRange(channel)) return;

    auto& state = getChannelState(channel);
    state.aftertouch = aftertouch;

    // Update output if note is playing
    if (state.currentNote.has_value()) {
        updateChannelOutput(channel);
    }
}

void MidiToPsgConverter::controlChange(int channel, int controller, int value) {
    // Pass through all CC messages unchanged
    emitPassthroughCC(channel, controller, value);
}

std::vector<juce::MidiMessage> MidiToPsgConverter::getOutputMessages() {
    return std::move(outputBuffer_);
}

const MidiToPsgConverter::ChannelState& MidiToPsgConverter::getChannelState(int channel) const {
    return channels_[static_cast<size_t>(channel - baseChannel_)];
}

MidiToPsgConverter::ChannelState& MidiToPsgConverter::getChannelState(int channel) {
    return channels_[static_cast<size_t>(channel - baseChannel_)];
}

bool MidiToPsgConverter::isChannelInRange(int channel) const {
    return channel >= baseChannel_ && channel < baseChannel_ + numChannels_;
}

void MidiToPsgConverter::emitVolumeCC(int channel, int volume) {
    auto msg = juce::MidiMessage::controllerEvent(channel, static_cast<int>(MidiCCType::Volume), volume);
    outputBuffer_.push_back(msg);
}

void MidiToPsgConverter::emitPeriodCC(int channel, int period) {
    // Split 12-bit period into coarse (high 5 bits) and fine (low 7 bits)
    int coarse = (period >> 7) & 0x7F;  // bits 7-11 -> 0-31 (high 5 bits)
    int fine = period & 0x7F;           // bits 0-6 -> 0-127 (low 7 bits)

    auto coarseMsg = juce::MidiMessage::controllerEvent(channel, static_cast<int>(MidiCCType::CC20PeriodCoarse), coarse);
    auto fineMsg = juce::MidiMessage::controllerEvent(channel, static_cast<int>(MidiCCType::CC52PeriodFine), fine);

    outputBuffer_.push_back(coarseMsg);
    outputBuffer_.push_back(fineMsg);
}

void MidiToPsgConverter::emitToneSwitchCC(int channel, bool on) {
    auto msg = juce::MidiMessage::controllerEvent(channel, static_cast<int>(MidiCCType::GPB1ToneSwitch), on ? 127 : 0);
    outputBuffer_.push_back(msg);
}

void MidiToPsgConverter::emitPassthroughCC(int channel, int controller, int value) {
    auto msg = juce::MidiMessage::controllerEvent(channel, controller, value);
    outputBuffer_.push_back(msg);
}

int MidiToPsgConverter::velocityAndAftertouchToVolume(int velocity, int aftertouch) const {
    // Combine velocity and aftertouch, map from 0-127 to 0-15
    int combined = juce::jlimit(0, 127, velocity + aftertouch);
    return (combined * 15) / 127;
}

void MidiToPsgConverter::updateChannelOutput(int channel) {
    auto& state = getChannelState(channel);
    if (!state.currentNote.has_value()) return;

    // Only emit volume if it changed
    int currentVolume = velocityAndAftertouchToVolume(state.velocity, state.aftertouch);
    if (currentVolume != state.lastVolume) {
        emitVolumeCC(channel, currentVolume);
        state.lastVolume = currentVolume;
    }

    // Emit period if tuning system is available
    if (tuningSystem_ != nullptr) {
        emitPeriodCC(channel, tuningSystem_->midiNoteToPeriod(state.currentNote.value()));
    }

    // Only emit tone on if not already on
    if (!state.toneOn) {
        emitToneSwitchCC(channel, true);
        state.toneOn = true;
    }
}

} // namespace MoTool::uZX