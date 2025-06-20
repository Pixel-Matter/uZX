#pragma once

#include "juce_core/juce_core.h"
#include <JuceHeader.h>
#include <vector>
#include <cmath>

namespace MoTool {

// Ratio representation (supports both equal-tempered and just)
class FractionNumber {
public:
    constexpr static FractionNumber fromSemitones(double semitones) {
        // convert semitones to a ratio
        if (semitones == 0.0) {
            return FractionNumber {1, 1}; // Unison
        }
        double ratio = std::pow(2.0, semitones / 12.0);
        return FractionNumber {ratio};
    }

    constexpr static FractionNumber fromCents(double cents) {
        // convert cents to a ratio
        if (cents == 0.0) {
            return FractionNumber {1, 1}; // Unison
        }
        double ratio = std::pow(2.0, cents / 1200.0);
        return FractionNumber {ratio};
    }

    constexpr explicit FractionNumber(double v)
      : value(v)
      , isRational(false)
      , num(-1)
      , denum(-1)
    {}

    constexpr FractionNumber(int numerator, int denominator)
      : value(static_cast<double>(numerator) / denominator)
      , isRational(true)
      , num(numerator)
      , denum(denominator)
    {}

    FractionNumber(const juce::String& ratio); // "3:2", "5:4", etc.

    constexpr double toSemitones() const noexcept {
        return std::log2(value) * 12.0; // Convert ratio to semitones
    }

    constexpr double toCents() const noexcept {
        return std::log2(value) * 1200.0; // Convert ratio to cents
    }

    constexpr bool isFraction() const noexcept { return isRational; }

    constexpr operator double() const noexcept { return value; }

    constexpr FractionNumber operator *(int number) const noexcept {
        if (isFraction()) {
            return FractionNumber {num * number, denum};
        } else {
            return FractionNumber {value * number};
        }
    }

    constexpr FractionNumber& operator *=(int number) noexcept {
        if (isFraction()) {
            num *= number;
            value = static_cast<double>(num) / denum;
        } else {
            value *= number;
        }
        return *this;
    }

    constexpr FractionNumber operator *(const FractionNumber& rhs) const noexcept {
        if (isFraction() && rhs.isFraction()) {
            return FractionNumber {num * rhs.num, denum * rhs.denum};
        } else {
            return FractionNumber {value * rhs.value};
        }
    }

    constexpr FractionNumber operator +(int number) const noexcept {
        if (isFraction()) {
            return FractionNumber {num + number * denum, denum};
        } else {
            return FractionNumber {value + number};
        }
    }

    constexpr FractionNumber& operator +=(int number) noexcept {
        if (isFraction()) {
            num += number * denum;
            value = static_cast<double>(num) / denum;
        } else {
            value += number;
        }
        return *this;
    }

    constexpr FractionNumber operator +(const FractionNumber& rhs) const noexcept {
        if (isFraction() && rhs.isFraction()) {
            // Common denominator: a/b + c/d = (a*d + c*b)/(b*d)
            return FractionNumber {num * rhs.denum + rhs.num * denum, denum * rhs.denum};
        } else {
            return FractionNumber {value + rhs.value};
        }
    }

    constexpr FractionNumber& operator +=(const FractionNumber& rhs) noexcept {
        if (isFraction() && rhs.isFraction()) {
            num = num * rhs.denum + rhs.num * denum;
            denum *= rhs.denum;
            value = static_cast<double>(num) / denum;
        } else {
            value += rhs.value;
            isRational = false;  // Result is no longer fractional
        }
        return *this;
    }

    constexpr FractionNumber operator -(int number) const noexcept {
        if (isFraction()) {
            return FractionNumber {num - number * denum, denum};
        } else {
            return FractionNumber {value - number};
        }
    }

    constexpr FractionNumber& operator -=(int number) noexcept {
        if (isFraction()) {
            num -= number * denum;
            value = static_cast<double>(num) / denum;
        } else {
            value -= number;
        }
        return *this;
    }

