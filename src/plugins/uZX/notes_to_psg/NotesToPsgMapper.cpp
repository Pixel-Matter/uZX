#include "NotesToPsgMapper.h"

#include <utility>

#include "../../../chip/PsgRenderer.h"
#include "../../../models/tuning/TuningRegistry.h"

namespace MoTool::uZX {

//==============================================================================
PsgParamFrameData ChipMixer::mix(const NoteFrame& frame, const TuningSystem& tuning) noexcept {
    const auto patch = renderPsg(allocator_.allocate(frame), [&](double pitch, PsgPeriodMode mode) {
        constexpr std::array modes{TuningSystem::Tone, TuningSystem::Envelope, TuningSystem::EnvelopeTriangle};
        return tuning.midiNoteToPeriod(pitch, modes[static_cast<std::size_t>(mode)]);
    });

    PsgParamFrameData params;
    const auto setIfPresent = [&](PsgParamType param, const auto& value) {
        if (value.has_value())
            params.set(param, static_cast<uint16_t>(*value));
    };
    for (int n = 0; n < numToneChannels; ++n) {
        const auto& channel = patch.channels[static_cast<std::size_t>(n)];
        params.set(PsgParamType::VolumeA + n, channel.volume);
        params.set(PsgParamType::EnvelopeIsOnA + n, channel.envelopeOn);
        setIfPresent(PsgParamType::TonePeriodA + n, channel.tonePeriod);
        setIfPresent(PsgParamType::ToneIsOnA + n, channel.toneOn);
        setIfPresent(PsgParamType::NoiseIsOnA + n, channel.noiseOn);
    }
    setIfPresent(PsgParamType::NoisePeriod, patch.noisePeriod);
    setIfPresent(PsgParamType::EnvelopePeriod, patch.envelopePeriod);
    setIfPresent(PsgParamType::EnvelopeShape, patch.envelopeShape);
    if (patch.retriggerEnvelope)
        params.set(PsgParamType::RetriggerEnvelope, 1);
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

void NotesToPsgMapper::setChannelPans(const std::array<double, ChipMixer::numToneChannels>& pans) {
    const ScopedLock sl(lock_);
    mixer_.setChannelPans(pans);
}

void NotesToPsgMapper::setBaseChannel(int channel) {
    const ScopedLock sl(lock_);
    reset();  // while the chip that heard the notes is still on these channels
    baseChannel_ = juce::jlimit(1, 16 - PsgCC::numChannels + 1, channel);
}

void NotesToPsgMapper::setRecorder(PsgRecorder* recorder) {
    const ScopedLock sl(lock_);
    recorder_ = recorder;
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

    if (recorder_ != nullptr)
        for (const auto& m : out)
            if (m.isController())
                recorder_->add(c.playPosition.inSeconds() + m.getTimeStamp(), m.getChannel() - baseChannel_,
                               m.getControllerNumber(), m.getControllerValue());

    c.buffer.swapWith(out);
}

}  // namespace MoTool::uZX
