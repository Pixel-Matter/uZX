#include <JuceHeader.h>
#include "TuningViewModel.h"

using namespace MoTool;

class TuningViewModelTest : public juce::UnitTest {
public:
    TuningViewModelTest() : UnitTest("TuningViewModel", "MoTool") {}

    void runTest() override {
        beginTest("Scale and Key selection - C Major");
        {
            TuningViewModel viewModel;

            // Default should be C Major
            expectEquals(static_cast<int>(viewModel.getCurrentRoot()), static_cast<int>(Scale::Tonic::C));
            expectEquals(static_cast<int>(viewModel.getCurrentScaleType()), static_cast<int>(Scale::ScaleType::IonianOrMajor));
            expectEquals(viewModel.getScaleName(), String("C Major (Ionian)"));

            // Check that C Major scale notes are in scale
            auto noteNames = viewModel.getColumnNoteNames();
            expectEquals(static_cast<int>(noteNames.size()), 12);

            // C Major scale intervals: [0, 2, 4, 5, 7, 9, 11] (C, D, E, F, G, A, B)
            expect(noteNames[0].isInScale);   // C
            expect(!noteNames[1].isInScale);  // C#
            expect(noteNames[2].isInScale);   // D
            expect(!noteNames[3].isInScale);  // D#
            expect(noteNames[4].isInScale);   // E
            expect(noteNames[5].isInScale);   // F
            expect(!noteNames[6].isInScale);  // F#
            expect(noteNames[7].isInScale);   // G
            expect(!noteNames[8].isInScale);  // G#
            expect(noteNames[9].isInScale);   // A
            expect(!noteNames[10].isInScale); // A#
            expect(noteNames[11].isInScale);  // B
        }

        beginTest("Scale and Key selection - A Minor");
        {
            TuningViewModel viewModel;

            // Set to A Minor (Natural Minor = Aeolian)
            viewModel.setCurrentRoot(Scale::Tonic::A);
            viewModel.setCurrentScaleType(Scale::ScaleType::AeolianOrMinor);

            expectEquals(static_cast<int>(viewModel.getCurrentRoot()), static_cast<int>(Scale::Tonic::A));
            expectEquals(static_cast<int>(viewModel.getCurrentScaleType()), static_cast<int>(Scale::ScaleType::AeolianOrMinor));
            expectEquals(viewModel.getScaleName(), String("A Minor (Aeolian)"));

            // Check that A Minor scale notes are in scale
            auto noteNames = viewModel.getColumnNoteNames();

            // A Minor scale intervals: [0, 2, 3, 5, 7, 8, 10] transposed to A (9): [9, 11, 0, 2, 4, 5, 7] (A, B, C, D, E, F, G)
            expect(noteNames[0].isInScale);   // C -> in scale
            expect(!noteNames[1].isInScale);  // C#
            expect(noteNames[2].isInScale);   // D -> in scale
            expect(!noteNames[3].isInScale);  // D#
            expect(noteNames[4].isInScale);   // E -> in scale
            expect(noteNames[5].isInScale);   // F -> in scale
            expect(!noteNames[6].isInScale);  // F#
            expect(noteNames[7].isInScale);   // G -> in scale
            expect(!noteNames[8].isInScale);  // G#
            expect(noteNames[9].isInScale);   // A -> in scale (root)
            expect(!noteNames[10].isInScale); // A#
            expect(noteNames[11].isInScale);  // B -> in scale
        }

        beginTest("Scale and Key selection - F# Major");
        {
            TuningViewModel viewModel;

            // Set to F# Major
            viewModel.setCurrentRoot(Scale::Tonic::FSharp);
            viewModel.setCurrentScaleType(Scale::ScaleType::IonianOrMajor);

            expectEquals(static_cast<int>(viewModel.getCurrentRoot()), static_cast<int>(Scale::Tonic::FSharp));
            expectEquals(viewModel.getScaleName(), String::fromUTF8("F♯ Major (Ionian)"));

            // Check that F# Major scale notes are in scale
            auto noteNames = viewModel.getColumnNoteNames();

            // F# Major scale intervals: [0, 2, 4, 5, 7, 9, 11] transposed to F# (6): [6, 8, 10, 11, 1, 3, 5] (F#, G#, A#, B, C#, D#, F)
            expect(!noteNames[0].isInScale);  // C
            expect(noteNames[1].isInScale);   // C# -> in scale
            expect(!noteNames[2].isInScale);  // D
            expect(noteNames[3].isInScale);   // D# -> in scale
            expect(!noteNames[4].isInScale);  // E
            expect(noteNames[5].isInScale);   // F -> in scale (as E#)
            expect(noteNames[6].isInScale);   // F# -> in scale (root)
            expect(!noteNames[7].isInScale);  // G
            expect(noteNames[8].isInScale);   // G# -> in scale
            expect(!noteNames[9].isInScale);  // A
            expect(noteNames[10].isInScale);  // A# -> in scale
            expect(noteNames[11].isInScale);  // B -> in scale
        }

        beginTest("Scale and Key selection - D Dorian");
        {
            TuningViewModel viewModel;

            // Set to D Dorian
            viewModel.setCurrentRoot(Scale::Tonic::D);
            viewModel.setCurrentScaleType(Scale::ScaleType::Dorian);

            expectEquals(static_cast<int>(viewModel.getCurrentRoot()), static_cast<int>(Scale::Tonic::D));
            expectEquals(viewModel.getScaleName(), String("D Dorian"));

            // Check that D Dorian scale notes are in scale
            auto noteNames = viewModel.getColumnNoteNames();

            // D Dorian scale intervals: [0, 2, 3, 5, 7, 9, 10] transposed to D (2): [2, 4, 5, 7, 9, 11, 0] (D, E, F, G, A, B, C)
            expect(noteNames[0].isInScale);   // C -> in scale
            expect(!noteNames[1].isInScale);  // C#
            expect(noteNames[2].isInScale);   // D -> in scale (root)
            expect(!noteNames[3].isInScale);  // D#
            expect(noteNames[4].isInScale);   // E -> in scale
            expect(noteNames[5].isInScale);   // F -> in scale
            expect(!noteNames[6].isInScale);  // F#
            expect(noteNames[7].isInScale);   // G -> in scale
            expect(!noteNames[8].isInScale);  // G#
            expect(noteNames[9].isInScale);   // A -> in scale
            expect(!noteNames[10].isInScale); // A#
            expect(noteNames[11].isInScale);  // B -> in scale
        }

        beginTest("Key names functionality");
        {
            TuningViewModel viewModel;

            auto keyNames = Scale::getAllKeyNames();
            expectEquals(static_cast<int>(keyNames.size()), 12);
            expectEquals(keyNames[0], String("C"));
            expectEquals(keyNames[1], String::fromUTF8("C♯"));
            expectEquals(keyNames[9], String("A"));
            expectEquals(keyNames[11], String("B"));

            expectEquals(Scale::getKeyName(Scale::Tonic::C), String("C"));
            expectEquals(Scale::getKeyName(Scale::Tonic::A), String("A"));
            expectEquals(Scale::getKeyName(Scale::Tonic::FSharp), String::fromUTF8("F♯"));
        }

        beginTest("Scale type names functionality");
        {
            TuningViewModel viewModel;

            auto scaleNames = viewModel.getScaleTypeNames();

            // Debug output
            DBG("Scale names count: " << scaleNames.size());
            if (scaleNames.size() > 0) {
                DBG("First few scale names:");
                for (int i = 0; i < jmin(5, scaleNames.size()); ++i) {
                    DBG("  " << i << ": " << scaleNames[i]);
                }
            }

            expectGreaterThan(static_cast<int>(scaleNames.size()), 45);  // Should have 45+ scales minus UserDefined
            expect(scaleNames.contains("Major (Ionian)"));
            expect(scaleNames.contains("Minor (Aeolian)"));
            expect(scaleNames.contains("Double Harmonic"));
            expect(scaleNames.contains("Dorian"));
            expect(!scaleNames.contains("User Defined"));  // Should not include UserDefined
        }

        beginTest("CSV export functionality");
        {
            TuningViewModel viewModel;

            // Set up test configuration
            viewModel.setCurrentScaleType(Scale::ScaleType::IonianOrMajor);
            viewModel.setCurrentRoot(Scale::Tonic::C);
            viewModel.setA4Frequency(440.0);

            // Export to CSV
            String csvData = viewModel.exportToCSV();

            // Basic validation - should not be empty
            expect(csvData.isNotEmpty(), "CSV data should not be empty");

            // Split into lines for analysis
            auto lines = StringArray::fromLines(csvData);
            expectGreaterThan(static_cast<int>(lines.size()), 10, "Should have multiple lines of data");

            // Check metadata header
            // expect(lines[0].startsWith("# Tuning System Export"), "Should start with metadata header");
            // expect(lines[1].contains("# Tuning:"), "Should contain tuning name");
            // expect(lines[2].contains("# Type:"), "Should contain tuning type");
            // expect(lines[3].contains("# Scale: C Major"), "Should contain scale info");
            // expect(lines[4].contains("# Clock Frequency:"), "Should contain clock frequency");
            // expect(lines[5].contains("# A4 Frequency: 440"), "Should contain A4 frequency");

            // Find data header line
            int dataHeaderIndex = -1;
            for (int i = 0; i < lines.size(); ++i) {
                if (lines[i].startsWith("MIDI,Note,Period")) {
                    dataHeaderIndex = i;
                    break;
                }
            }
            // expect(dataHeaderIndex >= 0, "Should contain CSV data header");

            // Check data header format
            String expectedHeader = "MIDI,Note,Period";
            expectEquals(lines[dataHeaderIndex], expectedHeader, "Data header should match expected format");

            // Check that we have data rows after header
            expectGreaterThan(static_cast<int>(lines.size()), dataHeaderIndex + 10, "Should have data rows after header");

            // Validate a few data rows format
            if (dataHeaderIndex + 1 < lines.size()) {
                String firstDataRow = lines[dataHeaderIndex + 1];
                auto fields = StringArray::fromTokens(firstDataRow, ",", "");
                expectEquals(static_cast<int>(fields.size()), 3, "Each data row should have 3 fields");

                // First field should be MIDI note, second should be note name
                expect(fields[0].containsOnly("0123456789") || fields[0] == "N/A", "First field should be MIDI note number");
                expect(fields[1].contains("C0"), "Second field should contain note name C0");

                // Check that note names use ASCII characters
                expect(!firstDataRow.contains(String::fromUTF8("♯")), "Should not contain Unicode sharp");
                expect(!firstDataRow.contains(String::fromUTF8("♭")), "Should not contain Unicode flat");
            }

            // Test with different scale
            viewModel.setCurrentScaleType(Scale::ScaleType::MinorPentatonic);
            viewModel.setCurrentRoot(Scale::Tonic::A);

            String csvData2 = viewModel.exportToCSV();
            expect(csvData2.isNotEmpty(), "CSV data should not be empty for different scale");

            // Test ASCII character conversion - should only use sharps, no flats
            String csvData3 = viewModel.exportToCSV();
            expect(!csvData3.contains(String::fromUTF8("♯")), "Should not contain Unicode sharp character");
            expect(!csvData3.contains(String::fromUTF8("♭")), "Should not contain Unicode flat character");
            expect(csvData3.contains("#"), "Should contain ASCII sharp character");

            // Verify only sharps are used, no flats
            expect(!csvData3.contains("Db"), "Should not contain Db (use C# instead)");
            expect(!csvData3.contains("Eb"), "Should not contain Eb (use D# instead)");
            expect(!csvData3.contains("Gb"), "Should not contain Gb (use F# instead)");
            expect(!csvData3.contains("Ab"), "Should not contain Ab (use G# instead)");
            expect(!csvData3.contains("Bb"), "Should not contain Bb (use A# instead)");

            // Verify sharps are present
            expect(csvData3.contains("C#"), "Should contain C#");
            expect(csvData3.contains("D#"), "Should contain D#");
            expect(csvData3.contains("F#"), "Should contain F#");
            expect(csvData3.contains("G#"), "Should contain G#");
            expect(csvData3.contains("A#"), "Should contain A#");
        }

        beginTest("Default export filename generation");
        {
            TuningViewModel viewModel;

            // Test default filename with C Major
            viewModel.setCurrentScaleType(Scale::ScaleType::IonianOrMajor);
            viewModel.setCurrentRoot(Scale::Tonic::C);
            viewModel.setA4Frequency(440.0);

            String filename1 = viewModel.getDefaultExportFilename();
            expect(filename1.isNotEmpty(), "Filename should not be empty");
            expect(filename1.endsWith(".csv"), "Filename should end with .csv");
            expect(filename1.contains("C Major"), "Filename should contain scale info");

            // Test with different scale and non-standard A4
            viewModel.setCurrentScaleType(Scale::ScaleType::MinorPentatonic);
            viewModel.setCurrentRoot(Scale::Tonic::A);
            viewModel.setA4Frequency(442.0);

            String filename2 = viewModel.getDefaultExportFilename();
            expect(filename2.contains("A Minor Pentatonic"), "Filename should contain new scale");
            expect(filename2.contains("A4=442"), "Filename should contain non-standard A4 frequency");
            expect(filename2 != filename1, "Filename should change with different settings");

            // Test filename safety (no invalid characters)
            expect(!filename2.contains("/"), "Filename should not contain forward slash");
            expect(!filename2.contains("\\"), "Filename should not contain backslash");
            expect(!filename2.contains(":"), "Filename should not contain colon");
            expect(!filename2.contains("*"), "Filename should not contain asterisk");
            expect(!filename2.contains("?"), "Filename should not contain question mark");
        }
    }
};

static TuningViewModelTest tuningViewModelTest;