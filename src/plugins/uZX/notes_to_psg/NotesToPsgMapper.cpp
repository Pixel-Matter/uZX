#include "NotesToPsgMapper.h"
#include "../../../models/PsgMidi.h"
#include "../../../models/tuning/TuningRegistry.h"
#include "juce_core/juce_core.h"

#include <cstddef>
#include <memory>

namespace MoTool::uZX {

NotesToPsgMapper::NotesToPsgMapper()
{
    // Default to standard 12-TET tuning
    setTuningSystem(defaultTuningSystem_.get());
}

// NotesToPsgMapper::NotesToPsgMapper(int baseChannel, int numChannels)
//     : baseChannel_(baseChannel)
//     , numChannels_(juce::jlimit(1, 4, numChannels))
// {}

void NotesToPsgMapper::initPSG() {
    // Initialize channel states
    // DBG("Initializing PSG with base channel " << baseChannel_ << " and " << numChannels_ << " channels");
    for (int i = baseChannel_; i < baseChannel_ + numChannels_; ++i) {
        auto& state = getChannelState(i);
        state.clear();
        // emitVolumeCC(i, 0); // Ensure all volumes are off initially (by default)
        // Contrary to that, AY on reset has all flags set
        // emitToneSwitchCC(i, false); // Ensure all tones are off initially
        emitToneSwitchCC(i, true); // Ensure all tones are on initially
        emitNoiseSwitchCC(i, false); // Ensure all noise is off initially
        // emitEnvSwitchCC(i, false); // Ensure all envelopes are off initially  (by default)
    }
}

void NotesToPsgMapper::noteOn(int channel, int note, int velocity) {
    DBG("Note On: Channel " << channel << ", Note " << note << ", Velocity " << velocity);
    if (!isChannelInRange(channel) || velocity == 0) return;
    jassert(tuningSystem_ != nullptr);

    auto& state = getChannelState(channel);

    // if note is playing, stop it
    state.clear();
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
            DBG("Setting volume to " << currentVolume << " for note " << note << " on channel " << channel
                << " (velocity " << velocity << ", aftertouch " << state.aftertouch << ")");
            state.lastVolume = currentVolume;
        }
    }
}

void NotesToPsgMapper::aftertouch(int channel, int note, int aftertouch) {
    if (!isChannelInRange(channel)) return;

    auto& state = getChannelState(channel);
    // DBG("Aftertouch: Channel " << channel << ", Note " << note << ", Aftertouch " << aftertouch);
    if (!state.currentNote.has_value() || state.currentNote != note) {
        // Ignore aftertouch for notes that are not currently playing
        // DBG("Ignoring aftertouch for non-playing note: Channel " << channel << ", Note " << note);
        return;
    }
    state.aftertouch = aftertouch;
    // DBG("Aftertouch: Channel " << channel << ", Note " << note << ", Aftertouch " << aftertouch);

    // Update output if note is playing
    if (state.currentNote.has_value()) {
        updateNoteVolume(channel);
    }
}

void NotesToPsgMapper::noteOff(int channel, int note) {
    if (!isChannelInRange(channel)) return;

    auto& state = getChannelState(channel);
    if (!state.currentNote.has_value() || state.currentNote != note) {
        // Ignore note off for notes that are not currently playing
        // DBG("Ignoring note off for non-playing note: Channel " << channel << ", Note " << note);
        return;
    }

    DBG("Note Off: Channel " << channel << ", Note " << note);

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

void NotesToPsgMapper::controlChange(int channel, int controller, int value) {
    // Pass through all CC messages unchanged
    // DBG("Control Change: Channel " << channel << ", Controller " << controller << ", Value " << value);
    if (controller == static_cast<int>(MidiCCType::AllNotesOff)) {
        DBG("All Notes Off CC received on channel " << channel);
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
    double combined = (double) velocity * aftertouch / 127.0 / 127.0;
    // DBG("Combined velocity " << velocity << " and aftertouch " << aftertouch << " to " << combined);
    return roundToInt(combined * 15.0);
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
        aftertouch(msg.getChannel(), msg.getNoteNumber(), msg.getAfterTouchValue());
    } else if (msg.isController()) {
        controlChange(msg.getChannel(), msg.getControllerNumber(), msg.getControllerValue());
    }
}


void NotesToPsgMapper::operator()(MidiBufferContext& c) {
    // Process MIDI input
    if (c.buffer.isEmpty())
        return;

    // DBG("\n--- " << c.processStartTime() << " - " << c.processEndTime() << " --- (" << c.duration() << " duration) ---");
    // DBG(">---");
    // c.debugMidiBuffer();

    for (auto& m : c.buffer) {
        processMidiMessageWithSource(m);
    }

    // Get output messages from converter and add to buffer
    auto outputMessages = getOutputMessages();
    c.buffer.clear();
    for (auto& msg : outputMessages) {
        c.buffer.addMidiMessage(std::move(msg), 0);
    }
    // DBG("--->");
    // c.debugMidiBuffer();
}

} // namespace MoTool::uZX