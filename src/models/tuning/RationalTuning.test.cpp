#include <JuceHeader.h>

#include "TemperamentSystem.h"
#include "Ratios.h"

using namespace MoTool;

class RationalTuningTest : public juce::UnitTest {
public:
    RationalTuningTest() : UnitTest("RationalTuning", "MoTool") {}

    void runTest() override {
        constexpr std::array<FractionNumber, 12> justIntonationRatios {
            FractionNumber {1, 1},   // Unison          C
            FractionNumber {16, 15}, // Minor second    C#
            FractionNumber {9, 8},   // Major second    D
            FractionNumber {6, 5},   // Minor third     D#
            FractionNumber {5, 4},   // Major third     E
            FractionNumber {4, 3},   // Perfect fourth  F
            FractionNumber {45, 32}, // Triton          F#
            FractionNumber {3, 2},   // Perfect fifth   G
            FractionNumber {8, 5},   // Minor sixth     G#
            FractionNumber {5, 3},   // Major sixth     A
            FractionNumber {16, 9},  // Minor seventh   A#
            FractionNumber {15, 8}   // Major seventh   B
        };
        auto scale = Scale(Scale::ScaleType::AeolianOrMinor);

        beginTest("Constructor with 7-limit tuning numbers");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::C, &scale};

            expectEquals(tuning.getA4Frequency(), 440.0, "Default A4 frequency should be 440.0 Hz");
            expectEquals(static_cast<int>(tuning.getType().value), static_cast<int>(TemperamentType::CustomRational), "Tuning type should be CustomRational, got " + String(tuning.getType().getLabel().data()));

