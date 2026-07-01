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

//==============================================================================
class FrameNotesTests : public UnitTest {
public:
    FrameNotesTests() : UnitTest("FrameNotes", "MoTool") {}

    void runTest() override {

        // Helper: collect all notes into a vector
        auto collect = [](const PsgParamFrameData& data) {
            std::vector<PsgFrameNote> notes;
            visitFrameNotes(data, [&](const PsgFrameNote& n) { notes.push_back(n); });
            return notes;
        };

        beginTest("envelopePeriodToTonePitch matches TonePeriodA scale at 16*P");
        {
            for (int P : {1, 16, 128, 255}) {
                const float expected = PsgParamType{PsgParamType::TonePeriodA}.valueToNormalized(16 * P);
                const float actual   = envelopePeriodToTonePitch(P);
                expectWithinAbsoluteError(actual, expected, 1e-6f,
                    "envelopePeriodToTonePitch mismatch for P=" + String(P));
            }
        }

        beginTest("Pitch-space alignment: env note pitch equals tone note pitch at 16*P");
        {
            const int P = 128;
            // Frame with envelope-only channel
            PsgParamFrameData envFrame;
            envFrame.set(PsgParamType::EnvelopePeriod, static_cast<uint16_t>(P));
            envFrame.set(PsgParamType::EnvelopeIsOnA, 1);
            envFrame.set(PsgParamType::ToneIsOnA, 0);
            auto envNotes = collect(envFrame);
            expect(!envNotes.empty(), "Expected at least one note from env-only channel");
            const float envPitch = envNotes.empty() ? 0.0f : envNotes[0].pitch;

            // Frame with tone at period 16*P on channel A
            PsgParamFrameData toneFrame;
            toneFrame.set(PsgParamType::TonePeriodA, static_cast<uint16_t>(16 * P));
            toneFrame.set(PsgParamType::ToneIsOnA, 1);
            toneFrame.set(PsgParamType::VolumeA, 10);
            auto toneNotes = collect(toneFrame);
            expect(!toneNotes.empty(), "Expected at least one note from tone channel");
            const float tonePitch = toneNotes.empty() ? 0.0f : toneNotes[0].pitch;

            expectWithinAbsoluteError(envPitch, tonePitch, 1e-6f,
                "Env pitch should match tone pitch at 16*P");
        }

        beginTest("Tone-only channel: one note with correct fields");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::ToneIsOnA, 1);
            data.set(PsgParamType::VolumeA, 10);
            data.set(PsgParamType::TonePeriodA, 500);

