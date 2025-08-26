#include "NotesToPsgMapper.h"
#include "../../../models/tuning/TuningRegistry.h"
#include "juce_core/juce_core.h"
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
        // reset();
    }

void NotesToPsgMapper::ChannelVoice::reset() {
    initialNote.reset();
    actualNote = -1.0f;
    velocity = -1;
    aftertouchValue = -1;
    chipVolume = -1;
    // do not clear modulation switches, they should stay on until explicitly turned off

    if (!isEnvChannel) {
        emitVolume(0);
        // TODO keep track of tone/noise/envelope state and only emit changes
        emitEnvSwitch(false); // Ensure envelope are off initially  (by default)
        emitToneSwitch(true); // Ensure tone are on initially
        emitNoiseSwitch(false); // Ensure noise is off initially
    }
}

bool NotesToPsgMapper::ChannelVoice::isActive() const {
    return initialNote.has_value();
}

int NotesToPsgMapper::ChannelVoice::getEffectiveChipVolume() {
    // Volume calculation based on velocity and aftertouch
    // Simple linear scaling for demonstration purposes
    // Combine velocity and aftertouch, map from 0-127 to 0-15
    auto vol = (double) velocity * aftertouchValue / 127.0 / 127.0;
    // DBG("Rsulting velocity " << velocity << " and aftertouch " << aftertouch << " to " << combined);
    return roundToInt(vol * 15.0);
}

void NotesToPsgMapper::ChannelVoice::noteOn(int note, int vel, const TuningSystem& tuning) {
    initialNote = note;
    // TODO track pitch bend independently of note on and offs, use MPEInstrument
    actualNote = static_cast<float>(note);
    velocity = vel;
    aftertouchValue = 127; // Default to max aftertouch on note on
    chipVolume = -1; // Force volume update

    if (isEnvChannel) {
        // DBG("Note on on channel 4, using envelope period mapping");
        // Channel 4 is reserved for envelope periods
        emitPeriod(tuning.midiNoteToPeriod(note, TuningSystem::Envelope));
    } else {
        // DBG("Note on period on channel " << channel);
        emitPeriod(tuning.midiNoteToPeriod(note, TuningSystem::Tone));
        updateVolume();
    }
}

void NotesToPsgMapper::ChannelVoice::noteOff(int note) {
    if (!isActive() || initialNote != note) {
        // Ignore note off for notes that are not currently playing
        return;
    }
    reset();
}

void NotesToPsgMapper::ChannelVoice::aftertouch(int note, int value) {
    if (!isActive() || initialNote != note) {
        // Ignore aftertouch for notes that are not currently playing
        return;
    }
    aftertouchValue = value;
    updateVolume();
}

void NotesToPsgMapper::ChannelVoice::controllerChange(MidiCCType controller, int value) {
    if (controller == MidiCCType::AllNotesOff) {
        reset();
    } else {  // passthru
        emitControllerChange(controller, value);
    }
}

void NotesToPsgMapper::ChannelVoice::emitControllerChange(MidiCCType controller, int value) {
    auto msg = juce::MidiMessage::controllerEvent(midiChannel, static_cast<int>(controller), value);
    // DBG("Emitting CC: Channel " << channel << ", Controller " << controller << ", Value " << value);
    midiBuffer_.addMidiMessage(std::move(msg), mpeSourceId_);
}

void NotesToPsgMapper::ChannelVoice::emitVolume(int volume) {
    // DBG("Emitting Volume CC: Channel " << channel << ", Volume " << volume);
    emitControllerChange(MidiCCType::Volume, volume);
}

