#pragma once

#include <JuceHeader.h>
#include <memory>

#include "Ratios.h"
#include "Scales.h"

#include "../../util/enumchoice.h"

namespace MoTool {

inline String getMidiNoteName(int note) {
    return juce::MidiMessage::getMidiNoteName(note, true, true, 4);
}

struct TemperamentTypeEnum {
    enum Enum : size_t {
        EqualTemperament,
        // WellTemperament,
        Just5Limit,
        // Just7Limit,
        // Pythagorean,
        CustomRational,
    };

    static inline constexpr std::string_view longLabels[] {
        "Equal Temperament",
        // "Well Temperament",
        "5-Limit Just Intonation",
        // "7-Limit Just Intonation",
        // "Pythagorean",
        "Custom Rational Intonation",
    };
};

using TemperamentType = MoTool::Util::EnumChoice<TemperamentTypeEnum>;

// Base reference tuning system interface
class TemperamentSystem {
public:

    enum NoteSearch {
        Nearest,    // Find the nearest defined note
        NextHigher, // Find the next higher defined note
        NextLower   // Find the next lower defined note
    };

    TemperamentSystem(double a4Frequency = 440.0)
        : a4Freq(a4Frequency)
    {}

    virtual ~TemperamentSystem() = default;

    virtual String getName() const;

    virtual TemperamentType getType() const = 0;

    virtual Scale::Key getTonic() const = 0;
    virtual void setTonic(Scale::Key newKey) = 0;

    // Core conversion functions
    // midiNote is double because we want slides and pitch bends
    virtual double midiNoteToFrequency(int midiNote) const = 0;
    virtual double midiNoteToFrequency(double midiNote) const = 0;
    virtual int frequencyToNearestMidiNote(double frequency, NoteSearch search = NoteSearch::Nearest) const = 0;
    virtual double frequencyToMidiNote(double frequency) const = 0;

    virtual bool isDefined(int midiNote) const = 0;

    // Setters
    void setA4Frequency(double frequency);
    double getA4Frequency() const;

    // Serialization
    // virtual juce::ValueTree getState() const = 0;
    // virtual void setState(const juce::ValueTree& state) = 0;
protected:
    // TODO use CachedValues refTo-ed to a state value in a tree
    double a4Freq;
};

class EqualTemperamentTuning final : public TemperamentSystem {
public:
    using TemperamentSystem::TemperamentSystem;

    TemperamentType getType() const override;
    double midiNoteToFrequency(int midiNote) const override;
    double midiNoteToFrequency(double midiNote) const override;
    double frequencyToMidiNote(double frequency) const override;
    Scale::Key getTonic() const override;
    void setTonic(Scale::Key newKey) override;
    int frequencyToNearestMidiNote(double frequency, NoteSearch search = NoteSearch::Nearest) const override;
    bool isDefined(int /*midiNote*/) const override;
};


class RationalTuning: public TemperamentSystem {
public:
    RationalTuning(
        const std::array<FractionNumber, 12>& rationalIntervals,
        const Scale::Key keyToUse,
        // const Scale* scaleToUse,
        double a4Frequency = 440.0
    );

    TemperamentType getType() const override;
    double midiNoteToFrequency(int midiNote) const override;
    double midiNoteToFrequency(double midiNote) const override;
    double frequencyToMidiNote(double frequency) const override;
    int frequencyToNearestMidiNote(double frequency, NoteSearch search = NoteSearch::Nearest) const override;
    bool isDefined(int midiNote) const override;

    void setTonic(Scale::Key newKey) override{
        tonic = newKey;
    }

    Scale::Key getTonic() const override{
        return tonic;
    }

    // void setScale(const Scale* newScale) {
    //     scale = newScale;
    // }

    // const Scale* getScale() const {
    //     return scale;
    // }

    double getTonicFrequency(int octave) const;

private:
    std::array<FractionNumber, 12> ratios;
    Scale::Key tonic;
    // const Scale* scale; // Scale to use for this tuning, if applicable

};


class JustIntonation5Limit final : public RationalTuning {
public:
    JustIntonation5Limit(
        const Scale::Key tonicToUse,
        double a4Frequency = 440.0
    );

    TemperamentType getType() const override;
};

std::unique_ptr<TemperamentSystem> makeTemperamentSystem(
    TemperamentType type,
    const Scale::Key tonic,
    double a4Frequency = 440.0
);

} // namespace MoTool