            expectEquals(static_cast<int>(tuning.getScale()->getType()), static_cast<int>(Scale::ScaleType::AeolianOrMinor),
                         "Scale type should be AeolianOrMinor, got " + String(tuning.getScale()->getType().getLabel().data()));
        }

        beginTest("Tonic to frequency conversion - A4 = 440.0 Hz");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale};

            expectWithinAbsoluteError(tuning.getTonicFrequency(4), 440.0, 1e-6, "Tonic should be A4 = 440.0 Hz");
            expectWithinAbsoluteError(tuning.getTonicFrequency(5), 880.0, 1e-6, "Tonic should be A5 = 880.0 Hz");
            expectWithinAbsoluteError(tuning.getTonicFrequency(3), 220.0, 1e-6, "Tonic should be A3 = 220.0 Hz");
        }

        beginTest("Tonic to frequency conversion - A4 = 432.0 Hz");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 432.0};

            expectWithinAbsoluteError(tuning.getTonicFrequency(4), 432.0, 1e-6, "Tonic should be A4 = 432.0 Hz");
        }

        beginTest("Tonic to frequency conversion - A4 = 440.0 Hz, tonic is B");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::B, &scale};

            auto frequency = tuning.getTonicFrequency(4);
            // A->B is WT, ratio is 9:8, 440 * 9:8 = tonic
            expectWithinAbsoluteError(frequency, 495.0, 1e-6, "Tonic B4 should be 495.0 Hz, got " + String(frequency));
        }

        beginTest("Tonic to frequency conversion - A4 = 440.0 Hz, tonic is C");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::C, &scale};

            auto frequency = tuning.getTonicFrequency(4);
            // C->A ratio is 5:3, tonic * 5:3 = 440, tonic = 440 * 3 / 5 = 264.0
            expectWithinAbsoluteError(frequency, 264.0, 1e-6, "Tonic C4 should be 264.0 Hz, got " + String(frequency));
        }

        beginTest("Tonic to frequency conversion - A4 = 440.0 Hz, tonic is D#");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::DSharp, &scale};

            auto frequency = tuning.getTonicFrequency(4);
            // ratio is 45:32, tonic * 45:32 = 440, tonic = 440 * 32 / 45 = 312.8888
            expectWithinAbsoluteError(frequency, 312.8888, 1e-3, "Tonic D#4 should be 312.8888 Hz, got " + String(frequency));
        }

        beginTest("MIDI note to frequency conversion - A4, tonic is A");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale};

            double frequency = tuning.midiNoteToFrequency(69); // A4
            expectWithinAbsoluteError(frequency, 440.0, 1e-6, "A4 (MIDI 69) should be 440.0 Hz, got " + String(frequency));
        }

        beginTest("MIDI note to frequency conversion - C4, tonic is C");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::C, &scale};

            double frequency = tuning.midiNoteToFrequency(60); // C4
            // C to A is 3:5
            expectWithinAbsoluteError(frequency, 440.0 / 5 * 3, 1e-6, "C4 (MIDI 60) should be " + String(440.0 / 5 * 3) + " Hz, got " + String(frequency));
        }

        beginTest("MIDI note to frequency conversion - C3, tonic is C");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::C, &scale};

            double frequency = tuning.midiNoteToFrequency(48); // C3
            // C to A is 3:5
            expectWithinAbsoluteError(frequency, 220.0 / 5 * 3, 1e-6, "C3 (MIDI 48) should be " + String(220.0 / 5 * 3) + " Hz, got " + String(frequency));
        }

        beginTest("MIDI note to frequency conversion - A4, tonic is C");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::C, &scale};

            double frequency = tuning.midiNoteToFrequency(69); // A4
            expectWithinAbsoluteError(frequency, 440.0, 1e-6, "A4 (MIDI 69) should be 440.0 Hz, got " + String(frequency));
        }

        beginTest("MIDI note to frequency conversion - A4, any tonic");
        {
            for (int i = 0; i < 12; i++) {
                auto tonic = static_cast<Scale::Key>(i);
                RationalTuning tuning {justIntonationRatios, tonic, &scale};
                DBG("\n\n======================== " << i << " =============================");
                double frequency = tuning.midiNoteToFrequency(69); // A4
                expectWithinAbsoluteError(frequency, 440.0, 1e-6, "A4 (MIDI 69) should be 440.0 Hz with tonic " + String(i) + ", got " + String(frequency));
            }
        }

        beginTest("MIDI note to frequency conversion - Middle C");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};
            double frequency = tuning.midiNoteToFrequency(60); // Middle C
            // In just intonation, C to A is major sixth (5:3), so C = A / (5/3) = A * (3/5)
            double expectedFreq = 440.0 * 3.0 / 5.0;
            expectWithinAbsoluteError(frequency, expectedFreq, 1e-6, "Middle C (MIDI 60) frequency should be correct");
        }

        beginTest("Fractional MIDI note to frequency conversion - Middle C+");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};
            double frequency = tuning.midiNoteToFrequency(60.5); // Middle C plus quarter tone
            // In just intonation, C to A is major sixth (5:3), so C = A / (5/3) = A * (3/5)
            double expectedFreq1 = 440.0 * 3.0 / 5.0;
            double expectedFreq2 = 440.0 * 3.0 / 5.0 * 16.0 / 15.0;
            expect(frequency > expectedFreq1 && frequency < expectedFreq2,
                   "Middle C+ (MIDI 60.5) frequency should be between " + String(expectedFreq1) + " and " + String(expectedFreq2) + ", got " + String(frequency));
        }

        beginTest("MIDI note to frequency conversion - octave relationships");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};
            double a4_freq = tuning.midiNoteToFrequency(69); // A4
            double a5_freq = tuning.midiNoteToFrequency(81); // A5 (one octave higher)
            double a3_freq = tuning.midiNoteToFrequency(57); // A3 (one octave lower)

            expectWithinAbsoluteError(a5_freq, a4_freq * 2.0, 1e-6, "A5 should be exactly double A4 frequency");
            expectWithinAbsoluteError(a3_freq, a4_freq / 2.0, 1e-6, "A3 should be exactly half A4 frequency");
        }

        beginTest("Frequency to MIDI note conversion - A4");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};
            int a4lower = tuning.frequencyToNearestMidiNote(450.0, TemperamentSystem::NoteSearch::NextLower);
            expectEquals(a4lower, 69, "450.0 Hz should convert to MIDI note 69 (A4)");

            int a4upper = tuning.frequencyToNearestMidiNote(450.0, TemperamentSystem::NoteSearch::NextHigher);
            expectEquals(a4upper, 70, "450.0 Hz should convert to MIDI note 70 (A4)");

            int a4near1 = tuning.frequencyToNearestMidiNote(450.0, TemperamentSystem::NoteSearch::Nearest);
            expectEquals(a4near1, 69, "450.0 Hz should convert to MIDI note 69 (A4)");

            int a4near2 = tuning.frequencyToNearestMidiNote(460.0, TemperamentSystem::NoteSearch::Nearest);
            expectEquals(a4near2, 70, "460.0 Hz should convert to MIDI note 70 (A4)");
        }

        beginTest("Frequency to MIDI note conversion - Middle C");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};
            double middleCFreq = 440.0 * 3.0 / 5.0; // C to A is 5:3 in just intonation
            double midiNote = tuning.frequencyToMidiNote(middleCFreq);
            expectWithinAbsoluteError(midiNote, 60.0, 1e-3, "Middle C frequency should convert to MIDI note 60");
        }

        beginTest("Frequency to MIDI note conversion - Middle C quarter tone");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};
            double middleCFreq = 269.444; // C+ is quarter tone above C
            double midiNote = tuning.frequencyToMidiNote(middleCFreq);

            expectWithinAbsoluteError(midiNote, 60.5, 1e-3, "Middle Cq frequency should convert to MIDI note 60.5");
        }

        beginTest("Round-trip conversion accuracy");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};
            for (double note = 21.0; note <= 108.0; note += 0.25) { // Test from A0 to A8
                double frequency = tuning.midiNoteToFrequency(note);
                double roundTripNote = tuning.frequencyToMidiNote(frequency);
                expectWithinAbsoluteError(roundTripNote, note, 1e-3,
                    "Round-trip conversion should be accurate for MIDI note " + String(note));
            }
        }

        beginTest("isDefined method - all notes should be defined");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};

            // Test various MIDI note ranges
            expect(tuning.isDefined(0), "MIDI note 0 should be defined");
            expect(tuning.isDefined(60), "MIDI note 60 (Middle C) should be defined");
            expect(tuning.isDefined(69), "MIDI note 69 (A4) should be defined");
            expect(tuning.isDefined(127), "MIDI note 127 should be defined");
        }

        beginTest("A4 frequency setter");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};
            tuning.setA4Frequency(432.0);
            expectWithinAbsoluteError(tuning.getA4Frequency(), 432.0, 1e-6, "A4 frequency should be updated to 432.0 Hz");

            // Verify that frequency calculations use the new A4 frequency
            double a4_freq = tuning.midiNoteToFrequency(69.0);
            expectWithinAbsoluteError(a4_freq, 432.0, 1e-6, "A4 frequency calculation should use updated value");
        }

        beginTest("getName method");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};
            String name = tuning.getName();
            expect(name.contains("Rational"), "Name should contain 'Rational'");
            expect(name.contains("440.00"), "Name should contain A4 frequency");
        }

        beginTest("Edge case - very high and low frequencies");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};

            // Test very low MIDI note
            double lowFreq = tuning.midiNoteToFrequency(0.0);
            expect(lowFreq > 0.0, "Very low MIDI note should produce positive frequency");

            // Test very high MIDI note
            double highFreq = tuning.midiNoteToFrequency(127.0);
            expect(highFreq > lowFreq, "Very high MIDI note should produce higher frequency than low note");

            // Test round-trip
            double roundTripLow = tuning.frequencyToMidiNote(lowFreq);
            double roundTripHigh = tuning.frequencyToMidiNote(highFreq);
            expectWithinAbsoluteError(roundTripLow, 0.0, 1e-6, "Round-trip for MIDI note 0 should be accurate");
            expectWithinAbsoluteError(roundTripHigh, 127.0, 1e-6, "Round-trip for MIDI note 127 should be accurate");
        }

        beginTest("Fractional MIDI notes");
        {
            RationalTuning tuning {justIntonationRatios, Scale::Key::A, &scale, 440.0};

            // Test quarter-tone between A4 and A#4
            double quarterToneFreq = tuning.midiNoteToFrequency(69.5);
            double a4_freq = tuning.midiNoteToFrequency(69.0);
            double asharp4_freq = tuning.midiNoteToFrequency(70.0);

            expect(quarterToneFreq > a4_freq, "Quarter-tone should be higher than A4");
            expect(quarterToneFreq < asharp4_freq, "Quarter-tone should be lower than A#4");

            // Test round-trip accuracy for fractional note
            double roundTrip = tuning.frequencyToMidiNote(quarterToneFreq);
            expectWithinAbsoluteError(roundTrip, 69.5, 1e-6, "Round-trip for fractional MIDI note should be accurate");
        }
    }
};

static RationalTuningTest rationalTemperamentTuningTest;