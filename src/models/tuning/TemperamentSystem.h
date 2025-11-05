#pragma once

#include <JuceHeader.h>
#include <memory>

#include "Ratios.h"
#include "Scales.h"

#include "../../util/enumchoice.h"

namespace MoTool {

namespace IDs {
    #define DECLARE_ID(name)  const Identifier name(#name);
    DECLARE_ID(TEMPERAMENT)
    DECLARE_ID(type)
    DECLARE_ID(a4Frequency)
    DECLARE_ID(tonic)
    DECLARE_ID(ratios)
    DECLARE_ID(scaleType)

    #undef DECLARE_ID
}

inline String getMidiNoteName(int note) {
    return juce::MidiMessage::getMidiNoteName(note, true, true, 4);
}

struct TuningSystemEnum {
    enum Enum : size_t {
        EqualTemperament,
        // WellTemperament,
        Just5Limit,
        Just5LimitT45_64,
        // Just7Limit,
        Pythagorean,
        CustomRational,
    };

    static inline constexpr std::string_view shortLabels[] {
        "12TET",
        // "WT",
        "5LJI",
        "5LJI2",
        // "7LJI",
        "3LJI",
        "Custom"
    };

    static inline constexpr std::string_view longLabels[] {
        "Equal Temperament",
        // "Well Temperament",
        "5-Limit Just Intonation",
        "5-Limit Just Intonation 6=45:64",
        // "7-Limit Just Intonation",
        "Pythagorean",
        "Custom Rational Intonation",
    };
};

using TuningSystemType = MoTool::Util::EnumChoice<TuningSystemEnum>;

// Base reference tuning system interface
class ReferenceTuningSystem {
public:

    enum NoteSearch {
        Nearest,    // Find the nearest defined note
        NextHigher, // Find the next higher defined note
        NextLower   // Find the next lower defined note
    };

    ReferenceTuningSystem(double a4Frequency = 440.0);
    ReferenceTuningSystem(const juce::ValueTree& state);

    virtual ~ReferenceTuningSystem() = default;

    virtual String getDescription() const;
    inline String getTypeName() const {
        return getType().getLongLabel().data();
    }

    virtual TuningSystemType getType() const = 0;

    virtual Scale::Tonic getTonic() const = 0;
    virtual void setTonic(Scale::Tonic newTonic) = 0;

    // Core conversion functions
    // midiNote is double because we want slides and pitch bends
    virtual double midiNoteToFrequency(int midiNote) const = 0;
    virtual double midiNoteToFrequency(double midiNote) const = 0;
    virtual int frequencyToNearestMidiNote(double frequency, NoteSearch search = NoteSearch::Nearest) const = 0;
    virtual double frequencyToMidiNote(double frequency) const = 0;
    virtual String getDegreeRepresentation(int degree) const = 0;

    virtual bool isDefined(int midiNote) const = 0;

    // Setters
    void setA4Frequency(double frequency);
    double getA4Frequency() const;

    // // Serialization
    // juce::ValueTree getState() const;
    // void setState(const juce::ValueTree& state);

    juce::ValueTree state;

protected:
    juce::CachedValue<double> a4Frequency;
};

class EqualTemperamentTuning final : public ReferenceTuningSystem {
public:
    EqualTemperamentTuning(double a4Frequency = 440.0);
    EqualTemperamentTuning(const juce::ValueTree& state);

    TuningSystemType getType() const override;
    double midiNoteToFrequency(int midiNote) const override;
    double midiNoteToFrequency(double midiNote) const override;
    double frequencyToMidiNote(double frequency) const override;
    Scale::Tonic getTonic() const override;
    void setTonic(Scale::Tonic newKey) override;
    int frequencyToNearestMidiNote(double frequency, NoteSearch search = NoteSearch::Nearest) const override;
    bool isDefined(int /*midiNote*/) const override;
    String getDegreeRepresentation(int degree) const override;
};


class RationalTuning: public ReferenceTuningSystem {
public:
    RationalTuning(
        const std::array<FractionNumber, 12>& rationalIntervals,
        const Scale::Tonic keyToUse,
        const Scale::ScaleType scaleType = Scale::ScaleType::IonianOrMajor,
        double a4Frequency = 440.0
    );
    RationalTuning(const juce::ValueTree& state);

