#include "ChipAllocator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace MoTool::uZX {

ChipAllocator::Voices ChipAllocator::allocate(const NoteFrame& frame) noexcept {
    // Best first: the higher priority, then the later voice in the frame
    std::array<const NoteVoice*, NoteFrame::maxVoices> best{};
    std::size_t count = 0;
    for (const auto& voice : frame)
        best[count++] = &voice;
    std::sort(best.begin(), best.begin() + count, [](const NoteVoice* a, const NoteVoice* b) {
        return a->priority != b->priority ? a->priority > b->priority : a > b;
    });

    Voices voices;
    std::array<const NoteVoice*, numToneChannels> playing{};  // those that get a channel, best first
    std::size_t numPlaying = 0;
    for (std::size_t i = 0; i < count; ++i) {
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
    const auto place = [&](std::size_t i, std::size_t n) {
        if (playing[i] != nullptr && voices.channels[n] == nullptr)
            voices.channels[n] = std::exchange(playing[i], nullptr);
    };
    // Where it asks to sound
    for (std::size_t i = 0; i < numPlaying; ++i)
        if (const auto pan = playing[i]->pan) {
            const auto nearest = std::min_element(pans_.begin(), pans_.end(), [&](double a, double b) {
                return std::abs(a - double(*pan)) < std::abs(b - double(*pan));
            });
            place(i, static_cast<std::size_t>(nearest - pans_.begin()));
        }
    // The channel it had
    for (std::size_t i = 0; i < numPlaying; ++i)
        for (std::size_t n = 0; n < numToneChannels; ++n)
            if (playing[i] != nullptr && held_[n] == idOf(*playing[i]))
                place(i, n);
    // The lowest free one
    for (std::size_t i = 0; i < numPlaying; ++i)
        for (std::size_t n = 0; n < numToneChannels; ++n)
            place(i, n);

    for (std::size_t n = 0; n < numToneChannels; ++n)
        held_[n] = voices.channels[n] != nullptr ? idOf(*voices.channels[n]) : VoiceId{};
    return voices;
}

}  // namespace MoTool::uZX
