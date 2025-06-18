#pragma once

#include <JuceHeader.h>
#include <vector>

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
    int num, denum;      // For rational intervals
};

// Utility functions for working with frequency and period ratios
std::vector<int> inverseRatios(const std::vector<int>& ratios);

}