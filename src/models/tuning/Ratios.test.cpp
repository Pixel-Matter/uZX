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

        beginTest("Mathematical validation - frequency x period should be proportional to constant");
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
            FractionNumber ratio(3, 2);
            expect(ratio.isFraction(), "Should be rational");
            expectWithinAbsoluteError(static_cast<double>(ratio), 1.5, 1e-9);
        }

        beginTest("Construction from double");
        {
            FractionNumber ratio(1.5);
            expect(!ratio.isFraction(), "Should not be rational");
            expectWithinAbsoluteError(static_cast<double>(ratio), 1.5, 1e-9);
        }

        beginTest("Construction from semitones");
        {
            auto ratio = FractionNumber::fromSemitones(12.0); // Octave
            expectWithinAbsoluteError(static_cast<double>(ratio), 2.0, 1e-9);
        }

        beginTest("Construction from cents");
        {
            auto ratio = FractionNumber::fromCents(1200.0); // Octave
            expectWithinAbsoluteError(static_cast<double>(ratio), 2.0, 1e-9);
        }

        beginTest("Rational multiplication");
        {
            FractionNumber third(5, 4);    // Major third
            FractionNumber fifth(3, 2);    // Perfect fifth

            auto result = third * fifth;   // Should be major seventh (15:8)
            expect(result.isFraction(), "Product should be rational");
            expectWithinAbsoluteError(static_cast<double>(result), 15.0/8.0, 1e-9);
        }

        beginTest("Rational division");
        {
            FractionNumber fifth(3, 2);    // Perfect fifth
            FractionNumber third(5, 4);    // Major third

            auto result = fifth / third;   // Should be (3/2) / (5/4) = (3/2) * (4/5) = 6/5
            expect(result.isFraction(), "Quotient should be rational");
            expectWithinAbsoluteError(static_cast<double>(result), 6.0/5.0, 1e-9);
        }

        beginTest("Mixed rational and irrational operations");
        {
            FractionNumber rational(3, 2);   // Perfect fifth
            FractionNumber irrational(1.41421356); // Approximate sqrt(2)

            auto result = rational * irrational;
            expect(!result.isFraction(), "Product should not be rational");
            expectWithinAbsoluteError(static_cast<double>(result), 1.5 * 1.41421356, 1e-6);
        }

        beginTest("Inversion");
        {
            FractionNumber fifth(3, 2);
            auto inverted = fifth.inverted();

            expect(inverted.isFraction(), "Inverted should be rational");
            expectWithinAbsoluteError(static_cast<double>(inverted), 2.0/3.0, 1e-9);

            // Test that inversion is reversible
            auto doubleInverted = inverted.inverted();
            expect(fifth == doubleInverted, "Inversion should be reversible");
            expectWithinAbsoluteError(static_cast<double>(doubleInverted), 1.5, 1e-9);
        }

        beginTest("Compound operations - perfect fifth circle");
        {
            FractionNumber fifth(3, 2);
            FractionNumber result = fifth;

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
            FractionNumber third(5, 4);

            auto result1 = 2 * third;  // int * FractionNumber -> FractionNumber
            auto result2 = 2.0 * third; // double * FractionNumber -> double

            static_assert(std::is_same_v<decltype(result1), FractionNumber>, "Result1 should be FractionNumber");
            static_assert(std::is_same_v<decltype(result2), double>, "Result2 should be double");

            expect(result1.isFraction(), "Result should be fraction");

            expectWithinAbsoluteError(static_cast<double>(result1), 2.5, 1e-9);
            expectWithinAbsoluteError(static_cast<double>(result2), 2.5, 1e-9);
        }

        beginTest("Free-standing operators - scalar division");
        {
            FractionNumber third(5, 4);

            auto result1 = 5 / third;  // int / FractionNumber -> FractionNumber
            auto result2 = 5.0 / third;  // double / FractionNumber -> double

            static_assert(std::is_same_v<decltype(result1), FractionNumber>, "Result1 should be FractionNumber");
            static_assert(std::is_same_v<decltype(result2), double>, "Result2 should be double");

            expectWithinAbsoluteError(static_cast<double>(result1), 4.0, 1e-9);
            expectWithinAbsoluteError(static_cast<double>(result2), 4.0, 1e-9);
        }

        beginTest("Addition operators");
        {
            FractionNumber third(5, 4);  // 1.25

            // Member operators
            auto result1 = third + 2;      // FractionNumber + int -> FractionNumber (1.25 + 2 = 3.25)
            auto result2 = third + third;  // FractionNumber + FractionNumber -> FractionNumber

            static_assert(std::is_same_v<decltype(result1), FractionNumber>, "Result1 should be FractionNumber");
            static_assert(std::is_same_v<decltype(result2), FractionNumber>, "Result2 should be FractionNumber");

            expectWithinAbsoluteError(static_cast<double>(result1), 3.25, 1e-9);
            expectWithinAbsoluteError(static_cast<double>(result2), 2.5, 1e-9);
            expect(result1.isFraction(), "Addition with int should be fractional");
            expect(result2.isFraction(), "Addition of fractions should be fractional");

            // Free-standing operators
            auto result3 = 2 + third;      // int + FractionNumber -> FractionNumber
            auto result4 = 2.0 + third;    // double + FractionNumber -> double

            static_assert(std::is_same_v<decltype(result3), FractionNumber>, "Result3 should be FractionNumber");
            static_assert(std::is_same_v<decltype(result4), double>, "Result4 should be double");

            expectWithinAbsoluteError(static_cast<double>(result3), 3.25, 1e-9);
            expectWithinAbsoluteError(result4, 3.25, 1e-9);
        }

        beginTest("Subtraction operators");
        {
            FractionNumber third(5, 4);  // 1.25

            // Member operators
            auto result1 = third - 1;      // FractionNumber - int -> FractionNumber (1.25 - 1 = 0.25)
            auto result2 = third - third;  // FractionNumber - FractionNumber -> FractionNumber (should be 0)

            expectWithinAbsoluteError(static_cast<double>(result1), 0.25, 1e-9);
            expectWithinAbsoluteError(static_cast<double>(result2), 0.0, 1e-9);
            expect(result1.isFraction(), "Subtraction with int should be fractional");
            expect(result2.isFraction(), "Subtraction of fractions should be fractional");

            // Free-standing operators
            auto result3 = 3 - third;      // int - FractionNumber -> FractionNumber (3 - 1.25 = 1.75)
            auto result4 = 3.0 - third;    // double - FractionNumber -> double

            static_assert(std::is_same_v<decltype(result3), FractionNumber>, "Result3 should be FractionNumber");
            static_assert(std::is_same_v<decltype(result4), double>, "Result4 should be double");

            expectWithinAbsoluteError(static_cast<double>(result3), 1.75, 1e-9);
            expectWithinAbsoluteError(result4, 1.75, 1e-9);
        }

        beginTest("Unary minus operator");
        {
            FractionNumber positive(3, 2);   // 1.5
            FractionNumber negative = -positive;

            expect(negative.isFraction(), "Unary minus should preserve fractional nature");
            expectWithinAbsoluteError(static_cast<double>(negative), -1.5, 1e-9);

            // Test with irrational number
            FractionNumber irrational(2.5);
            FractionNumber negIrrational = -irrational;

            expect(!negIrrational.isFraction(), "Unary minus should preserve non-fractional nature");
            expectWithinAbsoluteError(static_cast<double>(negIrrational), -2.5, 1e-9);

            // Test double negation
            FractionNumber doubleNeg = -negative;
            expect(doubleNeg == positive, "Double negation should return to original");
        }

        beginTest("Assignment operators for addition and subtraction");
        {
            FractionNumber ratio(5, 4);  // 1.25 = 5/4

            ratio += 2;  // Should become 5/4 + 2 = 5/4 + 8/4 = 13/4 = 3.25
            expectWithinAbsoluteError(static_cast<double>(ratio), 3.25, 1e-9);
            expect(ratio.isFraction(), "After += int, should be fractional");

            ratio -= 1;  // Should become 13/4 - 1 = 13/4 - 4/4 = 9/4 = 2.25
            expectWithinAbsoluteError(static_cast<double>(ratio), 2.25, 1e-9);
            expect(ratio.isFraction(), "After -= int, should be fractional");

            FractionNumber other(1, 4);  // 0.25 = 1/4
            ratio += other;  // Should become 9/4 + 1/4 = 10/4 = 2.5
            expectWithinAbsoluteError(static_cast<double>(ratio), 2.5, 1e-9);

            ratio -= other;  // Should become 10/4 - 1/4 = 9/4 = 2.25 again
            expectWithinAbsoluteError(static_cast<double>(ratio), 2.25, 1e-9);
        }

        beginTest("Proper fractional arithmetic verification");
        {
            // Test that fractional arithmetic is exact, not just approximate
            FractionNumber half(1, 2);      // 1/2
            FractionNumber third(1, 3);     // 1/3

            auto sum = half + third;         // 1/2 + 1/3 = 3/6 + 2/6 = 5/6
            expect(sum.isFraction(), "Sum should be fractional");
            expectWithinAbsoluteError(static_cast<double>(sum), 5.0/6.0, 1e-15);

            FractionNumber quarter(1, 4);   // 1/4
            auto diff = half - quarter;      // 1/2 - 1/4 = 2/4 - 1/4 = 1/4
            expect(diff.isFraction(), "Difference should be fractional");
            expectWithinAbsoluteError(static_cast<double>(diff), 0.25, 1e-15);

            // Test free-standing arithmetic
            auto result = 2 + third;         // 2 + 1/3 = 6/3 + 1/3 = 7/3
            expect(result.isFraction(), "Free-standing addition should be fractional");
            expectWithinAbsoluteError(static_cast<double>(result), 7.0/3.0, 1e-15);

            auto result2 = 3 - quarter;      // 3 - 1/4 = 12/4 - 1/4 = 11/4
            expect(result2.isFraction(), "Free-standing subtraction should be fractional");
            expectWithinAbsoluteError(static_cast<double>(result2), 11.0/4.0, 1e-15);
        }

        beginTest("Equality comparison - rational vs rational");
        {
            FractionNumber r1(6, 4);   // 6/4 = 3/2
            FractionNumber r2(3, 2);   // 3/2
            FractionNumber r3(5, 4);   // 5/4

            expect(r1 == r2, "6/4 should equal 3/2");
            expect(!(r1 == r3), "6/4 should not equal 5/4");
        }

        beginTest("Equality comparison - mixed types");
        {
            FractionNumber rational(3, 2);
            FractionNumber irrational(1.5);

            expect(rational == irrational, "3/2 should equal 1.5");
        }

        beginTest("Comparison operators - FractionNumber vs int");
        {
            FractionNumber half(1, 2);        // 0.5
            FractionNumber three_halves(3, 2); // 1.5

            // Test less than
            expect(half < 1, "1/2 should be less than 1");
            expect(!(three_halves < 1), "3/2 should not be less than 1");

            // Test greater than
            expect(three_halves > 1, "3/2 should be greater than 1");
            expect(!(half > 1), "1/2 should not be greater than 1");

            // Test less than or equal
            expect(half <= 1, "1/2 should be less than or equal to 1");
            expect(three_halves <= 2, "3/2 should be less than or equal to 2");

            // Test greater than or equal
            expect(three_halves >= 1, "3/2 should be greater than or equal to 1");
            expect(half >= 0, "1/2 should be greater than or equal to 0");
        }

        beginTest("Comparison operators - FractionNumber vs FractionNumber");
        {
            FractionNumber half(1, 2);         // 0.5
            FractionNumber third(1, 3);        // 0.333...
            FractionNumber two_thirds(2, 3);   // 0.666...
            FractionNumber three_quarters(3, 4); // 0.75

            // Test less than with different denominators
            expect(third < half, "1/3 should be less than 1/2");
            expect(half < two_thirds, "1/2 should be less than 2/3");
            expect(two_thirds < three_quarters, "2/3 should be less than 3/4");

            // Test greater than
            expect(three_quarters > two_thirds, "3/4 should be greater than 2/3");
            expect(two_thirds > half, "2/3 should be greater than 1/2");
            expect(half > third, "1/2 should be greater than 1/3");

            // Test equality edge cases
            FractionNumber half_alt(2, 4);     // 2/4 = 1/2
            expect(half == half_alt, "1/2 should equal 2/4");
            expect(!(half < half_alt), "1/2 should not be less than 2/4");
            expect(!(half > half_alt), "1/2 should not be greater than 2/4");
        }

        beginTest("Free-standing comparison operators - int vs FractionNumber");
        {
            FractionNumber half(1, 2);        // 0.5
            FractionNumber three_halves(3, 2); // 1.5

            // Test int < FractionNumber
            expect(0 < half, "0 should be less than 1/2");
            expect(1 < three_halves, "1 should be less than 3/2");
            expect(!(2 < three_halves), "2 should not be less than 3/2");

            // Test int > FractionNumber
            expect(1 > half, "1 should be greater than 1/2");
            expect(2 > three_halves, "2 should be greater than 3/2");
            expect(!(0 > half), "0 should not be greater than 1/2");

            // Test int <= and >= FractionNumber
            expect(0 <= half, "0 should be less than or equal to 1/2");
            expect(1 >= half, "1 should be greater than or equal to 1/2");
        }

        beginTest("Free-standing comparison operators - double vs FractionNumber");
        {
            FractionNumber half(1, 2);        // 0.5
            FractionNumber pi_approx(22, 7);  // ~3.14159

            // Test double comparisons (using implicit conversion)
            expect(0.25 < half, "0.25 should be less than 1/2");
            expect(0.75 > half, "0.75 should be greater than 1/2");
            expect(3.0 < pi_approx, "3.0 should be less than 22/7");
            expect(3.5 > pi_approx, "3.5 should be greater than 22/7");

            // Test edge cases near equality
            expect(!(0.5 < half), "0.5 should not be less than 1/2");
            expect(!(0.5 > half), "0.5 should not be greater than 1/2");
        }

        beginTest("Comparison operators with mixed rational/irrational");
        {
            FractionNumber rational(3, 2);     // 1.5 (rational)
            FractionNumber irrational(1.6);    // 1.6 (irrational)

            expect(rational < irrational, "3/2 should be less than 1.6");
            expect(irrational > rational, "1.6 should be greater than 3/2");
            expect(rational < 2, "3/2 should be less than 2");
            expect(1 < rational, "1 should be less than 3/2");
        }

        beginTest("Constexpr expressions with fractions");
        {
            // Test basic constexpr operations
            constexpr FractionNumber half(1, 2);
            constexpr FractionNumber third(1, 3);
            constexpr FractionNumber quarter(1, 4);

            // Simple arithmetic
            constexpr auto sum = half + third;              // 1/2 + 1/3 = 5/6
            constexpr auto diff = half - quarter;           // 1/2 - 1/4 = 1/4
            constexpr auto product = half * third;          // 1/2 * 1/3 = 1/6
            constexpr auto quotient = half / quarter;       // 1/2 / 1/4 = 2

            // Test the constexpr results at runtime
            expect(sum.isFraction(), "Constexpr sum should be fractional");
            expectWithinAbsoluteError(static_cast<double>(sum), 5.0/6.0, 1e-15);

            expect(diff.isFraction(), "Constexpr difference should be fractional");
            expectWithinAbsoluteError(static_cast<double>(diff), 0.25, 1e-15);

            expect(product.isFraction(), "Constexpr product should be fractional");
            expectWithinAbsoluteError(static_cast<double>(product), 1.0/6.0, 1e-15);

            expect(quotient.isFraction(), "Constexpr quotient should be fractional");
            expectWithinAbsoluteError(static_cast<double>(quotient), 2.0, 1e-15);

            // Test constexpr comparisons
            constexpr bool half_less_than_third = half < third;
            constexpr bool third_greater_than_quarter = third > quarter;
            constexpr bool half_equals_two_quarters = half == (quarter * 2);

            expect(!half_less_than_third, "1/2 should not be less than 1/3");
            expect(third_greater_than_quarter, "1/3 should be greater than 1/4");
            expect(half_equals_two_quarters, "1/2 should equal 1/4 * 2");
        }

        beginTest("Complex constexpr musical interval calculation");
        {
            // Complex compile-time calculation of musical intervals
            // Calculate the syntonic comma: (3/2)^4 / 2^2 / (5/4) = 81/80
            // This represents the difference between just intonation and Pythagorean tuning
            constexpr FractionNumber perfect_fifth(3, 2);
            constexpr FractionNumber major_third(5, 4);
            constexpr FractionNumber octave(2, 1);

            // Build a complex constexpr expression step by step
            constexpr auto four_fifths = perfect_fifth * perfect_fifth * perfect_fifth * perfect_fifth;  // (3/2)^4
            constexpr auto two_octaves = octave * octave;                                                 // 2^2 = 4/1
            constexpr auto pythagorean_major_third = four_fifths / two_octaves;                          // 81/64
            constexpr auto syntonic_comma = pythagorean_major_third / major_third;                       // (81/64) / (5/4) = 81/80

            // Alternative: one massive constexpr expression
            constexpr auto syntonic_comma_direct =
                (perfect_fifth * perfect_fifth * perfect_fifth * perfect_fifth) /
                (octave * octave) / major_third;

            // Test that both methods give the same result
            expect(syntonic_comma == syntonic_comma_direct, "Both syntonic comma calculations should be equal");
            expect(syntonic_comma.isFraction(), "Syntonic comma should be fractional");
            expect(syntonic_comma_direct.isFraction(), "Direct syntonic comma should be fractional");

            // The syntonic comma should be approximately 21.5 cents (very small interval)
            double syntonic_comma_cents = syntonic_comma.toCents();
            expectWithinAbsoluteError(syntonic_comma_cents, 21.506, 0.01); // ~21.5 cents

            // Test that it's exactly 81/80 as expected in music theory
            expectWithinAbsoluteError(static_cast<double>(syntonic_comma), 81.0/80.0, 1e-15);

            // Verify the mathematical relationship: 81/80 = 1.0125
            expect(syntonic_comma > FractionNumber(1, 1), "Syntonic comma should be greater than unison");
            expect(syntonic_comma < FractionNumber(6, 5), "Syntonic comma should be less than minor third");

            // Additional complex constexpr calculation: Pythagorean comma
            // Pythagorean comma = 12 perfect fifths down 7 octaves: (3/2)^12 / 2^7
            constexpr auto twelve_fifths = perfect_fifth * perfect_fifth * perfect_fifth * perfect_fifth *
                                          perfect_fifth * perfect_fifth * perfect_fifth * perfect_fifth *
                                          perfect_fifth * perfect_fifth * perfect_fifth * perfect_fifth;
            constexpr auto seven_octaves = octave * octave * octave * octave * octave * octave * octave;
            constexpr auto pythagorean_comma = twelve_fifths / seven_octaves;

            expect(pythagorean_comma.isFraction(), "Pythagorean comma should be fractional");
            // Pythagorean comma should be slightly larger than unison (about 23.5 cents)
            expect(pythagorean_comma > FractionNumber(1, 1), "Pythagorean comma should be larger than unison");
            expect(pythagorean_comma < FractionNumber(6, 5), "Pythagorean comma should be less than minor third");

            // Test the famous musical relationship: Pythagorean comma ≈ 531441/524288
            expectWithinAbsoluteError(static_cast<double>(pythagorean_comma), 531441.0/524288.0, 1e-10);
        }

        beginTest("Conversion to semitones and cents");
        {
            FractionNumber octave(2, 1);
            FractionNumber fifth(3, 2);

            expectWithinAbsoluteError(octave.toSemitones(), 12.0, 1e-6);
            expectWithinAbsoluteError(octave.toCents(), 1200.0, 1e-6);

            // Perfect fifth is approximately 7.02 semitones
            expectWithinAbsoluteError(fifth.toSemitones(), std::log2(1.5) * 12.0, 1e-6);
            expectWithinAbsoluteError(fifth.toCents(), std::log2(1.5) * 1200.0, 1e-6);
        }

        beginTest("Assignment operators");
        {
            FractionNumber ratio(5, 4);

            ratio *= 2;  // Should become 10/4 = 5/2
            expectWithinAbsoluteError(static_cast<double>(ratio), 2.5, 1e-9);

            ratio /= 5;  // Should become 2/4 = 1/2
            expectWithinAbsoluteError(static_cast<double>(ratio), 0.5, 1e-9);
        }

        beginTest("Complex musical interval calculations");
        {
            // Test some common musical interval arithmetic
            FractionNumber majorSecond(9, 8);
            FractionNumber majorThird(5, 4);
            FractionNumber perfectFourth(4, 3);
            FractionNumber perfectFifth(3, 2);

            // Major third + minor third should equal perfect fifth
            FractionNumber minorThird = perfectFifth / majorThird; // (3/2) / (5/4) = 6/5
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
            FractionNumber zero(0, 1);
            FractionNumber negative(-3, 2);

            expectWithinAbsoluteError(static_cast<double>(zero), 0.0, 1e-9);
            expectWithinAbsoluteError(static_cast<double>(negative), -1.5, 1e-9);

            // Test multiplication with zero
            FractionNumber third(5, 4);
            auto result = third * zero;
            expectWithinAbsoluteError(static_cast<double>(result), 0.0, 1e-9);
        }
    }
};

static RationalNumberTest rationalNumberTest;