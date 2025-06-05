#pragma once

#include "JuceHeader.h"

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

enum class TuningType : size_t {
    EqualTempered,
    JustIntonation,
    Pythagorean,
    Custom
};

// Base tuning system interface
class TuningSystem {
public:
    virtual ~TuningSystem() = default;

    virtual String getName() const = 0;
    virtual TuningType getType() const = 0;

    // Core conversion functions
    virtual int midiNoteToPeriod(int midiNote, int midiChannel = 0) const = 0;
    virtual double midiNoteToFrequency(int midiNote) const = 0;
    virtual int periodToMidiNote(int period) const = 0;

    // Accuracy and validation
    virtual bool isNoteSupported(int midiNote) const = 0;
    virtual double getAccuracy(int midiNote) const = 0;
    virtual bool isEnvelopeCompatible(int midiNote) const = 0;

    // Serialization
    virtual juce::ValueTree getState() const = 0;
    virtual void setState(const juce::ValueTree& state) = 0;
};

}