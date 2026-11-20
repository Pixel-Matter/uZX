#pragma once

#include <array>

#include "NoteFrame.h"

namespace MoTool::uZX {

/**
    Allocates the chip's channels and generators for a frame. Higher priority wins, with the later voice in the
    frame winning ties.

    The first three voices with tone, noise or envelope enabled get channels. They ask for the channel nearest
    their pan if they have one, then keep their previous channel if free, then take the lowest free channel.
    The first allocated voice with noise enabled drives the noise generator. The highest-ranked envelope driver controls the
    envelope generator, even when it has no channel.
*/
class ChipAllocator
{
public:
    static constexpr int numToneChannels = 3;

    /** Voices in the caller's frame. A null pointer means the channel or generator has no voice. */
    struct Voices
    {
        std::array<const NoteVoice*, numToneChannels> channels{};
        const NoteVoice* envelope = nullptr;
        const NoteVoice* noise = nullptr;
    };

    /** Assigns the voices and remembers their channel placements for the next frame. */
    Voices allocate(const NoteFrame& frame) noexcept;
    Voices allocate(const NoteFrame&&) = delete;  // the result borrows voices from the frame

    /** Stereo positions of channels A, B and C, from 0 (left) to 1 (right). */
    void setChannelPans(const std::array<double, numToneChannels>& pans) noexcept { pans_ = pans; }

private:
    struct VoiceId
    {
        int channel = 0;  // 0 for none
        int key = 0;
        bool operator==(const VoiceId&) const = default;
    };

    std::array<VoiceId, numToneChannels> held_{};
    std::array<double, numToneChannels> pans_{0.0, 0.5, 1.0};
};

}  // namespace MoTool::uZX
