#include "AutoTuning.h"

namespace MoTool {

String AutoTuning::getDescription() const {
    jassert(referenceTuning != nullptr);
    return referenceTuning->getTypeName() + String::formatted(" auto tuning, chip clock = %.3fMHz, A4 = %.2fHz", clockFrequency / 1000000.0, getA4Frequency());
}

TuningType AutoTuning::getType() const {
    return TuningType::AutoTuning;
}

int AutoTuning::midiNoteToPeriod(double midiNote, PeriodMode mode) const {
    return frequencyToPeriod(getReferenceFrequency(midiNote), mode);
}

double AutoTuning::periodToMidiNote(int period, PeriodMode mode) const {
    jassert(referenceTuning != nullptr);
    return referenceTuning->frequencyToMidiNote(periodToFrequency(period, mode));
}

bool AutoTuning::isDefined(int midiNote) const {
    jassert(referenceTuning != nullptr);
    return referenceTuning->isDefined(midiNote);
}

}