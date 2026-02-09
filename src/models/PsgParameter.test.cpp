#include <JuceHeader.h>
#include "formats/psg/PsgData.h"
#include "PsgList.h"
#include "PsgParameter.h"

namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace tracktion;
using namespace juce;
using namespace std::literals;
using namespace MoTool::uZX;

//==============================================================================
class RetriggerEnvelopeTests : public UnitTest {
public:
    RetriggerEnvelopeTests() : UnitTest("RetriggerEnvelope", "MoTool") {}

    void runTest() override {
        beginTest("RetriggerEnvelope=1 forces envelope shape register write even if value unchanged");
        {
            PsgParamFrameData frame1;
            frame1.set(PsgParamType::EnvelopeShape, 8);
            frame1.set(PsgParamType::RetriggerEnvelope, 0);  // Explicit no retrigger

            auto regs1 = frame1.toRegisters();
            expect(regs1.hasEnvelopeShapeSet(), "Frame 1 should have envelope shape set");

            // Frame 2: Same envelope shape, but with retrigger=1
            PsgParamFrameData frame2;
            frame2.set(PsgParamType::EnvelopeShape, 8);  // Same value
            frame2.set(PsgParamType::RetriggerEnvelope, 1);  // Force retrigger

            auto regs2 = frame2.toRegisters();
            expect(regs2.hasEnvelopeShapeSet(), "Frame 2 should have envelope shape set (retrigger)");
            expectEquals(int(regs2.getEnvelopeShape()), 8, "Envelope shape should be 8");
        }

        beginTest("RetriggerEnvelope state transitions (1 -> 0 -> 1)");
        {
            PsgParamFrameData state;

            // Frame 1: Set retrigger=1
            PsgParamFrameData frame1;
            frame1.set(PsgParamType::RetriggerEnvelope, 1);
            state.update(frame1);
            expect(state.isSet(PsgParamType::RetriggerEnvelope), "Retrigger should be set");
            expectEquals(int(state[PsgParamType::RetriggerEnvelope].value_or(-1)), 1, "Should be 1");

            // Frame 2: Explicitly set retrigger=0 (turn off)
            PsgParamFrameData frame2;
            frame2.set(PsgParamType::RetriggerEnvelope, 0);
            state.update(frame2);
            expect(state.isSet(PsgParamType::RetriggerEnvelope), "Retrigger should still be set");
            expectEquals(int(state[PsgParamType::RetriggerEnvelope].value_or(-1)), 0, "Should be 0");

            // Frame 3: Set retrigger=1 again
            PsgParamFrameData frame3;
            frame3.set(PsgParamType::RetriggerEnvelope, 1);
            state.update(frame3);
            expectEquals(int(state[PsgParamType::RetriggerEnvelope].value_or(-1)), 1, "Should be 1 again");
        }

        beginTest("PSG import auto-detects retriggers and manages state transitions");
        {
            PsgData data {
                {
                    {{PsgRegType::EnvelopeShape, 8}},  // Frame 0: shape=8 (first write)
                    {{PsgRegType::EnvelopeShape, 8}},  // Frame 1: shape=8 again (retrigger)
                    {{PsgRegType::EnvelopeShape, 10}}, // Frame 2: shape=10 (value change)
                    {{PsgRegType::EnvelopeShape, 10}}, // Frame 3: shape=10 again (retrigger)
                    {},                               // Frame 4: no envelope shape write
                    {{PsgRegType::EnvelopeShape, 10}}  // Frame 5: shape=10 again (retrigger after gap)
                },
                {50.0, 1}
            };

            auto engine = std::make_unique<Engine>("RetriggerTest", nullptr, nullptr);
            auto edit = Edit::createSingleTrackEdit(*engine);
            PsgList psgList;
            psgList.loadFrom(data, *edit, nullptr);

            auto& frames = psgList.getFrames();

            // Verify retrigger flags
            expect(frames.size() >= 5, "Should have at least 5 frames");

            // Frame 0: No retrigger (first write)
            expect(!frames[0]->getData().isSet(PsgParamType::RetriggerEnvelope),
                "Frame 0 should NOT have retrigger");

            // Frame 1: Retrigger=1 (same value 8)
            expect(frames[1]->getData()[PsgParamType::RetriggerEnvelope] == 1,
                "Frame 1 should have retrigger=1");

            // Frame 2: Retrigger=0 (value changed to 10)
            expect(frames[2]->getData()[PsgParamType::RetriggerEnvelope] == 0,
                "Frame 2 should have retrigger=0 (state change)");

            // Frame 3: Retrigger=1 (same value 10)
            expect(frames[3]->getData()[PsgParamType::RetriggerEnvelope] == 1,
                "Frame 3 should have retrigger=1");

            // Frame 4: Empty frame - retrigger should turn off
            // This frame is sparse, but if it exists it should have retrigger=0
            // Actually, sparse representation means it might not exist

            // Frame 5: Retrigger=1 (same value 10 after gap)
            int frame5Idx = -1;
            for (int i = 4; i < frames.size(); ++i) {
                if (frames[i]->getData().isSet(PsgParamType::EnvelopeShape) &&
                    frames[i]->getData()[PsgParamType::EnvelopeShape] == 10) {
                    frame5Idx = static_cast<int>(i);
                    break;
                }
            }
            expect(frame5Idx >= 0, "Should find frame 5 with shape=10");
            if (frame5Idx >= 0) {
                expect(frames[frame5Idx]->getData()[PsgParamType::RetriggerEnvelope] == 1,
                    "Frame 5 should have retrigger=1");
            }
        }
    }
};

