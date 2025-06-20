#include <JuceHeader.h>
#include "Ratios.h"

using namespace MoTool;

class RatiosTest : public juce::UnitTest {
public:
    RatiosTest() : UnitTest("Ratios", "MoTool") {}

    void runTest() override {
        beginTest("Empty input returns empty output");
        {
            std::vector<int> input = {};
            auto result = inverseRatios(input);
            expect(result.empty(), "Result should be empty for empty input");
        }

        beginTest("Single value returns same value");
        {
            std::vector<int> input = {100};
            auto result = inverseRatios(input);
            expectEquals(static_cast<int>(result.size()), 1);
            expectEquals(result[0], 100);
        }

        beginTest("Example from documentation - Major scale frequency ratios");
        {
            // Input: frequency ratios {48, 45, 40, 36, 32, 30, 27}
            // Expected: period ratios {90, 96, 108, 120, 135, 144, 160}
            std::vector<int> frequencyRatios = {48, 45, 40, 36, 32, 30, 27};
            std::vector<int> expected = {90, 96, 108, 120, 135, 144, 160};

            auto result = inverseRatios(frequencyRatios);

            expectEquals(static_cast<int>(result.size()), static_cast<int>(expected.size()));
            for (size_t i = 0; i < expected.size(); ++i) {
                expectEquals(result[i], expected[i], "Element " + String(static_cast<int>(i)));
            }
        }

        beginTest("Simple octave relationship");
        {
            // Octave: 2:1 frequency ratio should become 1:2 period ratio
            std::vector<int> input = {200, 100}; // 2:1 frequency ratio
            auto result = inverseRatios(input);

            // Should return periods in 1:2 ratio
            expectEquals(static_cast<int>(result.size()), 2);
            expectEquals(result[1], 2 * result[0], "Second period should be twice the first");
        }

        beginTest("Perfect fifth relationship");
        {
            // Perfect fifth: 3:2 frequency ratio should become 2:3 period ratio
            std::vector<int> input = {300, 200}; // 3:2 frequency ratio
            auto result = inverseRatios(input);

            expectEquals(static_cast<int>(result.size()), 2);
            // Check that the ratio is inverted: if freq is 3:2, period should be 2:3
            expectEquals(result[0] * 3, result[1] * 2, "Frequency 3:2 should become period 2:3");
        }

        beginTest("Powers of 2 - should handle binary scales");
        {
            std::vector<int> input = {64, 32, 16, 8}; // Powers of 2 (octave divisions)
            auto result = inverseRatios(input);

            expectEquals(static_cast<int>(result.size()), 4);
            // Each period should be half the previous (inverse of doubling frequency)
            expectEquals(result[1], 2 * result[0], "Second period should be 2x first");
            expectEquals(result[2], 4 * result[0], "Third period should be 4x first");
            expectEquals(result[3], 8 * result[0], "Fourth period should be 8x first");
        }

        beginTest("Maintains ordering - higher frequencies become lower periods");
        {
            std::vector<int> descendingFreqs = {100, 90, 80, 70, 60};
            auto result = inverseRatios(descendingFreqs);

            expectEquals(static_cast<int>(result.size()), 5);
            // Result should be in ascending order (inverse of descending input)
            for (size_t i = 1; i < result.size(); ++i) {
                expect(result[i] > result[i-1], "Periods should be in ascending order");
            }
        }

        beginTest("Mathematical validation - frequency × period should be proportional to constant");
        {
            std::vector<int> input = {120, 100, 80, 60};
            auto result = inverseRatios(input);

            expectEquals(static_cast<int>(result.size()), 4);

            // For inverse relationship, freq[i] × period[i] should be constant
            double constant = input[0] * result[0];
            for (size_t i = 1; i < input.size(); ++i) {
                double product = input[i] * result[i];
                // Allow small floating point tolerance
                expect(std::abs(product - constant) < 0.01, "Product should be constant");
            }
        }

        beginTest("Just intonation ratios - common musical intervals");
        {
            // Common just intonation intervals as integer ratios
            // Unison(1:1), Major 2nd(9:8), Major 3rd(5:4), Perfect 4th(4:3),
            // Perfect 5th(3:2), Major 6th(5:3), Major 7th(15:8), Octave(2:1)
            std::vector<int> input = {240, 216, 192, 180, 160, 144, 128, 120}; // LCM=240
            auto result = inverseRatios(input);

            expectEquals(static_cast<int>(result.size()), 8);

            // Verify some key relationships
            // Octave: last should be half of first period
            expectEquals(result[7], 2 * result[0], "Octave should be 2:1 period ratio");

            // Perfect fifth (3:2) becomes (2:3)
            expectEquals(result[0] * 3, result[4] * 2, "Perfect fifth should invert to 2:3");

            // Perfect fourth (4:3) becomes (3:4)
            expectEquals(result[0] * 4, result[3] * 3, "Perfect fourth should invert to 3:4");
        }
    }
};

