#include "juce_core/juce_core.h"
#include "model/Ids.h"
#include <JuceHeader.h>

#include <string>
#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>
#include <model/PsgMidi.h>


namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace tracktion;
using namespace juce;
using namespace std::literals;

//==============================================================================
//==============================================================================
class PsgParamsMidiTests  : public UnitTest {
public:
    PsgParamsMidiTests() : UnitTest("PsgParams", "MoTool") {}

    void runTest() override {
        // auto& engine = *Engine::getEngines()[0];
        // Clipboard clipboard;
        // auto edit = Edit::createSingleTrackEdit(engine);

        beginTest("PsgParamFrameData getParams");
        {
            PsgParamFrameData data {
                {
                    1, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 1
                },
                {
                    true, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false,
                    false, false, false, false, true
                }
            };

            // expect(sizeof(PsgParamFrameData::values) == 42, "Expected 42 bytes, got " + std::to_string(sizeof(PsgParamFrameData::values)));
            // expect(sizeof(PsgParamFrameData::masks) == 21, "Expected 21 bytes, got " + std::to_string(sizeof(PsgParamFrameData::masks)));
            expect(sizeof(PsgParamFrameData) == 64, "Expected 64 bytes, got " + std::to_string(sizeof(PsgParamFrameData)));

            expect(data.getParams().size() == 2, "Expected 2 params, but got " + std::to_string(data.getParams().size()));
            expect(data.getParams()[1].first == PsgParamType::EnvelopeShape, "Expected EnvelopeShape");
            expect(data[PsgParamType::VolumeA] == 1, "Expected VolumeA to be 1");
            expect(data[PsgParamType::VolumeB] == std::nullopt, "Expected VolumeB to be nullopt");

            data.remove(PsgParamType::VolumeA);
            expect(data.getParams().size() == 1, "Expected 1 param, but got " + std::to_string(data.getParams().size()));
            expect(data[PsgParamType::VolumeA] == std::nullopt, "Expected VolumeB to be nullopt");
            expect(data.getParams()[0].first == PsgParamType::EnvelopeShape, "Expected EnvelopeShape");
        }

        beginTest("PsgParamFrameData getters/setters");
        {
            PsgParamFrameData data;
            data.set(PsgParamType::VolumeB, 0x1234);

            expect(data[PsgParamType::VolumeB] == 0x1234, "Expected VolumeB to be 0x1234");
        }

        beginTest("PsgParamFrame::createPsgFrameValueTree");
        {
            PsgParamFrameData data {
                {
                    1, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 2
                },
                {
                    true, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false,
                    false, false, false, false, true
                }
            };
            auto v = PsgParamFrame::createPsgFrameValueTree(2.3_bp, data);
            expect(v.hasType(IDs::FRAME), "Expected FRAME type");
            expect(v.getProperty(te::IDs::b).equals(2.3), "Expected beat number to be 2.3");
            expect(v.getProperty(IDs::va).equals(1), "Expected VolumeA to be 1");
            expect(v.getProperty(IDs::va).isInt(), "Expected VolumeA to be an int");
            expect(!v.hasProperty(IDs::vb), "Expected VolumeB to be missing");
            expect(v.getProperty(IDs::s).equals(2), "Expected EnvelopeShape to be 1");
        }

        beginTest("uZX::PsgRegsFrame tone period");
        {
            uZX::PsgRegsFrame regs;
            regs.setTonePeriod(0, 0x1234);
            expect(regs.registers[uZX::PsgRegsFrame::TonePeriodFineA] == 0x34, "Expected TonePeriodFineA to be 0x34");
            expect(regs.registers[uZX::PsgRegsFrame::TonePeriodCoarseA] == 0x12, "Expected TonePeriodCoarseA to be 0x12");
            expect(regs.getTonePeriod(0) == 0x1234, "Expected TonePeriodA to be 0x1234");
            expect(regs.hasTonePeriodSet(0), "Expected TonePeriodB to be set");
            expect(!regs.hasTonePeriodSet(1), "Expected TonePeriodB to not be set");
            regs.setTonePeriodCoarse(0, 0x56);
            expect(regs.registers[uZX::PsgRegsFrame::TonePeriodCoarseA] == 0x56, "Expected TonePeriodCoarseA to be 0x56");
            expect(regs.getTonePeriod(0) == 0x5634, "Expected TonePeriodA to be 0x5634");
            regs.setTonePeriodFine(0, 0x78);
            expect(regs.registers[uZX::PsgRegsFrame::TonePeriodFineA] == 0x78, "Expected TonePeriodFineA to be 0x78");
            expect(regs.getTonePeriod(0) == 0x5678, "Expected TonePeriodA to be 0x5678");

            regs.setTonePeriod(1, 0x5678);
            expect(regs.registers[uZX::PsgRegsFrame::TonePeriodFineB] == 0x78, "Expected TonePeriodFineB to be 0x78");
            expect(regs.registers[uZX::PsgRegsFrame::TonePeriodCoarseB] == 0x56, "Expected TonePeriodCoarseB to be 0x56");
            expect(regs.getTonePeriod(1) == 0x5678, "Expected TonePeriodB to be 0x5678");
            expect(regs.hasTonePeriodSet(0), "Expected TonePeriodA to be set");

            regs.setTonePeriod(2, 0x9abc);
            expect(regs.registers[uZX::PsgRegsFrame::TonePeriodFineC] == 0xbc, "Expected TonePeriodFineC to be 0xbc");
            expect(regs.registers[uZX::PsgRegsFrame::TonePeriodCoarseC] == 0x9a, "Expected TonePeriodCoarseC to be 0x9a");
            expect(regs.getTonePeriod(2) == 0x9abc, "Expected TonePeriodC to be 0x9abc");
            expect(regs.hasTonePeriodSet(2), "Expected TonePeriodC to be set");
        }

        beginTest("uZX::PsgRegsFrame volume");
        {
            uZX::PsgRegsFrame regs;
            regs.setVolume(0, 1);
            expect(regs.registers[uZX::PsgRegsFrame::VolumeA] == 1, "Expected VolumeA to be 1");
            expect(regs.hasVolumeSet(0), "Expected VolumeA to be set");
            expect(!regs.hasVolumeSet(1), "Expected VolumeB to not be set");
            expect(regs.getVolume(0) == 1, "Expected VolumeA to be 1");
            expect(regs.getVolumeAndEnvMod(0) == 1, "Expected VolumeA to be 1");
            expect(!regs.getEnvMod(0), "Expected EnvModA to be false");
            expect(regs.hasVolumeOrEnvModSet(0), "Expected VolumeOrEnvModSet for A to be set");

            regs.setEnvMod(0, true);
            expect(regs.registers[uZX::PsgRegsFrame::VolumeA] == 0x11, "Expected VolumeA to be 0x11");
            expect(regs.getVolume(0) == 1, "Expected VolumeA to be 1");
            expect(regs.getEnvMod(0), "Expected EnvModA to be true");

            regs.setEnvMod(1, true);
            expect(regs.registers[uZX::PsgRegsFrame::VolumeB] == 0x10, "Expected VolumeB to be 0x10");
            expect(regs.hasVolumeOrEnvModSet(1), "Expected VolumeOrEnvModSet for A to be set");
            expect(!regs.hasVolumeSet(1), "Expected VolumeB to be not set");

            regs.setVolume(1, 2);
            expect(regs.registers[uZX::PsgRegsFrame::VolumeB] == 0x12, "Expected VolumeB to be 0x12");
            expect(regs.hasVolumeSet(1), "Expected VolumeB to be set");
            expect(!regs.hasVolumeSet(2), "Expected VolumeC to not be set");
            expect(regs.getVolume(1) == 2, "Expected VolumeB to be 2");

            regs.setVolume(2, 3);
            expect(regs.registers[uZX::PsgRegsFrame::VolumeC] == 3, "Expected VolumeC to be 3");
            expect(regs.hasVolumeSet(2), "Expected VolumeC to be set");
            expect(regs.getVolume(2) == 3, "Expected VolumeC to be 3");
        }

        beginTest("uZX::PsgRegsFrame noise period");
        {
            uZX::PsgRegsFrame regs;
            expect(!regs.hasNoisePeriodSet(), "Expected NoisePeriod to be not set");
            regs.setNoisePeriod(0x1f);
            expect(regs.registers[uZX::PsgRegsFrame::NoisePeriod] == 0x1f, "Expected NoisePeriod to be 0xf0");
            expect(regs.hasNoisePeriodSet(), "Expected NoisePeriod to be set");
        }

        beginTest("uZX::PsgRegsFrame envelope period");
        {
            uZX::PsgRegsFrame regs;
            regs.setEnvelopePeriod(0x1234);
            expect(regs.registers[uZX::PsgRegsFrame::EnvelopePeriodFine] == 0x34, "Expected EnvelopePeriodFine to be 0x34");
            expect(regs.registers[uZX::PsgRegsFrame::EnvelopePeriodCoarse] == 0x12, "Expected EnvelopePeriodCoarse to be 0x12");
            expect(regs.getEnvelopePeriod() == 0x1234, "Expected EnvelopePeriod to be 0x1234");
            expect(regs.hasEnvelopePeriodSet(), "Expected EnvelopePeriod to be set");
            regs.setEnvelopePeriodCoarse(0x56);
            expect(regs.registers[uZX::PsgRegsFrame::EnvelopePeriodCoarse] == 0x56, "Expected EnvelopePeriodCoarse to be 0x56");
            expect(regs.getEnvelopePeriod() == 0x5634, "Expected EnvelopePeriod to be 0x5634");
            regs.setEnvelopePeriodFine(0x78);
            expect(regs.registers[uZX::PsgRegsFrame::EnvelopePeriodFine] == 0x78, "Expected EnvelopePeriodFine to be 0x78");
            expect(regs.getEnvelopePeriod() == 0x5678, "Expected EnvelopePeriod to be 0x5678");
        }

        beginTest("uZX::PsgRegsFrame envelope shape");
        {
            uZX::PsgRegsFrame regs;
            expect(!regs.hasEnvelopeShapeSet(), "Expected EnvelopeShape to be not set");
            regs.setEnvelopeShape(0x0f);
            expect(regs.registers[uZX::PsgRegsFrame::EnvelopeShape] == 0x0f, "Expected EnvelopeShape to be 0x0f");
            expect(regs.hasEnvelopeShapeSet(), "Expected EnvelopeShape to be set");
        }

        beginTest("uZX::PsgRegsFrame mixer");
        {
            uZX::PsgRegsFrame regs;
            expect(!regs.hasMixerSet(), "Expected Mixer to be not set");
            regs.setMixer(0x2d);
            expect(regs.registers[uZX::PsgRegsFrame::Mixer] == 0x2d, "Expected Mixer to be 0x2d");
            expect(regs.hasMixerSet(), "Expected Mixer to be set");

            expect(regs.getToneOn(0), "Expected ToneOnA to be true");
            expect(!regs.getToneOn(1), "Expected ToneOnB to be false");
            expect(regs.getToneOn(2), "Expected ToneOnC to be true");
            expect(regs.getNoiseOn(0), "Expected NoiseOnA to be true");
            expect(!regs.getNoiseOn(1), "Expected NoiseOnB to be false");
            expect(regs.getNoiseOn(2), "Expected NoiseOnC to be true");
        }

        beginTest("PsgParamFrameData from uZX::PsgRegsFrame");
        {
            uZX::PsgRegsFrame regs;
            regs.setTonePeriod(0, 0x1234);
            regs.setToneOn(0, true);
            regs.setEnvMod(0, true);

            regs.setTonePeriod(1, 0x5678);
            regs.setToneOn(1, true);
            regs.setNoiseOn(1, true);
            regs.setVolume(1, 8);

            regs.setNoisePeriod(0xf0);
            regs.setEnvelopePeriod(0x1234);
            regs.setEnvelopeShape(0x0f);

            auto f = PsgParamFrameData {regs};

            expect(f[PsgParamType::TonePeriodA] == 0x1234, "Expected TonePeriodA to be 0x1234");
            expect(f[PsgParamType::VolumeA] == 0, "Expected VolumeA to be 0");
            expect(f[PsgParamType::ToneIsOnA] == true, "Expected ToneIsOnA to be true");
            expect(f[PsgParamType::EnvelopeIsOnA] == true, "Expected EnvelopeIsOnA to be true");

            expect(f[PsgParamType::TonePeriodB] == 0x5678, "Expected TonePeriodB to be 0x5678");
            expect(f[PsgParamType::VolumeB] == 8, "Expected VolumeB to be 8");
            expect(f[PsgParamType::ToneIsOnB] == true, "Expected ToneIsOnB to be true");

            expect(f[PsgParamType::TonePeriodC] == std::nullopt, "Expected TonePeriodC to be nullopt");
            expect(f[PsgParamType::VolumeC] == std::nullopt, "Expected VolumeC to be nullopt");
            // we can not detect if this was set separately from other mixer bits
            expect(f[PsgParamType::ToneIsOnC] == false, "Expected ToneIsOnC to be false");

            expect(f[PsgParamType::NoisePeriod] == 0xf0, "Expected NoisePeriod to be 0xf0");
            expect(f[PsgParamType::EnvelopePeriod] == 0x1234, "Expected EnvelopePeriod to be 0x1234");
            expect(f[PsgParamType::EnvelopeShape] == 0x0f, "Expected EnvelopeShape to be 0x0f");
        }

        beginTest("PsgParamFrame ctor");
        {
            PsgParamFrameData data {
                {
                    1, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 2
                },
                {
                    true, false, false, false, false, false, false, false,
                    false, false, false, false, false, false, false, false,
                    false, false, false, false, true
                }
            };
            auto v = PsgParamFrame::createPsgFrameValueTree(2.3_bp, data);
            PsgParamFrame frame(v);

            expectWithinAbsoluteError(frame.getBeatPosition().inBeats(), 2.3, 0.0001, "Expected beat number to be 2.3");

            expect(frame.getParam(PsgParamType::VolumeA) == 1, "Expected VolumeA to be 1, got " + std::to_string(frame.getParam(PsgParamType::VolumeA).value_or(-1)));
            expect(frame.getParam(PsgParamType::VolumeB) == std::nullopt, "Expected VolumeB to be nullopt");
            expect(frame.getParam(PsgParamType::EnvelopeShape) == 2, "Expected EnvelopeShape to be 2");
        }
    }
};

static PsgParamsMidiTests psgParamsMidiTests;

}  // namespace MoTool::Tests