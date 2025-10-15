#pragma once

#include "TemperamentSystem.h"
#include "TuningSystemBase.h"
#include "AutoTuning.h"
#include "TuningTable.h"

#include "../../plugins/uZX/aychip/aychip.h"

#include <memory>

namespace MoTool {


struct BuiltinTuningEnum {
    enum Enum : size_t {
        EqualTemperament = 0, // Standard equal temperament tuning
        Just5Limit,           // Just Intonation (5-limit)
        Just5Limit2,          // Just Intonation (5-limit T=45:64)
        Pythagorean,          // Pythagorean tuning
        CustomPT_0_PT,        // ProTracker #0 (Original PT3 table)
        CustomPT_1_ST,        // ProTracker #1 (SoundTracker)
        CustomPT_2_ASM,       // ProTracker #2 (ASM)
        CustomPT_3_REAL,      // ProTracker #3 (REAL)
        CustomVT_4_NATURAL,   // ProTracker #4 (Natural Cmaj/Am)
        RoschinFixed,         // IvanRoschin Fixed (Natural Cmaj/Am)
    };

    static inline constexpr std::string_view longLabels[] {
        "Equal Temperament",
        "Just Intonation (5-limit D Phrygian)",
        "Just Intonation (5-limit T=45:64)",
        "Pythagorean",
        "ProTracker #0 (PT)",
        "ProTracker #1 (ST)",
        "ProTracker #2 (ASM)",
        "ProTracker #3 (REAL)",
        "IvanRoschin #4 (NATURAL Cmaj/Am)",
        "IvanRoschin Fixed (Natural D#maj)",
    };
};

using BuiltinTuningType = MoTool::Util::EnumChoice<BuiltinTuningEnum>;


struct TuningOptions {
    BuiltinTuningType tableType;
    TuningSystemType temperamentType;
    Scale::Tonic tonic;
    Scale::ScaleType scaleType;
    ChipClockChoice chipChoice;
    double chipClock;
    double a4Frequency;
};

// Factory functions for ProTracker-style custom table tunings
std::unique_ptr<TuningSystem> makeBuiltinTuning(TuningOptions& options);

std::unique_ptr<TuningSystem> makeBuiltinTuning(BuiltinTuningType type);

// Get descriptive name for tuning table
const std::string_view getTuningTableName(BuiltinTuningType tableType);

}

using namespace MoTool;
using namespace MoTool::Util;

template <>
struct juce::VariantConverter<BuiltinTuningType> : public EnumVariantConverter<BuiltinTuningType> {};
