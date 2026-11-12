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
class EnvelopeRestartTests : public UnitTest {
public:
    EnvelopeRestartTests() : UnitTest("EnvelopeRestart", "MoTool") {}

    void runTest() override {
        beginTest("A set envelope shape writes the register, whatever the value before");
        {
            PsgParamFrameData frame;
            frame.set(PsgParamType::EnvelopeShape, 8);
            expect(frame.toRegisters().hasEnvelopeShapeSet());
            expectEquals(int(frame.toRegisters().getEnvelopeShape()), 8);

            PsgParamFrameData other;
            other.set(PsgParamType::EnvelopePeriod, 100);
            expect(!other.toRegisters().hasEnvelopeShapeSet(), "no shape set, no write, the envelope runs on");
        }

        beginTest("PSG import keeps every write of the shape, the same shape too");
        {
            PsgData data {
                {
                    {{PsgRegType::EnvelopeShape, 8}},   // Frame 0: first write
                    {{PsgRegType::EnvelopeShape, 8}},   // Frame 1: the same shape again, a restart
                    {{PsgRegType::EnvelopeShape, 10}},  // Frame 2: another shape
                    {{PsgRegType::EnvelopeShape, 10}},  // Frame 3: a restart
                    {{PsgRegType::VolumeA, 5}},         // Frame 4: no write of the shape
                    {{PsgRegType::EnvelopeShape, 10}}   // Frame 5: a restart after a gap
                },
                {50.0, 1}
            };

            auto& engine = *Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit(engine);
            PsgList psgList;
            psgList.loadFrom(data, *edit, {}, nullptr);

            String shapes;
            for (auto* frame : psgList.getFrames()) {
                const auto shape = frame->getData()[PsgParamType::EnvelopeShape];
                shapes << (shape ? String(int(*shape)) : String("-")) << " ";
            }
            expectEquals(shapes, String("8 8 10 10 - 10 "));
        }
    }
};

