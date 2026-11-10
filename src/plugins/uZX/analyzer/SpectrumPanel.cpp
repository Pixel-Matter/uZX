#include "SpectrumPanel.h"

#include "../../../gui/common/LevelMeter.h"
#include "../../../gui/common/LookAndFeel.h"

namespace MoTool::uZX {

namespace {
// Where the level meters turn yellow and red
constexpr std::array<float, 4> zones{0.0f, 0.6f, 0.85f, 1.0f};
constexpr float barAlpha = 0.6f;  // the peak caps at full colour
}  // namespace

SpectrumPanel::SpectrumPanel(te::Edit& edit, EditViewState& view)
    : edit_(edit), view_(view), atLeft_(view.vizOptions.spectrum == VizOptions::Spectrum::left) {
    constrainer_.setMinimumWidth(atLeft_ ? EditViewState::minVizSpectrumWidth : 0);
    constrainer_.setMinimumHeight(atLeft_ ? 0 : EditViewState::minVizSpectrumHeight);
    addAndMakeVisible(resizer_);
}

void SpectrumPanel::refresh(double seconds) {
    // A frame after a pause or a seek moves the bars no further than a slow frame would
    const auto elapsed = (float) jlimit(0.0, 0.1, seconds - lastSeconds_.value_or(seconds));
    lastSeconds_ = seconds;

    auto* analyzer = edit_.getMasterPluginList().findFirstPluginOfType<SpectrumAnalyzerPlugin>();
    if (analyzer == nullptr)
        return;

    if (analyzer->getBuffer().copyTo(samples_.data(), SpectrumBars::fftSize) == 0)
        return;  // a block was being written: the next frame takes it
    const auto& levels = bars_.analyse(samples_.data(), analyzer->getCaptureRate());
    const auto& s = analyzer->settings;
    if (bars_.advance(levels,
                      elapsed,
                      {s.barFalloff.getStoredValue(), s.peakHold.getStoredValue(), s.peakGravity.getStoredValue()}))
        repaint();
}

void SpectrumPanel::resized() {
    // The document lays out the notes beside it
    if (atLeft_ && getWidth() > 0)
        view_.vizSpectrumWidth = getWidth();
    else if (!atLeft_ && getHeight() > 0)
        view_.vizSpectrumHeight = getHeight();
    resizer_.setBounds(atLeft_ ? getLocalBounds().removeFromRight(gap) : getLocalBounds().removeFromTop(gap));
}

void SpectrumPanel::paint(juce::Graphics& g) {
    // Classic colours: white bars on black, the gap a grey rule as between the scopes; else the meters' colours
    const bool classic = view_.isVizClassic();
    g.fillAll(classic ? Colors::VizClassic::background : Colors::Timeline::vizLane);
    if (classic) {
        g.setColour(Colors::VizClassic::rule);
        g.fillRect(atLeft_ ? juce::Rectangle<int>(getWidth() - gap / 2, 0, 1, getHeight())
                           : juce::Rectangle<int>(0, gap / 2, getWidth(), 1));
    }
    const auto colour = [classic](float fill) {
        return classic ? Colors::VizClassic::trace : LedMeter::ledColour(fill);
    };

    // Bands run along the panel from its low end, levels across it from its base: the notes' side at the left,
    // the bottom along the bottom. Whole pixels throughout: an edge on a fraction of one blurs.
    const auto area = atLeft_ ? getLocalBounds().withTrimmedRight(gap) : getLocalBounds().withTrimmedTop(gap);
    const int length = atLeft_ ? area.getHeight() : area.getWidth();
    const int depth = atLeft_ ? area.getWidth() : area.getHeight();
    const auto rect = [&](int from, int to, float low, float high) {
        const int a = roundToInt(low * (float) depth), b = roundToInt(high * (float) depth);
        return atLeft_ ? juce::Rectangle<int>::leftTopRightBottom(
                             area.getRight() - b, area.getBottom() - to, area.getRight() - a, area.getBottom() - from)
                       : juce::Rectangle<int>::leftTopRightBottom(
                             area.getX() + from, area.getBottom() - b, area.getX() + to, area.getBottom() - a);
    };
    const int bandGap = jmax(1, length / SpectrumBars::numBands / 5);
    const float cap = (float) jmax(2, depth / 100) / (float) jmax(1, depth);
    const auto toFill = [](float db) { return jlimit(0.0f, 1.0f, 1.0f - db / SpectrumBars::floorDb); };

    for (int i = 0; i < SpectrumBars::numBands; ++i) {
        const int from = length * i / SpectrumBars::numBands;
        const int to = length * (i + 1) / SpectrumBars::numBands - bandGap;

        const float bar = toFill(bars_.getBars()[(size_t) i]);
        for (size_t zone = 0; zone + 1 < zones.size() && zones[zone] < bar; ++zone) {
            g.setColour(colour(zones[zone]).withAlpha(barAlpha));
            g.fillRect(rect(from, to, zones[zone], jmin(bar, zones[zone + 1])));
        }

        const float peak = toFill(bars_.getPeaks()[(size_t) i]);
        if (peak > 0.0f) {
            g.setColour(colour(peak));
            g.fillRect(rect(from, to, jmax(0.0f, peak - cap), jmax(peak, cap)));
        }
    }
}

}  // namespace MoTool::uZX
