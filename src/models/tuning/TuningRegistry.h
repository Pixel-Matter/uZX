#pragma once

#include "TuningSystem.h"
#include <memory>

namespace MoTool {

enum class CustomTuningTable {
    CustomPT3_0_PT = 0,         // ProTracker #0 (Original PT3 table)
    CustomPT3_1_ST = 1,         // ProTracker #1 (SoundTracker)
    CustomPT3_2_ASM = 2,        // ProTracker #2 (ASM)
    CustomPT3_3_REAL = 3,       // ProTracker #3 (REAL)
    CustomVT_4_NATURAL = 4,     // ProTracker #4 (Natural Cmaj/Am)
    CustomNaturalEPhrygian = 5  // Natural E Phrygian
};

// Factory functions for standard tuning systems
inline std::unique_ptr<TuningSystem> makeEqualTemperamentTuning(const ChipCapabilities& capabilities, double A4Frequency = 440.0) {
    return std::make_unique<EqualTemperamentTuning>(capabilities, A4Frequency);
}

// Factory functions for ProTracker-style custom table tunings
std::unique_ptr<CustomTuning> makeCustomTableTuning(CustomTuningTable tableType, const ChipCapabilities& capabilities);

// Get descriptive name for tuning table
const char* getTuningTableName(CustomTuningTable tableType);

}