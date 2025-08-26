#include <JuceHeader.h>

#include "NotesToPsgMapper.h"

#include "../../../models/tuning/TuningSystemBase.h"
#include "../../../models/tuning/TuningRegistry.h"
#include "../../../models/tuning/TemperamentSystem.h"
#include "../../../models/tuning/Scales.h"
#include "../../../models/PsgMidi.h"

namespace MoTool::Tests {

using namespace MoTool::uZX;
using namespace juce;

class MidiToPsgConverterTests : public UnitTest {
public:
    MidiToPsgConverterTests() : UnitTest("NotesToPsgMapper", "MoTool") {}

private:
    ChipCapabilities testCaps {16, Range<int>(1, 4096)};

    std::unique_ptr<TuningSystem> createTestTuning() {
        TuningOptions options {
            .tableType = BuiltinTuningEnum::EqualTemperament,
        };
        return makeBuiltinTuning(options);
    }

    // Helper to count specific CC messages
    int countCC(const std::vector<juce::MidiMessage>& messages, MidiCCType ccType, int value = -1) {
        int count = 0;
        for (const auto& msg : messages) {
            if (msg.isController() && msg.getControllerNumber() == static_cast<int>(ccType)) {
                if (value == -1 || msg.getControllerValue() == value) {
                    count++;
                }
            }
        }
        return count;
    }

    // Helper to check if CC is present
    void expectCC(const std::vector<juce::MidiMessage>& messages, MidiCCType ccType, const String& description) {
        expect(countCC(messages, ccType) > 0, description);
    }

    // Helper to check if CC is absent
    void expectNoCC(const std::vector<juce::MidiMessage>& messages, MidiCCType ccType, const String& description) {
        expectEquals(countCC(messages, ccType), 0, description);
    }

    // Helper to check CC value
    void expectCCEquals(const std::vector<juce::MidiMessage>& messages, MidiCCType ccType, int expectedValue, const String& description) {
        expectEquals(countCC(messages, ccType, expectedValue), 1, description);
    }

