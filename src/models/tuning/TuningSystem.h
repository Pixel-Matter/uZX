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

    static inline constexpr std::string_view longLabels[] {
        "Equal Temperament",
        "Just Intonation",
        "Pythagorean",
        "Custom"
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
    virtual double periodToFrequency(int period) const = 0;
    virtual int frequencyToPeriod(double frequency) const = 0;
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
    EqualTemperamentTuning(const ChipCapabilities& caps, double a4Frequency = 440.0)
        : TuningSystem(caps)
        , a5Freq(a4Frequency)
    {
    }

    String getName() const override {
        return String(std::string(getType().getLabel())) + String::formatted(" Chip clock = %.3f MHz, A4 = %.2f Hz", chip.clockFrequency / 1000000.0, a5Freq);
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

    int frequencyToPeriod(double frequency) const override {
        return jlimit(
            chip.registerRange.getStart(),
            chip.registerRange.getEnd() - 1,
            static_cast<int>(std::round(chip.clockFrequency / chip.divider / frequency))
        );
    }

    double periodToFrequency(int period) const override {
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

class CustomTuning : public TuningSystem {
public:
    // Constructor with period table for specific MIDI note range
    CustomTuning(const ChipCapabilities& caps,
                 const std::map<int, int>& periodTable,
                 const String& customName = "Custom Tuning")
        : TuningSystem(caps)
        , periodTable_(periodTable)
        , customName_(customName)
    {
        // Find the range of defined notes
        if (!periodTable_.empty()) {
            minDefinedNote_ = periodTable_.begin()->first;
            maxDefinedNote_ = periodTable_.rbegin()->first;
        } else {
            minDefinedNote_ = 60; // Default to middle C
            maxDefinedNote_ = 60;
        }
    }

    String getName() const override {
        return customName_ + String::formatted(" (Custom, %d notes defined)", static_cast<int>(periodTable_.size()));
    }

    TuningType getType() const override {
        return TuningType::Custom;
    }

    double midiNoteToFrequency(double midiNote) const override {
        int period = midiNoteToPeriod(midiNote);
        return periodToFrequency(period);
    }

    double frequencyToMidiNote(double frequency) const override {
        int period = frequencyToPeriod(frequency);
        return periodToMidiNote(period);
    }

    int midiNoteToPeriod(double midiNote) const override {
        int note = static_cast<int>(std::round(midiNote));

        // If exact note is in table, return it
        auto it = periodTable_.find(note);
        if (it != periodTable_.end()) {
            return it->second;
        }

        // Handle notes outside the defined range
        if (note < minDefinedNote_) {
            // Extrapolate downward using octave relationship
            int octaveShift = (minDefinedNote_ - note + 11) / 12;
            int baseNote = note + octaveShift * 12;
            if (auto baseIt = periodTable_.find(baseNote); baseIt != periodTable_.end()) {
                return baseIt->second * (1 << octaveShift); // Double period for each octave down
            }
            return periodTable_.begin()->second * 2; // Fallback
        }

        if (note > maxDefinedNote_) {
            // Extrapolate upward using octave relationship
            int octaveShift = (note - maxDefinedNote_ + 11) / 12;
            int baseNote = note - octaveShift * 12;
            if (auto baseIt = periodTable_.find(baseNote); baseIt != periodTable_.end()) {
                return std::max(1, baseIt->second / (1 << octaveShift)); // Halve period for each octave up
            }
            return std::max(1, periodTable_.rbegin()->second / 2); // Fallback
        }

        // Interpolate between nearest defined notes
        auto upper = periodTable_.upper_bound(note);
        if (upper == periodTable_.end()) {
            return periodTable_.rbegin()->second;
        }
        if (upper == periodTable_.begin()) {
            return upper->second;
        }

        auto lower = std::prev(upper);

        // Linear interpolation in logarithmic space (geometric interpolation)
        double ratio = static_cast<double>(note - lower->first) / (upper->first - lower->first);
        double logPeriod = std::log(static_cast<double>(lower->second)) * (1.0 - ratio) +
                          std::log(static_cast<double>(upper->second)) * ratio;

        return static_cast<int>(std::round(std::exp(logPeriod)));
    }

    double periodToMidiNote(int period) const override {
        // Find the closest period in the table
        int closestNote = 60; // Default to middle C
        int closestPeriodDiff = std::numeric_limits<int>::max();

        for (const auto& [note, notePeriod] : periodTable_) {
            int diff = std::abs(notePeriod - period);
            if (diff < closestPeriodDiff) {
                closestPeriodDiff = diff;
                closestNote = note;
            }
        }

        // Calculate more precise note using frequency relationship
        double actualFreq = periodToFrequency(period);
        double targetFreq = periodToFrequency(periodTable_.at(closestNote));
        double noteOffset = 12.0 * std::log2(actualFreq / targetFreq);

        return closestNote + noteOffset;
    }

    double getOfftune(double midiNote) const override {
        // Calculate the difference between the custom tuning and equal temperament
        double customFreq = midiNoteToFrequency(midiNote);
        double equalTempFreq = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);

        if (equalTempFreq == 0.0) return 0.0;

        // Return difference in cents
        return 1200.0 * std::log2(customFreq / equalTempFreq);
    }

    // Additional methods specific to CustomTuning
    const std::map<int, int>& getPeriodTable() const { return periodTable_; }

    void updatePeriodTable(const std::map<int, int>& newTable) {
        periodTable_ = newTable;
        if (!periodTable_.empty()) {
            minDefinedNote_ = periodTable_.begin()->first;
            maxDefinedNote_ = periodTable_.rbegin()->first;
        }
    }

    Range<int> getDefinedNoteRange() const {
        return Range<int>(minDefinedNote_, maxDefinedNote_ + 1);
    }

private:
    std::map<int, int> periodTable_; // MIDI note -> chip period
    String customName_;
    int minDefinedNote_;
    int maxDefinedNote_;

    double periodToFrequency(int period) const {
        if (period <= 0) return 0.0;
        return chip.clockFrequency / chip.divider / period;
    }

    int frequencyToPeriod(double frequency) const {
        if (frequency <= 0.0) return chip.registerRange.getEnd() - 1;
        return jlimit(
            chip.registerRange.getStart(),
            chip.registerRange.getEnd() - 1,
            static_cast<int>(std::round(chip.clockFrequency / chip.divider / frequency))
        );
    }
};

}