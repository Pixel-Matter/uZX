#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace MoTool::uZX {

/**
 * Lock-free ring buffer for audio→UI communication.
 *
 * Audio thread pushes samples via pushSamples() (non-blocking write).
 * UI thread reads snapshots via copyTo() (non-blocking read).
 *
 * Readers skip a snapshot if a publication overlaps the copy. Competing producers
 * (such as during a graph handoff) skip a block if another producer is publishing.
 */
class ScopeBuffer
{
public:
    static constexpr int kBufferSize = 8192;  // ~185ms at 44.1kHz

    inline constexpr static int size() noexcept { return kBufferSize; }

    /**
     * Push samples from audio thread (non-blocking).
     * May overwrite unread samples - this is acceptable for visualization.
     * `sync` and `amplitude`, if given, are a chip channel's trigger and the highest level its volume reaches;
     * they share each sample's slot, so they never drift apart from it.
     */
    void pushSamples(const float* samples,
                     int numSamples,
                     const float* sync = nullptr,
                     const float* amplitude = nullptr) noexcept {
        if (samples == nullptr || numSamples <= 0)
            return;

        auto revision = revision_.load(std::memory_order_relaxed);
        if ((revision & 1) != 0
            || !revision_.compare_exchange_strong(
                revision, revision + 1, std::memory_order_acquire, std::memory_order_relaxed))
            return;

        // Pair with the reader's acquire fence so any in-flight store also exposes the busy revision.
        std::atomic_thread_fence(std::memory_order_release);

        const bool cleared = clearRequested_.exchange(false, std::memory_order_relaxed);
        const int available = cleared ? 0 : available_.load(std::memory_order_relaxed);
        const int writePos = writeIndex_.load(std::memory_order_relaxed);

        const std::array<const float*, kNumLanes> sources{samples, sync, amplitude};
        for (size_t lane = 0; lane < kNumLanes; ++lane) {
            const auto* source = sources[lane];
            for (int i = 0; i < numSamples; ++i)
                lanes_[lane][ringIndex(writePos, i)].store(source != nullptr ? source[i] : 0.0f,
                                                           std::memory_order_relaxed);
        }

        writeIndex_.store(static_cast<int>(ringIndex(writePos, numSamples)), std::memory_order_relaxed);
        available_.store(available + std::min(numSamples, kBufferSize - available), std::memory_order_relaxed);
        revision_.store(revision + 2, std::memory_order_release);
    }

    /**
     * Copy most recent samples to destination buffer (UI thread).
     * Returns the number of samples copied, with leading zeros if history is short.
     * A return of zero means the destination must be ignored.
     *
     * @param dest Destination buffer (must have capacity for numSamples)
     * @param numSamples Number of samples to copy (will be clamped to buffer size)
     * @param syncDest If given, takes the sync of the same samples
     * @param amplitudeDest If given, takes the amplitude of the same samples
     */
    int copyTo(float* dest, int numSamples, float* syncDest = nullptr, float* amplitudeDest = nullptr) const noexcept {
        if (dest == nullptr || numSamples <= 0)
            return 0;

        numSamples = std::min(numSamples, kBufferSize);

        const auto revision = revision_.load(std::memory_order_acquire);
        if ((revision & 1) != 0)
            return 0;

        const int writePos = writeIndex_.load(std::memory_order_relaxed);
        const int available = clearRequested_.load(std::memory_order_relaxed)
                                  ? 0
                                  : std::min(available_.load(std::memory_order_relaxed), numSamples);
        const int leadingZeros = numSamples - available;
        const int startPos = writePos - available + kBufferSize;

        const std::array<float*, kNumLanes> dests{dest, syncDest, amplitudeDest};
        for (size_t lane = 0; lane < kNumLanes; ++lane) {
            auto* output = dests[lane];
            if (output == nullptr)
                continue;

            std::fill_n(output, leadingZeros, 0.0f);
            for (int i = 0; i < available; ++i)
                output[leadingZeros + i] = lanes_[lane][ringIndex(startPos, i)].load(std::memory_order_relaxed);
        }

        std::atomic_thread_fence(std::memory_order_acquire);
        return revision_.load(std::memory_order_relaxed) == revision ? numSamples : 0;
    }

    /**
     * Discard history without writing sample slots from the caller's thread.
     * Readers see silence until a producer starts publishing fresh samples.
     */
    void requestClear() noexcept { clearRequested_.store(true, std::memory_order_relaxed); }

private:
    static constexpr size_t kNumLanes = 3;  // samples, sync, amplitude
    using Ring = std::array<std::atomic<float>, kBufferSize>;
    static_assert(std::atomic<float>::is_always_lock_free && std::atomic<std::uint64_t>::is_always_lock_free
                  && std::atomic<int>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);

    static size_t ringIndex(int start, int offset) noexcept {
        return (static_cast<size_t>(start) + static_cast<size_t>(offset)) % kBufferSize;
    }

    std::array<Ring, kNumLanes> lanes_{};
    std::atomic<std::uint64_t> revision_{0};  // odd while the producer owns the ring
    std::atomic<int> writeIndex_{0};
    std::atomic<int> available_{0};
    std::atomic<bool> clearRequested_{false};
};

}  // namespace MoTool::uZX
