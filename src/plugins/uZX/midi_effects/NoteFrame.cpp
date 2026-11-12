#include "NoteFrame.h"

namespace MoTool::uZX {

void NoteEncoder::encode(const NoteFrame& frame, tracktion::MidiMessageArray& out, double time) {
    for (const auto& old : sent_) {
        const auto* now = frame.find(old.channel, old.key);
        if (now == nullptr || now->attack)
            out.addMidiMessage(MidiMessage::noteOff(old.channel, old.key, 0.0f), time, 0);
    }

    for (const auto& voice : frame) {
        const auto* last = voice.attack ? nullptr : sent_.find(voice.channel, voice.key);

        if (last == nullptr || !approximatelyEqual(last->pitch, voice.pitch)) {
            const auto bend = (voice.pitch - voice.key) / notePitchBendRange;
            out.addMidiMessage(
                MidiMessage::pitchWheel(voice.channel, jlimit(0, 16383, 8192 + roundToInt(bend * 8191.0))), time, 0);
        }

        // At least 1: a note on with velocity 0 is a note off
        if (last == nullptr)
            out.addMidiMessage(
                MidiMessage::noteOn(
                    voice.channel, voice.key, static_cast<uint8>(jlimit(1, 127, roundToInt(voice.level * 127.0f)))),
                time,
                0);
        else if (!approximatelyEqual(last->level, voice.level))
            out.addMidiMessage(
                MidiMessage::aftertouchChange(voice.channel, voice.key, roundToInt(voice.level * 127.0f)), time, 0);
    }

    sent_ = frame;
}

//==============================================================================
bool NoteControllers::decode(const juce::MidiMessage& m) noexcept {
    const auto channel = m.getChannel();
    if (channel < 1 || channel > 16 || !m.isController())
        return false;
    auto& chan = channels_[static_cast<size_t>(channel - 1)];

    // ponytail: a switch is on for any value but 0, as the chip reads its own; MIDI's 64 and up when the
    // wires get one meaning for a switch
    const auto value = m.getControllerValue();
    switch (static_cast<NoteCC>(m.getControllerNumber())) {
        case NoteCC::Tone:
            chan.timbre.tone = value != 0;
            break;
        case NoteCC::Noise:
            chan.timbre.noise = value != 0;
            break;
        case NoteCC::Envelope:
            chan.timbre.envelope = value != 0;
            break;
        case NoteCC::EnvelopeShape:
            chan.restartEnvelope = chan.restartEnvelope || chan.timbre.envelopeShape != value;
            chan.timbre.envelopeShape = value;
            break;
        case NoteCC::EnvelopeRestart:
            chan.restartEnvelope = chan.restartEnvelope || value != 0;
            break;
        case NoteCC::EnvelopeDriver:
            chan.timbre.drivesEnvelope = value != 0;
            break;
        case NoteCC::NoisePeriod:
            chan.timbre.noisePeriod = value;
            break;
        case NoteCC::Priority:
            chan.priority = value;
            break;
        case NoteCC::Pan:
            chan.pan = static_cast<float>(value) / 127.0f;
            break;
        default:
            return false;
    }
    return true;
}

void NoteControllers::applyTo(NoteFrame& frame) noexcept {
    for (auto& voice : frame) {
        const auto& chan = channels_[static_cast<size_t>(voice.channel - 1)];
        voice.timbre = chan.timbre;
        voice.restartEnvelope = chan.restartEnvelope;
        voice.priority = chan.priority >= 0 ? chan.priority : voice.channel;
        voice.pan = chan.pan;
    }
    for (const auto& voice : frame)
        channels_[static_cast<size_t>(voice.channel - 1)].restartEnvelope = false;
}

//==============================================================================
bool NoteDecoder::decode(const juce::MidiMessage& m) noexcept {
    const auto channel = m.getChannel();
    if (channel < 1 || channel > 16)
        return false;

    if (m.isNoteOn()) {
        // ponytail: a seventeenth note is dropped; steal the oldest if anything ever plays that many
        held_.remove(channel, m.getNoteNumber());
        held_.add({.channel = channel, .key = m.getNoteNumber(), .level = m.getFloatVelocity(), .attack = true});
    } else if (m.isNoteOff()) {
        held_.remove(channel, m.getNoteNumber());
    } else if (m.isAftertouch()) {
        if (auto* voice = held_.find(channel, m.getNoteNumber()))
            voice->level = static_cast<float>(m.getAfterTouchValue()) / 127.0f;
    } else if (m.isPitchWheel()) {
        bends_[static_cast<size_t>(channel - 1)] = (m.getPitchWheelValue() - 8192) / 8191.0 * notePitchBendRange;
    } else if (m.isAllNotesOff() || m.isAllSoundOff()) {
        held_.removeIf([=](const NoteVoice& v) { return v.channel == channel; });
    } else {
        return controllers_.decode(m);
    }
    return true;
}

NoteFrame NoteDecoder::takeFrame() noexcept {
    auto frame = held_;
    for (auto& voice : frame)
        voice.pitch = voice.key + bends_[static_cast<size_t>(voice.channel - 1)];
    controllers_.applyTo(frame);
    for (auto& voice : held_)
        voice.attack = false;
    return frame;
}

}  // namespace MoTool::uZX