            auto notes = collect(data);
            expectEquals(int(notes.size()), 1, "Expected exactly one note");
            if (!notes.empty()) {
                const auto& n = notes[0];
                expectEquals(n.channelIndex, 0, "channelIndex should be 0 (A)");
                expect(!n.hasEnvMod, "hasEnvMod should be false");
                expectEquals(int(n.volume), 10, "volume should be 10");
                const float expectedPitch = PsgParamType{PsgParamType::TonePeriodA}.valueToNormalized(500);
                expectWithinAbsoluteError(n.pitch, expectedPitch, 1e-6f, "pitch mismatch");
            }
        }

        beginTest("Silent channel (volume=0, no env): no notes");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::ToneIsOnA, 1);
            data.set(PsgParamType::VolumeA, 0);
            data.set(PsgParamType::TonePeriodA, 500);

            auto notes = collect(data);
            expectEquals(int(notes.size()), 0, "Expected no notes from silent channel");
        }

        beginTest("Env-only channel (toneOn=0, envMod=1): one channel note, no envelope note");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::EnvelopePeriod, 100);
            data.set(PsgParamType::EnvelopeIsOnA, 1);
            data.set(PsgParamType::ToneIsOnA, 0);

            auto notes = collect(data);
            expectEquals(int(notes.size()), 1, "Expected exactly one note");
            if (!notes.empty()) {
                const auto& n = notes[0];
                expectEquals(n.channelIndex, 0, "channelIndex should be 0 (A)");
                expect(n.hasEnvMod, "hasEnvMod should be true");
                const float expectedPitch = envelopePeriodToTonePitch(100);
                expectWithinAbsoluteError(n.pitch, expectedPitch, 1e-6f, "pitch mismatch");
                // No channelIndex-3 note (toneIsOn is false, so anyToneAndEnv stays false)
                bool hasEnvNote = false;
                for (const auto& note : notes)
                    if (note.channelIndex == 3) hasEnvNote = true;
                expect(!hasEnvNote, "Should not emit channelIndex-3 note when toneIsOn=0");
            }
        }

        beginTest("Tone+env on channel A: two notes (ch0 at tone pitch, ch3 at env pitch)");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::TonePeriodA, 500);
            data.set(PsgParamType::ToneIsOnA, 1);
            data.set(PsgParamType::EnvelopeIsOnA, 1);
            data.set(PsgParamType::EnvelopePeriod, 100);

            auto notes = collect(data);
            expectEquals(int(notes.size()), 2, "Expected exactly two notes");

            const PsgFrameNote* ch0Note = nullptr;
            const PsgFrameNote* ch3Note = nullptr;
            for (const auto& n : notes) {
                if (n.channelIndex == 0) ch0Note = &n;
                if (n.channelIndex == 3) ch3Note = &n;
            }

            expect(ch0Note != nullptr, "Should have channel A (index 0) note");
            expect(ch3Note != nullptr, "Should have envelope (index 3) note");

            if (ch0Note) {
                expect(ch0Note->hasEnvMod, "Channel A note should have hasEnvMod=true");
                const float expectedTonePitch = PsgParamType{PsgParamType::TonePeriodA}.valueToNormalized(500);
                expectWithinAbsoluteError(ch0Note->pitch, expectedTonePitch, 1e-6f,
                    "Channel A pitch should be at tone period 500");
            }
            if (ch3Note) {
                const float expectedEnvPitch = envelopePeriodToTonePitch(100);
                expectWithinAbsoluteError(ch3Note->pitch, expectedEnvPitch, 1e-6f,
                    "Envelope note pitch should be at envPeriod=100");
                expectEquals(int(ch3Note->volume), 15, "Envelope note volume should be 15");
            }
        }

        beginTest("Slow envelope filtered: envPeriod=5000 (16*5000>4095) yields no notes");
        {
            // Env-only channel with out-of-range env period
            PsgParamFrameData envOnly;
            envOnly.set(PsgParamType::EnvelopePeriod, 5000);
            envOnly.set(PsgParamType::EnvelopeIsOnA, 1);
            envOnly.set(PsgParamType::ToneIsOnA, 0);
            auto notes1 = collect(envOnly);
            expectEquals(int(notes1.size()), 0,
                "Env-only channel with envPeriod=5000 should yield no notes");

            // Tone+env channel with out-of-range env period: only tone note, no ch3 note
            PsgParamFrameData toneAndEnv;
            toneAndEnv.set(PsgParamType::TonePeriodA, 500);
            toneAndEnv.set(PsgParamType::ToneIsOnA, 1);
            toneAndEnv.set(PsgParamType::VolumeA, 8);
            toneAndEnv.set(PsgParamType::EnvelopeIsOnA, 1);
            toneAndEnv.set(PsgParamType::EnvelopePeriod, 5000);
            auto notes2 = collect(toneAndEnv);
            expectEquals(int(notes2.size()), 1, "Only tone note, no ch3 note expected");
            if (!notes2.empty()) {
                expectEquals(notes2[0].channelIndex, 0, "Only note should be channel A");
            }
        }

        beginTest("Ultra-high tone (period<8) with env fallback: env pitch emitted, plus ch3 note");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::TonePeriodA, 4);   // <8, filtered out as tone
            data.set(PsgParamType::ToneIsOnA, 1);
            data.set(PsgParamType::VolumeA, 10);
            data.set(PsgParamType::EnvelopeIsOnA, 1);
            data.set(PsgParamType::EnvelopePeriod, 100);

            auto notes = collect(data);
            // Expect: channel A note (env fallback) + ch3 envelope note = 2
            expectEquals(int(notes.size()), 2, "Expected two notes: env-fallback ch0 + ch3");

            const PsgFrameNote* ch0Note = nullptr;
            const PsgFrameNote* ch3Note = nullptr;
            for (const auto& n : notes) {
                if (n.channelIndex == 0) ch0Note = &n;
                if (n.channelIndex == 3) ch3Note = &n;
            }

            expect(ch0Note != nullptr, "Should have channel A note");
            expect(ch3Note != nullptr, "Should have envelope (index 3) note");

            if (ch0Note) {
                expect(ch0Note->hasEnvMod, "Channel A note should have hasEnvMod=true");
                const float expectedEnvPitch = envelopePeriodToTonePitch(100);
                expectWithinAbsoluteError(ch0Note->pitch, expectedEnvPitch, 1e-6f,
                    "Channel A note should be at env pitch (tone period too small)");
            }
        }

        beginTest("envPeriod=0: env notes skipped (envTonePeriod=0 < hearableMin)");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::EnvelopePeriod, 0);
            data.set(PsgParamType::EnvelopeIsOnA, 1);
            data.set(PsgParamType::ToneIsOnA, 0);

            auto notes = collect(data);
            expectEquals(int(notes.size()), 0, "envPeriod=0 should yield no notes");
        }

        beginTest("Three channels tone+env: 4 notes (3 channel notes + 1 envelope note)");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::EnvelopePeriod, 100);
            for (int ch = 0; ch < 3; ++ch) {
                data.set(PsgParamType(PsgParamType::TonePeriodA + ch), static_cast<uint16_t>(200 + ch * 100));
                data.set(PsgParamType(PsgParamType::ToneIsOnA    + ch), 1);
                data.set(PsgParamType(PsgParamType::EnvelopeIsOnA + ch), 1);
            }

            auto notes = collect(data);
            expectEquals(int(notes.size()), 4, "Expected 4 notes: ch0, ch1, ch2 + envelope");

            int envNoteCount = 0;
            for (const auto& n : notes)
                if (n.channelIndex == 3) ++envNoteCount;
            expectEquals(envNoteCount, 1, "Exactly one envelope (ch3) note");
        }
    }
};

static FrameNotesTests frameNotesTests;

}  // namespace MoTool::Tests
