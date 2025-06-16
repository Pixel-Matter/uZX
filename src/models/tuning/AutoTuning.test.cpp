#include <JuceHeader.h>

#include "TuningSystem.h"

using namespace MoTool;

class AutoTuningTest : public juce::UnitTest {
public:
    AutoTuningTest() : UnitTest("AutoTuning", "MoTool") {}

    void runTest() override {
        ChipCapabilities testCaps {16, Range<int>(1, 4096)};
        double testClockFreq = 1773400.0; // ZX Spectrum clock frequency

        beginTest("Constructor with equal temperament reference");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            expectEquals(tuning.getA4Frequency(), 440.0, "A4 frequency should be 440.0 Hz");
            expectEquals(tuning.getClockFrequency(), testClockFreq, "Clock frequency should be set correctly");
            expectEquals(static_cast<int>(tuning.getType().value), static_cast<int>(TuningType::AutoTuning));
        }

        beginTest("getName method");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            String name = tuning.getName();
            expect(name.contains("Equal Temperament"), "Name should contain reference tuning type");
            expect(name.contains("auto tuning"), "Name should contain 'auto tuning'");
            expect(name.contains("1.773"), "Name should contain clock frequency in MHz");
            expect(name.contains("440.00"), "Name should contain A4 frequency");
        }

        beginTest("MIDI note to period conversion - A4");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            int period = tuning.midiNoteToPeriod(69.0); // A4 = 440 Hz
            
            // Expected period: clockFreq / (divider * frequency) = 1773400 / (16 * 440) ≈ 252
            int expectedPeriod = static_cast<int>(std::round(testClockFreq / (testCaps.divider * 440.0)));
            expectEquals(period, expectedPeriod, "A4 period should be calculated correctly");
        }

        beginTest("MIDI note to period conversion - Middle C");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            int period = tuning.midiNoteToPeriod(60.0); // Middle C
            
            // Calculate expected frequency for Middle C
            double middleCFreq = 440.0 * std::pow(2.0, (60.0 - 69.0) / 12.0);
            int expectedPeriod = static_cast<int>(std::round(testClockFreq / (testCaps.divider * middleCFreq)));
            expectEquals(period, expectedPeriod, "Middle C period should be calculated correctly");
        }

        beginTest("Period to MIDI note conversion");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            
            // Test with A4 period
            int a4Period = tuning.midiNoteToPeriod(69.0);
            double roundTripNote = tuning.periodToMidiNote(a4Period);
            expect(std::abs(roundTripNote - 69.0) < 0.1, "Round-trip conversion should be accurate for A4");
        }

        beginTest("Octave relationships in periods");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            
            int a4Period = tuning.midiNoteToPeriod(69.0); // A4
            int a5Period = tuning.midiNoteToPeriod(81.0); // A5 (one octave higher)
            int a3Period = tuning.midiNoteToPeriod(57.0); // A3 (one octave lower)
            
            // A5 should have roughly half the period of A4 (double frequency)
            expect(std::abs(a5Period - a4Period / 2) <= 1, "A5 period should be roughly half of A4");
            
            // A3 should have roughly double the period of A4 (half frequency)  
            expect(std::abs(a3Period - a4Period * 2) <= 2, "A3 period should be roughly double A4");
        }

        beginTest("Period range limiting");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            
            // Test very low note that might exceed max period
            int lowPeriod = tuning.midiNoteToPeriod(0.0); // Very low C
            expect(lowPeriod >= testCaps.registerRange.getStart(), "Period should not be below minimum");
            expect(lowPeriod < testCaps.registerRange.getEnd(), "Period should not exceed maximum");
            
            // Test very high note that might go below min period
            int highPeriod = tuning.midiNoteToPeriod(127.0); // Very high G
            expect(highPeriod >= testCaps.registerRange.getStart(), "Period should not be below minimum");
            expect(highPeriod < testCaps.registerRange.getEnd(), "Period should not exceed maximum");
        }

        beginTest("Frequency conversion methods");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            
            // Test midiNoteToFrequency
            double a4Freq = tuning.midiNoteToFrequency(69.0);
            expect(std::abs(a4Freq - 440.0) < 1.0, "A4 frequency should be close to 440 Hz");
            
            // Test frequencyToMidiNote
            double roundTripNote = tuning.frequencyToMidiNote(440.0);
            expect(std::abs(roundTripNote - 69.0) < 0.5, "440 Hz should convert back to approximately MIDI note 69");
        }

        beginTest("isDefined method - delegates to reference tuning");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            
            // Equal temperament defines all notes
            expect(tuning.isDefined(0), "MIDI note 0 should be defined");
            expect(tuning.isDefined(60), "MIDI note 60 should be defined");
            expect(tuning.isDefined(69), "MIDI note 69 should be defined");
            expect(tuning.isDefined(127), "MIDI note 127 should be defined");
        }

        beginTest("A4 frequency setter");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            tuning.setA4Frequency(432.0);
            expectEquals(tuning.getA4Frequency(), 432.0, "A4 frequency should be updated");
            
            // Verify that period calculations use the new A4 frequency
            int newA4Period = tuning.midiNoteToPeriod(69.0);
            int expectedPeriod = static_cast<int>(std::round(testClockFreq / (testCaps.divider * 432.0)));
            expectEquals(newA4Period, expectedPeriod, "A4 period should reflect new frequency");
        }

        beginTest("Clock frequency setter");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            double newClockFreq = 1750000.0;
            tuning.setClockFrequency(newClockFreq);
            expectEquals(tuning.getClockFrequency(), newClockFreq, "Clock frequency should be updated");
            
            // Verify that period calculations use the new clock frequency
            int newA4Period = tuning.midiNoteToPeriod(69.0);
            int expectedPeriod = static_cast<int>(std::round(newClockFreq / (testCaps.divider * 440.0)));
            expectEquals(newA4Period, expectedPeriod, "A4 period should reflect new clock frequency");
        }

        beginTest("Offtune calculation");
        {
            AutoTuning tuning(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            
            // For auto tuning with equal temperament reference, offtune should be minimal
            double offtune = tuning.getOfftune(69.0); // A4
            expect(std::abs(offtune) < 5.0, "Offtune for A4 should be minimal (< 5 cents)");
            
            // Test other notes
            double offtuneC = tuning.getOfftune(60.0); // Middle C
            expect(std::abs(offtuneC) < 5.0, "Offtune for Middle C should be minimal");
        }

        beginTest("Different reference tuning A4 frequencies");
        {
            // Test with A4 = 432 Hz
            AutoTuning tuning432(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(432.0));
            int period432 = tuning432.midiNoteToPeriod(69.0);
            
            // Test with A4 = 440 Hz
            AutoTuning tuning440(testCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            int period440 = tuning440.midiNoteToPeriod(69.0);
            
            expect(period432 > period440, "A4 at 432 Hz should have larger period than at 440 Hz");
        }

        beginTest("Edge cases with chip capabilities");
        {
            // Test with different chip capabilities
            ChipCapabilities narrowCaps {8, Range<int>(10, 1000)};
            AutoTuning narrowTuning(narrowCaps, testClockFreq, std::make_unique<EqualTemperamentTuning>(440.0));
            
            int period = narrowTuning.midiNoteToPeriod(69.0);
            expect(period >= narrowCaps.registerRange.getStart(), "Period should respect minimum range");
            expect(period < narrowCaps.registerRange.getEnd(), "Period should respect maximum range");
        }
    }
};

static AutoTuningTest autoTuningTest;