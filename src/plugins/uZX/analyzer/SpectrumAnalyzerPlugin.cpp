#include "SpectrumAnalyzerPlugin.h"

#include <cmath>
#include <numeric>

namespace MoTool::uZX {

//==============================================================================
float SpectrumBars::bandCentre(int band) {
    return 1000.0f * std::exp2((float) (band - 15) / 3.0f);
}

SpectrumBars::SpectrumBars() {
    levels_.fill(floorDb);
    bars_.fill(floorDb);
    peaks_.fill(floorDb);

    std::array<float, fftSize> ones;
    ones.fill(1.0f);
    window_.multiplyWithWindowingTable(ones.data(), ones.size());
    windowEnergy_ = std::inner_product(ones.begin(), ones.end(), ones.begin(), 0.0f);
}

const SpectrumBars::Levels& SpectrumBars::analyse(const float* samples, double sampleRate) {
    // Without its mean: a DC offset would leak into the lowest bands
    const float mean = std::accumulate(samples, samples + fftSize, 0.0f) / (float) fftSize;
    for (int i = 0; i < fftSize; ++i)
        work_[(size_t) i] = samples[i] - mean;
    window_.multiplyWithWindowingTable(work_.data(), (size_t) fftSize);
    fft_.performFrequencyOnlyForwardTransform(work_.data(), true);

    // Parseval: a band's mean square is twice its one-sided bins' power over the window's energy and length.
    // Re a full-scale sine's mean square, 1/2, that is 4 Σ|X|² / (N Σw²).
    const float scale = 4.0f / ((float) fftSize * windowEnergy_);
    const auto binsPerHz = (float) (fftSize / sampleRate);
    const float halfBand = std::exp2(1.0f / 6.0f);

    for (int band = 0; band < numBands; ++band) {
        // Each bin owns the slot one bin wide around it; a band takes the part of each slot it covers,
        // so a band narrower than a bin still gets its share
        const float lo = bandCentre(band) / halfBand * binsPerHz;
        const float hi = jmin(bandCentre(band) * halfBand * binsPerHz, (float) fftSize / 2.0f);
        float power = 0.0f;
        for (int bin = jmax(1, (int) std::floor(lo + 0.5f)); (float) bin - 0.5f < hi; ++bin) {
            const float overlap = jmin(hi, (float) bin + 0.5f) - jmax(lo, (float) bin - 0.5f);
            power += work_[(size_t) bin] * work_[(size_t) bin] * overlap;
        }
        levels_[(size_t) band] = jmax(floorDb, 10.0f * std::log10(power * scale + 1.0e-12f));
    }
    return levels_;
}

bool SpectrumBars::advance(const Levels& levels, float seconds, Falloff falloff) {
    bool moved = false;
    for (size_t i = 0; i < (size_t) numBands; ++i) {
        const float bar = jmax(levels[i], bars_[i] - falloff.barDbPerSecond * seconds);
        float peak = peaks_[i];
        if (bar >= peak) {
            peak = bar;
            holds_[i] = falloff.peakHoldSeconds;
            velocities_[i] = 0.0f;
        } else if (holds_[i] > 0.0f) {
            holds_[i] -= seconds;
        } else {
            velocities_[i] += falloff.peakDbPerSecondSquared * seconds;
            peak = jmax(bar, peak - velocities_[i] * seconds);
        }
        moved = moved || !exactlyEqual(bar, bars_[i]) || !exactlyEqual(peak, peaks_[i]);
        bars_[i] = bar;
        peaks_[i] = peak;
    }
    return moved;
}

//==============================================================================
const char* SpectrumAnalyzerPlugin::xmlTypeName = "spectrumAnalyzer";

SpectrumAnalyzerPlugin::SpectrumAnalyzerPlugin(te::PluginCreationInfo info) : Plugin(info) {
    settings.referTo(state, getUndoManager());
}

SpectrumAnalyzerPlugin::~SpectrumAnalyzerPlugin() {
    notifyListenersOfDeletion();
}

void SpectrumAnalyzerPlugin::initialise(const te::PluginInitialisationInfo&) {
    buffer_.requestClear();
}

void SpectrumAnalyzerPlugin::applyToBuffer(const te::PluginRenderContext& fc) {
    if (fc.destBuffer != nullptr)
        capture(*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples, sampleRate);
}

void SpectrumAnalyzerPlugin::capture(const juce::AudioBuffer<float>& buffer,
                                     int startSample,
                                     int numSamples,
                                     double rate) noexcept {
    SCOPED_REALTIME_CHECK

    const int channels = buffer.getNumChannels();
    if (channels == 0)
        return;

    std::array<float, 256> mono;
    for (int done = 0; done < numSamples;) {
        const int n = jmin(numSamples - done, (int) mono.size());
        FloatVectorOperations::copyWithMultiply(
            mono.data(), buffer.getReadPointer(0, startSample + done), 1.0f / (float) channels, n);
        for (int c = 1; c < channels; ++c)
            FloatVectorOperations::addWithMultiply(
                mono.data(), buffer.getReadPointer(c, startSample + done), 1.0f / (float) channels, n);
        buffer_.pushSamples(mono.data(), n);
        done += n;
    }
    captureRate_.store(rate, std::memory_order_relaxed);
}

void SpectrumAnalyzerPlugin::restorePluginStateFromValueTree(const juce::ValueTree& v) {
    settings.restoreStateFromValueTree(v);
}

}  // namespace MoTool::uZX
