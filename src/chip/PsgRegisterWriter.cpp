#include "PsgRegisterWriter.h"

namespace MoTool::uZX {

namespace {

/** A missing parameter leaves the previous value in effect. The first explicit value always gets sent. */
template <typename T>
bool changed(const std::optional<T>& incoming, std::optional<T>& previous) noexcept {
    if (!incoming || incoming == previous)
        return false;
    previous = incoming;
    return true;
}

}  // namespace

PsgRegisterFrame PsgRegisterWriter::write(const PsgPatch& patch) noexcept {
    held_.clear();  // Clear write flags, retaining bytes that contain other voices' switches.
    for (std::size_t i = 0; i < patch.channels.size(); ++i) {
        const auto& voice = patch.channels[i];
        auto& previous = last_.channels[i];
        if (!initialized_ || voice.volume != previous.volume || voice.envelopeOn != previous.envelopeOn) {
            held_.setVolumeAndEnvMod(i, static_cast<std::uint8_t>(voice.volume), voice.envelopeOn);
            previous.volume = voice.volume;
            previous.envelopeOn = voice.envelopeOn;
        }
        if (changed(voice.tonePeriod, previous.tonePeriod))
            held_.setTonePeriod(i, *voice.tonePeriod);  // A period change writes both bytes.
        if (changed(voice.toneOn, previous.toneOn))
            held_.setToneOn(i, *voice.toneOn);
        if (changed(voice.noiseOn, previous.noiseOn))
            held_.setNoiseOn(i, *voice.noiseOn);
    }

    if (changed(patch.noisePeriod, last_.noisePeriod))
        held_.setNoisePeriod(static_cast<std::uint8_t>(*patch.noisePeriod));
    if (changed(patch.envelopePeriod, last_.envelopePeriod))
        held_.setEnvelopePeriod(*patch.envelopePeriod);

    const auto shape = static_cast<std::uint8_t>(patch.envelopeShape.value_or(held_.getEnvelopeShape()));
    if (patch.retriggerEnvelope || shape != held_.getEnvelopeShape())
        held_.setEnvelopeShape(shape);

    initialized_ = true;
    return held_;
}

}  // namespace MoTool::uZX
