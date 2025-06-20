#pragma once

#include "juce_core/juce_core.h"
#include <JuceHeader.h>
#include <vector>
#include <cmath>

namespace MoTool {

// Ratio representation (supports both equal-tempered and just)
class RationalNumber {
public:
    constexpr static RationalNumber fromSemitones(double semitones) {
        // convert semitones to a ratio
        if (semitones == 0.0) {
            return RationalNumber {1, 1}; // Unison
        }
        double ratio = std::pow(2.0, semitones / 12.0);
        return RationalNumber {ratio};
    }

    constexpr static RationalNumber fromCents(double cents) {
        // convert cents to a ratio
        if (cents == 0.0) {
            return RationalNumber {1, 1}; // Unison
        }
        double ratio = std::pow(2.0, cents / 1200.0);
        return RationalNumber {ratio};
    }

    constexpr explicit RationalNumber(double v)
      : value(v)
      , isRational(false)
      , num(-1)
      , denum(-1)
    {}

    constexpr RationalNumber(int numerator, int denominator)
      : value(static_cast<double>(numerator) / denominator)
      , isRational(true)
      , num(numerator)
      , denum(denominator)
    {}

    RationalNumber(const juce::String& ratio); // "3:2", "5:4", etc.

    constexpr double toSemitones() const noexcept {
        return std::log2(value) * 12.0; // Convert ratio to semitones
    }

    constexpr double toCents() const noexcept {
        return std::log2(value) * 1200.0; // Convert ratio to cents
    }

    constexpr bool isJustInterval() const noexcept { return isRational; }

    constexpr operator double() const noexcept { return value; }

    constexpr RationalNumber operator *(int number) const noexcept {
        if (isJustInterval()) {
            return RationalNumber {num * number, denum};
        } else {
            return RationalNumber {value * number};
        }
    }

    constexpr RationalNumber& operator *=(int number) noexcept {
        if (isJustInterval()) {
            num *= number;
            value = static_cast<double>(num) / denum;
        } else {
            value *= number;
        }
        return *this;
    }

    constexpr RationalNumber operator *(double number) const noexcept {
        return RationalNumber {value * number};
    }

    constexpr RationalNumber& operator *=(double number) noexcept {
        value *= number;
        return *this;
    }

    constexpr RationalNumber operator *(const RationalNumber& rhs) const noexcept {
        if (isJustInterval() && rhs.isJustInterval()) {
            return RationalNumber {num * rhs.num, denum * rhs.denum};
        } else {
            return RationalNumber {value * rhs.value};
        }
    }

    constexpr RationalNumber& operator *=(const RationalNumber& rhs) noexcept {
        if (isJustInterval() && rhs.isJustInterval()) {
            num *= rhs.num;
            denum *= rhs.denum;
            value = static_cast<double>(num) / denum;
        } else {
            value *= rhs.value;
            isRational = false;  // Result is no longer rational
        }
        return *this;
    }

    constexpr RationalNumber operator /(int number) const {
        if (isJustInterval()) {
            return RationalNumber {num, denum * number};
        } else {
            return RationalNumber {value / number};
        }
    }

    constexpr RationalNumber& operator /=(int number) {
        if (isJustInterval()) {
            denum *= number;
            value = static_cast<double>(num) / denum;
        } else {
            value /= number;
        }
        return *this;
    }

    constexpr RationalNumber operator /(double number) const {
        return RationalNumber {value / number};
    }

    constexpr RationalNumber& operator /=(double number) {
        value /= number;
        return *this;
    }

    constexpr RationalNumber operator /(const RationalNumber& rhs) const {
        if (isJustInterval() && rhs.isJustInterval()) {
            return RationalNumber {num * rhs.denum, denum * rhs.num};
        } else {
            return RationalNumber {value / rhs.value};
        }
    }

    constexpr RationalNumber& operator /=(const RationalNumber& rhs) {
        if (isJustInterval() && rhs.isJustInterval()) {
            num *= rhs.denum;
            denum *= rhs.num;
        } else {
            value /= rhs.value;
        }
        return *this;
    }

    constexpr bool isFinite() const noexcept {
        if (isJustInterval()) {
            return denum != 0;
        }
        return std::isfinite(value);
    }

