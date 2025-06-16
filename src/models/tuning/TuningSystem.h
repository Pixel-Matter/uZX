#pragma once

#include <JuceHeader.h>
#include <memory>

#include "../../util/enumchoice.h"
#include "Scales.h"


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


struct TuningTypeEnum {
    enum Enum : size_t {
        AutoTuning,
        CustomTable,
    };

    static inline constexpr std::string_view longLabels[] {
        "Auto tuning",
        "Custom tuning table",
    };
};


using TuningType = MoTool::Util::EnumChoice<TuningTypeEnum>;


struct ChipCapabilities {
    // double clockFrequency; // Chip clock frequency in Hz
    int divider;           // Divider value for the chip (e.g., 16 for AY-3-8910)
    Range<int> registerRange; // Range of tone register supported by the chip (e.g., 1-4095 inclusive)
};

// Base reference tuning system interface
class TemperamentSystem {
public:
    TemperamentSystem(double a4Frequency = 440.0)
        : a4Freq(a4Frequency)
    {}

    virtual ~TemperamentSystem() = default;

    virtual String getName() const {
        return String(String::formatted("%s tuning, A4 = %.2f Hz", getType().getLongLabel(), getA4Frequency()));
    }

    virtual TemperamentType getType() const = 0;

    // Core conversion functions
    // midiNote is double because we want slides and pitch bends
    virtual double midiNoteToFrequency(double midiNote) const = 0;
    virtual double frequencyToMidiNote(double frequency) const = 0;

    virtual bool isDefined(int midiNote) const = 0;

    // Setters
    void setA4Frequency(double frequency) {
        a4Freq = frequency;
    }

    double getA4Frequency() const {
        return a4Freq;
    }

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

    TemperamentType getType() const override {
        return TemperamentType::EqualTemperament;
    }

    double midiNoteToFrequency(double midiNote) const override {
        return getA4Frequency() * std::pow(2.0, (midiNote - 69) / 12.0);
    }

    double frequencyToMidiNote(double frequency) const override {
        return 69 + 12 * std::log2(frequency / getA4Frequency());
    }

    bool isDefined(int /*midiNote*/) const override {
        // Equal temperament is defined for all MIDI notes
        return true;
    }
};

// TODO 5-Limit tuning


class TuningSystem {
public:
    TuningSystem(const ChipCapabilities& capabilities, double chipClock, std::unique_ptr<TemperamentSystem> refTuning)
        : chip(capabilities)
        , clockFrequency(chipClock)
        , referenceTuning(std::move(refTuning))
    {
        jassert(referenceTuning != nullptr);
    }

    virtual ~TuningSystem() = default;
    virtual String getName() const = 0;
    virtual TuningType getType() const = 0;
    virtual int midiNoteToPeriod(double midiNote) const = 0;
    virtual double periodToMidiNote(int period) const = 0;
    virtual bool isDefined(int midiNote) const = 0;

    // Default chip-based period/frequency conversion
    double periodToFrequency(int period) const {
        if (period <= 0) return 0.0;
        return clockFrequency / chip.divider / period;
    }

    int frequencyToPeriod(double frequency) const {
        if (frequency <= 0.0) return chip.registerRange.getEnd() - 1;
        return jlimit(
            chip.registerRange.getStart(),
            chip.registerRange.getEnd() - 1,
            static_cast<int>(std::round(clockFrequency / chip.divider / frequency))
        );
    }

    // Core conversion functions
    // midiNote is double because we want slides and pitch bends
    double midiNoteToFrequency(double midiNote) const {
        int period = midiNoteToPeriod(midiNote);
        return periodToFrequency(period);
    }

    double frequencyToMidiNote(double frequency) const {
        int period = frequencyToPeriod(frequency);
        return periodToMidiNote(period);
    }

    double getOfftune(double midiNote) const {
        // Calculate the difference between the custom tuning and equal temperament
        double freq = midiNoteToFrequency(midiNote);
        double refFreq = getReferenceFrequency(midiNote);

        // Return difference in cents
        return 1200.0 * std::log2(freq / refFreq);
    }

    // Setters
    void setA4Frequency(double frequency) {
        jassert(referenceTuning != nullptr);
        referenceTuning->setA4Frequency(frequency);
    }

    double getA4Frequency() const {
        jassert(referenceTuning != nullptr);
        return referenceTuning->getA4Frequency();
    }

    void setClockFrequency(double frequency) {
        clockFrequency = frequency;
    }
    double getClockFrequency() const {
        return clockFrequency;
    }

    // Serialization
    // virtual juce::ValueTree getState() const = 0;
    // virtual void setState(const juce::ValueTree& state) = 0;
protected:
    // TODO use CachedValues refTo-ed to a state value in a tree
    const ChipCapabilities& chip;
    double clockFrequency;
    std::unique_ptr<TemperamentSystem> referenceTuning;

    double getReferenceFrequency(double midiNote) const {
        jassert(referenceTuning != nullptr);
        return referenceTuning->midiNoteToFrequency(midiNote);
    }

};

// Auto tuning uses reference tuning to calculate frequencies and converts them to periods
class AutoTuning final : public TuningSystem {
public:
    using TuningSystem::TuningSystem;

    String getName() const override {
        jassert(referenceTuning != nullptr);
        return referenceTuning->getName() + String::formatted(" auto tuning, chip clock = %.3f MHz, A4 = %.2f Hz", clockFrequency / 1000000.0, getA4Frequency());
    }

    TuningType getType() const override {
        return TuningType::AutoTuning;
    }

    int midiNoteToPeriod(double midiNote) const override {
        jassert(referenceTuning != nullptr);
        return frequencyToPeriod(getReferenceFrequency(midiNote));
    }

    double periodToMidiNote(int period) const override {
        return frequencyToMidiNote(periodToFrequency(period));
    }

    bool isDefined(int midiNote) const override {
        jassert(referenceTuning != nullptr);
        return referenceTuning->isDefined(midiNote);
    }
};


}