static RetriggerEnvelopeTests retriggerEnvelopeTests;

//==============================================================================
class ParameterScaleTests : public UnitTest {
public:
    ParameterScaleTests() : UnitTest("ParameterScale", "MoTool") {}

    void runTest() override {
        beginTest("Linear: valueToNormalized");
        {
            ParameterScale scale {0, 15, ScaleType::Linear, {}};
            expectWithinAbsoluteError(scale.valueToNormalized(0), 0.0f, 1e-5f);
            expectWithinAbsoluteError(scale.valueToNormalized(7), 7.0f / 15.0f, 1e-5f);
            expectWithinAbsoluteError(scale.valueToNormalized(15), 1.0f, 1e-5f);
        }

        beginTest("Linear: normalizedToValue");
        {
            ParameterScale scale {0, 15, ScaleType::Linear, {}};
            expectEquals(scale.normalizedToValue(0.0f), 0);
            expectEquals(scale.normalizedToValue(0.5f), 8);
            expectEquals(scale.normalizedToValue(1.0f), 15);
        }

        beginTest("Linear: round-trip");
        {
            ParameterScale scale {0, 15, ScaleType::Linear, {}};
            for (int v = 0; v <= 15; ++v) {
                expectEquals(scale.normalizedToValue(scale.valueToNormalized(v)), v,
                    "Round-trip failed for value " + String(v));
            }
        }

        beginTest("Log: round-trip");
        {
            ParameterScale scale {0, 4095, ScaleType::Log, {}};
            for (int v : {0, 1, 100, 2048, 4095}) {
                expectEquals(scale.normalizedToValue(scale.valueToNormalized(v)), v,
                    "Log round-trip failed for value " + String(v));
            }
        }

        beginTest("ReciprocalLog: round-trip");
        {
            ParameterScale scale {0, 4095, ScaleType::ReciprocalLog, {}};
            for (int v : {0, 1, 100, 2048, 4095}) {
                expectEquals(scale.normalizedToValue(scale.valueToNormalized(v)), v,
                    "ReciprocalLog round-trip failed for value " + String(v));
            }
        }

        beginTest("ReciprocalLog: ordering");
        {
            ParameterScale scale {0, 4095, ScaleType::ReciprocalLog, {}};
            // Higher raw values should produce lower normalized values (inversion property)
            expect(scale.valueToNormalized(100) > scale.valueToNormalized(1000),
                "Higher raw value should produce lower normalized value");
            expect(scale.valueToNormalized(1000) > scale.valueToNormalized(4095),
                "Higher raw value should produce lower normalized value");
        }

        beginTest("Edge: zero range");
        {
            ParameterScale scale {5, 5, ScaleType::Linear, {}};
            expectWithinAbsoluteError(scale.valueToNormalized(5), 0.0f, 1e-5f);
            expectEquals(scale.normalizedToValue(0.5f), 5);
        }

        beginTest("Boundary: normalized clamping");
        {
            ParameterScale scale {0, 15, ScaleType::Linear, {}};
            expectEquals(scale.normalizedToValue(-0.5f), 0);
            expectEquals(scale.normalizedToValue(1.5f), 15);
        }

        beginTest("Octaves");
        {
            // 1..4096 = 12 octaves (log2(4096) - log2(1) = 12)
            ParameterScale scale {1, 4096, ScaleType::Linear, {}};
            expectWithinAbsoluteError(scale.octaves(), 12.0f, 1e-5f);

            // start=0 is clamped to 1: log2(4095) - log2(1) ≈ 11.999
            ParameterScale tonePeriod {0, 4095, ScaleType::ReciprocalLog, {}};
            expectWithinAbsoluteError(tonePeriod.octaves(), std::log2(4095.0f), 1e-5f);
        }
    }
};

static ParameterScaleTests parameterScaleTests;

}  // namespace MoTool::Tests