    // Helper to check converter state
    void expectChannelState(const NotesToPsgMapper& converter, int channel, int expectedNote, const String& description) {
        const auto& constConverter = static_cast<const NotesToPsgMapper&>(converter);
        const auto& state = constConverter.getChannelState(channel);
        expectEquals(state.currentNote.value_or(-1), expectedNote, description);
    }

public:
    void runTest() override {
        beginTest("Basic note on/off");
        {
            NotesToPsgMapper converter;
            converter.setBaseChannel(1);
            converter.setNumChannels(3);
            auto tuning = createTestTuning();
            converter.setTuningSystem(tuning.get());

            // Test note on
            converter.noteOn(1, 60, 100);
            auto messages = converter.takeOutputMessages();

            expectEquals(messages.size(), 4ul); // Volume, PeriodCoarse, PeriodFine, ToneSwitch

            expectCC(messages, MidiCCType::Volume, "Should emit volume CC");
            expectCC(messages, MidiCCType::CC20PeriodCoarse, "Should emit period coarse CC");
            expectCC(messages, MidiCCType::CC52PeriodFine, "Should emit period fine CC");
            expectCCEquals(messages, MidiCCType::GPB1ToneSwitch, 127, "Tone switch should be 127 for note on");

            // Test note off
            converter.clearOutput();
            converter.noteOff(1, 60);
            messages = converter.takeOutputMessages();

            expectEquals(messages.size(), 1ul); // Just tone switch off
            expectCCEquals(messages, MidiCCType::GPB1ToneSwitch, 0, "Tone switch should be 0 for note off");
        }

        beginTest("Channel filtering");
        {
            NotesToPsgMapper converter;
            converter.setBaseChannel(2);
            converter.setNumChannels(2);
            auto tuning = createTestTuning();
            converter.setTuningSystem(tuning.get());

            // Test channel 1 (should be ignored)
            converter.noteOn(1, 60, 100);
            auto messages = converter.takeOutputMessages();
            expectEquals(messages.size(), 0ul, "Channel 1 should be ignored");

            // Test channel 2 (should work)
            converter.noteOn(2, 60, 100);
            messages = converter.takeOutputMessages();
            expect(messages.size() > 0, "Channel 2 should work");

            // Test channel 4 (should be ignored)
            converter.clearOutput();
            converter.noteOn(4, 60, 100);
            messages = converter.takeOutputMessages();
            expectEquals(messages.size(), 0ul, "Channel 4 should be ignored");
        }

        beginTest("Monophonic behavior");
        {
            NotesToPsgMapper converter;
            converter.setBaseChannel(1);
            converter.setNumChannels(1);
            auto tuning = createTestTuning();
            converter.setTuningSystem(tuning.get());

            // Play first note
            converter.noteOn(1, 60, 100);
            expectChannelState(converter, 1, 60, "Should track first note");

            // Play second note (should replace first without tone off)
            converter.clearOutput();
            converter.noteOn(1, 62, 120);
            expectChannelState(converter, 1, 62, "Should track second note");

            auto messages = converter.takeOutputMessages();

            // Should NOT have tone switch messages - tone is already on
            expectNoCC(messages, MidiCCType::GPB1ToneSwitch, "Should NOT have tone switch messages when switching notes");

            // Should update volume due to different velocity (100 vs 120)
            expectCC(messages, MidiCCType::Volume, "Should update volume when velocity changes between notes");

            // Play third note with same velocity (should not emit volume CC)
            converter.clearOutput();
            converter.noteOn(1, 64, 120);
            expectChannelState(converter, 1, 64, "Should track third note");

            auto messages2 = converter.takeOutputMessages();
            expectNoCC(messages2, MidiCCType::Volume, "Should NOT emit volume CC when velocity stays the same");
            expectNoCC(messages2, MidiCCType::GPB1ToneSwitch, "Should NOT emit tone switch for third note");
            expectCC(messages2, MidiCCType::CC20PeriodCoarse, "Should emit period coarse for third note");
            expectCC(messages2, MidiCCType::CC52PeriodFine, "Should emit period fine for third note");
        }

        beginTest("Velocity and aftertouch mapping");
        {
            NotesToPsgMapper converter;
            converter.setBaseChannel(1);
            converter.setNumChannels(1);
            auto tuning = createTestTuning();
            converter.setTuningSystem(tuning.get());

            // Test velocity mapping
            converter.noteOn(1, 60, 100); // Moderate velocity
            auto messages = converter.takeOutputMessages();

            expectCC(messages, MidiCCType::Volume, "Volume should be present for note on");

            // Test aftertouch affecting volume
            converter.clearOutput();
            converter.aftertouch(1, 20); // Aftertouch will change combined volume (100+20=120 vs original 100)
            messages = converter.takeOutputMessages();

            // Should re-emit volume with updated value
            expectCC(messages, MidiCCType::Volume, "Aftertouch should update volume");
        }

        beginTest("CC passthrough");
        {
            NotesToPsgMapper converter;
            converter.setBaseChannel(1);
            converter.setNumChannels(1);

            // Test random CC passthrough
            converter.controlChange(1, 64, 100); // Sustain pedal
            auto messages = converter.takeOutputMessages();

            expectEquals(messages.size(), 1ul, "Should pass through CC");
            expect(messages[0].isController(), "Should be controller message");
            expectEquals(messages[0].getControllerNumber(), 64, "Should preserve controller number");
            expectEquals(messages[0].getControllerValue(), 100, "Should preserve controller value");
        }

        beginTest("Optimized tone switching");
        {
            NotesToPsgMapper converter;
            converter.setBaseChannel(1);
            converter.setNumChannels(3);
            auto tuning = createTestTuning();
            converter.setTuningSystem(tuning.get());

            // First note - should turn tone ON
            converter.noteOn(1, 60, 100);
            auto messages = converter.takeOutputMessages();

            expectCCEquals(messages, MidiCCType::GPB1ToneSwitch, 127, "First note should turn tone ON");

            // Second note - should NOT send tone switch (already on)
            converter.clearOutput();
            converter.noteOn(1, 62, 120);
            messages = converter.takeOutputMessages();

            expectNoCC(messages, MidiCCType::GPB1ToneSwitch, "Second note should NOT send tone switch");
            expectCC(messages, MidiCCType::Volume, "Second note should send volume");
            expectCC(messages, MidiCCType::CC20PeriodCoarse, "Second note should send period coarse");
            expectCC(messages, MidiCCType::CC52PeriodFine, "Second note should send period fine");

            // Note off - should turn tone OFF
            converter.clearOutput();
            converter.noteOff(1, 62);
            messages = converter.takeOutputMessages();

            expectCCEquals(messages, MidiCCType::GPB1ToneSwitch, 0, "Note off should turn tone OFF");
        }

        beginTest("Period encoding correctness");
        {
            NotesToPsgMapper converter;
            converter.setBaseChannel(1);
            converter.setNumChannels(1);
            auto tuning = createTestTuning();
            converter.setTuningSystem(tuning.get());

            // Test a specific period value
            converter.noteOn(1, 60, 100); // Middle C
            auto messages = converter.takeOutputMessages();

            expectCC(messages, MidiCCType::CC20PeriodCoarse, "Should find coarse period value");
            expectCC(messages, MidiCCType::CC52PeriodFine, "Should find fine period value");

            int coarseValue = -1, fineValue = -1;
            for (const auto& msg : messages) {
                if (msg.isController()) {
                    if (msg.getControllerNumber() == static_cast<int>(MidiCCType::CC20PeriodCoarse)) {
                        coarseValue = msg.getControllerValue();
                    }
                    else if (msg.getControllerNumber() == static_cast<int>(MidiCCType::CC52PeriodFine)) {
                        fineValue = msg.getControllerValue();
                    }
                }
            }

            // Reconstruct the period using the same method as PsgMidi.cpp
            int reconstructedPeriod = (coarseValue << 7) | fineValue;

            // Get the original period from tuning system for verification
            int originalPeriod = tuning->midiNoteToPeriod(60);

            expectEquals(reconstructedPeriod, originalPeriod,
                "Reconstructed period should match original: coarse=" + String(coarseValue) +
                " fine=" + String(fineValue) + " reconstructed=" + String(reconstructedPeriod) +
                " original=" + String(originalPeriod));

            // Test with some known period values to verify encoding/decoding
            std::vector<int> testPeriods = {1, 127, 128, 1000, 2000, 4095};

            for (int testPeriod : testPeriods) {
                // Encode the period the same way as our emitPeriodCC method
                int coarse = (testPeriod >> 7) & 0x7F;
                int fine = testPeriod & 0x7F;

                // Decode it back
                int decoded = (coarse << 7) | fine;

                expectEquals(decoded, testPeriod,
                    "Period encoding/decoding should be lossless for period " + String(testPeriod));
            }
        }
    }
};

static MidiToPsgConverterTests midiToPsgConverterTests;

} // namespace MoTool::Tests