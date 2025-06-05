#pragma once

#include "JuceHeader.h"

#include "../../util/enumchoice.h"

namespace MoTool {

// Interval representation (supports both equal-tempered and just)
class Interval {
public:
    static Interval fromSemitones(double semitones) {
        return Interval {semitones};
    }

    static Interval fromRatio(int numerator, int denominator);
    static Interval fromRatio(const juce::String& ratio); // "3:2", "5:4", etc.

    double toSemitones() const {
        return value;
    }

    double toCents() const {
        return (value - 1.0) * 1200.0;
    }

    double toRatio() const;
    bool isJustInterval() const { return isRational; }

private:
    explicit Interval(double v)
      : value(v)
      , isRational(false)
      , num(-1)
      , denum(-1)
    {}

    Interval(double v, bool isR, int n, int d)
      : value(v)
      , isRational(isR)
      , num(n)
      , denum(d)
    {}

    double value;        // Semitones or ratio
    bool isRational;     // true = just interval, false = equal tempered
    int num, denum;        // For rational intervals
};

// Scale/mode definition
class Scale {
public:
    Scale(const String& name, const std::vector<Interval>& intervals);
    Scale(const String& name, const std::vector<int>& dividers); // For example, {48, 45, 40, 36, 32, 30, 27}

    String getName() const { return scaleName; }
    const std::vector<Interval>& getIntervals() const { return intervals; }
    size_t getNumSteps() const { return intervals.size(); }

    // // Generate frequency ratios for given tuning system
    // std::vector<double> getFrequencyRatios(const TuningSystem& tuning) const;

private:
    String scaleName;
    std::vector<Interval> intervals;  // In semitones or ratio notation
};


struct TuningTypeEnum {
    enum Enum : size_t {
        EqualTemperament,
        JustIntonation,
        Pythagorean,
        Custom
    };
};

using TuningType = MoTool::Util::EnumChoice<TuningTypeEnum>;

struct ChipCapabilities {
    double clockFrequency; // Chip clock frequency in Hz
    int divider;           // Divider value for the chip (e.g., 16 for AY-3-8910)
    Range<int> registerRange; // Range of tone register supported by the chip (e.g., 1-4095 inclusive)
};

// Base tuning system interface
class TuningSystem {
public:
    TuningSystem(const ChipCapabilities& capabilities)
        : chip(capabilities)
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

    // Accuracy and validation
    virtual double getOfftune(double midiNote) const = 0;

    // Serialization
    // virtual juce::ValueTree getState() const = 0;
    // virtual void setState(const juce::ValueTree& state) = 0;
protected:
    const ChipCapabilities& chip;
};

class EqualTemperamentTuning : public TuningSystem {
public:
    EqualTemperamentTuning(const ChipCapabilities& caps, double a5Frequency = 440.0)
        : TuningSystem(caps)
        , a5Freq(a5Frequency)
    {
    }

    String getName() const override {
        return String(std::string(getType().getLabel())) + String::formatted(" Chip clock = %.3f MHz, A5 = %.2f Hz", chip.clockFrequency / 1000000.0, a5Freq);
    }

    TuningType getType() const override {
        return TuningType::EqualTemperament;
    }

    double midiNoteToFrequency(double midiNote) const override {
        return a5Freq * std::pow(2.0, (midiNote - 69) / 12.0);
    }

    double frequencyToMidiNote(double frequency) const override {
        return 69 + 12 * std::log2(frequency / a5Freq);
    }

    int frequencyToPeriod(double frequency) const {
        // DBG("Frequency: " << frequency
        //     << ", Clock: " << chip.clockFrequency
        //     << ", Divider: " << chip.divider
        //     << ", Range: " << chip.registerRange.getStart() << "-" << chip.registerRange.getEnd() - 1;
        // );
        return jlimit(
            chip.registerRange.getStart(),
            chip.registerRange.getEnd() - 1,
            static_cast<int>(std::round(chip.clockFrequency / chip.divider / frequency))
        );
    }

    double periodToFrequency(int period) const {
        return chip.clockFrequency / chip.divider / period;
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

private:
    double a5Freq;
};

}