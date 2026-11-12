#include "NoteFrame.h"

namespace MoTool::uZX {

namespace {

/** The controllers that turn what a channel was last sent into what a voice is now. */
void sendControllers(const NoteVoice& before, const NoteVoice& now, int channel, tracktion::MidiMessageArray& out,
                     double time) {
    const auto send = [&](NoteCC controller, int value) {
        out.addMidiMessage(MidiMessage::controllerEvent(channel, static_cast<int>(controller), value), time, 0);
    };
    const auto sendIfChanged = [&](NoteCC controller, int was, int is) {
        if (was != is)
            send(controller, is);
    };
    const auto onOff = [](bool on) { return on ? 127 : 0; };
    const auto panOf = [](const NoteVoice& voice) { return voice.pan ? roundToInt(*voice.pan * 127.0f) : -1; };

    sendIfChanged(NoteCC::Tone, onOff(before.timbre.tone), onOff(now.timbre.tone));
    sendIfChanged(NoteCC::Noise, onOff(before.timbre.noise), onOff(now.timbre.noise));
    sendIfChanged(NoteCC::Envelope, onOff(before.timbre.envelope), onOff(now.timbre.envelope));
    sendIfChanged(NoteCC::EnvelopeDriver, onOff(before.timbre.drivesEnvelope), onOff(now.timbre.drivesEnvelope));
    const auto shapeOf = [](const NoteVoice& voice) {
        return stepToController(voice.timbre.envelopeShape.asUnderlying(), static_cast<int>(EnvelopeShape::size()));
    };
    sendIfChanged(NoteCC::EnvelopeShape, shapeOf(before), shapeOf(now));
    sendIfChanged(NoteCC::NoisePeriod, stepToController(before.timbre.noisePeriod, numNoisePeriods),
                  stepToController(now.timbre.noisePeriod, numNoisePeriods));
    sendIfChanged(NoteCC::Priority, before.priority, jlimit(0, 127, now.priority));
    // ponytail: a voice cannot give its pan up while it sounds; a reset and everything again if one ever does
    if (panOf(now) >= 0)
        sendIfChanged(NoteCC::Pan, panOf(before), panOf(now));
    if (now.restartEnvelope) {
        // Released at once, so the value a host sends again is the released one
        send(NoteCC::EnvelopeRestart, 127);
        send(NoteCC::EnvelopeRestart, 0);
    }
}

}  // namespace

void NoteEncoder::encode(const NoteFrame& frame, tracktion::MidiMessageArray& out, double time) {
    const auto channelOf = [](size_t slot) { return firstChannel + static_cast<int>(slot); };

    // Voices that are gone, or start again: their channels are free
    for (size_t slot = 0; slot < sent_.size(); ++slot) {
        if (!sent_[slot])
            continue;
        const auto* now = frame.find(sent_[slot]->channel, sent_[slot]->key);
        if (now == nullptr || now->attack) {
            out.addMidiMessage(MidiMessage::noteOff(channelOf(slot), sent_[slot]->key, 0.0f), time, 0);
            sent_[slot].reset();
        }
    }

    for (const auto& voice : frame) {
        auto slot = static_cast<size_t>(
            std::find_if(sent_.begin(), sent_.end(), [&](const std::optional<NoteVoice>& s) {
                return s && s->channel == voice.channel && s->key == voice.key;
            }) - sent_.begin());
        const bool starts = slot == sent_.size();

        if (starts) {
            // The channel after the last one taken that is free: of the free ones, free the longest
            for (size_t i = 0; i < sent_.size() && slot == sent_.size(); ++i)
                if (const auto candidate = (next_ + i) % sent_.size(); !sent_[candidate])
                    slot = candidate;
            // ponytail: a voice beyond the fifteen channels is not sent; a second zone if anything plays that many
            if (slot == sent_.size())
                continue;
            next_ = (slot + 1) % sent_.size();
        }

        const int channel = channelOf(slot);
        // A new voice finds the controllers of whichever voice had the channel before, so they are reset
        NoteVoice before{.priority = -1};
        if (starts)
            out.addMidiMessage(MidiMessage::allControllersOff(channel), time, 0);
        else
            before = *sent_[slot];
        sendControllers(before, voice, channel, out, time);

        if (starts || !approximatelyEqual(before.pitch, voice.pitch)) {
            const auto bend = (voice.pitch - voice.key) / notePitchBendRange;
            out.addMidiMessage(
                MidiMessage::pitchWheel(channel, jlimit(0, 16383, 8192 + roundToInt(bend * 8191.0))), time, 0);
        }

        // At least 1: a note on with velocity 0 is a note off
        if (starts)
            out.addMidiMessage(
                MidiMessage::noteOn(channel, voice.key, static_cast<uint8>(jlimit(1, 127, roundToInt(voice.level * 127.0f)))),
                time,
                0);
        else if (!approximatelyEqual(before.level, voice.level))
            out.addMidiMessage(MidiMessage::aftertouchChange(channel, voice.key, roundToInt(voice.level * 127.0f)), time, 0);

        sent_[slot] = voice;
        sent_[slot]->priority = jlimit(0, 127, voice.priority);
    }
}

//==============================================================================
bool NoteControllers::decode(const juce::MidiMessage& m) noexcept {
    const auto channel = m.getChannel();
    if (channel < 1 || channel > 16 || !m.isController())
        return false;
    auto& chan = channels_[static_cast<size_t>(channel - 1)];

    if (m.isResetAllControllers()) {
        chan = {};
        return true;
    }

    const auto value = m.getControllerValue();
    const bool on = value >= 64;
    switch (static_cast<NoteCC>(m.getControllerNumber())) {
        case NoteCC::Tone:
            chan.timbre.tone = on;
            break;
        case NoteCC::Noise:
            chan.timbre.noise = on;
            break;
        case NoteCC::Envelope:
            chan.timbre.envelope = on;
            break;
        case NoteCC::EnvelopeShape:
            chan.timbre.envelopeShape = controllerToStep(value, static_cast<int>(EnvelopeShape::size()));
            break;
        case NoteCC::EnvelopeRestart:
            chan.restartEnvelope = chan.restartEnvelope || on;
            break;
        case NoteCC::EnvelopeDriver:
            chan.timbre.drivesEnvelope = on;
            break;
        case NoteCC::NoisePeriod:
            chan.timbre.noisePeriod = controllerToStep(value, numNoisePeriods);
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
        if (m.isResetAllControllers())
            bends_[static_cast<size_t>(channel - 1)] = 0.0;  // the wheel is one of them
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
