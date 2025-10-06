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
    int countCC(const tracktion::MidiMessageArray& messages, MidiCCType ccType, int value = -1) {
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
    void expectCC(const te::MidiMessageArray& messages, MidiCCType ccType, const String& description) {
        expect(countCC(messages, ccType) > 0, description);
    }

    // Helper to check if CC is absent
    void expectNoCC(const te::MidiMessageArray& messages, MidiCCType ccType, const String& description) {
        expectEquals(countCC(messages, ccType), 0, description);
    }

    // Helper to check CC value
    void expectCCEquals(const te::MidiMessageArray& messages, MidiCCType ccType, int expectedValue, const String& description) {
        expectEquals(countCC(messages, ccType, expectedValue), 1, description);
    }

    // Helper to debug mesagesa
    void debugMessages(const te::MidiMessageArray& messages) {
        for (const auto& msg : messages) {
            DBG(msg.getDescription());
        }
    }

    // Helper to check mapper state
    void expectChannelState(const NotesToPsgMapper& mapper, int channel, int expectedNote, const String& description) {
        const auto& constmapper = static_cast<const NotesToPsgMapper&>(mapper);
        const auto& voice = constmapper.getVoice(channel);
        expectEquals(static_cast<int>(voice.mpeNote.initialNote), expectedNote, description);
    }

public:
    void runTest() override {
        beginTest("Basic note on/off");
        {
            NotesToPsgMapper mapper;
            auto tuning = createTestTuning();
            mapper.setTuningSystem(tuning.get());

            // Test note on
            mapper.handleMidiMessage({MidiMessage::noteOn(1, 60, (uint8)100), 0});
            auto messages = mapper.renderVoices();

            expectEquals(messages.size(), 5); // Volume, PeriodCoarse, PeriodFine, ToneSwitch

            expectCC(messages, MidiCCType::Volume,           "Should emit volume CC");
            expectCC(messages, MidiCCType::CC20PeriodCoarse, "Should emit period coarse CC");
            expectCC(messages, MidiCCType::CC52PeriodFine,   "Should emit period fine CC");
            expectCC(messages, MidiCCType::GPB1ToneSwitch,   "Should emit tone switch CC");
            expectCC(messages, MidiCCType::GPB2NoiseSwitch,  "Should emit noise switch CC (off)");

            expectCCEquals(messages, MidiCCType::GPB1ToneSwitch, 127, "Tone switch should be 127 for note on");
            expectCCEquals(messages, MidiCCType::GPB2NoiseSwitch, 0,   "Noise switch should be 0 (off) for note on");

            // Test note off
            mapper.handleMidiMessage({MidiMessage::noteOff(1, 60), 0});
            messages = mapper.renderVoices();

            expectEquals(messages.size(), 1);
            expectCCEquals(messages, MidiCCType::Volume, 0, "Volume should be 0");
        }

        beginTest("Channel filtering");
        {
            NotesToPsgMapper mapper;
            mapper.setBaseChannel(2);
            mapper.setPassthruOutsideChannels(false); // Disable passthrough for test
            auto tuning = createTestTuning();
            mapper.setTuningSystem(tuning.get());

            // Test channel 1 (should be ignored)
            mapper.handleMidiMessage({MidiMessage::noteOn(1, 60, (uint8)100), 0});
            auto messages = mapper.renderVoices();
            expectEquals(messages.size(), 0, "Channel 1 should be ignored");

            // Test channel 2 (should work)
            mapper.handleMidiMessage({MidiMessage::noteOn(2, 60, (uint8)100), 0});
            messages = mapper.renderVoices();
            expect(messages.size() > 0, "Channel 2 should work");

            // Test channel 6 (should be ignored)
            mapper.clearOutput();
            mapper.handleMidiMessage({MidiMessage::noteOn(6, 60, (uint8)100), 0});
            messages = mapper.renderVoices();
            expectEquals(messages.size(), 0, "Channel 6 should be ignored");
        }

        beginTest("Monophonic behavior");
        {
            NotesToPsgMapper mapper;
            mapper.setBaseChannel(1);
            auto tuning = createTestTuning();
            mapper.setTuningSystem(tuning.get());

            // Play first note
            mapper.handleMidiMessage({MidiMessage::noteOn(1, 60, (uint8)100), 0});
            expectChannelState(mapper, 1, 60, "Should track first note");
            ignoreUnused(mapper.renderVoices());

            // Play second note (should replace first without tone off)
            mapper.handleMidiMessage({MidiMessage::noteOn(1, 62, (uint8)120), 0});
            expectChannelState(mapper, 1, 62, "Should track second note");
            auto messages = mapper.renderVoices();

            // Should NOT have tone switch messages - tone is already on
            expectNoCC(messages, MidiCCType::GPB1ToneSwitch, "Should NOT have tone switch messages when switching notes");

            // Should update volume due to different velocity (100 vs 120)
            expectCC(messages, MidiCCType::Volume, "Should update volume when velocity changes between notes");

            // Play third note with same velocity (should not emit volume CC)
            mapper.clearOutput();
            mapper.handleMidiMessage({MidiMessage::noteOn(1, 64, (uint8)120), 0});
            expectChannelState(mapper, 1, 64, "Should track third note");

            auto messages2 = mapper.renderVoices();
            expectNoCC(messages2, MidiCCType::Volume,         "Should NOT emit volume CC when velocity stays the same");
            expectNoCC(messages2, MidiCCType::GPB1ToneSwitch, "Should NOT emit tone switch for third note");
            expectCC(messages2, MidiCCType::CC20PeriodCoarse, "Should emit period coarse for third note");
            expectCC(messages2, MidiCCType::CC52PeriodFine,   "Should emit period fine for third note");
        }

        beginTest("Velocity and aftertouch mapping");
        {
            NotesToPsgMapper mapper;
            mapper.setBaseChannel(1);
            auto tuning = createTestTuning();
            mapper.setTuningSystem(tuning.get());

            mapper.handleMidiMessage({MidiMessage::noteOn(1, 60, (uint8)100), 0});
            auto messages = mapper.renderVoices();

            expectCC(messages, MidiCCType::Volume, "Volume should be present for note on");

            mapper.handleMidiMessage({MidiMessage::aftertouchChange(1, 60, 120), 0});
            messages = mapper.renderVoices();

            expectCC(messages, MidiCCType::Volume, "Aftertouch should update volume");
        }

        beginTest("CC passthrough");
        {
            NotesToPsgMapper mapper;
            mapper.setBaseChannel(1);

            mapper.handleMidiMessage({MidiMessage::controllerEvent(1, 64, 100), 0});
            auto messages = mapper.renderVoices();

            expectEquals(messages.size(), 1, "Should pass through CC");
            expect(messages[0].isController(), "Should be controller message");
            expectEquals(messages[0].getControllerNumber(), 64, "Should preserve controller number");
            expectEquals(messages[0].getControllerValue(), 100, "Should preserve controller value");
        }

        beginTest("Optimized tone switching");
        {
            NotesToPsgMapper mapper;
            mapper.setBaseChannel(1);
            auto tuning = createTestTuning();
            mapper.setTuningSystem(tuning.get());

            mapper.handleMidiMessage({MidiMessage::noteOn(1, 60, (uint8)100), 0});
            auto messages = mapper.renderVoices();

            expectCCEquals(messages, MidiCCType::GPB1ToneSwitch, 127, "First note should turn tone ON");

            mapper.handleMidiMessage({MidiMessage::noteOn(1, 62, (uint8)120), 0});
            messages = mapper.renderVoices();

            expectNoCC(messages, MidiCCType::GPB1ToneSwitch, "Second note should NOT send tone switch");
            expectCC(messages, MidiCCType::Volume, "Second note should send volume");
            expectCC(messages, MidiCCType::CC20PeriodCoarse, "Second note should send period coarse");
            expectCC(messages, MidiCCType::CC52PeriodFine, "Second note should send period fine");

            mapper.handleMidiMessage({MidiMessage::noteOff(1, 62), 0});
            messages = mapper.renderVoices();

            expectCCEquals(messages, MidiCCType::Volume, 0, "Note off should mute the channel");
            expectNoCC(messages, MidiCCType::GPB1ToneSwitch, "Note off should not emit redundant tone switch");
        }

        beginTest("Period encoding correctness");
        {
            NotesToPsgMapper mapper;
            mapper.setBaseChannel(1);
            auto tuning = createTestTuning();
            mapper.setTuningSystem(tuning.get());

            mapper.handleMidiMessage({MidiMessage::noteOn(1, 60, (uint8)100), 0});
            auto messages = mapper.renderVoices();

            expectCC(messages, MidiCCType::CC20PeriodCoarse, "Should find coarse period value");
            expectCC(messages, MidiCCType::CC52PeriodFine, "Should find fine period value");

            int coarseValue = -1, fineValue = -1;
            for (const auto& msg : messages) {
                if (!msg.isController()) {
                    continue;
                }

                if (msg.getControllerNumber() == static_cast<int>(MidiCCType::CC20PeriodCoarse)) {
                    coarseValue = msg.getControllerValue();
                } else if (msg.getControllerNumber() == static_cast<int>(MidiCCType::CC52PeriodFine)) {
                    fineValue = msg.getControllerValue();
                }
            }

            auto reconstructedPeriod = (coarseValue << 7) | fineValue;
            auto originalPeriod = tuning->midiNoteToPeriod(60.0);

            expectEquals(reconstructedPeriod, originalPeriod,
                "Reconstructed period should match original: coarse=" + String(coarseValue) +
                " fine=" + String(fineValue) + " reconstructed=" + String(reconstructedPeriod) +
                " original=" + String(originalPeriod));

            std::vector<int> testPeriods {1, 127, 128, 1000, 2000, 4095};

            for (auto testPeriod : testPeriods) {
                auto coarse = (testPeriod >> 7) & 0x7F;
                auto fine = testPeriod & 0x7F;
                auto decoded = (coarse << 7) | fine;

                expectEquals(decoded, testPeriod,
                    "Period encoding/decoding should be lossless for period " + String(testPeriod));
            }
        }
    }
};

static MidiToPsgConverterTests midiToPsgmapperTests;

} // namespace MoTool::Tests
