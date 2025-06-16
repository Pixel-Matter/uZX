#include <JuceHeader.h>

#include "TuningSystem.h"

using namespace MoTool;

class EqualTemperamentTuningTest : public juce::UnitTest {
public:
    EqualTemperamentTuningTest() : UnitTest("EqualTemperamentTuning", "MoTool") {}

    void runTest() override {
        beginTest("Constructor with default A4 frequency");
        {
            EqualTemperamentTuning tuning;
            expectEquals(tuning.getA4Frequency(), 440.0, "Default A4 frequency should be 440.0 Hz");
            expectEquals(static_cast<int>(tuning.getType().value), static_cast<int>(TemperamentType::EqualTemperament));
        }

        beginTest("Constructor with custom A4 frequency");
        {
            EqualTemperamentTuning tuning(432.0);
            expectEquals(tuning.getA4Frequency(), 432.0, "Custom A4 frequency should be 432.0 Hz");
        }

        beginTest("MIDI note to frequency conversion - A4");
        {
            EqualTemperamentTuning tuning(440.0);
            double frequency = tuning.midiNoteToFrequency(69.0); // A4
            expect(std::abs(frequency - 440.0) < 0.001, "A4 (MIDI 69) should be 440.0 Hz");
        }

        beginTest("MIDI note to frequency conversion - Middle C");
        {
            EqualTemperamentTuning tuning(440.0);
            double frequency = tuning.midiNoteToFrequency(60.0); // Middle C
            double expectedFreq = 440.0 * std::pow(2.0, (60.0 - 69.0) / 12.0);
            expect(std::abs(frequency - expectedFreq) < 0.001, "Middle C (MIDI 60) frequency should be correct");
        }

        beginTest("MIDI note to frequency conversion - octave relationships");
        {
            EqualTemperamentTuning tuning(440.0);
            double a4_freq = tuning.midiNoteToFrequency(69.0); // A4
            double a5_freq = tuning.midiNoteToFrequency(81.0); // A5 (one octave higher)
            double a3_freq = tuning.midiNoteToFrequency(57.0); // A3 (one octave lower)

            expect(std::abs(a5_freq - (a4_freq * 2.0)) < 0.001, "A5 should be exactly double A4 frequency");
            expect(std::abs(a3_freq - (a4_freq / 2.0)) < 0.001, "A3 should be exactly half A4 frequency");
        }

        beginTest("Frequency to MIDI note conversion - A4");
        {
            EqualTemperamentTuning tuning(440.0);
            double midiNote = tuning.frequencyToMidiNote(440.0);
            expect(std::abs(midiNote - 69.0) < 0.001, "440.0 Hz should convert to MIDI note 69 (A4)");
        }

        beginTest("Frequency to MIDI note conversion - Middle C");
        {
            EqualTemperamentTuning tuning(440.0);
            double middleCFreq = 440.0 * std::pow(2.0, (60.0 - 69.0) / 12.0);
            double midiNote = tuning.frequencyToMidiNote(middleCFreq);
            expect(std::abs(midiNote - 60.0) < 0.001, "Middle C frequency should convert to MIDI note 60");
        }

        beginTest("Round-trip conversion accuracy");
        {
            EqualTemperamentTuning tuning(440.0);
            for (int note = 21; note <= 108; note += 12) { // Test octaves from A0 to A8
                double frequency = tuning.midiNoteToFrequency(static_cast<double>(note));
                double roundTripNote = tuning.frequencyToMidiNote(frequency);
                expect(std::abs(roundTripNote - note) < 0.001, 
                    "Round-trip conversion should be accurate for MIDI note " + String(note));
            }
        }

        beginTest("isDefined method - all notes should be defined");
        {
            EqualTemperamentTuning tuning(440.0);
            
            // Test various MIDI note ranges
            expect(tuning.isDefined(0), "MIDI note 0 should be defined");
            expect(tuning.isDefined(60), "MIDI note 60 (Middle C) should be defined");
            expect(tuning.isDefined(69), "MIDI note 69 (A4) should be defined");
            expect(tuning.isDefined(127), "MIDI note 127 should be defined");
        }

        beginTest("A4 frequency setter");
        {
            EqualTemperamentTuning tuning(440.0);
            tuning.setA4Frequency(432.0);
            expectEquals(tuning.getA4Frequency(), 432.0, "A4 frequency should be updated to 432.0 Hz");
            
            // Verify that frequency calculations use the new A4 frequency
            double a4_freq = tuning.midiNoteToFrequency(69.0);
            expect(std::abs(a4_freq - 432.0) < 0.001, "A4 frequency calculation should use updated value");
        }

        beginTest("getName method");
        {
            EqualTemperamentTuning tuning(440.0);
            String name = tuning.getName();
            expect(name.contains("Equal Temperament"), "Name should contain 'Equal Temperament'");
            expect(name.contains("440.00"), "Name should contain A4 frequency");
        }

        beginTest("Edge case - very high and low frequencies");
        {
            EqualTemperamentTuning tuning(440.0);
            
            // Test very low MIDI note
            double lowFreq = tuning.midiNoteToFrequency(0.0);
            expect(lowFreq > 0.0, "Very low MIDI note should produce positive frequency");
            
            // Test very high MIDI note
            double highFreq = tuning.midiNoteToFrequency(127.0);
            expect(highFreq > lowFreq, "Very high MIDI note should produce higher frequency than low note");
            
            // Test round-trip
            double roundTripLow = tuning.frequencyToMidiNote(lowFreq);
            double roundTripHigh = tuning.frequencyToMidiNote(highFreq);
            expect(std::abs(roundTripLow - 0.0) < 0.001, "Round-trip for MIDI note 0 should be accurate");
            expect(std::abs(roundTripHigh - 127.0) < 0.001, "Round-trip for MIDI note 127 should be accurate");
        }

        beginTest("Fractional MIDI notes");
        {
            EqualTemperamentTuning tuning(440.0);
            
            // Test quarter-tone between A4 and A#4
            double quarterToneFreq = tuning.midiNoteToFrequency(69.5);
            double a4_freq = tuning.midiNoteToFrequency(69.0);
            double asharp4_freq = tuning.midiNoteToFrequency(70.0);
            
            expect(quarterToneFreq > a4_freq, "Quarter-tone should be higher than A4");
            expect(quarterToneFreq < asharp4_freq, "Quarter-tone should be lower than A#4");
            
            // Test round-trip accuracy for fractional note
            double roundTrip = tuning.frequencyToMidiNote(quarterToneFreq);
            expect(std::abs(roundTrip - 69.5) < 0.001, "Round-trip for fractional MIDI note should be accurate");
        }
    }
};

static EqualTemperamentTuningTest equalTemperamentTuningTest;