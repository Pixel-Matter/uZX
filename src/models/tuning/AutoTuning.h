#pragma once

#include "TuningSystemBase.h"

namespace MoTool {

// Auto tuning uses reference tuning to calculate frequencies and converts them to periods
class AutoTuning final : public TuningSystem {
public:
    using TuningSystem::TuningSystem;

    String getDescription() const override;
    TuningType getType() const override;
    int midiNoteToPeriod(double midiNote, PeriodMode mode = PeriodMode::Tone) const override;
    double periodToMidiNote(int period, PeriodMode mode = PeriodMode::Tone) const override;
    bool isDefined(int midiNote) const override;
};

}