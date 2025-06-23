#pragma once

#include "TemperamentSystem.h"
#include "TuningSystemBase.h"
#include "AutoTuning.h"
#include "TuningTable.h"
#include <memory>

namespace MoTool {

struct BuiltinTuningEnum {
    enum Enum : size_t {
        EqualTemperament = 0, // Standard equal temperament tuning
        Just5Limit,       // Just Intonation (5-limit)
        CustomPT_0_PT,        // ProTracker #0 (Original PT3 table)
        CustomPT_1_ST,        // ProTracker #1 (SoundTracker)
        CustomPT_2_ASM,       // ProTracker #2 (ASM)
        CustomPT_3_REAL,      // ProTracker #3 (REAL)
        CustomVT_4_NATURAL,   // ProTracker #4 (Natural Cmaj/Am)
        // CustomNaturalEPhrygian  // Natural E Phrygian
    };

    static inline constexpr std::string_view longLabels[] {
        "Equal Temperament",
        "Just Intonation (5-limit)",
        "ProTracker #0",
        "ProTracker #1 (ST)",
        "ProTracker #2 (ASM)",
        "ProTracker #3 (REAL)",
        "IvanRochin #4 (NATURAL Cmaj/Am)",
        // "Natural E Phrygian"
    };
};

using BuiltinTuningType = MoTool::Util::EnumChoice<BuiltinTuningEnum>;

// Factory functions for standard tuning systems
inline std::unique_ptr<TuningSystem> makeEqualTemperamentTuning(const ChipCapabilities& capabilities, double chipClock, double A4Frequency) {
    return std::make_unique<AutoTuning>(capabilities, chipClock, std::make_unique<EqualTemperamentTuning>(A4Frequency));
}

// Factory functions for ProTracker-style custom table tunings
std::unique_ptr<TuningSystem> makeBuiltinTuning(
    BuiltinTuningType tableType,
    const ChipCapabilities& capabilities,
    double chipClock,
    double A4Frequency = 440.0
);

// Get descriptive name for tuning table
const std::string_view getTuningTableName(BuiltinTuningType tableType);

}