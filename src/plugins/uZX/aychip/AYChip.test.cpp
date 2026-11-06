#include <JuceHeader.h>

#include "aychip.h"

#include <algorithm>
#include <array>
#include <cstddef>

// #include "TestUtils.h"

using namespace MoTool::uZX;

namespace {
    // for example to bypass initial click
    void bypassBlock(AyumiEmulator& ay, size_t samples) {
        std::vector<float> out(samples);
        ay.processBlockStereo(out.data(), out.data(), samples);
    }

    void bypassBlock(AyumiEmulator& ay, double duration_s = 0.03) {
        const auto sampleRate = ay.getSampleRate();
        const size_t samples = (size_t) std::ceil(duration_s * sampleRate);
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
            auto emulator = AyumiEmulator {44100, 2000000, ChipType::AY};
            expectEquals(emulator.getSampleRate(), 44100, "Sample rate should be 44100");
            expectEquals(emulator.getClock(), 2000000.0, "Clock rate should be 2000000");
            expect(emulator.getType() == ChipType::AY, "Type should be AY");
        }
        beginTest("AYEmulator params");
        {
            auto emulator = AyumiEmulator {44100, 2000000, ChipType::AY};
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

            expect(emulator.getEnvelopeShape() == EnvShape::DOWN_HOLD_BOTTOM_0, "Envelope shape should be Hold");
            emulator.setEnvelopeShape(EnvShape::DOWN_DOWN_8);
            expect(emulator.getEnvelopeShape() == EnvShape::DOWN_DOWN_8, "Envelope shape should be Down");

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
            auto emulator = AyumiEmulator {44100, 2000000, ChipType::AY};
            emulator.setChannelPan(0, 0.5);
            emulator.setTonePeriod(0, 1000);
            emulator.setToneOn(0, true);
            emulator.setVolume(0, 15);
            bypassBlock(emulator);

            std::vector<float> outLeft(44100), outRight(44100);
            emulator.processBlockStereo(outLeft.data(), outRight.data(), 44100);

            expect(mean(outLeft.data(), 44100) > 0.2, "Tone should be audible");
            expect(mean(outRight.data(), 44100) > 0.2, "Tone should be audible");
        }
        beginTest("AYEmulator noise");
        {
            auto emulator = AyumiEmulator {44100, 2000000, ChipType::AY};
            emulator.setChannelPan(0, 0.5);
            emulator.setToneOn(0, false);
            emulator.setNoiseOn(0, true);
            emulator.setVolume(0, 15);
            emulator.setNoisePeriod(31);
            bypassBlock(emulator);

            std::vector<float> outLeft(44100), outRight(44100);
            emulator.processBlockStereo(outLeft.data(), outRight.data(), 44100);

            auto leftMean = mean(outLeft.data(), 44100);
            auto rightMean = mean(outRight.data(), 44100);
            expect(leftMean > 0.2, "Noise should be audible");
            expect(rightMean > 0.2, "Noise should be audible");
        }
        beginTest("AYEmulator envelope");
        {
            auto emulator = AyumiEmulator {44100, 2000000, ChipType::AY};
            emulator.setChannelPan(0, 0.5);
            emulator.setToneOn(0, false);
            emulator.setNoiseOn(0, false);
            emulator.setEnvelopeOn(0, true);
            emulator.setVolume(0, 15);
            emulator.setEnvelopePeriod(100);
            emulator.setEnvelopeShape(EnvShape::DOWN_DOWN_8);
            bypassBlock(emulator);

            std::vector<float> outLeft(44100), outRight(44100);
            emulator.processBlockStereo(outLeft.data(), outRight.data(), 44100);

            auto leftMean = mean(outLeft.data(), 44100);
            auto rightMean = mean(outRight.data(), 44100);
            expect(leftMean > 0.1, "Envelope should be audible");
            expect(rightMean > 0.1, "Envelope should be audible");
        }
        beginTest("AYEmulator unmixed output");
        {
            auto emulator = AyumiEmulator {44100, 2000000, ChipType::AY};
            // Set up different settings for each channel
            emulator.setChannelPan(0, 0.0);  // Ch0: left
            emulator.setChannelPan(1, 1.0);  // Ch1: right
            emulator.setChannelPan(2, 0.5);  // Ch2: center

            emulator.setTonePeriod(0, 1000);
            emulator.setTonePeriod(1, 800);
            emulator.setTonePeriod(2, 600);

            emulator.setToneOn(0, true);
            emulator.setToneOn(1, true);
            emulator.setToneOn(2, true);

            emulator.setVolume(0, 15);
            emulator.setVolume(1, 10);
            emulator.setVolume(2, 5);

            bypassBlock(emulator);

            std::vector<float> outCh0(44100), outCh1(44100), outCh2(44100);
            emulator.processBlockSeparate(outCh0.data(), outCh1.data(), outCh2.data(), 44100);

            auto ch0Mean = mean(outCh0.data(), 44100);
            auto ch1Mean = mean(outCh1.data(), 44100);
            auto ch2Mean = mean(outCh2.data(), 44100);

            expect(ch0Mean > 0.2, "Channel 0 should be audible");
            expect(ch1Mean > 0.1, "Channel 1 should be audible");
            expect(ch2Mean > 0.01, "Channel 2 should be audible (vol=5 produces ~0.02)");
            expect(ch0Mean > ch1Mean, "Channel 0 (vol=15) should be louder than Channel 1 (vol=10)");
            expect(ch1Mean > ch2Mean, "Channel 1 (vol=10) should be louder than Channel 2 (vol=5)");
        }
        beginTest("AYEmulator raw channel levels");
        {
            auto emulator = AyumiEmulator {44100, 2000000, ChipType::AY};
            emulator.setMasterVolume(0.5f);
            emulator.setTonePeriod(0, 1000);
            emulator.setToneOn(0, true);
            emulator.setVolume(0, 15);

            std::vector<float> left(44100), right(44100), a(44100), b(44100), c(44100);
            std::array<float*, 3> raw {a.data(), b.data(), c.data()};
            emulator.processBlockStereo(left.data(), right.data(), 44100, true, raw.data());

            const auto [lo, hi] = std::minmax_element(a.begin(), a.end());
            expectEquals(*lo, 0.0f, "A's tone drops to the DAC's zero");
            expectEquals(*hi, 1.0f, "A's volume 15 reaches the DAC's top, whatever the master volume");
            expect(std::all_of(b.begin(), b.end(), [](float x) { return x == 0.0f; }), "Silent B stays at zero");

            int wideEdges = 0;
            for (size_t i = 1; i < a.size(); ++i)
                wideEdges += a[i - 1] > 0.0f && a[i - 1] < 1.0f && a[i] > 0.0f && a[i] < 1.0f;
            expectEquals(wideEdges, 0, "Each edge takes at most one sample between the levels");
        }
        beginTest("AYEmulator raw levels with fewer chip ticks than samples");
        {
            auto emulator = AyumiEmulator {192000, ChipClockChoice(ChipClockEnum::NES_PAL_0_83_MHz).getClockValue(),
                                           ChipType::AY};
            emulator.setTonePeriod(0, 100);
            emulator.setToneOn(0, true);
            emulator.setVolume(0, 15);

            std::vector<float> left(19200), right(19200), a(19200), b(19200), c(19200);
            std::array<float*, 3> raw {a.data(), b.data(), c.data()};
            emulator.processBlockStereo(left.data(), right.data(), 19200, true, raw.data());

            expect(std::all_of(a.begin(), a.end(), [](float x) { return x >= 0.0f && x <= 1.0f; }),
                   "A sample with no chip tick holds the level");
            expectEquals(*std::max_element(a.begin(), a.end()), 1.0f, "A's tone still shows");
        }
        beginTest("AYEmulator renders all three channels after a reset");
        {
            auto emulator = AyumiEmulator {44100, 2000000, ChipType::AY};
            std::vector<float> outA(44100), outB(44100), outC(44100);
            emulator.processBlockSeparate(outA.data(), outB.data(), outC.data(), 4410);

            emulator.setClock(1773400);  // resets the chip
            emulator.setTonePeriod(2, 1000);
            emulator.setToneOn(2, true);
            emulator.setVolume(2, 15);
            emulator.processBlockSeparate(outA.data(), outB.data(), outC.data(), 4410);
            emulator.processBlockSeparate(outA.data(), outB.data(), outC.data(), 44100);

            expect(mean(outC.data(), 44100) > 0.2, "Channel C should be audible");
            expect(mean(outA.data(), 44100) < 0.01, "Channel A should be silent");
        }
    }
};


static AyChipPluginTests ayChipPluginTests;
