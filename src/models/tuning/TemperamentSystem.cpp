#include <JuceHeader.h>
#include "TemperamentSystem.h"
#include "Ratios.h"
#include "Scales.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include <cmath>
#include <limits>

namespace MoTool {

String TemperamentSystem::getDescription() const {
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

double EqualTemperamentTuning::midiNoteToFrequency(int midiNote) const {
    return midiNoteToFrequency(static_cast<double>(midiNote));
}

double EqualTemperamentTuning::midiNoteToFrequency(double midiNote) const {
    return getA4Frequency() * std::pow(2.0, (midiNote - 69) / 12.0);
}

double EqualTemperamentTuning::frequencyToMidiNote(double frequency) const {
    return 69 + 12 * std::log2(frequency / getA4Frequency());
}

int EqualTemperamentTuning::frequencyToNearestMidiNote(double frequency, NoteSearch search) const {
    double midiNote = frequencyToMidiNote(frequency);
    switch (search) {
        case NoteSearch::Nearest: {
            return static_cast<int>(std::round(midiNote));
        }
        case NoteSearch::NextLower: {
            return static_cast<int>(std::floor(midiNote));
        }
        case NoteSearch::NextHigher: {
            return static_cast<int>(std::ceil(midiNote));
        }
    }
}

bool EqualTemperamentTuning::isDefined(int /*midiNote*/) const {
    // Equal temperament is defined for all MIDI notes
    return true;
}

void EqualTemperamentTuning::setTonic(Scale::Key) {
    // not required for equal temperament
}

Scale::Key EqualTemperamentTuning::getTonic() const {
    return Scale::Key::C; // Default to C for equal temperament
}

String EqualTemperamentTuning::getDegreeRepresentation(int degree) const {
    return String(degree * 100);  // just in cents
}


// RationalTuning class implementation

RationalTuning::RationalTuning(
    const std::array<FractionNumber, 12>& rationalIntervals,
    const Scale::Key keyToUse,
    // const Scale* scaleToUse,  // TODO remove?
    double a4Frequency
)
    : TemperamentSystem(a4Frequency)
    , ratios(rationalIntervals)
    , tonic(keyToUse)
    // , scale(scaleToUse)
{
    // jassert(scaleToUse != nullptr && "Scale must not be null for RationalTuning");
    jassert(ratios.size() == 12 && "RationalTuning must have exactly 12 intervals for the 12 semitones");
}

TemperamentType RationalTuning::getType() const {
    return TemperamentType::CustomRational;
}

double RationalTuning::midiNoteToFrequency(int midiNote) const {
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

    // DBG("---\n"
    //     << "MIDI note " << midiNote
    //     << ": octave " << octave << ", pitch class " << pitchClass
    //     << ", note index " << noteIdx
    // );

    if (noteIdx < 0) {
        --octave;
        noteIdx += 12;
        // DBG("noteIdx < 0 "
        //     << ": octave " << octave
        //     << ", note index " << noteIdx
        // );
    }
    auto ratio = ratios.at(size_t(noteIdx % 12));
    auto tonicFrequency = getTonicFrequency(octave);
    // DBG("Ratio " << String(ratio)
    //     << ", tonic frequency " << tonicFrequency
    //     << ", result " << String(tonicFrequency * static_cast<double>(ratio))
    // );

    return tonicFrequency * static_cast<double>(ratio);
}

double RationalTuning::midiNoteToFrequency(double midiNote) const {
    int intNote = static_cast<int>(std::floor(midiNote));
    auto freq1 = midiNoteToFrequency(intNote);
    auto freq2 = midiNoteToFrequency(intNote + 1);
    double fractionalPart = midiNote - intNote;
    double interpolatedFreq = freq1 * std::pow(freq2 / freq1, fractionalPart);
    // DBG("Interpolated frequency for MIDI note " << midiNote
    //     << ": freq1 = " << freq1
    //     << ", freq2 = " << freq2
    //     << ", fractional part = " << fractionalPart
    //     << ", result = " << interpolatedFreq
    // );
    return interpolatedFreq;
}

int RationalTuning::frequencyToNearestMidiNote(double frequency, NoteSearch search) const {
    constexpr int searchRange = 6; // +/- 6 semitones around note estimate by 12TET
    double a4Freq = getA4Frequency();
    int noteEstimate = static_cast<int>(std::round(69 + 12 * std::log2(frequency / a4Freq)));

    // Search for the closest MIDI notes around the estimate
    int left = std::max(0, noteEstimate - searchRange);
    int right = std::min(127, noteEstimate + searchRange);

    int closestNote = -1;
    double smallestDifference = std::numeric_limits<double>::max();
    double closestFrequency = -1.0;

    // Find the note with the smallest frequency difference
    for (int note = left; note <= right; ++note) {
        double noteFreq = midiNoteToFrequency(note);
        double difference = std::abs(noteFreq - frequency);

        if (difference < smallestDifference) {
            smallestDifference = difference;
            closestNote = note;
            closestFrequency = noteFreq;
        }
    }

    // Handle different search modes
    switch (search) {
        case NoteSearch::Nearest:
            return closestNote;

        case NoteSearch::NextLower: {
            if (closestFrequency <= frequency) {
                return closestNote; // Already the closest lower note
            }
            return closestNote - 1; // Return the next lower note if the closest is higher than frequency
        }

        case NoteSearch::NextHigher: {
            if (closestFrequency >= frequency) {
                return closestNote; // Already the closest higher note
            }
            return closestNote + 1; // Return the next higher note if the closest is lower than frequency
        }
    }

    return closestNote;
}

double RationalTuning::frequencyToMidiNote(double frequency) const {
    // Estimate the octave using A4 as reference (similar to equal temperament approach)
    auto note1 = frequencyToNearestMidiNote(frequency, NoteSearch::NextLower);
    auto note2 = note1 + 1;
    auto freq1 = midiNoteToFrequency(note1);
    auto freq2 = midiNoteToFrequency(note2);
    // use log to interpolate between two frequencies
    double fractionalPart = std::log(frequency / freq1) / std::log(freq2 / freq1);
    double interpolatedNote = note1 + fractionalPart;
    // DBG("Interpolated MIDI note for frequency " << frequency
    //     << "\n: note1 = " << note1
    //     << "\n, note2 = " << note2
    //     << "\n, freq1 = " << freq1
    //     << "\n, freq2 = " << freq2
    //     << "\n, result = " << interpolatedNote
    // );
    return interpolatedNote;
}

bool RationalTuning::isDefined(int /*midiNote*/) const {
    return true;
}

double RationalTuning::getTonicFrequency(int octave) const {
    auto semitonesFromTonicToA = static_cast<int>(tonic) - static_cast<int>(Scale::Key::A);
    auto ratio = ratios.at(size_t (std::abs(semitonesFromTonicToA)));
    // DBG("Tonic " << static_cast<int>(tonic) << " frequency for octave " << octave
    //     << ": semitones from tonic " << static_cast<int>(tonic) << " to A = " << semitonesFromTonicToA
    // );
    if (semitonesFromTonicToA < 0) {
        ratio.invert();
        // DBG("Inverted ratio for negative semitones: " << String(ratio));
    }
    // DBG("Ratio = " << String(ratio));
    return getA4Frequency() * ratio * std::pow(2.0, octave - 4);
}

String RationalTuning::getDegreeRepresentation(int degree) const {
    const auto& fraction = ratios.at(size_t(degree % 12));
    return String::formatted("%d:%d", fraction.getDenominator(), fraction.getNumerator());
}


// Just Intonation 5-limit tuning
JustIntonation5Limit::JustIntonation5Limit(const Scale::Key tonicToUse, double a4Frequency)
    : RationalTuning({
        FractionNumber(1, 1),   // Unison
        FractionNumber(16, 15), // Minor second
        FractionNumber(9, 8),   // Major second
        FractionNumber(6, 5),   // Minor third
        FractionNumber(5, 4),   // Major third
        FractionNumber(4, 3),   // Perfect fourth
        FractionNumber(45, 32), // Augmented fourth / diminished fifth
        FractionNumber(3, 2),   // Perfect fifth
        FractionNumber(8, 5),   // Minor sixth
        FractionNumber(5, 3),   // Major sixth
        FractionNumber(16, 9),  // Minor seventh
        FractionNumber(15, 8)   // Major seventh
    }, tonicToUse, a4Frequency)
{}

TemperamentType JustIntonation5Limit::getType() const {
    return TemperamentType::Just5Limit;
}


// standalone functions
std::unique_ptr<TemperamentSystem> makeTemperamentSystem(
    TemperamentType type,
    const Scale::Key tonic,
    double a4Frequency
) {
    switch (type) {
        case TemperamentTypeEnum::EqualTemperament:
            return std::make_unique<EqualTemperamentTuning>(a4Frequency);

        case TemperamentTypeEnum::Just5Limit:
            return std::make_unique<JustIntonation5Limit>(tonic, a4Frequency);

        // Add other temperament types here as needed
        default:
            jassertfalse; // Unsupported temperament type
            return nullptr;
    }
}

}