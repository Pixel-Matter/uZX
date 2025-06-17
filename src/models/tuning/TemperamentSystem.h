#pragma once

#include <JuceHeader.h>
#include <memory>

#include "../../util/enumchoice.h"

namespace MoTool {

inline String getMidiNoteName(int note) {
    return juce::MidiMessage::getMidiNoteName(note, true, true, 4);
}

struct TemperamentTypeEnum {
    enum Enum : size_t {
        EqualTemperament,
        WellTemperament,
        Just5Limit,
        Just7Limit,
        Pythagorean,
        CustomRational,
    };

    static inline constexpr std::string_view longLabels[] {
        "Equal Temperament",
        "Well Temperament",
        "5-Limit Just Intonation",
        "7-Limit Just Intonation",
        "Pythagorean",
        "Custom Rational Intonation",
    };
};

using TemperamentType = MoTool::Util::EnumChoice<TemperamentTypeEnum>;

// Base reference tuning system interface
class TemperamentSystem {
public:
    TemperamentSystem(double a4Frequency = 440.0)
        : a4Freq(a4Frequency)
    {}

    virtual ~TemperamentSystem() = default;

    virtual String getName() const;

    virtual TemperamentType getType() const = 0;

    // Core conversion functions
    // midiNote is double because we want slides and pitch bends
    virtual double midiNoteToFrequency(double midiNote) const = 0;
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
    double midiNoteToFrequency(double midiNote) const override;
    double frequencyToMidiNote(double frequency) const override;
    bool isDefined(int /*midiNote*/) const override;
};


class RationalTuning final : public TemperamentSystem {
public:
    using TemperamentSystem::TemperamentSystem;

    TemperamentType getType() const override;
    double midiNoteToFrequency(double midiNote) const override;
    double frequencyToMidiNote(double frequency) const override;
    bool isDefined(int midiNote) const override;
};

} // namespace MoTool