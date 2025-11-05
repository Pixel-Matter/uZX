#include <JuceHeader.h>
#include "TemperamentSystem.h"
#include "Ratios.h"
#include "Scales.h"
#include <cmath>
#include <limits>

namespace MoTool {

// ReferenceTuningSystem base class implementation
ReferenceTuningSystem::ReferenceTuningSystem(double a4Freq)
    : state(IDs::TEMPERAMENT)
{
    a4Frequency.referTo(state, IDs::a4Frequency, nullptr, a4Freq);
}

ReferenceTuningSystem::ReferenceTuningSystem(const juce::ValueTree& initialState)
    : state(initialState)
{
    a4Frequency.referTo(state, IDs::a4Frequency, nullptr, 440.0);
}

String ReferenceTuningSystem::getDescription() const {
    return String(String::formatted("%s tuning, A4 = %.2f Hz", getType().getLongLabel().data(), getA4Frequency()));
}

void ReferenceTuningSystem::setA4Frequency(double frequency) {
    a4Frequency = frequency;
}

double ReferenceTuningSystem::getA4Frequency() const {
    return a4Frequency.get();
}

// juce::ValueTree ReferenceTuningSystem::getState() const {
//     return state;
// }

// void ReferenceTuningSystem::setState(const juce::ValueTree& newState) {
//     state = newState;
// }

// EqualTemperamentTuning implementation
EqualTemperamentTuning::EqualTemperamentTuning(double a4Freq)
    : ReferenceTuningSystem(a4Freq)
{
    state.setProperty(IDs::type, "EqualTemperament", nullptr);
}

EqualTemperamentTuning::EqualTemperamentTuning(const juce::ValueTree& initialState)
    : ReferenceTuningSystem(initialState)
{
}

TuningSystemType EqualTemperamentTuning::getType() const {
    return TuningSystemType::EqualTemperament;
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

void EqualTemperamentTuning::setTonic(Scale::Tonic) {
    // not required for equal temperament
}

Scale::Tonic EqualTemperamentTuning::getTonic() const {
    return Scale::Tonic::C; // Default to C for equal temperament
}

String EqualTemperamentTuning::getDegreeRepresentation(int degree) const {
    return String(degree * 100);  // just in cents
}


// RationalTuning class implementation

RationalTuning::RationalTuning(
    const std::array<FractionNumber, 12>& rationalIntervals,
    const Scale::Tonic keyToUse,
    const Scale::ScaleType scaleTypeToUse,
    double a4Freq
)
    : ReferenceTuningSystem(a4Freq)
    , cachedRatios(rationalIntervals)
{
    state.setProperty(IDs::type, "CustomRational", nullptr);
    tonic.referTo(state, IDs::tonic, nullptr, keyToUse);
    scaleType.referTo(state, IDs::scaleType, nullptr, scaleTypeToUse);
    ratiosString.referTo(state, IDs::ratios, nullptr, getRatiosAsString(rationalIntervals));
}

RationalTuning::RationalTuning(const juce::ValueTree& initialState)
    : ReferenceTuningSystem(initialState)
    , cachedRatios(getRatiosFromString(initialState.getProperty(IDs::ratios, "")))
{
    tonic.referTo(state, IDs::tonic, nullptr, Scale::Tonic::C);
    scaleType.referTo(state, IDs::scaleType, nullptr, Scale::ScaleType::IonianOrMajor);
    ratiosString.referTo(state, IDs::ratios, nullptr);
}

TuningSystemType RationalTuning::getType() const {
    return TuningSystemType(state.getProperty(IDs::type, "CustomRational").toString().toStdString());
}

double RationalTuning::midiNoteToFrequency(int midiNote) const {
    // Implement custom rational tuning logic here
    auto octave = static_cast<int>(midiNote / 12 - 1); // MIDI note 0 is C-1, so octave starts at -1
    auto pitchClass = static_cast<int>(midiNote) % 12; // MIDI pitch class is the note within the octave (0-11)
    auto noteIdx = pitchClass - tonic.get();
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
    auto ratio = cachedRatios.at(size_t(noteIdx % 12));
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
    auto semitonesFromTonicToA = tonic.get() - static_cast<int>(Scale::Tonic::A);
    auto ratio = cachedRatios.at(size_t (std::abs(semitonesFromTonicToA)));
    // DBG("Tonic " << tonic.get() << " frequency for octave " << octave
    //     << ": semitones from tonic " << tonic.get() << " to A = " << semitonesFromTonicToA
    // );
    if (semitonesFromTonicToA < 0) {
        ratio.invert();
        // DBG("Inverted ratio for negative semitones: " << String(ratio));
    }
    // DBG("Ratio = " << String(ratio));
    return getA4Frequency() * ratio * std::pow(2.0, octave - 4);
}

String RationalTuning::getDegreeRepresentation(int degree) const {
    const auto& fraction = cachedRatios.at(size_t(degree % 12));
    return String::formatted("%d:%d", fraction.getDenominator(), fraction.getNumerator());
}

void RationalTuning::setTonic(Scale::Tonic newKey) {
    tonic = newKey;
}

Scale::Tonic RationalTuning::getTonic() const {
    return tonic.get();
}

void RationalTuning::setScaleType(Scale::ScaleType newScaleType) {
    scaleType = newScaleType;
}

Scale::ScaleType RationalTuning::getScaleType() const {
    return scaleType.get();
}

void RationalTuning::updateCachedRatios() {
    cachedRatios = getRatiosFromString(ratiosString.get());
}

std::array<FractionNumber, 12> RationalTuning::getRatiosFromString(const juce::String& str) const {
    std::array<FractionNumber, 12> result = {{
        FractionNumber(1, 1), FractionNumber(1, 1), FractionNumber(1, 1), FractionNumber(1, 1),
        FractionNumber(1, 1), FractionNumber(1, 1), FractionNumber(1, 1), FractionNumber(1, 1),
        FractionNumber(1, 1), FractionNumber(1, 1), FractionNumber(1, 1), FractionNumber(1, 1)
    }};

    auto tokens = juce::StringArray::fromTokens(str, ",", "");

    for (int i = 0; i < 12 && i < tokens.size(); ++i) {
        auto parts = juce::StringArray::fromTokens(tokens[i], "/", "");
        if (parts.size() == 2) {
            int numerator = parts[0].getIntValue();
            int denominator = parts[1].getIntValue();
            result[static_cast<size_t>(i)] = FractionNumber(numerator, denominator);
        } else {
            result[static_cast<size_t>(i)] = FractionNumber(1, 1); // Default to unison if parsing fails
        }
    }

    return result;
}

juce::String RationalTuning::getRatiosAsString(const std::array<FractionNumber, 12>& ratios) const {
    juce::StringArray tokens;
    for (const auto& ratio : ratios) {
        tokens.add(String::formatted("%d/%d", ratio.getNumerator(), ratio.getDenominator()));
    }
    return tokens.joinIntoString(",");
}

//==============================================================================
// Just Intonation 5-limit tuning
JustIntonation5Limit::JustIntonation5Limit(const Scale::Tonic tonicToUse, const Scale::ScaleType scaleTypeToUse, double a4Freq, std::array<FractionNumber, 12> ratios)
    : RationalTuning(ratios, tonicToUse, scaleTypeToUse, a4Freq)
{
    state.setProperty(IDs::type, "Just5Limit", nullptr);
}

//==============================================================================
JustIntonation5LimitT45_64::JustIntonation5LimitT45_64(const Scale::Tonic tonicToUse, const Scale::ScaleType scaleTypeToUse, double a4Freq, std::array<FractionNumber, 12> ratios)
    : RationalTuning(ratios, tonicToUse, scaleTypeToUse, a4Freq)
{
    state.setProperty(IDs::type, "Just5LimitT45_64", nullptr);
}

//==============================================================================
PythagoreanTuning::PythagoreanTuning(const Scale::Tonic tonicToUse, const Scale::ScaleType scaleTypeToUse, double a4Freq, std::array<FractionNumber, 12> ratios)
    : RationalTuning(ratios, tonicToUse, scaleTypeToUse, a4Freq)
{
    state.setProperty(IDs::type, "Pythagorean", nullptr);
}

//==============================================================================
// standalone functions
std::unique_ptr<ReferenceTuningSystem> makeReferenceTuningSystem(
    TuningSystemType type,
    const Scale::Tonic tonic,
    const Scale::ScaleType scaleType,
    double a4Frequency
) {
    switch (type) {
        case TuningSystemEnum::EqualTemperament:
            return std::make_unique<EqualTemperamentTuning>(a4Frequency);

        case TuningSystemEnum::Just5Limit:
            return std::make_unique<JustIntonation5Limit>(tonic, scaleType, a4Frequency);

        case TuningSystemEnum::Just5LimitT45_64:
            return std::make_unique<JustIntonation5LimitT45_64>(tonic, scaleType, a4Frequency);

        case TuningSystemEnum::Pythagorean:
            return std::make_unique<PythagoreanTuning>(tonic, scaleType, a4Frequency);

        // Add other temperament types here as needed
        case TuningSystemEnum::CustomRational:
        default:
            jassertfalse; // Unsupported temperament type
            return nullptr;
    }
}

std::unique_ptr<ReferenceTuningSystem> makeReferenceTuningSystemFromState(const juce::ValueTree& state) {
    auto typeString = state.getProperty(IDs::type, "EqualTemperament").toString();
    TuningSystemType type = TuningSystemType(typeString.toStdString());

    if (type == TuningSystemType::EqualTemperament) {
        return std::make_unique<EqualTemperamentTuning>(state);
    } else if (type == TuningSystemType::Just5Limit) {
        return std::make_unique<JustIntonation5Limit>(state);
    } else if (type == TuningSystemType::Just5LimitT45_64) {
        return std::make_unique<JustIntonation5LimitT45_64>(state);
    } else if (type == TuningSystemType::Pythagorean) {
        return std::make_unique<PythagoreanTuning>(state);
    } else if (type == TuningSystemType::CustomRational) {
        return std::make_unique<RationalTuning>(state);
    }

    jassertfalse; // Unknown temperament type
    return nullptr;
}

}