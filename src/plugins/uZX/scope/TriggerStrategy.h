#pragma once

#include "../../../util/enumchoice.h"
#include <functional>

namespace MoTool::uZX {

/**
 * Trigger modes for oscilloscope display.
 */

struct TriggerModeEnum {
    enum Enum : uint8_t {
        FreeRunning,
        RisingEdge,
        FallingEdge
    };

    static inline constexpr std::string_view labels[] {
        "Free Running",
        "Rising Edge",
        "Falling Edge"
    };
};

using TriggerMode = Util::EnumChoice<TriggerModeEnum>;

/**
 * Trigger strategy for oscilloscope.
 *
 * Pluggable functor interface allows extending with custom trigger modes
 * (e.g., tone-sync from AYPlugin in future).
 */
struct TriggerStrategy {
    using FindTriggerFn = std::function<int(const float* samples, int numSamples, float threshold)>;

    TriggerMode mode;
    FindTriggerFn findTrigger;

    /**
     * Find trigger point in sample buffer.
     * @return Index where display should start, or 0 if no trigger found.
     */
    int operator()(const float* samples, int numSamples, float threshold) const {
        if (findTrigger)
            return findTrigger(samples, numSamples, threshold);
        return 0;
    }

    // Factory methods for standard trigger modes

    static TriggerStrategy freeRunning() {
        return {
            TriggerMode::FreeRunning,
            [](const float*, int, float) { return 0; }
        };
    }

    static TriggerStrategy risingEdge() {
        return {
            TriggerMode::RisingEdge,
            [](const float* samples, int numSamples, float threshold) -> int {
                // Need at least 2 samples to detect edge
                if (numSamples < 2)
                    return 0;

                for (int i = 1; i < numSamples; ++i) {
                    if (samples[i - 1] <= threshold && samples[i] > threshold)
                        return i;
                }
                return 0;  // No trigger found
            }
        };
    }

    static TriggerStrategy fallingEdge() {
        return {
            TriggerMode::FallingEdge,
            [](const float* samples, int numSamples, float threshold) -> int {
                if (numSamples < 2)
                    return 0;

                for (int i = 1; i < numSamples; ++i) {
                    if (samples[i - 1] >= threshold && samples[i] < threshold)
                        return i;
                }
                return 0;
            }
        };
    }

    /**
     * Create trigger strategy from mode enum.
     */
    static TriggerStrategy fromMode(TriggerMode mode) {
        switch (mode.asEnum()) {
            case TriggerModeEnum::RisingEdge:
                return risingEdge();
            case TriggerModeEnum::FallingEdge:
                return fallingEdge();
            case TriggerModeEnum::FreeRunning:
            default:
                return freeRunning();
        }
    }
};

}  // namespace MoTool::uZX


namespace juce {

using namespace MoTool::uZX;
using namespace MoTool::Util;

template <>
struct VariantConverter<TriggerMode> : public EnumVariantConverter<TriggerMode> {};

}