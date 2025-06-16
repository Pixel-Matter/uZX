#pragma once

#include <JuceHeader.h>

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
        AutoTune,
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

// Base tuning system interface
class TuningSystem {
public:
    TuningSystem(const ChipCapabilities& capabilities, double chipClock, double a4Frequency)
        : chip(capabilities)
        , clockFrequency(chipClock)
        , a4Freq(a4Frequency)
    {}

    virtual ~TuningSystem() = default;

    virtual String getName() const = 0;
    virtual TuningType getType() const = 0;

    // Core conversion functions
    // midiNote is double because we want slides and pitch bends
    virtual double midiNoteToFrequency(double midiNote) const = 0;
    virtual double frequencyToMidiNote(double frequency) const = 0;
    virtual int midiNoteToPeriod(double midiNote) const = 0;
    virtual double periodToMidiNote(int period) const = 0;

    // Default chip-based period/frequency conversion (can be overridden if needed)
    virtual double periodToFrequency(int period) const {
        if (period <= 0) return 0.0;
        return clockFrequency / chip.divider / period;
    }

    virtual int frequencyToPeriod(double frequency) const {
        if (frequency <= 0.0) return chip.registerRange.getEnd() - 1;
        return jlimit(
            chip.registerRange.getStart(),
            chip.registerRange.getEnd() - 1,
            static_cast<int>(std::round(clockFrequency / chip.divider / frequency))
        );
    }

    // Accuracy and validation
    virtual double getOfftune(double midiNote) const = 0;
    virtual bool isDefined(int midiNote) const = 0;

    // Setters
    void setA4Frequency(double frequency) {
        a4Freq = frequency;
    }

    double getA4Frequency() const {
        return a4Freq;
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
    double a4Freq;

    // Reference frequency calculation (A4 = 69, default 440Hz)
    virtual double getReferenceFrequency(double midiNote) const {
        return a4Freq * std::pow(2.0, (midiNote - 69) / 12.0);
    }

};

class EqualTemperamentTuning : public TuningSystem {
public:
    using TuningSystem::TuningSystem;

    String getName() const override {
        return String(std::string(getType().getLabel())) + String::formatted(" Chip clock = %.3f MHz, A4 = %.2f Hz", clockFrequency / 1000000.0, a4Freq);
    }

    TuningType getType() const override {
        return TuningType::AutoTune;
    }

    double midiNoteToFrequency(double midiNote) const override {
        return a4Freq * std::pow(2.0, (midiNote - 69) / 12.0);
    }

    double frequencyToMidiNote(double frequency) const override {
        return 69 + 12 * std::log2(frequency / a4Freq);
    }

    double getReferenceFrequency(double midiNote) const override {
        return a4Freq * std::pow(2.0, (midiNote - 69) / 12.0);
    }

    int midiNoteToPeriod(double midiNote) const override {
        return frequencyToPeriod(midiNoteToFrequency(midiNote));
    }

    double periodToMidiNote(int period) const override {
        return frequencyToMidiNote(periodToFrequency(period));
    }

    double getOfftune(double midiNote) const override {
        int period = frequencyToPeriod(midiNoteToFrequency(midiNote));
        double actualNote = frequencyToMidiNote(periodToFrequency(period));
        // DBG("Offtune for MIDI note " << midiNote
        //     << ": Period = " << period
        //     << ", Frequency = " << actualNote
        //     << ", Actual Note = " << actualNote
        //     << ", Expected Note = " << midiNoteToFrequency(midiNote));
        return (actualNote - midiNote) * 100.0; // Convert to cents
    }

    bool isDefined(int /*midiNote*/) const override {
        // Equal temperament is defined for all MIDI notes
        return true;
    }
};


}