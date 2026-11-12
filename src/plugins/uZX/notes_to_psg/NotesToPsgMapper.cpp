#include "NotesToPsgMapper.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "../../../models/tuning/TuningRegistry.h"

namespace MoTool::uZX {

//==============================================================================
ChipMixer::Voices ChipMixer::allocate(const NoteFrame& frame) noexcept {
    // Best first: the higher priority, then the later voice in the frame
    std::array<const NoteVoice*, NoteFrame::maxVoices> best{};
    size_t count = 0;
    for (const auto& voice : frame)
        best[count++] = &voice;
    std::sort(best.begin(), best.begin() + count, [](const NoteVoice* a, const NoteVoice* b) {
        return a->priority != b->priority ? a->priority > b->priority : a > b;
    });

    Voices voices;
    std::array<const NoteVoice*, numToneChannels> playing{};  // those that get a channel, best first
    size_t numPlaying = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto* voice = best[i];
        const auto& timbre = voice->timbre;
        if (voices.envelope == nullptr && timbre.drivesEnvelope)
            voices.envelope = voice;
        if (numPlaying == numToneChannels || !(timbre.tone || timbre.noise || timbre.envelope))
            continue;
        playing[numPlaying++] = voice;
        if (voices.noise == nullptr && timbre.noise)
            voices.noise = voice;
    }

    const auto idOf = [](const NoteVoice& voice) { return VoiceId{voice.channel, voice.key}; };
    const auto place = [&](size_t i, size_t n) {
        if (playing[i] != nullptr && voices.channels[n] == nullptr)
            voices.channels[n] = std::exchange(playing[i], nullptr);
    };
    // Where it asks to sound
    for (size_t i = 0; i < numPlaying; ++i)
        if (const auto pan = playing[i]->pan) {
            const auto nearest = std::min_element(pans_.begin(), pans_.end(), [&](double a, double b) {
                return std::abs(a - double(*pan)) < std::abs(b - double(*pan));
            });
            place(i, static_cast<size_t>(nearest - pans_.begin()));
        }
    // The channel it had
    for (size_t i = 0; i < numPlaying; ++i)
        for (size_t n = 0; n < numToneChannels; ++n)
            if (playing[i] != nullptr && held_[n] == idOf(*playing[i]))
                place(i, n);
    // The lowest free one
    for (size_t i = 0; i < numPlaying; ++i)
        for (size_t n = 0; n < numToneChannels; ++n)
            place(i, n);

    for (size_t n = 0; n < numToneChannels; ++n)
        held_[n] = voices.channels[n] != nullptr ? idOf(*voices.channels[n]) : VoiceId{};
    return voices;
}

PsgParamFrameData ChipMixer::render(const Voices& voices, const TuningSystem& tuning) noexcept {
    PsgParamFrameData params;
    const auto set = [&](PsgParamType param, int value) { params.set(param, static_cast<uint16_t>(value)); };
    // A note the tuning has no period for leaves the period as it is
    const auto setPeriod = [&](PsgParamType param, const NoteVoice& voice, TuningSystem::PeriodMode mode) {
        if (const auto period = tuning.midiNoteToPeriod(voice.pitch, mode); period != 0)
            set(param, period);
    };

    for (int n = 0; n < numToneChannels; ++n) {
        const auto* voice = voices.channels[static_cast<size_t>(n)];
        // With its envelope on a channel sounds whatever its volume, so silence turns both off
        set(PsgParamType::VolumeA + n, voice != nullptr ? roundToInt(voice->level * 15.0f) : 0);
        set(PsgParamType::EnvelopeIsOnA + n, voice != nullptr && voice->timbre.envelope);
        if (voice == nullptr)
            continue;
        setPeriod(PsgParamType::TonePeriodA + n, *voice, TuningSystem::Tone);
        set(PsgParamType::ToneIsOnA + n, voice->timbre.tone);
        set(PsgParamType::NoiseIsOnA + n, voice->timbre.noise);
    }

    if (const auto* voice = voices.noise)
        set(PsgParamType::NoisePeriod, voice->timbre.noisePeriod);

    if (const auto* voice = voices.envelope) {
        setPeriod(PsgParamType::EnvelopePeriod, *voice, TuningSystem::Envelope);
        set(PsgParamType::EnvelopeShape, toRegister(voice->timbre.envelopeShape));
        // Only when the voice asks: a note that starts or lasts lets the envelope run
        if (voice->restartEnvelope)
            set(PsgParamType::RetriggerEnvelope, 1);
    }
    return params;
}

PsgParamFrameData ChipMixer::mix(const NoteFrame& frame, const TuningSystem& tuning) noexcept {
    return render(allocate(frame), tuning);
}

//==============================================================================
NotesToPsgMapper::NotesToPsgMapper() {
    // Default to standard 12-TET tuning
    setTuningSystem(makeBuiltinTuning(BuiltinTuningType::EqualTemperament));
}

TuningSystem& NotesToPsgMapper::getTuningSystem() {
    jassert(tuningSystem_ != nullptr);
    return *tuningSystem_;
}

void NotesToPsgMapper::setTuningSystem(std::shared_ptr<TuningSystem> tuning) {
    {
        const ScopedLock sl(lock_);
        std::swap(tuningSystem_, tuning);
    }
    // `tuning` is the old one now, freed here without keeping the audio thread waiting
}

void NotesToPsgMapper::setChannelPans(const std::array<double, ChipMixer::numToneChannels>& pans) {
    const ScopedLock sl(lock_);
    mixer_.setChannelPans(pans);
}

void NotesToPsgMapper::setBaseChannel(int channel) {
    const ScopedLock sl(lock_);
    reset();  // while the chip that heard the notes is still on these channels
    baseChannel_ = juce::jlimit(1, 16 - PsgCC::numChannels + 1, channel);
}

void NotesToPsgMapper::reset() {
    const ScopedLock sl(lock_);
    decoder_.reset();
    if (encoder_.hasSent())
        render(pending_, 0.0);  // no voices: what silences the chip, and the mixer forgets who played where
    encoder_.reset();
}

void NotesToPsgMapper::render(tracktion::MidiMessageArray& out, double time) {
    encoder_.encode(mixer_.mix(decoder_.takeFrame(), *tuningSystem_), out, time, baseChannel_);
}

void NotesToPsgMapper::operator()(MidiBufferContext& c) {
    const ScopedLock sl(lock_);
    const bool allNotesOff = c.isAllNotesOff();
    if (allNotesOff)
        reset();

    tracktion::MidiMessageArray out;
    out.mergeFromAndClear(pending_);
    out.isAllNotesOff = allNotesOff;  // for the chip as well

    // The messages of one time are read, then rendered at that time
    double time = 0.0;
    bool changed = false;
    for (const auto& m : c.buffer) {
        if (m.getTimeStamp() > time) {
            if (std::exchange(changed, false))
                render(out, time);
            time = m.getTimeStamp();
        }
        if (m.getChannel() > 0)
            changed = decoder_.decode(m) || changed;  // read or not, it goes no further
        else
            out.add(m);
    }
    if (changed)
        render(out, time);

    c.buffer.swapWith(out);
}

}  // namespace MoTool::uZX
