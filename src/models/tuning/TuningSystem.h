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
        Just5LimitIntonation,
        Pythagorean,
        CustomRationalIntonation,
    };

    static inline constexpr std::string_view longLabels[] {
        "Equal Temperament",
        "5-Limit Just Intonation",
        "Pythagorean",
        "Custom Rational Intonation",
    };
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
    TuningSystem(const ChipCapabilities& capabilities, double a4Frequency = 440.0)
        : chip(capabilities), a4Freq(a4Frequency)
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
        return chip.clockFrequency / chip.divider / period;
    }

    virtual int frequencyToPeriod(double frequency) const {
        if (frequency <= 0.0) return chip.registerRange.getEnd() - 1;
        return jlimit(
            chip.registerRange.getStart(),
            chip.registerRange.getEnd() - 1,
            static_cast<int>(std::round(chip.clockFrequency / chip.divider / frequency))
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

    // Serialization
    // virtual juce::ValueTree getState() const = 0;
    // virtual void setState(const juce::ValueTree& state) = 0;
protected:
    // TODO use CachedValues refTo-ed to a state value in a tree
    const ChipCapabilities& chip;
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
        return String(std::string(getType().getLabel())) + String::formatted(" Chip clock = %.3f MHz, A4 = %.2f Hz", chip.clockFrequency / 1000000.0, a4Freq);
    }

    TuningType getType() const override {
        return TuningType::EqualTemperament;
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

class CustomTuning : public TuningSystem {
public:
    CustomTuning(const ChipCapabilities& caps,
                 const std::map<int, int>& periodTable,
                 const String& customName = "Custom Tuning",
                 double a4Frequency = 440.0)
        : TuningSystem(caps, a4Frequency)
        , periodTable_(periodTable)
        , customName_(customName)
    {
        // Find the range of defined notes
        if (!periodTable_.empty()) {
            minDefinedNote_ = periodTable_.begin()->first;
            maxDefinedNote_ = periodTable_.rbegin()->first;
        } else {
            minDefinedNote_ = 0; // Default to middle C
            maxDefinedNote_ = 0;
        }
    }

    // Constructor for ProTracker-style sequential period tables
    CustomTuning(const ChipCapabilities& caps,
                 int startingMidiNote,
                 const std::vector<int>& periods,
                 const String& customName = "Custom Tuning",
                 double a4Frequency = 440.0)
        : TuningSystem(caps, a4Frequency)
        , customName_(customName)
    {
        // Build period table from sequential array starting at startingMidiNote
        for (size_t i = 0; i < periods.size(); ++i) {
            periodTable_[startingMidiNote + static_cast<int>(i)] = periods[i];
        }

        // Find the range of defined notes
        if (!periodTable_.empty()) {
            minDefinedNote_ = periodTable_.begin()->first;
            maxDefinedNote_ = periodTable_.rbegin()->first;
        } else {
            minDefinedNote_ = startingMidiNote;
            maxDefinedNote_ = startingMidiNote;
        }
    }

    // Constructor with initializer list for convenience
    CustomTuning(const ChipCapabilities& caps,
                 int startingMidiNote,
                 std::initializer_list<int> periods,
                 const String& customName = "Custom Tuning",
                 double a4Frequency = 440.0)
        : CustomTuning(caps, startingMidiNote, std::vector<int>(periods), customName, a4Frequency)
    {
    }

    String getName() const override {
        return customName_ + String::formatted(" (Custom, defined notes %s-%s, A4 = %.2fHz)",
            getMidiNoteName(minDefinedNote_).toUTF8(), getMidiNoteName(maxDefinedNote_).toUTF8(), getA4Frequency());
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

        int result;
        // Handle notes outside the defined range
        if (note < minDefinedNote_) {
            // Extrapolate downward using octave relationship
            int octaveShift = (minDefinedNote_ - note + 11) / 12;
            int baseNote = note + octaveShift * 12;
            if (auto baseIt = periodTable_.find(baseNote); baseIt != periodTable_.end()) {
                result = baseIt->second * (1 << octaveShift); // Double period for each octave down
            } else {
                result = periodTable_.begin()->second * 2; // Fallback
            }
            return jlimit(chip.registerRange.getStart(), chip.registerRange.getEnd() - 1, result);
        }

        if (note > maxDefinedNote_) {
            // Extrapolate upward using octave relationship
            int octaveShift = (note - maxDefinedNote_ + 11) / 12;
            int baseNote = note - octaveShift * 12;
            if (auto baseIt = periodTable_.find(baseNote); baseIt != periodTable_.end()) {
                result = std::max(1, baseIt->second / (1 << octaveShift)); // Halve period for each octave up
            } else {
                result = std::max(1, periodTable_.rbegin()->second / 2); // Fallback
            }
            return jlimit(chip.registerRange.getStart(), chip.registerRange.getEnd() - 1, result);
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

        result = static_cast<int>(std::round(std::exp(logPeriod)));
        return jlimit(chip.registerRange.getStart(), chip.registerRange.getEnd() - 1, result);
    }

    double periodToMidiNote(int period) const override {
        if (periodTable_.empty()) {
            return 60.0; // Default to middle C
        }

        int closestNote = findClosestNoteByPeriod(period);

        // Calculate more precise note using frequency relationship
        double actualFreq = periodToFrequency(period);
        double targetFreq = periodToFrequency(periodTable_.at(closestNote));
        double noteOffset = 12.0 * std::log2(actualFreq / targetFreq);

        return closestNote + noteOffset;
    }

    double getOfftune(double midiNote) const override {
        // Calculate the difference between the custom tuning and equal temperament
        double customFreq = midiNoteToFrequency(midiNote);
        double equalTempFreq = getReferenceFrequency(midiNote);

        if (equalTempFreq == 0.0) return 0.0;

        // Return difference in cents
        return 1200.0 * std::log2(customFreq / equalTempFreq);
    }

    bool isDefined(int midiNote) const override {
        // Check if note is directly in the period table
        if (periodTable_.find(midiNote) != periodTable_.end()) {
            return true;
        }
        return false;  // only defined notes are those in the period table, not interpolated ones
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

    int findClosestNoteByPeriod(int period) const {
        if (periodTable_.empty()) {
            return 60; // Default to middle C
        }

        // Binary search for closest period using lower_bound
        // Since periods decrease as notes get higher, we need to search by period value
        int closestNote = periodTable_.begin()->first;
        int closestPeriodDiff = std::abs(periodTable_.begin()->second - period);

        // Find the first note with period <= target period
        auto it = std::lower_bound(periodTable_.begin(), periodTable_.end(), period,
            [](const auto& pair, int targetPeriod) {
                return pair.second > targetPeriod; // Note: periods decrease with higher notes
            });

        // Check the found position and its neighbors
        if (it != periodTable_.end()) {
            int diff = std::abs(it->second - period);
            if (diff < closestPeriodDiff) {
                closestPeriodDiff = diff;
                closestNote = it->first;
            }
        }

        // Check the previous element if it exists
        if (it != periodTable_.begin()) {
            auto prev = std::prev(it);
            int diff = std::abs(prev->second - period);
            if (diff < closestPeriodDiff) {
                closestPeriodDiff = diff;
                closestNote = prev->first;
            }
        }

        return closestNote;
    }
};

}