    constexpr bool operator ==(const RationalNumber& other) const noexcept {
        if (isJustInterval() && other.isJustInterval()) {
            // Use cross-multiplication to compare fractions: a/b == c/d if a*d == b*c
            return num * other.denum == denum * other.num;
        } else {
            return std::abs(value - other.value) < 1e-9; // Use a small epsilon for floating-point comparison
        }
    }

    constexpr bool operator !=(const RationalNumber& other) const noexcept {
        return !(*this == other);
    }

    operator String() const {
        if (isJustInterval()) {
            return String::formatted("%d:%d", num, denum);
        } else {
            return String(value);
        }
    }

    constexpr RationalNumber inverted() const noexcept {
        if (isJustInterval()) {
            return RationalNumber(denum, num);
        } else {
            return RationalNumber(1.0 / value);
        }
    }

    constexpr RationalNumber& invert() noexcept {
        if (isJustInterval()) {
            std::swap(num, denum);
        } else {
            value = 1.0 / value;
        }
        return *this;
    }


private:
    constexpr RationalNumber(double v, bool isR, int n, int d) noexcept
      : value(v)
      , isRational(isR)
      , num(n)
      , denum(d)
    {}

    double value;        // Semitones or ratio
    bool isRational;     // true = just interval, false = equal tempered
    int num, denum;      // For rational intervals
};

constexpr inline RationalNumber operator *(int number, const RationalNumber& ratio) noexcept {
    return ratio * number; // Use the existing operator* for multiplication
}

constexpr inline RationalNumber operator *(double number, const RationalNumber& ratio) noexcept {
    return ratio * number; // Use the existing operator* for multiplication
}

constexpr inline RationalNumber operator /(int number, const RationalNumber& ratio) {
    if (ratio.isJustInterval()) {
        // Special case for 1 / ratio, return the inverted ratio
        return ratio.inverted() * number; // Invert the ratio and multiply
    }
    return RationalNumber{static_cast<double>(number) / static_cast<double>(ratio)};
}

constexpr inline RationalNumber operator /(double number, const RationalNumber& ratio) {
    return RationalNumber{number / static_cast<double>(ratio)};
}

// constexpr inline RationalNumber operator +(const RationalNumber& lhs, const RationalNumber& rhs) noexcept {
//     return RationalNumber{static_cast<double>(lhs) + static_cast<double>(rhs)};
// }

// constexpr inline RationalNumber operator -(const RationalNumber& lhs, const RationalNumber& rhs) noexcept {
//     return RationalNumber{static_cast<double>(lhs) - static_cast<double>(rhs)};
// }

// constexpr inline RationalNumber operator *(const RationalNumber& lhs, const RationalNumber& rhs) noexcept {
//     return RationalNumber{static_cast<double>(lhs) * static_cast<double>(rhs)};
// }

// constexpr inline RationalNumber operator /(const RationalNumber& lhs, const RationalNumber& rhs) {
//     return RationalNumber{static_cast<double>(lhs) / static_cast<double>(rhs)};
// }

// constexpr inline bool operator ==(const RationalNumber& lhs, const RationalNumber& rhs) noexcept {
//     return std::abs(static_cast<double>(lhs) - static_cast<double>(rhs)) < 1e-9;
// }

// constexpr inline bool operator !=(const RationalNumber& lhs, const RationalNumber& rhs) noexcept {
//     return !(lhs == rhs);
// }

// constexpr inline bool operator <(const RationalNumber& lhs, const RationalNumber& rhs) noexcept {
//     return static_cast<double>(lhs) < static_cast<double>(rhs);
// }

// constexpr inline bool operator >(const RationalNumber& lhs, const RationalNumber& rhs) noexcept {
//     return rhs < lhs;
// }

// constexpr inline bool operator <=(const RationalNumber& lhs, const RationalNumber& rhs) noexcept {
//     return !(rhs < lhs);
// }

// constexpr inline bool operator >=(const RationalNumber& lhs, const RationalNumber& rhs) noexcept {
//     return !(lhs < rhs);
// }

// Utility functions for working with frequency and period ratios
std::vector<int> inverseRatios(const std::vector<int>& ratios);

}