#include "MidiToPsgTransformer.h"
#include "../../models/PsgMidi.h"
#include <cstddef>

namespace MoTool::uZX {

MidiToPsgTransformer::MidiToPsgTransformer(int baseChannel, int numChannels)
    : baseChannel_(baseChannel)
    , numChannels_(juce::jlimit(1, 4, numChannels))
{
}

void MidiToPsgTransformer::initPSG() {
    // Initialize channel states
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

void MidiToPsgTransformer::noteOn(int channel, int note, int velocity) {
    // DBG("Note On: Channel " << channel << ", Note " << note << ", Velocity " << velocity);
    if (!isChannelInRange(channel) || velocity == 0) return;
    jassert(tuningSystem_ != nullptr);

    auto& state = getChannelState(channel);

    state.currentNote = note;
    state.velocity = velocity;

    // DBG("noteOn " << (state.currentNote.has_value() ? std::to_string(state.currentNote.value()) : "none")
    //     << ", channel " << channel
    //     << ", velocity: " << state.velocity
    //     // << ", toneOn: " << (state.toneOn ? "Yes" : "No")
    //     // << ", noiseOn: " << (state.noiseOn ? "Yes" : "No")
    //     // << ", envOn: " << (state.envOn ? "Yes" : "No")
    // );

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

void MidiToPsgTransformer::noteOff(int channel, int note) {
    // DBG("Note Off: Channel " << channel << ", Note " << note);
    if (!isChannelInRange(channel)) return;

    auto& state = getChannelState(channel);

    // Only turn off if this is the currently playing note
    // DBG("Current note: " << (state.currentNote.has_value() ? std::to_string(state.currentNote.value()) : "none"));
    if (state.currentNote == note) {
        if (channel - baseChannel_ != 3) {
            // DBG("Turning off note on channel " << channel);
            emitVolumeCC(channel, 0);
        }
        state.clear();
    }
}

void MidiToPsgTransformer::aftertouch(int channel, int aftertouch) {
    if (!isChannelInRange(channel)) return;

    auto& state = getChannelState(channel);
    state.aftertouch = aftertouch;

    // Update output if note is playing
    if (state.currentNote.has_value()) {
        updateNoteVolume(channel);
    }
}

void MidiToPsgTransformer::controlChange(int channel, int controller, int value) {
    // Pass through all CC messages unchanged
    emitCC(channel, controller, value);
}

std::vector<juce::MidiMessage> MidiToPsgTransformer::getOutputMessages() {
    auto result = std::move(outputBuffer_);
    outputBuffer_.clear();
    return result;
}

const MidiToPsgTransformer::ChannelState& MidiToPsgTransformer::getChannelState(int channel) const {
    return channels_[static_cast<size_t>(channel - baseChannel_)];
}

MidiToPsgTransformer::ChannelState& MidiToPsgTransformer::getChannelState(int channel) {
    return channels_[static_cast<size_t>(channel - baseChannel_)];
}

bool MidiToPsgTransformer::isChannelInRange(int channel) const {
    return channel >= baseChannel_ && channel < baseChannel_ + numChannels_;
}

void MidiToPsgTransformer::emitVolumeCC(int channel, int volume) {
    // DBG("Emitting Volume CC: Channel " << channel << ", Volume " << volume);
    auto msg = juce::MidiMessage::controllerEvent(channel, static_cast<int>(MidiCCType::Volume), volume);
    outputBuffer_.push_back(msg);
}

void MidiToPsgTransformer::emitPeriodCC(int channel, int period) {
    // Split 12-bit period into coarse (high 5 bits) and fine (low 7 bits)
    int coarse = (period >> 7) & 0x7F;  // bits 7-11 -> 0-31 (high 5 bits)
    int fine = period & 0x7F;           // bits 0-6 -> 0-127 (low 7 bits)

    // DBG("Emitting Period CC: Channel " << channel << ", period " << period);

    auto coarseMsg = juce::MidiMessage::controllerEvent(channel, static_cast<int>(MidiCCType::CC20PeriodCoarse), coarse);
    auto fineMsg = juce::MidiMessage::controllerEvent(channel, static_cast<int>(MidiCCType::CC52PeriodFine), fine);

    outputBuffer_.push_back(coarseMsg);
    outputBuffer_.push_back(fineMsg);
}

void MidiToPsgTransformer::emitToneSwitchCC(int channel, bool on) {
    emitCC(channel, static_cast<int>(MidiCCType::GPB1ToneSwitch), on ? 127 : 0);
}

void MidiToPsgTransformer::emitNoiseSwitchCC(int channel, bool on) {
    emitCC(channel, static_cast<int>(MidiCCType::GPB2NoiseSwitch), on ? 127 : 0);
}

void MidiToPsgTransformer::emitEnvSwitchCC(int channel, bool on) {
    emitCC(channel, static_cast<int>(MidiCCType::GPB3EnvSwitch), on ? 127 : 0);
}

void MidiToPsgTransformer::emitCC(int channel, int controller, int value) {
    auto msg = juce::MidiMessage::controllerEvent(channel, controller, value);
    // DBG("Emitting CC: Channel " << channel << ", Controller " << controller << ", Value " << value);
    // auto& state = getChannelState(channel);
    // DBG("emitCC Current note: " << (state.currentNote.has_value() ? std::to_string(state.currentNote.value()) : "none"));

    outputBuffer_.push_back(msg);
}

int MidiToPsgTransformer::velocityAndAftertouchToVolume(int velocity, int aftertouch) const {
    // Combine velocity and aftertouch, map from 0-127 to 0-15
    int combined = juce::jlimit(0, 127, velocity + aftertouch);
    return (combined * 15) / 127;
}

void MidiToPsgTransformer::updateNoteVolume(int channel) {
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

} // namespace MoTool::uZX