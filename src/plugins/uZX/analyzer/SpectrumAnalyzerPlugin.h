#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>

#include "../../../controllers/Parameters.h"
#include "../scope/ScopeBuffer.h"

namespace te = tracktion;

namespace MoTool::uZX {

namespace AnalyzerIDs {
#define DECLARE_ID(name) inline const juce::Identifier name(#name);
DECLARE_ID(barFalloff)
DECLARE_ID(peakHold)
DECLARE_ID(peakGravity)
#undef DECLARE_ID
}  // namespace AnalyzerIDs

/** How the bars and their peak caps fall, in dB and seconds: the same at any display or video frame rate. */
struct AnalyzerSettings : ParamsBase<AnalyzerSettings>
{
    template <typename Visitor>
    void visit(Visitor&& visitor) {
        visitor(barFalloff);
        visitor(peakHold);
        visitor(peakGravity);
    }

    ParameterValue<float> barFalloff{{"barFalloff",
                                      AnalyzerIDs::barFalloff,
                                      "Bar Falloff",
                                      "How fast the bars fall",
                                      150.0f,
                                      {20.0f, 600.0f, 1.0f},
                                      "dB/s"}};

    ParameterValue<float> peakHold{{"peakHold",
                                    AnalyzerIDs::peakHold,
                                    "Peak Hold",
                                    "How long a peak cap stays up before it falls",
                                    0.5f,
                                    {0.0f, 3.0f, 0.01f},
                                    "s"}};

    ParameterValue<float> peakGravity{{"peakGravity",
                                       AnalyzerIDs::peakGravity,
                                       "Peak Gravity",
                                       "How fast a falling peak cap speeds up",
                                       120.0f,
                                       {10.0f, 1000.0f, 1.0f},
                                       juce::String::fromUTF8("dB/s\xc2\xb2")}};
};

//==============================================================================
/** 1/3-octave band levels of a window of sound, and bars with peak caps that fall from them. */
class SpectrumBars
{
public:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;  // 85 ms at 48 kHz: coarse below 100 Hz, as most analyzers
    static constexpr int numBands = 28;
    static constexpr float floorDb = -60.0f;  // ponytail: fixed range, a parameter if mixes sit too low

    using Levels = std::array<float, numBands>;

    /** Centre of a band: 31.5 Hz for the first, 1 kHz for the 16th, 16 kHz for the last. */
    static float bandCentre(int band);

    SpectrumBars();

    /** Each band's level in dB, a full-scale sine reading 0, from the last fftSize samples; floorDb at the least.
        Power sums over each band, so pink noise reads flat. */
    const Levels& analyse(const float* samples, double sampleRate);

    struct Falloff
    {
        float barDbPerSecond, peakHoldSeconds, peakDbPerSecondSquared;
    };

    /** Bars jump up to the levels and fall at a constant rate; each cap holds, then falls ever faster,
        never below its bar. False when nothing moved. */
    bool advance(const Levels& levels, float seconds, Falloff falloff);

    const Levels& getBars() const noexcept { return bars_; }
    const Levels& getPeaks() const noexcept { return peaks_; }

private:
    juce::dsp::FFT fft_{fftOrder};
    juce::dsp::WindowingFunction<float> window_{(size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false};
    std::array<float, 2 * fftSize> work_{};
    float windowEnergy_ = 0.0f;  // the sum of the window's squares

    Levels levels_, bars_, peaks_, holds_{}, velocities_{};
};

//==============================================================================
/**
 * The master mix for a spectrum analyzer, passed through unchanged: Viz mode draws it in a panel at the
 * left (SpectrumPanel). The Viz options put it on the master or take it off.
 */
class SpectrumAnalyzerPlugin : public te::Plugin
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<SpectrumAnalyzerPlugin>;

    SpectrumAnalyzerPlugin(te::PluginCreationInfo);
    ~SpectrumAnalyzerPlugin() override;

    static const char* getPluginName() { return "Spectrum Analyzer"; }
    static const char* xmlTypeName;

    juce::String getVendor() override { return "PixelMatter"; }
    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "Spectrum"; }
    juce::String getSelectableDescription() override { return "Spectrum analyzer"; }
    bool isSynth() override { return false; }

    void initialise(const te::PluginInitialisationInfo&) override;
    void deinitialise() override {}
    void applyToBuffer(const te::PluginRenderContext&) override;

    bool takesMidiInput() override { return false; }
    bool takesAudioInput() override { return true; }
    bool producesAudioWhenNoAudioInput() override { return false; }
    void restorePluginStateFromValueTree(const juce::ValueTree&) override;

    AnalyzerSettings settings;

    /** Takes the channels' average; the video render feeds its mix here, without the master plugins. */
    void capture(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples, double rate) noexcept;

    /** Forgets what it heard: a video render starts from silence. */
    void forget() noexcept { buffer_.requestClear(); }
    const ScopeBuffer& getBuffer() const noexcept { return buffer_; }
    /** The sample rate of what capture() took last. */
    double getCaptureRate() const noexcept { return captureRate_.load(std::memory_order_relaxed); }

private:
    ScopeBuffer buffer_;
    std::atomic<double> captureRate_{48000.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzerPlugin)
};

}  // namespace MoTool::uZX
