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
            expectEquals(static_cast<int>(viewModel.getCurrentKey()), static_cast<int>(Key::C));
            expectEquals(static_cast<int>(viewModel.getCurrentScale()), static_cast<int>(Scale::ScaleType::IonianOrMajor));
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
            viewModel.setCurrentKey(Key::A);
            viewModel.setCurrentScale(Scale::ScaleType::AeolianOrMinor);
            
            expectEquals(static_cast<int>(viewModel.getCurrentKey()), static_cast<int>(Key::A));
            expectEquals(static_cast<int>(viewModel.getCurrentScale()), static_cast<int>(Scale::ScaleType::AeolianOrMinor));
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
            viewModel.setCurrentKey(Key::FSharp);
            viewModel.setCurrentScale(Scale::ScaleType::IonianOrMajor);
            
            expectEquals(static_cast<int>(viewModel.getCurrentKey()), static_cast<int>(Key::FSharp));
            expectEquals(viewModel.getScaleName(), String("F# Major (Ionian)"));
            
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
            viewModel.setCurrentKey(Key::D);
            viewModel.setCurrentScale(Scale::ScaleType::Dorian);
            
            expectEquals(static_cast<int>(viewModel.getCurrentKey()), static_cast<int>(Key::D));
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
            
            auto keyNames = viewModel.getKeyNames();
            expectEquals(static_cast<int>(keyNames.size()), 12);
            expectEquals(keyNames[0], String("C"));
            expectEquals(keyNames[1], String("C#"));
            expectEquals(keyNames[9], String("A"));
            expectEquals(keyNames[11], String("B"));
            
            expectEquals(TuningViewModel::getKeyName(Key::C), String("C"));
            expectEquals(TuningViewModel::getKeyName(Key::A), String("A"));
            expectEquals(TuningViewModel::getKeyName(Key::FSharp), String("F#"));
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
            
            expectGreaterThan(static_cast<int>(scaleNames.size()), 50);  // Should have 53+ scales minus UserDefined
            expect(scaleNames.contains("Major (Ionian)"));
            expect(scaleNames.contains("Minor (Aeolian)"));
            expect(scaleNames.contains("Double Harmonic"));
            expect(scaleNames.contains("Dorian"));
            expect(!scaleNames.contains("User Defined"));  // Should not include UserDefined
        }
    }
};

static TuningViewModelTest tuningViewModelTest;