static EnvelopeRestartTests envelopeRestartTests;

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
                expectEquals(int(n.channelMask), 1, "channelMask should be A");
                expect(!n.isEnvelopePitch, "Tone note should not be an envelope pitch note");
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

        beginTest("Env-only channel (toneOn=0, envMod=1): one shared envelope pitch note");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::EnvelopePeriod, 100);
            data.set(PsgParamType::EnvelopeIsOnA, 1);
            data.set(PsgParamType::ToneIsOnA, 0);

            auto notes = collect(data);
            expectEquals(int(notes.size()), 1, "Expected exactly one note");
            if (!notes.empty()) {
                const auto& n = notes[0];
                expectEquals(int(n.channelMask), 1, "channelMask should be A");
                expect(n.isEnvelopePitch, "Env-only note should be an envelope pitch note");
                expect(n.hasEnvMod, "hasEnvMod should be true");
                const float expectedPitch = envelopePeriodToTonePitch(100);
                expectWithinAbsoluteError(n.pitch, expectedPitch, 1e-6f, "pitch mismatch");
            }
        }

        beginTest("Tone+env on channel A: two notes (A tone pitch, shared env pitch)");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::TonePeriodA, 500);
            data.set(PsgParamType::ToneIsOnA, 1);
            data.set(PsgParamType::EnvelopeIsOnA, 1);
            data.set(PsgParamType::EnvelopePeriod, 100);

            auto notes = collect(data);
            expectEquals(int(notes.size()), 2, "Expected exactly two notes");

            const PsgFrameNote* ch0Note = nullptr;
            const PsgFrameNote* envNote = nullptr;
            for (const auto& n : notes) {
                if (! n.isEnvelopePitch && n.channelMask == 1) ch0Note = &n;
                if (n.isEnvelopePitch) envNote = &n;
            }

            expect(ch0Note != nullptr, "Should have channel A tone note");
            expect(envNote != nullptr, "Should have shared envelope pitch note");

            if (ch0Note) {
                expectEquals(int(ch0Note->channelMask), 1, "Channel A note mask should be A");
                expect(!ch0Note->isEnvelopePitch, "Channel A note should be a tone pitch note");
                expect(ch0Note->hasEnvMod, "Channel A note should have hasEnvMod=true");
                const float expectedTonePitch = PsgParamType{PsgParamType::TonePeriodA}.valueToNormalized(500);
                expectWithinAbsoluteError(ch0Note->pitch, expectedTonePitch, 1e-6f,
                    "Channel A pitch should be at tone period 500");
            }
            if (envNote) {
                expectEquals(int(envNote->channelMask), 1, "Envelope note mask should be A");
                expect(envNote->isEnvelopePitch, "Envelope note should be marked as envelope pitch");
                const float expectedEnvPitch = envelopePeriodToTonePitch(100);
                expectWithinAbsoluteError(envNote->pitch, expectedEnvPitch, 1e-6f,
                    "Envelope note pitch should be at envPeriod=100");
                expectEquals(int(envNote->volume), 15, "Envelope note volume should be 15");
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

            // Tone+env channel with out-of-range env period: only tone note, no envelope pitch note
            PsgParamFrameData toneAndEnv;
            toneAndEnv.set(PsgParamType::TonePeriodA, 500);
            toneAndEnv.set(PsgParamType::ToneIsOnA, 1);
            toneAndEnv.set(PsgParamType::VolumeA, 8);
            toneAndEnv.set(PsgParamType::EnvelopeIsOnA, 1);
            toneAndEnv.set(PsgParamType::EnvelopePeriod, 5000);
            auto notes2 = collect(toneAndEnv);
            expectEquals(int(notes2.size()), 1, "Only tone note, no envelope pitch note expected");
            if (!notes2.empty()) {
                expectEquals(int(notes2[0].channelMask), 1, "Only note should be channel A");
                expect(! notes2[0].isEnvelopePitch, "Only note should be a tone pitch note");
            }
        }

        beginTest("Ultra-high tone (period<8) with env fallback: one shared env pitch note");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::TonePeriodA, 4);   // <8, filtered out as tone
            data.set(PsgParamType::ToneIsOnA, 1);
            data.set(PsgParamType::VolumeA, 10);
            data.set(PsgParamType::EnvelopeIsOnA, 1);
            data.set(PsgParamType::EnvelopePeriod, 100);

            auto notes = collect(data);
            expectEquals(int(notes.size()), 1, "Expected one shared envelope pitch note");

            const PsgFrameNote* envNote = nullptr;
            for (const auto& n : notes) {
                if (n.isEnvelopePitch) envNote = &n;
            }

            expect(envNote != nullptr, "Should have shared envelope pitch note");

            if (envNote) {
                expectEquals(int(envNote->channelMask), 1, "Envelope note mask should be A");
                expect(envNote->isEnvelopePitch, "Envelope note should be marked as envelope pitch");
                expect(envNote->hasEnvMod, "Envelope note should have hasEnvMod=true");
                const float expectedEnvPitch = envelopePeriodToTonePitch(100);
                expectWithinAbsoluteError(envNote->pitch, expectedEnvPitch, 1e-6f,
                    "Envelope note should be at env pitch");
            }
        }

        beginTest("envPeriod=0 plays as envPeriod=1: one shared envelope pitch note");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::EnvelopePeriod, 0);
            data.set(PsgParamType::EnvelopeIsOnA, 1);
            data.set(PsgParamType::ToneIsOnA, 0);

            auto notes = collect(data);
            expectEquals(int(notes.size()), 1, "envPeriod=0 should yield the envPeriod=1 note");
            if (!notes.empty()) {
                expect(notes[0].isEnvelopePitch, "Note should be an envelope pitch note");
                expectWithinAbsoluteError(notes[0].pitch, envelopePeriodToTonePitch(1), 1e-6f,
                    "envPeriod=0 pitch should equal envPeriod=1 pitch");
            }
        }

        beginTest("Three channels tone+env: 4 notes (3 tone notes + 1 envelope note)");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::EnvelopePeriod, 100);
            for (int ch = 0; ch < 3; ++ch) {
                data.set(PsgParamType(PsgParamType::TonePeriodA + ch), static_cast<uint16_t>(200 + ch * 100));
                data.set(PsgParamType(PsgParamType::ToneIsOnA    + ch), 1);
                data.set(PsgParamType(PsgParamType::EnvelopeIsOnA + ch), 1);
            }

            auto notes = collect(data);
            expectEquals(int(notes.size()), 4, "Expected 4 notes: A, B, C + envelope");

            int envNoteCount = 0;
            const PsgFrameNote* envNote = nullptr;
            for (const auto& n : notes) {
                if (n.isEnvelopePitch) {
                    ++envNoteCount;
                    envNote = &n;
                }
            }
            expectEquals(envNoteCount, 1, "Exactly one shared envelope pitch note");
            if (envNote != nullptr)
                expectEquals(int(envNote->channelMask), 7, "Envelope note should be shared by A+B+C");
        }
    }
};

static FrameNotesTests frameNotesTests;

}  // namespace MoTool::Tests