    constexpr FractionNumber operator -(const FractionNumber& rhs) const noexcept {
        if (isFraction() && rhs.isFraction()) {
            // Common denominator: a/b - c/d = (a*d - c*b)/(b*d)
            return FractionNumber {num * rhs.denum - rhs.num * denum, denum * rhs.denum};
        } else {
            return FractionNumber {value - rhs.value};
        }
    }

    constexpr FractionNumber& operator -=(const FractionNumber& rhs) noexcept {
        if (isFraction() && rhs.isFraction()) {
            num = num * rhs.denum - rhs.num * denum;
            denum *= rhs.denum;
            value = static_cast<double>(num) / denum;
        } else {
            value -= rhs.value;
            isRational = false;  // Result is no longer fractional
        }
        return *this;
    }

    constexpr FractionNumber operator -() const noexcept {
        if (isFraction()) {
            return FractionNumber {-num, denum};
        } else {
            return FractionNumber {-value};
        }
    }

    constexpr FractionNumber& operator *=(const FractionNumber& rhs) noexcept {
        if (isFraction() && rhs.isFraction()) {
            num *= rhs.num;
            denum *= rhs.denum;
            value = static_cast<double>(num) / denum;
        } else {
            value *= rhs.value;
            isRational = false;  // Result is no longer fractional
        }
        return *this;
    }

    constexpr FractionNumber operator /(int number) const {
        if (isFraction()) {
            return FractionNumber {num, denum * number};
        } else {
            return FractionNumber {value / number};
        }
    }

    constexpr FractionNumber& operator /=(int number) {
        if (isFraction()) {
            denum *= number;
            value = static_cast<double>(num) / denum;
        } else {
            value /= number;
        }
        return *this;
    }

    constexpr FractionNumber operator /(const FractionNumber& rhs) const {
        if (isFraction() && rhs.isFraction()) {
            return FractionNumber {num * rhs.denum, denum * rhs.num};
        } else {
            return FractionNumber {value / rhs.value};
        }
    }

    constexpr FractionNumber& operator /=(const FractionNumber& rhs) {
        if (isFraction() && rhs.isFraction()) {
            num *= rhs.denum;
            denum *= rhs.num;
            value = static_cast<double>(num) / denum;
        } else {
            value /= rhs.value;
            isRational = false;  // Result is no longer rational
        }
        return *this;
    }

    constexpr bool isFinite() const noexcept {
        if (isFraction()) {
            return denum != 0;
        }
        return std::isfinite(value);
    }

    constexpr bool operator ==(const FractionNumber& other) const noexcept {
        if (isFraction() && other.isFraction()) {
            // Use cross-multiplication to compare fractions: a/b == c/d if a*d == b*c
            return num * other.denum == denum * other.num;
        } else {
            return std::abs(value - other.value) < 1e-9; // Use a small epsilon for floating-point comparison
        }
    }

    constexpr bool operator !=(const FractionNumber& other) const noexcept {
        return !(*this == other);
    }

    constexpr bool operator <(int number) const noexcept {
        if (isFraction()) {
            // a/b < n  =>  a < n*b
            return num < number * denum;
        } else {
            return value < number;
        }
    }

    constexpr bool operator <(const FractionNumber& other) const noexcept {
        if (isFraction() && other.isFraction()) {
            // a/b < c/d  =>  a*d < c*b
            return num * other.denum < other.num * denum;
        } else {
            return value < other.value;
        }
    }

    constexpr bool operator >(int number) const noexcept {
        if (isFraction()) {
            // a/b > n  =>  a > n*b
            return num > number * denum;
        } else {
            return value > number;
        }
    }

    constexpr bool operator >(const FractionNumber& other) const noexcept {
        if (isFraction() && other.isFraction()) {
            // a/b > c/d  =>  a*d > c*b
            return num * other.denum > other.num * denum;
        } else {
            return value > other.value;
        }
    }

    constexpr bool operator <=(int number) const noexcept {
        return !(*this > number);
    }

    constexpr bool operator <=(const FractionNumber& other) const noexcept {
        return !(*this > other);
    }

    constexpr bool operator >=(int number) const noexcept {
        return !(*this < number);
    }

