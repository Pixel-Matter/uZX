#include "AutoTuning.h"
#include "juce_core/system/juce_PlatformDefs.h"

namespace MoTool {

String AutoTuning::getName() const {
    jassert(referenceTuning != nullptr);
    return referenceTuning->getName() + String::formatted(" auto tuning, chip clock = %.3f MHz, A4 = %.2f Hz", clockFrequency / 1000000.0, getA4Frequency());
}

TuningType AutoTuning::getType() const {
    return TuningType::AutoTuning;
}

int AutoTuning::midiNoteToPeriod(double midiNote) const {
    return frequencyToPeriod(getReferenceFrequency(midiNote));
}

double AutoTuning::periodToMidiNote(int period) const {
    jassert(referenceTuning != nullptr);
    return referenceTuning->frequencyToMidiNote(periodToFrequency(period));
}

bool AutoTuning::isDefined(int midiNote) const {
    jassert(referenceTuning != nullptr);
    return referenceTuning->isDefined(midiNote);
}

}