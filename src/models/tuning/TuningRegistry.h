#pragma once

#include "TemperamentSystem.h"
#include "TuningSystemBase.h"
#include "AutoTuning.h"
#include "TuningTable.h"
#include <memory>

namespace MoTool {

struct CustomTuningEnum {
    enum Enum : size_t {
        CustomPT_0_PT = 0,          // ProTracker #0 (Original PT3 table)
        CustomPT_1_ST = 1,          // ProTracker #1 (SoundTracker)
        CustomPT_2_ASM = 2,         // ProTracker #2 (ASM)
        CustomPT_3_REAL = 3,        // ProTracker #3 (REAL)
        CustomVT_4_NATURAL = 4,     // ProTracker #4 (Natural Cmaj/Am)
        // CustomNaturalEPhrygian = 5  // Natural E Phrygian
    };

    static inline constexpr std::string_view longLabels[] {
        "ProTracker #0",
        "ProTracker #1 (ST)",
        "ProTracker #2 (ASM)",
        "ProTracker #3 (REAL)",
        "IvanRochin #4 (NATURAL Cmaj/Am)",
        // "Natural E Phrygian"
    };
};

using CustomTuningType = MoTool::Util::EnumChoice<CustomTuningEnum>;

// Factory functions for standard tuning systems
inline std::unique_ptr<TuningSystem> makeEqualTemperamentTuning(const ChipCapabilities& capabilities, double chipClock, double A4Frequency) {
    return std::make_unique<AutoTuning>(capabilities, chipClock, std::make_unique<EqualTemperamentTuning>(A4Frequency));
}

// Factory functions for ProTracker-style custom table tunings
std::unique_ptr<TuningTable> makeCustomTableTuning(CustomTuningType tableType, const ChipCapabilities& capabilities);

// Get descriptive name for tuning table
const std::string_view getTuningTableName(CustomTuningType tableType);

}