    TuningSystemType getType() const override;
    double midiNoteToFrequency(int midiNote) const override;
    double midiNoteToFrequency(double midiNote) const override;
    double frequencyToMidiNote(double frequency) const override;
    int frequencyToNearestMidiNote(double frequency, NoteSearch search = NoteSearch::Nearest) const override;
    bool isDefined(int midiNote) const override;
    String getDegreeRepresentation(int degree) const override;

    void setTonic(Scale::Tonic newKey) override;
    Scale::Tonic getTonic() const override;

    void setScaleType(Scale::ScaleType newScaleType);
    Scale::ScaleType getScaleType() const;

    double getTonicFrequency(int octave) const;

protected:
    juce::CachedValue<Scale::Tonic> tonic;
    juce::CachedValue<Scale::ScaleType> scaleType;
    juce::CachedValue<juce::String> ratiosString;

private:
    std::array<FractionNumber, 12> getRatiosFromString(const juce::String& str) const;
    juce::String getRatiosAsString(const std::array<FractionNumber, 12>& ratios) const;
    void updateCachedRatios();
    std::array<FractionNumber, 12> cachedRatios;

};


class JustIntonation5Limit final : public RationalTuning {
public:
    JustIntonation5Limit(
        const Scale::Tonic tonicToUse,
        const Scale::ScaleType scaleType = Scale::ScaleType::IonianOrMajor,
        double a4Frequency = 440.0,
        std::array<FractionNumber, 12> ratios = {
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
        }
    );
    using RationalTuning::RationalTuning;
};

//==============================================================================
class JustIntonation5LimitT45_64 final : public RationalTuning {
public:
    JustIntonation5LimitT45_64(
        const Scale::Tonic tonicToUse,
        const Scale::ScaleType scaleType = Scale::ScaleType::IonianOrMajor,
        double a4Frequency = 440.0,
        std::array<FractionNumber, 12> ratios = {
            FractionNumber(1, 1),   // Unison
            FractionNumber(16, 15), // Minor second
            FractionNumber(9, 8),   // Major second
            FractionNumber(6, 5),   // Minor third
            FractionNumber(5, 4),   // Major third
            FractionNumber(4, 3),   // Perfect fourth
            FractionNumber(64, 45), // Diminished fifth
            FractionNumber(3, 2),   // Perfect fifth
            FractionNumber(8, 5),   // Minor sixth
            FractionNumber(5, 3),   // Major sixth
            FractionNumber(16, 9),  // Minor seventh
            FractionNumber(15, 8)   // Major seventh
        }
    );
    using RationalTuning::RationalTuning;
};

//==============================================================================
class PythagoreanTuning final : public RationalTuning {
public:
    PythagoreanTuning(
        const Scale::Tonic tonicToUse,
        const Scale::ScaleType scaleType = Scale::ScaleType::IonianOrMajor,
        double a4Frequency = 440.0,
        std::array<FractionNumber, 12> ratios = {
            FractionNumber(1, 1),     // Unison
            FractionNumber(256, 243), // Minor second
            FractionNumber(9, 8),     // Major second
            FractionNumber(32, 27),   // Minor third
            FractionNumber(81, 64),   // Major third
            FractionNumber(4, 3),     // Perfect fourth
            FractionNumber(729, 512), // Augmented fourth
            FractionNumber(3, 2),     // Perfect fifth
            FractionNumber(128, 81),  // Minor sixth
            FractionNumber(27, 16),   // Major sixth
            FractionNumber(16, 9),    // Minor seventh
            FractionNumber(243, 128)  // Major seventh
        }
    );
    using RationalTuning::RationalTuning;
};

//==============================================================================
std::unique_ptr<ReferenceTuningSystem> makeReferenceTuningSystem(
    TuningSystemType type,
    const Scale::Tonic tonic,
    const Scale::ScaleType scaleType = Scale::ScaleType::IonianOrMajor,
    double a4Frequency = 440.0
);

std::unique_ptr<ReferenceTuningSystem> makeReferenceTuningSystemFromState(const juce::ValueTree& state);

} // namespace MoTool


namespace juce {

using namespace MoTool;
using namespace MoTool::Util;

template <>
struct VariantConverter<TuningSystemType> : public EnumVariantConverter<TuningSystemType> {};

}