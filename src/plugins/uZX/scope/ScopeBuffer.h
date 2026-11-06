#pragma once

#include <array>
#include <atomic>
#include <algorithm>
#include <cstring>

namespace MoTool::uZX {

/**
 * Lock-free ring buffer for audio→UI communication.
 *
 * Audio thread pushes samples via pushSamples() (non-blocking write).
 * UI thread reads snapshots via copyTo() (non-blocking read).
 *
 * Uses relaxed atomics for write index since we only need eventual consistency
 * for visualization purposes - dropped frames are acceptable.
 */
class ScopeBuffer {
public:
    static constexpr int kBufferSize = 8192;  // ~185ms at 44.1kHz

    ScopeBuffer() {
        buffer_.fill(0.0f);
        sync_.fill(0.0f);
    }

    inline constexpr static int size() noexcept {
        return kBufferSize;
    }

    /**
     * Push samples from audio thread (non-blocking).
     * May overwrite unread samples - this is acceptable for visualization.
     * `sync`, if given, is a trigger signal that shares each sample's slot, so the two never drift apart.
     */
    void pushSamples(const float* samples, int numSamples, const float* sync = nullptr) noexcept {
        if (samples == nullptr || numSamples <= 0)
            return;

        int writePos = writeIndex_.load(std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i) {
            buffer_[writePos] = samples[i];
            sync_[writePos] = sync != nullptr ? sync[i] : 0.0f;
            writePos = (writePos + 1) % kBufferSize;
        }

        writeIndex_.store(writePos, std::memory_order_release);
    }

    /**
     * Copy most recent samples to destination buffer (UI thread).
     * Returns the number of samples actually copied.
     *
     * @param dest Destination buffer (must have capacity for numSamples)
     * @param numSamples Number of samples to copy (will be clamped to buffer size)
     * @param syncDest If given, takes the sync signal of the same samples
     */
    int copyTo(float* dest, int numSamples, float* syncDest = nullptr) const noexcept {
        if (dest == nullptr || numSamples <= 0)
            return 0;

        numSamples = std::min(numSamples, kBufferSize);

        // Read write index with acquire to synchronize with audio thread
        int writePos = writeIndex_.load(std::memory_order_acquire);

        // Calculate start position (numSamples before current write position)
        int startPos = (writePos - numSamples + kBufferSize) % kBufferSize;

        copyRing(buffer_, dest, startPos, numSamples);
        if (syncDest != nullptr)
            copyRing(sync_, syncDest, startPos, numSamples);

        return numSamples;
    }

    /**
     * Clear the buffer (call from UI thread when resetting display).
     */
    void clear() noexcept {
        buffer_.fill(0.0f);
        sync_.fill(0.0f);
        writeIndex_.store(0, std::memory_order_release);
    }

private:
    using Ring = std::array<float, kBufferSize>;

    static void copyRing(const Ring& ring, float* dest, int startPos, int numSamples) noexcept {
        // Copy samples, handling wrap-around
        if (startPos + numSamples <= kBufferSize) {
            // No wrap-around needed
            std::memcpy(dest, ring.data() + startPos, numSamples * sizeof(float));
        } else {
            // Wrap-around: copy in two parts
            int firstPart = kBufferSize - startPos;
            std::memcpy(dest, ring.data() + startPos, firstPart * sizeof(float));
            std::memcpy(dest + firstPart, ring.data(), (numSamples - firstPart) * sizeof(float));
        }
    }

    Ring buffer_;
    Ring sync_;
    std::atomic<int> writeIndex_{0};
};

}  // namespace MoTool::uZX
