#include <JuceHeader.h>
#include "TemperamentSystem.h"
#include "Scales.h"
#include "juce_core/system/juce_PlatformDefs.h"

namespace MoTool {

String TemperamentSystem::getName() const {
    return String(String::formatted("%s tuning, A4 = %.2f Hz", getType().getLongLabel().data(), getA4Frequency()));
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


// RationalTuning class implementation

RationalTuning::RationalTuning(
    const std::array<FractionNumber, 12>& rationalIntervals,
    const Scale::Key keyToUse,
    const Scale* scaleToUse,  // TODO remove?
    double a4Frequency
)
    : TemperamentSystem(a4Frequency)
    , ratios(rationalIntervals)
    , tonic(keyToUse)
    , scale(scaleToUse)
{
    jassert(scaleToUse != nullptr && "Scale must not be null for RationalTuning");
    jassert(ratios.size() == 12 && "RationalTuning must have exactly 12 intervals for the 12 semitones");
}

TemperamentType RationalTuning::getType() const {
    return TemperamentType::CustomRational;
}

double RationalTuning::midiNoteToFrequency(double midiNote) const {
    // Implement custom rational tuning logic here
    auto octave = static_cast<int>(midiNote / 12 - 1); // MIDI note 0 is C-1, so octave starts at -1
    auto pitchClass = static_cast<int>(midiNote) % 12; // MIDI pitch class is the note within the octave (0-11)
    auto noteIdx = pitchClass - static_cast<int>(tonic);
    /*
    For example the key is C, note is A
    C  C# D  D# E  F  F# G  G# A  A# B
    K          --(9)--         N

    the key is A, note is C
    C  C# D  D# E  F  F# G  G# A  A# B
    N         --(-9)--         K
    */

    DBG("---\n"
        << "MIDI note " << midiNote
        << ": octave " << octave << ", pitch class " << pitchClass
        << ", note index " << noteIdx
    );

    if (noteIdx < 0) {
        --octave;
        noteIdx += 12;
        DBG("noteIdx < 0 "
            << ": octave " << octave
            << ", note index " << noteIdx
        );
    }
    auto ratio = ratios.at(size_t(noteIdx % 12));
    auto tonicFrequency = getTonicFrequency(octave);
    DBG("Ratio " << String(ratio)
        << ", tonic frequency " << tonicFrequency
        << ", result " << String(tonicFrequency * static_cast<double>(ratio))
    );

    return tonicFrequency * static_cast<double>(ratio);
}

double RationalTuning::frequencyToMidiNote(double frequency) const {
    // Implement custom rational tuning logic here
    return 0.0; // Placeholder
}

bool RationalTuning::isDefined(int /*midiNote*/) const {
    return true;
}

double RationalTuning::getTonicFrequency(int octave) const {
    auto semitonesFromTonicToA = static_cast<int>(tonic) - static_cast<int>(Scale::Key::A);
    auto ratio = ratios.at(size_t (semitonesFromTonicToA + 12) % 12);
    if (semitonesFromTonicToA < 0) {
        semitonesFromTonicToA += 12; // Wrap around if negative
        --octave;
    }
    DBG("Tonic " << static_cast<int>(tonic) << " frequency for octave " << octave
        << ": semitones from tonic " << static_cast<int>(tonic) << " to A = " << semitonesFromTonicToA
        << ", ratio = " << String(ratio)
    );
    return getA4Frequency() * ratio * std::pow(2.0, octave - 4);
}

}