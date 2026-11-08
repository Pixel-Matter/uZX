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
        CustomPT_0_PT33,      // ProTracker #0 as PT 3.3 and older play it
        CustomPT_2_ASM33,     // ProTracker #2 as PT 3.3 and older play it
    };

    static inline constexpr std::string_view shortLabels[] {
        "12TET",
        "5L JI",
        "5L JI2",
        "3L JI",
        "PT #0",
        "PT #1",
        "PT #2",
        "PT #3",
        "VT #4",
        "VT #4 Fix",
        "PT #0 (3.3)",
        "PT #2 (3.3)",
    };

    static inline constexpr std::string_view labels[] {
        "Equal Temperament",
        "Just Intonation 5L",
        "Just Intonation 5L 2",
        "Pythagorean 3L",
        "ProTracker #0",
        "ProTracker #1",
        "ProTracker #2",
        "ProTracker #3",
        "Ivan Roschin's #4",
        "Ivan Roschin's Fixed",
        "ProTracker #0 (3.3)",
        "ProTracker #2 (3.3)",
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
        "Ivan Roschin's #4 (NATURAL Cmaj/Am)",
        "Ivan Roschin's Fixed (Natural D#maj)",
        "ProTracker #0 (PT, PT 3.3 and older)",
        "ProTracker #2 (ASM, PT 3.3 and older)",
    };
};

using BuiltinTuningType = MoTool::Util::EnumChoice<BuiltinTuningEnum>;


struct TuningOptions {
    BuiltinTuningType builtinTable;
    TuningSystemType tuningSystemType;
    Scale::Tonic tonic;
    Scale::ScaleType scaleType;
    double chipClockFreq;
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
