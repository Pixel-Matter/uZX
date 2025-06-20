#include <JuceHeader.h>
#include "TemperamentSystem.h"
#include "Ratios.h"
#include "Scales.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include <limits>

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
    auto octave = static_cast<int>(std::floor(midiNote / 12.0 - 1.0)); // MIDI note 0 is C-1, so octave starts at -1
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
    // Estimate the octave using A4 as reference (similar to equal temperament approach)
    double a4Freq = getA4Frequency();
    double octaveEstimate = 4.0 + std::log2(frequency / a4Freq);
    
    // Search within a reasonable range around the estimated octave
    int minOctave = std::max(-1, static_cast<int>(std::floor(octaveEstimate)) - 1);
    int maxOctave = std::min(10, static_cast<int>(std::floor(octaveEstimate)) + 2);
    
    double bestMidiNote = 0.0;
    double minDifference = std::numeric_limits<double>::max();
    
    // Search through octaves and pitch classes
    for (int octave = minOctave; octave <= maxOctave; ++octave) {
        for (int pitchClass = 0; pitchClass < 12; ++pitchClass) {
            int midiNote = (octave + 1) * 12 + pitchClass;
            if (midiNote >= 0 && midiNote <= 127) {
                double calculatedFreq = midiNoteToFrequency(static_cast<double>(midiNote));
                double difference = std::abs(calculatedFreq - frequency);
                
                if (difference < minDifference) {
                    minDifference = difference;
                    bestMidiNote = static_cast<double>(midiNote);
                }
            }
        }
    }
    
    // For better accuracy, check fractional notes around the best integer match
    int baseMidi = static_cast<int>(bestMidiNote);
    double bestFractionalMidi = bestMidiNote;
    minDifference = std::numeric_limits<double>::max();
    
    // Check fractional notes in a small range around the best integer match
    for (double fractionalOffset = -0.5; fractionalOffset <= 0.5; fractionalOffset += 0.01) {
        double testMidi = baseMidi + fractionalOffset;
        if (testMidi >= 0.0 && testMidi <= 127.0) {
            double calculatedFreq = midiNoteToFrequency(testMidi);
            double difference = std::abs(calculatedFreq - frequency);
            
            if (difference < minDifference) {
                minDifference = difference;
                bestFractionalMidi = testMidi;
            }
        }
    }
    
    return bestFractionalMidi;
}

bool RationalTuning::isDefined(int /*midiNote*/) const {
    return true;
}

double RationalTuning::getTonicFrequency(int octave) const {
    auto semitonesFromTonicToA = static_cast<int>(tonic) - static_cast<int>(Scale::Key::A);
    auto ratio = ratios.at(size_t (std::abs(semitonesFromTonicToA)));
    DBG("Tonic " << static_cast<int>(tonic) << " frequency for octave " << octave
        << ": semitones from tonic " << static_cast<int>(tonic) << " to A = " << semitonesFromTonicToA
    );
    if (semitonesFromTonicToA < 0) {
        ratio.invert();
        DBG("Inverted ratio for negative semitones: " << String(ratio));
    }
    DBG("Ratio = " << String(ratio));
    return getA4Frequency() * ratio * std::pow(2.0, octave - 4);
}

}