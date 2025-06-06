#include <JuceHeader.h>
#include "TuningSystem.h"

using namespace MoTool;

class CustomTuningTest : public juce::UnitTest {
public:
    CustomTuningTest() : UnitTest("CustomTuning", "MoTool") {}

    void runTest() override {
        ChipCapabilities testCaps {1773400, 16, Range<int>(1, 4096)};

        beginTest("Constructor with empty period table");
        {
            std::map<int, int> emptyTable;
            CustomTuning tuning(testCaps, emptyTable, "Empty Test");

            expectEquals(tuning.getName(), String("Empty Test (Custom, 0 notes defined)"));
            expectEquals(static_cast<int>(tuning.getType().value), static_cast<int>(TuningType::Custom));
        }

        beginTest("Constructor with simple period table");
        {
            std::map<int, int> simpleTable = {
                {60, 512},  // Middle C
                {61, 480},  // C#
                {62, 450}   // D
            };
            CustomTuning tuning(testCaps, simpleTable, "Simple Test");

            expectEquals(tuning.getName(), String("Simple Test (Custom, 3 notes defined)"));
        }

        beginTest("Constructor with vector of periods");
        {
            CustomTuning tuning(
                testCaps,
                60,
                {1000, 891, 794, 750, 668, 595, 530},
                "Vector Test Tuning"
            );

            expectEquals(tuning.getName(), String("Vector Test Tuning (Custom, 7 notes defined)"));

            const auto& periodTable = tuning.getPeriodTable();
            expectEquals(static_cast<int>(periodTable.size()), 7);

            // Test that the period table was built correctly from vector
            expectEquals(tuning.midiNoteToPeriod(60), 1000); // First note (C)
            expectEquals(tuning.midiNoteToPeriod(61), 891);  // Second note (C#)
            expectEquals(tuning.midiNoteToPeriod(62), 794);  // Third note (D)
            expectEquals(tuning.midiNoteToPeriod(66), 530);  // Last note (F#)

            // Test defined note range
            auto range = tuning.getDefinedNoteRange();
            expectEquals(range.getStart(), 60);
            expectEquals(range.getEnd(), 67); // startingNote + vector size
        }

        beginTest("Constructor with empty vector");
        {
            CustomTuning tuning(testCaps, 60, {}, "Empty Vector Test");

            const auto& periodTable = tuning.getPeriodTable();
            expectEquals(static_cast<int>(periodTable.size()), 0);

            expectEquals(tuning.getName(), String("Empty Vector Test (Custom, 0 notes defined)"));

            auto range = tuning.getDefinedNoteRange();
            expectEquals(range.getStart(), 60);
            expectEquals(range.getEnd(), 61); // startingNote + 1 when empty
        }

        beginTest("Constructor with vector - default name");
        {
            CustomTuning tuning(testCaps, 72, {512, 480, 450}); // No custom name provided

            expectEquals(tuning.getName(), String("Custom Tuning (Custom, 3 notes defined)"));

            expectEquals(tuning.midiNoteToPeriod(72), 512);
            expectEquals(tuning.midiNoteToPeriod(73), 480);
            expectEquals(tuning.midiNoteToPeriod(74), 450);
        }

        beginTest("Direct period lookup for defined notes");
        {
            CustomTuning tuning(testCaps, {
                {60, 512},  // Middle C
                {61, 480},  // C#
                {62, 450}   // D
            });

            expectEquals(tuning.midiNoteToPeriod(60), 512);
            expectEquals(tuning.midiNoteToPeriod(61), 480);
            expectEquals(tuning.midiNoteToPeriod(62), 450);
        }

        beginTest("Period to frequency conversion");
        {
            CustomTuning tuning(testCaps, {
                {60, 1000}  // Simple test case
            });

            double frequency = tuning.midiNoteToFrequency(60);
            double expectedFreq = testCaps.clockFrequency / (16.0 * 1000);  // Clock / (divider * period)

            expect(std::abs(frequency - expectedFreq) < 0.01, "Frequency calculation should be accurate");
        }

        beginTest("Octave extrapolation");
        {
            CustomTuning tuning(testCaps, {
                {60, 1000}  // Middle C (C4)
            });

            // C5 should be half the period of C4 (one octave higher)
            int period_C5 = tuning.midiNoteToPeriod(72);  // C5 = C4 + 12
            expectEquals(period_C5, 500);  // 1000 / 2

            // C6 should be quarter the period of C4 (two octaves higher)
            int period_C6 = tuning.midiNoteToPeriod(84);  // C6 = C4 + 24
            expectEquals(period_C6, 250);  // 1000 / 4

            // C3 should be double the period of C4 (one octave lower)
            int period_C3 = tuning.midiNoteToPeriod(48);  // C3 = C4 - 12
            expectEquals(period_C3, 2000);  // 1000 * 2

            // C2 should be quadruple the period of C4 (two octaves lower)
            int period_C2 = tuning.midiNoteToPeriod(36);  // C2 = C4 - 24
            expectEquals(period_C2, 4000);  // 1000 * 4
        }

        beginTest("ProTracker-style tuning table");
        {
            // Example from TuningViewModel.h
            std::map<int, int> proTrackerTable = {
                {0, 1024}, {1, 960}, {2, 896}, {3, 840}, {4, 800},
                {5, 768}, {6, 720}, {7, 680}, {8, 640}, {9, 600},
                {10, 560}, {11, 512}
            };
            CustomTuning tuning(testCaps, proTrackerTable, "ProTracker Tuning #1");

            expectEquals(tuning.getName(), String("ProTracker Tuning #1 (Custom, 12 notes defined)"));

            // Test some specific values
            expectEquals(tuning.midiNoteToPeriod(0), 1024);
            expectEquals(tuning.midiNoteToPeriod(6), 720);
            expectEquals(tuning.midiNoteToPeriod(11), 512);

            // Test octave extrapolation from defined range
            expectEquals(tuning.midiNoteToPeriod(12), 512);  // Octave of note 0: 1024/2
            expectEquals(tuning.midiNoteToPeriod(24), 256);  // Two octaves: 1024/4
        }

        beginTest("MIDI note to frequency conversion");
        {
            std::map<int, int> table = {{60, 1000}};
            CustomTuning tuning(testCaps, table);

            double frequency = tuning.midiNoteToFrequency(60);
            double expectedFreq = testCaps.clockFrequency / (16.0 * 1000);

            expect(std::abs(frequency - expectedFreq) < 0.01, "MIDI to frequency conversion should be accurate");
        }

        beginTest("Edge case - single note table");
        {
            std::map<int, int> singleNote = {{69, 850}};  // A4
            CustomTuning tuning(testCaps, singleNote, "Single Note");

            expectEquals(tuning.getName(), String("Single Note (Custom, 1 notes defined)"));
            expectEquals(tuning.midiNoteToPeriod(69), 850);

            // Test extrapolation works from single note
            expectEquals(tuning.midiNoteToPeriod(81), 425);  // A5 = A4/2
            expectEquals(tuning.midiNoteToPeriod(57), 1700); // A3 = A4*2
        }

        beginTest("isDefined method - directly defined notes");
        {
            CustomTuning tuning(testCaps, {
                {60, 1000},  // Middle C
                {61, 891},   // C#
                {62, 794}    // D
            });

            // Test directly defined notes
            expect(tuning.isDefined(60), "Note 60 should be defined");
            expect(tuning.isDefined(61), "Note 61 should be defined");
            expect(tuning.isDefined(62), "Note 62 should be defined");

            // Test undefined notes
            expect(!tuning.isDefined(59), "Note 59 should not be defined");
            expect(!tuning.isDefined(63), "Note 63 should not be defined");
        }

        beginTest("isDefined method - empty tuning");
        {
            CustomTuning tuning(testCaps, {}, "Empty Tuning");

            // Empty tuning should have no defined notes
            expect(!tuning.isDefined(60), "No notes should be defined in empty tuning");
            expect(!tuning.isDefined(72), "No notes should be defined in empty tuning");
            expect(!tuning.isDefined(48), "No notes should be defined in empty tuning");
        }
    }
};

static CustomTuningTest customTuningTest;