static RatiosTest ratiosTest;

class RationalNumberTest : public juce::UnitTest {
public:
    RationalNumberTest() : UnitTest("RationalNumber", "MoTool") {}

    void runTest() override {
        beginTest("Construction from integers");
        {
            RationalNumber ratio(3, 2);
            expect(ratio.isJustInterval(), "Should be rational");
            expectWithinAbsoluteError(static_cast<double>(ratio), 1.5, 1e-9);
        }

        beginTest("Construction from double");
        {
            RationalNumber ratio(1.5);
            expect(!ratio.isJustInterval(), "Should not be rational");
            expectWithinAbsoluteError(static_cast<double>(ratio), 1.5, 1e-9);
        }

        beginTest("Construction from semitones");
        {
            auto ratio = RationalNumber::fromSemitones(12.0); // Octave
            expectWithinAbsoluteError(static_cast<double>(ratio), 2.0, 1e-9);
        }

        beginTest("Construction from cents");
        {
            auto ratio = RationalNumber::fromCents(1200.0); // Octave
            expectWithinAbsoluteError(static_cast<double>(ratio), 2.0, 1e-9);
        }

        beginTest("Rational multiplication");
        {
            RationalNumber third(5, 4);    // Major third
            RationalNumber fifth(3, 2);    // Perfect fifth
            
            auto result = third * fifth;   // Should be major seventh (15:8)
            expect(result.isJustInterval(), "Product should be rational");
            expectWithinAbsoluteError(static_cast<double>(result), 15.0/8.0, 1e-9);
        }

        beginTest("Rational division");
        {
            RationalNumber fifth(3, 2);    // Perfect fifth
            RationalNumber third(5, 4);    // Major third
            
            auto result = fifth / third;   // Should be (3/2) / (5/4) = (3/2) * (4/5) = 6/5
            expect(result.isJustInterval(), "Quotient should be rational");
            expectWithinAbsoluteError(static_cast<double>(result), 6.0/5.0, 1e-9);
        }

        beginTest("Mixed rational and irrational operations");
        {
            RationalNumber rational(3, 2);   // Perfect fifth
            RationalNumber irrational(1.41421356); // Approximate sqrt(2)
            
            auto result = rational * irrational;
            expect(!result.isJustInterval(), "Product should not be rational");
            expectWithinAbsoluteError(static_cast<double>(result), 1.5 * 1.41421356, 1e-6);
        }

        beginTest("Inversion");
        {
            RationalNumber fifth(3, 2);
            auto inverted = fifth.inverted();
            
            expect(inverted.isJustInterval(), "Inverted should be rational");
            expectWithinAbsoluteError(static_cast<double>(inverted), 2.0/3.0, 1e-9);
            
            // Test that inversion is reversible
            auto doubleInverted = inverted.inverted();
            expectWithinAbsoluteError(static_cast<double>(doubleInverted), 1.5, 1e-9);
        }

        beginTest("Compound operations - perfect fifth circle");
        {
            RationalNumber fifth(3, 2);
            RationalNumber result = fifth;
            
            // Multiply by fifth 12 times (circle of fifths)
            for (int i = 0; i < 11; ++i) {
                result *= fifth;
            }
            
            // Should be close to multiple octaves (powers of 2)
            // 12 fifths = (3/2)^12 = 531441/4096 ≈ 129.746
            // This is approximately 2^7 = 128 (Pythagorean comma difference)
            double expected = std::pow(3.0/2.0, 12);
            expectWithinAbsoluteError(static_cast<double>(result), expected, 1e-6);
        }

        beginTest("Free-standing operators - scalar multiplication");
        {
            RationalNumber third(5, 4);
            
            auto result1 = 2 * third;  // int * RationalNumber
            auto result2 = 2.0 * third; // double * RationalNumber
            
            expectWithinAbsoluteError(static_cast<double>(result1), 2.5, 1e-9);
            expectWithinAbsoluteError(static_cast<double>(result2), 2.5, 1e-9);
        }

        beginTest("Free-standing operators - scalar division");
        {
            RationalNumber third(5, 4);
            
            auto result1 = 5 / third;    // Should be 5 / (5/4) = 4
            auto result2 = 5.0 / third;  // Should be 5.0 / (5/4) = 4.0
            
            expectWithinAbsoluteError(static_cast<double>(result1), 4.0, 1e-9);
            expectWithinAbsoluteError(static_cast<double>(result2), 4.0, 1e-9);
        }

        beginTest("Equality comparison - rational vs rational");
        {
            RationalNumber r1(6, 4);   // 6/4 = 3/2
            RationalNumber r2(3, 2);   // 3/2
            RationalNumber r3(5, 4);   // 5/4
            
            expect(r1 == r2, "6/4 should equal 3/2");
            expect(!(r1 == r3), "6/4 should not equal 5/4");
        }

        beginTest("Equality comparison - mixed types");
        {
            RationalNumber rational(3, 2);
            RationalNumber irrational(1.5);
            
            expect(rational == irrational, "3/2 should equal 1.5");
        }

        beginTest("Conversion to semitones and cents");
        {
            RationalNumber octave(2, 1);
            RationalNumber fifth(3, 2);
            
            expectWithinAbsoluteError(octave.toSemitones(), 12.0, 1e-6);
            expectWithinAbsoluteError(octave.toCents(), 1200.0, 1e-6);
            
            // Perfect fifth is approximately 7.02 semitones
            expectWithinAbsoluteError(fifth.toSemitones(), std::log2(1.5) * 12.0, 1e-6);
            expectWithinAbsoluteError(fifth.toCents(), std::log2(1.5) * 1200.0, 1e-6);
        }

        beginTest("Assignment operators");
        {
            RationalNumber ratio(5, 4);
            
            ratio *= 2;  // Should become 10/4 = 5/2
            expectWithinAbsoluteError(static_cast<double>(ratio), 2.5, 1e-9);
            
            ratio /= 5;  // Should become 2/4 = 1/2
            expectWithinAbsoluteError(static_cast<double>(ratio), 0.5, 1e-9);
        }

        beginTest("Complex musical interval calculations");
        {
            // Test some common musical interval arithmetic
            RationalNumber majorSecond(9, 8);
            RationalNumber majorThird(5, 4);
            RationalNumber perfectFourth(4, 3);
            RationalNumber perfectFifth(3, 2);
            
            // Major third + minor third should equal perfect fifth
            RationalNumber minorThird = perfectFifth / majorThird; // (3/2) / (5/4) = 6/5
            auto testFifth = majorThird * minorThird;
            expectWithinAbsoluteError(static_cast<double>(testFifth), 1.5, 1e-9);
            
            // Perfect fourth + perfect fifth should equal octave
            auto testOctave = perfectFourth * perfectFifth;
            expectWithinAbsoluteError(static_cast<double>(testOctave), 2.0, 1e-9);
            
            // Two major seconds should be close to major third (in equal temperament)
            auto twoSeconds = majorSecond * majorSecond;
            double diff = std::abs(static_cast<double>(twoSeconds) - static_cast<double>(majorThird));
            expect(diff < 0.1, "Two major seconds should be close to major third");
        }

        beginTest("Edge cases - zero and negative values");
        {
            RationalNumber zero(0, 1);
            RationalNumber negative(-3, 2);
            
            expectWithinAbsoluteError(static_cast<double>(zero), 0.0, 1e-9);
            expectWithinAbsoluteError(static_cast<double>(negative), -1.5, 1e-9);
            
            // Test multiplication with zero
            RationalNumber third(5, 4);
            auto result = third * zero;
            expectWithinAbsoluteError(static_cast<double>(result), 0.0, 1e-9);
        }
    }
};

static RationalNumberTest rationalNumberTest;