#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ChipAllocator.h"

namespace MoTool::uZX {

/** How the tuning converts a sounding pitch to a chip period. */
enum class PsgPeriodMode
{
    Tone,
    Envelope,
    EnvelopeTriangle,
};

struct PsgChannelPatch
{
    std::uint16_t volume = 0;
    bool envelopeOn = false;
    std::optional<std::uint16_t> tonePeriod;
    std::optional<bool> toneOn;
    std::optional<bool> noiseOn;
};

/** Parameters to write for one frame. An absent parameter retains its previous value. */
struct PsgPatch
{
    std::array<PsgChannelPatch, ChipAllocator::numToneChannels> channels{};
    std::optional<std::uint16_t> noisePeriod;
    std::optional<std::uint16_t> envelopePeriod;
    std::optional<std::uint16_t> envelopeShape;
    bool retriggerEnvelope = false;
};

/** Renders the assigned voices into PSG parameters. The tuning callback returns 0 for an undefined period. */
template <typename PeriodFor>
PsgPatch renderPsg(const ChipAllocator::Voices& voices, PeriodFor&& periodFor) noexcept {
    const auto getPeriod = [&](const NoteVoice& voice, PsgPeriodMode mode) -> std::optional<std::uint16_t> {
        if (const auto period = periodFor(voice.pitch, mode); period != 0)
            return static_cast<std::uint16_t>(period);
        return std::nullopt;
    };

    PsgPatch patch;
    for (std::size_t n = 0; n < patch.channels.size(); ++n) {
        const auto* voice = voices.channels[n];
        if (voice == nullptr)
            continue;  // The default volume and envelope switch silence an unassigned channel.

        auto& channel = patch.channels[n];
        // JUCE's roundToInt rounds halfway levels to even under the default rounding mode.
        channel.volume = static_cast<std::uint16_t>(std::nearbyint(voice->level * 15.0f));
        channel.envelopeOn = voice->timbre.envelope;
        channel.tonePeriod = getPeriod(*voice, PsgPeriodMode::Tone);
        channel.toneOn = voice->timbre.tone;
        channel.noiseOn = voice->timbre.noise;
    }

    if (const auto* voice = voices.noise)
        patch.noisePeriod = static_cast<std::uint16_t>(voice->timbre.noisePeriod);

    if (const auto* voice = voices.envelope) {
        const auto shape = voice->timbre.envelopeShape;
        const auto mode = isTriangle(shape)
            ? PsgPeriodMode::EnvelopeTriangle
            : PsgPeriodMode::Envelope;
        patch.envelopePeriod = getPeriod(*voice, mode);
        patch.envelopeShape = static_cast<std::uint16_t>(toRegister(shape));
        patch.retriggerEnvelope = voice->restartEnvelope;
    }
    return patch;
}

}  // namespace MoTool::uZX
