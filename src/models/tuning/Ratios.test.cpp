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