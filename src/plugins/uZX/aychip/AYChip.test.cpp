#include <JuceHeader.h>

#include "aychip.h"

#include <cstddef>

// #include "TestUtils.h"

using namespace MoTool::uZX;

namespace {
    // for example to bypass initial click
    void bypassBlock(AyumiEmulator& ay, size_t samples) {
        float out[samples];
        ay.processBlock(out, out, samples);
    }

    void bypassBlock(AyumiEmulator& ay, double duration_s = 0.03) {
        const auto sampleRate = ay.getSampleRate();
        const size_t samples = int(std::ceil(duration_s * sampleRate));
        bypassBlock(ay, samples);
    }

    double mean(const float* data, size_t size) {
        double sum = 0;
        for (size_t i = 0; i < size; ++i) {
            sum += std::abs(data[i]);
        }
        return sum / size;
    }
}

//==============================================================================
class AyChipPluginTests : public juce::UnitTest {
public:
    AyChipPluginTests() : UnitTest("AYChipPlugin", "MoTool") {}

    void runTest() override {
        beginTest("AYEmulator creation");
        {
            auto emulator = AyumiEmulator {44100, 2000000, AyumiEmulator::TypeEnum::AY};
            expect(emulator.getSampleRate() == 44100, "Sample rate should be 44100");
            expect(emulator.getClock() == 2000000, "Clock rate should be 2000000");
            expect(emulator.getType() == AyumiEmulator::TypeEnum::AY, "Type should be AY");
        }
        beginTest("AYEmulator params");
        {
            auto emulator = AyumiEmulator {44100, 2000000, AyumiEmulator::TypeEnum::AY};
            expect(emulator.getMasterVolume() == 1.0f, "Master volume should be 1.0f");
            emulator.setMasterVolume(0.5f);
            expect(emulator.getMasterVolume() == 0.5f, "Master volume should be 0.5f");

            expect(emulator.getVolume(0) == 0, "Volume should be 0");
            emulator.setVolume(0, 15);
            expect(emulator.getVolume(0) == 15, "Volume should be 15");

            expect(emulator.getChannelPan(0) == 0.25, "Pan should be 0.25");
            emulator.setChannelPan(0, 0.5);
            expect(emulator.getChannelPan(0) == 0.5, "Pan should be 0.5");

            expect(emulator.getTonePeriod(0) == 1, "Tone period should be 1");
            emulator.setTonePeriod(0, 42);
            expect(emulator.getTonePeriod(0) == 42, "Tone period should be 42");

            expect(emulator.getNoisePeriod() == 0, "Noise period should be 0");
            emulator.setNoisePeriod(31);
            expect(emulator.getNoisePeriod() == 31, "Noise period should be 42");

            expect(emulator.getEnvelopePeriod() == 1, "Envelope period should be 0");
            emulator.setEnvelopePeriod(42);
            expect(emulator.getEnvelopePeriod() == 42, "Envelope period should be 42");

            expect(emulator.getEnvelopeShape() == AyumiEmulator::EnvShapeEnum::DOWN_HOLD_BOTTOM_0, "Envelope shape should be Hold");
            emulator.setEnvelopeShape(AyumiEmulator::EnvShapeEnum::DOWN_DOWN_8);
            expect(emulator.getEnvelopeShape() == AyumiEmulator::EnvShapeEnum::DOWN_DOWN_8, "Envelope shape should be Down");

            expect(emulator.getMixer(0) == std::make_tuple(false, false, false), "Mixer should be false, false, false");
            emulator.setMixer(0, true, false, true);
            expect(emulator.getMixer(0) == std::make_tuple(true, false, true), "Mixer should be true, false, true");
            emulator.setToneOn(0, false);
            emulator.setNoiseOn(0, true);
            emulator.setEnvelopeOn(0, false);
            expect(emulator.getMixer(0) == std::make_tuple(false, true, false), "Mixer should be false, true, false");
        }
        beginTest("AYEmulator tone");
        {
            auto emulator = AyumiEmulator {44100, 2000000, AyumiEmulator::TypeEnum::AY};
            emulator.setChannelPan(0, 0.5);
            emulator.setTonePeriod(0, 1000);
            emulator.setToneOn(0, true);
            emulator.setVolume(0, 15);
            bypassBlock(emulator);

            float outLeft[44100];
            float outRight[44100];
            emulator.processBlock(outLeft, outRight, 44100);

            expect(mean(outLeft, 44100) > 0.2, "Tone should be audible");
            expect(mean(outRight, 44100) > 0.2, "Tone should be audible");
        }
        beginTest("AYEmulator noise");
        {
            auto emulator = AyumiEmulator {44100, 2000000, AyumiEmulator::TypeEnum::AY};
            emulator.setChannelPan(0, 0.5);
            emulator.setToneOn(0, false);
            emulator.setNoiseOn(0, true);
            emulator.setVolume(0, 15);
            emulator.setNoisePeriod(31);
            bypassBlock(emulator);

            float outLeft[44100];
            float outRight[44100];
            emulator.processBlock(outLeft, outRight, 44100);

            auto leftMean = mean(outLeft, 44100);
            auto rightMean = mean(outRight, 44100);
            expect(leftMean > 0.2, "Noise should be audible");
            expect(rightMean > 0.2, "Noise should be audible");
        }
        beginTest("AYEmulator envelope");
        {
            auto emulator = AyumiEmulator {44100, 2000000, AyumiEmulator::TypeEnum::AY};
            emulator.setChannelPan(0, 0.5);
            emulator.setToneOn(0, false);
            emulator.setNoiseOn(0, false);
            emulator.setEnvelopeOn(0, true);
            emulator.setVolume(0, 15);
            emulator.setEnvelopePeriod(100);
            emulator.setEnvelopeShape(AyumiEmulator::EnvShapeEnum::DOWN_DOWN_8);
            bypassBlock(emulator);

            float outLeft[44100];
            float outRight[44100];
            emulator.processBlock(outLeft, outRight, 44100);

            auto leftMean = mean(outLeft, 44100);
            auto rightMean = mean(outRight, 44100);
            expect(leftMean > 0.1, "Envelope should be audible");
            expect(rightMean > 0.1, "Envelope should be audible");
        }
    }
};

static AyChipPluginTests ayChipPluginTests;
