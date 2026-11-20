#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include "EnvelopeShape.h"

namespace MoTool::uZX {

/** A voice's chip timbre. */
struct NoteTimbre
{
    bool tone = true;       ///< the square wave sounds
    bool noise = false;     ///< the noise sounds
    bool envelope = false;  ///< the chip's envelope shapes the level
    bool drivesEnvelope = false;  ///< its pitch is the pitch of the chip's envelope
    EnvelopeShapeEnum::Enum envelopeShape{};  ///< the chip's envelope shape, from the voice that drives the envelope
    int noisePeriod = 0;    ///< the chip's noise period, 0..31, from a voice with the noise on
};

/** One voice sounding in a frame. */
struct NoteVoice
{
    int channel = 1;      ///< the MIDI channel it was played or read on; with the key, which voice this is
    int key = 0;          ///< the MIDI note it started as
    double pitch = 0.0;   ///< what sounds, in semitones on the MIDI note scale
    float level = 0.0f;   ///< 0..1
    bool attack = false;  ///< starts in this frame, even if the same voice sounded in the frame before
    bool restartEnvelope = false;  ///< restarts the chip's envelope in this frame, if it drives the envelope
    int priority = 0;  ///< who plays when the chip has too few channels: the higher, then the later in the frame
    std::optional<float> pan{};  ///< where it asks to sound, 0 left .. 1 right; anywhere without one
    NoteTimbre timbre{};
};

/** The voices sounding in one frame, in the order they were added. */
class NoteFrame
{
public:
    static constexpr std::size_t maxVoices = 16;

    /** A full frame drops the voice. */
    void add(const NoteVoice& voice) noexcept {
        if (count_ < maxVoices)
            voices_[count_++] = voice;
    }

    template <typename Predicate>
    void removeIf(Predicate&& matches) noexcept {
        count_ = static_cast<std::size_t>(std::remove_if(begin(), end(), matches) - begin());
    }

    void remove(int channel, int key) noexcept {
        removeIf([=](const NoteVoice& v) { return v.channel == channel && v.key == key; });
    }

    const NoteVoice* find(int channel, int key) const noexcept {
        for (const auto& v : *this)
            if (v.channel == channel && v.key == key)
                return &v;
        return nullptr;
    }

    NoteVoice* find(int channel, int key) noexcept {
        return const_cast<NoteVoice*>(std::as_const(*this).find(channel, key));
    }

    bool isEmpty() const noexcept { return count_ == 0; }

    const NoteVoice* begin() const noexcept { return voices_.data(); }
    const NoteVoice* end() const noexcept { return voices_.data() + count_; }
    NoteVoice* begin() noexcept { return voices_.data(); }
    NoteVoice* end() noexcept { return voices_.data() + count_; }

private:
    std::array<NoteVoice, maxVoices> voices_{};
    std::size_t count_ = 0;
};

}  // namespace MoTool::uZX
