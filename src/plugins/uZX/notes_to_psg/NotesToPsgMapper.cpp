#include "NotesToPsgMapper.h"

#include <array>

#include "../../../models/tuning/TuningRegistry.h"

namespace MoTool::uZX {

//==============================================================================
PsgParamFrameData mixVoices(const NoteFrame& frame, const TuningSystem& tuning, int baseChannel) {
    constexpr int numToneChannels = 3;
    std::array<const NoteVoice*, PsgCC::numChannels> onChannel{};
    for (const auto& voice : frame)
        if (const auto n = voice.channel - baseChannel; isPositiveAndBelow(n, PsgCC::numChannels))
            onChannel[static_cast<size_t>(n)] = &voice;

    PsgParamFrameData params;
    const auto set = [&](PsgParamType param, int value) { params.set(param, static_cast<uint16_t>(value)); };
    // A note the tuning has no period for leaves the period as it is
    const auto setPeriod = [&](PsgParamType param, const NoteVoice& voice, TuningSystem::PeriodMode mode) {
        if (const auto period = tuning.midiNoteToPeriod(voice.pitch, mode); period != 0)
            set(param, period);
    };

    for (int n = 0; n < numToneChannels; ++n) {
        const auto* voice = onChannel[static_cast<size_t>(n)];
        // With its envelope on a channel sounds whatever its volume, so silence turns both off
        set(PsgParamType::VolumeA + n, voice != nullptr ? roundToInt(voice->level * 15.0f) : 0);
        set(PsgParamType::EnvelopeIsOnA + n, voice != nullptr && voice->timbre.envelope);
        if (voice == nullptr)
            continue;
        setPeriod(PsgParamType::TonePeriodA + n, *voice, TuningSystem::Tone);
        set(PsgParamType::ToneIsOnA + n, voice->timbre.tone);
        set(PsgParamType::NoiseIsOnA + n, voice->timbre.noise);
    }

    if (const auto* voice = onChannel[numToneChannels]) {
        setPeriod(PsgParamType::EnvelopePeriod, *voice, TuningSystem::Envelope);
        set(PsgParamType::NoisePeriod, voice->timbre.noisePeriod);
        // Setting the shape writes it, and a write restarts the envelope: only when the voice asks.
        // A note that starts or lasts lets the envelope run
        if (voice->restartEnvelope)
            set(PsgParamType::EnvelopeShape, voice->timbre.envelopeShape);
    }
    return params;
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

void NotesToPsgMapper::setBaseChannel(int channel) {
    const ScopedLock sl(lock_);
    reset();  // while the chip that heard the notes is still on these channels
    baseChannel_ = juce::jlimit(1, 16 - PsgCC::numChannels + 1, channel);
}

void NotesToPsgMapper::reset() {
    const ScopedLock sl(lock_);
    decoder_.reset();
    if (encoder_.hasSent())
        render(pending_, 0.0);  // no voices: what silences the chip
    encoder_.reset();
}

bool NotesToPsgMapper::isChannelInRange(int channel) const {
    return isPositiveAndBelow(channel - baseChannel_, PsgCC::numChannels);
}

void NotesToPsgMapper::render(tracktion::MidiMessageArray& out, double time) {
    encoder_.encode(mixVoices(decoder_.takeFrame(), *tuningSystem_, baseChannel_), out, time, baseChannel_);
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
        if (isChannelInRange(m.getChannel()))
            changed = decoder_.decode(m) || changed;
        else
            out.add(m);
    }
    if (changed)
        render(out, time);

    c.buffer.swapWith(out);
}

}  // namespace MoTool::uZX