void NotesToPsgMapper::ChannelVoice::emitPeriod(int period) {
    // Split 12-bit period into coarse (high 5 bits) and fine (low 7 bits)
    int coarse = (period >> 7) & 0x7F;  // bits 7-11 -> 0-31 (high 5 bits)
    int fine = period & 0x7F;           // bits 0-6 -> 0-127 (low 7 bits)

    // DBG("Emitting Period CC: Channel " << channel << ", period " << period);

    auto coarseMsg = juce::MidiMessage::controllerEvent(midiChannel, static_cast<int>(MidiCCType::CC20PeriodCoarse), coarse);
    auto fineMsg = juce::MidiMessage::controllerEvent(midiChannel, static_cast<int>(MidiCCType::CC52PeriodFine), fine);

    midiBuffer_.addMidiMessage(std::move(coarseMsg), mpeSourceId_);
    midiBuffer_.addMidiMessage(std::move(fineMsg), mpeSourceId_);
}

void NotesToPsgMapper::ChannelVoice::emitToneSwitch(bool on) {
    emitControllerChange(MidiCCType::GPB1ToneSwitch, on ? 127 : 0);
}

void NotesToPsgMapper::ChannelVoice::emitNoiseSwitch(bool on) {
    emitControllerChange(MidiCCType::GPB2NoiseSwitch, on ? 127 : 0);
}

void NotesToPsgMapper::ChannelVoice::emitEnvSwitch(bool on) {
    emitControllerChange(MidiCCType::GPB3EnvSwitch, on ? 127 : 0);
}

// Emit volume if it was changed
void NotesToPsgMapper::ChannelVoice::updateVolume() {
    if (!isActive())
        return;

    if (int volume = getEffectiveChipVolume(); volume != chipVolume) {
        chipVolume = volume;
        emitVolume(volume);
    }
}

void NotesToPsgMapper::ChannelVoice::debug() const {
    DBG("ChannelVoice {"
        << " midiChannel: " << midiChannel
        << ", initialNote: " << (initialNote.has_value() ? std::to_string(initialNote.value()) : "none")
        << ", actualNote: " << actualNote
        << ", velocity: " << velocity
        << ", aftertouch: " << aftertouchValue
        << ", chipVolume: " << chipVolume
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

    mpeInstrument_.enableLegacyMode();
    // mpeInstrument_.addListener(this);
}

NotesToPsgMapper::~NotesToPsgMapper() {
    // mpeInstrument_.removeListener(this);
    setTuningSystem(nullptr);
}

void NotesToPsgMapper::setBaseChannel(int channel) {
    baseChannel_ = juce::jlimit(1, 16, channel);
    // Update midiChannel in voices
    for (size_t i = 0; i < voices_.size(); ++i) {
        voices_[i].midiChannel = baseChannel_ + static_cast<int>(i);
    }
}

void NotesToPsgMapper::reset() {
    for (auto& voice : voices_) {
        voice.reset();
    }
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
    auto& voice = getVoice(channel);
    if (msg.isNoteOn()) {
        jassert(tuningSystem_ != nullptr);
        voice.noteOn(msg.getNoteNumber(), msg.getVelocity(), *tuningSystem_);
    } else if (msg.isNoteOff()) {
        voice.noteOff(msg.getNoteNumber());
    } else if (msg.isAftertouch()) {
        voice.aftertouch(msg.getNoteNumber(), msg.getAfterTouchValue());
    } else if (msg.isController()) {
        voice.controllerChange(static_cast<MidiCCType>(msg.getControllerNumber()), msg.getControllerValue());
    } else if (passthruUnprocessedMIDI_) {
        midiBuffer_.addMidiMessage(msg, msg.mpeSourceID);
    }
    // TODO other mesages: pitchBend, tuning, channel aftertouch...
}

// Process MIDI input
void NotesToPsgMapper::operator()(MidiBufferContext& c) {
    if (c.buffer.isEmpty())
        return;

    // DBG("\n--- " << c.processStartTime() << " - " << c.processEndTime() << " --- (" << c.duration() << " duration) ---");
    // DBG(">---");
    // c.debugMidiBuffer();

    te::MidiMessageArray tempBuffer;

    for (const auto& m : c.buffer) {
        handleMidiMessage(m);
        tempBuffer.mergeFromWithOffset(takeOutputMessages(), m.getTimeStamp());
    }
    c.buffer.swapWith(tempBuffer);

    // DBG("--->");
    // c.debugMidiBuffer();
}

} // namespace MoTool::uZX