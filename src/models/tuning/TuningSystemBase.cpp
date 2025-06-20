#include "TuningSystemBase.h"

namespace MoTool {

double TuningSystem::periodToFrequency(int period) const {
    if (period <= 0) return 0.0;
    return clockFrequency / chip.divider / period;
}

int TuningSystem::frequencyToPeriod(double frequency) const {
    if (frequency <= 0.0) return chip.registerRange.getEnd() - 1;
    return jlimit(
        chip.registerRange.getStart(),
        chip.registerRange.getEnd() - 1,
        static_cast<int>(std::round(clockFrequency / chip.divider / frequency))
    );
}

double TuningSystem::midiNoteToFrequency(double midiNote) const {
    int period = midiNoteToPeriod(midiNote);
    return periodToFrequency(period);
}

double TuningSystem::frequencyToMidiNote(double frequency) const {
    int period = frequencyToPeriod(frequency);
    return periodToMidiNote(period);
}

double TuningSystem::getOfftune(double midiNote) const {
    // Calculate the difference between the custom tuning and equal temperament
    double freq = midiNoteToFrequency(midiNote);
    double refFreq = getReferenceFrequency(midiNote);

    // Return difference in cents
    return 1200.0 * std::log2(freq / refFreq);
}

void TuningSystem::setA4Frequency(double frequency) {
    jassert(referenceTuning != nullptr);
    referenceTuning->setA4Frequency(frequency);
}

double TuningSystem::getA4Frequency() const {
    jassert(referenceTuning != nullptr);
    return referenceTuning->getA4Frequency();
}

void TuningSystem::setClockFrequency(double frequency) {
    clockFrequency = frequency;
}

double TuningSystem::getClockFrequency() const {
    return clockFrequency;
}

double TuningSystem::getReferenceFrequency(double midiNote) const {
    jassert(referenceTuning != nullptr);
    return referenceTuning->midiNoteToFrequency(midiNote);
}

double TuningSystem::getReferenceFrequency(int midiNote) const {
    jassert(referenceTuning != nullptr);
    return referenceTuning->midiNoteToFrequency(midiNote);
}

}