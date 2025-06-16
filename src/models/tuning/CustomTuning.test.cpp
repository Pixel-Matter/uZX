#include <JuceHeader.h>

#include "TuningSystem.h"
#include "TuningTables.h"


using namespace MoTool;

class CustomTuningTest : public juce::UnitTest {
public:
    CustomTuningTest() : UnitTest("CustomTuning", "MoTool") {}

    void runTest() override {
        ChipCapabilities testCaps {16, Range<int>(1, 4096)};

        beginTest("Constructor with empty period table");
        {
            std::map<int, int> emptyTable;
            CustomTuningTable tuning(testCaps, emptyTable, "Empty Test", 1773400, 440.0);

            expectEquals(tuning.getName(), String("Empty Test (Custom, defined notes C-1-C-1, A4 = 440.00Hz)"));
            expectEquals(static_cast<int>(tuning.getType().value), static_cast<int>(TuningType::CustomTable));
        }

        beginTest("Constructor with simple period table");
        {
            std::map<int, int> simpleTable = {
                {60, 512},  // Middle C
                {61, 480},  // C#
                {62, 450}   // D
            };
            CustomTuningTable tuning(testCaps, simpleTable, "Simple Test", 1773400, 440.0);

            expectEquals(tuning.getName(), String("Simple Test (Custom, defined notes C4-D4, A4 = 440.00Hz)"));
        }

        beginTest("Constructor with vector of periods");
        {
            CustomTuningTable tuning(
                testCaps,
                60,
                {1000, 891, 794, 750, 668, 595, 530},
                "Vector Test Tuning",
                1773400,
                440.0
            );

            expectEquals(tuning.getName(), String("Vector Test Tuning (Custom, defined notes C4-F#4, A4 = 440.00Hz)"));

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
            CustomTuningTable tuning(testCaps, 60, {}, "Empty Vector Test", 1773400, 440.0);

            const auto& periodTable = tuning.getPeriodTable();
            expectEquals(static_cast<int>(periodTable.size()), 0);

            expectEquals(tuning.getName(), String("Empty Vector Test (Custom, defined notes C4-C4, A4 = 440.00Hz)"));

            auto range = tuning.getDefinedNoteRange();
            expectEquals(range.getStart(), 60);
            expectEquals(range.getEnd(), 61); // startingNote + 1 when empty
        }

        beginTest("Direct period lookup for defined notes");
        {
            CustomTuningTable tuning(testCaps, {
                {60, 512},  // Middle C
                {61, 480},  // C#
                {62, 450}   // D
            },
            "Direct Lookup Test", 1773400, 440.0);

            expectEquals(tuning.midiNoteToPeriod(60), 512);
            expectEquals(tuning.midiNoteToPeriod(61), 480);
            expectEquals(tuning.midiNoteToPeriod(62), 450);
        }

        beginTest("Period to frequency conversion");
        {
            CustomTuningTable tuning(testCaps, {
                {60, 1000}  // Simple test case
            }, "Frequency Conversion Test", 1773400, 440.0);

            double frequency = tuning.midiNoteToFrequency(60);
            double expectedFreq = 1773400 / (16.0 * 1000);  // Clock / (divider * period)

            expect(std::abs(frequency - expectedFreq) < 0.01, "Frequency calculation should be accurate");
        }

        beginTest("Octave extrapolation");
        {
            CustomTuningTable tuning(testCaps, {
                {60, 1000}  // Middle C (C4)
            }, "Octave Extrapolation Test", 1773400, 440.0);

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
            CustomTuningTable tuning(testCaps, proTrackerTable, "ProTracker Tuning #1", 1773400, 440.0);

            expectEquals(tuning.getName(), String("ProTracker Tuning #1 (Custom, defined notes C-1-B-1, A4 = 440.00Hz)"));

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
            CustomTuningTable tuning(testCaps, table, "Frequency Conversion Test", 1773400, 440.0);

            double frequency = tuning.midiNoteToFrequency(60);
            double expectedFreq = 1773400 / (16.0 * 1000);

            expect(std::abs(frequency - expectedFreq) < 0.01, "MIDI to frequency conversion should be accurate");
        }

        beginTest("Edge case - single note table");
        {
            std::map<int, int> singleNote = {{69, 850}};  // A4
            CustomTuningTable tuning(testCaps, singleNote, "Single Note", 1773400, 440.0);

            expectEquals(tuning.getName(), String("Single Note (Custom, defined notes A4-A4, A4 = 440.00Hz)"));
            expectEquals(tuning.midiNoteToPeriod(69), 850);

            // Test extrapolation works from single note
            expectEquals(tuning.midiNoteToPeriod(81), 425);  // A5 = A4/2
            expectEquals(tuning.midiNoteToPeriod(57), 1700); // A3 = A4*2
        }

        beginTest("isDefined method - directly defined notes");
        {
            CustomTuningTable tuning(testCaps, {
                {60, 1000},  // Middle C
                {61, 891},   // C#
                {62, 794}    // D
            }, "Direct Lookup Test", 1773400, 440.0);

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
            CustomTuningTable tuning(testCaps, {}, "Empty Tuning", 1773400, 440.0);

            // Empty tuning should have no defined notes
            expect(!tuning.isDefined(60), "No notes should be defined in empty tuning");
            expect(!tuning.isDefined(72), "No notes should be defined in empty tuning");
            expect(!tuning.isDefined(48), "No notes should be defined in empty tuning");
        }

        beginTest("ProTracker 96-note table starting from MIDI 12");
        {
            // Test with actual ProTracker data from TuningViewModel.h (PT3NoteTable_ST)
            // Starting at MIDI note 12 (C0) for full ProTracker compatibility
            CustomTuningTable tuning(
                testCaps,
                12, // Starting at MIDI note 12 (C0)
                {
                    // First 12 notes (C0-B0): Testing with smaller subset
                    3832, 3600, 3424, 3200, 3032, 2856, 2696, 2544, 2400, 2272, 2136, 2016
                },
                "ProTracker Test", 1773400, 440.0
            );

            expectEquals(tuning.getName(), String("ProTracker Test (Custom, defined notes C0-B0, A4 = 440.00Hz)"));

            // Test specific period lookups
            expectEquals(tuning.midiNoteToPeriod(12), 3832); // C0 ($0EF8)
            expectEquals(tuning.midiNoteToPeriod(13), 3600); // C#0 ($0E10)
            expectEquals(tuning.midiNoteToPeriod(14), 3424); // D0 ($0D60)
            expectEquals(tuning.midiNoteToPeriod(23), 2016); // B0 ($07E0)

            // Test that all notes in the range are defined
            for (int note = 12; note < 24; ++note) {
                expect(tuning.isDefined(note), "MIDI note " + String(note) + " should be defined");
            }

            // Test that notes outside the range are not directly defined
            expect(!tuning.isDefined(11), "MIDI note 11 should not be defined");
            expect(!tuning.isDefined(24), "MIDI note 24 should not be defined");

            // Test octave extrapolation works
            int period_C1 = tuning.midiNoteToPeriod(24); // One octave higher
            expectEquals(period_C1, 1916); // Should be 3832 / 2

            int period_C_minus1 = tuning.midiNoteToPeriod(0); // One octave lower
            expectEquals(period_C_minus1, 4095); // Should be clamped to chip range maximum
        }

        beginTest("ProTracker full 96-note table construction");
        {
            // Test construction with the complete 96-note ProTracker table
            std::vector<int> fullProTrackerTable = {
                // Octave 1 (C1-B1): $0EF8-$07E0
                3832, 3600, 3424, 3200, 3032, 2856, 2696, 2544, 2400, 2272, 2136, 2016,
                // Octave 2 (C2-B2): $077C-$03FD
                1916, 1800, 1712, 1600, 1516, 1428, 1348, 1272, 1200, 1136, 1068, 1021,
                // Octave 3 (C3-B3): $03BE-$01F8
                958, 900, 856, 800, 758, 714, 674, 636, 600, 568, 534, 504,
                // Octave 4 (C4-B4): $01DF-$00FC
                479, 450, 428, 400, 379, 357, 337, 318, 300, 284, 266, 252,
                // Octave 5 (C5-B5): $00EF-$007E
                239, 225, 214, 200, 189, 178, 168, 159, 150, 142, 133, 126,
                // Octave 6 (C6-B6): $0077-$003F
                119, 112, 107, 100, 94, 89, 84, 79, 75, 71, 66, 63,
                // Octave 7 (C7-B7): $003B-$001F
                59, 56, 53, 50, 47, 44, 42, 39, 37, 35, 33, 31,
                // Octave 8 (C8-B8): $001D-$000F
                29, 28, 26, 25, 23, 22, 21, 19, 18, 17, 16, 15
            };

            CustomTuningTable tuning(testCaps, 24, fullProTrackerTable, "Full ProTracker", 1773400, 440.0); // Start at C1 (MIDI 24)

            // Verify the table size
            expectEquals(static_cast<int>(fullProTrackerTable.size()), 96);
            expectEquals(static_cast<int>(tuning.getPeriodTable().size()), 96);

            // Test a few key values from different octaves
            expectEquals(tuning.midiNoteToPeriod(24), 3832);  // C1 (first value)
            expectEquals(tuning.midiNoteToPeriod(36), 1916);  // C2 (second octave start)
            expectEquals(tuning.midiNoteToPeriod(48), 958);   // C3 (third octave start)
            expectEquals(tuning.midiNoteToPeriod(119), 15);   // Last note (B8)

            // Test range boundaries
            auto range = tuning.getDefinedNoteRange();
            expectEquals(range.getStart(), 24);              // C1
            expectEquals(range.getEnd(), 120);               // 24 + 96 = 120 (one past last note)
        }

        beginTest("ProTracker ASM table (table 2)");
        {
            // Test with the third ProTracker table (PT3NoteTable_ASM)
            CustomTuningTable tuning(
                testCaps,
                24, // Starting at MIDI note 24 (C1)
                {
                    // First octave from ASM table: $0D10-$06EC
                    3344, 3157, 2980, 2812, 2655, 2506, 2365, 2232, 2107, 1989, 1877, 1772
                },
                "ProTracker ASM Test", 1773400, 440.0
            );

            expectEquals(tuning.getName(), String("ProTracker ASM Test (Custom, defined notes C1-B1, A4 = 440.00Hz)"));

            // Test specific period lookups from ASM table
            expectEquals(tuning.midiNoteToPeriod(24), 3344); // C1 ($0D10)
            expectEquals(tuning.midiNoteToPeriod(25), 3157); // C#1 ($0C55)
            expectEquals(tuning.midiNoteToPeriod(26), 2980); // D1 ($0BA4)
            expectEquals(tuning.midiNoteToPeriod(35), 1772); // B1 ($06EC)

            // Test that all notes in the range are defined
            for (int note = 24; note < 36; ++note) {
                expect(tuning.isDefined(note), "MIDI note " + String(note) + " should be defined in ASM table");
            }

            // Test octave extrapolation works correctly
            int period_C2 = tuning.midiNoteToPeriod(36); // One octave higher
            expectEquals(period_C2, 1672); // Should be 3344 / 2
        }
    }
};

static CustomTuningTest customTuningTest;