    constexpr bool operator >=(const FractionNumber& other) const noexcept {
        return !(*this < other);
    }

    operator String() const {
        if (isFraction()) {
            return String::formatted("%d:%d", num, denum);
        } else {
            return String(value);
        }
    }

    constexpr FractionNumber inverted() const noexcept {
        if (isFraction()) {
            return FractionNumber(denum, num);
        } else {
            return FractionNumber(1.0 / value);
        }
    }

    constexpr FractionNumber& invert() noexcept {
        if (isFraction()) {
            std::swap(num, denum);
        } else {
            value = 1.0 / value;
        }
        return *this;
    }

    constexpr int getNumerator() const noexcept { return num; }
    constexpr int getDenominator() const noexcept { return denum; }

private:
    constexpr FractionNumber(double v, bool isR, int n, int d) noexcept
      : value(v)
      , isRational(isR)
      , num(n)
      , denum(d)
    {}

    double value;        // Semitones or ratio
    bool isRational;     // true = just interval, false = equal tempered
    int num, denum;      // For rational intervals
};

constexpr inline FractionNumber operator *(int number, const FractionNumber& ratio) noexcept {
    return ratio * number; // Use the existing operator* for multiplication
}

constexpr inline double operator *(double number, const FractionNumber& ratio) noexcept {
    return static_cast<double>(ratio) * number; // Convert ratio to double and multiply
}

constexpr inline FractionNumber operator /(int number, const FractionNumber& ratio) noexcept {
    return ratio.inverted() * number; // Use the existing operator* for multiplication
}

constexpr inline double operator /(double number, const FractionNumber& ratio) noexcept {
    return number / static_cast<double>(ratio); // Convert ratio to double and divide
}

constexpr inline FractionNumber operator +(int number, const FractionNumber& ratio) noexcept {
    return ratio + number; // Use the existing operator+ for addition
}

constexpr inline double operator +(double number, const FractionNumber& ratio) noexcept {
    return static_cast<double>(ratio) + number; // Convert ratio to double and add
}

constexpr inline FractionNumber operator -(int number, const FractionNumber& ratio) noexcept {
    if (ratio.isFraction()) {
        // number - a/b = (number*b - a)/b
        return FractionNumber{number * ratio.getDenominator() - ratio.getNumerator(), ratio.getDenominator()};
    } else {
        return FractionNumber{static_cast<double>(number) - static_cast<double>(ratio)};
    }
}

constexpr inline double operator -(double number, const FractionNumber& ratio) noexcept {
    return number - static_cast<double>(ratio); // Convert ratio to double and subtract
}

constexpr inline bool operator <(int number, const FractionNumber& ratio) noexcept {
    if (ratio.isFraction()) {
        // n < a/b  =>  n*b < a
        return number * ratio.getDenominator() < ratio.getNumerator();
    } else {
        return number < static_cast<double>(ratio);
    }
}

constexpr inline bool operator <(double number, const FractionNumber& ratio) noexcept {
    return number < static_cast<double>(ratio);
}

constexpr inline bool operator >(int number, const FractionNumber& ratio) noexcept {
    if (ratio.isFraction()) {
        // n > a/b  =>  n*b > a
        return number * ratio.getDenominator() > ratio.getNumerator();
    } else {
        return number > static_cast<double>(ratio);
    }
}

constexpr inline bool operator >(double number, const FractionNumber& ratio) noexcept {
    return number > static_cast<double>(ratio);
}

constexpr inline bool operator <=(int number, const FractionNumber& ratio) noexcept {
    return !(number > ratio);
}

constexpr inline bool operator <=(double number, const FractionNumber& ratio) noexcept {
    return number <= static_cast<double>(ratio);
}

constexpr inline bool operator >=(int number, const FractionNumber& ratio) noexcept {
    return !(number < ratio);
}

constexpr inline bool operator >=(double number, const FractionNumber& ratio) noexcept {
    return number >= static_cast<double>(ratio);
}

// Utility functions for working with frequency and period ratios
std::vector<int> inverseRatios(const std::vector<int>& ratios);

}