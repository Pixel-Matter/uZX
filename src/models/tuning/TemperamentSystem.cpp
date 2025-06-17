#include <JuceHeader.h>
#include "TemperamentSystem.h"

namespace MoTool {

String TemperamentSystem::getName() const {
    return String(String::formatted("%s tuning, A4 = %.2f Hz", getType().getLongLabel(), getA4Frequency()));
}

void TemperamentSystem::setA4Frequency(double frequency) {
    a4Freq = frequency;
}

double TemperamentSystem::getA4Frequency() const {
    return a4Freq;
}

TemperamentType EqualTemperamentTuning::getType() const {
    return TemperamentType::EqualTemperament;
}

double EqualTemperamentTuning::midiNoteToFrequency(double midiNote) const {
    return getA4Frequency() * std::pow(2.0, (midiNote - 69) / 12.0);
}

double EqualTemperamentTuning::frequencyToMidiNote(double frequency) const {
    return 69 + 12 * std::log2(frequency / getA4Frequency());
}

bool EqualTemperamentTuning::isDefined(int /*midiNote*/) const {
    // Equal temperament is defined for all MIDI notes
    return true;
}

TemperamentType RationalTuning::getType() const {
    return TemperamentType::CustomRational;
}

double RationalTuning::midiNoteToFrequency(double midiNote) const {
    // Implement custom rational tuning logic here
    return 0.0; // Placeholder
}

double RationalTuning::frequencyToMidiNote(double frequency) const {
    // Implement custom rational tuning logic here
    return 0.0; // Placeholder
}

bool RationalTuning::isDefined(int midiNote) const {
    // Implement custom rational tuning logic here
    return false; // Placeholder
}

}