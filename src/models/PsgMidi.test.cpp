#include <JuceHeader.h>
#include "formats/psg/PsgData.h"
#include "Ids.h"
#include "PsgList.h"
#include "PsgMidi.h"

#include <cstdint>
#include <optional>
#include <string>


namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace tracktion;
using namespace juce;
using namespace std::literals;
using namespace MoTool::uZX;

//==============================================================================
//==============================================================================
class PsgParamsTests  : public UnitTest {
public:
    PsgParamsTests() : UnitTest("PsgParams", "MoTool") {}

    void runTest() override {
        beginTest("PsgParamFrameData getParams");
        {
            PsgParamFrameData data {
                {PsgParamTypeEnum::VolumeA, 1},
                {PsgParamTypeEnum::EnvelopeShape, 1}
            };

            // expect(sizeof(PsgParamFrameData::values) == 42, "Expected 42 bytes, got " + std::to_string(sizeof(PsgParamFrameData::values)));
            // expect(sizeof(PsgParamFrameData::masks) == 21, "Expected 21 bytes, got " + std::to_string(sizeof(PsgParamFrameData::masks)));
            expectEquals(sizeof(PsgParamFrameData), 66ul);

            expectEquals(data.getParams().size(), 2ul);
            expect(data.getParams()[1].first == PsgParamType::EnvelopeShape, "Expected EnvelopeShape");
            expect(data[PsgParamType::VolumeA] == 1, "Expected VolumeA to be 1");
            expect(data[PsgParamType::VolumeB] == std::nullopt, "Expected VolumeB to be nullopt");

            data.unSet(PsgParamType::VolumeA);
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

        beginTest("PsgRegType tone period");
        {
            PsgRegsFrame regs;
            regs.setTonePeriod(0, 0x1234);
            expect(regs.registers[PsgRegType::TonePeriodFineA] == 0x34, "Expected TonePeriodFineA to be 0x34");
            expect(regs.registers[PsgRegType::TonePeriodCoarseA] == 0x12, "Expected TonePeriodCoarseA to be 0x12");
            expect(regs.getTonePeriod(0) == 0x1234, "Expected TonePeriodA to be 0x1234");
            expect(regs.hasTonePeriodSet(0), "Expected TonePeriodB to be set");
            expect(!regs.hasTonePeriodSet(1), "Expected TonePeriodB to not be set");
            regs.setTonePeriodCoarse(0, 0x56);
            expect(regs.registers[PsgRegType::TonePeriodCoarseA] == 0x56, "Expected TonePeriodCoarseA to be 0x56");
            expect(regs.getTonePeriod(0) == 0x5634, "Expected TonePeriodA to be 0x5634");
            regs.setTonePeriodFine(0, 0x78);
            expect(regs.registers[PsgRegType::TonePeriodFineA] == 0x78, "Expected TonePeriodFineA to be 0x78");
            expect(regs.getTonePeriod(0) == 0x5678, "Expected TonePeriodA to be 0x5678");

            regs.setTonePeriod(1, 0x5678);
            expect(regs.registers[PsgRegType::TonePeriodFineB] == 0x78, "Expected TonePeriodFineB to be 0x78");
            expect(regs.registers[PsgRegType::TonePeriodCoarseB] == 0x56, "Expected TonePeriodCoarseB to be 0x56");
            expect(regs.getTonePeriod(1) == 0x5678, "Expected TonePeriodB to be 0x5678");
            expect(regs.hasTonePeriodSet(0), "Expected TonePeriodA to be set");

            regs.setTonePeriod(2, 0x9abc);
            expect(regs.registers[PsgRegType::TonePeriodFineC] == 0xbc, "Expected TonePeriodFineC to be 0xbc");
            expect(regs.registers[PsgRegType::TonePeriodCoarseC] == 0x9a, "Expected TonePeriodCoarseC to be 0x9a");
            expect(regs.getTonePeriod(2) == 0x9abc, "Expected TonePeriodC to be 0x9abc");
            expect(regs.hasTonePeriodSet(2), "Expected TonePeriodC to be set");
        }

        beginTest("PsgRegType volume");
        {
            PsgRegsFrame regs;
            regs.setVolume(0, 1);
            expect(regs.registers[PsgRegType::VolumeA] == 1, "Expected VolumeA to be 1");
            expect(regs.hasVolumeSet(0), "Expected VolumeA to be set");
            expect(!regs.hasVolumeSet(1), "Expected VolumeB to not be set");
            expect(regs.getVolume(0) == 1, "Expected VolumeA to be 1");
            expect(regs.getVolumeAndEnvMod(0) == 1, "Expected VolumeA to be 1");
            expect(!regs.getEnvMod(0), "Expected EnvModA to be false");
            expect(regs.hasVolumeOrEnvModSet(0), "Expected VolumeOrEnvModSet for A to be set");

            regs.setEnvMod(0, true);
            expect(regs.registers[PsgRegType::VolumeA] == 0x11, "Expected VolumeA to be 0x11");
            expect(regs.getVolume(0) == 1, "Expected VolumeA to be 1");
            expect(regs.getEnvMod(0), "Expected EnvModA to be true");

            regs.setEnvMod(1, true);
            expect(regs.registers[PsgRegType::VolumeB] == 0x10, "Expected VolumeB to be 0x10");
            expect(regs.hasVolumeOrEnvModSet(1), "Expected VolumeOrEnvModSet for A to be set");
            expect(!regs.hasVolumeSet(1), "Expected VolumeB to be not set");

            regs.setVolume(1, 2);
            expect(regs.registers[PsgRegType::VolumeB] == 0x12, "Expected VolumeB to be 0x12");
            expect(regs.hasVolumeSet(1), "Expected VolumeB to be set");
            expect(!regs.hasVolumeSet(2), "Expected VolumeC to not be set");
            expect(regs.getVolume(1) == 2, "Expected VolumeB to be 2");

            regs.setVolume(2, 3);
            expect(regs.registers[PsgRegType::VolumeC] == 3, "Expected VolumeC to be 3");
            expect(regs.hasVolumeSet(2), "Expected VolumeC to be set");
            expect(regs.getVolume(2) == 3, "Expected VolumeC to be 3");
        }

        beginTest("PsgRegType noise period");
        {
            PsgRegsFrame regs;
            expect(!regs.hasNoisePeriodSet(), "Expected NoisePeriod to be not set");
            regs.setNoisePeriod(0x1f);
            expect(regs.registers[PsgRegType::NoisePeriod] == 0x1f, "Expected NoisePeriod to be 0xf0");
            expect(regs.hasNoisePeriodSet(), "Expected NoisePeriod to be set");
        }

        beginTest("PsgRegType envelope period");
        {
            PsgRegsFrame regs;
            regs.setEnvelopePeriod(0x1234);
            expect(regs.registers[PsgRegType::EnvelopePeriodFine] == 0x34, "Expected EnvelopePeriodFine to be 0x34");
            expect(regs.registers[PsgRegType::EnvelopePeriodCoarse] == 0x12, "Expected EnvelopePeriodCoarse to be 0x12");
            expect(regs.getEnvelopePeriod() == 0x1234, "Expected EnvelopePeriod to be 0x1234");
            expect(regs.hasEnvelopePeriodSet(), "Expected EnvelopePeriod to be set");
            regs.setEnvelopePeriodCoarse(0x56);
            expect(regs.registers[PsgRegType::EnvelopePeriodCoarse] == 0x56, "Expected EnvelopePeriodCoarse to be 0x56");
            expect(regs.getEnvelopePeriod() == 0x5634, "Expected EnvelopePeriod to be 0x5634");
            regs.setEnvelopePeriodFine(0x78);
            expect(regs.registers[PsgRegType::EnvelopePeriodFine] == 0x78, "Expected EnvelopePeriodFine to be 0x78");
            expect(regs.getEnvelopePeriod() == 0x5678, "Expected EnvelopePeriod to be 0x5678");
        }

        beginTest("PsgRegType envelope shape");
        {
            PsgRegsFrame regs;
            expect(!regs.hasEnvelopeShapeSet(), "Expected EnvelopeShape to be not set");
            regs.setEnvelopeShape(0x0f);
            expect(regs.registers[PsgRegType::EnvelopeShape] == 0x0f, "Expected EnvelopeShape to be 0x0f");
            expect(regs.hasEnvelopeShapeSet(), "Expected EnvelopeShape to be set");
        }

        beginTest("PsgRegType mixer");
        {
            PsgRegsFrame regs;
            expect(!regs.hasMixerSet(), "Expected Mixer to be not set");
            regs.setMixer(0b00010010);  // inverted
            expect(regs.registers[PsgRegType::Mixer] == 0b00010010, "Expected Mixer to be 0x2d");
            expect(regs.hasMixerSet(), "Expected Mixer to be set");

            expect(regs.getToneOn(0), "Expected ToneOnA to be true");
            expect(!regs.getToneOn(1), "Expected ToneOnB to be false");
            expect(regs.getToneOn(2), "Expected ToneOnC to be true");
            expect(regs.getNoiseOn(0), "Expected NoiseOnA to be true");
            expect(!regs.getNoiseOn(1), "Expected NoiseOnB to be false");
            expect(regs.getNoiseOn(2), "Expected NoiseOnC to be true");
        }

        beginTest("PsgParamFrameData from PsgRegsFrame");
        {
            PsgRegsFrame regs;
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
            expect(f[PsgParamType::ToneIsOnB] == true, "Expected ToneIsOnB to be true");
            expect(f[PsgParamType::EnvelopeIsOnA] == true, "Expected EnvelopeIsOnA to be true");

            expect(f[PsgParamType::TonePeriodB] == 0x5678, "Expected TonePeriodB to be 0x5678");
            expect(f[PsgParamType::VolumeB] == 8, "Expected VolumeB to be 8");
            expect(f[PsgParamType::ToneIsOnB] == true, "Expected ToneIsOnB to be true");

            expect(f[PsgParamType::TonePeriodC] == std::nullopt, "Expected TonePeriodC to be nullopt");
            expect(f[PsgParamType::VolumeC] == std::nullopt, "Expected VolumeC to be nullopt");
            // we can not detect if this was set separately from other mixer bits
            expect(f[PsgParamType::ToneIsOnC] == true, "Expected ToneIsOnC to be true");

            expect(f[PsgParamType::NoisePeriod] == 0xf0, "Expected NoisePeriod to be 0xf0");
            expect(f[PsgParamType::EnvelopePeriod] == 0x1234, "Expected EnvelopePeriod to be 0x1234");
            expect(f[PsgParamType::EnvelopeShape] == 0x0f, "Expected EnvelopeShape to be 0x0f");
        }

        beginTest("PsgParamFrameData from PsgRegsFrame series");
        {
            static constexpr std::array<PsgRegsFrame, 3> testRegFrames {
                {{
                //  Period A       Period B      Period C      Noise Mxr NNNTTT  Vol A  Vol B  Vol C  Env Period    Shape
                    {0x12,  0x34,  0x00,  0x00,  0x57,  0x13,  0x1f, 0b00110101, 0x03,  0x01,  0x00,  0x00,  0x00,  0x00},
                    {true,  true,  false, false, true,  true,  true, true,       true,  true,  false, false, false, false}
                }, {
                    {0x00,  0x00,  0x56,  0x32,  0x00,  0x24,  0x00, 0b00101110, 0x00,  0x18,  0x01,  0x12,  0x34,  0x08},
                    {false, false, true,  true,  false, true,  true, true,       false, true,  true,  true,  true,  true}
                }, {
                    {0x00,  0x33,  0x00,  0x00,  0x34,  0x00,  0x10, 0b00000000, 0x13,  0x00,  0x11,  0x24,  0x00,  0x00},
                    {false, true,  true,  false, true,  false, true, false,      true,  false, false, true,  false, false}
                }}
            };

            static constexpr std::array<std::array<uint8_t, 14>, 3> expectedCumulRegStates {
                {{
                //  Period A       Period B      Period C      Noise Mxr NNNTTT  Vol A  Vol B  Vol C  Env Period    Shape
                    {0x12,  0x34,  0x00,  0x00,  0x57,  0x13,  0x1f, 0b00110101, 0x03,  0x01,  0x00,  0x00,  0x00,  0x00},
                }, {
                    {0x12,  0x34,  0x56,  0x32,  0x57,  0x24,  0x00, 0b00101110, 0x03,  0x18,  0x01,  0x12,  0x34,  0x08},
                }, {
                    {0x12,  0x33,  0x00,  0x32,  0x34,  0x24,  0x10, 0b00101110, 0x13,  0x18,  0x01,  0x24,  0x34,  0x08},
                }}
            };

            static std::array<PsgParamFrameData, 3> expectedParams {{
                {
                    {PsgParamType::TonePeriodA,   0x3412},
                    {PsgParamType::TonePeriodC,   0x1357},
                    {PsgParamType::NoisePeriod,   0x1f},
                    {PsgParamType::ToneIsOnA,     false},
                    {PsgParamType::ToneIsOnB,     true},
                    {PsgParamType::ToneIsOnC,     false},
                    {PsgParamType::NoiseIsOnA,    true},
                    {PsgParamType::NoiseIsOnB,    false},
                    {PsgParamType::NoiseIsOnC,    false},
                    {PsgParamType::VolumeA,       0x03},
                    {PsgParamType::VolumeB,       0x01},
                    {PsgParamType::EnvelopeIsOnA, false},
                    {PsgParamType::EnvelopeIsOnB, false},
                },
                {
                    {PsgParamType::TonePeriodB,   0x3256},
                    {PsgParamType::TonePeriodC,   0x2457},
                    {PsgParamType::NoisePeriod,   0x00},
                    {PsgParamType::ToneIsOnA,     true},
                    {PsgParamType::ToneIsOnB,     false},
                    {PsgParamType::ToneIsOnC,     false},
                    {PsgParamType::NoiseIsOnA,    false},
                    {PsgParamType::NoiseIsOnB,    true},
                    {PsgParamType::NoiseIsOnC,    false},
                    {PsgParamType::VolumeB,       0x08},
                    {PsgParamType::VolumeC,       0x01},
                    {PsgParamType::EnvelopeIsOnB, true},
                    {PsgParamType::EnvelopeIsOnC, false},
                    {PsgParamType::EnvelopePeriod,0x3412},
                    {PsgParamType::EnvelopeShape, 0x08}
                },
                {
                    {PsgParamType::TonePeriodA,   0x3312},
                    {PsgParamType::TonePeriodB,   0x3200},
                    {PsgParamType::TonePeriodC,   0x2434},
                    {PsgParamType::NoisePeriod,   0x10},
                    {PsgParamType::VolumeA,       0x03},
                    {PsgParamType::EnvelopeIsOnA, true},
                    {PsgParamType::EnvelopePeriod,0x3424},
                }
            }};

            PsgRegsFrame state;
            for (size_t i = 0; i < testRegFrames.size(); ++i) {
                state.clear();
                state.update(testRegFrames[i]);
                auto params = PsgParamFrameData {state};

                auto& expected = expectedCumulRegStates[i];
                expect(state.registers == expected, "Unexpected register state in index " + std::to_string(i));
                if (state.registers != expected) {
                    DBG("------------------------------");
                    DBG("--- reg state ---");
                    state.debugPrintSet();
                }

                expect(params.getParams() == expectedParams[i].getParams(), "Unexpected params in index " + std::to_string(i));
                if (params.getParams() != expectedParams[i].getParams()) {
                    DBG("------------------------------");
                    DBG("--- reg state ---");
                    state.debugPrintSet();
                    DBG("--- existing ---");
                    params.debugPrintSet();
                    DBG("--- expected ---");
                    expectedParams[i].debugPrintSet();
                }
            }

        }

        beginTest("PsgParamFrame::createPsgFrameValueTree");
        {
            PsgParamFrameData data {
                {PsgParamType::VolumeA, 1},
                {PsgParamType::EnvelopeShape, 2}
            };
            auto v = PsgParamFrame::createPsgFrameValueTree(2.3_bp, data);
            expect(v.hasType(IDs::FRAME), "Expected FRAME type");
            expect(v.getProperty(te::IDs::b).equals(2.3), "Expected beat number to be 2.3");
            expect(v.getProperty(IDs::va).equals(1), "Expected VolumeA to be 1");
            expect(v.getProperty(IDs::va).isInt(), "Expected VolumeA to be an int");
            expect(!v.hasProperty(IDs::vb), "Expected VolumeB to be missing");
            expect(v.getProperty(IDs::s).equals(2), "Expected EnvelopeShape to be 2");
        }

        beginTest("PsgParamFrame ctor");
        {
            PsgParamFrameData data {
                {PsgParamType::VolumeA, 1},
                {PsgParamType::EnvelopeShape, 2}

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

class PsgParamsChangeTrackingTest  : public UnitTest {
public:
    PsgParamsChangeTrackingTest() : UnitTest("PsgParamsChangeTracker", "MoTool") {}

    void runTest() override {
        beginTest("PsgParamsMidiWriterTests not changed");
        {
            std::vector<std::pair<PsgParamType, uint16_t>> fullChange {
                {PsgParamType::VolumeA,           1},
                {PsgParamType::VolumeB,           1},
                {PsgParamType::VolumeC,           2},
                {PsgParamType::TonePeriodA,       3},
                {PsgParamType::TonePeriodB,       4},
                {PsgParamType::TonePeriodC,       5},
                {PsgParamType::ToneIsOnA,         6},
                {PsgParamType::ToneIsOnB,         7},
                {PsgParamType::ToneIsOnC,         8},
                {PsgParamType::NoiseIsOnA,        9},
                {PsgParamType::NoiseIsOnB,        10},
                {PsgParamType::NoiseIsOnC,        11},
                {PsgParamType::EnvelopeIsOnA,     12},
                {PsgParamType::EnvelopeIsOnB,     13},
                {PsgParamType::EnvelopeIsOnC,     14},
                {PsgParamType::NoisePeriod,       15},
                {PsgParamType::EnvelopePeriod,    16},
                {PsgParamType::EnvelopeShape,     17},
                {PsgParamType::RetriggerToneA,    18},
                {PsgParamType::RetriggerToneB,    19},
                {PsgParamType::RetriggerToneC,    20},
                {PsgParamType::RetriggerEnvelope, 21},
            };
            PsgParamFrameData data {fullChange};
            expect(data.getParams() == fullChange, "Expected all params to be set");

            data.update(data);
            expect(data.getParams() == std::vector<std::pair<PsgParamType, uint16_t>> {}, "Expected no params to be changed");

            PsgParamFrameData partialData {{
                {PsgParamType::VolumeA,           15, true},  // here
                {PsgParamType::VolumeB,            1, false},
                {PsgParamType::VolumeC,            2, false},
                {PsgParamType::TonePeriodA,        3, false},
                {PsgParamType::TonePeriodB,        4, false},
                {PsgParamType::TonePeriodC,        5, false},
                {PsgParamType::ToneIsOnA,          6, false},
                {PsgParamType::ToneIsOnB,          7, false},
                {PsgParamType::ToneIsOnC,          1, true},
                {PsgParamType::NoiseIsOnA,         9, false},  // here
                {PsgParamType::NoiseIsOnB,        10, false},
                {PsgParamType::NoiseIsOnC,        11, false},
                {PsgParamType::EnvelopeIsOnA,     12, false},
                {PsgParamType::EnvelopeIsOnB,     13, false},
                {PsgParamType::EnvelopeIsOnC,     14, false},
                {PsgParamType::NoisePeriod,       15, false},
                {PsgParamType::EnvelopePeriod,   100, false},  // here but not set
                {PsgParamType::EnvelopeShape,     17, false},
                {PsgParamType::RetriggerToneA,    18, false},
                {PsgParamType::RetriggerToneB,    19, false},
                {PsgParamType::RetriggerToneC,    20, false},
                {PsgParamType::RetriggerEnvelope, 21, false},
            }};
            data.update(partialData);
            expect(data.getParams() == std::vector<std::pair<PsgParamType, uint16_t>> {
                {PsgParamType::VolumeA, 15},
                {PsgParamType::ToneIsOnC, 1},
            }, "Expected 2 params to be changed");
        }
    }
};

class PsgParamsToRegistersTest  : public UnitTest {
public:
    PsgParamsToRegistersTest() : UnitTest("PsgParamsToRegisters", "MoTool") {}

    void runTest() override {
        beginTest("PsgParamsToRegistersTest");
        {
            PsgParamFrameData data {
                {PsgParamType::VolumeA, 1},
                {PsgParamType::VolumeB, 8},
                {PsgParamType::TonePeriodA, 0x1234},
                {PsgParamType::TonePeriodB, 0x5678},
                {PsgParamType::ToneIsOnA, 1},
                {PsgParamType::ToneIsOnB, 1},
                {PsgParamType::ToneIsOnC, 0},
                {PsgParamType::NoiseIsOnA, 1},
                {PsgParamType::NoiseIsOnB, 0},
                {PsgParamType::NoiseIsOnC, 0},
                {PsgParamType::EnvelopeIsOnA, 0},
                {PsgParamType::EnvelopeIsOnB, 1},
                {PsgParamType::EnvelopeIsOnC, 0},
                {PsgParamType::NoisePeriod, 0x10},
                {PsgParamType::EnvelopePeriod, 0x4321},
                {PsgParamType::EnvelopeShape, 2}
            };
            auto regs = data.toRegisters();

            expect(regs.registers[PsgRegType::VolumeA] == 0x1, "Expected VolumeA to be 0x1");
            expect(regs.registers[PsgRegType::VolumeB] == 0x18, "Expected VolumeB to be 0x18");  // env mod
            expect(regs.registers[PsgRegType::TonePeriodFineA]   == 0x34, "Expected TonePeriodFineA to be 0x34");
            expect(regs.registers[PsgRegType::TonePeriodCoarseA] == 0x12, "Expected TonePeriodCoarseA to be 0x12");
            expect(regs.registers[PsgRegType::TonePeriodFineB]   == 0x78, "Expected TonePeriodFineB to be 0x78");
            expect(regs.registers[PsgRegType::TonePeriodCoarseB] == 0x56, "Expected TonePeriodCoarseB to be 0x56");
            expect(regs.registers[PsgRegType::TonePeriodFineC]   == 0x00, "Expected TonePeriodFineC to be 0");
            expect(regs.registers[PsgRegType::TonePeriodCoarseC] == 0x00, "Expected TonePeriodCoarseC to be 0");
            expect(regs.registers[PsgRegType::Mixer] == 0b00110100, "Expected Mixer to be 0b00110100, got "
                + std::to_string(regs.registers[PsgRegType::Mixer]));

            expect(regs.registers[PsgRegType::NoisePeriod] == 0x10, "Expected NoisePeriod to be 0x10");
            expect(regs.registers[PsgRegType::EnvelopePeriodFine] == 0x21, "Expected EnvelopePeriodFine to be 0x21");
            expect(regs.registers[PsgRegType::EnvelopePeriodCoarse] == 0x43, "Expected EnvelopePeriodCoarse to be 0x43");
            expect(regs.registers[PsgRegType::EnvelopeShape] == 2, "Expected EnvelopeShape to be 2");
        }
    }
};



class PsgParamsMidiConverterTests  : public UnitTest {
public:
    PsgParamsMidiConverterTests() : UnitTest("PsgParamsMidiConverter", "MoTool") {}

    void compareEvents(const juce::MidiMessageSequence &sequence,
                        const std::map<double, std::set<std::string>> &expected) {

        std::map<double, std::set<std::string>> actual;
        for (auto e : sequence) {
            actual[e->message.getTimeStamp()].insert(e->message.getDescription().toStdString());
        }

        expectEquals(actual.size(), expected.size(), "Different number of timestamps");

        for (const auto &[timestamp, expectedEvents] : expected) {
            expect(actual.contains(timestamp), "Missing timestamp: " + std::to_string(timestamp));
            if (actual.contains(timestamp)) {
                expectEquals(actual.at(timestamp).size(), expected.at(timestamp).size(),
                    "Wrong number of events at timestamp: " + std::to_string(timestamp));
                for (const auto& expectedStr : expected.at(timestamp)) {
                    expect(actual.at(timestamp).contains(expectedStr),
                        "Missing expected event at " + std::to_string(timestamp) + ": " + expectedStr);
                }
            }
        }
    }

    static std::vector<PsgRegsFrame> getTestRegFrames() {
        return {{
            {0x12, 0x34, 0,     0,     0,     0,     0x1f, 0b00110110, 0x3,   0,     0,     0,     0,     0},
            {true, true, false, false, false, false, true, true,       true,  false, false, false, false, false}
        }, {
            {0,     0,     0x56, 0x32, 0,     0,     0x10, 0b00111100, 0xf,   0x18,  0,     0x12,  0x34,  0x8},
            {false, false, true, true, false, false, true, true,       true,  true,  false, true,  true,  true}
        }};
    }

    inline static void addEvent(juce::MidiMessageSequence& sequence, double time, int channel, MidiCCType type, int value) {
        sequence.addEvent(juce::MidiMessage::controllerEvent(channel, static_cast<int>(type), value), time);
    }

    void runTest() override {
        beginTest("PsgParamsMidiWriterTests write");
        {
            PsgParamsMidiWriter writer {1};
            auto frames = getTestRegFrames();

            PsgParamFrameData params {};
            for (size_t i = 0; i < frames.size(); ++i) {
                params.update(frames[i]);
                writer.write(0.02 * static_cast<double>(i), params);
            }
            auto seq = writer.getSequence();
            // for (auto e : seq) {
            //     DBG(e->message.getDescription() << " @" << e->message.getTimeStamp());
            // }
            // DBG("----------------------------------------------");

            // group by timestamp and compare sets or maps
            std::map<double, std::set<std::string>> expectedEvents = {
                {0.0, {
                    "Controller Volume (coarse): 3 Channel 1",
                    "Controller 20: 104 Channel 1",
                    "Controller 52: 18 Channel 1",
                    "Controller General Purpose Button 1 (on/off): 1 Channel 1",
                    "Controller General Purpose Button 2 (on/off): 1 Channel 1",
                    "Controller Breath controller (coarse): 31 Channel 4"
                }},
                {0.02, {
                    "Controller Volume (coarse): 15 Channel 1",
                    "Controller Volume (coarse): 8 Channel 2",
                    "Controller 20: 100 Channel 2",
                    "Controller 52: 86 Channel 2",
                    "Controller General Purpose Button 1 (on/off): 1 Channel 2",
                    "Controller General Purpose Button 2 (on/off): 0 Channel 1",
                    "Controller General Purpose Button 3 (on/off): 1 Channel 2",
                    "Controller Breath controller (coarse): 16 Channel 4",
                    "Controller 20: 104 Channel 4",
                    "Controller 52: 18 Channel 4",
                    "Controller Sound Variation: 8 Channel 4"
                }}
            };
            compareEvents(seq, expectedEvents);
        }

        beginTest("PsgParamsMidiReader read");
        {
            juce::MidiMessageSequence seq;
            addEvent(seq, 0.00, 1, MidiCCType::Volume, 3);
            addEvent(seq, 0.00, 1, MidiCCType::CC20PeriodCoarse, 104);
            addEvent(seq, 0.00, 1, MidiCCType::CC52PeriodFine, 18);
            addEvent(seq, 0.00, 2, MidiCCType::GPB1, 0);
            addEvent(seq, 0.00, 2, MidiCCType::GPB2, 0);
            addEvent(seq, 0.00, 3, MidiCCType::GPB1, 0);
            addEvent(seq, 0.00, 3, MidiCCType::GPB2, 0);
            addEvent(seq, 0.00, 4, MidiCCType::Breath, 31);

            addEvent(seq, 0.02, 1, MidiCCType::Volume, 15);
            addEvent(seq, 0.02, 2, MidiCCType::Volume, 8);
            addEvent(seq, 0.02, 2, MidiCCType::CC20PeriodCoarse, 100);
            addEvent(seq, 0.02, 2, MidiCCType::CC52PeriodFine, 86);
            addEvent(seq, 0.02, 2, MidiCCType::GPB1, 1);
            addEvent(seq, 0.02, 1, MidiCCType::GPB2, 0);
            addEvent(seq, 0.02, 2, MidiCCType::GPB3, 1);
            addEvent(seq, 0.02, 4, MidiCCType::Breath, 16);
            addEvent(seq, 0.02, 4, MidiCCType::CC20PeriodCoarse, 104);
            addEvent(seq, 0.02, 4, MidiCCType::CC52PeriodFine, 18);
            addEvent(seq, 0.02, 4, MidiCCType::SoundVariation, 8);

            PsgParamsMidiReader reader {1};
            std::vector<PsgParamFrameData> frames;
            for (auto e : seq) {
                auto newParams = reader.read(e->message);
                if (newParams.has_value()) {
                    frames.push_back(newParams.value());
                }
            }
            frames.push_back(reader.getParams());
            // frames[0].debugPrint();
            // DBG("----------------------------------------------");

            expect(frames.size() == 2, "Expected 2 frames, got " + std::to_string(frames.size()));
            expect(frames[0].getParams().size() == 7,
                "Expected 7 params, got " + std::to_string(frames[0].getParams().size()));
            expect(frames[1].getParams().size() == 9,
                "Expected 9 params, got " + std::to_string(frames[1].getParams().size()));

            expect(frames[0][PsgParamType::VolumeA] == 3, "Expected VolumeA to be 3");
            expect(frames[0][PsgParamType::VolumeB] == std::nullopt,
                "Expected VolumeB to be nullopt");
            expect(frames[0][PsgParamType::TonePeriodA] == 0x3412,
                "Expected TonePeriodA to be 0x3412, got "
                + std::to_string(frames[0][PsgParamType::TonePeriodA].value_or(-1)));
            expect(frames[0].getRaw(PsgParamType::ToneIsOnA) == 1, "Expected ToneIsOnA be true");
            expect(frames[0][PsgParamType::ToneIsOnB] == 0,
                "Expected ToneIsOnB be 0");
            expect(frames[0].getRaw(PsgParamType::NoiseIsOnA) == 1, "Expected NoiseIsOnA be true");
            expect(frames[0][PsgParamType::NoiseIsOnB] == 0,
                "Expected NoiseIsOnB be 0");

            expect(frames[0][PsgParamType::NoisePeriod] == 0x1f, "Expected NoisePeriod be 0x1f");

            // frames[1].debugPrint();
            // DBG("----------------------------------------------");

            expect(frames[1][PsgParamType::VolumeA] == 15, "Expected VolumeA to be 15");
            expect(frames[1][PsgParamType::VolumeB] == 8, "Expected VolumeB to be 8");
            expect(frames[1][PsgParamType::VolumeC] == std::nullopt, "Expected VolumeC to be nullopt");
            expect(frames[1][PsgParamType::TonePeriodA] == std::nullopt,
                "Expected TonePeriodA to be std::nullopt, got "
                + std::to_string(frames[1][PsgParamType::TonePeriodA].value_or(-1)));
            expect(frames[1][PsgParamType::TonePeriodB] == 0x3256,
                "Expected TonePeriodB to be 0x3256, got "
                + std::to_string(frames[1][PsgParamType::TonePeriodB].value_or(-1)));
            expect(frames[1][PsgParamType::ToneIsOnA]      == std::nullopt, "Expected TonePeriodA to be nullopt");
            expect(frames[1][PsgParamType::ToneIsOnB]      == 1,            "Expected ToneIsOnB be 0");
            expect(frames[1][PsgParamType::ToneIsOnC]      == std::nullopt, "Expected ToneIsOnC be nullopt");
            expect(frames[1][PsgParamType::NoiseIsOnA]     == 0,            "Expected NoiseIsOnA be 0");
            expect(frames[1][PsgParamType::NoiseIsOnB]     == std::nullopt, "Expected NoiseIsOnB be nullopt");
            expect(frames[1][PsgParamType::EnvelopeIsOnA]  == std::nullopt, "Expected EnvelopeIsOnA be nullopt");
            expect(frames[1][PsgParamType::EnvelopeIsOnB]  == 1,            "Expected EnvelopeIsOnB be 1");
            expect(frames[1][PsgParamType::NoisePeriod]    == 0x10,         "Expected NoisePeriod be 0x10");
            expect(frames[1][PsgParamType::EnvelopePeriod] == 0x3412,       "Expected EnvelopePeriod be 0x3412, got "
                + std::to_string(frames[1][PsgParamType::EnvelopePeriod].value_or(-1)));
            expect(frames[1][PsgParamType::EnvelopeShape]  == 8,            "Expected EnvelopeShape be 8");

        }
    }
};

static PsgParamsTests psgParamsTests;
static PsgParamsChangeTrackingTest psgParamsChangeTrackingTests;
static PsgParamsToRegistersTest psgParamsToRegisterTest;
static PsgParamsMidiConverterTests psgParamsMidiConverterTests;

}  // namespace MoTool::Tests