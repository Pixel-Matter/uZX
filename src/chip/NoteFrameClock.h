#pragma once

#include <cstdint>

namespace MoTool::uZX {

/** Places note ticks on the first audio sample at or after their ideal time.
    Sample zero is the first tick. A block owns its first sample and ends before the next block's first sample.
*/
class NoteFrameClock
{
public:
    explicit NoteFrameClock(std::uint32_t sampleRate, std::uint32_t frameRate = 50) noexcept
        : sampleRate_(sampleRate), frameRate_(frameRate) {
        reset();
    }

    void reset() noexcept {
        blockStart_ = 0;
        nextTick_ = 0;
        // Adding this initial remainder rounds each cumulative tick position up.
        remainder_ = frameRate_ == 0 ? 0 : frameRate_ - 1;
    }

    /** Visits absolute tick positions for one audio block. A zero rate disables ticks. */
    template <typename OnTick>
    void advance(std::uint32_t blockSamples, OnTick&& onTick) {
        const auto end = blockStart_ + blockSamples;
        if (sampleRate_ > 0 && frameRate_ > 0) {
            while (nextTick_ < end) {
                onTick(nextTick_);
                remainder_ += sampleRate_;
                nextTick_ += remainder_ / frameRate_;
                remainder_ %= frameRate_;
            }
        }
        blockStart_ = end;
    }

private:
    std::uint32_t sampleRate_;
    std::uint32_t frameRate_;
    std::uint64_t blockStart_{};
    std::uint64_t nextTick_{};
    std::uint64_t remainder_{};
};

}  // namespace MoTool::uZX
